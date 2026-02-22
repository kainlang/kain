//! Integration tests for state machine code generation
//!
//! These tests verify the complete pipeline from AST to generated C++ code.

use kain_core::ast::{StateMachineDef, StateDef, TransitionDef, Block, Attribute};
use kain_core::span::Span;
use ue5::ue5::context::Ue5Context;
use ue5::state_machine_ir::convert_to_state_machine_ir;
use ue5::state_machine_codegen::generate_state_machine_code;

fn dummy_span() -> Span {
    Span::new(0, 0)
}

fn make_character_animations_state_machine() -> StateMachineDef {
    StateMachineDef {
        name: "CharacterAnimations".to_string(),
        states: vec![
            StateDef {
                name: "Idle".to_string(),
                is_entry: true,
                animation: Some("Idle_Anim".to_string()),
                properties: vec![],
                transitions: vec![
                    TransitionDef {
                        to_state: "Walk".to_string(),
                        condition: None, // Will use placeholder
                        priority: 1,
                        attributes: vec![],
                        span: dummy_span(),
                    },
                ],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
            StateDef {
                name: "Walk".to_string(),
                is_entry: false,
                animation: Some("Walk_Anim".to_string()),
                properties: vec![],
                transitions: vec![
                    TransitionDef {
                        to_state: "Run".to_string(),
                        condition: None,
                        priority: 1,
                        attributes: vec![],
                        span: dummy_span(),
                    },
                    TransitionDef {
                        to_state: "Idle".to_string(),
                        condition: None,
                        priority: 0,
                        attributes: vec![],
                        span: dummy_span(),
                    },
                ],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
            StateDef {
                name: "Run".to_string(),
                is_entry: false,
                animation: Some("Run_Anim".to_string()),
                properties: vec![],
                transitions: vec![
                    TransitionDef {
                        to_state: "Walk".to_string(),
                        condition: None,
                        priority: 1,
                        attributes: vec![],
                        span: dummy_span(),
                    },
                ],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        attributes: vec![],
        span: dummy_span(),
    }
}

#[test]
fn test_state_machine_end_to_end() {
    // 1. Create AST
    let state_machine_ast = make_character_animations_state_machine();
    
    // 2. Convert to IR
    let ctx = Ue5Context::new("TestPlugin", None);
    let ir = convert_to_state_machine_ir(&state_machine_ast, &ctx)
        .expect("Failed to convert to IR");
    
    // 3. Generate code
    let output = generate_state_machine_code(&ir, "TestPlugin");
    
    // 4. Verify header contains expected elements
    assert!(output.header.contains("enum class ECharacterAnimationsState"));
    assert!(output.header.contains("Idle UMETA(DisplayName = \"Idle\")"));
    assert!(output.header.contains("Walk UMETA(DisplayName = \"Walk\")"));
    assert!(output.header.contains("Run UMETA(DisplayName = \"Run\")"));
    
    assert!(output.header.contains("class TESTPLUGIN_API UCharacterAnimationsStateMachine"));
    assert!(output.header.contains("ECharacterAnimationsState CurrentState"));
    
    assert!(output.header.contains("UAnimSequence* IdleAnimation"));
    assert!(output.header.contains("UAnimSequence* WalkAnimation"));
    assert!(output.header.contains("UAnimSequence* RunAnimation"));
    
    assert!(output.header.contains("void UpdateStateMachine(float DeltaTime)"));
    assert!(output.header.contains("UAnimSequence* GetCurrentAnimation()"));
    
    assert!(output.header.contains("bool CanTransitionToWalk()"));
    assert!(output.header.contains("bool CanTransitionToRun()"));
    assert!(output.header.contains("bool CanTransitionToIdle()"));
    
    // 5. Verify source contains expected implementations
    assert!(output.source.contains("UCharacterAnimationsStateMachine::UCharacterAnimationsStateMachine()"));
    assert!(output.source.contains("CurrentState = ECharacterAnimationsState::Idle"));
    
    assert!(output.source.contains("void UCharacterAnimationsStateMachine::UpdateStateMachine(float DeltaTime)"));
    assert!(output.source.contains("switch (CurrentState)"));
    assert!(output.source.contains("case ECharacterAnimationsState::Idle:"));
    assert!(output.source.contains("if (CanTransitionToWalk())"));
    assert!(output.source.contains("TransitionToState(ECharacterAnimationsState::Walk)"));
    
    assert!(output.source.contains("UAnimSequence* UCharacterAnimationsStateMachine::GetCurrentAnimation() const"));
    assert!(output.source.contains("return IdleAnimation"));
    assert!(output.source.contains("return WalkAnimation"));
    assert!(output.source.contains("return RunAnimation"));
    
    assert!(output.source.contains("bool UCharacterAnimationsStateMachine::CanTransitionToWalk() const"));
    assert!(output.source.contains("bool UCharacterAnimationsStateMachine::CanTransitionToRun() const"));
    assert!(output.source.contains("bool UCharacterAnimationsStateMachine::CanTransitionToIdle() const"));
    
    assert!(output.source.contains("void UCharacterAnimationsStateMachine::TransitionToState(ECharacterAnimationsState NewState)"));
    
    // 6. Verify includes
    assert!(output.includes.contains(&"CoreMinimal.h".to_string()));
    assert!(output.includes.contains(&"Animation/AnimSequence.h".to_string()));
}

#[test]
fn test_state_machine_with_callbacks() {
    let mut state_machine_ast = make_character_animations_state_machine();
    
    // Add entry/exit callbacks to Idle state
    state_machine_ast.states[0].on_enter = Some(Block {
        stmts: vec![],
        span: dummy_span(),
    });
    state_machine_ast.states[0].on_exit = Some(Block {
        stmts: vec![],
        span: dummy_span(),
    });
    
    let ctx = Ue5Context::new("TestPlugin", None);
    let ir = convert_to_state_machine_ir(&state_machine_ast, &ctx)
        .expect("Failed to convert to IR");
    
    let output = generate_state_machine_code(&ir, "TestPlugin");
    
    // Verify callback declarations
    assert!(output.header.contains("void OnEnterIdle()"));
    assert!(output.header.contains("void OnExitIdle()"));
    
    // Verify callback implementations
    assert!(output.source.contains("void UCharacterAnimationsStateMachine::OnEnterIdle()"));
    assert!(output.source.contains("void UCharacterAnimationsStateMachine::OnExitIdle()"));
    
    // Verify callbacks are called in TransitionToState
    assert!(output.source.contains("case ECharacterAnimationsState::Idle:"));
    assert!(output.source.contains("OnExitIdle()"));
    assert!(output.source.contains("OnEnterIdle()"));
}

#[test]
fn test_state_machine_transition_priority() {
    let state_machine_ast = make_character_animations_state_machine();
    
    let ctx = Ue5Context::new("TestPlugin", None);
    let ir = convert_to_state_machine_ir(&state_machine_ast, &ctx)
        .expect("Failed to convert to IR");
    
    // Verify Walk state has transitions sorted by priority
    let walk_state = ir.states.iter().find(|s| s.name == "Walk").unwrap();
    assert_eq!(walk_state.transitions.len(), 2);
    
    // Higher priority should come first
    assert_eq!(walk_state.transitions[0].to_state, "Run");
    assert_eq!(walk_state.transitions[0].priority, 1);
    assert_eq!(walk_state.transitions[1].to_state, "Idle");
    assert_eq!(walk_state.transitions[1].priority, 0);
    
    let output = generate_state_machine_code(&ir, "TestPlugin");
    
    // Verify generated code evaluates transitions in priority order
    let walk_case_start = output.source.find("case ECharacterAnimationsState::Walk:").unwrap();
    let run_transition = output.source[walk_case_start..].find("if (CanTransitionToRun())").unwrap();
    let idle_transition = output.source[walk_case_start..].find("if (CanTransitionToIdle())").unwrap();
    
    // Run transition should come before Idle transition
    assert!(run_transition < idle_transition);
}

#[test]
fn test_state_machine_no_transitions() {
    let mut state_machine_ast = StateMachineDef {
        name: "SimpleStateMachine".to_string(),
        states: vec![
            StateDef {
                name: "OnlyState".to_string(),
                is_entry: true,
                animation: Some("OnlyAnim".to_string()),
                properties: vec![],
                transitions: vec![],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        attributes: vec![],
        span: dummy_span(),
    };
    
    let ctx = Ue5Context::new("TestPlugin", None);
    let ir = convert_to_state_machine_ir(&state_machine_ast, &ctx)
        .expect("Failed to convert to IR");
    
    let output = generate_state_machine_code(&ir, "TestPlugin");
    
    // Verify state with no transitions generates valid code
    assert!(output.header.contains("enum class ESimpleStateMachineState"));
    assert!(output.header.contains("OnlyState UMETA(DisplayName = \"OnlyState\")"));
    assert!(output.source.contains("case ESimpleStateMachineState::OnlyState:"));
    assert!(output.source.contains("// No transitions from this state"));
}

#[test]
fn test_state_machine_multiple_entry_states_error() {
    let state_machine_ast = StateMachineDef {
        name: "InvalidStateMachine".to_string(),
        states: vec![
            StateDef {
                name: "State1".to_string(),
                is_entry: true,
                animation: None,
                properties: vec![],
                transitions: vec![],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
            StateDef {
                name: "State2".to_string(),
                is_entry: true,
                animation: None,
                properties: vec![],
                transitions: vec![],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        attributes: vec![],
        span: dummy_span(),
    };
    
    let ctx = Ue5Context::new("TestPlugin", None);
    let result = convert_to_state_machine_ir(&state_machine_ast, &ctx);
    
    // Should fail with multiple entry states error
    assert!(result.is_err());
    assert!(result.unwrap_err().contains("multiple entry states"));
}

#[test]
fn test_state_machine_no_entry_state_error() {
    let state_machine_ast = StateMachineDef {
        name: "InvalidStateMachine".to_string(),
        states: vec![
            StateDef {
                name: "State1".to_string(),
                is_entry: false,
                animation: None,
                properties: vec![],
                transitions: vec![],
                on_enter: None,
                on_exit: None,
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        attributes: vec![],
        span: dummy_span(),
    };
    
    let ctx = Ue5Context::new("TestPlugin", None);
    let result = convert_to_state_machine_ir(&state_machine_ast, &ctx);
    
    // Should fail with no entry state error
    assert!(result.is_err());
    assert!(result.unwrap_err().contains("must have at least one entry state"));
}
