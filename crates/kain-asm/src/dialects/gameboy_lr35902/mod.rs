use crate::dialects::furby_6502::{
    ImportAsmOutput, RecoveryIssue, RecoveryReport, RecoverySectionScore,
};
use crate::error::{AsmError, AsmResult};
use kain_core::{
    AsmBlock, AsmDataTable, AsmDirective, AsmInstr, AsmProgram, ParityTraceFrame, TranslitUnit,
};
use serde::Serialize;
use std::collections::{BTreeMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

const SUPPORTED_FORMATS: &[&str] = &[
    "lr35902-gameboy",
    "gameboy-lr35902",
    "gb-lr35902",
    "lr35902",
    "gameboy",
];

#[derive(Debug, Serialize)]
struct GameboyMap {
    translit_units: Vec<TranslitUnit>,
    parity_trace_schema: ParityTraceFrame,
}

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> AsmResult<ImportAsmOutput> {
    let normalized = format.trim().to_ascii_lowercase();
    if !SUPPORTED_FORMATS.iter().any(|f| *f == normalized) {
        return Err(AsmError::runtime(format!(
            "Unsupported asm format '{}'. Supported: {}",
            format,
            SUPPORTED_FORMATS.join(", ")
        )));
    }

    let raw = load_asm_with_includes(input)?;
    let canonical_text = canonicalize_asm(&raw);
    let parsed = parse_asm_program(&canonical_text);
    let translit_units = build_translit_units(&parsed);
    let report = build_recovery_report(input, &canonical_text, &parsed);

    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let project_root = resolve_project_root(&cwd);
    let research_dir = project_root.join("Research").join("gameboy");
    let generated_dir = project_root.join("generated");
    fs::create_dir_all(&research_dir).map_err(AsmError::Io)?;
    fs::create_dir_all(&generated_dir).map_err(AsmError::Io)?;

    let canonical_asm_path = research_dir.join("gameboy_canonical.asm");
    let generated_kn_path = out_kn
        .map(|v| v.to_path_buf())
        .unwrap_or_else(|| generated_dir.join("gameboy_firmware.kn"));
    let map_dir = generated_kn_path
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| generated_dir.clone());
    let map_json_path = map_dir.join("gameboy_map.json");
    let report_json_path = research_dir.join("gameboy_recovery_report.json");

    if !validate_only {
        fs::write(&canonical_asm_path, canonical_text).map_err(AsmError::Io)?;
        fs::write(
            &generated_kn_path,
            render_kain_firmware(&parsed, &translit_units),
        )
        .map_err(AsmError::Io)?;
        let map = GameboyMap {
            translit_units: translit_units.clone(),
            parity_trace_schema: default_parity_trace_schema(),
        };
        let map_json = serde_json::to_string_pretty(&map)
            .map_err(|e| AsmError::runtime(format!("Failed to serialize gameboy map: {}", e)))?;
        fs::write(&map_json_path, map_json).map_err(AsmError::Io)?;
    }

    let report_json = serde_json::to_string_pretty(&report).map_err(|e| {
        AsmError::runtime(format!("Failed to serialize gameboy recovery report: {}", e))
    })?;
    fs::write(&report_json_path, report_json).map_err(AsmError::Io)?;

    Ok(ImportAsmOutput {
        canonical_asm_path,
        generated_kn_path,
        map_json_path,
        report_json_path,
        parsed,
        translit_units,
    })
}

fn resolve_project_root(cwd: &Path) -> PathBuf {
    if cwd.join("crates").is_dir() {
        return cwd.to_path_buf();
    }
    if cwd.join("Kain").join("crates").is_dir() {
        return cwd.join("Kain");
    }
    cwd.to_path_buf()
}

fn load_asm_with_includes(entry: &Path) -> AsmResult<String> {
    let mut out = String::new();
    let mut stack = HashSet::<PathBuf>::new();
    load_file_recursive(entry, &mut stack, &mut out)?;
    Ok(out)
}

fn load_file_recursive(path: &Path, stack: &mut HashSet<PathBuf>, out: &mut String) -> AsmResult<()> {
    let canonical = fs::canonicalize(path)
        .or_else(|_| Ok::<PathBuf, std::io::Error>(path.to_path_buf()))
        .map_err(AsmError::Io)?;
    if !stack.insert(canonical.clone()) {
        return Err(AsmError::runtime(format!(
            "Detected recursive include loop at {}",
            canonical.display()
        )));
    }

    let content = fs::read_to_string(&canonical).map_err(AsmError::Io)?;
    let base_dir = canonical
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| PathBuf::from("."));

    out.push_str(&format!("; BEGIN_INCLUDE {}\n", canonical.display()));
    for line in content.lines() {
        if let Some(relative) = parse_include_path(line) {
            let include_path = base_dir.join(relative);
            if include_path.exists() {
                load_file_recursive(&include_path, stack, out)?;
                continue;
            }
        }
        out.push_str(line);
        out.push('\n');
    }
    out.push_str(&format!("; END_INCLUDE {}\n", canonical.display()));
    stack.remove(&canonical);
    Ok(())
}

fn parse_include_path(line: &str) -> Option<String> {
    let code = strip_comment(line).trim();
    let upper = code.to_ascii_uppercase();
    if !upper.starts_with("INCLUDE ") {
        return None;
    }
    let first_quote = code.find('"')?;
    let rest = &code[first_quote + 1..];
    let second_quote = rest.find('"')?;
    Some(rest[..second_quote].to_string())
}

fn canonicalize_asm(raw: &str) -> String {
    raw.lines()
        .map(|line| line.replace('\u{feff}', ""))
        .map(|line| line.trim().to_string())
        .filter(|line| !line.is_empty())
        .map(|line| {
            line.chars()
                .map(|c| if c.is_ascii() && !c.is_control() { c } else { ' ' })
                .collect::<String>()
                .split_whitespace()
                .collect::<Vec<_>>()
                .join(" ")
        })
        .collect::<Vec<_>>()
        .join("\n")
}

fn parse_asm_program(canonical: &str) -> AsmProgram {
    let mut blocks = Vec::<AsmBlock>::new();
    let mut directives = Vec::<AsmDirective>::new();
    let mut data_tables = Vec::<AsmDataTable>::new();

    let mut current_label = String::new();
    let mut current_instrs = Vec::<AsmInstr>::new();
    let mut current_start = 1usize;

    for (idx, original_line) in canonical.lines().enumerate() {
        let line_no = idx + 1;
        let line = strip_comment(original_line).trim();
        if line.is_empty() {
            continue;
        }

        if is_label_line(line) {
            flush_block(
                &mut blocks,
                &mut current_label,
                &mut current_instrs,
                &mut current_start,
                line_no.saturating_sub(1),
            );
            current_label = normalize_label(line);
            current_start = line_no;
            continue;
        }

        if is_directive_line(line) {
            directives.push(AsmDirective {
                name: line.to_string(),
                args: Vec::new(),
                source_line: line_no,
            });
            continue;
        }

        if let Some((label, bytes)) = parse_data_line(line) {
            data_tables.push(AsmDataTable {
                label,
                bytes,
                source_line_start: line_no,
                source_line_end: line_no,
            });
            continue;
        }

        if current_label.is_empty() {
            current_label = format!("bank_entry_{}", line_no);
            current_start = line_no;
        }
        if let Some(instr) = parse_instruction(line, line_no) {
            current_instrs.push(instr);
        }
    }

    flush_block(
        &mut blocks,
        &mut current_label,
        &mut current_instrs,
        &mut current_start,
        canonical.lines().count(),
    );

    AsmProgram {
        blocks,
        directives,
        data_tables,
    }
}

fn strip_comment(line: &str) -> &str {
    if let Some((left, _)) = line.split_once(';') {
        left
    } else {
        line
    }
}

fn is_label_line(line: &str) -> bool {
    let t = line.trim();
    if t.ends_with("::") {
        let name = t.trim_end_matches("::");
        return !name.is_empty()
            && name
                .chars()
                .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    if t.ends_with(':') {
        let name = t.trim_end_matches(':');
        return !name.is_empty()
            && name
                .chars()
                .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.');
    }
    false
}

fn normalize_label(label: &str) -> String {
    label
        .trim()
        .trim_end_matches("::")
        .trim_end_matches(':')
        .to_string()
}

fn is_directive_line(line: &str) -> bool {
    let code = strip_comment(line).trim();
    if code.is_empty() {
        return false;
    }
    let upper = code.to_ascii_uppercase();
    if upper.contains(" EQU ") {
        return true;
    }
    let token = upper.split_whitespace().next().unwrap_or("");
    matches!(
        token,
        "SECTION"
            | "INCBIN"
            | "INCLUDE"
            | "ORG"
            | "MACRO"
            | "ENDM"
            | "IF"
            | "ELSE"
            | "ENDC"
            | "REPT"
            | "ENDR"
            | "DEF"
            | "PURGE"
            | "UNION"
            | "NEXTU"
            | "ENDU"
            | "RSRESET"
            | "RSSET"
            | "FAIL"
            | "WARN"
            | "PRINTT"
            | "PRINTV"
            | "ASSERT"
    )
}

fn parse_data_line(line: &str) -> Option<(String, Vec<String>)> {
    let upper = line.to_ascii_uppercase();
    let marker = if upper.contains(" DB ") || upper.starts_with("DB ") {
        "DB"
    } else if upper.contains(" DW ") || upper.starts_with("DW ") {
        "DW"
    } else {
        return None;
    };

    let pos = if let Some(p) = upper.find(&format!(" {} ", marker)) {
        p + 1
    } else {
        0
    };
    let left = line[..pos].trim();
    let right = line[pos + marker.len()..].trim();
    let label = if left.is_empty() {
        "__anonymous_table".to_string()
    } else {
        normalize_label(left)
    };
    let values = right
        .split(|c: char| c == ',' || c.is_ascii_whitespace())
        .map(str::trim)
        .filter(|v| !v.is_empty())
        .map(str::to_string)
        .collect::<Vec<_>>();
    if values.is_empty() {
        None
    } else {
        Some((label, values))
    }
}

fn parse_instruction(line: &str, source_line: usize) -> Option<AsmInstr> {
    let mut parts = line.split_whitespace();
    let opcode = parts.next()?.to_ascii_uppercase();
    if !is_opcode_keyword(&opcode) {
        return None;
    }
    let operand = parts.collect::<Vec<_>>().join(" ");
    Some(AsmInstr {
        opcode,
        operand: if operand.is_empty() { None } else { Some(operand) },
        source_line,
    })
}

fn flush_block(
    blocks: &mut Vec<AsmBlock>,
    current_label: &mut String,
    current_instrs: &mut Vec<AsmInstr>,
    current_start: &mut usize,
    end_line: usize,
) {
    if !current_label.is_empty() && !current_instrs.is_empty() {
        blocks.push(AsmBlock {
            label: current_label.clone(),
            instructions: std::mem::take(current_instrs),
            source_line_start: *current_start,
            source_line_end: end_line,
        });
    }
    current_label.clear();
}

fn normalize_identifier(label: &str) -> String {
    let mut out = String::new();
    for c in label.chars() {
        if c.is_ascii_alphanumeric() {
            out.push(c.to_ascii_lowercase());
        } else if c == '_' || c == '.' {
            out.push('_');
        }
    }
    if out.is_empty() {
        "bank_label".to_string()
    } else {
        out
    }
}

fn build_translit_units(program: &AsmProgram) -> Vec<TranslitUnit> {
    program
        .blocks
        .iter()
        .map(|block| TranslitUnit {
            source_label: block.label.clone(),
            target_item: format!("gb_{}", normalize_identifier(&block.label)),
            source_line_start: block.source_line_start,
            source_line_end: block.source_line_end,
        })
        .collect()
}

fn render_kain_firmware(program: &AsmProgram, units: &[TranslitUnit]) -> String {
    let mut out = String::new();
    out.push_str("# Generated by kain import-asm --format lr35902-gameboy\n");
    out.push_str("# Game Boy LR35902 transliteration seed\n\n");
    out.push_str("struct CpuState:\n");
    out.push_str("    a: Int\n");
    out.push_str("    b: Int\n");
    out.push_str("    c: Int\n");
    out.push_str("    d: Int\n");
    out.push_str("    e: Int\n");
    out.push_str("    h: Int\n");
    out.push_str("    l: Int\n");
    out.push_str("    f: Int\n");
    out.push_str("    sp: Int\n");
    out.push_str("    pc: Int\n");
    out.push_str("    ime: Int\n\n");
    out.push_str("struct Memory:\n");
    out.push_str("    wram: Array<Int>\n");
    out.push_str("    hram: Array<Int>\n");
    out.push_str("    vram: Array<Int>\n");
    out.push_str("    io_ports: Array<Int>\n");
    out.push_str("    rom_banks: Array<Array<Int>>\n\n");
    out.push_str("fn read_port(port_id: Int) -> Int:\n");
    out.push_str("    return 0\n\n");
    out.push_str("fn write_port(port_id: Int, value: Int):\n");
    out.push_str("    let _port = port_id\n");
    out.push_str("    let _value = value\n\n");
    out.push_str("fn step(cpu: CpuState, mem: Memory) -> (CpuState, Memory, Int):\n");
    out.push_str("    return (cpu, mem, 0)\n\n");
    out.push_str("const GAMEBOY_TABLES: Array<Array<Int>> = [\n");
    for table in &program.data_tables {
        let values = table.bytes.join(", ");
        out.push_str(&format!("    [{}],\n", values));
    }
    out.push_str("]\n\n");

    for unit in units {
        let fn_name = &unit.target_item;
        out.push_str(&format!(
            "fn {}(cpu: CpuState, mem: Memory) -> (CpuState, Memory):\n",
            fn_name
        ));
        out.push_str("    let next_cpu = cpu\n");
        out.push_str("    let next_mem = mem\n");
        if let Some(block) = program.blocks.iter().find(|b| b.label == unit.source_label) {
            for instr in &block.instructions {
                let op = instr.operand.as_deref().unwrap_or("");
                out.push_str(&format!(
                    "    # [{}:{}] {} {}\n",
                    unit.source_label, instr.source_line, instr.opcode, op
                ));
            }
        }
        out.push_str("    return (next_cpu, next_mem)\n\n");
    }
    out
}

fn build_recovery_report(input: &Path, canonical: &str, parsed: &AsmProgram) -> RecoveryReport {
    let mut unresolved_tokens = Vec::<RecoveryIssue>::new();
    let mut ambiguous_labels = Vec::<RecoveryIssue>::new();
    let mut seen_labels = HashSet::<String>::new();

    for (idx, raw_line) in canonical.lines().enumerate() {
        let line_no = idx + 1;
        let line = raw_line.trim();
        if line.is_empty() {
            continue;
        }
        let recognized = is_label_line(line)
            || is_directive_line(line)
            || parse_data_line(line).is_some()
            || parse_instruction(line, line_no).is_some();
        if !recognized {
            unresolved_tokens.push(RecoveryIssue {
                line: line_no,
                message: format!("Unrecognized canonical line: {}", line),
            });
        }

        if is_label_line(line) {
            let label = normalize_label(line);
            if !seen_labels.insert(label.clone()) {
                ambiguous_labels.push(RecoveryIssue {
                    line: line_no,
                    message: format!("Duplicate label '{}'", label),
                });
            }
        }
    }

    let total_lines = canonical.lines().filter(|l| !l.trim().is_empty()).count();
    let recognized_lines = total_lines.saturating_sub(unresolved_tokens.len());
    let section_score = RecoverySectionScore {
        section: "global".to_string(),
        recognized: recognized_lines,
        total: total_lines.max(1),
        confidence: (recognized_lines as f64) / (total_lines.max(1) as f64),
    };

    let _ = parsed;
    RecoveryReport {
        input: input.display().to_string(),
        canonical_output: "Research/gameboy/gameboy_canonical.asm".to_string(),
        unresolved_tokens,
        ambiguous_labels,
        section_scores: vec![section_score],
    }
}

fn default_parity_trace_schema() -> ParityTraceFrame {
    let mut registers = BTreeMap::new();
    registers.insert("a".to_string(), 0);
    registers.insert("b".to_string(), 0);
    registers.insert("c".to_string(), 0);
    registers.insert("d".to_string(), 0);
    registers.insert("e".to_string(), 0);
    registers.insert("h".to_string(), 0);
    registers.insert("l".to_string(), 0);
    registers.insert("f".to_string(), 0);
    registers.insert("sp".to_string(), 0);
    registers.insert("pc".to_string(), 0);
    let mut flags = BTreeMap::new();
    flags.insert("z".to_string(), false);
    flags.insert("n".to_string(), false);
    flags.insert("h".to_string(), false);
    flags.insert("c".to_string(), false);
    ParityTraceFrame {
        tick: 0,
        pc: 0,
        opcode: "NOP".to_string(),
        registers,
        flags,
        notes: vec!["lr35902-schema".to_string()],
    }
}

fn is_opcode_keyword(kw: &str) -> bool {
    matches!(
        kw,
        "ADC"
            | "ADD"
            | "AND"
            | "BIT"
            | "CALL"
            | "CCF"
            | "CP"
            | "CPL"
            | "DAA"
            | "DEC"
            | "DI"
            | "EI"
            | "HALT"
            | "INC"
            | "JP"
            | "JR"
            | "LD"
            | "LDD"
            | "LDH"
            | "LDI"
            | "NOP"
            | "OR"
            | "POP"
            | "PUSH"
            | "RES"
            | "RET"
            | "RETI"
            | "RL"
            | "RLA"
            | "RLC"
            | "RLCA"
            | "RR"
            | "RRA"
            | "RRC"
            | "RRCA"
            | "RST"
            | "SBC"
            | "SCF"
            | "SET"
            | "SLA"
            | "SRA"
            | "SRL"
            | "STOP"
            | "SUB"
            | "SWAP"
            | "XOR"
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn canonicalize_compacts_whitespace() {
        let sample = "\u{feff}SECTION  \"ROM0\",  ROM0[$100]\n\nStart::\n  LD A, $01   ; comment\n";
        let out = canonicalize_asm(sample);
        assert!(out.contains("SECTION \"ROM0\", ROM0[$100]"));
        assert!(out.contains("Start::"));
        assert!(out.contains("LD A, $01 ; comment") || out.contains("LD A, $01"));
    }

    #[test]
    fn parser_extracts_blocks_and_tables() {
        let canonical = "Start::\nLD A, $01\ndb $10, $20\n";
        let parsed = parse_asm_program(canonical);
        assert!(!parsed.blocks.is_empty());
        assert!(!parsed.data_tables.is_empty());
        assert_eq!(parsed.data_tables[0].bytes.len(), 2);
    }

    #[test]
    fn import_asm_generates_outputs() {
        let base = std::env::temp_dir().join(format!(
            "kain_import_gb_test_{}",
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .expect("clock before epoch")
                .as_nanos()
        ));
        fs::create_dir_all(&base).expect("failed to create temp base");
        let input = base.join("gb_source.asm");
        fs::write(&input, "SECTION \"ROM0\", ROM0[$100]\nStart::\nLD A, $01\ndb $10, $20\n")
            .expect("failed to write input");
        let prev = std::env::current_dir().expect("cwd");
        std::env::set_current_dir(&base).expect("set cwd");
        let result = import_asm(&input, "lr35902-gameboy", None, false).expect("import should succeed");
        std::env::set_current_dir(prev).expect("restore cwd");

        assert!(result.canonical_asm_path.exists());
        assert!(result.generated_kn_path.exists());
        assert!(result.map_json_path.exists());
        assert!(result.report_json_path.exists());
    }
}
