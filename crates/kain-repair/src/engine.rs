use crate::{AppliedFix, FixKind, RepairMode};

pub fn apply_normalize_line_endings(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    if !text.contains('\r') {
        return None;
    }
    let replacement = text.replace("\r\n", "\n").replace('\r', "\n");
    (replacement != text).then(|| {
        (
            replacement.clone(),
            AppliedFix {
                kind: FixKind::NormalizeLineEndings,
                start: 0,
                end: text.len(),
                replacement,
                note: Some("normalized CRLF/CR line endings to LF".into()),
            },
        )
    })
}

pub fn apply_trim_trailing_whitespace(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let mut changed = false;
    let mut out = String::with_capacity(text.len());
    for line in text.split_inclusive('\n') {
        let (body, newline) = line.strip_suffix('\n').map_or((line, ""), |s| (s, "\n"));
        let trimmed = body.trim_end_matches([' ', '\t']);
        changed |= trimmed.len() != body.len();
        out.push_str(trimmed);
        out.push_str(newline);
    }
    if !text.ends_with('\n') {
        let trimmed = out.trim_end_matches([' ', '\t']).to_string();
        changed |= trimmed != out;
        out = trimmed;
    }
    changed.then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::TrimTrailingWhitespace,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("trimmed trailing spaces and tabs".into()),
            },
        )
    })
}

pub fn apply_collapse_extra_blank_lines(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let mut out = String::with_capacity(text.len());
    let mut blank_run = 0usize;
    let mut changed = false;
    for line in text.split_inclusive('\n') {
        let is_blank = line.trim() == "";
        if is_blank { blank_run += 1; } else { blank_run = 0; }
        if blank_run > 2 { changed = true; continue; }
        out.push_str(line);
    }
    changed.then(|| {
        (
            out.clone(),
            AppliedFix { kind: FixKind::CollapseExtraBlankLines, start: 0, end: text.len(), replacement: out, note: Some("collapsed excessive blank line runs".into()) },
        )
    })
}

pub fn apply_close_unterminated_block_comment(text: &str, mode: RepairMode) -> Option<(String, AppliedFix)> {
    let open_count = text.match_indices("/*").count();
    let close_count = text.match_indices("*/").count();
    if open_count <= close_count || matches!(mode, RepairMode::Check) { return None; }
    let mut out = text.to_string();
    let start = out.len();
    if !out.ends_with('\n') { out.push('\n'); }
    out.push_str("*/\n");
    Some((
        out.clone(),
        AppliedFix {
            kind: if matches!(mode, RepairMode::ApplyAggressive) { FixKind::InsertMissingBlockCommentCloser } else { FixKind::CloseUnterminatedBlockComment },
            start,
            end: out.len(),
            replacement: "*/\n".into(),
            note: Some("appended block comment closer for unterminated comment".into()),
        },
    ))
}

pub fn apply_normalize_indentation(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = normalize_indentation(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::NormalizeIndentation, start: 0, end: text.len(), replacement: out, note: Some("normalized tabs and ragged leading indentation".into()) }))
}

pub fn apply_rewrite_reserved_identifiers(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = rewrite_reserved_identifiers(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::RewriteReservedIdentifier, start: 0, end: text.len(), replacement: out, note: Some("rewrote parser-hostile reserved identifiers with a deterministic suffix".into()) }))
}

pub fn apply_normalize_self_constructor_syntax(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = normalize_self_constructor_syntax(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::NormalizeSelfConstructorSyntax, start: 0, end: text.len(), replacement: out, note: Some("normalized invalid Self constructor syntax to Self::".into()) }))
}

pub fn apply_rewrite_inline_initializers(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = rewrite_inline_initializers(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::RewriteInlineInitialization, start: 0, end: text.len(), replacement: out, note: Some("rewrote low-risk inline constructor initializers into parser-safe form".into()) }))
}

pub fn apply_normalize_namespace_paths(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = normalize_namespace_paths(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::NormalizeNamespacePath, start: 0, end: text.len(), replacement: out, note: Some("normalized namespace/path separators that confuse the parser".into()) }))
}

pub fn apply_reconstruct_parser_safe_blocks(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = reconstruct_parser_safe_blocks(text);
    (out != text).then(|| (out.clone(), AppliedFix { kind: FixKind::ReconstructParserSafeBlock, start: 0, end: text.len(), replacement: out, note: Some("reconstructed block indentation from parser-safe structure".into()) }))
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

fn normalize_self_constructor_syntax(source: &str) -> String { source.replace("Self:", "Self::").replace("Self :", "Self::") }
fn rewrite_inline_initializers(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    for line in source.lines() {
        let trimmed = line.trim();
        if let Some((lhs, rhs)) = trimmed.split_once(" = ") {
            if rhs.contains('(') && rhs.contains(')') && !rhs.trim_end().ends_with(':') {
                out.push_str(lhs); out.push_str(" = "); out.push_str(rhs); out.push('\n'); continue;
            }
        }
        out.push_str(line); out.push('\n');
    }
    out.trim_end_matches('\n').to_string()
}
fn normalize_namespace_paths(source: &str) -> String { source.replace("/", "::").replace("\\", "::") }
fn reconstruct_parser_safe_blocks(source: &str) -> String {
    let mut out = String::with_capacity(source.len()); let mut depth = 0usize;
    for line in source.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with('}') || trimmed.starts_with("end") { depth = depth.saturating_sub(1); }
        let expected = "    ".repeat(depth);
        if !trimmed.is_empty() { out.push_str(&expected); out.push_str(trimmed); }
        out.push('\n');
        if trimmed.ends_with(':') || trimmed.ends_with('{') { depth += 1; }
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
        if before_ok && after_ok { return Some(abs); }
        search = abs + needle.len();
    }
    None
}
