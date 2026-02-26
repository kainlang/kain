mod dialects;
mod error;

use std::path::Path;

pub use dialects::furby_6502::{ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore};
pub use error::{AsmError, AsmResult};

const DIALECT_6502_FURBY: &str = "6502-furby";

pub fn supported_formats() -> &'static [&'static str] {
    &[DIALECT_6502_FURBY]
}

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> AsmResult<ImportAsmOutput> {
    match format {
        DIALECT_6502_FURBY => dialects::furby_6502::import_asm(input, format, out_kn, validate_only),
        _ => Err(AsmError::runtime(format!(
            "Unsupported asm format '{}'. Supported: {}",
            format,
            supported_formats().join(", ")
        ))),
    }
}
