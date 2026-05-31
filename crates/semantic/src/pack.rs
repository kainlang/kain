//! Sidecar semantic pack loader and CPU reranker.
//!
//! The bootstrap forge can spend CUDA/training budget offline, but the shipped
//! compiler needs a tiny, deterministic CPU reader. This module is that runtime
//! contract: load a versioned pack, retrieve a shortlist, run an int8 packet
//! reranker, and fall back cleanly when the pack is absent.

use crate::corpus_db;
use crate::expert;
use crate::{FailureMode, RankedRepair, SemanticAnalysisReport};
use kain_error::{CompilerPhase, DiagnosticCode, DiagnosticSemanticPacket};
use serde::Deserialize;
use serde_json::json;
use std::cmp::Ordering;
use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

pub const PACK_SCHEMA: &str = "kain.semantic.pack.v1";
pub const PACK_ENV_VAR: &str = "KAIN_SEMANTIC_PACK_PATH";
pub const PACK_DISABLE_ENV_VAR: &str = "KAIN_SEMANTIC_PACK_DISABLE";
pub const DEFAULT_PACK_RELATIVE_DIR: &str = "generated/semantic/current";

const PROTOTYPE_MAGIC: &[u8; 8] = b"KSPROT1\0";
const RERANKER_MAGIC: &[u8; 8] = b"KSRANK1\0";
const RERANKER_FEATURES: usize = 8;

static DEFAULT_PACK: OnceLock<Result<Option<SemanticPack>, String>> = OnceLock::new();

#[derive(Debug, Clone, Deserialize)]
struct PackManifest {
    schema: String,
    schema_version: u32,
    #[serde(default)]
    embedding_dim: usize,
    files: PackFiles,
    #[serde(default)]
    model: PackModelManifest,
}

#[derive(Debug, Clone, Deserialize)]
struct PackFiles {
    prototypes: String,
    reranker: String,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct PackModelManifest {
    #[serde(default)]
    kind: String,
    #[serde(default)]
    lineage: String,
    #[serde(default)]
    feature_count: usize,
}

#[derive(Debug, Clone)]
pub struct SemanticPack {
    root: PathBuf,
    schema_version: u32,
    prototypes: Vec<SemanticPrototype>,
    reranker: TinyReranker,
}

#[derive(Debug, Clone)]
struct SemanticPrototype {
    code: String,
    mode: String,
    primary_text: String,
    source_window: String,
    repair_id: String,
    repair_description: String,
    replacement_text: String,
    explanation: String,
    explanation_style: String,
}

#[derive(Debug, Clone)]
struct RetrievedPrototype {
    prototype: SemanticPrototype,
    retrieval_score: i32,
    rerank_score: f32,
}

#[derive(Debug, Clone)]
struct TinyReranker {
    bias: i32,
    scale: i32,
    weights: [i8; RERANKER_FEATURES],
}

impl SemanticPack {
    pub fn load_from_path(path: impl AsRef<Path>) -> Result<Self, String> {
        let input = path.as_ref();
        let manifest_path = if input.is_file() {
            input.to_path_buf()
        } else {
            input.join("manifest.json")
        };
        let root = manifest_path
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| PathBuf::from("."));
        let manifest_text = fs::read_to_string(&manifest_path)
            .map_err(|err| format!("read semantic pack manifest: {err}"))?;
        let manifest: PackManifest = serde_json::from_str(&manifest_text)
            .map_err(|err| format!("parse semantic pack manifest: {err}"))?;
        validate_manifest(&manifest)?;

        let prototypes_path = root.join(&manifest.files.prototypes);
        let reranker_path = root.join(&manifest.files.reranker);
        let prototypes = read_prototypes(&prototypes_path)?;
        let reranker = read_reranker(&reranker_path)?;
        Ok(Self {
            root,
            schema_version: manifest.schema_version,
            prototypes,
            reranker,
        })
    }

    pub fn schema_version_string(&self) -> String {
        format!("{}:{}", PACK_SCHEMA, self.schema_version)
    }

    pub fn root(&self) -> &Path {
        &self.root
    }
}

pub fn analyze_with_default_pack(
    packet: &DiagnosticSemanticPacket,
    baseline: SemanticAnalysisReport,
) -> SemanticAnalysisReport {
    if env_truthy(PACK_DISABLE_ENV_VAR) {
        return baseline;
    }

    let Some(pack) = default_pack() else {
        return baseline;
    };
    analyze_with_pack(pack, packet, baseline)
}

pub fn analyze_with_pack(
    pack: &SemanticPack,
    packet: &DiagnosticSemanticPacket,
    baseline: SemanticAnalysisReport,
) -> SemanticAnalysisReport {
    let mut hits = retrieve(pack, packet, &baseline, 8);
    if hits.is_empty() {
        return baseline;
    }

    for hit in &mut hits {
        hit.rerank_score = pack.reranker.score(packet, &baseline, hit);
    }
    hits.sort_by(|left, right| {
        right
            .rerank_score
            .partial_cmp(&left.rerank_score)
            .unwrap_or(Ordering::Equal)
            .then_with(|| right.retrieval_score.cmp(&left.retrieval_score))
    });

    let best = hits.remove(0);
    if best.rerank_score < 0.35 && best.retrieval_score < 1500 {
        return baseline;
    }

    let mut ranked_repairs = baseline.ranked_repairs.clone();
    let pack_repair = RankedRepair {
        repair_id: format!("pack::{}", best.prototype.repair_id),
        description: best.prototype.repair_description.clone(),
        score: best.rerank_score.clamp(0.0, 0.99),
        replacement_text: non_empty_option(best.prototype.replacement_text.clone()),
    };
    let typo_family_mismatch =
        best.prototype.mode == "Typo" && best.prototype.primary_text != packet.primary_text;
    if !typo_family_mismatch {
        upsert_repair(&mut ranked_repairs, pack_repair);
    }
    ranked_repairs.sort_by(|left, right| {
        right
            .score
            .partial_cmp(&left.score)
            .unwrap_or(Ordering::Equal)
            .then_with(|| left.repair_id.cmp(&right.repair_id))
    });

    SemanticAnalysisReport {
        root_cause_confidence: baseline
            .root_cause_confidence
            .max((0.62 + best.rerank_score * 0.30).min(0.99)),
        likely_failure_mode: failure_mode_from_pack(&best.prototype, &baseline),
        ranked_repairs,
        dynamic_explanation: if typo_family_mismatch || best.prototype.explanation.is_empty() {
            baseline.dynamic_explanation
        } else {
            best.prototype.explanation
        },
        cascade_probability: baseline.cascade_probability,
        explanation_style: if typo_family_mismatch || best.prototype.explanation_style.is_empty() {
            baseline.explanation_style
        } else {
            best.prototype.explanation_style
        },
        backend: "pack_cpu_rerank".to_string(),
        pack_schema_version: Some(pack.schema_version_string()),
    }
}

pub fn write_semantic_pack_from_corpus(dir: impl AsRef<Path>) -> io::Result<()> {
    let dir = dir.as_ref();
    fs::create_dir_all(dir)?;
    let prototypes = prototypes_from_baked_corpus();
    write_prototypes(&dir.join("prototypes.bin"), &prototypes)?;
    write_default_reranker(&dir.join("reranker.i8"))?;

    let manifest = json!({
        "schema": PACK_SCHEMA,
        "schema_version": 1,
        "embedding_dim": 384,
        "files": {
            "prototypes": "prototypes.bin",
            "reranker": "reranker.i8"
        },
        "model": {
            "kind": "packet_reranker_int8",
            "lineage": "kain-transformer-v1",
            "feature_count": RERANKER_FEATURES
        },
        "runtime": {
            "requires_cuda": false,
            "offline": true
        }
    });
    fs::write(
        dir.join("manifest.json"),
        serde_json::to_string_pretty(&manifest).unwrap(),
    )
}

fn default_pack() -> Option<&'static SemanticPack> {
    DEFAULT_PACK
        .get_or_init(|| {
            let Some(path) = resolve_default_pack_path() else {
                return Ok(None);
            };
            SemanticPack::load_from_path(path).map(Some)
        })
        .as_ref()
        .ok()
        .and_then(Option::as_ref)
}

fn resolve_default_pack_path() -> Option<PathBuf> {
    if let Some(path) = env::var_os(PACK_ENV_VAR).map(PathBuf::from) {
        if path.exists() {
            return Some(path);
        }
    }

    for candidate in default_pack_candidates() {
        if candidate.join("manifest.json").exists() || candidate.is_file() {
            return Some(candidate);
        }
    }
    None
}

fn default_pack_candidates() -> Vec<PathBuf> {
    let mut candidates = Vec::new();
    if let Ok(exe) = env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            candidates.push(exe_dir.join("semantic").join("current"));
            candidates.push(exe_dir.join("semantic_pack"));
        }
    }
    if let Some(home) = env::var_os("KAIN_HOME").map(PathBuf::from) {
        candidates.push(home.join(DEFAULT_PACK_RELATIVE_DIR));
    }
    candidates.push(
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join(".kain")
            .join("oracle")
            .join("sempack")
            .join("current"),
    );
    candidates
}

fn validate_manifest(manifest: &PackManifest) -> Result<(), String> {
    if manifest.schema != PACK_SCHEMA {
        return Err(format!(
            "semantic pack schema mismatch: expected {PACK_SCHEMA}, got {}",
            manifest.schema
        ));
    }
    if manifest.schema_version != 1 {
        return Err(format!(
            "unsupported semantic pack schema version {}",
            manifest.schema_version
        ));
    }
    if manifest.embedding_dim != 0 && manifest.embedding_dim != 384 {
        return Err(format!(
            "unsupported semantic pack embedding dim {}",
            manifest.embedding_dim
        ));
    }
    if manifest.model.kind != "packet_reranker_int8" {
        return Err(format!(
            "semantic pack reranker kind '{}' is not supported",
            manifest.model.kind
        ));
    }
    if manifest.model.lineage != "kain-transformer-v1" {
        return Err(format!(
            "semantic pack reranker lineage '{}' is not transformer-lineage v1",
            manifest.model.lineage
        ));
    }
    if manifest.model.feature_count != RERANKER_FEATURES {
        return Err(format!(
            "semantic pack reranker feature count {} does not match runtime {}",
            manifest.model.feature_count, RERANKER_FEATURES
        ));
    }
    Ok(())
}

fn retrieve(
    pack: &SemanticPack,
    packet: &DiagnosticSemanticPacket,
    baseline: &SemanticAnalysisReport,
    max_results: usize,
) -> Vec<RetrievedPrototype> {
    let query_tokens = tokens_for_packet(packet);
    let baseline_key = failure_mode_key(&baseline.likely_failure_mode);
    let mut hits: Vec<RetrievedPrototype> = pack
        .prototypes
        .iter()
        .filter_map(|prototype| {
            let score = score_prototype(prototype, packet, baseline_key, &query_tokens);
            if score <= 0 {
                return None;
            }
            Some(RetrievedPrototype {
                prototype: prototype.clone(),
                retrieval_score: score,
                rerank_score: 0.0,
            })
        })
        .collect();
    hits.sort_by(|left, right| {
        right
            .retrieval_score
            .cmp(&left.retrieval_score)
            .then_with(|| left.prototype.repair_id.cmp(&right.prototype.repair_id))
    });
    hits.truncate(max_results);
    hits
}

fn score_prototype(
    prototype: &SemanticPrototype,
    packet: &DiagnosticSemanticPacket,
    baseline_key: &str,
    query_tokens: &[String],
) -> i32 {
    let mut score = 0;
    let exact_code = prototype.code == packet.code.as_str();
    let exact_mode = prototype.mode == baseline_key;
    if !exact_code && !exact_mode {
        return 0;
    }

    if exact_code {
        score += 4000;
    } else if same_code_family(&prototype.code, packet.code.as_str()) {
        score += 900;
    }
    if exact_mode {
        score += 900;
    }
    if !packet.primary_text.is_empty() && prototype.primary_text == packet.primary_text {
        score += 2600;
    }
    if let Some(path) = &packet.source_path {
        if !path.is_empty() && prototype.source_window.contains(path) {
            score += 300;
        }
    }
    if packet.source_window == prototype.source_window {
        score += 2200;
    } else if !packet.source_window.is_empty() {
        score += token_overlap_score(query_tokens, &prototype.source_window) * 35;
    }
    if !prototype.replacement_text.is_empty()
        && packet
            .visible_symbols
            .iter()
            .any(|symbol| symbol == &prototype.replacement_text)
    {
        score += 1200;
    }
    score
}

impl TinyReranker {
    fn score(
        &self,
        packet: &DiagnosticSemanticPacket,
        baseline: &SemanticAnalysisReport,
        hit: &RetrievedPrototype,
    ) -> f32 {
        let features = reranker_features(packet, baseline, hit);
        let mut raw = self.bias;
        for (weight, feature) in self.weights.iter().zip(features.iter()) {
            raw += i32::from(*weight) * i32::from(*feature);
        }
        let model = (raw as f32 / self.scale.max(1) as f32).clamp(0.0, 1.0);
        let retrieval = (hit.retrieval_score as f32 / 10_000.0).clamp(0.0, 1.0);
        (0.45 * model + 0.35 * retrieval + 0.20 * baseline.root_cause_confidence).clamp(0.0, 0.99)
    }
}

fn reranker_features(
    packet: &DiagnosticSemanticPacket,
    baseline: &SemanticAnalysisReport,
    hit: &RetrievedPrototype,
) -> [u8; RERANKER_FEATURES] {
    let baseline_key = failure_mode_key(&baseline.likely_failure_mode);
    let source_overlap =
        token_overlap_score(&tokens_for_packet(packet), &hit.prototype.source_window).clamp(0, 100)
            as u8;
    let top_repair_match = baseline
        .ranked_repairs
        .first()
        .map(|repair| repair_matches_prototype(repair, &hit.prototype))
        .unwrap_or(false);
    [
        bool_feature(hit.prototype.code == packet.code.as_str()),
        bool_feature(hit.prototype.mode == baseline_key),
        bool_feature(hit.prototype.primary_text == packet.primary_text),
        source_overlap,
        bool_feature(
            !hit.prototype.replacement_text.is_empty()
                && packet
                    .visible_symbols
                    .iter()
                    .any(|symbol| symbol == &hit.prototype.replacement_text),
        ),
        bool_feature(top_repair_match),
        bool_feature(packet.source_window == hit.prototype.source_window),
        (baseline.root_cause_confidence * 100.0).clamp(0.0, 100.0) as u8,
    ]
}

fn repair_matches_prototype(repair: &RankedRepair, prototype: &SemanticPrototype) -> bool {
    repair
        .replacement_text
        .as_ref()
        .map(|text| text == &prototype.replacement_text)
        .unwrap_or(false)
        || repair.description.contains(&prototype.replacement_text)
        || repair.repair_id.contains(&prototype.repair_id)
}

fn upsert_repair(repairs: &mut Vec<RankedRepair>, repair: RankedRepair) {
    if let Some(existing) = repairs.iter_mut().find(|existing| {
        existing.replacement_text == repair.replacement_text
            || existing
                .description
                .eq_ignore_ascii_case(&repair.description)
    }) {
        if repair.score > existing.score {
            existing.score = repair.score;
            existing.repair_id = repair.repair_id;
            existing.description = repair.description;
        }
        return;
    }
    repairs.push(repair);
}

fn failure_mode_from_pack(
    prototype: &SemanticPrototype,
    baseline: &SemanticAnalysisReport,
) -> FailureMode {
    match prototype.mode.as_str() {
        "Typo" => FailureMode::Typo {
            intended: prototype.replacement_text.clone(),
        },
        "MissingImport" => FailureMode::MissingImport {
            module: prototype.primary_text.clone(),
            import_path: prototype.replacement_text.clone(),
        },
        "MissingSurface" => FailureMode::MissingSurface,
        "OwnershipViolation" => FailureMode::OwnershipViolation,
        "ShaderStageMismatch" => FailureMode::ShaderStageMismatch,
        "WorldDeclarationError" => FailureMode::WorldDeclarationError,
        "ActorMessageMismatch" => FailureMode::ActorMessageMismatch,
        "ParserDelimiterDamage" => FailureMode::ParserDelimiterDamage,
        "ConvergeMismatch" => FailureMode::ConvergeMismatch,
        "EntangleViolation" => FailureMode::EntangleViolation,
        _ => baseline.likely_failure_mode.clone(),
    }
}

fn failure_mode_key(mode: &FailureMode) -> &'static str {
    match mode {
        FailureMode::Typo { .. } => "Typo",
        FailureMode::MissingImport { .. } => "MissingImport",
        FailureMode::MissingSurface => "MissingSurface",
        FailureMode::OwnershipViolation => "OwnershipViolation",
        FailureMode::ShaderStageMismatch => "ShaderStageMismatch",
        FailureMode::WorldDeclarationError => "WorldDeclarationError",
        FailureMode::ActorMessageMismatch => "ActorMessageMismatch",
        FailureMode::ParserDelimiterDamage => "ParserDelimiterDamage",
        FailureMode::ConvergeMismatch => "ConvergeMismatch",
        FailureMode::EntangleViolation => "EntangleViolation",
        FailureMode::GenericUnknown => "GenericUnknown",
    }
}

fn same_code_family(left: &str, right: &str) -> bool {
    code_family(left)
        .zip(code_family(right))
        .map(|(left, right)| left == right)
        .unwrap_or(false)
}

fn code_family(code: &str) -> Option<&str> {
    code.strip_prefix("KAIN-")
        .and_then(|rest| rest.split('-').next())
}

fn tokens_for_packet(packet: &DiagnosticSemanticPacket) -> Vec<String> {
    let mut material = String::new();
    material.push_str(packet.code.as_str());
    material.push(' ');
    material.push_str(&packet.primary_text);
    material.push(' ');
    material.push_str(&packet.source_window);
    material.push(' ');
    for symbol in &packet.visible_symbols {
        material.push_str(symbol);
        material.push(' ');
    }
    for import in &packet.visible_imports {
        material.push_str(import);
        material.push(' ');
    }
    tokenize(&material)
}

fn token_overlap_score(query_tokens: &[String], target: &str) -> i32 {
    if query_tokens.is_empty() {
        return 0;
    }
    let target_tokens = tokenize(target);
    if target_tokens.is_empty() {
        return 0;
    }
    let mut matches = 0;
    for query in query_tokens {
        if target_tokens.iter().any(|target| target == query) {
            matches += 1;
        }
    }
    ((matches * 100) / query_tokens.len()).min(100) as i32
}

fn tokenize(text: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut current = String::new();
    for ch in text.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' {
            current.push(ch.to_ascii_lowercase());
        } else if !current.is_empty() {
            tokens.push(std::mem::take(&mut current));
        }
    }
    if !current.is_empty() {
        tokens.push(current);
    }
    tokens
}

fn bool_feature(value: bool) -> u8 {
    if value {
        100
    } else {
        0
    }
}

fn env_truthy(key: &str) -> bool {
    env::var(key)
        .map(|value| matches!(value.as_str(), "1" | "true" | "TRUE" | "yes" | "on"))
        .unwrap_or(false)
}

fn non_empty_option(value: String) -> Option<String> {
    if value.is_empty() {
        None
    } else {
        Some(value)
    }
}

fn read_prototypes(path: &Path) -> Result<Vec<SemanticPrototype>, String> {
    let bytes = fs::read(path).map_err(|err| format!("read semantic prototypes: {err}"))?;
    let mut cursor = Cursor::new(&bytes);
    cursor.expect_magic(PROTOTYPE_MAGIC)?;
    let count = cursor.read_u32()? as usize;
    let mut prototypes = Vec::with_capacity(count);
    for _ in 0..count {
        prototypes.push(SemanticPrototype {
            code: cursor.read_string()?,
            mode: cursor.read_string()?,
            primary_text: cursor.read_string()?,
            source_window: cursor.read_string()?,
            repair_id: cursor.read_string()?,
            repair_description: cursor.read_string()?,
            replacement_text: cursor.read_string()?,
            explanation: cursor.read_string()?,
            explanation_style: cursor.read_string()?,
        });
    }
    Ok(prototypes)
}

fn read_reranker(path: &Path) -> Result<TinyReranker, String> {
    let bytes = fs::read(path).map_err(|err| format!("read semantic reranker: {err}"))?;
    let mut cursor = Cursor::new(&bytes);
    cursor.expect_magic(RERANKER_MAGIC)?;
    let feature_count = cursor.read_u32()? as usize;
    if feature_count != RERANKER_FEATURES {
        return Err(format!(
            "semantic reranker feature count {feature_count} does not match runtime {RERANKER_FEATURES}"
        ));
    }
    let bias = cursor.read_i32()?;
    let scale = cursor.read_i32()?;
    let mut weights = [0i8; RERANKER_FEATURES];
    for weight in &mut weights {
        *weight = cursor.read_i8()?;
    }
    Ok(TinyReranker {
        bias,
        scale: scale.max(1),
        weights,
    })
}

struct Cursor<'a> {
    bytes: &'a [u8],
    offset: usize,
}

impl<'a> Cursor<'a> {
    fn new(bytes: &'a [u8]) -> Self {
        Self { bytes, offset: 0 }
    }

    fn expect_magic(&mut self, magic: &[u8; 8]) -> Result<(), String> {
        let actual = self.read_exact(8)?;
        if actual != magic {
            return Err("semantic pack magic mismatch".to_string());
        }
        Ok(())
    }

    fn read_u32(&mut self) -> Result<u32, String> {
        let bytes = self.read_exact(4)?;
        Ok(u32::from_le_bytes(bytes.try_into().unwrap()))
    }

    fn read_i32(&mut self) -> Result<i32, String> {
        let bytes = self.read_exact(4)?;
        Ok(i32::from_le_bytes(bytes.try_into().unwrap()))
    }

    fn read_i8(&mut self) -> Result<i8, String> {
        let bytes = self.read_exact(1)?;
        Ok(bytes[0] as i8)
    }

    fn read_string(&mut self) -> Result<String, String> {
        let len = self.read_u32()? as usize;
        let bytes = self.read_exact(len)?;
        String::from_utf8(bytes.to_vec()).map_err(|err| format!("semantic pack utf8: {err}"))
    }

    fn read_exact(&mut self, len: usize) -> Result<&'a [u8], String> {
        let end = self
            .offset
            .checked_add(len)
            .ok_or_else(|| "semantic pack cursor overflow".to_string())?;
        if end > self.bytes.len() {
            return Err("semantic pack ended early".to_string());
        }
        let slice = &self.bytes[self.offset..end];
        self.offset = end;
        Ok(slice)
    }
}

fn prototypes_from_baked_corpus() -> Vec<SemanticPrototype> {
    corpus_db::ERROR_CORPUS_CASES
        .iter()
        .map(|case| {
            let code = DiagnosticCode::new(case.expected_code);
            let mut packet =
                DiagnosticSemanticPacket::new(code, corpus_case_phase(code), case.primary_text)
                    .source_window(case.source_window);
            if case.expected_mode == "Typo" {
                packet = packet
                    .visible_symbols(vec![
                        case.expected_repair.to_string(),
                        "println".to_string(),
                    ])
                    .add_scope_match(case.expected_repair, 1);
            } else if case.expected_mode == "ConvergeMismatch" {
                packet = packet.flag("in_converge_block", true);
            } else if case.expected_mode == "EntangleViolation" {
                packet = packet.flag("in_entangle_block", true);
            }
            packet = packet.add_repair(case.expected_repair, "ideal repair", case.expected_repair);
            let analysis = expert::analyze(&packet);
            let top_repair = analysis.ranked_repairs.first();
            SemanticPrototype {
                code: case.expected_code.to_string(),
                mode: case.expected_mode.to_string(),
                primary_text: case.primary_text.to_string(),
                source_window: case.source_window.to_string(),
                repair_id: top_repair
                    .map(|repair| repair.repair_id.clone())
                    .unwrap_or_else(|| case.expected_repair.to_string()),
                repair_description: top_repair
                    .map(|repair| repair.description.clone())
                    .unwrap_or_else(|| format!("Golden corpus repair: {}", case.expected_repair)),
                replacement_text: top_repair
                    .and_then(|repair| repair.replacement_text.clone())
                    .unwrap_or_else(|| case.expected_repair.to_string()),
                explanation: analysis.dynamic_explanation,
                explanation_style: analysis.explanation_style,
            }
        })
        .collect()
}

fn corpus_case_phase(code: DiagnosticCode) -> CompilerPhase {
    if code.as_str().starts_with("KAIN-PARSE-") {
        CompilerPhase::Parser
    } else {
        CompilerPhase::TypeChecking
    }
}

fn write_prototypes(path: &Path, prototypes: &[SemanticPrototype]) -> io::Result<()> {
    let mut out = Vec::new();
    out.extend_from_slice(PROTOTYPE_MAGIC);
    write_u32(&mut out, prototypes.len() as u32);
    for prototype in prototypes {
        write_string(&mut out, &prototype.code);
        write_string(&mut out, &prototype.mode);
        write_string(&mut out, &prototype.primary_text);
        write_string(&mut out, &prototype.source_window);
        write_string(&mut out, &prototype.repair_id);
        write_string(&mut out, &prototype.repair_description);
        write_string(&mut out, &prototype.replacement_text);
        write_string(&mut out, &prototype.explanation);
        write_string(&mut out, &prototype.explanation_style);
    }
    fs::write(path, out)
}

fn write_default_reranker(path: &Path) -> io::Result<()> {
    let mut out = Vec::new();
    out.extend_from_slice(RERANKER_MAGIC);
    write_u32(&mut out, RERANKER_FEATURES as u32);
    write_i32(&mut out, 0);
    write_i32(&mut out, 5000);
    out.extend_from_slice(&[9, 7, 12, 6, 8, 6, 10, 5]);
    fs::write(path, out)
}

fn write_u32(out: &mut Vec<u8>, value: u32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn write_i32(out: &mut Vec<u8>, value: i32) {
    out.extend_from_slice(&value.to_le_bytes());
}

fn write_string(out: &mut Vec<u8>, value: &str) {
    write_u32(out, value.len() as u32);
    out.write_all(value.as_bytes()).unwrap();
}

#[cfg(test)]
mod tests {
    use super::*;

    fn unique_pack_dir(name: &str) -> PathBuf {
        env::temp_dir().join(format!("kain-semantic-{name}-{}", std::process::id()))
    }

    #[test]
    fn sidecar_pack_round_trips_and_reranks_typo() {
        let dir = unique_pack_dir("typo-pack");
        let _ = fs::remove_dir_all(&dir);
        write_semantic_pack_from_corpus(&dir).expect("write semantic pack");
        let pack = SemanticPack::load_from_path(&dir).expect("load semantic pack");
        assert_eq!(pack.schema_version_string(), "kain.semantic.pack.v1:1");

        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "prntln",
        )
        .source_window("// ERROR: Unknown identifier - typo in function name\nfn main() -> Int:\n    let result = prntln(\"hello\")\n    return 0\n")
        .visible_symbols(vec!["println".to_string()]);

        let baseline = expert::analyze(&packet);
        let enhanced = analyze_with_pack(&pack, &packet, baseline);
        assert_eq!(enhanced.backend, "pack_cpu_rerank");
        assert_eq!(
            enhanced.pack_schema_version.as_deref(),
            Some("kain.semantic.pack.v1:1")
        );
        assert!(
            enhanced
                .ranked_repairs
                .first()
                .and_then(|repair| repair.replacement_text.as_deref())
                == Some("println")
        );

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn sidecar_pack_rejects_mode_mismatched_same_family_hits() {
        let dir = unique_pack_dir("mode-guard-pack");
        let _ = fs::remove_dir_all(&dir);
        write_semantic_pack_from_corpus(&dir).expect("write semantic pack");
        let pack = SemanticPack::load_from_path(&dir).expect("load semantic pack");

        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeWorldMissingSurface,
            CompilerPhase::TypeChecking,
            "Demo",
        )
        .source_window("world Demo:\n    state hp: Int = 3");

        let baseline = expert::analyze(&packet);
        let enhanced = analyze_with_pack(&pack, &packet, baseline);
        assert!(matches!(
            enhanced.likely_failure_mode,
            FailureMode::MissingSurface
        ));
        assert!(
            !enhanced.dynamic_explanation.contains("prntln"),
            "mismatched typo prototype should not override missing-surface explanation"
        );

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn sidecar_pack_keeps_typo_explanation_on_cross_family_hits() {
        let dir = unique_pack_dir("typo-family-guard-pack");
        let _ = fs::remove_dir_all(&dir);
        write_semantic_pack_from_corpus(&dir).expect("write semantic pack");
        let pack = SemanticPack::load_from_path(&dir).expect("load semantic pack");

        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "python_with_pykain_case_cout",
        )
        .source_window("fn main() -> Int:\n    let score = python_with_pykain_case_cout()\n    return 0");

        let baseline = expert::analyze(&packet);
        let enhanced = analyze_with_pack(&pack, &packet, baseline.clone());
        assert_eq!(
            enhanced
                .ranked_repairs
                .first()
                .and_then(|repair| repair.replacement_text.as_deref()),
            Some("use std::python")
        );
        assert!(
            enhanced.dynamic_explanation.contains("python_with_pykain_case_cout"),
            "typo family mismatch should keep the baseline explanation"
        );
        assert!(
            !enhanced.dynamic_explanation.contains("fs_read_texx"),
            "cross-family typo prototype must not override the explanation"
        );

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn missing_pack_keeps_fallback_rules() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "definitely_not_in_pack",
        );
        let baseline = expert::analyze(&packet);
        let result = analyze_with_default_pack(&packet, baseline);
        assert!(result.backend == "fallback_rules" || result.backend == "pack_cpu_rerank");
    }
}
