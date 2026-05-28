//! Semantic packet bridge.
//!
//! `kain-error-semantic` consumes the canonical packet contract owned by
//! `kain-error` so the compiler-facing schema lives in exactly one place.

pub use kain_error::{DeterministicRepair, DiagnosticSemanticPacket};

/// Back-compat alias for older lane-A helper code.
pub type CandidateRepair = DeterministicRepair;
