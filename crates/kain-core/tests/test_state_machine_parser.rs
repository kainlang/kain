use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::ast::Item;
use kain_core::diagnostics::SpanMapper;

#[test]
fn test_parse_state_machine_basic() {
    let source = r#"
@state_machine
struct CharacterAnimations:
    @state(entry: true)
    struct Idle:
        animation: "Idle_Anim"
        
        @transition(to: "Walk")
        fn can_walk() -> Bool:
            return speed > 0.1
    
    @state
    struct Walk:
        animation: "Walk_Anim"
        
        @transition(to: "Run")
        fn can_run() -> Bool:
            return speed > 5.0
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::StateMachine(sm_def) => {
            assert_eq!(sm_def.name, "CharacterAnimations");
            assert_eq!(sm_def.states.len(), 2);
            
            // Check first state (Idle)
            let idle_state = &sm_def.states[0];
            assert_eq!(idle_state.name, "Idle");
            assert!(idle_state.is_entry);
            assert_eq!(idle_state.animation, Some("Idle_Anim".to_string()));
            assert_eq!(idle_state.transitions.len(), 1);
            assert_eq!(idle_state.transitions[0].to_state, "Walk");
            
            // Check second state (Walk)
            let walk_state = &sm_def.states[1];
            assert_eq!(walk_state.name, "Walk");
            assert!(!walk_state.is_entry);
            assert_eq!(walk_state.animation, Some("Walk_Anim".to_string()));
            assert_eq!(walk_state.transitions.len(), 1);
            assert_eq!(walk_state.transitions[0].to_state, "Run");
        }
        _ => panic!("Expected StateMachine item"),
    }
}

#[test]
fn test_parse_state_machine_with_properties() {
    let source = r#"
@state_machine
struct CombatStateMachine:
    @state(entry: true)
    struct Idle:
        animation: "Idle"
        timeout: Float = 5.0
        
        @transition(to: "Attack")
        fn should_attack() -> Bool:
            return has_target
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    match &program.items[0] {
        Item::StateMachine(sm_def) => {
            assert_eq!(sm_def.name, "CombatStateMachine");
            let idle_state = &sm_def.states[0];
            assert_eq!(idle_state.properties.len(), 1);
            assert_eq!(idle_state.properties[0].name, "timeout");
        }
        _ => panic!("Expected StateMachine item"),
    }
}

#[test]
fn test_parse_state_machine_multiple_transitions() {
    let source = r#"
@state_machine
struct MovementStateMachine:
    @state(entry: true)
    struct Idle:
        animation: "Idle"
        
        @transition(to: "Walk")
        fn can_walk() -> Bool:
            return speed > 0.1
        
        @transition(to: "Jump")
        fn can_jump() -> Bool:
            return jump_pressed
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    match &program.items[0] {
        Item::StateMachine(sm_def) => {
            let idle_state = &sm_def.states[0];
            assert_eq!(idle_state.transitions.len(), 2);
            assert_eq!(idle_state.transitions[0].to_state, "Walk");
            assert_eq!(idle_state.transitions[1].to_state, "Jump");
        }
        _ => panic!("Expected StateMachine item"),
    }
}
