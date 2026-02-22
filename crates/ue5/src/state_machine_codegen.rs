//! Animation State Machine Code Generation
//!
//! This module generates C++ code for animation state machines including:
//! - State machine runtime class (UObject)
//! - State enum with all state variants
//! - State classes with animation references
//! - Transition evaluation methods
//! - State entry/exit methods
//! - State machine update logic

use crate::state_machine_ir::{StateMachineIR, StateIR, TransitionIR};

/// Output from state machine code generation
#[derive(Debug, Clone)]
pub struct StateMachineCodegenOutput {
    /// Header file content
    pub header: String,
    
    /// Source file content
    pub source: String,
    
    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Generate state machine code from IR
///
/// # Arguments
/// * `ir` - The state machine intermediate representation
/// * `plugin_name` - Name of the plugin (for API macro)
///
/// # Returns
/// * `StateMachineCodegenOutput` - Generated header and source files
pub fn generate_state_machine_code(
    ir: &StateMachineIR,
    plugin_name: &str,
) -> StateMachineCodegenOutput {
    let class_name = format!("U{}StateMachine", ir.name);
    let api_macro = format!("{}_API", plugin_name.to_uppercase());
    
    let mut header = String::new();
    let mut source = String::new();
    
    // Generate header
    generate_header(ir, &class_name, &api_macro, &mut header);
    
    // Generate source
    generate_source(ir, &class_name, &mut source);
    
    StateMachineCodegenOutput {
        header,
        source,
        includes: vec![
            "CoreMinimal.h".to_string(),
            "UObject/NoExportTypes.h".to_string(),
            "Animation/AnimSequence.h".to_string(),
        ],
    }
}

/// Generate header file content
fn generate_header(
    ir: &StateMachineIR,
    class_name: &str,
    api_macro: &str,
    output: &mut String,
) {
    // Header guard
    let guard = format!("{}_{}_H", class_name.to_uppercase(), "GENERATED");
    output.push_str(&format!("#pragma once\n\n"));
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"UObject/NoExportTypes.h\"\n");
    output.push_str("#include \"Animation/AnimSequence.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.name));
    
    // State enum
    generate_state_enum(ir, output);
    
    // State machine class
    output.push_str(&format!("UCLASS(BlueprintType)\n"));
    output.push_str(&format!("class {} {} : public UObject\n", api_macro, class_name));
    output.push_str("{\n");
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");
    
    // Constructor
    output.push_str(&format!("    {}();\n\n", class_name));
    
    // Current state property
    output.push_str("    /** Current active state */\n");
    output.push_str("    UPROPERTY(BlueprintReadOnly, Category = \"State Machine\")\n");
    output.push_str(&format!("    E{}State CurrentState;\n\n", ir.name));
    
    // Animation references per state
    generate_animation_properties(ir, output);
    
    // Update method
    output.push_str("    /** Update state machine and evaluate transitions */\n");
    output.push_str("    UFUNCTION(BlueprintCallable, Category = \"State Machine\")\n");
    output.push_str("    void UpdateStateMachine(float DeltaTime);\n\n");
    
    // Get current animation method
    output.push_str("    /** Get animation for current state */\n");
    output.push_str("    UFUNCTION(BlueprintPure, Category = \"State Machine\")\n");
    output.push_str("    UAnimSequence* GetCurrentAnimation() const;\n\n");
    
    // Transition evaluation methods
    generate_transition_method_declarations(ir, output);
    
    // State entry/exit methods
    generate_state_callback_declarations(ir, output);
    
    output.push_str("private:\n");
    
    // Internal transition method
    output.push_str("    /** Internal method to transition to a new state */\n");
    output.push_str(&format!("    void TransitionToState(E{}State NewState);\n\n", ir.name));
    
    output.push_str("};\n");
}

/// Generate state enum
fn generate_state_enum(ir: &StateMachineIR, output: &mut String) {
    output.push_str("/** State enum for state machine */\n");
    output.push_str("UENUM(BlueprintType)\n");
    output.push_str(&format!("enum class E{}State : uint8\n", ir.name));
    output.push_str("{\n");
    
    for (i, state) in ir.states.iter().enumerate() {
        output.push_str(&format!("    {} UMETA(DisplayName = \"{}\")", state.name, state.name));
        if i < ir.states.len() - 1 {
            output.push_str(",\n");
        } else {
            output.push_str("\n");
        }
    }
    
    output.push_str("};\n\n");
}

/// Generate animation properties for each state
fn generate_animation_properties(ir: &StateMachineIR, output: &mut String) {
    output.push_str("    /** Animation references for each state */\n");
    
    for state in &ir.states {
        if state.animation.is_some() {
            output.push_str(&format!("    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = \"State Machine|Animations\")\n"));
            output.push_str(&format!("    UAnimSequence* {}Animation;\n\n", state.name));
        }
    }
}

/// Generate transition evaluation method declarations
fn generate_transition_method_declarations(ir: &StateMachineIR, output: &mut String) {
    output.push_str("    /** Transition evaluation methods */\n");
    
    for state in &ir.states {
        for transition in &state.transitions {
            let method_name = format!("CanTransitionTo{}", transition.to_state);
            output.push_str(&format!("    bool {}() const;\n", method_name));
        }
    }
    
    if !ir.states.is_empty() && ir.states.iter().any(|s| !s.transitions.is_empty()) {
        output.push_str("\n");
    }
}

/// Generate state callback method declarations
fn generate_state_callback_declarations(ir: &StateMachineIR, output: &mut String) {
    let mut has_callbacks = false;
    
    for state in &ir.states {
        if state.on_enter.is_some() || state.on_exit.is_some() {
            has_callbacks = true;
            break;
        }
    }
    
    if !has_callbacks {
        return;
    }
    
    output.push_str("    /** State lifecycle callbacks */\n");
    
    for state in &ir.states {
        if state.on_enter.is_some() {
            output.push_str(&format!("    void OnEnter{}();\n", state.name));
        }
        if state.on_exit.is_some() {
            output.push_str(&format!("    void OnExit{}();\n", state.name));
        }
    }
    
    output.push_str("\n");
}

/// Generate source file content
fn generate_source(
    ir: &StateMachineIR,
    class_name: &str,
    output: &mut String,
) {
    // Include header
    output.push_str(&format!("#include \"{}.h\"\n\n", ir.name));
    
    // Constructor
    generate_constructor(ir, class_name, output);
    
    // UpdateStateMachine implementation
    generate_update_method(ir, class_name, output);
    
    // GetCurrentAnimation implementation
    generate_get_current_animation(ir, class_name, output);
    
    // Transition evaluation implementations
    generate_transition_method_implementations(ir, class_name, output);
    
    // State callback implementations
    generate_state_callback_implementations(ir, class_name, output);
    
    // TransitionToState implementation
    generate_transition_to_state_method(ir, class_name, output);
}

/// Generate constructor implementation
fn generate_constructor(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    output.push_str(&format!("    // Initialize to entry state\n"));
    output.push_str(&format!("    CurrentState = E{}State::{};\n", ir.name, ir.entry_state));
    
    // Initialize animation pointers to nullptr
    for state in &ir.states {
        if state.animation.is_some() {
            output.push_str(&format!("    {}Animation = nullptr;\n", state.name));
        }
    }
    
    output.push_str("}\n\n");
}

/// Generate UpdateStateMachine method implementation
fn generate_update_method(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("void {}::UpdateStateMachine(float DeltaTime)\n", class_name));
    output.push_str("{\n");
    output.push_str("    // Evaluate transitions based on current state\n");
    output.push_str("    switch (CurrentState)\n");
    output.push_str("    {\n");
    
    for state in &ir.states {
        output.push_str(&format!("    case E{}State::{}:\n", ir.name, state.name));
        output.push_str("    {\n");
        
        if state.transitions.is_empty() {
            output.push_str("        // No transitions from this state\n");
        } else {
            output.push_str("        // Evaluate transitions in priority order\n");
            
            for transition in &state.transitions {
                let method_name = format!("CanTransitionTo{}", transition.to_state);
                output.push_str(&format!("        if ({}())\n", method_name));
                output.push_str("        {\n");
                output.push_str(&format!("            TransitionToState(E{}State::{});\n", ir.name, transition.to_state));
                output.push_str("            return;\n");
                output.push_str("        }\n");
            }
        }
        
        output.push_str("        break;\n");
        output.push_str("    }\n");
    }
    
    output.push_str("    default:\n");
    output.push_str("        break;\n");
    output.push_str("    }\n");
    output.push_str("}\n\n");
}

/// Generate GetCurrentAnimation method implementation
fn generate_get_current_animation(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("UAnimSequence* {}::GetCurrentAnimation() const\n", class_name));
    output.push_str("{\n");
    output.push_str("    switch (CurrentState)\n");
    output.push_str("    {\n");
    
    for state in &ir.states {
        if state.animation.is_some() {
            output.push_str(&format!("    case E{}State::{}:\n", ir.name, state.name));
            output.push_str(&format!("        return {}Animation;\n", state.name));
        }
    }
    
    output.push_str("    default:\n");
    output.push_str("        return nullptr;\n");
    output.push_str("    }\n");
    output.push_str("}\n\n");
}

/// Generate transition evaluation method implementations
fn generate_transition_method_implementations(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    for state in &ir.states {
        for transition in &state.transitions {
            let method_name = format!("CanTransitionTo{}", transition.to_state);
            output.push_str(&format!("bool {}::{}() const\n", class_name, method_name));
            output.push_str("{\n");
            
            // Use the condition from IR
            if transition.condition == "true" {
                output.push_str("    // Always transition (no condition)\n");
                output.push_str("    return true;\n");
            } else {
                output.push_str(&format!("    // Transition condition\n"));
                output.push_str(&format!("    return {};\n", transition.condition));
            }
            
            output.push_str("}\n\n");
        }
    }
}

/// Generate state callback implementations
fn generate_state_callback_implementations(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    for state in &ir.states {
        if let Some(on_enter) = &state.on_enter {
            output.push_str(&format!("void {}::OnEnter{}()\n", class_name, state.name));
            output.push_str("{\n");
            output.push_str(&format!("    // Entry callback for {} state\n", state.name));
            output.push_str(&format!("    {}\n", on_enter));
            output.push_str("}\n\n");
        }
        
        if let Some(on_exit) = &state.on_exit {
            output.push_str(&format!("void {}::OnExit{}()\n", class_name, state.name));
            output.push_str("{\n");
            output.push_str(&format!("    // Exit callback for {} state\n", state.name));
            output.push_str(&format!("    {}\n", on_exit));
            output.push_str("}\n\n");
        }
    }
}

/// Generate TransitionToState method implementation
fn generate_transition_to_state_method(ir: &StateMachineIR, class_name: &str, output: &mut String) {
    output.push_str(&format!("void {}::TransitionToState(E{}State NewState)\n", class_name, ir.name));
    output.push_str("{\n");
    output.push_str("    if (CurrentState == NewState)\n");
    output.push_str("    {\n");
    output.push_str("        return; // Already in this state\n");
    output.push_str("    }\n\n");
    
    // Call exit callback for current state
    output.push_str("    // Call exit callback for current state\n");
    output.push_str("    switch (CurrentState)\n");
    output.push_str("    {\n");
    
    for state in &ir.states {
        if state.on_exit.is_some() {
            output.push_str(&format!("    case E{}State::{}:\n", ir.name, state.name));
            output.push_str(&format!("        OnExit{}();\n", state.name));
            output.push_str("        break;\n");
        }
    }
    
    output.push_str("    default:\n");
    output.push_str("        break;\n");
    output.push_str("    }\n\n");
    
    // Update current state
    output.push_str("    // Update current state\n");
    output.push_str("    CurrentState = NewState;\n\n");
    
    // Call entry callback for new state
    output.push_str("    // Call entry callback for new state\n");
    output.push_str("    switch (NewState)\n");
    output.push_str("    {\n");
    
    for state in &ir.states {
        if state.on_enter.is_some() {
            output.push_str(&format!("    case E{}State::{}:\n", ir.name, state.name));
            output.push_str(&format!("        OnEnter{}();\n", state.name));
            output.push_str("        break;\n");
        }
    }
    
    output.push_str("    default:\n");
    output.push_str("        break;\n");
    output.push_str("    }\n");
    output.push_str("}\n");
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::state_machine_ir::{StateMachineIR, StateIR, TransitionIR};
    
    fn make_simple_state_machine() -> StateMachineIR {
        StateMachineIR {
            name: "CharacterAnimations".to_string(),
            states: vec![
                StateIR {
                    name: "Idle".to_string(),
                    animation: Some("Idle_Anim".to_string()),
                    transitions: vec![
                        TransitionIR {
                            to_state: "Walk".to_string(),
                            condition: "Speed > 0.1f".to_string(),
                            priority: 1,
                        },
                    ],
                    on_enter: None,
                    on_exit: None,
                },
                StateIR {
                    name: "Walk".to_string(),
                    animation: Some("Walk_Anim".to_string()),
                    transitions: vec![
                        TransitionIR {
                            to_state: "Run".to_string(),
                            condition: "Speed > 5.0f".to_string(),
                            priority: 1,
                        },
                    ],
                    on_enter: None,
                    on_exit: None,
                },
                StateIR {
                    name: "Run".to_string(),
                    animation: Some("Run_Anim".to_string()),
                    transitions: vec![],
                    on_enter: None,
                    on_exit: None,
                },
            ],
            entry_state: "Idle".to_string(),
        }
    }
    
    #[test]
    fn test_generate_state_machine_header() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        // Check header contains expected elements
        assert!(output.header.contains("enum class ECharacterAnimationsState"));
        assert!(output.header.contains("class TESTPLUGIN_API UCharacterAnimationsStateMachine"));
        assert!(output.header.contains("ECharacterAnimationsState CurrentState"));
        assert!(output.header.contains("void UpdateStateMachine(float DeltaTime)"));
        assert!(output.header.contains("UAnimSequence* GetCurrentAnimation()"));
        assert!(output.header.contains("bool CanTransitionToWalk()"));
        assert!(output.header.contains("bool CanTransitionToRun()"));
    }
    
    #[test]
    fn test_generate_state_enum() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.header.contains("Idle UMETA(DisplayName = \"Idle\")"));
        assert!(output.header.contains("Walk UMETA(DisplayName = \"Walk\")"));
        assert!(output.header.contains("Run UMETA(DisplayName = \"Run\")"));
    }
    
    #[test]
    fn test_generate_animation_properties() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.header.contains("UAnimSequence* IdleAnimation"));
        assert!(output.header.contains("UAnimSequence* WalkAnimation"));
        assert!(output.header.contains("UAnimSequence* RunAnimation"));
    }
    
    #[test]
    fn test_generate_constructor() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.source.contains("UCharacterAnimationsStateMachine::UCharacterAnimationsStateMachine()"));
        assert!(output.source.contains("CurrentState = ECharacterAnimationsState::Idle"));
        assert!(output.source.contains("IdleAnimation = nullptr"));
    }
    
    #[test]
    fn test_generate_update_method() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.source.contains("void UCharacterAnimationsStateMachine::UpdateStateMachine(float DeltaTime)"));
        assert!(output.source.contains("switch (CurrentState)"));
        assert!(output.source.contains("case ECharacterAnimationsState::Idle:"));
        assert!(output.source.contains("if (CanTransitionToWalk())"));
        assert!(output.source.contains("TransitionToState(ECharacterAnimationsState::Walk)"));
    }
    
    #[test]
    fn test_generate_transition_methods() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.source.contains("bool UCharacterAnimationsStateMachine::CanTransitionToWalk() const"));
        assert!(output.source.contains("return Speed > 0.1f"));
        assert!(output.source.contains("bool UCharacterAnimationsStateMachine::CanTransitionToRun() const"));
        assert!(output.source.contains("return Speed > 5.0f"));
    }
    
    #[test]
    fn test_generate_get_current_animation() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.source.contains("UAnimSequence* UCharacterAnimationsStateMachine::GetCurrentAnimation() const"));
        assert!(output.source.contains("case ECharacterAnimationsState::Idle:"));
        assert!(output.source.contains("return IdleAnimation"));
    }
    
    #[test]
    fn test_generate_transition_to_state() {
        let ir = make_simple_state_machine();
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.source.contains("void UCharacterAnimationsStateMachine::TransitionToState(ECharacterAnimationsState NewState)"));
        assert!(output.source.contains("if (CurrentState == NewState)"));
        assert!(output.source.contains("CurrentState = NewState"));
    }
    
    #[test]
    fn test_generate_with_callbacks() {
        let mut ir = make_simple_state_machine();
        ir.states[0].on_enter = Some("UE_LOG(LogTemp, Log, TEXT(\"Entering Idle\"));".to_string());
        ir.states[0].on_exit = Some("UE_LOG(LogTemp, Log, TEXT(\"Exiting Idle\"));".to_string());
        
        let output = generate_state_machine_code(&ir, "TestPlugin");
        
        assert!(output.header.contains("void OnEnterIdle()"));
        assert!(output.header.contains("void OnExitIdle()"));
        assert!(output.source.contains("void UCharacterAnimationsStateMachine::OnEnterIdle()"));
        assert!(output.source.contains("void UCharacterAnimationsStateMachine::OnExitIdle()"));
        assert!(output.source.contains("UE_LOG(LogTemp, Log, TEXT(\"Entering Idle\"))"));
    }
}
