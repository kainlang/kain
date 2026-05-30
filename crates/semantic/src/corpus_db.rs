//! Corpus database — zero-allocation query layer over build-time baked symbol data.
//!
//! The build-time indexer scans `.kn` files from symbol_corpus, error_corpus,
//! stdlib, smoketest, and any paths in `KAIN_CORPUS_PATH`, then codegens
//! static arrays that this module queries at near-zero cost.

use strsim::jaro_winkler;

include!(concat!(env!("OUT_DIR"), "/corpus_data.rs"));

/// A scored symbol match from the corpus.
#[derive(Debug, Clone)]
pub struct CorpusMatch {
    pub name: &'static str,
    pub kind: &'static str,
    pub module_path: &'static str,
    pub source_path: &'static str,
    pub source_lane: &'static str,
    pub canonical_import_path: Option<&'static str>,
    pub is_pub: bool,
    pub similarity: f64,
}

/// Find the N nearest symbols to a given typo string.
/// Returns matches sorted by descending similarity score.
pub fn find_nearest_symbols(typo: &str, max_results: usize) -> Vec<CorpusMatch> {
    let typo_lower = typo.to_lowercase();
    let mut scored: Vec<CorpusMatch> = CORPUS_SYMBOLS
        .iter()
        .map(|entry| {
            let sim = jaro_winkler(&typo_lower, &entry.name.to_lowercase());
            CorpusMatch {
                name: entry.name,
                kind: entry.kind,
                module_path: entry.module_path,
                source_path: entry.source_path,
                source_lane: entry.source_lane,
                canonical_import_path: entry.canonical_import_path,
                is_pub: entry.is_pub,
                similarity: sim,
            }
        })
        .filter(|m| m.similarity > 0.75)
        .collect();

    scored.sort_by(|a, b| {
        b.similarity
            .partial_cmp(&a.similarity)
            .unwrap_or(std::cmp::Ordering::Equal)
            .then_with(|| b.is_pub.cmp(&a.is_pub))
            .then_with(|| a.source_lane.cmp(b.source_lane))
            .then_with(|| a.source_path.cmp(b.source_path))
    });
    scored.truncate(max_results);
    scored
}

/// Suggest a canonical import path for a missing symbol when corpus ownership
/// makes that import unambiguous.
pub fn suggest_import_for_symbol(symbol: &str) -> Option<&'static str> {
    let sym_lower = symbol.to_lowercase();

    for entry in CORPUS_SYMBOLS.iter() {
        if entry.is_pub && entry.name.eq_ignore_ascii_case(symbol) {
            if let Some(import_path) = entry.canonical_import_path {
                return Some(import_path);
            }
        }
    }

    for import in CORPUS_IMPORTS.iter() {
        if sym_lower.starts_with(&format!("{}_", import.symbol_prefix)) {
            return Some(import.import_path);
        }
    }

    None
}

/// Find the exact golden corpus case for a code + source-window key.
///
/// This is the offline oracle hook: build.rs records the original source
/// window and derived primary text for each annotated error case, and the
/// expert engine can use the exact match to recover the corpus-authoritative
/// repair and explanation shape without any CUDA runtime.
pub fn find_error_corpus_case(
    code: &str,
    source_window: &str,
    primary_text: &str,
) -> Option<&'static ErrorCorpusCase> {
    ERROR_CORPUS_CASES
        .iter()
        .find(|case| case.expected_code == code && case.source_window == source_window)
        .or_else(|| {
            ERROR_CORPUS_CASES
                .iter()
                .find(|case| case.expected_code == code && case.primary_text == primary_text)
        })
}

/// Get corpus statistics.
pub fn corpus_stats() -> (usize, usize) {
    (CORPUS_SYMBOL_COUNT, CORPUS_IMPORT_COUNT)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn suggest_import_uses_stdlib_root_ownership() {
        let import = suggest_import_for_symbol("fs_read_text");
        assert_eq!(import, Some("std::fs"));
    }

    #[test]
    fn nearest_matches_carry_source_identity() {
        let matches = find_nearest_symbols("prntln", 3);
        if let Some(first) = matches.first() {
            assert!(!first.source_lane.is_empty());
            assert!(!first.source_path.is_empty());
            assert!(!first.module_path.is_empty());
        }
    }
}
