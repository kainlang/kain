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
    changed.then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::CollapseExtraBlankLines,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("collapsed excessive blank line runs".into()),
            },
        )
    })
}

pub fn apply_close_unterminated_block_comment(
    text: &str,
    mode: RepairMode,
) -> Option<(String, AppliedFix)> {
    let open_count = text.match_indices("/*").count();
    let close_count = text.match_indices("*/").count();
    if open_count <= close_count || matches!(mode, RepairMode::Check) {
        return None;
    }
    let mut out = text.to_string();
    let start = out.len();
    if !out.ends_with('\n') {
        out.push('\n');
    }
    out.push_str("*/\n");
    Some((
        out.clone(),
        AppliedFix {
            kind: if matches!(mode, RepairMode::ApplyAggressive) {
                FixKind::InsertMissingBlockCommentCloser
            } else {
                FixKind::CloseUnterminatedBlockComment
            },
            start,
            end: out.len(),
            replacement: "*/\n".into(),
            note: Some("appended block comment closer for unterminated comment".into()),
        },
    ))
}

pub fn apply_normalize_indentation(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = normalize_indentation(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::NormalizeIndentation,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("normalized tabs and ragged leading indentation".into()),
            },
        )
    })
}

pub fn apply_normalize_declaration_headers(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = normalize_declaration_headers(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::NormalizeDeclarationHeader,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some(
                    "normalized parser-hostile declaration headers into canonical Kain syntax"
                        .into(),
                ),
            },
        )
    })
}

pub fn apply_flatten_nested_declaration_placement(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = flatten_nested_declaration_placement(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::FlattenNestedDeclarationPlacement,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("flattened nested declaration blocks to top-level placement".into()),
            },
        )
    })
}

pub fn apply_rewrite_reserved_identifiers(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = rewrite_reserved_identifiers(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::RewriteReservedIdentifier,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some(
                    "rewrote parser-hostile reserved identifiers with a deterministic suffix"
                        .into(),
                ),
            },
        )
    })
}

pub fn apply_normalize_self_constructor_syntax(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = normalize_self_constructor_syntax(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::NormalizeSelfConstructorSyntax,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("normalized invalid Self constructor syntax to Self::".into()),
            },
        )
    })
}

pub fn apply_rewrite_inline_initializers(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = rewrite_inline_initializers(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::RewriteInlineInitialization,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some(
                    "rewrote low-risk inline constructor initializers into parser-safe form".into(),
                ),
            },
        )
    })
}

pub fn apply_normalize_namespace_paths(text: &str, _: RepairMode) -> Option<(String, AppliedFix)> {
    let out = normalize_namespace_paths(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::NormalizeNamespacePath,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("normalized namespace/path separators that confuse the parser".into()),
            },
        )
    })
}

pub fn apply_reconstruct_parser_safe_blocks(
    text: &str,
    _: RepairMode,
) -> Option<(String, AppliedFix)> {
    let out = reconstruct_parser_safe_blocks(text);
    (out != text).then(|| {
        (
            out.clone(),
            AppliedFix {
                kind: FixKind::ReconstructParserSafeBlock,
                start: 0,
                end: text.len(),
                replacement: out,
                note: Some("reconstructed block indentation from parser-safe structure".into()),
            },
        )
    })
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
    const RESERVED: &[&str] = &[
        "type", "mod", "self", "Self", "use", "impl", "struct", "enum", "trait", "const", "var",
    ];
    let mut out_lines = Vec::new();
    for line in source.lines() {
        let (_, trimmed) = split_leading_whitespace(line);
        let header_keyword = trimmed
            .split_whitespace()
            .next()
            .map(|kw| kw.trim_end_matches(':'))
            .filter(|kw| matches!(*kw, "enum" | "struct" | "trait" | "impl"));

        let mut current = line.to_string();
        for reserved in RESERVED {
            if header_keyword == Some(*reserved) {
                continue;
            }
            let replacement = format!("{reserved}_");
            let mut cursor = 0usize;
            while let Some(pos) = find_word(&current, reserved, cursor) {
                current.replace_range(pos..pos + reserved.len(), &replacement);
                cursor = pos + replacement.len();
            }
        }
        out_lines.push(current);
    }
    out_lines.join("\n")
}

fn normalize_declaration_headers(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    let mut changed = false;
    for line in source.lines() {
        let (prefix, body) = split_leading_whitespace(line);
        let (next, line_changed) = normalize_declaration_header_line(body);
        changed |= line_changed;
        out.push_str(prefix);
        out.push_str(&next);
        out.push('\n');
    }
    if source.ends_with('\n') {
        out
    } else {
        changed
            .then_some(out.trim_end_matches('\n').to_string())
            .unwrap_or_else(|| source.to_string())
    }
}

fn normalize_declaration_header_line(line: &str) -> (String, bool) {
    let trimmed = line.trim_start();
    let indent = &line[..line.len() - trimmed.len()];
    let Some(split_at) = trimmed.find(char::is_whitespace) else {
        return (line.to_string(), false);
    };
    let (keyword, rest) = trimmed.split_at(split_at);
    let rest = rest.trim_start();
    let is_declaration = matches!(keyword, "enum_" | "struct_" | "trait_" | "impl_");
    if !is_declaration {
        return (line.to_string(), false);
    }
    let canonical = keyword.trim_end_matches('_');
    let rest = rest.trim_end_matches(':');
    let mut normalized = String::new();
    normalized.push_str(indent);
    normalized.push_str(canonical);
    normalized.push(' ');
    normalized.push_str(rest);
    if trimmed.ends_with(':') {
        normalized.push(':');
    }
    (normalized, true)
}

fn flatten_nested_declaration_placement(source: &str) -> String {
    const DECLARATION_KEYWORDS: [&str; 4] = ["enum", "struct", "trait", "impl"];
    let lines: Vec<&str> = source.lines().collect();
    let mut out = String::with_capacity(source.len());
    let mut index = 0usize;

    while index < lines.len() {
        let line = lines[index];
        let (indent, trimmed) = split_leading_whitespace(line);
        let current_indent_width = indent_width(indent);
        let starts_nested_declaration = current_indent_width > 0
            && starts_with_declaration_header(trimmed, &DECLARATION_KEYWORDS);

        if !starts_nested_declaration {
            out.push_str(line);
            out.push('\n');
            index += 1;
            continue;
        }

        let block_end =
            find_declaration_block_end(&lines, index, current_indent_width, &DECLARATION_KEYWORDS);
        let base_indent = current_indent_width;
        for block_index in index..block_end {
            let block_line = lines[block_index];
            if block_line.trim().is_empty() {
                out.push('\n');
                continue;
            }
            let (block_indent, _) = split_leading_whitespace(block_line);
            let dedented = if indent_width(block_indent) >= base_indent {
                &block_line[base_indent.min(block_line.len())..]
            } else {
                block_line
            };
            out.push_str(dedented);
            out.push('\n');
        }
        index = block_end;
    }

    out.trim_end_matches('\n').to_string()
}

fn find_declaration_block_end(
    lines: &[&str],
    start: usize,
    base_indent: usize,
    keywords: &[&str],
) -> usize {
    let mut index = start + 1;
    while index < lines.len() {
        let line = lines[index];
        let (indent, trimmed) = split_leading_whitespace(line);
        if trimmed.is_empty() {
            index += 1;
            continue;
        }
        let current_indent = indent_width(indent);
        if current_indent <= base_indent && starts_with_declaration_header(trimmed, keywords) {
            break;
        }
        if current_indent < base_indent {
            break;
        }
        index += 1;
    }
    index
}

fn starts_with_declaration_header(text: &str, keywords: &[&str]) -> bool {
    let keyword = text
        .split_whitespace()
        .next()
        .map(|token| token.trim_end_matches(':'));
    keyword.map(|kw| keywords.contains(&kw)).unwrap_or(false)
}

fn indent_width(indent: &str) -> usize {
    indent
        .chars()
        .map(|ch| if ch == '\t' { 4 } else { 1 })
        .sum()
}

fn indent_width_of(indent: &str) -> usize {
    indent_width(indent)
}

fn split_leading_whitespace(line: &str) -> (&str, &str) {
    let trimmed = line.trim_start_matches([' ', '\t']);
    let indent_len = line.len() - trimmed.len();
    (&line[..indent_len], trimmed)
}

fn normalize_self_constructor_syntax(source: &str) -> String {
    let mut out = String::with_capacity(source.len());
    for line in source.lines() {
        let (next, _) = normalize_self_constructor_line(line);
        out.push_str(&next);
        out.push('\n');
    }
    out.trim_end_matches('\n').to_string()
}

fn normalize_self_constructor_line(line: &str) -> (String, bool) {
    let mut current = line.to_string();
    let mut changed = false;

    changed |= replace_word_form(&mut current, "Self_:", "Self::");
    changed |= replace_word_form(&mut current, "Self_ :", "Self::");
    changed |= replace_word_form(&mut current, "Self_::", "Self::");
    changed |= replace_word_form(&mut current, "-> Self_", "-> Self");
    changed |= replace_word_form(&mut current, ": Self_", ": Self");
    changed |= replace_word_form(&mut current, "(Self_", "(Self");
    changed |= replace_word_form(&mut current, " Self_)", " Self)");
    changed |= replace_word_form(&mut current, ", Self_", ", Self");
    changed |= replace_word_form(&mut current, " Self_,", " Self,");

    (current, changed)
}

fn replace_word_form(text: &mut String, needle: &str, replacement: &str) -> bool {
    if !text.contains(needle) {
        return false;
    }
    let updated = text.replace(needle, replacement);
    if updated == *text {
        return false;
    }
    *text = updated;
    true
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
        let before_ok = before
            .map(|c| !c.is_ascii_alphanumeric() && c != '_')
            .unwrap_or(true);
        let after_ok = after
            .map(|c| !c.is_ascii_alphanumeric() && c != '_')
            .unwrap_or(true);
        if before_ok && after_ok {
            return Some(abs);
        }
        search = abs + needle.len();
    }
    None
}
