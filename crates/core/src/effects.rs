//! KAIN Effect System - Track side effects at compile time

use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{CompilerPhase, DiagnosticReport, ErrorKind, KainError, KainResult};
use crate::span::Span;
use std::collections::HashSet;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Effect {
    Pure,     // No side effects
    IO,       // File/Network/Console
    Async,    // Can await
    GPU,      // Runs on graphics hardware
    Reactive, // Triggers UI updates
    Unsafe,   // Breaks safety guarantees
    Alloc,    // Memory allocation
    Panic,    // Can abort
}

impl Effect {
    pub fn from_str(s: &str) -> Option<Self> {
        match s {
            "Pure" => Some(Effect::Pure),
            "IO" => Some(Effect::IO),
            "Async" => Some(Effect::Async),
            "GPU" => Some(Effect::GPU),
            "Reactive" => Some(Effect::Reactive),
            "Unsafe" => Some(Effect::Unsafe),
            _ => None,
        }
    }
}

fn effect_name(effect: Effect) -> &'static str {
    match effect {
        Effect::Pure => "Pure",
        Effect::IO => "IO",
        Effect::Async => "Async",
        Effect::GPU => "GPU",
        Effect::Reactive => "Reactive",
        Effect::Unsafe => "Unsafe",
        Effect::Alloc => "Alloc",
        Effect::Panic => "Panic",
    }
}

fn effect_set_to_string(effect_set: &EffectSet) -> String {
    if effect_set.is_pure() {
        return "Pure".to_string();
    }

    let mut labels = Vec::new();
    for effect in effect_set.effects.iter().copied() {
        labels.push(effect_name(effect).to_string());
    }
    labels.join(", ")
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct EffectSet {
    pub effects: HashSet<Effect>,
}

impl EffectSet {
    pub fn new() -> Self {
        Self {
            effects: HashSet::new(),
        }
    }
    pub fn pure() -> Self {
        Self::new().with(Effect::Pure)
    }

    pub fn with(mut self, e: Effect) -> Self {
        self.effects.insert(e);
        self
    }

    pub fn is_pure(&self) -> bool {
        self.effects.is_empty() || self.effects.iter().all(|e| *e == Effect::Pure)
    }

    pub fn can_call(&self, callee: &EffectSet) -> bool {
        if callee.is_pure() {
            return true;
        }
        if self.is_pure() {
            return false;
        }
        if self.effects.contains(&Effect::Unsafe) {
            return true;
        }
        callee.effects.iter().all(|e| self.effects.contains(e))
    }
}

pub fn check_effect_call(
    caller: &EffectSet,
    callee: &EffectSet,
    caller_name: &str,
    callee_name: &str,
    span: Span,
) -> KainResult<()> {
    if !caller.can_call(callee) {
        let caller_effect_str = effect_set_to_string(caller);
        let callee_effect_str = effect_set_to_string(callee);

        let report = DiagnosticReport::new(
            ErrorKind::Effect,
            DiagnosticCode::EffectViolation,
            format!(
                "{} function '{}' cannot call {} function '{}'",
                caller_effect_str, caller_name, callee_effect_str, callee_name
            ),
        )
        .phase(CompilerPhase::EffectChecking)
        .primary_label(
            span,
            format!(
                "'{}' requires {}, but '{}' is only {}",
                callee_name, callee_effect_str, caller_name, caller_effect_str
            ),
        )
        .note(format!(
            "Caller '{}' is marked {} while '{}' is marked {}.",
            caller_name, caller_effect_str, callee_name, callee_effect_str
        ))
        .help(format!(
            "Broaden '{}' to {} if this call is intentional.",
            caller_name, callee_effect_str
        ))
        .help(format!(
            "Keep '{}' {} and move the effectful work behind a helper or different call path.",
            caller_name, caller_effect_str
        ));

        return Err(KainError::rich(report));
    }
    Ok(())
}
