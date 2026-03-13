//! Shared semantic schema used by importer strict modes and future generators.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KainLanguageSchema {
    pub schema_version: u32,
    pub item_kinds: Vec<String>,
    pub expr_kinds: Vec<String>,
    pub type_kinds: Vec<String>,
    pub pattern_kinds: Vec<String>,
    pub effect_kinds: Vec<String>,
    pub reserved_identifiers: Vec<String>,
}

impl KainLanguageSchema {
    pub fn bootstrap_core_schema() -> Self {
        Self {
            schema_version: 1,
            item_kinds: vec![
                "Function",
                "Struct",
                "Enum",
                "Impl",
                "Const",
                "TypeAlias",
                "Mod",
                "Component",
                "Actor",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            expr_kinds: vec![
                "Ident",
                "Int",
                "Float",
                "String",
                "Bool",
                "None",
                "Binary",
                "Unary",
                "Call",
                "MethodCall",
                "Field",
                "Index",
                "Assign",
                "Array",
                "Tuple",
                "Struct",
                "Lambda",
                "If",
                "Match",
                "Range",
                "Ref",
                "Await",
                "Try",
                "Cast",
                "Block",
                "MacroCall",
                "JSX",
                "Return",
                "Break",
                "Continue",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            type_kinds: vec![
                "Named", "Ref", "Ptr", "Array", "Slice", "Tuple", "Function", "Option", "Result",
                "Impl", "Infer", "Never", "Unit",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            pattern_kinds: vec![
                "Binding", "Wildcard", "Literal", "Tuple", "Variant", "Range", "Or", "Slice",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            effect_kinds: vec![
                "Pure", "IO", "Async", "GPU", "Reactive", "Unsafe", "Alloc", "Panic",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            reserved_identifiers: vec![
                "self",
                "Self",
                "fn",
                "struct",
                "enum",
                "impl",
                "mod",
                "match",
                "if",
                "else",
                "for",
                "while",
                "loop",
                "return",
                "break",
                "continue",
                "component",
                "actor",
                "render",
                "state",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
        }
    }
}
