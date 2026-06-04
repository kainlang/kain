use kain_core::CompileTarget;
use kain_error::DiagnosticReport;
use std::path::Path;

#[derive(Debug, Clone)]
pub struct FormatResult {
    pub formatted: String,
    pub already_formatted: bool,
    pub diagnostics: Vec<DiagnosticReport>,
}

pub fn format_document(_path: &Path, source: &str, _target: CompileTarget) -> FormatResult {
    match kain_fmt::format_source(source) {
        Ok(formatted) => FormatResult {
            already_formatted: formatted == source,
            formatted,
            diagnostics: Vec::new(),
        },
        Err(error) => FormatResult {
            formatted: source.to_string(),
            already_formatted: false,
            diagnostics: error.to_diagnostic_reports(),
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formatter_uses_canonical_formatter() {
        let source = "fn main() -> Int:\n    return 1\n";
        let result = format_document(Path::new("main.kn"), source, CompileTarget::Llvm);
        assert!(result.diagnostics.is_empty());
        assert!(result.formatted.contains("fn main"));
    }
}
