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
    pub normalize_indentation: bool,
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FixKind {
    NormalizeLineEndings,
    TrimTrailingWhitespace,
    EnsureFinalNewline,
    CollapseExtraBlankLines,
    CloseUnterminatedBlockComment,
    InsertMissingBlockCommentCloser,
    NormalizeIndentation,
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
    if input.profile.fix_unterminated_block_comments {
        apply_close_unterminated_block_comment(&mut text, &mut fixes, input.mode);
    }
    if input.profile.normalize_indentation {
        let before = text.clone();
        text = normalize_indentation(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::NormalizeIndentation,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("normalized tabs and ragged leading indentation".into()),
            });
        }
    }
    if input.profile.rewrite_reserved_identifiers {
        let before = text.clone();
        text = rewrite_reserved_identifiers(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::RewriteReservedIdentifier,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("rewrote parser-hostile reserved identifiers with a deterministic suffix".into()),
            });
        }
    }
    if input.profile.normalize_self_constructor_syntax {
        let before = text.clone();
        text = normalize_self_constructor_syntax(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::NormalizeSelfConstructorSyntax,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("normalized invalid Self constructor syntax to Self::".into()),
            });
        }
    }
    if input.profile.rewrite_inline_initializers {
        let before = text.clone();
        text = rewrite_inline_initializers(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::RewriteInlineInitialization,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("rewrote low-risk inline constructor initializers into parser-safe form".into()),
            });
        }
    }
    if input.profile.normalize_namespace_paths {
        let before = text.clone();
        text = normalize_namespace_paths(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::NormalizeNamespacePath,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("normalized namespace/path separators that confuse the parser".into()),
            });
        }
    }
    if input.profile.reconstruct_parser_safe_blocks {
        let before = text.clone();
        text = reconstruct_parser_safe_blocks(&text);
        if text != before {
            fixes.push(AppliedFix {
                kind: FixKind::ReconstructParserSafeBlock,
                start: 0,
                end: before.len(),
                replacement: text.clone(),
                note: Some("reconstructed block indentation from parser-safe structure".into()),
            });
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
        let start = text.len();
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
            start,
            end: text.len(),
            replacement: "*/\n".into(),
            note: Some("appended block comment closer for unterminated comment".into()),
        });
    }
}

fn normalize_indentation(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    for line in source.lines() {
        let trimmed = line.trim_start_matches([' ', '\t']);
        let indent_len = line.len() - trimmed.len();
        let indent = line[..indent_len].replace('\t', "    ");
        let normalized = " ".repeat((indent.len() / 4) * 4);
        out.push_str(&normalized);
        out.push_str(trimmed);
        out.push('\n');
    }
    out.trim_end_matches('\n').to_string()
}

fn rewrite_reserved_identifiers(source: &str) -> String {
    const RESERVED: &[&str] = &["type", "mod", "self", "Self", "use", "impl", "struct", "enum", "trait", "const", "var"];
    let mut out = source.to_string();
    for reserved in RESERVED {
        let replacement = format!("{reserved}_");
        let mut cursor = 0usize;
        while let Some(pos) = find_word(&out, reserved, cursor) {
            out.replace_range(pos..pos + reserved.len(), &replacement);
            cursor = pos + replacement.len();
        }
    }
    out
}

fn normalize_self_constructor_syntax(source: &str) -> String {
    source.replace("Self:", "Self::").replace("Self :", "Self::")
}

fn rewrite_inline_initializers(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    for line in source.lines() {
        let trimmed = line.trim();
        if let Some((lhs, rhs)) = trimmed.split_once(" = ") {
            if rhs.contains('(') && rhs.contains(')') && !rhs.trim_end().ends_with(':') {
                out.push_str(lhs);
                out.push_str(" = ");
                out.push_str(rhs);
                out.push('\n');
                continue;
            }
        }
        out.push_str(line);
        out.push('\n');
    }
    out.trim_end_matches('\n').to_string()
}

fn normalize_namespace_paths(source: &str) -> String {
    source.replace("/", "::").replace("\\", "::")
}

fn reconstruct_parser_safe_blocks(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    let mut depth = 0usize;
    for line in source.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with('}') || trimmed.starts_with("end") {
            depth = depth.saturating_sub(1);
        }
        let expected = "    ".repeat(depth);
        if !trimmed.is_empty() {
            out.push_str(&expected);
            out.push_str(trimmed);
        }
        out.push('\n');
        if trimmed.ends_with(':') || trimmed.ends_with('{') {
            depth += 1;
        }
    }
    out.trim_end_matches('\n').to_string()
}

fn find_word(text: &str, needle: &str, from: usize) -> Option<usize> {
    let mut search = from;
    while let Some(pos) = text[search..].find(needle) {
        let abs = search + pos;
        let before = text[..abs].chars().next_back();
        let after = text[abs + needle.len()..].chars().next();
        let before_ok = before.map(|c| !c.is_ascii_alphanumeric() && c != '_').unwrap_or(true);
        let after_ok = after.map(|c| !c.is_ascii_alphanumeric() && c != '_').unwrap_or(true);
        if before_ok && after_ok {
            return Some(abs);
        }
        search = abs + needle.len();
    }
    None
}

fn collapse_parser_hostile_header_noise(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    let mut in_header = true;
    let mut seen_code = false;

    for line in source.lines() {
        let trimmed = line.trim();
        if !seen_code {
            if trimmed.is_empty() {
                if in_header {
                    continue;
                }
                out.push('\n');
                continue;
            }
            if trimmed.starts_with("//")
                || trimmed.starts_with('#')
                || trimmed.starts_with('/')
                || trimmed.starts_with('=')
                || trimmed.starts_with('-')
                || trimmed.starts_with('*')
            {
                continue;
            }
            seen_code = true;
            in_header = false;
        }
        out.push_str(line);
        out.push('\n');
    }

    out.trim_end_matches('\n').to_string()
}
