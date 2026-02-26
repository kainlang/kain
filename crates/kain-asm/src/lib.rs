mod dialects;
mod error;

use std::path::Path;

pub use dialects::furby_6502::{ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore};
pub use error::{AsmError, AsmResult};

const DIALECT_6502_FURBY: &str = "6502-furby";
const DIALECT_LR35902_GAMEBOY: &str = "lr35902-gameboy";

pub fn supported_formats() -> &'static [&'static str] {
    &[DIALECT_6502_FURBY, DIALECT_LR35902_GAMEBOY]
}

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> AsmResult<ImportAsmOutput> {
    match format {
        DIALECT_6502_FURBY => dialects::furby_6502::import_asm(input, format, out_kn, validate_only),
        DIALECT_LR35902_GAMEBOY
        | "gameboy-lr35902"
        | "gb-lr35902"
        | "lr35902"
        | "gameboy" => dialects::gameboy_lr35902::import_asm(input, format, out_kn, validate_only),
        _ => Err(AsmError::runtime(format!(
            "Unsupported asm format '{}'. Supported: {}",
            format,
            supported_formats().join(", ")
        ))),
    }
}
