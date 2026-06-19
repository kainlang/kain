//! kain-semantic — Semantic diagnostic coprocessor for the Kain compiler.
//!
//! Corpus-baked, data-driven error intelligence. The hot compiler path stays
//! CPU-only: a pure-Rust expert engine establishes a baseline and a sidecar pack
//! reranks repairs, cascade risk, and explanations. Experimental CUDA-forged
//! packs are built offline and then loaded through the same deterministic CPU
//! reader when the lane selector asks for them.
//!
//! No GPU or ML runtime on the hot path. No allocator pressure in the normal
//! diagnostic flow. Sub-millisecond.

pub mod corpus_db;
pub mod expert;
pub mod pack;
pub mod packet;

use kain_error::{
    DiagnosticReport, DiagnosticSemanticRepair, DiagnosticSemanticSummary, DiagnosticSeverity,
};
pub use packet::DiagnosticSemanticPacket;

/// The failure mode taxonomy for Kain diagnostics.
#[derive(Debug, Clone, serde::Serialize)]
pub enum FailureMode {
    Typo {
        intended: String,
    },
    MissingImport {
        module: String,
        import_path: String,
    },
    MissingSurface,
    OwnershipViolation,
    ShaderStageMismatch,
    ShaderHostBoundary,
    ShaderResourceContract,
    CudaKernelContract,
    PythonInteropBoundary {
        symbol: String,
        import_path: String,
    },
    CAbiBoundary {
        symbol: String,
        import_path: Option<String>,
    },
    WorldDeclarationError,
    ActorMessageMismatch,
    ParserDelimiterDamage,
    ConvergeMismatch,
    EntangleViolation,
    GenericUnknown,
}

impl FailureMode {
    fn as_key(&self) -> &'static str {
        match self {
            Self::Typo { .. } => "typo",
            Self::MissingImport { .. } => "missing_import",
            Self::MissingSurface => "missing_surface",
            Self::OwnershipViolation => "ownership_violation",
            Self::ShaderStageMismatch => "shader_stage_mismatch",
            Self::ShaderHostBoundary => "shader_host_boundary",
            Self::ShaderResourceContract => "shader_resource_contract",
            Self::CudaKernelContract => "cuda_kernel_contract",
            Self::PythonInteropBoundary { .. } => "python_interop_boundary",
            Self::CAbiBoundary { .. } => "c_abi_boundary",
            Self::WorldDeclarationError => "world_declaration_error",
            Self::ActorMessageMismatch => "actor_message_mismatch",
            Self::ParserDelimiterDamage => "parser_delimiter_damage",
            Self::ConvergeMismatch => "converge_mismatch",
            Self::EntangleViolation => "entangle_violation",
            Self::GenericUnknown => "generic_unknown",
        }
    }
}

/// A scored repair suggestion.
#[derive(Debug, Clone, serde::Serialize)]
pub struct RankedRepair {
    pub repair_id: String,
    pub description: String,
    pub score: f32,
    pub replacement_text: Option<String>,
}

/// The full analysis report produced by the semantic coprocessor.
#[derive(Debug, Clone, serde::Serialize)]
pub struct SemanticAnalysisReport {
    pub root_cause_confidence: f32,
    pub likely_failure_mode: FailureMode,
    pub ranked_repairs: Vec<RankedRepair>,
    pub dynamic_explanation: String,
    pub cascade_probability: f32,
    pub explanation_style: String,
    pub backend: String,
    pub pack_schema_version: Option<String>,
}

impl SemanticAnalysisReport {
    pub fn to_summary(&self) -> DiagnosticSemanticSummary {
        DiagnosticSemanticSummary {
            failure_mode: self.likely_failure_mode.as_key().to_string(),
            explanation_style: self.explanation_style.clone(),
            explanation: self.dynamic_explanation.clone(),
            root_cause_confidence: self.root_cause_confidence,
            cascade_probability: self.cascade_probability,
            repairs: self
                .ranked_repairs
                .iter()
                .map(|repair| DiagnosticSemanticRepair {
                    repair_id: repair.repair_id.clone(),
                    description: repair.description.clone(),
                    score: repair.score,
                    replacement_text: repair.replacement_text.clone(),
                })
                .collect(),
            backend: self.backend.clone(),
            pack_schema_version: self.pack_schema_version.clone(),
        }
    }
}

/// Top-level semantic coprocessor. Stateless for now; lane selection and pack
/// provenance live in the sidecar pack module.
pub struct SemanticCoprocessor;

impl SemanticCoprocessor {
    pub fn new() -> Self {
        Self
    }

    pub fn analyze(&self, packet: &DiagnosticSemanticPacket) -> SemanticAnalysisReport {
        analyze(packet)
    }
}

pub fn analyze(packet: &DiagnosticSemanticPacket) -> SemanticAnalysisReport {
    let baseline = expert::analyze(packet);
    pack::analyze_with_default_pack(packet, baseline)
}

pub fn enrich_report(
    report: DiagnosticReport,
    packet: &DiagnosticSemanticPacket,
) -> DiagnosticReport {
    let analysis = analyze(packet);
    let summary = analysis.to_summary();
    let is_tentative = summary.failure_mode == "generic_unknown"
        && summary.explanation.is_empty()
        && summary.repairs.is_empty();
    if is_tentative {
        return report;
    }

    let mut report = report.semantic_summary(summary.clone());

    if report.fixits.is_empty()
        && report.severity == DiagnosticSeverity::Error
        && summary.failure_mode == "typo"
    {
        if let (Some(primary_span), Some(repair)) = (
            report.primary_span,
            summary
                .repairs
                .iter()
                .find(|repair| repair.replacement_text.is_some()),
        ) {
            if let Some(replacement) = &repair.replacement_text {
                report = report.fixit_certain(
                    primary_span,
                    replacement.clone(),
                    format!("replace with '{}'", replacement),
                );
            }
        }
    }

    report
}

/// Lightweight enrichment for codegen-phase errors.
///
/// Unlike the full `enrich_report()` which requires AST-level context
/// (visible symbols, scope matches, etc.), this function works with only
/// the error message and span --- what the LLVM codegen has available.
/// It classifies the failure mode and adds targeted help text.
pub fn enrich_codegen_error(
    report: &mut DiagnosticReport,
    error_message: &str,
) {
    // Classify failure mode from the error message pattern
    let mode = classify_codegen_failure(error_message);

    // Add classification as a note
    if let Some(mode_str) = mode.as_ref() {
        report.tags.push(format!("failure-mode:{}", mode_str));
    }

    match mode.as_deref() {
        Some("atomic-ordering") => {
            report.help.push(
                "Atomic ordering codes: 0=relaxed, 2=acquire, 3=release, 4=acq_rel, 5=seq_cst. \
                 Check that store only uses relaxed/release/seq_cst and compare_exchange \
                 failure ordering is not stronger than success.".to_string()
            );
        }
        Some("unsupported-target") => {
            report.help.push(
                "This construct requires a specific target architecture. \
                 Consider wrapping it in an axiom block with `when target(...)` to gate it.".to_string()
            );
        }
        Some("type-mapping") => {
            report.help.push(
                "A Kain type does not have a direct LLVM representation. \
                 Check that you're using types with known LLVM mappings (Int→i64, Float→double, etc.).".to_string()
            );
        }
        Some("bitcast-width") => {
            report.help.push(
                "bitcast requires source and target types to have the same byte width at the LLVM level. \
                 Use size_of<T>() to compare widths before bitcasting.".to_string()
            );
        }
        Some("method-arity") => {
            report.help.push(
                "Check the expected argument count for this method. \
                 unwrap() takes 0 args, expect(msg) takes 1, unwrap_or(default) takes 1.".to_string()
            );
        }
        Some("actor-message") => {
            report.help.push(
                "Actor message names must match the actor's handler declarations. \
                 Use `send reply_to.Reply(value = ...)` for reply ports.".to_string()
            );
        }
        Some("shader-stage") => {
            report.help.push(
                "Shader stage validation failed. Check that vertex/fragment/compute inputs \
                 and outputs match the expected stage interface.".to_string()
            );
        }
        _ => {
            // No specific help available, add generic suggestion
            report.help.push(
                "Try running `kain check` for additional validation. \
                 If the error persists, this may be a codegen-internal issue.".to_string()
            );
        }
    }
}

/// Classify a codegen error message into a failure mode category.
fn classify_codegen_failure(msg: &str) -> Option<String> {
    let lower = msg.to_lowercase();
    if lower.contains("atomic") || lower.contains("ordering") {
        Some("atomic-ordering".to_string())
    } else if lower.contains("x86_64-only") || lower.contains("unsupported target")
        || lower.contains("not supported on")
    {
        Some("unsupported-target".to_string())
    } else if lower.contains("bitcast") || lower.contains("width")
        || lower.contains("llvm type")
    {
        Some("bitcast-width".to_string())
    } else if lower.contains("expects") && (lower.contains("argument") || lower.contains("arg")) {
        Some("method-arity".to_string())
    } else if lower.contains("actor") || lower.contains("message") || lower.contains("reply") {
        Some("actor-message".to_string())
    } else if lower.contains("shader") || lower.contains("stage") || lower.contains("vertex")
        || lower.contains("fragment") || lower.contains("compute")
    {
        Some("shader-stage".to_string())
    } else if lower.contains("type") && (lower.contains("map") || lower.contains("represent")
        || lower.contains("lower"))
    {
        Some("type-mapping".to_string())
    } else {
        None
    }
}
