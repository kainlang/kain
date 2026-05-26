use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AsmProgram {
    pub blocks: Vec<AsmBlock>,
    pub directives: Vec<AsmDirective>,
    pub data_tables: Vec<AsmDataTable>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AsmBlock {
    pub label: String,
    pub instructions: Vec<AsmInstr>,
    pub source_line_start: usize,
    pub source_line_end: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AsmInstr {
    pub opcode: String,
    pub operand: Option<String>,
    pub source_line: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AsmDirective {
    pub name: String,
    pub args: Vec<String>,
    pub source_line: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AsmDataTable {
    pub label: String,
    pub bytes: Vec<String>,
    pub source_line_start: usize,
    pub source_line_end: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct TranslitUnit {
    pub source_label: String,
    pub target_item: String,
    pub source_line_start: usize,
    pub source_line_end: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ParityTraceFrame {
    pub tick: u64,
    pub pc: u32,
    pub opcode: String,
    pub registers: BTreeMap<String, i64>,
    pub flags: BTreeMap<String, bool>,
    pub notes: Vec<String>,
}
