use crate::error::{KainError, KainResult};
use kain_asm::ImportAsmOutput;
use std::path::Path;

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> KainResult<ImportAsmOutput> {
    kain_asm::import_asm(input, format, out_kn, validate_only)
        .map_err(|e| KainError::runtime(e.to_string()))
}
