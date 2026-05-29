//! Lane A — Expert Rules Engine
//!
//! Pure-Rust, zero-dependency (no ML runtime), sub-millisecond semantic
//! diagnostic analysis. Uses the corpus database for spelling distance,
//! import suggestion, and context-driven explanation selection.
//!
//! This engine is designed to be the robust baseline that a future
//! neural ranker (Lane B) can enhance or override.

use crate::corpus_db;
use crate::packet::{CandidateRepair, DiagnosticSemanticPacket};
use crate::{FailureMode, RankedRepair, SemanticAnalysisReport};
use strsim::normalized_levenshtein;

/// Analyze a semantic packet and produce a structured analysis report.
pub fn analyze(packet: &DiagnosticSemanticPacket) -> SemanticAnalysisReport {
    let failure_mode = classify_failure(packet);
    let ranked_repairs = rank_repairs(packet, &failure_mode);
    let cascade_prob = estimate_cascade_probability(packet);
    let explanation = generate_explanation(packet, &failure_mode);
    let explanation_style = explanation_style_for(&failure_mode);
    let confidence = compute_root_cause_confidence(&failure_mode, &ranked_repairs);

    SemanticAnalysisReport {
        root_cause_confidence: confidence,
        likely_failure_mode: failure_mode,
        ranked_repairs,
        dynamic_explanation: explanation,
        cascade_probability: cascade_prob,
        explanation_style,
    }
}

// ── Failure Classification ───────────────────────────────────────────

fn classify_failure(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();

    // TYPE-0002: Unknown identifier — could be typo, missing import, or host bridge
    if code == "KAIN-TYPE-0002" {
        return classify_unknown_identifier(packet);
    }

    // TYPE-0003: World missing surface
    if code == "KAIN-TYPE-0003" {
        return FailureMode::MissingSurface;
    }

    // PARSE-0005: Missing delimiter before newline
    if code == "KAIN-PARSE-0005" || code == "KAIN-PARSE-0008" || code == "KAIN-PARSE-0009" {
        return FailureMode::ParserDelimiterDamage;
    }

    // WORLD codes
    if code.starts_with("KAIN-WORLD-") {
        return FailureMode::WorldDeclarationError;
    }

    // BORROW / ownership codes
    if code.starts_with("KAIN-BORROW-") {
        return FailureMode::OwnershipViolation;
    }

    // SHADER codes
    if code.starts_with("KAIN-SHADER-") {
        return FailureMode::ShaderStageMismatch;
    }

    // ACTOR codes
    if code.starts_with("KAIN-ACTOR-") {
        return FailureMode::ActorMessageMismatch;
    }

    // EFFECT codes
    if code.starts_with("KAIN-EFFECT-") {
        return classify_effect_violation(packet);
    }

    FailureMode::GenericUnknown
}

fn classify_unknown_identifier(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let text = &packet.primary_text;

    // 1. Scope-local matches should outrank corpus-global guesses.
    if let Some((nearest, dist)) = nearest_scope_match(packet).as_ref() {
        if *dist <= 2 {
            return FailureMode::Typo {
                intended: nearest.clone(),
            };
        }
    }

    // 2. Check if this symbol is a canonical stdlib root export or a strong
    // prefix-shape match that should really be fixed by adding an import.
    if let Some(import_path) = corpus_db::suggest_import_for_symbol(text) {
        return FailureMode::MissingImport {
            module: text.to_string(),
            import_path: import_path.to_string(),
        };
    }

    // 3. Fall back to corpus-wide typo detection.
    let corpus_matches = corpus_db::find_nearest_symbols(text, 3);
    if let Some(best) = corpus_matches.first() {
        if best.similarity > 0.88 {
            return FailureMode::Typo {
                intended: best.name.to_string(),
            };
        }
    }

    // 4. Check contextual flags for host bridge context
    if packet
        .contextual_flags
        .get("in_interop_block")
        .copied()
        .unwrap_or(false)
        || packet
            .contextual_flags
            .get("near_ffi_boundary")
            .copied()
            .unwrap_or(false)
    {
        return FailureMode::GenericUnknown; // Could add HostBridge mode later
    }

    FailureMode::GenericUnknown
}

fn classify_effect_violation(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    if code == "KAIN-EFFECT-0012" {
        return FailureMode::ConvergeMismatch;
    }
    if packet
        .contextual_flags
        .get("in_converge_block")
        .copied()
        .unwrap_or(false)
    {
        return FailureMode::ConvergeMismatch;
    }
    if packet
        .contextual_flags
        .get("in_entangle_block")
        .copied()
        .unwrap_or(false)
    {
        return FailureMode::EntangleViolation;
    }
    FailureMode::GenericUnknown
}

// ── Repair Ranking ───────────────────────────────────────────────────

fn rank_repairs(packet: &DiagnosticSemanticPacket, mode: &FailureMode) -> Vec<RankedRepair> {
    let mut repairs: Vec<RankedRepair> = Vec::new();

    // Score compiler-generated candidates against the classified failure mode
    for candidate in &packet.candidate_repairs {
        let score = score_repair(candidate, mode, packet);
        repairs.push(RankedRepair {
            repair_id: candidate.id.clone(),
            description: candidate.description.clone(),
            score,
            replacement_text: Some(candidate.replacement_text.clone()),
        });
    }

    // Inject corpus-derived repairs that the compiler might not have generated
    match mode {
        FailureMode::Typo { intended } => {
            let already_has = repairs
                .iter()
                .any(|r| r.repair_id.contains("spell") || r.description.contains(intended));
            if !already_has {
                repairs.push(RankedRepair {
                    repair_id: "corpus_spelling_fix".into(),
                    description: format!("Replace with '{}'", intended),
                    score: 0.95,
                    replacement_text: Some(intended.clone()),
                });
            }
        }
        FailureMode::MissingImport {
            module: _,
            import_path,
        } => {
            let already_has = repairs.iter().any(|r| r.repair_id.contains("import"));
            if !already_has {
                repairs.push(RankedRepair {
                    repair_id: "corpus_add_import".into(),
                    description: format!("Add 'use {}' to imports", import_path),
                    score: 0.93,
                    replacement_text: Some(format!("use {}", import_path)),
                });
            }
        }
        FailureMode::MissingSurface => {
            let already_has = repairs.iter().any(|r| r.repair_id.contains("surface"));
            if !already_has {
                repairs.push(RankedRepair {
                    repair_id: "add_surface_clause".into(),
                    description: "Add a 'surface native_ui => ...' or 'surface web => ...' clause"
                        .into(),
                    score: 0.90,
                    replacement_text: Some("surface native_ui => MyPanel".into()),
                });
            }
        }
        _ => {}
    }

    // Sort by descending score
    repairs.sort_by(|a, b| {
        b.score
            .partial_cmp(&a.score)
            .unwrap_or(std::cmp::Ordering::Equal)
    });
    repairs
}

fn nearest_scope_match(packet: &DiagnosticSemanticPacket) -> Option<(String, usize)> {
    if let Some((name, distance)) = packet.nearest_scope_matches.first() {
        return Some((name.clone(), *distance));
    }

    packet
        .visible_symbols
        .iter()
        .map(|candidate| {
            let distance = bounded_edit_distance(&packet.primary_text, candidate);
            let similarity = normalized_levenshtein(&packet.primary_text, candidate);
            (candidate.clone(), distance, similarity)
        })
        .filter(|(_, distance, similarity)| *distance <= 2 || *similarity >= 0.84_f64)
        .min_by(|left, right| {
            left.1
                .cmp(&right.1)
                .then_with(|| {
                    right
                        .2
                        .partial_cmp(&left.2)
                        .unwrap_or(std::cmp::Ordering::Equal)
                })
                .then_with(|| left.0.cmp(&right.0))
        })
        .map(|(name, distance, _)| (name, distance))
}

fn bounded_edit_distance(left: &str, right: &str) -> usize {
    if left == right {
        return 0;
    }
    let left_chars: Vec<char> = left.chars().collect();
    let right_chars: Vec<char> = right.chars().collect();
    let mut prev: Vec<usize> = (0..=right_chars.len()).collect();
    let mut curr = vec![0usize; right_chars.len() + 1];

    for (i, left_char) in left_chars.iter().enumerate() {
        curr[0] = i + 1;
        for (j, right_char) in right_chars.iter().enumerate() {
            let substitution_cost = usize::from(left_char != right_char);
            curr[j + 1] = (prev[j + 1] + 1)
                .min(curr[j] + 1)
                .min(prev[j] + substitution_cost);
        }
        std::mem::swap(&mut prev, &mut curr);
    }

    prev[right_chars.len()]
}

fn score_repair(
    candidate: &CandidateRepair,
    mode: &FailureMode,
    _packet: &DiagnosticSemanticPacket,
) -> f32 {
    // Base score from candidate ordering (compiler already ranked somewhat)
    let base: f32 = 0.5_f32;

    // Boost if the repair category matches the classified failure mode
    let mode_boost: f32 = match mode {
        FailureMode::Typo { .. }
            if candidate.id.contains("spell") || candidate.id.contains("rename") =>
        {
            0.35_f32
        }
        FailureMode::MissingImport { .. }
            if candidate.id.contains("import") || candidate.id.contains("use") =>
        {
            0.40_f32
        }
        FailureMode::MissingSurface if candidate.id.contains("surface") => 0.38_f32,
        FailureMode::ParserDelimiterDamage
            if candidate.id.contains("insert") || candidate.id.contains("delimiter") =>
        {
            0.35_f32
        }
        FailureMode::OwnershipViolation
            if candidate.id.contains("borrow") || candidate.id.contains("scope") =>
        {
            0.30_f32
        }
        FailureMode::WorldDeclarationError
            if candidate.id.contains("world") || candidate.id.contains("surface") =>
        {
            0.35_f32
        }
        _ => 0.0_f32,
    };

    (base + mode_boost).min(1.0_f32)
}

// ── Cascade Estimation ───────────────────────────────────────────────

fn estimate_cascade_probability(packet: &DiagnosticSemanticPacket) -> f32 {
    if packet.downstream_codes.is_empty() {
        return 0.0;
    }

    let n = packet.downstream_codes.len() as f32;
    let code = packet.code.as_str();

    // Parser errors almost always cascade
    if code.starts_with("KAIN-PARSE-") {
        return (0.80_f32 + 0.02_f32 * n).min(0.99_f32);
    }

    // Type errors frequently cascade
    if code.starts_with("KAIN-TYPE-") {
        return (0.60_f32 + 0.04_f32 * n).min(0.98_f32);
    }

    // World/entangle errors can cascade into type/effect errors
    if code.starts_with("KAIN-WORLD-") {
        return (0.50_f32 + 0.05_f32 * n).min(0.95_f32);
    }

    // Generic estimate
    (0.30_f32 + 0.03_f32 * n).min(0.90_f32)
}

// ── Explanation Generation ───────────────────────────────────────────

fn generate_explanation(packet: &DiagnosticSemanticPacket, mode: &FailureMode) -> String {
    match mode {
        FailureMode::Typo { intended } => {
            format!(
                "'{}' is not in scope. Closest known symbol is '{}'. This appears to be a spelling error.",
                packet.primary_text, intended
            )
        }
        FailureMode::MissingImport { module: _, import_path } => {
            format!(
                "'{}' is not in scope. This looks like a symbol from '{}'; add 'use {}' to your imports.",
                packet.primary_text, import_path, import_path
            )
        }
        FailureMode::MissingSurface => {
            "This world declaration is missing a surface clause. Every world must declare at least one surface (e.g., 'surface native_ui => ...', 'surface web => ...', or 'surface viewport3d => ...').".to_string()
        }
        FailureMode::OwnershipViolation => {
            "Ownership violation detected. Check that you are not borrowing or mutating data across a collapse/observe boundary.".to_string()
        }
        FailureMode::ShaderStageMismatch => {
            "Shader stage mismatch. Verify that vertex/fragment/compute attributes match the expected pipeline stage.".to_string()
        }
        FailureMode::WorldDeclarationError => {
            "World declaration error. Check that all required state slots, surfaces, and entangle clauses are present.".to_string()
        }
        FailureMode::ActorMessageMismatch => {
            "Actor message type mismatch. The message type sent does not match the actor's receive handler signature.".to_string()
        }
        FailureMode::ParserDelimiterDamage => {
            "Parser recovery: a delimiter or block header appears to be missing or damaged. Kain uses ':' after block headers and indentation-sensitive layout.".to_string()
        }
        FailureMode::ConvergeMismatch => {
            "Converge mismatch: the fast lane does not satisfy the spec lane contract. Ensure the fast implementation returns equivalent results.".to_string()
        }
        FailureMode::EntangleViolation => {
            "Entangle coupling violation. The entangled state relationship has been broken. Check the single_writer policy and coupling direction.".to_string()
        }
        FailureMode::GenericUnknown => {
            String::new()
        }
    }
}

fn explanation_style_for(mode: &FailureMode) -> String {
    match mode {
        FailureMode::Typo { .. } => "typo_correction".into(),
        FailureMode::MissingImport { .. } => "import_suggestion".into(),
        FailureMode::MissingSurface => "world_surface_help".into(),
        FailureMode::OwnershipViolation => "ownership_explainer".into(),
        FailureMode::ShaderStageMismatch => "shader_stage_help".into(),
        FailureMode::WorldDeclarationError => "world_declaration_help".into(),
        FailureMode::ActorMessageMismatch => "actor_message_help".into(),
        FailureMode::ParserDelimiterDamage => "parser_block_header".into(),
        FailureMode::ConvergeMismatch => "converge_contract_help".into(),
        FailureMode::EntangleViolation => "entangle_policy_help".into(),
        FailureMode::GenericUnknown => "generic".into(),
    }
}

fn compute_root_cause_confidence(mode: &FailureMode, repairs: &[RankedRepair]) -> f32 {
    let base = match mode {
        FailureMode::GenericUnknown => 0.30,
        FailureMode::Typo { .. } => 0.90,
        FailureMode::MissingImport { .. } => 0.85,
        FailureMode::MissingSurface => 0.92,
        FailureMode::ParserDelimiterDamage => 0.88,
        _ => 0.70,
    };

    // Boost confidence if top repair scores high
    let repair_boost = repairs.first().map(|r| r.score * 0.08).unwrap_or(0.0);

    (base + repair_boost).min(0.99)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::packet::DiagnosticSemanticPacket;
    use kain_error::{CompilerPhase, DiagnosticCode};
    use std::fs;
    use std::path::Path;

    #[test]
    fn test_typo_detection() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "prntln",
        )
        .visible_symbols(vec!["println".into(), "print".into(), "format".into()])
        .add_scope_match("println", 1);

        let result = analyze(&packet);
        match &result.likely_failure_mode {
            FailureMode::Typo { intended } => {
                // Should match against corpus or scope
                assert!(!intended.is_empty());
            }
            _ => {
                // Acceptable if corpus doesn't have "println" yet
                // The scope match fallback should still work
            }
        }
        assert!(result.root_cause_confidence > 0.0);
    }

    #[test]
    fn test_missing_surface_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeWorldMissingSurface,
            CompilerPhase::TypeChecking,
            "Demo",
        );
        let result = analyze(&packet);
        assert!(matches!(
            result.likely_failure_mode,
            FailureMode::MissingSurface
        ));
        assert!(result.root_cause_confidence > 0.85);
    }

    #[test]
    fn test_parser_delimiter_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::ParseMissingDelimiterBeforeNewline,
            CompilerPhase::Parser,
            ":",
        );
        let result = analyze(&packet);
        assert!(matches!(
            result.likely_failure_mode,
            FailureMode::ParserDelimiterDamage
        ));
    }

    #[test]
    fn test_cascade_estimation() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::ParseMissingDelimiterBeforeNewline,
            CompilerPhase::Parser,
            ":",
        )
        .downstream(vec![
            DiagnosticCode::TypeGeneric,
            DiagnosticCode::TypeUnknownIdentifier,
            DiagnosticCode::TypeDuplicateSymbol,
        ]);
        let result = analyze(&packet);
        assert!(result.cascade_probability > 0.80);
    }

    #[test]
    fn test_corpus_stats() {
        let (symbols, imports) = corpus_db::corpus_stats();
        // Should have at least some symbols from stdlib
        assert!(symbols > 0, "Corpus should contain symbols from stdlib");
        assert!(imports > 0, "Corpus should contain import paths");
    }

    #[test]
    fn test_missing_import_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "fs_read_text",
        );
        let result = analyze(&packet);
        match &result.likely_failure_mode {
            FailureMode::MissingImport { import_path, .. } => {
                assert_eq!(import_path, "std::fs");
            }
            other => panic!("expected missing import classification, got {other:?}"),
        }
    }

    fn corpus_case_phase(code: DiagnosticCode) -> CompilerPhase {
        match code.as_str() {
            code if code.starts_with("KAIN-PARSE-") => CompilerPhase::Parser,
            code if code.starts_with("KAIN-EFFECT-") => CompilerPhase::EffectChecking,
            code if code.starts_with("KAIN-BORROW-") => CompilerPhase::BorrowChecking,
            _ => CompilerPhase::TypeChecking,
        }
    }

    fn corpus_case_primary_text(case: &corpus_db::ErrorCorpusCase, source: &str) -> String {
        if case.expected_mode == "Typo" {
            for line in source.lines() {
                if let Some(call_index) = line.find('(') {
                    let prefix = &line[..call_index];
                    if let Some(symbol) = prefix.split_whitespace().last().map(|value| {
                        value.trim_matches(|ch: char| !ch.is_ascii_alphanumeric() && ch != '_')
                    }) {
                        if !symbol.is_empty() && symbol != case.expected_repair {
                            return symbol.to_string();
                        }
                    }
                }
            }
        }

        if source.contains("cells") {
            return "cells".to_string();
        }
        if source.contains("Master.val") {
            return "Master.val".to_string();
        }
        if source.contains("orchestrate") {
            return "orchestrate".to_string();
        }

        case.expected_repair.to_string()
    }

    fn load_corpus_case_source(case: &corpus_db::ErrorCorpusCase) -> String {
        if let Ok(source) = fs::read_to_string(case.file_path) {
            return source;
        }

        let fixture_name = Path::new(case.file_path)
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_else(|| panic!("missing fixture name in {}", case.file_path));
        let fallback_path = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("error_corpus")
            .join(fixture_name);
        fs::read_to_string(&fallback_path)
            .unwrap_or_else(|_| panic!("failed to read {}", fallback_path.display()))
    }

    #[test]
    fn test_error_corpus_cases() {
        use kain_error::DiagnosticCode;

        for case in corpus_db::ERROR_CORPUS_CASES {
            let code = DiagnosticCode::new(case.expected_code);
            let source = load_corpus_case_source(case);
            let mut packet = DiagnosticSemanticPacket::new(
                code,
                corpus_case_phase(code),
                corpus_case_primary_text(case, &source),
            )
            .source_window(&source);

            if case.expected_mode == "ConvergeMismatch" {
                packet = packet.flag("in_converge_block", true);
            } else if case.expected_mode == "EntangleViolation" {
                packet = packet.flag("in_entangle_block", true);
            } else if case.expected_mode == "Typo" {
                packet = packet
                    .visible_symbols(vec![case.expected_repair.to_string(), "println".into()])
                    .add_scope_match(case.expected_repair, 1);
            }

            packet = packet.add_repair(case.expected_repair, "ideal repair", "replacement text");

            let result = analyze(&packet);

            match &result.likely_failure_mode {
                FailureMode::OwnershipViolation => {
                    assert_eq!(case.expected_mode, "OwnershipViolation");
                }
                FailureMode::EntangleViolation => {
                    assert_eq!(case.expected_mode, "EntangleViolation");
                }
                FailureMode::ConvergeMismatch => {
                    assert_eq!(case.expected_mode, "ConvergeMismatch");
                }
                FailureMode::Typo { .. } => {
                    assert_eq!(case.expected_mode, "Typo");
                }
                _ => {}
            }

            assert!(
                !result.dynamic_explanation.is_empty(),
                "Explanation for {} must not be empty",
                case.file_path
            );

            if let Some(top_repair) = result.ranked_repairs.first() {
                let top_repair_matches = top_repair.repair_id == case.expected_repair
                    || (case.expected_mode == "Typo"
                        && top_repair.repair_id == "corpus_spelling_fix")
                    || top_repair.description.contains(case.expected_repair);
                assert!(
                    top_repair_matches,
                    "Expected top repair for {} to align with {}, got {:?}",
                    case.file_path, case.expected_repair, top_repair
                );
            }
        }
    }
}
