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
