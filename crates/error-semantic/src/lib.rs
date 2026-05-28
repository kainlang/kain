//! kain-error-semantic — Semantic diagnostic coprocessor for the Kain compiler.
//!
//! Corpus-baked, data-driven error intelligence. Lane A is a pure-Rust expert
//! rules engine that classifies failures, ranks repairs, estimates cascade
//! probability, and generates context-sensitive explanations using a
//! build-time-indexed symbol corpus from stdlib, smoketest, and user sources.
//!
//! No ML runtime. No allocator pressure on the hot path. Sub-millisecond.

pub mod corpus_db;
pub mod expert;
pub mod packet;

pub use expert::analyze;
pub use packet::DiagnosticSemanticPacket;

/// The failure mode taxonomy for Kain diagnostics.
#[derive(Debug, Clone, serde::Serialize)]
pub enum FailureMode {
    Typo { intended: String },
    MissingImport { module: String, import_path: String },
    MissingSurface,
    OwnershipViolation,
    ShaderStageMismatch,
    WorldDeclarationError,
    ActorMessageMismatch,
    ParserDelimiterDamage,
    ConvergeMismatch,
    EntangleViolation,
    GenericUnknown,
}

/// A scored repair suggestion.
#[derive(Debug, Clone, serde::Serialize)]
pub struct RankedRepair {
    pub repair_id: String,
    pub description: String,
    pub score: f32,
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
}

/// Top-level semantic coprocessor. Stateless for now — all intelligence
/// lives in the expert engine and the baked corpus.
pub struct SemanticCoprocessor;

impl SemanticCoprocessor {
    pub fn new() -> Self {
        Self
    }

    pub fn analyze(&self, packet: &DiagnosticSemanticPacket) -> SemanticAnalysisReport {
        expert::analyze(packet)
    }
}
