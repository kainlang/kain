//! Animation State Machine Intermediate Representation
//!
//! This module defines the IR structures for animation state machines
//! and provides conversion from AST to IR with proper type mapping.
//!
//! The StateMachine system supports:
//! - State definitions with animation references
//! - Transition conditions with priority ordering
//! - Entry/exit callbacks for state lifecycle
//! - Property storage per state

use crate::ue5::context::Ue5Context;
use kain_core::ast::{Block, StateDef, StateMachineDef, TransitionDef};

/// Animation state machine intermediate representation
/// Represents a state machine with states, transitions, and animation references
#[derive(Debug, Clone)]
pub struct StateMachineIR {
    /// Name of the state machine (without U prefix)
    pub name: String,

    /// List of states in the state machine
    pub states: Vec<StateIR>,

    /// Name of the entry state (first state to activate)
    pub entry_state: String,
}

/// A single state within a state machine
#[derive(Debug, Clone)]
pub struct StateIR {
    /// State name (used for enum variant)
    pub name: String,

    /// Optional animation asset reference
    pub animation: Option<String>,

    /// Transitions from this state to other states
    pub transitions: Vec<TransitionIR>,

    /// Optional callback code when entering this state
    pub on_enter: Option<String>,

    /// Optional callback code when exiting this state
    pub on_exit: Option<String>,
}

/// Transition between states with condition and priority
#[derive(Debug, Clone)]
pub struct TransitionIR {
    /// Target state name
    pub to_state: String,

    /// Condition expression (C++ code) that must be true for transition
    pub condition: String,

    /// Priority for transition evaluation (higher = evaluated first)
    pub priority: i32,
}

/// Convert a state machine definition from AST to StateMachineIR
///
/// # Arguments
/// * `state_machine` - The state machine definition from AST
/// * `ctx` - UE5 compilation context for type mapping
///
/// # Returns
/// * `Ok(StateMachineIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_state_machine_ir(
    state_machine: &StateMachineDef,
    ctx: &Ue5Context,
) -> Result<StateMachineIR, String> {
    // Find entry state
    let entry_state = find_entry_state(&state_machine.states)?;

    // Convert all states
    let mut states = Vec::new();
    for state_def in &state_machine.states {
        let state_ir = convert_state(state_def, ctx)?;
        states.push(state_ir);
    }

    Ok(StateMachineIR {
        name: state_machine.name.clone(),
        states,
        entry_state,
    })
}

/// Find the entry state in the state machine
fn find_entry_state(states: &[StateDef]) -> Result<String, String> {
    let entry_states: Vec<_> = states.iter().filter(|s| s.is_entry).collect();

    match entry_states.len() {
        0 => {
            Err("State machine must have at least one entry state (use is_entry: true)".to_string())
        }
        1 => Ok(entry_states[0].name.clone()),
        _ => Err(format!(
            "State machine has multiple entry states: {}. Only one entry state is allowed.",
            entry_states
                .iter()
                .map(|s| s.name.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        )),
    }
}

/// Convert a single state definition to StateIR
fn convert_state(state_def: &StateDef, ctx: &Ue5Context) -> Result<StateIR, String> {
    // Convert transitions
    let mut transitions = Vec::new();
    for transition_def in &state_def.transitions {
        let transition_ir = convert_transition(transition_def, ctx)?;
        transitions.push(transition_ir);
    }

    // Sort transitions by priority (highest first)
    transitions.sort_by(|a, b| b.priority.cmp(&a.priority));

    // Convert on_enter callback
    let on_enter = state_def
        .on_enter
        .as_ref()
        .map(|block| convert_block_to_cpp(block, ctx));

    // Convert on_exit callback
    let on_exit = state_def
        .on_exit
        .as_ref()
        .map(|block| convert_block_to_cpp(block, ctx));

    Ok(StateIR {
        name: state_def.name.clone(),
        animation: state_def.animation.clone(),
        transitions,
        on_enter,
        on_exit,
    })
}

/// Convert a transition definition to TransitionIR
fn convert_transition(
    transition_def: &TransitionDef,
    ctx: &Ue5Context,
) -> Result<TransitionIR, String> {
    // Convert condition block to C++ expression
    let condition = if let Some(block) = &transition_def.condition {
        convert_block_to_cpp(block, ctx)
    } else {
        // No condition means always transition (use "true")
        "true".to_string()
    };

    Ok(TransitionIR {
        to_state: transition_def.to_state.clone(),
        condition,
        priority: transition_def.priority,
    })
}

/// Convert a KAIN block to C++ code
///
/// This is a placeholder implementation that will be replaced with proper
/// expression codegen when the full codegen pipeline is integrated.
fn convert_block_to_cpp(block: &Block, _ctx: &Ue5Context) -> String {
    // For now, return a placeholder comment
    // TODO: Integrate with expression codegen from ue5 crate
    format!("/* Block with {} statements */", block.stmts.len())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Block, StateDef, StateMachineDef, TransitionDef};
    use kain_core::span::Span;

    fn dummy_span() -> Span {
        Span::new(0, 0)
    }

    fn make_simple_state(name: &str, is_entry: bool) -> StateDef {
        StateDef {
            name: name.to_string(),
            is_entry,
            animation: Some(format!("{}_Anim", name)),
            properties: vec![],
            transitions: vec![],
            on_enter: None,
            on_exit: None,
            attributes: vec![],
            span: dummy_span(),
        }
    }

    fn make_transition(to_state: &str, priority: i32) -> TransitionDef {
        TransitionDef {
            to_state: to_state.to_string(),
            condition: None,
            priority,
            attributes: vec![],
            span: dummy_span(),
        }
    }

    #[test]
    fn test_find_entry_state() {
        let states = vec![
            make_simple_state("Idle", true),
            make_simple_state("Walk", false),
            make_simple_state("Run", false),
        ];

        let entry = find_entry_state(&states).unwrap();
        assert_eq!(entry, "Idle");
    }

    #[test]
    fn test_find_entry_state_missing() {
        let states = vec![
            make_simple_state("Walk", false),
            make_simple_state("Run", false),
        ];

        let result = find_entry_state(&states);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .contains("must have at least one entry state"));
    }

    #[test]
    fn test_find_entry_state_multiple() {
        let states = vec![
            make_simple_state("Idle", true),
            make_simple_state("Walk", true),
        ];

        let result = find_entry_state(&states);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("multiple entry states"));
    }

    #[test]
    fn test_convert_simple_state_machine() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let mut idle_state = make_simple_state("Idle", true);
        idle_state.transitions.push(make_transition("Walk", 1));

        let walk_state = make_simple_state("Walk", false);

        let state_machine = StateMachineDef {
            name: "CharacterAnimations".to_string(),
            states: vec![idle_state, walk_state],
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_to_state_machine_ir(&state_machine, &ctx).unwrap();

        assert_eq!(ir.name, "CharacterAnimations");
        assert_eq!(ir.entry_state, "Idle");
        assert_eq!(ir.states.len(), 2);
        assert_eq!(ir.states[0].name, "Idle");
        assert_eq!(ir.states[0].animation, Some("Idle_Anim".to_string()));
        assert_eq!(ir.states[0].transitions.len(), 1);
        assert_eq!(ir.states[0].transitions[0].to_state, "Walk");
    }

    #[test]
    fn test_transition_priority_sorting() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let mut idle_state = make_simple_state("Idle", true);
        idle_state.transitions.push(make_transition("Walk", 1));
        idle_state.transitions.push(make_transition("Run", 10));
        idle_state.transitions.push(make_transition("Jump", 5));

        let state_machine = StateMachineDef {
            name: "TestMachine".to_string(),
            states: vec![idle_state],
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_to_state_machine_ir(&state_machine, &ctx).unwrap();

        // Transitions should be sorted by priority (highest first)
        assert_eq!(ir.states[0].transitions[0].to_state, "Run");
        assert_eq!(ir.states[0].transitions[0].priority, 10);
        assert_eq!(ir.states[0].transitions[1].to_state, "Jump");
        assert_eq!(ir.states[0].transitions[1].priority, 5);
        assert_eq!(ir.states[0].transitions[2].to_state, "Walk");
        assert_eq!(ir.states[0].transitions[2].priority, 1);
    }

    #[test]
    fn test_convert_state_with_callbacks() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let mut state = make_simple_state("Idle", true);
        state.on_enter = Some(Block {
            stmts: vec![],
            span: dummy_span(),
        });
        state.on_exit = Some(Block {
            stmts: vec![],
            span: dummy_span(),
        });

        let state_ir = convert_state(&state, &ctx).unwrap();

        assert!(state_ir.on_enter.is_some());
        assert!(state_ir.on_exit.is_some());
    }

    #[test]
    fn test_convert_transition_without_condition() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let transition = make_transition("Walk", 1);
        let transition_ir = convert_transition(&transition, &ctx).unwrap();

        // No condition should default to "true"
        assert_eq!(transition_ir.condition, "true");
    }
}
