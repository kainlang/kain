//! Data-driven identifier sanitization and stable rename mapping for importers.
//!
//! This keeps source identifiers parseable in KAIN while preserving stable
//! declaration/use alignment via deterministic rename lookups.

use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum IdentifierDomain {
    Value,
    Type,
    Field,
    Variant,
}

const EXTRA_LEXER_KEYWORDS: &[&str] = &[
    // These tokenize as operators and cannot appear in identifier positions.
    "and", "or",
    // First-class/contextual tokens that the parser does not accept in normal
    // imported value/type/field positions even when the source language does.
    "shader",
    "state",
    "vertex",
    "fragment",
    "Pure",
    "IO",
    "GPU",
    "Reactive",
    "Unsafe",
    "patch",
    "law",
    "axiom",
    "pulse",
    "orchestrate",
    "converge",
    "world",
    "entangle",
    "shatter",
    "teleport",
    "every",
    "when",
    "guarantee",
    "fallback",
    "spec",
    "fast",
    "verify",
    "random",
    "jitter",
    "target",
    "capability",
    "from",
    "to",
    "via",
    "surface",
    "native_ui",
    "viewport3d",
    "web",
    "ue5",
    "compute",
    "uniform",
    "render",
    "on",
    "weak",
    "single_writer",
];

pub fn is_reserved_identifier(name: &str) -> bool {
    kain_core::parser::RESERVED_KEYWORDS.contains(&name) || EXTRA_LEXER_KEYWORDS.contains(&name)
}

#[derive(Debug, Clone, Default)]
pub struct StableIdentifierRenamer {
    by_domain: HashMap<IdentifierDomain, HashMap<String, String>>,
    used_by_domain: HashMap<IdentifierDomain, HashSet<String>>,
}

impl StableIdentifierRenamer {
    pub fn resolve(&mut self, domain: IdentifierDomain, raw: &str) -> String {
        let trimmed = raw.trim();
        if let Some(existing) = self
            .by_domain
            .get(&domain)
            .and_then(|entries| entries.get(trimmed))
        {
            return existing.clone();
        }

        let base = sanitize_identifier_base(trimmed);
        let used = self.used_by_domain.entry(domain).or_default();

        let mut candidate = base.clone();
        let mut index = 2;
        while used.contains(&candidate) {
            candidate = format!("{base}_{index}");
            index += 1;
        }
        used.insert(candidate.clone());

        self.by_domain
            .entry(domain)
            .or_default()
            .insert(trimmed.to_string(), candidate.clone());

        candidate
    }
}

pub fn sanitize_identifier_base(raw: &str) -> String {
    let mut out = String::with_capacity(raw.len().max(4));
    let mut previous_was_underscore = false;

    for ch in raw.chars() {
        let mapped = if ch.is_ascii_alphanumeric() || ch == '_' {
            ch
        } else {
            '_'
        };

        if mapped == '_' {
            if !previous_was_underscore {
                out.push('_');
            }
            previous_was_underscore = true;
        } else {
            out.push(mapped);
            previous_was_underscore = false;
        }
    }

    out = out.trim_matches('_').to_string();
    if out.is_empty() {
        out = "c_id".to_string();
    }
    if out == "_" {
        out = "c_id".to_string();
    }
    if out
        .chars()
        .next()
        .is_some_and(|first| first.is_ascii_digit())
    {
        out = format!("c_{out}");
    }
    if is_reserved_identifier(&out) {
        out.push('_');
    }

    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanitize_identifier_handles_reserved_and_digits() {
        assert_eq!(sanitize_identifier_base("type"), "type_");
        assert_eq!(sanitize_identifier_base("in"), "in_");
        assert_eq!(sanitize_identifier_base("state"), "state_");
        assert_eq!(sanitize_identifier_base("pulse"), "pulse_");
        assert_eq!(sanitize_identifier_base("123abc"), "c_123abc");
    }

    #[test]
    fn renamer_assigns_stable_domain_scoped_names() {
        let mut renamer = StableIdentifierRenamer::default();

        assert_eq!(renamer.resolve(IdentifierDomain::Value, "type"), "type_");
        assert_eq!(renamer.resolve(IdentifierDomain::Value, "type"), "type_");
        assert_eq!(renamer.resolve(IdentifierDomain::Value, "type_"), "type__2");
        assert_eq!(renamer.resolve(IdentifierDomain::Field, "type"), "type_");
    }
}
