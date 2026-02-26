use crate::error::{KainError, KainResult};
use kain_core::{AsmBlock, AsmDataTable, AsmDirective, AsmInstr, AsmProgram, ParityTraceFrame, TranslitUnit};
use serde::Serialize;
use std::collections::{BTreeMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

const SUPPORTED_FORMAT: &str = "6502-furby";

#[derive(Debug, Serialize, Clone)]
pub struct RecoveryIssue {
    pub line: usize,
    pub message: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct RecoverySectionScore {
    pub section: String,
    pub recognized: usize,
    pub total: usize,
    pub confidence: f64,
}

#[derive(Debug, Serialize, Clone)]
pub struct RecoveryReport {
    pub input: String,
    pub canonical_output: String,
    pub unresolved_tokens: Vec<RecoveryIssue>,
    pub ambiguous_labels: Vec<RecoveryIssue>,
    pub section_scores: Vec<RecoverySectionScore>,
}

#[derive(Debug, Serialize, Clone)]
pub struct ImportAsmOutput {
    pub canonical_asm_path: PathBuf,
    pub generated_kn_path: PathBuf,
    pub map_json_path: PathBuf,
    pub report_json_path: PathBuf,
    pub parsed: AsmProgram,
    pub translit_units: Vec<TranslitUnit>,
}

#[derive(Debug, Serialize)]
struct FurbyMap {
    translit_units: Vec<TranslitUnit>,
    parity_trace_schema: ParityTraceFrame,
}

pub fn import_asm(
    input: &Path,
    format: &str,
    out_kn: Option<&Path>,
    validate_only: bool,
) -> KainResult<ImportAsmOutput> {
    if format != SUPPORTED_FORMAT {
        return Err(KainError::runtime(format!(
            "Unsupported asm format '{}'. Supported: {}",
            format, SUPPORTED_FORMAT
        )));
    }

    let raw = fs::read_to_string(input).map_err(KainError::Io)?;
    let canonical_text = canonicalize_asm(&raw);
    let parsed = parse_asm_program(&canonical_text);
    let translit_units = build_translit_units(&parsed);
    let report = build_recovery_report(input, &canonical_text, &parsed);

    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let research_dir = cwd.join("Research").join("furby");
    let generated_dir = cwd.join("Kain").join("generated");
    fs::create_dir_all(&research_dir).map_err(KainError::Io)?;
    fs::create_dir_all(&generated_dir).map_err(KainError::Io)?;

    let canonical_asm_path = research_dir.join("furby_canonical.asm");
    let generated_kn_path = out_kn
        .map(|v| v.to_path_buf())
        .unwrap_or_else(|| generated_dir.join("furby_firmware.kn"));
    let map_json_path = generated_dir.join("furby_map.json");
    let report_json_path = research_dir.join("furby_recovery_report.json");

    if !validate_only {
        fs::write(&canonical_asm_path, canonical_text).map_err(KainError::Io)?;
        fs::write(&generated_kn_path, render_kain_firmware(&parsed, &translit_units)).map_err(KainError::Io)?;
        let furby_map = FurbyMap {
            translit_units: translit_units.clone(),
            parity_trace_schema: default_parity_trace_schema(),
        };
        let map_json = serde_json::to_string_pretty(&furby_map)
            .map_err(|e| KainError::runtime(format!("Failed to serialize furby map: {}", e)))?;
        fs::write(&map_json_path, map_json).map_err(KainError::Io)?;
    }

    let report_json = serde_json::to_string_pretty(&report)
        .map_err(|e| KainError::runtime(format!("Failed to serialize recovery report: {}", e)))?;
    fs::write(&report_json_path, report_json).map_err(KainError::Io)?;

    Ok(ImportAsmOutput {
        canonical_asm_path,
        generated_kn_path,
        map_json_path,
        report_json_path,
        parsed,
        translit_units,
    })
}

fn canonicalize_asm(raw: &str) -> String {
    let mut out = Vec::new();
    for line in raw.lines() {
        let trimmed = line.trim().replace('\u{feff}', "");
        if trimmed.is_empty() {
            continue;
        }
        if trimmed.starts_with("----------------------- Page ") {
            continue;
        }
        if trimmed.chars().all(|c| c == '-') {
            continue;
        }
        if trimmed.starts_with('A') && trimmed.chars().skip(1).all(|c| c.is_ascii_digit() || c == '-') {
            continue;
        }
        let sanitized = trimmed
            .chars()
            .map(|c| {
                if c.is_ascii() && !c.is_control() {
                    c
                } else if c == '\t' {
                    ' '
                } else {
                    ' '
                }
            })
            .collect::<String>();
        let squashed = sanitized.split_whitespace().collect::<Vec<_>>().join(" ");
        if !squashed.is_empty() {
            out.push(squashed);
        }
    }
    out.join("\n")
}

fn parse_asm_program(canonical: &str) -> AsmProgram {
    let mut blocks = Vec::new();
    let mut directives = Vec::new();
    let mut data_tables = Vec::new();

    let mut current_label = String::new();
    let mut current_instrs: Vec<AsmInstr> = Vec::new();
    let mut current_start = 0usize;
    let mut current_data: Option<AsmDataTable> = None;

    for (idx, line) in canonical.lines().enumerate() {
        let line_no = idx + 1;
        let upper = line.to_ascii_uppercase();

        if is_label_line(line) {
            flush_data_table(&mut data_tables, &mut current_data, line_no.saturating_sub(1));
            flush_block(
                &mut blocks,
                &mut current_label,
                &mut current_instrs,
                &mut current_start,
                line_no.saturating_sub(1),
            );
            current_label = line.trim_end_matches(':').to_string();
            current_start = line_no;
            continue;
        }

        if upper.starts_with(".CODE")
            || upper.starts_with(".SYNTAX")
            || upper.starts_with(".LINKLIST")
            || upper.starts_with(".SYMBOLS")
        {
            directives.push(AsmDirective {
                name: line.split_whitespace().next().unwrap_or("").to_string(),
                args: line.split_whitespace().skip(1).map(str::to_string).collect(),
                source_line: line_no,
            });
            continue;
        }

        if is_equ_directive(line) {
            let parts: Vec<&str> = line.split_whitespace().collect();
            directives.push(AsmDirective {
                name: "EQU".to_string(),
                args: parts.into_iter().map(str::to_string).collect(),
                source_line: line_no,
            });
            continue;
        }

        if let Some((label, bytes)) = parse_db_line(line) {
            if current_data.is_none() {
                current_data = Some(AsmDataTable {
                    label,
                    bytes: Vec::new(),
                    source_line_start: line_no,
                    source_line_end: line_no,
                });
            }
            if let Some(ref mut table) = current_data {
                table.bytes.extend(bytes);
                table.source_line_end = line_no;
            }
            continue;
        }

        if let Some(instr) = parse_instruction(line, line_no) {
            if current_label.is_empty() {
                current_label = format!("__entry_{}", line_no);
                current_start = line_no;
            }
            current_instrs.push(instr);
        }
    }

    flush_data_table(&mut data_tables, &mut current_data, canonical.lines().count());
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

fn is_label_line(line: &str) -> bool {
    let t = line.trim();
    if !t.ends_with(':') || t.len() < 2 {
        return false;
    }
    let name = t.trim_end_matches(':');
    name.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '.')
}

fn is_equ_directive(line: &str) -> bool {
    let upper = line.to_ascii_uppercase();
    upper.contains(" EQU ")
}

fn parse_db_line(line: &str) -> Option<(String, Vec<String>)> {
    let upper = line.to_ascii_uppercase();
    let db_pos = upper.find("DB ")?;
    let left = line[..db_pos].trim();
    let right = line[db_pos + 2..].trim();
    let label = if left.is_empty() {
        "__anonymous_table".to_string()
    } else {
        left.trim_end_matches(':').to_string()
    };
    let values = right
        .trim_start_matches('B')
        .split(',')
        .map(|v| v.trim().to_string())
        .filter(|v| !v.is_empty() && !v.starts_with(';'))
        .collect::<Vec<_>>();
    if values.is_empty() {
        None
    } else {
        Some((label, values))
    }
}

fn parse_instruction(line: &str, source_line: usize) -> Option<AsmInstr> {
    let t = line.trim();
    if t.is_empty() || t.starts_with(';') {
        return None;
    }
    let mut parts = t.split_whitespace();
    let opcode = parts.next()?.to_ascii_uppercase();
    if opcode.chars().any(|c| !c.is_ascii_alphanumeric()) {
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

fn flush_data_table(tables: &mut Vec<AsmDataTable>, current: &mut Option<AsmDataTable>, end_line: usize) {
    if let Some(mut table) = current.take() {
        table.source_line_end = end_line.max(table.source_line_start);
        tables.push(table);
    }
}

fn build_translit_units(parsed: &AsmProgram) -> Vec<TranslitUnit> {
    parsed
        .blocks
        .iter()
        .map(|b| TranslitUnit {
            source_label: b.label.clone(),
            target_item: format!("fw_{}", normalize_ident(&b.label)),
            source_line_start: b.source_line_start,
            source_line_end: b.source_line_end,
        })
        .collect()
}

fn normalize_ident(src: &str) -> String {
    let mut out = String::new();
    for c in src.chars() {
        if c.is_ascii_alphanumeric() {
            out.push(c.to_ascii_lowercase());
        } else {
            out.push('_');
        }
    }
    if out.is_empty() {
        "__unnamed".to_string()
    } else {
        out
    }
}

fn render_kain_firmware(parsed: &AsmProgram, translit: &[TranslitUnit]) -> String {
    let mut out = String::new();
    out.push_str("# Generated by kain import-asm --format 6502-furby\n");
    out.push_str("# Furby firmware transliteration seed\n\n");
    out.push_str("struct CpuState:\n");
    out.push_str("    a: Int\n");
    out.push_str("    x: Int\n");
    out.push_str("    y: Int\n");
    out.push_str("    sp: Int\n");
    out.push_str("    pc: Int\n");
    out.push_str("    flags: Int\n");
    out.push_str("    bank: Int\n\n");
    out.push_str("struct Memory:\n");
    out.push_str("    ram: Array<Int>\n");
    out.push_str("    io_ports: Array<Int>\n");
    out.push_str("    timers: Array<Int>\n");
    out.push_str("    rom_tables: Array<Array<Int>>\n\n");
    out.push_str("fn read_port(port_id: Int) -> Int:\n");
    out.push_str("    return 0\n\n");
    out.push_str("fn write_port(port_id: Int, value: Int):\n");
    out.push_str("    let _port = port_id\n");
    out.push_str("    let _value = value\n\n");
    out.push_str("fn step(cpu: CpuState, mem: Memory) -> (CpuState, Memory, Int):\n");
    out.push_str("    return (cpu, mem, 0)\n\n");

    out.push_str("const FURBY_TABLES: Array<Array<Int>> = [\n");
    for table in &parsed.data_tables {
        out.push_str("    [");
        let table_values = table
            .bytes
            .iter()
            .map(|b| normalize_byte_literal(b))
            .collect::<Vec<_>>()
            .join(", ");
        out.push_str(&table_values);
        out.push_str("],\n");
    }
    out.push_str("]\n\n");

    let unit_by_label = translit
        .iter()
        .map(|u| (u.source_label.as_str(), u))
        .collect::<BTreeMap<_, _>>();

    for block in &parsed.blocks {
        let fn_name = unit_by_label
            .get(block.label.as_str())
            .map(|u| u.target_item.clone())
            .unwrap_or_else(|| format!("fw_{}", normalize_ident(&block.label)));
        out.push_str(&format!("fn {}(cpu: CpuState, mem: Memory) -> (CpuState, Memory):\n", fn_name));
        out.push_str("    let next_cpu = cpu\n");
        out.push_str("    let next_mem = mem\n");
        for instr in &block.instructions {
            let operand = instr.operand.clone().unwrap_or_default();
            out.push_str(&format!(
                "    # L{} {} {}\n",
                instr.source_line,
                instr.opcode,
                operand
            ));
        }
        out.push_str("    return (next_cpu, next_mem)\n\n");
    }

    out.push_str("@component\n");
    out.push_str("struct FurbyFirmwareComponent:\n");
    out.push_str("    cpu_a: Int\n");
    out.push_str("    cpu_x: Int\n");
    out.push_str("    cpu_y: Int\n");
    out.push_str("    bank: Int\n\n");
    out.push_str("actor FurbyActor:\n");
    out.push_str("    state tick_count: Int = 0\n");
    out.push_str("    on Tick():\n");
    out.push_str("        tick_count = tick_count + 1\n\n");
    out.push_str("@component\n");
    out.push_str("struct FurbyDebugComponent:\n");
    out.push_str("    last_trace_tick: Int\n");
    out.push_str("    last_pc: Int\n");
    out
}

fn normalize_byte_literal(raw: &str) -> String {
    let t = raw.trim().trim_end_matches(';').to_ascii_uppercase();
    if t.ends_with('H') {
        let hex = t.trim_end_matches('H');
        if hex.chars().all(|c| c.is_ascii_hexdigit()) {
            return format!("0x{}", hex);
        }
    }
    if t.chars().all(|c| c.is_ascii_digit()) {
        return t;
    }
    "0".to_string()
}

fn build_recovery_report(input: &Path, canonical: &str, parsed: &AsmProgram) -> RecoveryReport {
    let mut unresolved_tokens = Vec::new();
    let mut ambiguous_labels = Vec::new();
    let mut sections = BTreeMap::<String, (usize, usize)>::new();
    let label_set = parsed.blocks.iter().map(|b| b.label.clone()).collect::<HashSet<_>>();

    for (idx, line) in canonical.lines().enumerate() {
        let line_no = idx + 1;
        let section = if line.to_ascii_uppercase().contains("TBL") {
            "tables".to_string()
        } else if line.to_ascii_uppercase().contains("EQU") {
            "equates".to_string()
        } else {
            "code".to_string()
        };
        let entry = sections.entry(section).or_insert((0usize, 0usize));
        entry.1 += 1;

        let has_known_token = is_label_line(line)
            || is_equ_directive(line)
            || parse_db_line(line).is_some()
            || parse_instruction(line, line_no).is_some();

        if has_known_token {
            entry.0 += 1;
        } else {
            unresolved_tokens.push(RecoveryIssue {
                line: line_no,
                message: format!("Unrecognized canonical line: {}", line),
            });
        }

        if is_label_line(line) {
            let label = line.trim_end_matches(':').to_string();
            if !label_set.contains(&label) {
                ambiguous_labels.push(RecoveryIssue {
                    line: line_no,
                    message: format!("Label parsed but not emitted as executable block: {}", label),
                });
            }
        }
    }

    let section_scores = sections
        .into_iter()
        .map(|(section, (recognized, total))| RecoverySectionScore {
            section,
            recognized,
            total,
            confidence: if total == 0 {
                1.0
            } else {
                recognized as f64 / total as f64
            },
        })
        .collect::<Vec<_>>();

    RecoveryReport {
        input: input.display().to_string(),
        canonical_output: "Research/furby/furby_canonical.asm".to_string(),
        unresolved_tokens,
        ambiguous_labels,
        section_scores,
    }
}

fn default_parity_trace_schema() -> ParityTraceFrame {
    let mut registers = BTreeMap::new();
    registers.insert("a".to_string(), 0);
    registers.insert("x".to_string(), 0);
    registers.insert("y".to_string(), 0);
    registers.insert("sp".to_string(), 0);
    registers.insert("pc".to_string(), 0);
    registers.insert("bank".to_string(), 0);

    let mut flags = BTreeMap::new();
    flags.insert("carry".to_string(), false);
    flags.insert("zero".to_string(), false);
    flags.insert("negative".to_string(), false);
    flags.insert("overflow".to_string(), false);

    ParityTraceFrame {
        tick: 0,
        pc: 0,
        opcode: "NOP".to_string(),
        registers,
        flags,
        notes: vec!["schema".to_string()],
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn canonicalize_removes_page_headers_and_bom() {
        let sample = "\u{feff}----------------------- Page 1-----------------------\nLabel1:\nLDA #10\nA-123\n";
        let out = canonicalize_asm(sample);
        assert!(!out.contains("Page 1"));
        assert!(!out.contains('\u{feff}'));
        assert!(out.contains("Label1:"));
        assert!(out.contains("LDA #10"));
    }

    #[test]
    fn parser_extracts_blocks_and_tables() {
        let canonical = "Start:\nLDA #10\nSTA PortA\nTable1: DB 10,20,30\n";
        let parsed = parse_asm_program(canonical);
        assert!(!parsed.blocks.is_empty());
        assert!(!parsed.data_tables.is_empty());
        assert_eq!(parsed.data_tables[0].bytes.len(), 3);
    }

    #[test]
    fn import_asm_generates_outputs() {
        let base = std::env::temp_dir().join(format!(
            "kain_import_asm_test_{}",
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .expect("clock before epoch")
                .as_nanos()
        ));
        fs::create_dir_all(&base).expect("failed to create temp base");
        let input = base.join("furby_raw.asm");
        fs::write(
            &input,
            "Start:\nLDA #10\nSTA PortA\nTable1: DB 10,20,30\n",
        )
        .expect("failed to write input");

        let prev = std::env::current_dir().expect("cwd");
        std::env::set_current_dir(&base).expect("set cwd");
        let result = import_asm(&input, "6502-furby", None, false).expect("import should succeed");
        std::env::set_current_dir(prev).expect("restore cwd");

        assert!(result.canonical_asm_path.exists());
        assert!(result.generated_kn_path.exists());
        assert!(result.map_json_path.exists());
        assert!(result.report_json_path.exists());
    }
}
