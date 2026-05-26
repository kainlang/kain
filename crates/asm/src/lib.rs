mod dialects;
mod error;

use std::path::Path;

pub use dialects::furby_6502::{
    ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore,
};
pub use error::{AsmError, AsmResult};

type ImporterFn = fn(&Path, &str, Option<&Path>, bool) -> AsmResult<ImportAsmOutput>;

struct AsmDialect {
    id: &'static str,
    aliases: &'static [&'static str],
    importer: ImporterFn,
}

const DIALECTS: &[AsmDialect] = &[
    AsmDialect {
        id: "6502-furby",
        aliases: &["furby-6502", "6502", "furby"],
        importer: dialects::furby_6502::import_asm,
    },
    AsmDialect {
        id: "lr35902-gameboy",
        aliases: &["gameboy-lr35902", "gb-lr35902", "lr35902", "gameboy"],
        importer: dialects::gameboy_lr35902::import_asm,
    },
    AsmDialect {
        id: "z80",
        aliases: &["z80-arcade", "z80-spectrum", "z80-msx"],
        importer: dialects::z80::import_asm,
    },
];

pub fn supported_formats() -> Vec<&'static str> {
    DIALECTS.iter().map(|d| d.id).collect()
}

pub fn supported_format_aliases() -> Vec<&'static str> {
    let mut out = Vec::<&'static str>::new();
    for dialect in DIALECTS {
        out.push(dialect.id);
        out.extend(dialect.aliases.iter().copied());
    }
    out
}

fn normalize_format(format: &str) -> String {
    format.trim().to_ascii_lowercase()
}

fn find_dialect(format: &str) -> Option<&'static AsmDialect> {
    let norm = normalize_format(format);
    DIALECTS
        .iter()
        .find(|dialect| dialect.id == norm || dialect.aliases.iter().any(|alias| *alias == norm))
}

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> AsmResult<ImportAsmOutput> {
    if let Some(dialect) = find_dialect(format) {
        return (dialect.importer)(input, format, out_kn, validate_only);
    }
    Err(AsmError::runtime(format!(
        "Unsupported asm format '{}'. Supported: {}",
        format,
        supported_format_aliases().join(", ")
    )))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alias_resolution_is_data_driven() {
        let gb = find_dialect("gameboy").expect("dialect");
        assert_eq!(gb.id, "lr35902-gameboy");

        let furby = find_dialect("6502-furby").expect("dialect");
        assert_eq!(furby.id, "6502-furby");
    }
}
