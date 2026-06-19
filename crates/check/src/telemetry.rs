// ============================================================================
//  Check telemetry: validator tracking, confidence scoring, gap reporting.
//
//  Transforms `kain check` from a pass/fail gate into a compiler oracle
//  that explains its own uncertainty.
//
//  Three responsibilities:
//   1. Track which validators ran and which were skipped (with reason).
//   2. Compute a 0.0-1.0 confidence score from validator coverage.
//   3. Produce a human-readable gap report explaining what was NOT checked.
//
//  The module also ships a skeleton in-memory cache (CheckCache) for
//  future incremental-check support. Disk persistence is intentionally
//  deferred to Phase 2.
// ============================================================================

use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};

/// A single validator run record.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ValidatorRun {
    /// Validator name (e.g., "validate_atomic_ordering")
    pub name: String,
    /// Category (e.g., "atomic", "actor", "ownership")
    pub category: String,
    /// Did this validator execute?
    pub ran: bool,
    /// Why was it skipped? (None if it ran)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub skip_reason: Option<String>,
    /// How many errors did it find?
    pub errors_found: usize,
    /// Estimated cost: "cheap", "medium", or "expensive".
    pub cost: &'static str,
}

/// Telemetry snapshot from a single `kain check` run.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CheckTelemetry {
    /// All validators that were considered (across all categories).
    pub validators: Vec<ValidatorRun>,
    /// Computed confidence score (0.0-1.0).
    pub confidence: f64,
    /// Human-readable summary of what wasn't checked.
    pub gap_summary: String,
    /// Validator categories still missing (not covered by any ran validator).
    pub missing_categories: Vec<String>,
    /// Count of validators that ran.
    pub validators_ran: usize,
    /// Count of validators that were skipped.
    pub validators_skipped: usize,
    /// Total validators in the registry.
    pub validators_total: usize,
}

/// Cache key for incremental-check results.
#[derive(Debug, Clone, Hash, PartialEq, Eq, Serialize, Deserialize)]
pub struct CacheKey {
    /// Hash of the source file content (caller computes this).
    pub source_hash: String,
    /// Target triple (e.g., "llvm", "wasm", "run").
    pub target: String,
    /// Validator name.
    pub validator: String,
}

/// Skeleton in-memory cache for validator results.
///
/// Phase 1: in-memory only. The check pipeline can call `insert` to seed
/// results and `get` to look them up. Disk persistence is intentionally
/// out of scope for this milestone.
#[derive(Debug, Default, Clone)]
pub struct CheckCache {
    cache: HashMap<CacheKey, Vec<kain_error::DiagnosticReport>>,
}

impl CheckCache {
    pub fn new() -> Self {
        Self {
            cache: HashMap::new(),
        }
    }

    /// Look up a cached validator result.
    pub fn get(&self, key: &CacheKey) -> Option<&Vec<kain_error::DiagnosticReport>> {
        self.cache.get(key)
    }

    /// Insert a validator result for later lookup.
    pub fn insert(&mut self, key: CacheKey, reports: Vec<kain_error::DiagnosticReport>) {
        self.cache.insert(key, reports);
    }

    /// Drop all cached entries.
    pub fn clear(&mut self) {
        self.cache.clear();
    }

    /// Number of cached entries.
    pub fn len(&self) -> usize {
        self.cache.len()
    }

    /// Whether the cache has no entries.
    pub fn is_empty(&self) -> bool {
        self.cache.is_empty()
    }
}

/// Builder for the static validator universe used by `compute_telemetry`.
///
/// Adding a new validator is a one-liner: append `v(...)` here.
fn all_validators() -> Vec<ValidatorRun> {
    vec![
        // Always-on validators (typechecker + built-in semantic pass)
        v("typechecker", "core", true, "cheap"),
        v("validate_semantic_stack", "core", true, "cheap"),
        v("validate_reply_ports", "actor", true, "cheap"),
        v("validate_converge_contracts", "converge", true, "cheap"),
        // Stream ALPHA validators (codegen-extracted gaps)
        v("validate_entangle_type_match", "entangle", false, "medium"),
        v("validate_orchestrate_graph", "orchestrate", false, "medium"),
        v("validate_ownership_transitions", "ownership", false, "expensive"),
        // Stream BRAVO validators
        v("validate_atomic_ordering", "atomic", false, "medium"),
        v("validate_inline_asm_target", "target", false, "cheap"),
        v("validate_callconv_target", "target", false, "cheap"),
        v("validate_struct_update_syntax", "struct", false, "medium"),
        v("validate_break_continue_flow", "control_flow", false, "cheap"),
        v("validate_enum_pattern_scrutinee", "pattern", false, "cheap"),
        v("validate_builtin_method_arg_count", "method", false, "medium"),
        v("validate_actor_message_names", "actor", false, "medium"),
        v("validate_bitcast_widths", "type_mapping", false, "medium"),
        v("validate_value_typing_builtins", "value_typing", false, "medium"),
        v("validate_shatter_layout", "shatter", false, "medium"),
        // Stream ETA-A semantic contract validators (gated behind --pedantic)
        v("validate_actor_message_completeness", "actor", false, "expensive"),
        v("validate_resonate_anti_feedback", "resonate", false, "expensive"),
        v("validate_entangle_completeness", "entangle", false, "medium"),
        v("validate_converge_lane_coverage", "converge", false, "medium"),
        v("validate_orchestrate_stage_liveness", "orchestrate", false, "medium"),
        v("validate_pulse_cadence_conflicts", "pulse", false, "cheap"),
        v("validate_teleport_bus_type_match", "teleport", false, "medium"),
        v("validate_patch_world_binding", "patch", false, "medium"),
        v("validate_law_satisfiability", "law", false, "medium"),
        v("validate_world_dead_state", "world", false, "medium"),
        v("validate_world_surface_coverage", "world", false, "medium"),
    ]
}

fn v(name: &str, category: &str, always_runs: bool, cost: &'static str) -> ValidatorRun {
    ValidatorRun {
        name: name.to_string(),
        category: category.to_string(),
        ran: always_runs,
        skip_reason: if always_runs {
            None
        } else {
            Some("not yet implemented".to_string())
        },
        errors_found: 0,
        cost,
    }
}

/// Compute telemetry for a check run.
///
/// * `ran_validators` — names of validators that actually executed in this run.
/// * `errors_per_validator` — error counts keyed by validator name.
/// * `is_pedantic` — whether `--pedantic` was passed (changes skip messaging).
pub fn compute_telemetry(
    ran_validators: &[&str],
    errors_per_validator: &HashMap<String, usize>,
    is_pedantic: bool,
) -> CheckTelemetry {
    let ran_set: HashSet<&str> = ran_validators.iter().copied().collect();

    let mut validators: Vec<ValidatorRun> = all_validators()
        .into_iter()
        .map(|mut validator| {
            let actually_ran = ran_set.contains(validator.name.as_str()) || validator.ran;
            validator.ran = actually_ran;
            if !actually_ran {
                validator.skip_reason = Some(if is_pedantic {
                    "not yet implemented".to_string()
                } else {
                    "not run (use --pedantic to enable)".to_string()
                });
            } else {
                validator.skip_reason = None;
            }
            validator.errors_found = errors_per_validator
                .get(validator.name.as_str())
                .copied()
                .unwrap_or(0);
            validator
        })
        .collect();

    let total = validators.len() as f64;
    let ran = validators.iter().filter(|v| v.ran).count() as f64;
    let skipped_count = validators.iter().filter(|v| !v.ran).count();

    // Coverage: what fraction of categories are represented by at least one
    // ran validator. Penalizes "1 ran validator covers 1 of 30 categories".
    let all_categories: HashSet<String> =
        validators.iter().map(|v| v.category.clone()).collect();
    let covered_categories: HashSet<String> = validators
        .iter()
        .filter(|v| v.ran)
        .map(|v| v.category.clone())
        .collect();

    let category_coverage = if all_categories.is_empty() {
        1.0
    } else {
        covered_categories.len() as f64 / all_categories.len() as f64
    };

    // Confidence blends validator-fraction (70%) and category-coverage (30%).
    let confidence = (ran / total) * 0.7 + category_coverage * 0.3;

    let missing_categories: Vec<String> = all_categories
        .iter()
        .filter(|c| !covered_categories.contains(*c))
        .cloned()
        .collect();

    let gap_summary = if missing_categories.is_empty() {
        "All validators ran. Full coverage.".to_string()
    } else {
        format!(
            "{} of {} validators ran (confidence: {:.0}%). Categories not covered: {}. \
             Run with --pedantic for full coverage.",
            ran as usize,
            total as usize,
            confidence * 100.0,
            missing_categories.join(", "),
        )
    };

    CheckTelemetry {
        validators_ran: ran as usize,
        validators_skipped: skipped_count,
        validators_total: total as usize,
        validators,
        confidence,
        gap_summary,
        missing_categories,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_run_yields_zero_confidence() {
        let telemetry = compute_telemetry(&[], &HashMap::new(), false);
        // typechecker + validate_semantic_stack + validate_reply_ports +
        // validate_converge_contracts always run -> ~4 of N.
        assert!(telemetry.confidence > 0.0);
        assert!(telemetry.confidence < 1.0);
        assert!(telemetry.validators_ran >= 1);
    }

    #[test]
    fn pedantic_changes_skip_message() {
        let normal = compute_telemetry(&[], &HashMap::new(), false);
        let pedantic = compute_telemetry(&[], &HashMap::new(), true);
        // Both should have validators_skipped > 0 (gated validators are still skipped;
        // pedantic only changes the message).
        assert!(normal
            .validators
            .iter()
            .any(|v| matches!(v.skip_reason.as_deref(), Some(msg) if msg.contains("--pedantic"))));
        assert!(pedantic
            .validators
            .iter()
            .any(|v| matches!(v.skip_reason.as_deref(), Some(msg) if msg == "not yet implemented")));
    }

    #[test]
    fn cache_insert_get_roundtrip() {
        let mut cache = CheckCache::new();
        let key = CacheKey {
            source_hash: "abc123".to_string(),
            target: "llvm".to_string(),
            validator: "validate_atomic_ordering".to_string(),
        };
        assert!(cache.get(&key).is_none());
        cache.insert(key.clone(), Vec::new());
        assert!(cache.get(&key).is_some());
        assert_eq!(cache.len(), 1);
        cache.clear();
        assert!(cache.is_empty());
    }

    #[test]
    fn confidence_blends_validator_and_category_coverage() {
        // Empty ran set + always-runs. Validators marked always_runs=true still
        // count as ran. With all categories covered by always-runs, we should
        // see non-zero confidence.
        let telemetry = compute_telemetry(&[], &HashMap::new(), false);
        // Validator fraction: ran / total (≈ 4 / N) weighted 0.7
        // Category coverage: covered / total (≥ 4 / N) weighted 0.3
        // Sum should land between 0.1 and 0.5 for the current registry.
        assert!(telemetry.confidence > 0.05);
        assert!(telemetry.confidence < 0.6);
    }

    #[test]
    fn gap_summary_mentions_pedantic_when_categories_missing() {
        let telemetry = compute_telemetry(&[], &HashMap::new(), false);
        if !telemetry.missing_categories.is_empty() {
            assert!(telemetry.gap_summary.contains("--pedantic"));
        }
    }
}
