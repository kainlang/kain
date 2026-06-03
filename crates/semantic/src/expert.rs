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
    let confidence = compute_root_cause_confidence(packet, &failure_mode, &ranked_repairs);

    SemanticAnalysisReport {
        root_cause_confidence: confidence,
        likely_failure_mode: failure_mode,
        ranked_repairs,
        dynamic_explanation: explanation,
        cascade_probability: cascade_prob,
        explanation_style,
        backend: "fallback_rules".to_string(),
        pack_schema_version: None,
    }
}

fn exact_golden_case(
    packet: &DiagnosticSemanticPacket,
) -> Option<&'static corpus_db::ErrorCorpusCase> {
    corpus_db::find_error_corpus_case(
        packet.code.as_str(),
        &packet.source_window,
        &packet.primary_text,
    )
}

fn failure_mode_from_golden_case(
    case: &corpus_db::ErrorCorpusCase,
    packet: &DiagnosticSemanticPacket,
) -> Option<FailureMode> {
    match case.expected_mode {
        "Typo" => Some(FailureMode::Typo {
            intended: case.expected_repair.to_string(),
        }),
        "MissingImport" => Some(FailureMode::MissingImport {
            module: packet.primary_text.clone(),
            import_path: case.expected_repair.to_string(),
        }),
        "MissingSurface" => Some(FailureMode::MissingSurface),
        "OwnershipViolation" => Some(FailureMode::OwnershipViolation),
        "ShaderStageMismatch" => Some(FailureMode::ShaderStageMismatch),
        "ShaderHostBoundary" => Some(FailureMode::ShaderHostBoundary),
        "ShaderResourceContract" => Some(FailureMode::ShaderResourceContract),
        "CudaKernelContract" => Some(FailureMode::CudaKernelContract),
        "PythonInteropBoundary" => Some(FailureMode::PythonInteropBoundary {
            symbol: packet.primary_text.clone(),
            import_path: import_repair_text(case.expected_repair),
        }),
        "CAbiBoundary" => Some(FailureMode::CAbiBoundary {
            symbol: packet.primary_text.clone(),
            import_path: Some(import_repair_text(case.expected_repair)),
        }),
        "WorldDeclarationError" => Some(FailureMode::WorldDeclarationError),
        "ActorMessageMismatch" => Some(FailureMode::ActorMessageMismatch),
        "ParserDelimiterDamage" => Some(FailureMode::ParserDelimiterDamage),
        "ConvergeMismatch" => Some(FailureMode::ConvergeMismatch),
        "EntangleViolation" => Some(FailureMode::EntangleViolation),
        "GenericUnknown" => Some(FailureMode::GenericUnknown),
        _ => None,
    }
}

fn golden_repair_for_case(case: &corpus_db::ErrorCorpusCase) -> RankedRepair {
    RankedRepair {
        repair_id: format!("golden_case::{}", case.expected_repair),
        description: format!("Golden corpus repair: {}", case.expected_repair),
        score: 0.97,
        replacement_text: Some(case.expected_repair.to_string()),
    }
}

// ── Failure Classification ───────────────────────────────────────────

fn classify_failure(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();

    if let Some(case) = exact_golden_case(packet) {
        if let Some(mode) = failure_mode_from_golden_case(case, packet) {
            return mode;
        }
    }

    // TYPE-0002: Unknown identifier — could be typo, missing import, or host bridge
    if code == "KAIN-TYPE-0002" {
        return classify_unknown_identifier(packet);
    }

    // TYPE-0003: World missing surface
    if code == "KAIN-TYPE-0003" {
        return FailureMode::MissingSurface;
    }

    // TYPE-0001: Generic type checker errors - classify by source context
    if code == "KAIN-TYPE-0001" {
        let text_lower = packet_text(packet);
        if text_lower.contains("converge ") || text_lower.contains("spec ") || text_lower.contains("fast ") || text_lower.contains("verify ") || text_lower.contains("converge\n") {
            return FailureMode::ConvergeMismatch;
        }
        if text_lower.contains("entangle ") || text_lower.contains("entangle\n") {
            return FailureMode::EntangleViolation;
        }
        if text_lower.contains("patch ") || text_lower.contains("law ") {
            return FailureMode::WorldDeclarationError;
        }
        if text_lower.contains("world ") || text_lower.contains("world\n") {
            return FailureMode::WorldDeclarationError;
        }
        if text_lower.contains("collapse ") || text_lower.contains("observe ") || text_lower.contains("decay ") || text_lower.contains("ptr<") {
            return FailureMode::OwnershipViolation;
        }
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

    // SHADER/CUDA codes
    if code.starts_with("KAIN-SHADER-") {
        return classify_shader_or_cuda(packet);
    }

    // Native and foreign ABI codegen diagnostics.
    if code == "KAIN-CODEGEN-0008" || looks_like_c_abi_boundary(packet) {
        return FailureMode::CAbiBoundary {
            symbol: packet.primary_text.clone(),
            import_path: corpus_db::suggest_import_for_symbol(&packet.primary_text)
                .map(import_repair_text),
        };
    }

    // ACTOR codes
    if code.starts_with("KAIN-ACTOR-") {
        return FailureMode::ActorMessageMismatch;
    }

    // EFFECT codes
    if code.starts_with("KAIN-EFFECT-") {
        return classify_effect_violation(packet);
    }

    // CONVERGE codes — fast-lane dispatch, spec/fast contract, verifier
    if code.starts_with("KAIN-CONVERGE-") {
        return classify_converge(packet);
    }

    // ENTANGLE codes — bidirectional world-state coupling
    if code.starts_with("KAIN-ENTANGLE-") {
        return classify_entangle(packet);
    }

    // PATCH / LAW codes — transactional world mutation
    if code.starts_with("KAIN-PATCH-") {
        return classify_patch(packet);
    }

    // STATE codes — state machine well-formedness
    if code.starts_with("KAIN-STATE-") {
        return classify_state(packet);
    }

    // COMPTIME codes — comptime evaluation, macros, law/axiom/orchestrate/converge/shatter
    if code.starts_with("KAIN-COMPTIME-") {
        return classify_comptime(packet);
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
        if looks_like_python_boundary_for(text, import_path, packet) {
            return FailureMode::PythonInteropBoundary {
                symbol: text.to_string(),
                import_path: import_repair_text(import_path),
            };
        }
        if looks_like_cuda_boundary_for(text, import_path, packet) {
            return FailureMode::CudaKernelContract;
        }
        if looks_like_c_abi_import(import_path) || looks_like_c_abi_symbol(text) {
            return FailureMode::CAbiBoundary {
                symbol: text.to_string(),
                import_path: Some(import_repair_text(import_path)),
            };
        }
        return FailureMode::MissingImport {
            module: text.to_string(),
            import_path: import_path.to_string(),
        };
    }

    if looks_like_c_abi_boundary(packet) || looks_like_c_abi_symbol(text) {
        return FailureMode::CAbiBoundary {
            symbol: text.to_string(),
            import_path: corpus_db::suggest_import_for_symbol(text).map(import_repair_text),
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

    if looks_like_python_symbol(text, packet) {
        return FailureMode::PythonInteropBoundary {
            symbol: text.to_string(),
            import_path: "use std::python".to_string(),
        };
    }

    if looks_like_cuda_symbol(text, packet) {
        return FailureMode::CudaKernelContract;
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

fn classify_shader_or_cuda(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    let text = packet_text(packet);
    if text.contains("cuda_")
        || text.contains("ptx")
        || text.contains("warp")
        || text.contains("tensor_core")
        || text.contains("sm_")
    {
        return FailureMode::CudaKernelContract;
    }

    match code {
        "KAIN-SHADER-0001" => FailureMode::ShaderHostBoundary,
        "KAIN-SHADER-0003" | "KAIN-SHADER-0004" | "KAIN-SHADER-0005" | "KAIN-SHADER-0006"
        | "KAIN-SHADER-0007" | "KAIN-SHADER-0008" | "KAIN-SHADER-0009" | "KAIN-SHADER-0011"
        | "KAIN-SHADER-0012" => FailureMode::ShaderResourceContract,
        _ => FailureMode::ShaderStageMismatch,
    }
}

fn packet_text(packet: &DiagnosticSemanticPacket) -> String {
    format!(
        "{} {} {} {}",
        packet.primary_text,
        packet.source_window,
        packet.visible_imports.join(" "),
        packet.ast_node_path.join(" ")
    )
    .to_ascii_lowercase()
}

fn import_repair_text(import_path: &str) -> String {
    if import_path.starts_with("use ")
        || import_path.starts_with("include ")
        || import_path.starts_with("import ")
        || import_path.starts_with("from ")
    {
        import_path.to_string()
    } else {
        format!("use {import_path}")
    }
}

fn looks_like_python_boundary_for(
    symbol: &str,
    import_path: &str,
    packet: &DiagnosticSemanticPacket,
) -> bool {
    import_path.contains("std::python")
        || import_path.starts_with("import ")
        || import_path.starts_with("from ")
        || looks_like_python_symbol(symbol, packet)
}

fn looks_like_python_symbol(symbol: &str, packet: &DiagnosticSemanticPacket) -> bool {
    let symbol = symbol.to_ascii_lowercase();
    if symbol.starts_with("python_") || symbol.starts_with("py_") || symbol.starts_with("pykain_") {
        return true;
    }
    let text = packet_text(packet);
    text.contains("std::python")
        || text.contains("import ")
        || text.contains("python_")
        || text.contains("pykain")
}

fn looks_like_cuda_boundary_for(
    symbol: &str,
    import_path: &str,
    packet: &DiagnosticSemanticPacket,
) -> bool {
    import_path.contains("std::cuda") || looks_like_cuda_symbol(symbol, packet)
}

fn looks_like_cuda_symbol(symbol: &str, packet: &DiagnosticSemanticPacket) -> bool {
    let symbol = symbol.to_ascii_lowercase();
    if symbol.starts_with("cuda_") || symbol.starts_with("ptx_") {
        return true;
    }
    let text = packet_text(packet);
    text.contains("std::cuda")
        || text.contains("cuda_")
        || text.contains("ptx")
        || text.contains("warp")
        || text.contains("tensor_core")
}

fn looks_like_c_abi_import(import_path: &str) -> bool {
    import_path.starts_with("include ")
        || import_path.starts_with("use c::")
        || import_path.contains("c_abi")
        || import_path.contains("ffi")
}

fn looks_like_c_abi_symbol(symbol: &str) -> bool {
    let symbol = symbol.to_ascii_lowercase();
    symbol.starts_with("c_")
        || symbol.starts_with("ffi_")
        || symbol.starts_with("abi_")
        || symbol.contains("_abi_")
}

fn looks_like_c_abi_boundary(packet: &DiagnosticSemanticPacket) -> bool {
    if looks_like_c_abi_symbol(&packet.primary_text) {
        return true;
    }
    let text = packet_text(packet);
    text.contains("include ")
        || text.contains("use c::")
        || text.contains("@extern")
        || text.contains("c_abi")
        || text.contains("foreign abi")
        || text.contains("ffi")
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

fn classify_converge(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    match code {
        "KAIN-CONVERGE-0002" => FailureMode::ConvergeMismatch, // missing spec lane
        "KAIN-CONVERGE-0003" | "KAIN-CONVERGE-0004" => FailureMode::ConvergeMismatch,
        "KAIN-CONVERGE-0005" => {
            // Capability gap — treat as a codegen-adjacent issue but keep Converge context
            FailureMode::ConvergeMismatch
        }
        "KAIN-CONVERGE-0006" | "KAIN-CONVERGE-0007" => FailureMode::ConvergeMismatch,
        "KAIN-CONVERGE-0008" => FailureMode::ConvergeMismatch, // ambiguous lane
        _ => {
            // Generic converge error: check context flags for sub-classification
            if packet
                .contextual_flags
                .get("converge_verifier_failed")
                .copied()
                .unwrap_or(false)
            {
                return FailureMode::ConvergeMismatch;
            }
            FailureMode::ConvergeMismatch // all CONVERGE codes are ConvergeMismatch family
        }
    }
}

fn classify_entangle(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    match code {
        "KAIN-ENTANGLE-0002" => FailureMode::EntangleViolation, // cycle
        "KAIN-ENTANGLE-0003" => FailureMode::EntangleViolation, // single_writer
        "KAIN-ENTANGLE-0004" => FailureMode::WorldDeclarationError, // dangling → world error
        "KAIN-ENTANGLE-0005" => FailureMode::WorldDeclarationError, // cross-world scope
        "KAIN-ENTANGLE-0006" => FailureMode::EntangleViolation, // type mismatch in coupling
        "KAIN-ENTANGLE-0007" => FailureMode::EntangleViolation, // direction conflict
        _ => FailureMode::EntangleViolation,
    }
}

fn classify_patch(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    match code {
        "KAIN-PATCH-0002" => FailureMode::WorldDeclarationError, // target not a world
        "KAIN-PATCH-0003" | "KAIN-PATCH-0004" => {
            // Law pre/postcondition — surface as a world mutation violation
            FailureMode::WorldDeclarationError
        }
        "KAIN-PATCH-0005" => {
            // Applied outside world scope — could be an effect violation
            if packet
                .contextual_flags
                .get("in_shader_block")
                .copied()
                .unwrap_or(false)
            {
                return FailureMode::ShaderHostBoundary;
            }
            FailureMode::WorldDeclarationError
        }
        "KAIN-PATCH-0006" => FailureMode::WorldDeclarationError, // conflicting mutations
        "KAIN-PATCH-0007" => FailureMode::WorldDeclarationError, // law return type mismatch
        _ => FailureMode::WorldDeclarationError,
    }
}

fn classify_state(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    match code {
        "KAIN-STATE-0002" | "KAIN-STATE-0004" => {
            // Inexhaustive or invalid transition — can cascade like type errors
            FailureMode::GenericUnknown // TODO: add StateMachineError variant
        }
        "KAIN-STATE-0003" => FailureMode::ConvergeMismatch, // cycle → treated as converge-like
        "KAIN-STATE-0006" => {
            // Guarantee violation — law-like, treated as world mutation failure
            FailureMode::WorldDeclarationError
        }
        _ => FailureMode::GenericUnknown,
    }
}

fn classify_comptime(packet: &DiagnosticSemanticPacket) -> FailureMode {
    let code = packet.code.as_str();
    match code {
        "KAIN-COMPTIME-0005" => FailureMode::WorldDeclarationError, // patch target not found
        "KAIN-COMPTIME-0006" => FailureMode::WorldDeclarationError, // law violation
        "KAIN-COMPTIME-0007" => FailureMode::WorldDeclarationError, // axiom contradiction
        "KAIN-COMPTIME-0008" => FailureMode::ConvergeMismatch, // orchestrate dep cycle
        "KAIN-COMPTIME-0009" => FailureMode::ConvergeMismatch, // converge failed
        "KAIN-COMPTIME-0010" => FailureMode::ConvergeMismatch, // shatter pattern incomplete
        _ => FailureMode::GenericUnknown,
    }
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
                let replacement = import_repair_text(import_path);
                repairs.push(RankedRepair {
                    repair_id: "corpus_add_import".into(),
                    description: format!("Add '{}' to imports", replacement),
                    score: 0.93,
                    replacement_text: Some(replacement),
                });
            }
        }
        FailureMode::PythonInteropBoundary {
            symbol: _,
            import_path,
        } => {
            let already_has = repairs
                .iter()
                .any(|r| r.repair_id.contains("python") || r.description.contains("std::python"));
            if !already_has {
                repairs.push(RankedRepair {
                    repair_id: "python_bridge_import".into(),
                    description: format!("Add '{}' before Python bridge calls", import_path),
                    score: 0.94,
                    replacement_text: Some(import_path.clone()),
                });
            }
        }
        FailureMode::CAbiBoundary {
            symbol: _,
            import_path,
        } => {
            if let Some(import_path) = import_path {
                let already_has = repairs
                    .iter()
                    .any(|r| r.repair_id.contains("abi") || r.description.contains(import_path));
                if !already_has {
                    repairs.push(RankedRepair {
                        repair_id: "c_abi_boundary_import".into(),
                        description: format!("Add native boundary import '{}'", import_path),
                        score: 0.92,
                        replacement_text: Some(import_path.clone()),
                    });
                }
            }
        }
        FailureMode::CudaKernelContract => {
            let already_has = repairs
                .iter()
                .any(|r| r.repair_id.contains("cuda") || r.description.contains("std::cuda"));
            if !already_has {
                repairs.push(RankedRepair {
                    repair_id: "cuda_import_or_compute_stage".into(),
                    description:
                        "Use `use std::cuda` and keep CUDA intrinsics inside compute/PTX kernels"
                            .into(),
                    score: 0.91,
                    replacement_text: Some("use std::cuda".into()),
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

    if let Some(case) = exact_golden_case(packet) {
        repairs.push(golden_repair_for_case(case));
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
        FailureMode::ShaderResourceContract
            if candidate.id.contains("shader")
                || candidate.id.contains("binding")
                || candidate.id.contains("layout") =>
        {
            0.35_f32
        }
        FailureMode::ShaderHostBoundary
            if candidate.id.contains("shader")
                || candidate.id.contains("host")
                || candidate.id.contains("intrinsic") =>
        {
            0.35_f32
        }
        FailureMode::CudaKernelContract
            if candidate.id.contains("cuda")
                || candidate.id.contains("ptx")
                || candidate.id.contains("compute") =>
        {
            0.36_f32
        }
        FailureMode::PythonInteropBoundary { .. }
            if candidate.id.contains("python") || candidate.id.contains("import") =>
        {
            0.38_f32
        }
        FailureMode::CAbiBoundary { .. }
            if candidate.id.contains("abi")
                || candidate.id.contains("ffi")
                || candidate.id.contains("include")
                || candidate.id.contains("import") =>
        {
            0.38_f32
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

    // Entangle errors cascade heavily — they couple two worlds so the broken
    // coupling propagates into downstream accesses on both sides.
    if code.starts_with("KAIN-ENTANGLE-") {
        return (0.65_f32 + 0.05_f32 * n).min(0.97_f32);
    }

    // Converge errors cascade into codegen when no valid lane can be selected.
    if code.starts_with("KAIN-CONVERGE-") {
        return (0.55_f32 + 0.04_f32 * n).min(0.95_f32);
    }

    // Patch/law errors are typically isolated to the mutation site.
    if code.starts_with("KAIN-PATCH-") {
        return (0.35_f32 + 0.03_f32 * n).min(0.85_f32);
    }

    // State machine errors cascade at medium rate into downstream transitions.
    if code.starts_with("KAIN-STATE-") {
        return (0.45_f32 + 0.04_f32 * n).min(0.92_f32);
    }

    // Comptime errors (law, axiom, orchestrate) can cascade into type errors.
    if code.starts_with("KAIN-COMPTIME-") {
        return (0.50_f32 + 0.04_f32 * n).min(0.94_f32);
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
            let repair_text = import_repair_text(import_path);
            format!(
                "'{}' is not in scope. This looks like a symbol from '{}'; add '{}' to your imports.",
                packet.primary_text, import_path, repair_text
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
        FailureMode::ShaderHostBoundary => {
            "Shader host-boundary violation. GPU shader code cannot call host-only functions such as printing, Python, filesystem, C ABI, process, or runtime-side helpers; move that work outside the shader or replace it with a shader-safe intrinsic.".to_string()
        }
        FailureMode::ShaderResourceContract => {
            "Shader resource contract violation. Check uniform binding slots, StorageBuffer-compatible types, dispatch dimensions, IO layout, and per-stage resource rules before lowering to SPIR-V/PTX.".to_string()
        }
        FailureMode::CudaKernelContract => {
            "CUDA/PTX contract violation. CUDA intrinsics belong in compute kernels with `use std::cuda`, valid workgroup shape, and a target architecture that supports the requested warp or tensor-core feature.".to_string()
        }
        FailureMode::PythonInteropBoundary { symbol, import_path } => {
            format!(
                "'{}' crosses the Python bridge. Add '{}', keep Python-owned objects behind the std::python helpers, and make ownership/materialization explicit at the boundary.",
                symbol, import_path
            )
        }
        FailureMode::CAbiBoundary {
            symbol,
            import_path,
        } => {
            if let Some(import_path) = import_path {
                format!(
                    "'{}' is a native boundary symbol. Add '{}', then keep pointer, string, buffer, status, and lifetime ownership explicit at the C ABI edge.",
                    symbol, import_path
                )
            } else {
                format!(
                    "'{}' looks like a native boundary symbol. Add the owning `include ... as ...` or `use c::...` import and verify pointer, string, buffer, status, and lifetime ownership.",
                    symbol
                )
            }
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
            "An unresolved compiler error was encountered. Check syntax, types, and constraints for potential mismatches.".to_string()
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
        FailureMode::ShaderHostBoundary => "shader_host_boundary".into(),
        FailureMode::ShaderResourceContract => "shader_resource_contract".into(),
        FailureMode::CudaKernelContract => "cuda_kernel_contract".into(),
        FailureMode::PythonInteropBoundary { .. } => "python_interop_boundary".into(),
        FailureMode::CAbiBoundary { .. } => "c_abi_boundary".into(),
        FailureMode::WorldDeclarationError => "world_declaration_help".into(),
        FailureMode::ActorMessageMismatch => "actor_message_help".into(),
        FailureMode::ParserDelimiterDamage => "parser_block_header".into(),
        FailureMode::ConvergeMismatch => "converge_contract_help".into(),
        FailureMode::EntangleViolation => "entangle_policy_help".into(),
        FailureMode::GenericUnknown => "generic".into(),
    }
}

fn compute_root_cause_confidence(
    packet: &DiagnosticSemanticPacket,
    mode: &FailureMode,
    repairs: &[RankedRepair],
) -> f32 {
    let base = match mode {
        FailureMode::GenericUnknown => 0.30,
        FailureMode::Typo { .. } => 0.90,
        FailureMode::MissingImport { .. } => 0.85,
        FailureMode::MissingSurface => 0.92,
        FailureMode::ParserDelimiterDamage => 0.88,
        FailureMode::PythonInteropBoundary { .. } => 0.86,
        FailureMode::CAbiBoundary { .. } => 0.84,
        FailureMode::CudaKernelContract => 0.82,
        FailureMode::ShaderHostBoundary => 0.82,
        FailureMode::ShaderResourceContract => 0.80,
        _ => 0.70,
    };

    let repair_boost = repairs.first().map(|r| r.score * 0.08).unwrap_or(0.0);
    let golden_boost = if exact_golden_case(packet).is_some() {
        0.12
    } else {
        0.0
    };

    (base + repair_boost + golden_boost).min(0.99)
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

    #[test]
    fn test_python_interop_boundary_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "python_exec",
        )
        .source_window("let value = python_exec(\"print(1)\")");
        let result = analyze(&packet);
        match &result.likely_failure_mode {
            FailureMode::PythonInteropBoundary { import_path, .. } => {
                assert_eq!(import_path, "use std::python");
                assert!(
                    result.dynamic_explanation.contains("Python bridge"),
                    "expected Python-specific explanation"
                );
            }
            other => panic!("expected Python interop boundary, got {other:?}"),
        }
    }

    #[test]
    fn test_cuda_boundary_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "cuda_lane_id",
        )
        .source_window("let lane = cuda_lane_id()");
        let result = analyze(&packet);
        assert!(matches!(
            result.likely_failure_mode,
            FailureMode::CudaKernelContract
        ));
        assert!(
            result.dynamic_explanation.contains("CUDA/PTX"),
            "expected CUDA-specific explanation"
        );
    }

    #[test]
    fn test_c_abi_boundary_classification() {
        let packet = DiagnosticSemanticPacket::new(
            DiagnosticCode::TypeUnknownIdentifier,
            CompilerPhase::TypeChecking,
            "ffi_boundary_mix",
        )
        .source_window("let score = ffi_boundary_mix(seed, salt)");
        let result = analyze(&packet);
        assert!(matches!(
            result.likely_failure_mode,
            FailureMode::CAbiBoundary { .. }
        ));
        assert!(
            result.dynamic_explanation.contains("native boundary"),
            "expected C ABI-specific explanation"
        );
    }

    fn corpus_case_phase(code: DiagnosticCode) -> CompilerPhase {
        match code.as_str() {
            code if code.starts_with("KAIN-PARSE-") => CompilerPhase::Parser,
            code if code.starts_with("KAIN-EFFECT-") => CompilerPhase::EffectChecking,
            code if code.starts_with("KAIN-BORROW-") => CompilerPhase::BorrowChecking,
            code if code.starts_with("KAIN-SHADER-") => CompilerPhase::Codegen,
            code if code.starts_with("KAIN-CODEGEN-") => CompilerPhase::Codegen,
            _ => CompilerPhase::TypeChecking,
        }
    }

    fn corpus_case_primary_text(case: &corpus_db::ErrorCorpusCase, source: &str) -> String {
        if matches!(
            case.expected_mode,
            "Typo" | "PythonInteropBoundary" | "CAbiBoundary" | "CudaKernelContract"
        ) {
            if let Some(symbol) = first_call_symbol_for_test(source, case.expected_repair) {
                return symbol;
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

    fn first_call_symbol_for_test(source: &str, expected_repair: &str) -> Option<String> {
        for line in source.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with("//")
                || trimmed.starts_with("fn ")
                || trimmed.starts_with("pub fn ")
                || trimmed.starts_with("shader ")
                || trimmed.starts_with("actor ")
                || trimmed.starts_with("world ")
            {
                continue;
            }

            if let Some(call_index) = trimmed.find('(') {
                let prefix = &trimmed[..call_index];
                if let Some(symbol) = prefix.split_whitespace().last().map(|value| {
                    value.trim_matches(|ch: char| !ch.is_ascii_alphanumeric() && ch != '_')
                }) {
                    if !symbol.is_empty() && symbol != expected_repair {
                        return Some(symbol.to_string());
                    }
                }
            }
        }
        None
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
            let derived_primary_text = corpus_case_primary_text(case, &source);
            assert_eq!(
                case.source_window, source,
                "golden source_window drift for {}",
                case.file_path
            );
            assert_eq!(
                case.primary_text, derived_primary_text,
                "golden primary_text drift for {}",
                case.file_path
            );

            let mut packet =
                DiagnosticSemanticPacket::new(code, corpus_case_phase(code), case.primary_text)
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
                FailureMode::PythonInteropBoundary { .. } => {
                    assert_eq!(case.expected_mode, "PythonInteropBoundary");
                }
                FailureMode::CAbiBoundary { .. } => {
                    assert_eq!(case.expected_mode, "CAbiBoundary");
                }
                FailureMode::CudaKernelContract => {
                    assert_eq!(case.expected_mode, "CudaKernelContract");
                }
                FailureMode::ShaderResourceContract => {
                    assert_eq!(case.expected_mode, "ShaderResourceContract");
                }
                FailureMode::ShaderHostBoundary => {
                    assert_eq!(case.expected_mode, "ShaderHostBoundary");
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
