//! Build-time corpus indexer for kain-error-semantic.
//!
//! Scans `.kn` files from `symbol_corpus/`, `error_corpus/`, `stdlib/`, `smoketest/src/`, and any
//! extra roots from `KAIN_CORPUS_PATH`. Extracts lightweight symbol metadata
//! via regex, preserves lane/path ownership, and bakes a zero-cost corpus into
//! `$OUT_DIR/corpus_data.rs` for runtime typo/import queries.

use regex::Regex;
use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

#[derive(Debug, Clone)]
struct ScanRoot {
    path: PathBuf,
    source_lane: String,
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct RawSymbol {
    name: String,
    kind: String,
    module_path: String,
    source_path: String,
    source_lane: String,
    canonical_import_path: Option<String>,
    is_pub: bool,
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct RawImport {
    symbol_prefix: String,
    import_path: String,
    source_path: String,
    source_lane: String,
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
struct RawErrorCase {
    file_path: String,
    expected_code: String,
    expected_mode: String,
    expected_repair: String,
}

fn main() {
    let crate_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let repo_root = crate_dir
        .join("..")
        .join("..")
        .canonicalize()
        .unwrap_or_else(|_| crate_dir.join("..").join(".."));
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR"));

    let mut scan_roots = vec![
        ScanRoot {
            path: crate_dir.join("symbol_corpus"),
            source_lane: "symbol_corpus".to_string(),
        },
        ScanRoot {
            path: crate_dir.join("error_corpus"),
            source_lane: "error_corpus".to_string(),
        },
        ScanRoot {
            path: repo_root.join("stdlib"),
            source_lane: "stdlib".to_string(),
        },
        ScanRoot {
            path: repo_root.join("smoketest").join("src"),
            source_lane: "smoketest".to_string(),
        },
    ];

    if let Some(extra_roots) = env::var_os("KAIN_CORPUS_PATH") {
        for path in env::split_paths(&extra_roots) {
            if path.as_os_str().is_empty() {
                continue;
            }
            scan_roots.push(ScanRoot {
                path,
                source_lane: "external".to_string(),
            });
        }
    }

    for root in &scan_roots {
        println!("cargo:rerun-if-changed={}", root.path.display());
    }
    println!("cargo:rerun-if-env-changed=KAIN_CORPUS_PATH");

    let re_pub_fn = Regex::new(r"(?m)^[ \t]*pub\s+fn\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_pub_struct = Regex::new(r"(?m)^[ \t]*pub\s+struct\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_pub_enum = Regex::new(r"(?m)^[ \t]*pub\s+enum\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_fn = Regex::new(r"(?m)^[ \t]*fn\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_struct = Regex::new(r"(?m)^[ \t]*struct\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_actor = Regex::new(r"(?m)^[ \t]*actor\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_world = Regex::new(r"(?m)^[ \t]*world\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_trait = Regex::new(r"(?m)^[ \t]*trait\s+([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_use_std = Regex::new(r"(?m)^[ \t]*use\s+std::([A-Za-z_][A-Za-z0-9_]*)").unwrap();
    let re_exp_code = Regex::new(r"(?m)^//\s*@expected_code:\s*([A-Za-z0-9-]+)").unwrap();
    let re_exp_mode = Regex::new(r"(?m)^//\s*@expected_mode:\s*([A-Za-z0-9_]+)").unwrap();
    let re_exp_repair = Regex::new(r"(?m)^//\s*@expected_repair:\s*([A-Za-z0-9_]+)").unwrap();

    let mut symbols: BTreeSet<RawSymbol> = BTreeSet::new();
    let mut imports: BTreeSet<RawImport> = BTreeSet::new();
    let mut error_cases: BTreeSet<RawErrorCase> = BTreeSet::new();
    let mut file_count: usize = 0;

    for root in &scan_roots {
        if !root.path.exists() {
            continue;
        }
        for entry in WalkDir::new(&root.path)
            .follow_links(true)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if !path.is_file() {
                continue;
            }
            if path.extension().and_then(|e| e.to_str()) != Some("kn") {
                continue;
            }

            let src = match fs::read_to_string(path) {
                Ok(s) => s,
                Err(_) => continue,
            };

            let rel_parts = relative_kn_parts(&root.path, path);
            if rel_parts.is_empty() {
                continue;
            }
            let module_path = module_path_for(&root.source_lane, &rel_parts);
            let source_path = source_path_for(&repo_root, path);
            let canonical_import_path = canonical_import_path_for(&root.source_lane, &rel_parts);

            if let Some(import_path) = canonical_import_path.as_deref() {
                if let Some(symbol_prefix) = rel_parts.last() {
                    imports.insert(RawImport {
                        symbol_prefix: symbol_prefix.clone(),
                        import_path: import_path.to_string(),
                        source_path: source_path.clone(),
                        source_lane: root.source_lane.clone(),
                    });
                }
            }

            for cap in re_use_std.captures_iter(&src) {
                let module_name = cap[1].to_string();
                imports.insert(RawImport {
                    symbol_prefix: module_name.clone(),
                    import_path: format!("std::{}", module_name),
                    source_path: source_path.clone(),
                    source_lane: root.source_lane.clone(),
                });
            }

            if root.source_lane == "error_corpus" {
                let exp_code = re_exp_code.captures(&src).map(|c| c[1].to_string()).unwrap_or_default();
                let exp_mode = re_exp_mode.captures(&src).map(|c| c[1].to_string()).unwrap_or_default();
                let exp_repair = re_exp_repair.captures(&src).map(|c| c[1].to_string()).unwrap_or_default();
                if !exp_code.is_empty() {
                    error_cases.insert(RawErrorCase {
                        file_path: source_path.clone(),
                        expected_code: exp_code,
                        expected_mode: exp_mode,
                        expected_repair: exp_repair,
                    });
                }
            }

            file_count += 1;

            collect_symbol_matches(
                &mut symbols,
                &re_pub_fn,
                &src,
                "fn",
                true,
                &module_path,
                &source_path,
                &root.source_lane,
                canonical_import_path.as_deref(),
            );
            collect_symbol_matches(
                &mut symbols,
                &re_pub_struct,
                &src,
                "struct",
                true,
                &module_path,
                &source_path,
                &root.source_lane,
                canonical_import_path.as_deref(),
            );
            collect_symbol_matches(
                &mut symbols,
                &re_pub_enum,
                &src,
                "enum",
                true,
                &module_path,
                &source_path,
                &root.source_lane,
                canonical_import_path.as_deref(),
            );
            collect_symbol_matches(
                &mut symbols,
                &re_fn,
                &src,
                "fn",
                false,
                &module_path,
                &source_path,
                &root.source_lane,
                None,
            );
            collect_symbol_matches(
                &mut symbols,
                &re_struct,
                &src,
                "struct",
                false,
                &module_path,
                &source_path,
                &root.source_lane,
                None,
            );
            collect_symbol_matches(
                &mut symbols,
                &re_actor,
                &src,
                "actor",
                false,
                &module_path,
                &source_path,
                &root.source_lane,
                None,
            );
            collect_symbol_matches(
                &mut symbols,
                &re_world,
                &src,
                "world",
                false,
                &module_path,
                &source_path,
                &root.source_lane,
                None,
            );
            collect_symbol_matches(
                &mut symbols,
                &re_trait,
                &src,
                "trait",
                false,
                &module_path,
                &source_path,
                &root.source_lane,
                None,
            );
        }
    }

    let out_path = out_dir.join("corpus_data.rs");
    let mut f = fs::File::create(&out_path).expect("failed to create corpus_data.rs");

    writeln!(
        f,
        "// Auto-generated by kain-error-semantic/build.rs — do not edit"
    )
    .unwrap();
    writeln!(f).unwrap();
    writeln!(f, "pub struct CorpusEntry {{").unwrap();
    writeln!(f, "    pub name: &'static str,").unwrap();
    writeln!(f, "    pub kind: &'static str,").unwrap();
    writeln!(f, "    pub module_path: &'static str,").unwrap();
    writeln!(f, "    pub source_path: &'static str,").unwrap();
    writeln!(f, "    pub source_lane: &'static str,").unwrap();
    writeln!(f, "    pub canonical_import_path: Option<&'static str>,").unwrap();
    writeln!(f, "    pub is_pub: bool,").unwrap();
    writeln!(f, "}}").unwrap();
    writeln!(f).unwrap();
    writeln!(f, "pub struct CorpusImport {{").unwrap();
    writeln!(f, "    pub symbol_prefix: &'static str,").unwrap();
    writeln!(f, "    pub import_path: &'static str,").unwrap();
    writeln!(f, "    pub source_path: &'static str,").unwrap();
    writeln!(f, "    pub source_lane: &'static str,").unwrap();
    writeln!(f, "}}").unwrap();
    writeln!(f).unwrap();

    let sym_vec: Vec<&RawSymbol> = symbols.iter().collect();
    writeln!(f, "pub static CORPUS_SYMBOLS: &[CorpusEntry] = &[").unwrap();
    for s in &sym_vec {
        writeln!(
            f,
            "    CorpusEntry {{ name: {:?}, kind: {:?}, module_path: {:?}, source_path: {:?}, source_lane: {:?}, canonical_import_path: {}, is_pub: {} }},",
            s.name,
            s.kind,
            s.module_path,
            s.source_path,
            s.source_lane,
            match &s.canonical_import_path {
                Some(path) => format!("Some({:?})", path),
                None => "None".to_string(),
            },
            s.is_pub,
        )
        .unwrap();
    }
    writeln!(f, "];").unwrap();
    writeln!(f).unwrap();

    let imp_vec: Vec<&RawImport> = imports.iter().collect();
    writeln!(f, "pub static CORPUS_IMPORTS: &[CorpusImport] = &[").unwrap();
    for i in &imp_vec {
        writeln!(
            f,
            "    CorpusImport {{ symbol_prefix: {:?}, import_path: {:?}, source_path: {:?}, source_lane: {:?} }},",
            i.symbol_prefix,
            i.import_path,
            i.source_path,
            i.source_lane,
        )
        .unwrap();
    }
    writeln!(f, "];").unwrap();
    writeln!(f).unwrap();

    writeln!(f, "pub struct ErrorCorpusCase {{").unwrap();
    writeln!(f, "    pub file_path: &'static str,").unwrap();
    writeln!(f, "    pub expected_code: &'static str,").unwrap();
    writeln!(f, "    pub expected_mode: &'static str,").unwrap();
    writeln!(f, "    pub expected_repair: &'static str,").unwrap();
    writeln!(f, "}}").unwrap();
    writeln!(f).unwrap();

    let err_vec: Vec<&RawErrorCase> = error_cases.iter().collect();
    writeln!(f, "pub static ERROR_CORPUS_CASES: &[ErrorCorpusCase] = &[").unwrap();
    for c in &err_vec {
        writeln!(
            f,
            "    ErrorCorpusCase {{ file_path: {:?}, expected_code: {:?}, expected_mode: {:?}, expected_repair: {:?} }},",
            c.file_path,
            c.expected_code,
            c.expected_mode,
            c.expected_repair,
        )
        .unwrap();
    }
    writeln!(f, "];").unwrap();
    writeln!(f).unwrap();

    writeln!(
        f,
        "pub const CORPUS_SYMBOL_COUNT: usize = {};",
        sym_vec.len()
    )
    .unwrap();
    writeln!(
        f,
        "pub const CORPUS_IMPORT_COUNT: usize = {};",
        imp_vec.len()
    )
    .unwrap();

    eprintln!(
        "Corpus indexed: {} symbols, {} imports from {} files",
        sym_vec.len(),
        imp_vec.len(),
        file_count
    );
}

fn collect_symbol_matches(
    symbols: &mut BTreeSet<RawSymbol>,
    regex: &Regex,
    src: &str,
    kind: &str,
    is_pub: bool,
    module_path: &str,
    source_path: &str,
    source_lane: &str,
    canonical_import_path: Option<&str>,
) {
    for cap in regex.captures_iter(src) {
        let name = cap[1].to_string();
        if !is_pub {
            let pub_exists = symbols.contains(&RawSymbol {
                name: name.clone(),
                kind: kind.to_string(),
                module_path: module_path.to_string(),
                source_path: source_path.to_string(),
                source_lane: source_lane.to_string(),
                canonical_import_path: None,
                is_pub: true,
            });
            if pub_exists {
                continue;
            }
        }
        symbols.insert(RawSymbol {
            name,
            kind: kind.to_string(),
            module_path: module_path.to_string(),
            source_path: source_path.to_string(),
            source_lane: source_lane.to_string(),
            canonical_import_path: canonical_import_path.map(|value| value.to_string()),
            is_pub,
        });
    }
}

fn relative_kn_parts(root: &Path, path: &Path) -> Vec<String> {
    let Ok(rel) = path.strip_prefix(root) else {
        return Vec::new();
    };
    let mut parts = Vec::new();
    for component in rel.components() {
        let text = component.as_os_str().to_string_lossy();
        if text.is_empty() {
            continue;
        }
        parts.push(text.into_owned());
    }
    if let Some(last) = parts.last_mut() {
        if last.ends_with(".kn") {
            last.truncate(last.len() - 3);
        }
    }
    parts.retain(|part| !part.is_empty());
    parts
}

fn module_path_for(source_lane: &str, rel_parts: &[String]) -> String {
    if rel_parts.is_empty() {
        return source_lane.to_string();
    }
    format!("{}.{}", source_lane, rel_parts.join("."))
}

fn source_path_for(repo_root: &Path, path: &Path) -> String {
    path.strip_prefix(repo_root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

fn canonical_import_path_for(source_lane: &str, rel_parts: &[String]) -> Option<String> {
    if source_lane == "stdlib" && rel_parts.len() == 1 {
        return Some(format!("std::{}", rel_parts[0]));
    }
    None
}
