//! C parser using lang-c

use super::CImportOptions;
use crate::{ImportError, Result};
use kain_fs as kfs;
use lang_c::ast::TranslationUnit;
use lang_c::driver::{parse, parse_preprocessed, Config};
use lang_c::span::Span as CSpan;
use std::collections::HashSet;
use std::path::Path;
use std::process::Command;

#[derive(Debug, Clone)]
pub(crate) struct ParsedCTranslationUnit {
    pub unit: TranslationUnit,
    pub layout: CSourceLayoutMetadata,
}

#[derive(Debug, Clone, Default)]
pub(crate) struct CSourceLayoutMetadata {
    source: String,
    line_starts: Vec<usize>,
    pack_align_by_line: Vec<Option<usize>>,
}

impl CSourceLayoutMetadata {
    pub fn from_source(source: &str) -> Self {
        let source = source.to_string();
        let line_starts = compute_line_starts(&source);
        let pack_align_by_line = collect_pack_align_by_line(&strip_c_comments(source.as_str()));
        Self {
            source,
            line_starts,
            pack_align_by_line,
        }
    }

    pub fn pack_align_bits_for_span(&self, span: CSpan) -> Option<usize> {
        let line = self.line_for_offset(span.start)?;
        self.pack_align_by_line
            .get(line.saturating_sub(1))
            .and_then(|align| *align)
            .map(|bytes| bytes.saturating_mul(8))
    }

    pub fn has_packed_attr_for_span(&self, span: CSpan) -> bool {
        normalized_span_snippet(self, span).is_some_and(|snippet| {
            (snippet.contains("__attribute__((") && snippet.contains("packed"))
                || snippet.contains("__packed")
        })
    }

    pub fn explicit_type_align_bits_for_span(&self, span: CSpan) -> Option<usize> {
        let snippet = normalized_span_snippet(self, span)?;
        parse_attr_align_bytes(snippet.as_str()).map(|bytes| bytes.saturating_mul(8))
    }

    fn line_for_offset(&self, offset: usize) -> Option<usize> {
        if self.line_starts.is_empty() {
            return None;
        }

        let idx = match self.line_starts.binary_search(&offset) {
            Ok(idx) => idx,
            Err(0) => 0,
            Err(idx) => idx.saturating_sub(1),
        };
        Some(idx + 1)
    }

    fn snippet_for_span(&self, span: CSpan) -> Option<&str> {
        if span.start >= self.source.len() {
            return None;
        }
        let end = span.end.min(self.source.len());
        self.source.get(span.start..end)
    }
}

/// Parse a C file using lang-c
#[cfg(test)]
pub(crate) fn parse_c_file(path: &Path) -> Result<TranslationUnit> {
    parse_c_file_with_metadata(path, &CImportOptions::default()).map(|parsed| parsed.unit)
}

pub(crate) fn parse_c_file_with_metadata(
    path: &Path,
    options: &CImportOptions,
) -> Result<ParsedCTranslationUnit> {
    let source = kfs::read_text(path)?;
    let layout = CSourceLayoutMetadata::from_source(&source);
    let config = build_driver_config(options);
    let defined_symbols = collect_defined_symbols(options);
    let direct_source = sanitize_for_preprocessed_parse(&source, false, &defined_symbols);

    match parse_preprocessed(&config, direct_source) {
        Ok(parse_result) => Ok(ParsedCTranslationUnit {
            unit: parse_result.unit,
            layout,
        }),
        Err(direct_err) => match parse(&config, path) {
            Ok(parse_result) => Ok(ParsedCTranslationUnit {
                unit: parse_result.unit,
                layout,
            }),
            Err(primary_err) => {
                let stripped = sanitize_for_preprocessed_parse(&source, true, &defined_symbols);

                match parse_preprocessed(&config, stripped) {
                Ok(parse_result) => Ok(ParsedCTranslationUnit {
                    unit: parse_result.unit,
                    layout,
                }),
                Err(fallback_err) => Err(ImportError::CParseError(format!(
                    "direct source parse failed: {:?}; primary parse failed: {}; fallback parse failed: {:?}",
                    direct_err, primary_err, fallback_err
                ))),
            }
            }
        },
    }
}

/// Parse C source code from a string
#[cfg(test)]
pub(crate) fn parse_c_source(source: &str) -> Result<TranslationUnit> {
    parse_c_source_with_metadata(source).map(|parsed| parsed.unit)
}

#[cfg(test)]
pub(crate) fn parse_c_source_with_metadata(source: &str) -> Result<ParsedCTranslationUnit> {
    let config = Config::default();
    let defined_symbols = HashSet::new();

    let parse_result = parse_preprocessed(
        &config,
        sanitize_for_preprocessed_parse(source, false, &defined_symbols),
    )
    .map_err(|e| ImportError::CParseError(format!("{:?}", e)))?;

    Ok(ParsedCTranslationUnit {
        unit: parse_result.unit,
        layout: CSourceLayoutMetadata::from_source(source),
    })
}

fn compute_line_starts(source: &str) -> Vec<usize> {
    let mut starts = vec![0];
    for (idx, ch) in source.char_indices() {
        if ch == '\n' && idx + 1 < source.len() {
            starts.push(idx + 1);
        }
    }
    starts
}

fn normalized_span_snippet(metadata: &CSourceLayoutMetadata, span: CSpan) -> Option<String> {
    let snippet = metadata.snippet_for_span(span)?;
    Some(
        snippet
            .chars()
            .filter(|ch| !ch.is_whitespace())
            .collect::<String>()
            .to_ascii_lowercase(),
    )
}

fn parse_attr_align_bytes(normalized_snippet: &str) -> Option<usize> {
    for marker in ["aligned(", "__declspec(align(", "alignas("] {
        if let Some(idx) = normalized_snippet.find(marker) {
            let start = idx + marker.len();
            let digits = normalized_snippet[start..]
                .chars()
                .take_while(|ch| ch.is_ascii_digit())
                .collect::<String>();
            if let Ok(value) = digits.parse::<usize>() {
                if value > 0 {
                    return Some(value);
                }
            }
        }
    }
    None
}

fn collect_pack_align_by_line(source: &str) -> Vec<Option<usize>> {
    let mut current = None;
    let mut stack = Vec::<PackFrame>::new();
    let mut by_line = Vec::new();

    for line in source.lines() {
        let trimmed = line.trim_start();
        if let Some(directive) = parse_pack_directive(trimmed) {
            match directive {
                PackDirective::Push { id, next } => {
                    stack.push(PackFrame {
                        id,
                        previous: current,
                    });
                    current = next.or(current);
                }
                PackDirective::Pop { id } => {
                    current = pop_pack_frame(&mut stack, id).unwrap_or(None);
                }
                PackDirective::Set(next) => {
                    current = next;
                }
            }
            by_line.push(current);
            continue;
        }

        by_line.push(current);
    }

    by_line
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum PackDirective {
    Push {
        id: Option<String>,
        next: Option<usize>,
    },
    Pop {
        id: Option<String>,
    },
    Set(Option<usize>),
}

#[derive(Debug, Clone)]
struct PackFrame {
    id: Option<String>,
    previous: Option<usize>,
}

fn pop_pack_frame(stack: &mut Vec<PackFrame>, id: Option<String>) -> Option<Option<usize>> {
    match id {
        None => stack.pop().map(|frame| frame.previous),
        Some(target) => {
            let idx = stack
                .iter()
                .rposition(|frame| frame.id.as_deref() == Some(target.as_str()))?;
            let frame = stack.remove(idx);
            stack.truncate(idx);
            Some(frame.previous)
        }
    }
}

fn parse_pack_directive(line: &str) -> Option<PackDirective> {
    let normalized = line.replace(' ', "").to_ascii_lowercase();
    if !normalized.starts_with("#pragmapack") {
        return None;
    }
    let start = normalized.find('(')?;
    let end = normalized.rfind(')')?;
    let inner = &normalized[start + 1..end];
    if inner.is_empty() {
        return Some(PackDirective::Set(None));
    }

    let parts = inner
        .split(',')
        .map(str::trim)
        .filter(|part| !part.is_empty())
        .collect::<Vec<_>>();
    if parts.is_empty() {
        return Some(PackDirective::Set(None));
    }

    match parts[0] {
        "push" => {
            let id = parts
                .iter()
                .skip(1)
                .find(|part| part.parse::<usize>().is_err())
                .map(|part| (*part).to_string());
            let next = parts
                .iter()
                .skip(1)
                .find_map(|part| part.parse::<usize>().ok())
                .filter(|value| *value > 0);
            Some(PackDirective::Push { id, next })
        }
        "pop" => {
            let id = parts
                .iter()
                .skip(1)
                .find(|part| part.parse::<usize>().is_err())
                .map(|part| (*part).to_string());
            Some(PackDirective::Pop { id })
        }
        _ => parts[0]
            .parse::<usize>()
            .ok()
            .filter(|value| *value > 0)
            .map(|value| PackDirective::Set(Some(value))),
    }
}

fn build_driver_config(options: &CImportOptions) -> Config {
    let mut config = if let Some(cpp_command) = &options.cpp_command {
        if cpp_command.to_ascii_lowercase().contains("clang") {
            let mut cfg = Config::with_clang();
            cfg.cpp_command = cpp_command.clone();
            cfg
        } else {
            let mut cfg = Config::with_gcc();
            cfg.cpp_command = cpp_command.clone();
            cfg
        }
    } else if command_exists("clang") {
        Config::with_clang()
    } else if command_exists("gcc") {
        Config::with_gcc()
    } else {
        Config::default()
    };

    for include_path in &options.include_paths {
        config.cpp_options.push(format!("-I{}", include_path));
    }
    for define in &options.defines {
        if define.starts_with("-D") {
            config.cpp_options.push(define.clone());
        } else {
            config.cpp_options.push(format!("-D{}", define));
        }
    }
    config
        .cpp_options
        .extend(options.cpp_options.iter().cloned());

    config
}

fn command_exists(command: &str) -> bool {
    Command::new(command).arg("--version").output().is_ok()
}

fn collect_defined_symbols(options: &CImportOptions) -> HashSet<String> {
    let mut defined = HashSet::new();
    for entry in &options.defines {
        if let Some(sym) = parse_define_symbol(entry) {
            defined.insert(sym);
        }
    }
    for entry in &options.cpp_options {
        if let Some(sym) = entry.strip_prefix("-D").and_then(parse_define_symbol) {
            defined.insert(sym);
        }
    }
    defined
}

fn parse_define_symbol(raw: &str) -> Option<String> {
    let normalized = raw.trim().trim_start_matches("-D").trim();
    if normalized.is_empty() {
        return None;
    }
    let symbol = normalized.split('=').next().unwrap_or("").trim();
    if symbol.is_empty() {
        None
    } else {
        Some(symbol.to_string())
    }
}

#[derive(Debug, Clone, Copy)]
struct ConditionalFrame {
    parent_active: bool,
    currently_active: bool,
    branch_taken: bool,
}

fn conditional_active(stack: &[ConditionalFrame]) -> bool {
    stack.last().map(|f| f.currently_active).unwrap_or(true)
}

fn eval_preprocessor_expr(expr: &str, defined_symbols: &HashSet<String>) -> bool {
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    enum Tok {
        LParen,
        RParen,
        Not,
        AndAnd,
        OrOr,
        Num(i64),
    }

    fn parse_number(text: &str) -> i64 {
        if let Some(hex) = text.strip_prefix("0x").or_else(|| text.strip_prefix("0X")) {
            i64::from_str_radix(hex, 16).unwrap_or(0)
        } else {
            text.parse::<i64>().unwrap_or(0)
        }
    }

    let chars: Vec<char> = expr.chars().collect();
    let mut i = 0usize;
    let mut tokens = Vec::new();
    while i < chars.len() {
        let c = chars[i];
        if c.is_whitespace() {
            i += 1;
            continue;
        }
        if c == '(' {
            tokens.push(Tok::LParen);
            i += 1;
            continue;
        }
        if c == ')' {
            tokens.push(Tok::RParen);
            i += 1;
            continue;
        }
        if c == '!' {
            tokens.push(Tok::Not);
            i += 1;
            continue;
        }
        if c == '&' && i + 1 < chars.len() && chars[i + 1] == '&' {
            tokens.push(Tok::AndAnd);
            i += 2;
            continue;
        }
        if c == '|' && i + 1 < chars.len() && chars[i + 1] == '|' {
            tokens.push(Tok::OrOr);
            i += 2;
            continue;
        }

        if c.is_ascii_alphabetic() || c == '_' {
            let start = i;
            i += 1;
            while i < chars.len() && (chars[i].is_ascii_alphanumeric() || chars[i] == '_') {
                i += 1;
            }
            let ident: String = chars[start..i].iter().collect();
            if ident == "defined" {
                while i < chars.len() && chars[i].is_whitespace() {
                    i += 1;
                }
                let symbol = if i < chars.len() && chars[i] == '(' {
                    i += 1;
                    while i < chars.len() && chars[i].is_whitespace() {
                        i += 1;
                    }
                    let start_sym = i;
                    while i < chars.len() && (chars[i].is_ascii_alphanumeric() || chars[i] == '_') {
                        i += 1;
                    }
                    let sym: String = chars[start_sym..i].iter().collect();
                    while i < chars.len() && chars[i].is_whitespace() {
                        i += 1;
                    }
                    if i < chars.len() && chars[i] == ')' {
                        i += 1;
                    }
                    sym
                } else {
                    let start_sym = i;
                    while i < chars.len() && (chars[i].is_ascii_alphanumeric() || chars[i] == '_') {
                        i += 1;
                    }
                    chars[start_sym..i].iter().collect()
                };
                let is_defined = defined_symbols.contains(symbol.as_str());
                tokens.push(Tok::Num(if is_defined { 1 } else { 0 }));
            } else {
                let is_defined = defined_symbols.contains(ident.as_str());
                tokens.push(Tok::Num(if is_defined { 1 } else { 0 }));
            }
            continue;
        }

        if c.is_ascii_digit() {
            let start = i;
            if c == '0' && i + 1 < chars.len() && (chars[i + 1] == 'x' || chars[i + 1] == 'X') {
                i += 2;
                while i < chars.len() && chars[i].is_ascii_hexdigit() {
                    i += 1;
                }
            } else {
                i += 1;
                while i < chars.len() && chars[i].is_ascii_digit() {
                    i += 1;
                }
            }
            let raw_num: String = chars[start..i].iter().collect();
            tokens.push(Tok::Num(parse_number(&raw_num)));
            continue;
        }

        i += 1;
    }

    struct Parser<'a> {
        tokens: &'a [Tok],
        pos: usize,
    }
    impl<'a> Parser<'a> {
        fn peek(&self) -> Option<Tok> {
            self.tokens.get(self.pos).copied()
        }
        fn bump(&mut self) -> Option<Tok> {
            let tok = self.peek();
            if tok.is_some() {
                self.pos += 1;
            }
            tok
        }
        fn parse_or(&mut self) -> bool {
            let mut lhs = self.parse_and();
            while matches!(self.peek(), Some(Tok::OrOr)) {
                self.bump();
                lhs = lhs || self.parse_and();
            }
            lhs
        }
        fn parse_and(&mut self) -> bool {
            let mut lhs = self.parse_unary();
            while matches!(self.peek(), Some(Tok::AndAnd)) {
                self.bump();
                lhs = lhs && self.parse_unary();
            }
            lhs
        }
        fn parse_unary(&mut self) -> bool {
            if matches!(self.peek(), Some(Tok::Not)) {
                self.bump();
                !self.parse_unary()
            } else {
                self.parse_primary()
            }
        }
        fn parse_primary(&mut self) -> bool {
            match self.bump() {
                Some(Tok::Num(n)) => n != 0,
                Some(Tok::LParen) => {
                    let value = self.parse_or();
                    if matches!(self.peek(), Some(Tok::RParen)) {
                        self.bump();
                    }
                    value
                }
                _ => false,
            }
        }
    }

    let mut parser = Parser {
        tokens: &tokens,
        pos: 0,
    };
    parser.parse_or()
}

fn sanitize_for_preprocessed_parse(
    source: &str,
    include_fallback_prelude: bool,
    defined_symbols: &HashSet<String>,
) -> String {
    let source = source.trim_start_matches('\u{feff}');
    let without_comments = strip_c_comments(source);
    let prelude_len = if include_fallback_prelude {
        FALLBACK_PRELUDE.len() + 1
    } else {
        0
    };
    let mut out = String::with_capacity(without_comments.len() + prelude_len + 16);
    if include_fallback_prelude {
        out.push_str(FALLBACK_PRELUDE);
        out.push('\n');
    }

    let mut conditional_stack: Vec<ConditionalFrame> = Vec::new();

    for line in without_comments.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with('#') {
            if let Some(rest) = trimmed.strip_prefix("#ifdef") {
                let symbol = rest.trim();
                let parent_active = conditional_active(&conditional_stack);
                let cond = defined_symbols.contains(symbol);
                conditional_stack.push(ConditionalFrame {
                    parent_active,
                    currently_active: parent_active && cond,
                    branch_taken: cond,
                });
            } else if let Some(rest) = trimmed.strip_prefix("#ifndef") {
                let symbol = rest.trim();
                let parent_active = conditional_active(&conditional_stack);
                let cond = !defined_symbols.contains(symbol);
                conditional_stack.push(ConditionalFrame {
                    parent_active,
                    currently_active: parent_active && cond,
                    branch_taken: cond,
                });
            } else if let Some(rest) = trimmed.strip_prefix("#if") {
                let expr = rest.trim();
                let parent_active = conditional_active(&conditional_stack);
                let cond = eval_preprocessor_expr(expr, defined_symbols);
                conditional_stack.push(ConditionalFrame {
                    parent_active,
                    currently_active: parent_active && cond,
                    branch_taken: cond,
                });
            } else if let Some(rest) = trimmed.strip_prefix("#elif") {
                let expr = rest.trim();
                if let Some(frame) = conditional_stack.last_mut() {
                    if !frame.parent_active || frame.branch_taken {
                        frame.currently_active = false;
                    } else {
                        let cond = eval_preprocessor_expr(expr, defined_symbols);
                        frame.currently_active = cond;
                        if cond {
                            frame.branch_taken = true;
                        }
                    }
                }
            } else if trimmed.starts_with("#else") {
                if let Some(frame) = conditional_stack.last_mut() {
                    frame.currently_active = frame.parent_active && !frame.branch_taken;
                    frame.branch_taken = true;
                }
            } else if trimmed.starts_with("#endif") {
                conditional_stack.pop();
            }
            out.push_str(&blank_like(line));
            out.push('\n');
            continue;
        }
        if !conditional_active(&conditional_stack) {
            out.push_str(&blank_like(line));
            out.push('\n');
            continue;
        }
        let line = strip_fallback_annotations(line);
        out.push_str(&line);
        out.push('\n');
    }

    out
}

fn strip_fallback_annotations(line: &str) -> String {
    // These are usually macro-based annotations in decomp projects and
    // should not change executable semantics for fallback parsing.
    const TOKENS: &[&str] = &["UNUSED", "ALIGNED8", "ALIGNED16", "ALIGNED32"];

    let mut bytes = line.as_bytes().to_vec();
    let upper = line.to_ascii_uppercase();

    for token in TOKENS {
        let mut start_at = 0;
        while let Some(found) = upper[start_at..].find(token) {
            let idx = start_at + found;
            let end = idx + token.len();
            let prev_ok = idx == 0
                || !line.as_bytes()[idx - 1].is_ascii_alphanumeric()
                    && line.as_bytes()[idx - 1] != b'_';
            let next_ok = end == line.len()
                || !line.as_bytes()[end].is_ascii_alphanumeric() && line.as_bytes()[end] != b'_';
            if prev_ok && next_ok {
                for slot in &mut bytes[idx..end] {
                    *slot = b' ';
                }
            }
            start_at = end;
        }
    }

    strip_leading_uppercase_declaration_macros(&mut bytes);
    String::from_utf8(bytes).unwrap_or_else(|_| line.to_string())
}

fn strip_leading_uppercase_declaration_macros(bytes: &mut [u8]) {
    let len = bytes.len();
    let mut idx = 0usize;
    while idx < len && bytes[idx].is_ascii_whitespace() {
        idx += 1;
    }

    loop {
        let start = idx;
        while idx < len && (bytes[idx].is_ascii_alphanumeric() || bytes[idx] == b'_') {
            idx += 1;
        }
        if idx == start {
            break;
        }

        let token = std::str::from_utf8(&bytes[start..idx]).unwrap_or("");
        let looks_like_macro = token
            .chars()
            .all(|ch| ch.is_ascii_uppercase() || ch == '_' || ch.is_ascii_digit())
            && token.chars().any(|ch| ch.is_ascii_uppercase());
        if !looks_like_macro {
            break;
        }

        for slot in &mut bytes[start..idx] {
            *slot = b' ';
        }

        while idx < len && bytes[idx].is_ascii_whitespace() {
            idx += 1;
        }
    }
}

fn blank_like(line: &str) -> String {
    line.chars()
        .map(|ch| if ch == '\t' { '\t' } else { ' ' })
        .collect()
}

const FALLBACK_PRELUDE: &str = r#"
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef long long intptr_t;
typedef unsigned long long uintptr_t;
typedef unsigned long long size_t;
typedef signed char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef int bool;
typedef struct FILE FILE;
typedef struct OSPfs OSPfs;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct OSPifRam OSPifRam;
typedef struct OSMesg OSMesg;
typedef struct OSThread OSThread;
typedef struct OSTimer OSTimer;
typedef struct __OSContRamReadFormat __OSContRamReadFormat;
"#;

fn strip_c_comments(input: &str) -> String {
    let bytes = input.as_bytes();
    let mut out = String::with_capacity(input.len());
    let mut i = 0;
    let mut in_string = false;
    let mut in_char = false;

    while i < bytes.len() {
        let c = bytes[i] as char;
        let next = if i + 1 < bytes.len() {
            Some(bytes[i + 1] as char)
        } else {
            None
        };

        if in_string {
            out.push(c);
            if c == '\\' {
                if let Some(n) = next {
                    out.push(n);
                    i += 2;
                    continue;
                }
            } else if c == '"' {
                in_string = false;
            }
            i += 1;
            continue;
        }

        if in_char {
            out.push(c);
            if c == '\\' {
                if let Some(n) = next {
                    out.push(n);
                    i += 2;
                    continue;
                }
            } else if c == '\'' {
                in_char = false;
            }
            i += 1;
            continue;
        }

        if c == '"' {
            in_string = true;
            out.push(c);
            i += 1;
            continue;
        }
        if c == '\'' {
            in_char = true;
            out.push(c);
            i += 1;
            continue;
        }

        if c == '/' && next == Some('/') {
            out.push(' ');
            out.push(' ');
            i += 2;
            while i < bytes.len() && bytes[i] as char != '\n' {
                out.push(' ');
                i += 1;
            }
            continue;
        }

        if c == '/' && next == Some('*') {
            out.push(' ');
            out.push(' ');
            i += 2;
            while i + 1 < bytes.len() {
                if bytes[i] as char == '*' && bytes[i + 1] as char == '/' {
                    out.push(' ');
                    out.push(' ');
                    i += 2;
                    break;
                }
                out.push(if bytes[i] as char == '\n' { '\n' } else { ' ' });
                i += 1;
            }
            continue;
        }

        out.push(c);
        i += 1;
    }

    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn test_parse_simple_function() {
        let source = r#"
            int add(int a, int b) {
                return a + b;
            }
        "#;

        let result = parse_c_source(source);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_struct() {
        let source = r#"
            struct Point {
                float x;
                float y;
            };
        "#;

        let result = parse_c_source(source);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_file_with_comments_and_include_lines() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let path = std::env::temp_dir().join(format!("kain_import_parser_{unique}.c"));
        let source = r#"
            // parser should tolerate this in fallback mode
            #include <stdio.h>
            int add(int a, int b) {
                return a + b;
            }
        "#;
        kfs::write_text(&path, source).unwrap();

        let result = parse_c_file(&path);
        let _ = kfs::remove_file(&path);
        assert!(result.is_ok());
    }

    #[test]
    fn test_sanitize_respects_ifdef_branches() {
        let source = r#"
            #ifdef VERSION_CN
            int cn_only() { return 1; }
            #else
            int us_only() { return 2; }
            #endif
        "#;
        let mut defined = HashSet::new();
        defined.insert("VERSION_CN".to_string());
        let sanitized = sanitize_for_preprocessed_parse(source, false, &defined);
        assert!(sanitized.contains("cn_only"));
        assert!(!sanitized.contains("us_only"));
    }

    #[test]
    fn test_sanitize_evaluates_if_expression() {
        let source = r#"
            #if defined(VERSION_US) || defined(VERSION_CN)
            int keep_me() { return 1; }
            #endif
        "#;
        let mut defined = HashSet::new();
        defined.insert("VERSION_US".to_string());
        let sanitized = sanitize_for_preprocessed_parse(source, false, &defined);
        assert!(sanitized.contains("keep_me"));
    }

    #[test]
    fn test_parse_source_with_export_macro_prefix_and_bool_return() {
        let source = r#"
            #if defined(_WIN32)
            #define BEACON_EXPORT __declspec(dllexport)
            #else
            #define BEACON_EXPORT
            #endif

            BEACON_EXPORT _Bool beacon_is_even(int value);
        "#;

        let result = parse_c_source(source);
        assert!(result.is_ok());
    }

    #[test]
    fn test_layout_metadata_tracks_pragma_pack_state() {
        let source =
            "#pragma pack(push, 1)\nstruct Packet { char tag; int value; };\n#pragma pack(pop)\n";
        let metadata = CSourceLayoutMetadata::from_source(source);
        let packet_start = source.find("struct Packet").unwrap();
        let packet_end = source.find("};").unwrap() + 2;
        let span = CSpan::span(packet_start, packet_end);

        assert_eq!(metadata.pack_align_bits_for_span(span), Some(8));
    }

    #[test]
    fn test_layout_metadata_detects_packed_and_aligned_attributes() {
        let source = "struct __attribute__((packed, aligned(16))) Packet { char tag; int value; };";
        let metadata = CSourceLayoutMetadata::from_source(source);
        let span = CSpan::span(0, source.len());

        assert!(metadata.has_packed_attr_for_span(span));
        assert_eq!(metadata.explicit_type_align_bits_for_span(span), Some(128));
    }

    #[test]
    fn test_layout_metadata_tracks_named_pragma_pack_stack() {
        let source = "#pragma pack(push, outer, 4)\nstruct A { char tag; int value; };\n#pragma pack(push, inner, 1)\nstruct B { char tag; int value; };\n#pragma pack(pop, inner)\nstruct C { char tag; int value; };\n#pragma pack(pop, outer)\n";
        let metadata = CSourceLayoutMetadata::from_source(source);
        let a_span = CSpan::span(
            source.find("struct A").unwrap(),
            source.find("struct B").unwrap(),
        );
        let b_span = CSpan::span(
            source.find("struct B").unwrap(),
            source.find("struct C").unwrap(),
        );
        let c_span = CSpan::span(source.find("struct C").unwrap(), source.len());

        assert_eq!(metadata.pack_align_bits_for_span(a_span), Some(32));
        assert_eq!(metadata.pack_align_bits_for_span(b_span), Some(8));
        assert_eq!(metadata.pack_align_bits_for_span(c_span), Some(32));
    }
}
