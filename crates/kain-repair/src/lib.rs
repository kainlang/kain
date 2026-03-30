use std::borrow::Cow;

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
    pub strip_utf8_bom: bool,
    pub collapse_parser_hostile_header_noise: bool,
}

impl Default for RepairProfile {
    fn default() -> Self {
        Self {
            trim_trailing_whitespace: true,
            ensure_final_newline: true,
            collapse_extra_blank_lines: true,
            normalize_line_endings: true,
            fix_unterminated_block_comments: true,
            strip_utf8_bom: false,
            collapse_parser_hostile_header_noise: false,
        }
    }
}

impl RepairProfile {
    pub fn migration_imported_rust() -> Self {
        Self {
            strip_utf8_bom: true,
            collapse_parser_hostile_header_noise: true,
            ..Self::default()
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FixKind {
    StripUtf8Bom,
    NormalizeLineEndings,
    TrimTrailingWhitespace,
    EnsureFinalNewline,
    CollapseExtraBlankLines,
    CollapseParserHostileHeaderNoise,
    CloseUnterminatedBlockComment,
    InsertMissingBlockCommentCloser,
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

pub fn repair_text(text: impl AsRef<str>) -> RepairResult {
    let input = RepairInput::new(text.as_ref().to_owned());
    repair_text_with_input(&input)
}

pub fn repair_text_with_input(input: &RepairInput<'_>) -> RepairResult {
    let mut text = input.text.as_ref().to_string();
    let mut fixes = Vec::new();

    if input.profile.normalize_line_endings {
        apply_normalize_line_endings(&mut text, &mut fixes);
    }
    if input.profile.trim_trailing_whitespace {
        apply_trim_trailing_whitespace(&mut text, &mut fixes);
    }
    if input.profile.collapse_extra_blank_lines {
        apply_collapse_extra_blank_lines(&mut text, &mut fixes);
    }
    if input.profile.ensure_final_newline {
        apply_ensure_final_newline(&mut text, &mut fixes);
    }
    if input.profile.fix_unterminated_block_comments {
        apply_close_unterminated_block_comment(&mut text, &mut fixes, input.mode);
    }

    RepairResult {
        changed: text != input.text.as_ref(),
        original: input.text.as_ref().to_string(),
        repaired: text,
        fixes,
    }
}

pub fn suggest_fixes(text: impl AsRef<str>) -> Vec<AppliedFix> {
    repair_text_with_input(&RepairInput {
        text: Cow::Borrowed(text.as_ref()),
        profile: RepairProfile::default(),
        mode: RepairMode::Suggest,
    })
    .fixes
}

fn apply_normalize_line_endings(text: &mut String, fixes: &mut Vec<AppliedFix>) {
    if text.contains('\r') {
        let replacement = text.replace("\r\n", "\n").replace('\r', "\n");
        if replacement != *text {
            fixes.push(AppliedFix {
                kind: FixKind::NormalizeLineEndings,
                start: 0,
                end: text.len(),
                replacement: replacement.clone(),
                note: Some("normalized CRLF/CR line endings to LF".into()),
            });
            *text = replacement;
        }
    }
}

fn apply_trim_trailing_whitespace(text: &mut String, fixes: &mut Vec<AppliedFix>) {
    let mut changed = false;
    let mut out = String::with_capacity(text.len());
    for line in text.split_inclusive('\n') {
        let (body, newline) = line.strip_suffix('\n').map_or((line, ""), |s| (s, "\n"));
        let trimmed = body.trim_end_matches([' ', '\t']);
        if trimmed.len() != body.len() {
            changed = true;
        }
        out.push_str(trimmed);
        out.push_str(newline);
    }
    if !text.ends_with('\n') {
        let trimmed = out.trim_end_matches([' ', '\t']).to_string();
        changed |= trimmed != out;
        out = trimmed;
    }
    if changed {
        fixes.push(AppliedFix {
            kind: FixKind::TrimTrailingWhitespace,
            start: 0,
            end: text.len(),
            replacement: out.clone(),
            note: Some("trimmed trailing spaces and tabs".into()),
        });
        *text = out;
    }
}

fn apply_collapse_extra_blank_lines(text: &mut String, fixes: &mut Vec<AppliedFix>) {
    let mut out = String::with_capacity(text.len());
    let mut blank_run = 0usize;
    let mut changed = false;
    for line in text.split_inclusive('\n') {
        let is_blank = line.trim() == "";
        if is_blank {
            blank_run += 1;
        } else {
            blank_run = 0;
        }
        if blank_run > 2 {
            changed = true;
            continue;
        }
        out.push_str(line);
    }
    if changed {
        fixes.push(AppliedFix {
            kind: FixKind::CollapseExtraBlankLines,
            start: 0,
            end: text.len(),
            replacement: out.clone(),
            note: Some("collapsed excessive blank line runs".into()),
        });
        *text = out;
    }
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

fn apply_close_unterminated_block_comment(
    text: &mut String,
    fixes: &mut Vec<AppliedFix>,
    mode: RepairMode,
) {
    let open_count = text.match_indices("/*").count();
    let close_count = text.match_indices("*/").count();
    if open_count > close_count {
        if matches!(mode, RepairMode::Check) {
            return;
        }
        if !text.ends_with('\n') {
            text.push('\n');
        }
        text.push_str("*/\n");
        fixes.push(AppliedFix {
            kind: if matches!(mode, RepairMode::ApplyAggressive) {
                FixKind::InsertMissingBlockCommentCloser
            } else {
                FixKind::CloseUnterminatedBlockComment
            },
            start: text.len().saturating_sub(3),
            end: text.len(),
            replacement: "*/\n".into(),
            note: Some("appended block comment closer for unterminated comment".into()),
        });
    }
}

impl<'a> From<&'a str> for RepairInput<'a> {
    fn from(value: &'a str) -> Self {
        Self::new(value)
    }
}

impl From<String> for RepairInput<'static> {
    fn from(value: String) -> Self {
        Self::new(value)
    }
}
