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
use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::OnceLock;

pub const PACK_SCHEMA: &str = "kain.semantic.pack.v1";
pub const PACK_ENV_VAR: &str = "KAIN_SEMANTIC_PACK_PATH";
pub const CUDA_PACK_ENV_VAR: &str = "KAIN_SEMANTIC_CUDA_PACK_PATH";
pub const LANE_ENV_VAR: &str = "KAIN_SEMANTIC_LANE";
pub const PACK_DISABLE_ENV_VAR: &str = "KAIN_SEMANTIC_PACK_DISABLE";
pub const PACK_STRICT_ENV_VAR: &str = "KAIN_SEMANTIC_PACK_STRICT";
pub const DEFAULT_PACK_RELATIVE_DIR: &str = "generated/semantic/current";
pub const DEFAULT_CUDA_PACK_RELATIVE_DIR: &str = "generated/semantic/cuda_forged/current";

const PROTOTYPE_MAGIC: &[u8; 8] = b"KSPROT1\0";
const RERANKER_MAGIC: &[u8; 8] = b"KSRANK1\0";
const RERANKER_FEATURES: usize = 8;

static DEFAULT_PACK: OnceLock<Result<Option<SemanticPack>, String>> = OnceLock::new();

const CUDA_FORGE_KERNELS: &[(&str, &str)] = &[
    ("search", "search_kernel"),
    ("transformer", "transformer"),
    ("training", "training"),
    ("error", "error_kernel"),
    ("repair", "repair_kernel"),
];

#[derive(Debug, Clone, Deserialize)]
struct PackManifest {
    schema: String,
    schema_version: u32,
    #[serde(default)]
    embedding_dim: usize,
    files: PackFiles,
    #[serde(default)]
    model: PackModelManifest,
    #[serde(default)]
    runtime: PackRuntimeManifest,
    #[serde(default)]
    forge: PackForgeManifest,
    #[serde(default)]
    priors: PackPriorManifest,
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

#[derive(Debug, Clone, Default, Deserialize)]
struct PackRuntimeManifest {
    #[serde(default)]
    requires_cuda: bool,
    #[serde(default)]
    offline: bool,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct PackForgeManifest {
    #[serde(default)]
    backend: String,
    #[serde(default)]
    corpus_fingerprint: String,
    #[serde(default)]
    oracle_manifest_fingerprint: String,
    #[serde(default)]
    oracle_pack_fingerprint: String,
    #[serde(default)]
    kernel_fingerprints: BTreeMap<String, String>,
}

#[derive(Debug, Clone, Copy, Deserialize)]
struct PackPriorManifest {
    #[serde(default = "default_root_confidence_floor")]
    root_confidence_floor: f32,
    #[serde(default = "default_exact_repair_bonus")]
    exact_repair_bonus: f32,
    #[serde(default = "default_cascade_suppression")]
    cascade_suppression: f32,
    #[serde(default = "default_source_match_bonus")]
    source_match_bonus: i32,
}

impl Default for PackPriorManifest {
    fn default() -> Self {
        Self {
            root_confidence_floor: default_root_confidence_floor(),
            exact_repair_bonus: default_exact_repair_bonus(),
            cascade_suppression: default_cascade_suppression(),
            source_match_bonus: default_source_match_bonus(),
        }
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
enum SemanticLane {
    Auto,
    Cpu,
    CudaForged,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
enum PackBackend {
    CpuLegacy,
    CudaForged,
}

#[derive(Debug, Clone)]
pub struct SemanticPack {
    root: PathBuf,
    schema_version: u32,
    backend: PackBackend,
    priors: PackPriorManifest,
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

impl SemanticLane {
    fn from_env() -> Self {
        match env::var(LANE_ENV_VAR)
            .unwrap_or_else(|_| "auto".to_string())
            .to_ascii_lowercase()
            .as_str()
        {
            "cpu" | "legacy" | "rules" => Self::Cpu,
            "cuda" | "cuda_forged" | "cuda-forged" | "forged" | "god" | "godmode" => {
                Self::CudaForged
            }
            _ => Self::Auto,
        }
    }

    fn was_explicit_cuda() -> bool {
        env::var(LANE_ENV_VAR)
            .map(|value| {
                matches!(
                    value.to_ascii_lowercase().as_str(),
                    "cuda" | "cuda_forged" | "cuda-forged" | "forged" | "god" | "godmode"
                )
            })
            .unwrap_or(false)
    }
}

impl PackBackend {
    fn from_manifest(manifest: &PackManifest) -> Self {
        if manifest.schema_version >= 2
            || manifest.forge.backend == "cuda_forged"
            || manifest.model.kind == "cuda_forged_packet_reranker_int8"
        {
            Self::CudaForged
        } else {
            Self::CpuLegacy
        }
    }

    fn backend_name(self) -> &'static str {
        match self {
            Self::CpuLegacy => "pack_cpu_rerank",
            Self::CudaForged => "pack_cuda_forged",
        }
    }

    fn schema_lane(self) -> &'static str {
        match self {
            Self::CpuLegacy => "cpu_legacy",
            Self::CudaForged => "cuda_forged",
        }
    }
}

fn default_root_confidence_floor() -> f32 {
    0.62
}

fn default_exact_repair_bonus() -> f32 {
    0.0
}

fn default_cascade_suppression() -> f32 {
    0.0
}

fn default_source_match_bonus() -> i32 {
    2200
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
        let backend = PackBackend::from_manifest(&manifest);

        let prototypes_path = root.join(&manifest.files.prototypes);
        let reranker_path = root.join(&manifest.files.reranker);
        let prototypes = read_prototypes(&prototypes_path)?;
        let reranker = read_reranker(&reranker_path)?;
        Ok(Self {
            root,
            schema_version: manifest.schema_version,
            backend,
            priors: manifest.priors,
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

    pub fn backend_name(&self) -> &'static str {
        self.backend.backend_name()
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
        score: repair_score_with_priors(pack, &best, packet).clamp(0.0, 0.99),
        replacement_text: non_empty_option(best.prototype.replacement_text.clone()),
    };
    let symbol_family_mismatch = symbol_family_requires_primary_match(&best.prototype.mode)
        && best.prototype.primary_text != packet.primary_text;
    if !symbol_family_mismatch {
        upsert_repair(&mut ranked_repairs, pack_repair);
    }
    ranked_repairs.sort_by(|left, right| {
        right
            .score
            .partial_cmp(&left.score)
            .unwrap_or(Ordering::Equal)
            .then_with(|| left.repair_id.cmp(&right.repair_id))
    });

    let cascade_probability =
        cascade_probability_with_priors(pack, packet, &baseline, &best, symbol_family_mismatch);
    let likely_failure_mode = if symbol_family_mismatch {
        baseline.likely_failure_mode.clone()
    } else {
        failure_mode_from_pack(&best.prototype, &baseline)
    };
    let dynamic_explanation = if symbol_family_mismatch || best.prototype.explanation.is_empty() {
        baseline.dynamic_explanation
    } else {
        best.prototype.explanation
    };
    let explanation_style = if symbol_family_mismatch || best.prototype.explanation_style.is_empty()
    {
        baseline.explanation_style
    } else {
        best.prototype.explanation_style
    };

    SemanticAnalysisReport {
        root_cause_confidence: baseline
            .root_cause_confidence
            .max((pack.priors.root_confidence_floor + best.rerank_score * 0.30).min(0.99)),
        likely_failure_mode,
        ranked_repairs,
        dynamic_explanation,
        cascade_probability,
        explanation_style,
        backend: pack.backend_name().to_string(),
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

#[cfg(feature = "cuda-forged-pack")]
pub fn write_cuda_forged_semantic_pack_from_corpus(
    dir: impl AsRef<Path>,
    oracle_root: impl AsRef<Path>,
) -> io::Result<()> {
    let dir = dir.as_ref();
    let oracle_root = oracle_root.as_ref();

    let oracle_manifest = oracle_root.join("kain_error_oracle.manifest.json");
    let oracle_pack = oracle_root.join("kain_error_oracle.bin");
    let oracle_manifest_fingerprint = required_file_fingerprint(&oracle_manifest)?;
    let oracle_pack_fingerprint = required_file_fingerprint(&oracle_pack)?;
    let kernel_fingerprints = collect_cuda_kernel_fingerprints(oracle_root)?;

    fs::create_dir_all(dir)?;
    let prototypes = distill_cuda_forged_prototypes(prototypes_from_baked_corpus());
    let corpus_fingerprint = fingerprint_prototypes(&prototypes);
    write_prototypes(&dir.join("prototypes.bin"), &prototypes)?;
    write_cuda_forged_reranker(&dir.join("reranker.i8"))?;

    let manifest = json!({
        "schema": PACK_SCHEMA,
        "schema_version": 2,
        "embedding_dim": 384,
        "files": {
            "prototypes": "prototypes.bin",
            "reranker": "reranker.i8"
        },
        "model": {
            "kind": "cuda_forged_packet_reranker_int8",
            "lineage": "kain-transformer-v2-cuda-forged",
            "feature_count": RERANKER_FEATURES
        },
        "runtime": {
            "requires_cuda": false,
            "offline": true
        },
        "forge": {
            "backend": "cuda_forged",
            "oracle_manifest": oracle_manifest.display().to_string(),
            "oracle_pack": oracle_pack.display().to_string(),
            "corpus_fingerprint": corpus_fingerprint,
            "oracle_manifest_fingerprint": oracle_manifest_fingerprint,
            "oracle_pack_fingerprint": oracle_pack_fingerprint,
            "kernel_fingerprints": kernel_fingerprints
        },
        "priors": {
            "root_confidence_floor": 0.74,
            "exact_repair_bonus": 0.08,
            "cascade_suppression": 0.42,
            "source_match_bonus": 3400
        }
    });
    fs::write(
        dir.join("manifest.json"),
        serde_json::to_string_pretty(&manifest).unwrap(),
    )
}

#[cfg(not(feature = "cuda-forged-pack"))]
pub fn write_cuda_forged_semantic_pack_from_corpus(
    _dir: impl AsRef<Path>,
    _oracle_root: impl AsRef<Path>,
) -> io::Result<()> {
    Err(io::Error::new(
        io::ErrorKind::Other,
        "cuda-forged semantic pack writing requires the cuda-forged-pack feature",
    ))
}

fn default_pack() -> Option<&'static SemanticPack> {
    DEFAULT_PACK
        .get_or_init(|| {
            let lane = SemanticLane::from_env();
            resolve_pack_for_lane(lane)
        })
        .as_ref()
        .ok()
        .and_then(Option::as_ref)
}

fn resolve_pack_for_lane(lane: SemanticLane) -> Result<Option<SemanticPack>, String> {
    match lane {
        SemanticLane::Cpu => load_pack_for_backend(PackBackend::CpuLegacy),
        SemanticLane::CudaForged => load_cuda_or_fallback(true),
        SemanticLane::Auto => {
            if cfg!(feature = "cuda-forged-pack") {
                match load_pack_for_backend(PackBackend::CudaForged) {
                    Ok(Some(pack)) => Ok(Some(pack)),
                    Ok(None) | Err(_) => load_pack_for_backend(PackBackend::CpuLegacy),
                }
            } else {
                load_pack_for_backend(PackBackend::CpuLegacy)
            }
        }
    }
}

fn load_cuda_or_fallback(explicit: bool) -> Result<Option<SemanticPack>, String> {
    if !cfg!(feature = "cuda-forged-pack") {
        warn_cuda_fallback("cuda-forged semantic feature is not compiled in", explicit);
        return load_pack_for_backend(PackBackend::CpuLegacy);
    }

    match load_pack_for_backend(PackBackend::CudaForged) {
        Ok(Some(pack)) => Ok(Some(pack)),
        Ok(None) => {
            warn_cuda_fallback("cuda-forged semantic pack was not found", explicit);
            if env_truthy(PACK_STRICT_ENV_VAR) {
                return Err("cuda-forged semantic pack was not found".to_string());
            }
            load_pack_for_backend(PackBackend::CpuLegacy)
        }
        Err(err) => {
            warn_cuda_fallback(&err, explicit);
            if env_truthy(PACK_STRICT_ENV_VAR) {
                return Err(err);
            }
            load_pack_for_backend(PackBackend::CpuLegacy)
        }
    }
}

fn load_pack_for_backend(backend: PackBackend) -> Result<Option<SemanticPack>, String> {
    let Some(path) = resolve_pack_path_for_backend(backend) else {
        return Ok(None);
    };
    let pack = SemanticPack::load_from_path(path)?;
    if pack.backend != backend {
        return Err(format!(
            "semantic pack lane mismatch: requested {}, loaded {}",
            backend.schema_lane(),
            pack.backend.schema_lane()
        ));
    }
    Ok(Some(pack))
}

fn resolve_pack_path_for_backend(backend: PackBackend) -> Option<PathBuf> {
    let env_keys: &[&str] = match backend {
        PackBackend::CpuLegacy => &[PACK_ENV_VAR],
        PackBackend::CudaForged => &[CUDA_PACK_ENV_VAR, PACK_ENV_VAR],
    };
    for key in env_keys {
        if let Some(path) = env::var_os(key).map(PathBuf::from) {
            if path.exists() {
                return Some(path);
            }
        }
    }

    for candidate in default_pack_candidates_for_backend(backend) {
        if candidate.join("manifest.json").exists() || candidate.is_file() {
            return Some(candidate);
        }
    }
    None
}

fn default_pack_candidates_for_backend(backend: PackBackend) -> Vec<PathBuf> {
    let mut candidates = Vec::new();
    if let Ok(exe) = env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            match backend {
                PackBackend::CpuLegacy => {
                    candidates.push(exe_dir.join("semantic").join("current"));
                    candidates.push(exe_dir.join("semantic_pack"));
                }
                PackBackend::CudaForged => {
                    candidates.push(exe_dir.join("semantic").join("cuda_forged").join("current"));
                    candidates.push(exe_dir.join("semantic_cuda_forged_pack"));
                }
            }
        }
    }
    if let Some(home) = env::var_os("KAIN_HOME").map(PathBuf::from) {
        match backend {
            PackBackend::CpuLegacy => candidates.push(home.join(DEFAULT_PACK_RELATIVE_DIR)),
            PackBackend::CudaForged => candidates.push(home.join(DEFAULT_CUDA_PACK_RELATIVE_DIR)),
        }
    }

    let crate_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    match backend {
        PackBackend::CpuLegacy => {
            candidates.push(
                crate_dir
                    .join(".kain")
                    .join("oracle")
                    .join("sempack")
                    .join("current"),
            );
        }
        PackBackend::CudaForged => {
            candidates.push(
                crate_dir
                    .join(".kain")
                    .join("oracle")
                    .join("sempack")
                    .join("cuda_forged")
                    .join("current"),
            );
        }
    }
    candidates
}

#[allow(dead_code)]
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
    default_pack_candidates_for_backend(PackBackend::CpuLegacy)
}

fn warn_cuda_fallback(reason: &str, explicit: bool) {
    if explicit || SemanticLane::was_explicit_cuda() {
        eprintln!("kain semantic: cuda_forged lane unavailable ({reason}); falling back to cpu");
    }
}

fn validate_manifest(manifest: &PackManifest) -> Result<(), String> {
    if manifest.schema != PACK_SCHEMA {
        return Err(format!(
            "semantic pack schema mismatch: expected {PACK_SCHEMA}, got {}",
            manifest.schema
        ));
    }
    if !(1..=2).contains(&manifest.schema_version) {
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
    let backend = PackBackend::from_manifest(manifest);
    match backend {
        PackBackend::CpuLegacy => {
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
        }
        PackBackend::CudaForged => {
            if !cfg!(feature = "cuda-forged-pack") {
                return Err(
                    "cuda-forged semantic pack requires the cuda-forged-pack feature".to_string(),
                );
            }
            if manifest.model.kind != "cuda_forged_packet_reranker_int8" {
                return Err(format!(
                    "cuda-forged semantic pack reranker kind '{}' is not supported",
                    manifest.model.kind
                ));
            }
            if manifest.model.lineage != "kain-transformer-v2-cuda-forged" {
                return Err(format!(
                    "cuda-forged semantic pack lineage '{}' is not transformer-lineage v2",
                    manifest.model.lineage
                ));
            }
            if manifest.forge.backend != "cuda_forged" {
                return Err("cuda-forged semantic pack is missing forge backend provenance".into());
            }
            if manifest.forge.corpus_fingerprint.is_empty()
                || manifest.forge.oracle_manifest_fingerprint.is_empty()
                || manifest.forge.oracle_pack_fingerprint.is_empty()
            {
                return Err("cuda-forged semantic pack is missing artifact fingerprints".into());
            }
            if manifest.forge.kernel_fingerprints.len() < CUDA_FORGE_KERNELS.len() * 3 {
                return Err("cuda-forged semantic pack is missing full kernel lineage".into());
            }
            if manifest.runtime.requires_cuda {
                return Err(
                    "compiler-facing semantic packs must not require CUDA at runtime".into(),
                );
            }
            if !manifest.runtime.offline {
                return Err("cuda-forged semantic pack must be marked offline-forged".into());
            }
        }
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
            let score = score_prototype(pack, prototype, packet, baseline_key, &query_tokens);
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
    pack: &SemanticPack,
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
        score += pack.priors.source_match_bonus;
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

fn repair_score_with_priors(
    pack: &SemanticPack,
    hit: &RetrievedPrototype,
    packet: &DiagnosticSemanticPacket,
) -> f32 {
    let mut score = hit.rerank_score;
    if pack.backend == PackBackend::CudaForged
        && hit.prototype.code == packet.code.as_str()
        && hit.prototype.primary_text == packet.primary_text
    {
        score += pack.priors.exact_repair_bonus;
    }
    score
}

fn cascade_probability_with_priors(
    pack: &SemanticPack,
    packet: &DiagnosticSemanticPacket,
    baseline: &SemanticAnalysisReport,
    hit: &RetrievedPrototype,
    symbol_family_mismatch: bool,
) -> f32 {
    if pack.backend != PackBackend::CudaForged || symbol_family_mismatch {
        return baseline.cascade_probability;
    }
    let exact_root = hit.prototype.code == packet.code.as_str()
        && (hit.prototype.primary_text == packet.primary_text
            || hit.prototype.source_window == packet.source_window);
    if !exact_root || baseline.cascade_probability < 0.55 {
        return baseline.cascade_probability;
    }
    let suppression = pack.priors.cascade_suppression.clamp(0.0, 0.85);
    (baseline.cascade_probability * (1.0 - suppression)).clamp(0.0, 0.99)
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
        "ShaderHostBoundary" => FailureMode::ShaderHostBoundary,
        "ShaderResourceContract" => FailureMode::ShaderResourceContract,
        "CudaKernelContract" => FailureMode::CudaKernelContract,
        "PythonInteropBoundary" => FailureMode::PythonInteropBoundary {
            symbol: prototype.primary_text.clone(),
            import_path: prototype.replacement_text.clone(),
        },
        "CAbiBoundary" => FailureMode::CAbiBoundary {
            symbol: prototype.primary_text.clone(),
            import_path: non_empty_option(prototype.replacement_text.clone()),
        },
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
        FailureMode::ShaderHostBoundary => "ShaderHostBoundary",
        FailureMode::ShaderResourceContract => "ShaderResourceContract",
        FailureMode::CudaKernelContract => "CudaKernelContract",
        FailureMode::PythonInteropBoundary { .. } => "PythonInteropBoundary",
        FailureMode::CAbiBoundary { .. } => "CAbiBoundary",
        FailureMode::WorldDeclarationError => "WorldDeclarationError",
        FailureMode::ActorMessageMismatch => "ActorMessageMismatch",
        FailureMode::ParserDelimiterDamage => "ParserDelimiterDamage",
        FailureMode::ConvergeMismatch => "ConvergeMismatch",
        FailureMode::EntangleViolation => "EntangleViolation",
        FailureMode::GenericUnknown => "GenericUnknown",
    }
}

fn symbol_family_requires_primary_match(mode: &str) -> bool {
    matches!(
        mode,
        "Typo" | "PythonInteropBoundary" | "CAbiBoundary" | "CudaKernelContract"
    )
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
    } else if code.as_str().starts_with("KAIN-SHADER-")
        || code.as_str().starts_with("KAIN-CODEGEN-")
    {
        CompilerPhase::Codegen
    } else {
        CompilerPhase::TypeChecking
    }
}

#[cfg(feature = "cuda-forged-pack")]
fn distill_cuda_forged_prototypes(prototypes: Vec<SemanticPrototype>) -> Vec<SemanticPrototype> {
    prototypes
        .into_iter()
        .map(|mut prototype| {
            if prototype.mode == "CudaKernelContract" {
                prototype.explanation = "CUDA/PTX contract violation. Keep CUDA intrinsics inside compute kernels, declare `use std::cuda`, validate workgroup and dispatch shape, and make residency/binding metadata match the staged kernel.".to_string();
                prototype.explanation_style = "cuda_forged_kernel_contract".to_string();
            } else if prototype.mode == "ShaderResourceContract" {
                prototype.explanation = "GPU resource contract violation. Check StorageBuffer element layout, uniform binding slots, workgroup/dispatch dimensions, and residency metadata before the shader bundle is forged.".to_string();
                prototype.explanation_style = "cuda_forged_resource_contract".to_string();
            } else if prototype.mode == "ConvergeMismatch" {
                let lower = prototype.source_window.to_ascii_lowercase();
                if lower.contains("orchestrate")
                    || lower.contains("stage ")
                    || lower.contains("dispatch")
                    || lower.contains("residency")
                    || lower.contains("transfer")
                {
                    prototype.explanation = "Orchestrate/converge graph mismatch. Start at the first stage whose lane, residency, transfer, guard, fallback, or law requirement cannot satisfy the reference contract.".to_string();
                    prototype.explanation_style = "cuda_forged_orchestrate_contract".to_string();
                }
            } else if prototype.mode == "ParserDelimiterDamage" {
                let lower = prototype.source_window.to_ascii_lowercase();
                if lower.contains("dispatch ") {
                    prototype.explanation = "Parser recovery: this looks like a damaged dispatch statement or nearby block header. Check the `dispatch \"key\" [x, y, z]` shape and the surrounding ':' / indentation boundary first.".to_string();
                    prototype.explanation_style = "cuda_forged_dispatch_recovery".to_string();
                }
            }
            prototype
        })
        .collect()
}

#[cfg(feature = "cuda-forged-pack")]
fn collect_cuda_kernel_fingerprints(oracle_root: &Path) -> io::Result<BTreeMap<String, String>> {
    let mut fingerprints = BTreeMap::new();
    for (name, stem) in CUDA_FORGE_KERNELS {
        let dir = oracle_root.join("gpu").join(stem);
        fingerprints.insert(
            format!("{name}.bundle"),
            required_file_fingerprint(&dir.join(format!("{stem}.shader_bundle.json")))?,
        );
        fingerprints.insert(
            format!("{name}.residency"),
            required_file_fingerprint(&dir.join("kain_compute_residency.json"))?,
        );
        fingerprints.insert(
            format!("{name}.ptx"),
            required_file_fingerprint(&dir.join(format!("{stem}.derived.ptx")))?,
        );
    }
    Ok(fingerprints)
}

#[cfg(feature = "cuda-forged-pack")]
fn required_file_fingerprint(path: &Path) -> io::Result<String> {
    let bytes = fs::read(path).map_err(|err| {
        io::Error::new(
            err.kind(),
            format!(
                "required CUDA forge artifact missing or unreadable {}: {err}",
                path.display()
            ),
        )
    })?;
    Ok(fnv1a64_hex(&bytes))
}

#[cfg(feature = "cuda-forged-pack")]
fn fingerprint_prototypes(prototypes: &[SemanticPrototype]) -> String {
    let mut bytes = Vec::new();
    for prototype in prototypes {
        bytes.extend_from_slice(prototype.code.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.mode.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.primary_text.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.source_window.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.repair_id.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.repair_description.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.replacement_text.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.explanation.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(prototype.explanation_style.as_bytes());
        bytes.push(0xff);
    }
    fnv1a64_hex(&bytes)
}

#[cfg(feature = "cuda-forged-pack")]
fn fnv1a64_hex(bytes: &[u8]) -> String {
    let mut hash: u64 = 0xcbf29ce484222325;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
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

#[cfg(feature = "cuda-forged-pack")]
fn write_cuda_forged_reranker(path: &Path) -> io::Result<()> {
    let mut out = Vec::new();
    out.extend_from_slice(RERANKER_MAGIC);
    write_u32(&mut out, RERANKER_FEATURES as u32);
    write_i32(&mut out, 420);
    write_i32(&mut out, 5200);
    out.extend_from_slice(&[14, 12, 18, 8, 11, 11, 14, 7]);
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

    #[cfg(feature = "cuda-forged-pack")]
    fn fake_cuda_oracle_root(name: &str) -> PathBuf {
        let root = unique_pack_dir(name).join("oracle");
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).expect("create fake oracle root");
        fs::write(root.join("kain_error_oracle.bin"), b"fake-oracle-pack")
            .expect("write fake oracle pack");
        fs::write(
            root.join("kain_error_oracle.manifest.json"),
            br#"{"schema":"kain.error.semantic.oracle.v1","code_ok":true,"kain_ok":true}"#,
        )
        .expect("write fake oracle manifest");

        for (_, stem) in CUDA_FORGE_KERNELS {
            let dir = root.join("gpu").join(stem);
            fs::create_dir_all(&dir).expect("create fake kernel dir");
            fs::write(
                dir.join(format!("{stem}.shader_bundle.json")),
                format!(r#"{{"kernel":"{stem}","target":"cuda"}}"#),
            )
            .expect("write fake shader bundle");
            fs::write(
                dir.join("kain_compute_residency.json"),
                format!(r#"{{"kernel":"{stem}","residency":true}}"#),
            )
            .expect("write fake residency");
            fs::write(
                dir.join(format!("{stem}.derived.ptx")),
                format!("// fake ptx for {stem}"),
            )
            .expect("write fake ptx");
        }
        root
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
        .source_window(
            "fn main() -> Int:\n    let score = python_with_pykain_case_cout()\n    return 0",
        );

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
            enhanced
                .dynamic_explanation
                .contains("python_with_pykain_case_cout"),
            "typo family mismatch should keep the baseline explanation"
        );
        assert!(
            !enhanced.dynamic_explanation.contains("fs_read_texx"),
            "cross-family typo prototype must not override the explanation"
        );

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn pack_disable_keeps_fallback_rules() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "definitely_not_in_pack",
        );
        let baseline = expert::analyze(&packet);
        let previous_disable = env::var_os(PACK_DISABLE_ENV_VAR);
        env::set_var(PACK_DISABLE_ENV_VAR, "1");
        let result = analyze_with_default_pack(&packet, baseline);
        match previous_disable {
            Some(value) => env::set_var(PACK_DISABLE_ENV_VAR, value),
            None => env::remove_var(PACK_DISABLE_ENV_VAR),
        }
        assert_eq!(result.backend, "fallback_rules");
    }

    #[cfg(feature = "cuda-forged-pack")]
    #[test]
    fn cuda_forged_pack_requires_full_oracle_artifacts() {
        let dir = unique_pack_dir("cuda-missing-artifacts-pack");
        let oracle = unique_pack_dir("cuda-missing-artifacts-oracle");
        let _ = fs::remove_dir_all(&dir);
        let _ = fs::remove_dir_all(&oracle);
        fs::create_dir_all(&oracle).expect("create oracle root");
        fs::write(oracle.join("kain_error_oracle.bin"), b"fake").expect("write fake oracle");

        let err = write_cuda_forged_semantic_pack_from_corpus(&dir, &oracle)
            .expect_err("missing oracle artifacts should fail");
        assert!(
            err.to_string().contains("required CUDA forge artifact"),
            "unexpected error: {err}"
        );
        let _ = fs::remove_dir_all(&dir);
        let _ = fs::remove_dir_all(&oracle);
    }

    #[cfg(feature = "cuda-forged-pack")]
    #[test]
    fn cuda_forged_pack_round_trips_and_changes_backend() {
        let dir = unique_pack_dir("cuda-forged-pack");
        let oracle = fake_cuda_oracle_root("cuda-forged-oracle");
        let _ = fs::remove_dir_all(&dir);
        write_cuda_forged_semantic_pack_from_corpus(&dir, &oracle)
            .expect("write cuda-forged semantic pack");
        let pack = SemanticPack::load_from_path(&dir).expect("load cuda-forged pack");
        assert_eq!(pack.schema_version_string(), "kain.semantic.pack.v1:2");
        assert_eq!(pack.backend_name(), "pack_cuda_forged");

        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "prntln",
        )
        .source_window("// ERROR: Unknown identifier - typo in function name\nfn main() -> Int:\n    let result = prntln(\"hello\")\n    return 0\n")
        .visible_symbols(vec!["println".to_string()])
        .downstream(vec![
            DiagnosticCode::TypeGeneric,
            DiagnosticCode::TypeDuplicateSymbol,
        ]);

        let baseline = expert::analyze(&packet);
        assert!(baseline.cascade_probability >= 0.55);
        let enhanced = analyze_with_pack(&pack, &packet, baseline.clone());
        assert_eq!(enhanced.backend, "pack_cuda_forged");
        assert_eq!(
            enhanced.pack_schema_version.as_deref(),
            Some("kain.semantic.pack.v1:2")
        );
        assert!(
            enhanced.cascade_probability < baseline.cascade_probability,
            "cuda-forged exact roots should suppress cascade notes"
        );
        assert_eq!(
            enhanced
                .ranked_repairs
                .first()
                .and_then(|repair| repair.replacement_text.as_deref()),
            Some("println")
        );

        let _ = fs::remove_dir_all(&dir);
        let _ = fs::remove_dir_all(oracle.parent().unwrap());
    }
}
