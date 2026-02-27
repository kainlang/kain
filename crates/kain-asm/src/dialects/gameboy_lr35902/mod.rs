use crate::dialects::furby_6502::{
    ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore,
};
use crate::error::{AsmError, AsmResult};
use indexmap::IndexMap;
use kain_core::{
    diagnostics::SpanMapper,
    span::Span,
    AsmBlock, AsmDataTable, AsmDirective, AsmInstr, AsmProgram, ParityTraceFrame, TranslitUnit,
};
use petgraph::graphmap::DiGraphMap;
use rayon::prelude::*;
use schemars::JsonSchema;
use serde::Serialize;
use smallvec::SmallVec;
use std::collections::{BTreeMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use tracing::{debug, info};

const SUPPORTED_FORMATS: &[&str] = &["lr35902-gameboy", "gameboy-lr35902", "gb-lr35902", "lr35902", "gameboy"];
const MAX_EXPAND_DEPTH: usize = 16;

#[derive(Clone)]
struct SourceLine {
    text: String,
    file: String,
    line: usize,
}

#[derive(Clone)]
struct CanonLine {
    text: String,
    file: String,
    line: usize,
    canon: usize,
}

#[derive(Clone)]
struct MacroDef {
    body: Vec<CanonLine>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct SourceProvenance {
    kind: String,
    symbol: String,
    source_file: String,
    source_line_start: usize,
    source_line_end: usize,
    canonical_line_start: usize,
    canonical_line_end: usize,
}

#[derive(Serialize)]
struct GameboyMap {
    translit_units: Vec<TranslitUnit>,
    parity_trace_schema: ParityTraceFrame,
    source_provenance: Vec<SourceProvenance>,
    rom_model_path: String,
    parity_harness_path: String,
    diagnostics_path: String,
}

#[derive(Clone, Serialize, JsonSchema)]
struct RomSection {
    name: String,
    bank: u16,
    start: u16,
    end: u16,
    bytes: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct RomBank {
    bank: u16,
    used_bytes: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyRomModel {
    sections: Vec<RomSection>,
    banks: Vec<RomBank>,
}

#[derive(Clone, Serialize, JsonSchema)]
struct ParityCheckpoint {
    tick: u64,
    label: String,
    source_line: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyParityHarness {
    checkpoints: Vec<ParityCheckpoint>,
    graph_nodes: usize,
    graph_edges: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct AsmDiagnostic {
    severity: String,
    code: String,
    message: String,
    canonical_line: usize,
    canonical_col: usize,
    canonical_snippet: String,
    source_file: String,
    source_line: usize,
}

#[derive(Clone, Serialize, JsonSchema)]
struct GameboyDiagnostics {
    diagnostics: Vec<AsmDiagnostic>,
}

pub fn import_asm(input: &Path, format: &str, out_kn: Option<&Path>, validate_only: bool) -> AsmResult<ImportAsmOutput> {
    let _ = tracing_subscriber::fmt::try_init();
    let normalized = format.trim().to_ascii_lowercase();
    if !SUPPORTED_FORMATS.iter().any(|v| *v == normalized) {
        return Err(AsmError::runtime(format!("Unsupported asm format '{}'. Supported: {}", format, SUPPORTED_FORMATS.join(", "))));
    }
    info!("kain-asm import start: format={}, input={}", normalized, input.display());
    let raw = load_asm_with_includes(input)?;
    let canonical = canonicalize_asm(&raw);
    let canonical_text = canonical.iter().map(|l| l.text.as_str()).collect::<Vec<_>>().join("\n");
    let expanded = expand_rgbds_semantics(&canonical);
    let expanded_text = expanded.iter().map(|l| l.text.as_str()).collect::<Vec<_>>().join("\n");
    let (parsed, provenance) = parse_asm_program(&expanded);
    let translit_units = build_translit_units(&parsed);
    let report = build_recovery_report(input, &expanded, &parsed);

    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let root = if cwd.join("crates").is_dir() { cwd.clone() } else if cwd.join("Kain").join("crates").is_dir() { cwd.join("Kain") } else { cwd };
    let research_dir = root.join("Research").join("gameboy");
    let generated_dir = root.join("generated");
    fs::create_dir_all(&research_dir).map_err(AsmError::Io)?;
    fs::create_dir_all(&generated_dir).map_err(AsmError::Io)?;

    let canonical_asm_path = research_dir.join("gameboy_canonical.asm");
    let generated_kn_path = out_kn.map(Path::to_path_buf).unwrap_or_else(|| generated_dir.join("gameboy_firmware.kn"));
    let map_json_path = generated_kn_path.parent().unwrap_or(&generated_dir).join("gameboy_map.json");
    let report_json_path = research_dir.join("gameboy_recovery_report.json");
    let rom_model_path = map_json_path.with_file_name("gameboy_rom_model.json");
    let parity_harness_path = map_json_path.with_file_name("gameboy_parity_harness.json");
    let diagnostics_path = map_json_path.with_file_name("gameboy_diagnostics.json");
    let rom_model = build_rom_model(&expanded, &parsed);
    let parity_harness = build_parity_harness(&parsed);
    let diagnostics = build_diagnostics(&expanded, &expanded_text, &report);
    debug!(
        "import stages done: canonical_lines={}, expanded_lines={}, blocks={}",
        canonical.len(),
        expanded.len(),
        parsed.blocks.len()
    );

    if !validate_only {
        fs::write(&canonical_asm_path, canonical_text).map_err(AsmError::Io)?;
        fs::write(&generated_kn_path, render_kain_firmware(&parsed, &translit_units)).map_err(AsmError::Io)?;
        let map = GameboyMap {
            translit_units: translit_units.clone(),
            parity_trace_schema: default_parity_trace_schema(),
            source_provenance: provenance,
            rom_model_path: rom_model_path.display().to_string(),
            parity_harness_path: parity_harness_path.display().to_string(),
            diagnostics_path: diagnostics_path.display().to_string(),
        };
        let map_json = serde_json::to_string_pretty(&map).map_err(|e| AsmError::runtime(format!("Failed to serialize gameboy map: {}", e)))?;
        fs::write(&map_json_path, map_json).map_err(AsmError::Io)?;
        let rom_json = serde_json::to_string_pretty(&rom_model).map_err(|e| AsmError::runtime(format!("Failed to serialize rom model: {}", e)))?;
        fs::write(&rom_model_path, rom_json).map_err(AsmError::Io)?;
        let parity_json = serde_json::to_string_pretty(&parity_harness).map_err(|e| AsmError::runtime(format!("Failed to serialize parity harness: {}", e)))?;
        fs::write(&parity_harness_path, parity_json).map_err(AsmError::Io)?;
        let diagnostics_json = serde_json::to_string_pretty(&diagnostics).map_err(|e| AsmError::runtime(format!("Failed to serialize diagnostics: {}", e)))?;
        fs::write(&diagnostics_path, diagnostics_json).map_err(AsmError::Io)?;
    }
    let report_json = serde_json::to_string_pretty(&report).map_err(|e| AsmError::runtime(format!("Failed to serialize gameboy recovery report: {}", e)))?;
    fs::write(&report_json_path, report_json).map_err(AsmError::Io)?;

    Ok(ImportAsmOutput { canonical_asm_path, generated_kn_path, map_json_path, report_json_path, parsed, translit_units })
}

fn load_asm_with_includes(entry: &Path) -> AsmResult<Vec<SourceLine>> {
    fn walk(path: &Path, stack: &mut HashSet<PathBuf>, out: &mut Vec<SourceLine>) -> AsmResult<()> {
        let canonical = fs::canonicalize(path).or_else(|_| Ok::<PathBuf, std::io::Error>(path.to_path_buf())).map_err(AsmError::Io)?;
        if !stack.insert(canonical.clone()) {
            return Err(AsmError::runtime(format!("Detected recursive include loop at {}", canonical.display())));
        }
        let content = fs::read_to_string(&canonical).map_err(AsmError::Io)?;
        let dir = canonical.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
        let file = canonical.display().to_string();
        for (idx, line) in content.lines().enumerate() {
            let ln = idx + 1;
            if let Some(inc) = parse_include_path(line) {
                let inc_path = dir.join(inc);
                if inc_path.exists() {
                    walk(&inc_path, stack, out)?;
                    continue;
                }
            }
            out.push(SourceLine { text: line.to_string(), file: file.clone(), line: ln });
        }
        stack.remove(&canonical);
        Ok(())
    }
    let mut out = Vec::<SourceLine>::new();
    let mut stack = HashSet::<PathBuf>::new();
    walk(entry, &mut stack, &mut out)?;
    Ok(out)
}

fn parse_include_path(line: &str) -> Option<String> {
    let code = strip_comment(line).trim();
    if !code.to_ascii_uppercase().starts_with("INCLUDE ") { return None; }
    let i0 = code.find('"')?;
    let rest = &code[i0 + 1..];
    let i1 = rest.find('"')?;
    Some(rest[..i1].to_string())
}

fn canonicalize_asm(raw: &[SourceLine]) -> Vec<CanonLine> {
    let mut out = Vec::<CanonLine>::new();
    for line in raw {
        let trimmed = line.text.replace('\u{feff}', "").trim().to_string();
        if trimmed.is_empty() { continue; }
        let squashed = trimmed.chars().map(|c| if c.is_ascii() && !c.is_control() { c } else { ' ' }).collect::<String>().split_whitespace().collect::<Vec<_>>().join(" ");
        if squashed.is_empty() { continue; }
        out.push(CanonLine { text: squashed, file: line.file.clone(), line: line.line, canon: out.len() + 1 });
    }
    out
}

fn expand_rgbds_semantics(lines: &[CanonLine]) -> Vec<CanonLine> {
    let mut out = Vec::<CanonLine>::new();
    let mut macros = IndexMap::<String, MacroDef>::new();
    let mut symbols = IndexMap::<String, i64>::new();
    let mut stack = Vec::<(bool, bool)>::new();
    let mut macro_invoke_counter = 0u64;
    let mut active = true;
    let mut i = 0usize;
    while i < lines.len() {
        let line = lines[i].clone();
        let text = strip_comment(&line.text).trim().to_string();
        if text.is_empty() { i += 1; continue; }
        if let Some((name, consumed)) = parse_macro_definition(lines, i) {
            if active { macros.insert(name.to_ascii_uppercase(), MacroDef { body: lines[(i + 1)..(i + consumed - 1)].to_vec() }); }
            i += consumed;
            continue;
        }
        let token = text.split_whitespace().next().unwrap_or("").to_ascii_uppercase();
        if token == "IF" {
            let cond = if active { eval_cond(text[2..].trim(), &symbols) } else { false };
            stack.push((active, cond));
            active = active && cond;
            i += 1;
            continue;
        }
        if token == "ELSE" {
            if let Some((parent, cond)) = stack.last().copied() { active = parent && !cond; }
            i += 1;
            continue;
        }
        if token == "ENDC" {
            if let Some((parent, _)) = stack.pop() { active = parent; }
            i += 1;
            continue;
        }
        if token == "REPT" {
            if let Some((repeat_count, consumed)) = parse_rept_block(lines, i, &symbols) {
                if active {
                    let body = &lines[(i + 1)..(i + consumed - 1)];
                    for _ in 0..repeat_count {
                        for entry in body {
                            out.extend(expand_macro_call(entry, &macros, 0, &mut macro_invoke_counter));
                        }
                    }
                }
                i += consumed;
                continue;
            }
        }
        if !active { i += 1; continue; }
        if let Some((name, value)) = parse_symbol_assignment(&text, &symbols) {
            symbols.insert(name.to_ascii_uppercase(), value);
            out.push(line);
            i += 1;
            continue;
        }
        out.extend(expand_macro_call(&line, &macros, 0, &mut macro_invoke_counter));
        i += 1;
    }
    out
}

fn parse_rept_block(lines: &[CanonLine], start: usize, symbols: &IndexMap<String, i64>) -> Option<(usize, usize)> {
    let head = strip_comment(&lines[start].text).trim();
    let mut parts = head.split_whitespace();
    if !parts.next()?.eq_ignore_ascii_case("REPT") { return None; }
    let count_expr = parts.collect::<Vec<_>>().join(" ");
    let repeat_count = eval_expr_i64(count_expr.trim(), symbols)?.max(0) as usize;
    let mut depth = 0i32;
    let mut idx = start + 1;
    while idx < lines.len() {
        let token = strip_comment(&lines[idx].text).trim().split_whitespace().next().unwrap_or("").to_ascii_uppercase();
        if token == "REPT" {
            depth += 1;
        } else if token == "ENDR" {
            if depth == 0 {
                return Some((repeat_count, idx - start + 1));
            }
            depth -= 1;
        }
        idx += 1;
    }
    None
}

fn parse_macro_definition(lines: &[CanonLine], start: usize) -> Option<(String, usize)> {
    let head = strip_comment(&lines[start].text).trim();
    let name = if let Some((left, right)) = head.split_once(':') {
        if right.trim().eq_ignore_ascii_case("MACRO") { left.trim().to_string() } else { String::new() }
    } else {
        let mut p = head.split_whitespace();
        if p.next()?.eq_ignore_ascii_case("MACRO") { p.next().unwrap_or("").to_string() } else { String::new() }
    };
    if name.is_empty() { return None; }
    let mut idx = start + 1;
    while idx < lines.len() {
        if strip_comment(&lines[idx].text).trim().eq_ignore_ascii_case("ENDM") { return Some((name, idx - start + 1)); }
        idx += 1;
    }
    None
}

fn parse_symbol_assignment(text: &str, symbols: &IndexMap<String, i64>) -> Option<(String, i64)> {
    if let Some((left, right)) = text.split_once(" EQU ").or_else(|| text.split_once(" equ ")) {
        return Some((left.trim().to_string(), eval_expr_i64(right.trim(), symbols)?));
    }
    let mut p = text.split_whitespace();
    let k = p.next()?;
    let op = p.next()?.to_ascii_uppercase();
    if op == "EQU" || op == "DEF" {
        return Some((k.to_string(), eval_expr_i64(p.collect::<Vec<_>>().join(" ").trim(), symbols)?));
    }
    None
}
fn eval_cond(expr: &str, symbols: &IndexMap<String, i64>) -> bool { eval_expr_i64(expr, symbols).unwrap_or(0) != 0 }

fn parse_num(v: &str) -> Option<i64> {
    let t = v.trim();
    if let Some(h) = t.strip_prefix('$') { return i64::from_str_radix(h, 16).ok(); }
    if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) { return i64::from_str_radix(h, 16).ok(); }
    if let Some(h) = t.strip_prefix('%') { return i64::from_str_radix(h, 2).ok(); }
    t.parse::<i64>().ok()
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum ExprTok {
    Num(i64),
    Ident(String),
    LParen,
    RParen,
    Comma,
    OrOr,
    AndAnd,
    EqEq,
    NotEq,
    Lt,
    Lte,
    Gt,
    Gte,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    End,
}

fn eval_expr_i64(expr: &str, symbols: &IndexMap<String, i64>) -> Option<i64> {
    let toks = tokenize_expr(expr)?;
    let mut p = ExprParser { toks, idx: 0, symbols };
    let value = p.parse_or()?;
    if p.peek() != &ExprTok::End { return None; }
    Some(value)
}

fn tokenize_expr(expr: &str) -> Option<Vec<ExprTok>> {
    let mut out = SmallVec::<[ExprTok; 64]>::new();
    let bytes = expr.as_bytes();
    let mut i = 0usize;
    while i < bytes.len() {
        let c = bytes[i] as char;
        if c.is_ascii_whitespace() { i += 1; continue; }
        if c == '$' {
            let start = i + 1;
            let mut j = start;
            while j < bytes.len() && (bytes[j] as char).is_ascii_hexdigit() { j += 1; }
            if j == start { return None; }
            out.push(ExprTok::Num(i64::from_str_radix(&expr[start..j], 16).ok()?));
            i = j;
            continue;
        }
        if c == '%' {
            let start = i + 1;
            let mut j = start;
            while j < bytes.len() && matches!(bytes[j] as char, '0' | '1') { j += 1; }
            if j > start {
                out.push(ExprTok::Num(i64::from_str_radix(&expr[start..j], 2).ok()?));
                i = j;
            } else {
                out.push(ExprTok::Percent);
                i += 1;
            }
            continue;
        }
        if c.is_ascii_digit() {
            let start = i;
            let mut j = i;
            while j < bytes.len() && (bytes[j] as char).is_ascii_alphanumeric() { j += 1; }
            out.push(ExprTok::Num(parse_num(&expr[start..j])?));
            i = j;
            continue;
        }
        if c.is_ascii_alphabetic() || c == '_' || c == '.' {
            let start = i;
            let mut j = i;
            while j < bytes.len() {
                let ch = bytes[j] as char;
                if ch.is_ascii_alphanumeric() || ch == '_' || ch == '.' { j += 1; } else { break; }
            }
            out.push(ExprTok::Ident(expr[start..j].to_ascii_uppercase()));
            i = j;
            continue;
        }

        let two = if i + 1 < bytes.len() { Some(&expr[i..i + 2]) } else { None };
        if let Some(op) = two {
            match op {
                "||" => { out.push(ExprTok::OrOr); i += 2; continue; }
                "&&" => { out.push(ExprTok::AndAnd); i += 2; continue; }
                "==" => { out.push(ExprTok::EqEq); i += 2; continue; }
                "!=" => { out.push(ExprTok::NotEq); i += 2; continue; }
                "<=" => { out.push(ExprTok::Lte); i += 2; continue; }
                ">=" => { out.push(ExprTok::Gte); i += 2; continue; }
                _ => {}
            }
        }
        match c {
            '(' => out.push(ExprTok::LParen),
            ')' => out.push(ExprTok::RParen),
            ',' => out.push(ExprTok::Comma),
            '<' => out.push(ExprTok::Lt),
            '>' => out.push(ExprTok::Gt),
            '+' => out.push(ExprTok::Plus),
            '-' => out.push(ExprTok::Minus),
            '*' => out.push(ExprTok::Star),
            '/' => out.push(ExprTok::Slash),
            '!' => out.push(ExprTok::Bang),
            _ => return None,
        }
        i += 1;
    }
    out.push(ExprTok::End);
    Some(out.into_vec())
}

struct ExprParser<'a> {
    toks: Vec<ExprTok>,
    idx: usize,
    symbols: &'a IndexMap<String, i64>,
}

impl<'a> ExprParser<'a> {
    fn peek(&self) -> &ExprTok { &self.toks[self.idx] }
    fn eat(&mut self) -> ExprTok { let t = self.toks[self.idx].clone(); self.idx += 1; t }
    fn expect(&mut self, tok: ExprTok) -> Option<()> { if self.peek() == &tok { self.eat(); Some(()) } else { None } }

    fn parse_or(&mut self) -> Option<i64> {
        let mut lhs = self.parse_and()?;
        while self.peek() == &ExprTok::OrOr {
            self.eat();
            let rhs = self.parse_and()?;
            lhs = if lhs != 0 || rhs != 0 { 1 } else { 0 };
        }
        Some(lhs)
    }
    fn parse_and(&mut self) -> Option<i64> {
        let mut lhs = self.parse_eq()?;
        while self.peek() == &ExprTok::AndAnd {
            self.eat();
            let rhs = self.parse_eq()?;
            lhs = if lhs != 0 && rhs != 0 { 1 } else { 0 };
        }
        Some(lhs)
    }
    fn parse_eq(&mut self) -> Option<i64> {
        let mut lhs = self.parse_rel()?;
        loop {
            match self.peek() {
                ExprTok::EqEq => { self.eat(); let rhs = self.parse_rel()?; lhs = if lhs == rhs { 1 } else { 0 }; }
                ExprTok::NotEq => { self.eat(); let rhs = self.parse_rel()?; lhs = if lhs != rhs { 1 } else { 0 }; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_rel(&mut self) -> Option<i64> {
        let mut lhs = self.parse_add()?;
        loop {
            match self.peek() {
                ExprTok::Lt => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs < rhs { 1 } else { 0 }; }
                ExprTok::Lte => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs <= rhs { 1 } else { 0 }; }
                ExprTok::Gt => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs > rhs { 1 } else { 0 }; }
                ExprTok::Gte => { self.eat(); let rhs = self.parse_add()?; lhs = if lhs >= rhs { 1 } else { 0 }; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_add(&mut self) -> Option<i64> {
        let mut lhs = self.parse_mul()?;
        loop {
            match self.peek() {
                ExprTok::Plus => { self.eat(); lhs += self.parse_mul()?; }
                ExprTok::Minus => { self.eat(); lhs -= self.parse_mul()?; }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_mul(&mut self) -> Option<i64> {
        let mut lhs = self.parse_unary()?;
        loop {
            match self.peek() {
                ExprTok::Star => { self.eat(); lhs *= self.parse_unary()?; }
                ExprTok::Slash => {
                    self.eat();
                    let rhs = self.parse_unary()?;
                    if rhs == 0 { return None; }
                    lhs /= rhs;
                }
                ExprTok::Percent => {
                    self.eat();
                    let rhs = self.parse_unary()?;
                    if rhs == 0 { return None; }
                    lhs %= rhs;
                }
                _ => break,
            }
        }
        Some(lhs)
    }
    fn parse_unary(&mut self) -> Option<i64> {
        match self.peek() {
            ExprTok::Bang => { self.eat(); Some(if self.parse_unary()? == 0 { 1 } else { 0 }) }
            ExprTok::Minus => { self.eat(); Some(-self.parse_unary()?) }
            ExprTok::Plus => { self.eat(); self.parse_unary() }
            _ => self.parse_primary(),
        }
    }
    fn parse_primary(&mut self) -> Option<i64> {
        match self.eat() {
            ExprTok::Num(n) => Some(n),
            ExprTok::Ident(name) => {
                if name == "TRUE" { return Some(1); }
                if name == "FALSE" { return Some(0); }
                if name == "DEF" {
                    self.expect(ExprTok::LParen)?;
                    let symbol = match self.eat() { ExprTok::Ident(s) => s, _ => return None };
                    self.expect(ExprTok::RParen)?;
                    return Some(if self.symbols.contains_key(&symbol) { 1 } else { 0 });
                }
                Some(self.symbols.get(&name).copied().unwrap_or(0))
            }
            ExprTok::LParen => {
                let v = self.parse_or()?;
                self.expect(ExprTok::RParen)?;
                Some(v)
            }
            _ => None,
        }
    }
}

fn expand_macro_call(line: &CanonLine, macros: &IndexMap<String, MacroDef>, depth: usize, macro_invoke_counter: &mut u64) -> Vec<CanonLine> {
    if depth >= MAX_EXPAND_DEPTH { return vec![line.clone()]; }
    let text = strip_comment(&line.text).trim().to_string();
    let mut p = text.split_whitespace();
    let name = p.next().unwrap_or("");
    if name.ends_with(':') { return vec![line.clone()]; }
    let Some(def) = macros.get(&name.to_ascii_uppercase()) else { return vec![line.clone()]; };
    *macro_invoke_counter += 1;
    let macro_id = *macro_invoke_counter;
    let local_prefix = format!("__m{}_{}", macro_id, name.to_ascii_lowercase());
    let args = text[name.len()..].trim().split(',').map(str::trim).filter(|v| !v.is_empty()).map(str::to_string).collect::<Vec<_>>();
    let mut out = Vec::<CanonLine>::new();
    for l in &def.body {
        let mut t = l.text.clone();
        for i in 0..args.len() { t = t.replace(&format!("\\{}", i + 1), &args[i]); }
        t = t.replace("\\@", &macro_id.to_string());
        t = rewrite_local_macro_labels(&t, &local_prefix);
        let nested = expand_macro_call(&CanonLine { text: t, file: l.file.clone(), line: l.line, canon: l.canon }, macros, depth + 1, macro_invoke_counter);
        out.extend(nested);
    }
    out
}

fn rewrite_local_macro_labels(text: &str, prefix: &str) -> String {
    let mut out = String::new();
    let chars = text.chars().collect::<Vec<_>>();
    let mut i = 0usize;
    while i < chars.len() {
        let c = chars[i];
        if c == '.' {
            let prev = if i == 0 { ' ' } else { chars[i - 1] };
            if !(prev.is_ascii_alphanumeric() || prev == '_' || prev == '.') {
                let mut j = i + 1;
                while j < chars.len() && (chars[j].is_ascii_alphanumeric() || chars[j] == '_') { j += 1; }
                if j > i + 1 {
                    out.push_str(prefix);
                    out.push('_');
                    out.extend(chars[(i + 1)..j].iter());
                    i = j;
                    continue;
                }
            }
        }
        out.push(c);
        i += 1;
    }
    out
}

fn parse_asm_program(lines: &[CanonLine]) -> (AsmProgram, Vec<SourceProvenance>) {
    let mut blocks = Vec::<AsmBlock>::new();
    let mut directives = Vec::<AsmDirective>::new();
    let mut data_tables = Vec::<AsmDataTable>::new();
    let mut provenance = Vec::<SourceProvenance>::new();
    let mut cur_label = String::new();
    let mut cur_instrs = Vec::<AsmInstr>::new();
    let mut cur_start = 1usize;
    let mut cur_file = String::new();
    let mut cur_src_start = 1usize;
    let mut cur_src_end = 1usize;

    let flush = |end_line: usize, blocks: &mut Vec<AsmBlock>, provenance: &mut Vec<SourceProvenance>, cur_label: &mut String, cur_instrs: &mut Vec<AsmInstr>, cur_start: usize, cur_file: &str, cur_src_start: usize, cur_src_end: usize| {
        if !cur_label.is_empty() && !cur_instrs.is_empty() {
            let label = cur_label.clone();
            blocks.push(AsmBlock { label: label.clone(), instructions: std::mem::take(cur_instrs), source_line_start: cur_start, source_line_end: end_line });
            provenance.push(SourceProvenance { kind: "block".to_string(), symbol: label, source_file: cur_file.to_string(), source_line_start: cur_src_start, source_line_end: cur_src_end, canonical_line_start: cur_start, canonical_line_end: end_line });
        }
        cur_label.clear();
    };

    for l in lines {
        let text = strip_comment(&l.text).trim();
        if text.is_empty() { continue; }
        if is_label_line(text) {
            flush(l.canon.saturating_sub(1), &mut blocks, &mut provenance, &mut cur_label, &mut cur_instrs, cur_start, &cur_file, cur_src_start, cur_src_end);
            cur_label = normalize_label(text);
            cur_start = l.canon;
            cur_file = l.file.clone();
            cur_src_start = l.line;
            cur_src_end = l.line;
            continue;
        }
        if is_directive_line(text) {
            let sym = text.split_whitespace().next().unwrap_or(text).to_string();
            directives.push(AsmDirective { name: text.to_string(), args: Vec::new(), source_line: l.canon });
            provenance.push(SourceProvenance { kind: "directive".to_string(), symbol: sym, source_file: l.file.clone(), source_line_start: l.line, source_line_end: l.line, canonical_line_start: l.canon, canonical_line_end: l.canon });
            continue;
        }
        if let Some((label, bytes)) = parse_data_line(text) {
            data_tables.push(AsmDataTable { label: label.clone(), bytes, source_line_start: l.canon, source_line_end: l.canon });
            provenance.push(SourceProvenance { kind: "data_table".to_string(), symbol: label, source_file: l.file.clone(), source_line_start: l.line, source_line_end: l.line, canonical_line_start: l.canon, canonical_line_end: l.canon });
            continue;
        }
        if cur_label.is_empty() {
            cur_label = format!("bank_entry_{}", l.canon);
            cur_start = l.canon;
            cur_file = l.file.clone();
            cur_src_start = l.line;
            cur_src_end = l.line;
        }
        if let Some(instr) = parse_instruction(text, l.canon) {
            cur_src_end = l.line;
            cur_instrs.push(instr);
        }
    }
    flush(lines.len(), &mut blocks, &mut provenance, &mut cur_label, &mut cur_instrs, cur_start, &cur_file, cur_src_start, cur_src_end);
    (AsmProgram { blocks, directives, data_tables }, provenance)
}

fn strip_comment(line: &str) -> &str { line.split_once(';').map(|(l, _)| l).unwrap_or(line) }
fn normalize_label(label: &str) -> String { label.trim().trim_end_matches("::").trim_end_matches(':').to_string() }
fn is_label_line(line: &str) -> bool {
    let t = line.trim();
    if t.ends_with("::") {
        let n = t.trim_end_matches("::");
        return !n.is_empty() && n.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    if t.ends_with(':') {
        let n = t.trim_end_matches(':');
        return !n.is_empty() && n.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    false
}

fn is_directive_line(line: &str) -> bool {
    let code = strip_comment(line).trim();
    if code.is_empty() { return false; }
    let upper = code.to_ascii_uppercase();
    if upper.contains(" EQU ") { return true; }
    let t = upper.split_whitespace().next().unwrap_or("");
    matches!(t, "SECTION" | "INCBIN" | "INCLUDE" | "ORG" | "MACRO" | "ENDM" | "REPT" | "ENDR" | "DEF" | "PURGE" | "UNION" | "NEXTU" | "ENDU" | "RSRESET" | "RSSET" | "FAIL" | "WARN" | "PRINTT" | "PRINTV" | "ASSERT")
}

fn parse_data_line(line: &str) -> Option<(String, Vec<String>)> {
    let upper = line.to_ascii_uppercase();
    let marker = if upper.contains(" DB ") || upper.starts_with("DB ") { "DB" } else if upper.contains(" DW ") || upper.starts_with("DW ") { "DW" } else { return None; };
    let pos = upper.find(&format!(" {} ", marker)).map(|p| p + 1).unwrap_or(0);
    let left = line[..pos].trim();
    let right = line[pos + marker.len()..].trim();
    let label = if left.is_empty() { "__anonymous_table".to_string() } else { normalize_label(left) };
    let values = right.split(|c: char| c == ',' || c.is_ascii_whitespace()).map(str::trim).filter(|v| !v.is_empty()).map(str::to_string).collect::<Vec<_>>();
    if values.is_empty() { None } else { Some((label, values)) }
}

fn parse_instruction(line: &str, source_line: usize) -> Option<AsmInstr> {
    let mut parts = line.split_whitespace();
    let opcode = parts.next()?.to_ascii_uppercase();
    if !is_opcode_keyword(&opcode) { return None; }
    let operand = parts.collect::<Vec<_>>().join(" ");
    Some(AsmInstr { opcode, operand: if operand.is_empty() { None } else { Some(operand) }, source_line })
}

fn build_translit_units(program: &AsmProgram) -> Vec<TranslitUnit> {
    program
        .blocks
        .par_iter()
        .map(|b| TranslitUnit {
            source_label: b.label.clone(),
            target_item: format!("gb_{}", normalize_identifier(&b.label)),
            source_line_start: b.source_line_start,
            source_line_end: b.source_line_end,
        })
        .collect()
}

fn normalize_identifier(label: &str) -> String {
    let mut out = String::new();
    for c in label.chars() {
        if c.is_ascii_alphanumeric() { out.push(c.to_ascii_lowercase()); } else if c == '_' || c == '.' { out.push('_'); }
    }
    if out.is_empty() { "bank_label".to_string() } else { out }
}

fn render_kain_firmware(program: &AsmProgram, units: &[TranslitUnit]) -> String {
    let mut out = String::new();
    out.push_str("# Generated by kain import-asm --format lr35902-gameboy\n");
    out.push_str("# Game Boy LR35902 transliteration seed\n");
    out.push_str("# Includes UE5-facing runtime shim entrypoints for fixed-step simulation\n\n");
    out.push_str("struct CpuState:\n    a: Int\n    b: Int\n    c: Int\n    d: Int\n    e: Int\n    h: Int\n    l: Int\n    f: Int\n    sp: Int\n    pc: Int\n    ime: Int\n\n");
    out.push_str("struct Memory:\n    wram: Array<Int>\n    hram: Array<Int>\n    vram: Array<Int>\n    io_ports: Array<Int>\n    rom_banks: Array<Array<Int>>\n\n");
    out.push_str("struct Ue5ShimState:\n    cpu: CpuState\n    mem: Memory\n    tick: Int\n    last_effect: Int\n\n");
    out.push_str("fn read_port(port_id: Int) -> Int:\n    let _port = port_id\n    return 0\n\n");
    out.push_str("fn write_port(port_id: Int, value: Int):\n    let _port = port_id\n    let _value = value\n\n");
    out.push_str("fn step(cpu: CpuState, mem: Memory) -> (CpuState, Memory, Int):\n");
    out.push_str("    let next_cpu = cpu\n");
    out.push_str("    let next_mem = mem\n");
    out.push_str("    let opcode = 0\n");
    out.push_str("    if opcode == 0:\n");
    out.push_str("        # NOP\n");
    out.push_str("        return (next_cpu, next_mem, 4)\n");
    out.push_str("    return (next_cpu, next_mem, 0)\n\n");
    out.push_str("fn parity_frame(state: Ue5ShimState) -> Int:\n");
    out.push_str("    let _state = state\n");
    out.push_str("    return 0\n\n");
    out.push_str("fn ue5_init(cpu: CpuState, mem: Memory) -> Ue5ShimState:\n    return Ue5ShimState { cpu: cpu, mem: mem, tick: 0, last_effect: 0 }\n\n");
    out.push_str("fn ue5_reset(state: Ue5ShimState, cpu: CpuState, mem: Memory) -> Ue5ShimState:\n    let _old = state\n    return Ue5ShimState { cpu: cpu, mem: mem, tick: 0, last_effect: 0 }\n\n");
    out.push_str("fn ue5_tick_step(state: Ue5ShimState, step_count: Int) -> Ue5ShimState:\n    let next_state = state\n    let _steps = step_count\n    return next_state\n\n");
    out.push_str("fn ue5_apply_sensor_input(state: Ue5ShimState, port_id: Int, value: Int) -> Ue5ShimState:\n    write_port(port_id, value)\n    return state\n\n");
    out.push_str("fn ue5_read_actuator_output(state: Ue5ShimState, port_id: Int) -> Int:\n    let _state = state\n    return read_port(port_id)\n\n");
    out.push_str("const GAMEBOY_TABLES: Array<Array<Int>> = [\n");
    for table in &program.data_tables { out.push_str(&format!("    [{}],\n", table.bytes.join(", "))); }
    out.push_str("]\n\n");
    for unit in units {
        out.push_str(&format!("fn {}(cpu: CpuState, mem: Memory) -> (CpuState, Memory):\n    let next_cpu = cpu\n    let next_mem = mem\n", unit.target_item));
        if let Some(block) = program.blocks.iter().find(|b| b.label == unit.source_label) {
            for instr in &block.instructions {
                let op = instr.operand.as_deref().unwrap_or("");
                out.push_str(&format!("    # [{}:{}] {} {}\n", unit.source_label, instr.source_line, instr.opcode, op));
            }
        }
        out.push_str("    return (next_cpu, next_mem)\n\n");
    }
    out
}

fn build_rom_model(lines: &[CanonLine], program: &AsmProgram) -> GameboyRomModel {
    let mut current_section = "ROM0".to_string();
    let mut current_bank = 0u16;
    let mut section_start = 0u16;
    let mut pc = 0u16;
    let mut section_bytes = 0usize;
    let mut sections = Vec::<RomSection>::new();
    let mut bank_usage = BTreeMap::<u16, usize>::new();

    let flush_section = |name: &str, bank: u16, start: u16, pc_now: u16, bytes: usize, sections: &mut Vec<RomSection>| {
        if bytes == 0 { return; }
        sections.push(RomSection {
            name: name.to_string(),
            bank,
            start,
            end: pc_now.saturating_sub(1),
            bytes,
        });
    };

    for line in lines {
        let text = strip_comment(&line.text).trim();
        if text.is_empty() { continue; }

        if text.to_ascii_uppercase().starts_with("SECTION ") {
            flush_section(&current_section, current_bank, section_start, pc, section_bytes, &mut sections);
            let (name, bank, base) = parse_section_header(text).unwrap_or(("ROM0".to_string(), 0, pc));
            current_section = name;
            current_bank = bank;
            section_start = base;
            pc = base;
            section_bytes = 0;
            continue;
        }

        if parse_data_line(text).is_some() {
            if let Some((_, vals)) = parse_data_line(text) {
                section_bytes += vals.len();
                pc = pc.saturating_add(vals.len() as u16);
            }
            continue;
        }
        if let Some(instr) = parse_instruction(text, line.canon) {
            let size = estimate_instruction_size(&instr);
            section_bytes += size as usize;
            pc = pc.saturating_add(size);
        }
    }
    flush_section(&current_section, current_bank, section_start, pc, section_bytes, &mut sections);
    for sec in &sections {
        *bank_usage.entry(sec.bank).or_insert(0usize) += sec.bytes;
    }
    for table in &program.data_tables {
        let _ = table;
    }
    let banks = bank_usage.into_iter().map(|(bank, used_bytes)| RomBank { bank, used_bytes }).collect::<Vec<_>>();
    GameboyRomModel { sections, banks }
}

fn build_parity_harness(program: &AsmProgram) -> GameboyParityHarness {
    let checkpoints = program
        .blocks
        .iter()
        .take(512)
        .enumerate()
        .map(|(idx, block)| ParityCheckpoint {
            tick: idx as u64,
            label: block.label.clone(),
            source_line: block.source_line_start,
        })
        .collect::<Vec<_>>();
    let mut graph = DiGraphMap::<usize, ()>::new();
    for idx in 0..program.blocks.len() {
        graph.add_node(idx);
    }
    for (idx, block) in program.blocks.iter().enumerate() {
        for instr in &block.instructions {
            let op = instr.opcode.as_str();
            if op != "JP" && op != "JR" && op != "CALL" {
                continue;
            }
            let Some(operand) = instr.operand.as_deref() else {
                continue;
            };
            let target = operand
                .split(',')
                .next_back()
                .unwrap_or("")
                .trim()
                .trim_start_matches('.');
            if target.is_empty() {
                continue;
            }
            if let Some(to_idx) = program
                .blocks
                .iter()
                .position(|b| b.label.eq_ignore_ascii_case(target) || b.label.ends_with(&format!(".{}", target)))
            {
                graph.add_edge(idx, to_idx, ());
            }
        }
    }
    GameboyParityHarness {
        checkpoints,
        graph_nodes: graph.node_count(),
        graph_edges: graph.edge_count(),
    }
}

fn parse_section_header(text: &str) -> Option<(String, u16, u16)> {
    let rest = text.trim().strip_prefix("SECTION ")?.trim();
    let mut name = "ROM0".to_string();
    if let Some(i0) = rest.find('"') {
        let rest2 = &rest[i0 + 1..];
        if let Some(i1) = rest2.find('"') { name = rest2[..i1].to_string(); }
    }
    let up = rest.to_ascii_uppercase();
    let bank = if let Some(idx) = up.find("BANK[") {
        let chunk = &rest[(idx + 5)..];
        if let Some(end) = chunk.find(']') { parse_num(chunk[..end].trim()).unwrap_or(0).max(0) as u16 } else { 0 }
    } else { 0 };
    let base = if let Some(idx) = up.find("ROM0[$") {
        let chunk = &rest[(idx + 6)..];
        if let Some(end) = chunk.find(']') { parse_num(&format!("${}", &chunk[..end])).unwrap_or(0).max(0) as u16 } else { 0 }
    } else if let Some(idx) = up.find("ROMX[$") {
        let chunk = &rest[(idx + 6)..];
        if let Some(end) = chunk.find(']') { parse_num(&format!("${}", &chunk[..end])).unwrap_or(0).max(0) as u16 } else { 0x4000 }
    } else { 0 };
    Some((name, bank, base))
}

fn estimate_instruction_size(instr: &AsmInstr) -> u16 {
    let op = instr.opcode.as_str();
    let operand = instr.operand.as_deref().unwrap_or("");
    if op == "RST" || op == "RET" || op == "RETI" || op == "NOP" || op == "HALT" || op == "DAA" || op == "SCF" || op == "CCF" || op == "CPL" || op == "DI" || op == "EI" { return 1; }
    if op == "JP" || op == "CALL" { return 3; }
    if op == "JR" { return 2; }
    if op == "LD" {
        if operand.contains("[$") || operand.contains("($") { return 3; }
        if operand.contains('$') || operand.contains('%') || operand.chars().any(|c| c.is_ascii_digit()) { return 2; }
        return 1;
    }
    if operand.contains('$') || operand.contains('%') { 2 } else { 1 }
}

fn build_recovery_report(input: &Path, canonical: &[CanonLine], parsed: &AsmProgram) -> RecoveryReport {
    let mut unresolved_tokens = Vec::<RecoveryIssue>::new();
    let mut ambiguous_labels = Vec::<RecoveryIssue>::new();
    let mut seen = HashSet::<String>::new();
    for line in canonical {
        let t = line.text.trim();
        if t.is_empty() { continue; }
        let ok = is_label_line(t) || is_directive_line(t) || parse_data_line(t).is_some() || parse_instruction(t, line.canon).is_some();
        if !ok { unresolved_tokens.push(RecoveryIssue { line: line.canon, message: format!("Unrecognized canonical line: {}", t) }); }
        if is_label_line(t) {
            let label = normalize_label(t);
            if !seen.insert(label.clone()) { ambiguous_labels.push(RecoveryIssue { line: line.canon, message: format!("Duplicate label '{}'", label) }); }
        }
    }
    let total = canonical.len().max(1);
    let rec = canonical.len().saturating_sub(unresolved_tokens.len());
    let _ = parsed;
    RecoveryReport {
        input: input.display().to_string(),
        canonical_output: "Research/gameboy/gameboy_canonical.asm".to_string(),
        unresolved_tokens,
        ambiguous_labels,
        section_scores: vec![RecoverySectionScore { section: "global".to_string(), recognized: rec, total, confidence: (rec as f64) / (total as f64) }],
    }
}

fn build_diagnostics(lines: &[CanonLine], expanded_text: &str, report: &RecoveryReport) -> GameboyDiagnostics {
    let mapper = SpanMapper::new(expanded_text);
    let line_starts = compute_line_starts(expanded_text);
    let mut diagnostics = Vec::<AsmDiagnostic>::new();

    for issue in &report.unresolved_tokens {
        if let Some(diag) = issue_to_diag("error", "ASM_UNRESOLVED", issue, lines, &mapper, &line_starts) {
            diagnostics.push(diag);
        }
    }
    for issue in &report.ambiguous_labels {
        if let Some(diag) = issue_to_diag("warning", "ASM_AMBIGUOUS", issue, lines, &mapper, &line_starts) {
            diagnostics.push(diag);
        }
    }
    GameboyDiagnostics { diagnostics }
}

fn issue_to_diag(
    severity: &str,
    code: &str,
    issue: &RecoveryIssue,
    lines: &[CanonLine],
    mapper: &SpanMapper,
    line_starts: &[usize],
) -> Option<AsmDiagnostic> {
    if issue.line == 0 || issue.line > lines.len() {
        return None;
    }
    let line_idx = issue.line - 1;
    let canon = &lines[line_idx];
    let col0 = canon
        .text
        .char_indices()
        .find_map(|(idx, ch)| if ch.is_ascii_whitespace() { None } else { Some(idx) })
        .unwrap_or(0);
    let span_start = line_starts.get(line_idx).copied().unwrap_or(0).saturating_add(col0);
    let span = Span::new(span_start, span_start.saturating_add(1));
    let loc = mapper.span_to_location(span, "<expanded>");

    Some(AsmDiagnostic {
        severity: severity.to_string(),
        code: code.to_string(),
        message: issue.message.clone(),
        canonical_line: issue.line,
        canonical_col: loc.col,
        canonical_snippet: canon.text.clone(),
        source_file: canon.file.clone(),
        source_line: canon.line,
    })
}

fn compute_line_starts(text: &str) -> Vec<usize> {
    let mut starts = vec![0usize];
    for (idx, ch) in text.char_indices() {
        if ch == '\n' {
            starts.push(idx + 1);
        }
    }
    starts
}

fn default_parity_trace_schema() -> ParityTraceFrame {
    let mut registers = BTreeMap::new();
    for reg in ["a", "b", "c", "d", "e", "h", "l", "f", "sp", "pc"] { registers.insert(reg.to_string(), 0); }
    let mut flags = BTreeMap::new();
    for fl in ["z", "n", "h", "c"] { flags.insert(fl.to_string(), false); }
    ParityTraceFrame { tick: 0, pc: 0, opcode: "NOP".to_string(), registers, flags, notes: vec!["lr35902-schema".to_string()] }
}

fn is_opcode_keyword(kw: &str) -> bool {
    matches!(kw, "ADC" | "ADD" | "AND" | "BIT" | "CALL" | "CCF" | "CP" | "CPL" | "DAA" | "DEC" | "DI" | "EI" | "HALT" | "INC" | "JP" | "JR" | "LD" | "LDD" | "LDH" | "LDI" | "NOP" | "OR" | "POP" | "PUSH" | "RES" | "RET" | "RETI" | "RL" | "RLA" | "RLC" | "RLCA" | "RR" | "RRA" | "RRC" | "RRCA" | "RST" | "SBC" | "SCF" | "SET" | "SLA" | "SRA" | "SRL" | "STOP" | "SUB" | "SWAP" | "XOR")
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn macro_if_expansion() {
        let src = vec![
            CanonLine { text: "FLAG EQU 1".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "LoadA: MACRO".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "LD A, \\1".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "ENDM".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "IF FLAG".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
            CanonLine { text: "LoadA $42".to_string(), file: "a.asm".to_string(), line: 6, canon: 6 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 7, canon: 7 },
            CanonLine { text: "NOP".to_string(), file: "a.asm".to_string(), line: 8, canon: 8 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 9, canon: 9 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text == "LD A, $42"));
        assert!(!out.iter().any(|l| l.text == "NOP"));
    }

    #[test]
    fn expression_engine_supports_logic_comparison_and_def() {
        let src = vec![
            CanonLine { text: "A EQU 2".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "B EQU 3".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "IF (A + B == 5) && DEF(A) || DEF(MISSING)".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "LD A, $11".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
            CanonLine { text: "LD A, $22".to_string(), file: "a.asm".to_string(), line: 6, canon: 6 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 7, canon: 7 },
            CanonLine { text: "IF DEF(MISSING) || (A * B != 6)".to_string(), file: "a.asm".to_string(), line: 8, canon: 8 },
            CanonLine { text: "LD B, $33".to_string(), file: "a.asm".to_string(), line: 9, canon: 9 },
            CanonLine { text: "ELSE".to_string(), file: "a.asm".to_string(), line: 10, canon: 10 },
            CanonLine { text: "LD B, $44".to_string(), file: "a.asm".to_string(), line: 11, canon: 11 },
            CanonLine { text: "ENDC".to_string(), file: "a.asm".to_string(), line: 12, canon: 12 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text == "LD A, $11"));
        assert!(!out.iter().any(|l| l.text == "LD A, $22"));
        assert!(!out.iter().any(|l| l.text == "LD B, $33"));
        assert!(out.iter().any(|l| l.text == "LD B, $44"));
    }

    #[test]
    fn rept_expands_body_count() {
        let src = vec![
            CanonLine { text: "REPT 3".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: "LD A, $01".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "ENDR".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert_eq!(out.iter().filter(|l| l.text == "LD A, $01").count(), 3);
    }

    #[test]
    fn macro_local_labels_are_rewritten() {
        let src = vec![
            CanonLine { text: "LoopMacro: MACRO".to_string(), file: "a.asm".to_string(), line: 1, canon: 1 },
            CanonLine { text: ".loop:".to_string(), file: "a.asm".to_string(), line: 2, canon: 2 },
            CanonLine { text: "JR .loop".to_string(), file: "a.asm".to_string(), line: 3, canon: 3 },
            CanonLine { text: "ENDM".to_string(), file: "a.asm".to_string(), line: 4, canon: 4 },
            CanonLine { text: "LoopMacro".to_string(), file: "a.asm".to_string(), line: 5, canon: 5 },
        ];
        let out = expand_rgbds_semantics(&src);
        assert!(out.iter().any(|l| l.text.contains("__m")));
        assert!(!out.iter().any(|l| l.text == ".loop:"));
        assert!(!out.iter().any(|l| l.text == "JR .loop"));
    }

    #[test]
    fn import_writes_outputs() {
        let base = std::env::temp_dir().join(format!("kain_import_gb_test_{}", SystemTime::now().duration_since(UNIX_EPOCH).expect("clock").as_nanos()));
        fs::create_dir_all(&base).expect("mkdir");
        let input = base.join("gb_source.asm");
        fs::write(&input, "SECTION \"ROM0\", ROM0[$100]\nStart::\nLD A, $01\ndb $10, $20\n").expect("write input");
        let prev = std::env::current_dir().expect("cwd");
        std::env::set_current_dir(&base).expect("set cwd");
        let result = import_asm(&input, "lr35902-gameboy", None, false).expect("import");
        std::env::set_current_dir(prev).expect("restore");
        assert!(result.canonical_asm_path.exists());
        assert!(result.generated_kn_path.exists());
        assert!(result.map_json_path.exists());
        assert!(result.report_json_path.exists());
        let diag_path = result.map_json_path.parent().expect("map parent").join("gameboy_diagnostics.json");
        assert!(diag_path.exists());
    }
}
