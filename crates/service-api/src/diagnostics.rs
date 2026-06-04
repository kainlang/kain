use crate::index::{analyze_document, DocumentAnalysis};
use kain_core::types::TypedProgram;
use kain_core::CompileTarget;
use kain_driver::DriverSession;
use kain_error::DiagnosticReport;
use std::path::Path;

#[derive(Debug, Clone)]
pub struct CheckResult {
    pub diagnostics: Vec<DiagnosticReport>,
    pub typed_program: Option<TypedProgram>,
    pub analysis: Option<DocumentAnalysis>,
}

impl CheckResult {
    pub fn passed(&self) -> bool {
        self.diagnostics.is_empty() && self.typed_program.is_some()
    }
}

pub fn check_document(path: &Path, source: &str, target: CompileTarget) -> CheckResult {
    let session = DriverSession::default();
    match session.frontend_to_checked_program_with_source_path(source, Some(path), target) {
        Ok(checked) => CheckResult {
            diagnostics: Vec::new(),
            typed_program: Some(checked.typed),
            analysis: analyze_document(path, source),
        },
        Err(error) => CheckResult {
            diagnostics: error.to_diagnostic_reports(),
            typed_program: None,
            analysis: analyze_document(path, source),
        },
    }
}

pub fn check_workspace<'a>(
    documents: impl IntoIterator<Item = (&'a Path, &'a str)>,
    target: CompileTarget,
) -> CheckResult {
    let mut diagnostics = Vec::new();
    let mut last_typed = None;
    let mut last_analysis = None;
    for (path, source) in documents {
        let checked = check_document(path, source, target);
        diagnostics.extend(checked.diagnostics);
        if checked.typed_program.is_some() {
            last_typed = checked.typed_program;
        }
        if checked.analysis.is_some() {
            last_analysis = checked.analysis;
        }
    }
    CheckResult {
        typed_program: if diagnostics.is_empty() {
            last_typed
        } else {
            None
        },
        diagnostics,
        analysis: last_analysis,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check_result_preserves_type_diagnostics() {
        let source = "fn main() -> Int:\n    return missing_name\n";
        let result = check_document(Path::new("broken.kn"), source, CompileTarget::Llvm);
        assert!(!result.diagnostics.is_empty());
        assert!(result.typed_program.is_none());
    }
}
