//! Global diagnostic registry.
//!
//! Maps `DiagnosticCode` → `DiagnosticSpec`. Initialized once from the
//! build.rs-generated data (or from runtime TOML loading in the future).
//!
//! Thread-safe, lazy-initialized via `once_cell::sync::Lazy`.

use crate::code::DiagnosticCode;
use crate::spec::DiagnosticSpec;
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::sync::RwLock;

/// Include the build.rs-generated static data.
mod generated {
    use crate::spec::GeneratedSpec;

    include!(concat!(env!("OUT_DIR"), "/registry_data.rs"));
}

/// The global diagnostic registry. Initialized on first access.
static REGISTRY: Lazy<RwLock<DiagnosticRegistry>> =
    Lazy::new(|| RwLock::new(DiagnosticRegistry::new()));

/// Holds the code → spec mapping and provides lookup/explain methods.
pub struct DiagnosticRegistry {
    specs: HashMap<DiagnosticCode, DiagnosticSpec>,
    /// All codes sorted for enumeration.
    codes: Vec<DiagnosticCode>,
}

impl DiagnosticRegistry {
    fn new() -> Self {
        let entries = generated::load_registry_entries();
        let mut specs: HashMap<DiagnosticCode, DiagnosticSpec> =
            HashMap::with_capacity(entries.len());
        let mut codes: Vec<DiagnosticCode> = Vec::with_capacity(entries.len());

        for spec in entries {
            codes.push(spec.code);
            specs.insert(spec.code, spec);
        }

        // Sort codes for stable iteration
        codes.sort_by_key(|c| c.as_str());

        Self { specs, codes }
    }

    /// Number of registered diagnostic codes.
    pub fn len(&self) -> usize {
        self.specs.len()
    }

    pub fn is_empty(&self) -> bool {
        self.specs.is_empty()
    }

    /// Look up a spec by code. Returns `None` for unknown codes.
    pub fn get(&self, code: DiagnosticCode) -> Option<&DiagnosticSpec> {
        self.specs.get(&code)
    }

    /// Iterate over all registered specs.
    pub fn all_specs(&self) -> impl Iterator<Item = &DiagnosticSpec> {
        self.codes.iter().filter_map(|c| self.specs.get(c))
    }

    /// Search for codes whose title or help text contains `query`
    /// (case-insensitive).
    pub fn search(&self, query: &str) -> Vec<&DiagnosticSpec> {
        let query = query.to_lowercase();
        self.specs
            .values()
            .filter(|spec| {
                spec.title.to_lowercase().contains(&query)
                    || spec.help.to_lowercase().contains(&query)
                    || spec.code.as_str().to_lowercase().contains(&query)
            })
            .collect()
    }

    /// List all codes in a given category prefix (e.g. `"PARSE"`).
    pub fn list_category(&self, prefix: &str) -> Vec<&DiagnosticSpec> {
        let prefix = prefix.to_uppercase();
        self.specs
            .values()
            .filter(|spec| spec.code.category_prefix().to_uppercase() == prefix)
            .collect()
    }
}

// ── Global accessor functions ─────────────────────────────────────────

/// Get a reference to the global registry (lazy-init).
pub fn registry() -> std::sync::RwLockReadGuard<'static, DiagnosticRegistry> {
    REGISTRY.read().expect("diagnostic registry poisoned")
}

/// Look up a diagnostic spec by code.
pub fn spec_for_code(code: DiagnosticCode) -> Option<&'static DiagnosticSpec> {
    // SAFETY: we leak the reference into a static lifetime. The registry
    // is initialized once and never deallocated.
    let guard = REGISTRY.read().expect("diagnostic registry poisoned");
    guard
        .get(code)
        .map(|s| unsafe { std::mem::transmute::<&DiagnosticSpec, &'static DiagnosticSpec>(s) })
}

/// Look up a diagnostic spec, panicking if the code is not registered.
pub fn expect_spec(code: DiagnosticCode) -> &'static DiagnosticSpec {
    spec_for_code(code).unwrap_or_else(|| {
        panic!(
            "Diagnostic code {} not found in registry ({} codes loaded). \
             Check that specs/*.toml contains this code.",
            code,
            registry().len()
        )
    })
}

/// Generate a full explanation for a diagnostic code (for `kain explain`).
pub fn explain_code(code: DiagnosticCode) -> String {
    match spec_for_code(code) {
        Some(spec) => spec.full_explanation(),
        None => format!(
            "Unknown diagnostic code: {code}\n\
             Use `kain explain --list` to see all registered codes.\n\
             ({n} codes available)",
            n = registry().len()
        ),
    }
}

pub fn spec_for_code_str(code: &str) -> Option<&'static DiagnosticSpec> {
    let guard = registry();
    let matched_code = guard
        .all_specs()
        .find(|spec| spec.code.as_str() == code)
        .map(|spec| spec.code);
    drop(guard);
    matched_code.and_then(spec_for_code)
}

/// Generate a compact listing of all registered codes.
pub fn list_all_codes() -> String {
    let guard = registry();
    let mut out = String::new();
    out.push_str(&format!("Registered diagnostic codes: {}\n\n", guard.len()));

    let mut current_category = String::new();
    for spec in guard.all_specs() {
        let cat = spec.code.category_prefix().to_string();
        if cat != current_category {
            current_category = cat;
            out.push_str(&format!("\n── {current_category} ──\n"));
        }
        out.push_str(&format!(
            "  {:20}  {:6}  {}\n",
            spec.code.as_str(),
            spec.severity.to_string(),
            spec.title
        ));
    }
    out
}
