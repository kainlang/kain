//! KAIN Effect System - Track side effects at compile time

use crate::diagnostic_registry::DiagnosticCode;
use crate::error::KainResult;
use crate::error::{DiagnosticBuilder, ErrorKind};
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
    _span: Span,
) -> KainResult<()> {
    if !caller.can_call(callee) {
        let caller_effect_str = if caller.is_pure() {
            "Pure".to_string()
        } else {
            caller
                .effects
                .iter()
                .map(|e| match e {
                    Effect::Pure => "Pure",
                    Effect::IO => "IO",
                    Effect::Async => "Async",
                    Effect::GPU => "GPU",
                    Effect::Reactive => "Reactive",
                    Effect::Unsafe => "Unsafe",
                    Effect::Alloc => "Alloc",
                    Effect::Panic => "Panic",
                })
                .collect::<Vec<_>>()
                .join(", ")
        };

        let callee_effect_str = if callee.is_pure() {
            "Pure".to_string()
        } else {
            callee
                .effects
                .iter()
                .map(|e| match e {
                    Effect::Pure => "Pure",
                    Effect::IO => "IO",
                    Effect::Async => "Async",
                    Effect::GPU => "GPU",
                    Effect::Reactive => "Reactive",
                    Effect::Unsafe => "Unsafe",
                    Effect::Alloc => "Alloc",
                    Effect::Panic => "Panic",
                })
                .collect::<Vec<_>>()
                .join(", ")
        };

        return Err(DiagnosticBuilder::new(
            ErrorKind::Validation,
            DiagnosticCode::EffectViolation,
            format!(
                "Effect violation: {} function '{}' cannot call {} function '{}'.

Effect System Rules:
  • Pure functions: No side effects, can only call Pure functions
  • IO functions: Can perform I/O (file/network/console), can call Pure or IO functions
  • Async functions: Can perform async operations, can call Pure, IO, or Async functions
  • GPU functions: Run on graphics hardware, can call Pure or GPU functions
  • Unsafe functions: Can break safety guarantees, can call any function

Current situation:
  • Caller '{}' is marked as {}
  • Callee '{}' is marked as {}

How to fix:
  1. Add effect annotation to caller: fn {}() -> RetType with {}
  2. OR mark callee as Pure if it has no side effects
  3. OR change your call chain to avoid mixing incompatible effects

Example (Pure calling IO - INVALID):
  fn read_config() -> String with IO:
      let data = load_from_disk()  # OK: IO can call IO
      return data
  
  fn calculate_score() -> Int with Pure:
      let config = read_config()   # ERROR: Pure cannot call IO
      return 42

Example (Fixed):
  fn calculate_score() -> Int with IO:  # Changed to IO
      let config = read_config()   # OK: IO can call IO
      return 42
",
                caller_effect_str,
                caller_name,
                callee_effect_str,
                callee_name,
                caller_name,
                caller_effect_str,
                callee_name,
                callee_effect_str,
                caller_name,
                callee_effect_str
            ),
        )
        .context(format!(
            "Caller '{}' ({}) attempted to call '{}' ({})",
            caller_name, caller_effect_str, callee_name, callee_effect_str
        ))
        .build());
    }
    Ok(())
}
