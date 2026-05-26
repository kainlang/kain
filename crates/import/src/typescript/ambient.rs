//! Embedded TypeScript ambient declarations used by the importer.
//!
//! The data in this module is generated from `reference/TypeScript-main/src/lib`
//! by `tools/typescript_import/extract_ambient_manifest.py`. Kain-specific
//! lowering helpers and collision aliases live in the companion override JSON so
//! the importer policy stays inspectable as data instead of scattered string
//! checks.

use serde::Deserialize;
use std::collections::{HashMap, HashSet};
use std::sync::OnceLock;

const TYPESCRIPT_AMBIENT_MANIFEST_JSON: &str =
    include_str!("data/typescript_ambient_manifest.json");

#[derive(Debug, Deserialize)]
pub struct TypeScriptAmbientManifest {
    pub schema_version: u32,
    pub source: TypeScriptAmbientSource,
    pub value_aliases: Vec<TypeScriptAmbientAlias>,
    pub suppressed_type_names: Vec<String>,
    pub type_names_lowered_to_any: Vec<String>,
    pub value_symbols: Vec<TypeScriptAmbientSymbol>,
    pub type_symbols: Vec<TypeScriptAmbientSymbol>,
}

#[derive(Debug, Deserialize)]
pub struct TypeScriptAmbientSource {
    pub typescript_lib_dir: String,
    pub override_file: String,
    pub lib_file_count: usize,
    pub lib_files: Vec<String>,
}

#[derive(Debug, Deserialize)]
pub struct TypeScriptAmbientAlias {
    pub ts_name: String,
    pub kain_name: String,
    pub reason: String,
}

#[derive(Debug, Deserialize)]
pub struct TypeScriptAmbientSymbol {
    pub ts_name: String,
    pub kain_name: String,
    pub kind: String,
    pub source_files: Vec<String>,
    pub declaration_count: usize,
    pub reason: Option<String>,
}

#[derive(Debug)]
struct TypeScriptAmbientIndex {
    manifest: TypeScriptAmbientManifest,
    value_name_by_ts_name: HashMap<String, String>,
    lowered_to_any_type_names: HashSet<String>,
}

static TYPESCRIPT_AMBIENT_INDEX: OnceLock<TypeScriptAmbientIndex> = OnceLock::new();

pub fn typescript_ambient_manifest() -> &'static TypeScriptAmbientManifest {
    &typescript_ambient_index().manifest
}

pub fn typescript_ambient_value_name(ts_name: &str) -> Option<&'static str> {
    typescript_ambient_index()
        .value_name_by_ts_name
        .get(ts_name)
        .map(String::as_str)
}

pub fn typescript_type_name_lowers_to_any(name: &str) -> bool {
    typescript_ambient_index()
        .lowered_to_any_type_names
        .contains(name)
}

fn typescript_ambient_index() -> &'static TypeScriptAmbientIndex {
    TYPESCRIPT_AMBIENT_INDEX.get_or_init(|| {
        let manifest: TypeScriptAmbientManifest =
            serde_json::from_str(TYPESCRIPT_AMBIENT_MANIFEST_JSON)
                .expect("embedded TypeScript ambient manifest must be valid JSON");
        assert_eq!(
            manifest.schema_version, 1,
            "unsupported TypeScript ambient manifest schema"
        );

        let value_name_by_ts_name = manifest
            .value_symbols
            .iter()
            .map(|symbol| (symbol.ts_name.clone(), symbol.kain_name.clone()))
            .collect();
        let lowered_to_any_type_names =
            manifest.type_names_lowered_to_any.iter().cloned().collect();

        TypeScriptAmbientIndex {
            manifest,
            value_name_by_ts_name,
            lowered_to_any_type_names,
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn manifest_contains_typescript_lib_ambient_values() {
        let manifest = typescript_ambient_manifest();
        assert!(manifest.source.lib_file_count >= 100);
        for name in [
            "console",
            "document",
            "Proxy",
            "Uint8ClampedArray",
            "window",
        ] {
            assert!(
                manifest
                    .value_symbols
                    .iter()
                    .any(|symbol| symbol.ts_name == name),
                "missing ambient value {name}"
            );
        }
    }

    #[test]
    fn manifest_contains_typescript_lib_ambient_types() {
        let manifest = typescript_ambient_manifest();
        for name in ["HTMLElement", "ImportMeta", "Proxy", "ReactNode"] {
            assert!(
                manifest
                    .type_symbols
                    .iter()
                    .any(|symbol| symbol.ts_name == name),
                "missing ambient type {name}"
            );
        }
    }

    #[test]
    fn value_alias_policy_is_data_driven() {
        assert_eq!(typescript_ambient_value_name("Array"), Some("ts_Array"));
        assert_eq!(typescript_ambient_value_name("Promise"), Some("ts_Promise"));
        assert_eq!(typescript_ambient_value_name("console"), Some("console"));
    }

    #[test]
    fn utility_type_lowering_policy_is_data_driven() {
        assert!(typescript_type_name_lowers_to_any("Partial"));
        assert!(typescript_type_name_lowers_to_any("ReturnType"));
        assert!(!typescript_type_name_lowers_to_any("HTMLElement"));
    }
}
