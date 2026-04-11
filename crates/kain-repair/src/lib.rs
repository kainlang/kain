use std::borrow::Cow;

mod engine;
mod registry;

pub use registry::{RepairRule, RuleMetadata, RuleSafety, RuleScope};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RepairInput<'a> {
    pub text: Cow<'a, str>,
    pub profile: RepairProfile,
    pub mode: RepairMode,
}

impl<'a> RepairInput<'a> {
    pub fn new(text: impl Into<Cow<'a, str>>) -> Self {
        Self {
            text: text.into(),
            profile: RepairProfile::default(),
            mode: RepairMode::ApplySafe,
        }
    }
    pub fn with_profile(mut self, profile: RepairProfile) -> Self {
        self.profile = profile;
        self
    }
    pub fn with_mode(mut self, mode: RepairMode) -> Self {
        self.mode = mode;
        self
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RepairProfile {
    pub trim_trailing_whitespace: bool,
    pub ensure_final_newline: bool,
    pub collapse_extra_blank_lines: bool,
    pub normalize_line_endings: bool,
    pub fix_unterminated_block_comments: bool,
    pub normalize_indentation: bool,
    pub normalize_declaration_headers: bool,
    pub flatten_nested_declarations: bool,
    pub rewrite_reserved_identifiers: bool,
    pub normalize_self_constructor_syntax: bool,
    pub rewrite_inline_initializers: bool,
    pub normalize_namespace_paths: bool,
    pub reconstruct_parser_safe_blocks: bool,
}

impl Default for RepairProfile {
    fn default() -> Self {
        Self {
            trim_trailing_whitespace: true,
            ensure_final_newline: true,
            collapse_extra_blank_lines: true,
            normalize_line_endings: true,
            fix_unterminated_block_comments: true,
            normalize_indentation: true,
            normalize_declaration_headers: true,
            flatten_nested_declarations: true,
            rewrite_reserved_identifiers: true,
            normalize_self_constructor_syntax: true,
            rewrite_inline_initializers: true,
            normalize_namespace_paths: true,
            reconstruct_parser_safe_blocks: true,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RepairMode {
    Check,
    Suggest,
    ApplySafe,
    ApplyAggressive,
}
impl RepairMode {
    pub fn writes(self) -> bool {
        matches!(self, Self::ApplySafe | Self::ApplyAggressive)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FixKind {
    NormalizeLineEndings,
    TrimTrailingWhitespace,
    EnsureFinalNewline,
    CollapseExtraBlankLines,
    CloseUnterminatedBlockComment,
    InsertMissingBlockCommentCloser,
    NormalizeIndentation,
    NormalizeDeclarationHeader,
    FlattenNestedDeclarationPlacement,
    NormalizeImplTypeToken,
    RewriteReservedIdentifier,
    NormalizeSelfConstructorSyntax,
    RewriteInlineInitialization,
    NormalizeNamespacePath,
    ReconstructParserSafeBlock,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AppliedFix {
    pub kind: FixKind,
    pub start: usize,
    pub end: usize,
    pub replacement: String,
    pub note: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RepairResult {
    pub original: String,
    pub repaired: String,
    pub changed: bool,
    pub fixes: Vec<AppliedFix>,
}

impl RepairResult {
    pub fn unchanged(original: impl Into<String>) -> Self {
        let original = original.into();
        Self {
            repaired: original.clone(),
            original,
            changed: false,
            fixes: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseValidation {
    pub passed: bool,
    pub detail: Option<String>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RepairSafetyClass {
    Safe,
    Aggressive,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RepairRiskLevel {
    Low,
    Elevated,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ParserProofStatus {
    Passed,
    Failed,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RepairReport {
    pub original: String,
    pub repaired: String,
    pub changed: bool,
    pub fixes: Vec<AppliedFix>,
    pub fixes_applied: usize,
    pub safety_class: RepairSafetyClass,
    pub remaining_unknown_risk: RepairRiskLevel,
    pub post_repair_parse: Option<ParseValidation>,
}

impl From<RepairResult> for RepairReport {
    fn from(value: RepairResult) -> Self {
        let fixes_applied = value.fixes.len();
        let has_aggressive_fix = value.fixes.iter().any(|fix| {
            matches!(
                fix.kind,
                FixKind::RewriteReservedIdentifier
                    | FixKind::NormalizeSelfConstructorSyntax
                    | FixKind::RewriteInlineInitialization
                    | FixKind::NormalizeNamespacePath
                    | FixKind::ReconstructParserSafeBlock
            )
        });
        let safety_class = if has_aggressive_fix {
            RepairSafetyClass::Aggressive
        } else {
            RepairSafetyClass::Safe
        };
        let remaining_unknown_risk = if fixes_applied == 0 {
            RepairRiskLevel::Unknown
        } else if has_aggressive_fix {
            RepairRiskLevel::Elevated
        } else {
            RepairRiskLevel::Low
        };
        Self {
            original: value.original,
            repaired: value.repaired,
            changed: value.changed,
            fixes: value.fixes,
            fixes_applied,
            safety_class,
            remaining_unknown_risk,
            post_repair_parse: None,
        }
    }
}

impl RepairReport {
    pub fn changed(&self) -> bool {
        self.changed
    }

    pub fn parser_proof_status(&self) -> Option<ParserProofStatus> {
        self.post_repair_parse.as_ref().map(|validation| {
            if validation.passed {
                ParserProofStatus::Passed
            } else {
                ParserProofStatus::Failed
            }
        })
    }

    pub fn with_post_repair_parse(mut self, validation: ParseValidation) -> Self {
        self.post_repair_parse = Some(validation);
        self
    }
}

pub fn repair_text(text: impl AsRef<str>) -> RepairResult {
    repair_text_with_input(&RepairInput::new(text.as_ref().to_owned()))
}

pub fn repair_text_with_input(input: &RepairInput<'_>) -> RepairResult {
    let mut text = input.text.as_ref().to_string();
    let mut fixes = Vec::new();
    for rule in registry::selected_rules(&input.profile, input.mode) {
        if let Some((next, fix)) = (rule.apply)(&text, input.mode) {
            text = next;
            fixes.push(fix);
        }
    }
    if input.profile.ensure_final_newline {
        apply_ensure_final_newline(&mut text, &mut fixes);
    }
    RepairResult {
        changed: text != input.text.as_ref(),
        original: input.text.as_ref().to_string(),
        repaired: text,
        fixes,
    }
}

pub fn repair_source(text: &str, mode: RepairMode) -> RepairReport {
    repair_source_with_profile(text, RepairProfile::default(), mode)
}

pub fn repair_source_with_profile(
    text: &str,
    profile: RepairProfile,
    mode: RepairMode,
) -> RepairReport {
    repair_text_with_input(&RepairInput {
        text: Cow::Borrowed(text),
        profile,
        mode,
    })
    .into()
}

pub fn suggest_fixes(text: impl AsRef<str>) -> Vec<AppliedFix> {
    repair_text_with_input(&RepairInput {
        text: Cow::Borrowed(text.as_ref()),
        profile: RepairProfile::default(),
        mode: RepairMode::Suggest,
    })
    .fixes
}

fn apply_ensure_final_newline(text: &mut String, fixes: &mut Vec<AppliedFix>) {
    if !text.ends_with('\n') {
        let pos = text.len();
        text.push('\n');
        fixes.push(AppliedFix {
            kind: FixKind::EnsureFinalNewline,
            start: pos,
            end: pos,
            replacement: "\n".into(),
            note: Some("added missing final newline".into()),
        });
    }
}
