use kain_core::{Lexer, Parser, comptime, types};
use ue5::{generate_with_context_typed, Ue5Context};

#[test]
fn test_network_sync_interpolated_codegen() {
    let source = r#"
@component
struct NetworkedTransform:
    @replicated(mode: Interpolated, back_time: 0.1, buffer_size: 32)
    position: Vec3
    
    @replicated(mode: Interpolated, back_time: 0.1)
    rotation: Quat
"#;

    // Parse
    let tokens = Lexer::new(source).tokenize().expect("Failed to tokenize");
    let mut ast = Parser::new(&tokens).parse().expect("Failed to parse");
    
    // Comptime evaluation
    comptime::eval_program(&mut ast).expect("Failed comptime eval");
    
    // Type check
    let typed_program = types::check(&ast).expect("Failed to type check");
    
    // Generate code
    let ctx = Ue5Context::new("TestPlugin", None);
    let output = generate_with_context_typed(&typed_program, Some("TestPlugin"), None, &ctx)
        .expect("Failed to generate code");
    
    // Verify header contains network sync structures
    assert!(output.header.contains("struct FNetworkState"), "Header should contain FNetworkState struct");
    assert!(output.header.contains("TArray<FNetworkState> StateBuffer"), "Header should contain StateBuffer");
    assert!(output.header.contains("float InterpolationBackTime"), "Header should contain InterpolationBackTime");
    
    // Verify constructor initializes network sync
    assert!(output.source.contains("InterpolationBackTime = 0.1f"), "Constructor should set InterpolationBackTime");
    assert!(output.source.contains("StateBuffer.Reserve(32)"), "Constructor should reserve StateBuffer");
    assert!(output.source.contains("SetIsReplicatedByDefault(true)"), "Constructor should enable replication");
    
    // Verify tick method contains interpolation logic
    assert!(output.source.contains("Interpolation logic"), "Tick should contain interpolation logic");
    assert!(output.source.contains("FMath::Lerp"), "Tick should use FMath::Lerp for interpolation");
    assert!(output.source.contains("FQuat::Slerp"), "Tick should use FQuat::Slerp for rotation");
    
    // Verify GetLifetimeReplicatedProps is generated
    assert!(output.source.contains("GetLifetimeReplicatedProps"), "Should generate GetLifetimeReplicatedProps");
    assert!(output.source.contains("DOREPLIFETIME_CONDITION"), "Should use conditional replication");
    assert!(output.source.contains("COND_SimulatedOnly"), "Should replicate to simulated clients only");
}

#[test]
fn test_network_sync_compressed_codegen() {
    let source = r#"
@component
struct CompressedComponent:
    @replicated(mode: Compressed, threshold: 0.01, use_half_float: true)
    velocity: Vec3
"#;

    // Parse
    let tokens = Lexer::new(source).tokenize().expect("Failed to tokenize");
    let mut ast = Parser::new(&tokens).parse().expect("Failed to parse");
    comptime::eval_program(&mut ast).expect("Failed comptime eval");
    let typed_program = types::check(&ast).expect("Failed to type check");
    
    // Generate code
    let ctx = Ue5Context::new("TestPlugin", None);
    let output = generate_with_context_typed(&typed_program, Some("TestPlugin"), None, &ctx)
        .expect("Failed to generate code");
    
    // Verify replication setup
    assert!(output.source.contains("GetLifetimeReplicatedProps"), "Should generate GetLifetimeReplicatedProps");
    assert!(output.source.contains("DOREPLIFETIME_CONDITION"), "Should use conditional replication");
}

#[test]
fn test_network_sync_extrapolated_codegen() {
    let source = r#"
@component
struct PredictedMovement:
    @replicated(mode: Extrapolated, limit: 100.0)
    predicted_position: Vec3
"#;

    // Parse
    let tokens = Lexer::new(source).tokenize().expect("Failed to tokenize");
    let mut ast = Parser::new(&tokens).parse().expect("Failed to parse");
    comptime::eval_program(&mut ast).expect("Failed comptime eval");
    let typed_program = types::check(&ast).expect("Failed to type check");
    
    // Generate code
    let ctx = Ue5Context::new("TestPlugin", None);
    let output = generate_with_context_typed(&typed_program, Some("TestPlugin"), None, &ctx)
        .expect("Failed to generate code");
    
    // Verify extrapolation logic
    assert!(output.source.contains("Extrapolation logic"), "Tick should contain extrapolation logic");
    assert!(output.source.contains("GetLifetimeReplicatedProps"), "Should generate GetLifetimeReplicatedProps");
}

#[test]
fn test_network_sync_simple_replication() {
    let source = r#"
@component
struct SimpleComponent:
    @replicated
    health: Float
    
    @replicated(mode: Simple)
    score: Int
"#;

    // Parse
    let tokens = Lexer::new(source).tokenize().expect("Failed to tokenize");
    let mut ast = Parser::new(&tokens).parse().expect("Failed to parse");
    comptime::eval_program(&mut ast).expect("Failed comptime eval");
    let typed_program = types::check(&ast).expect("Failed to type check");
    
    // Generate code
    let ctx = Ue5Context::new("TestPlugin", None);
    let output = generate_with_context_typed(&typed_program, Some("TestPlugin"), None, &ctx)
        .expect("Failed to generate code");
    
    // Verify simple replication (no interpolation structures)
    assert!(!output.header.contains("struct FNetworkState"), "Header should NOT contain FNetworkState for simple replication");
    assert!(output.source.contains("GetLifetimeReplicatedProps"), "Should generate GetLifetimeReplicatedProps");
    assert!(output.source.contains("DOREPLIFETIME"), "Should use DOREPLIFETIME for simple replication");
}

#[test]
fn test_network_sync_snap_threshold() {
    let source = r#"
@component
@network_config(snap_threshold: 500.0)
struct TeleportableTransform:
    @replicated(mode: Interpolated, back_time: 0.1)
    position: Vec3
"#;

    // Parse
    let tokens = Lexer::new(source).tokenize().expect("Failed to tokenize");
    let mut ast = Parser::new(&tokens).parse().expect("Failed to parse");
    comptime::eval_program(&mut ast).expect("Failed comptime eval");
    let typed_program = types::check(&ast).expect("Failed to type check");
    
    // Generate code
    let ctx = Ue5Context::new("TestPlugin", None);
    let output = generate_with_context_typed(&typed_program, Some("TestPlugin"), None, &ctx)
        .expect("Failed to generate code");
    
    // Verify snap threshold logic
    assert!(output.source.contains("Snap threshold check"), "Tick should contain snap threshold check");
    assert!(output.source.contains("500"), "Tick should use configured snap threshold");
    assert!(output.source.contains("Teleportation detected"), "Tick should handle teleportation");
}
