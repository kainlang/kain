// Integration tests for member access codegen (-> vs . operator selection)
// Tests that UObject-derived types use -> and value types use .

use kain_core::*;
use ue5::{generate, Ue5Output};

/// Helper: Parse, typecheck, monomorphize, and generate UE5 C++
fn compile_ue5(source: &str) -> Result<Ue5Output, error::KainError> {
    // Parse
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = kain_core::diagnostics::SpanMapper::new(source);
    let mut ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;

    // Compile-time evaluation
    comptime::eval_program(&mut ast)?;

    // Type checking
    let typed = types::check(&ast, &span_mapper, "<test>")?;

    // Monomorphization
    let mono = monomorphize::monomorphize(&typed)?;

    // UE5 codegen
    let output = generate(&mono, None, None)?;

    Ok(output)
}

// ============================================================================
// A. VALUE TYPE MEMBER ACCESS (should use .)
// ============================================================================

#[test]
fn test_vector_member_access_uses_dot() {
    let source = r#"
struct Transform:
    position: Vec3
    rotation: Vec3

fn get_x(t: Transform) -> Float:
    return t.position.x
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Vec3 (FVector) is a value type, should use . not ->
    assert!(
        cpp.contains("t.position.X"),
        "Should use dot notation for FVector field access"
    );
    assert!(
        !cpp.contains("t.position->X"),
        "Should NOT use pointer notation for FVector"
    );
}

#[test]
fn test_primitive_struct_member_access() {
    let source = r#"
struct Stats:
    health: Float
    mana: Float

fn get_health(s: Stats) -> Float:
    return s.health
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Primitive fields in structs should use dot notation
    assert!(
        cpp.contains("s.health"),
        "Should use dot notation for primitive field"
    );
    assert!(
        !cpp.contains("s->health"),
        "Should NOT use pointer notation for struct field"
    );
}

#[test]
fn test_nested_value_type_access() {
    let source = r#"
struct Inner:
    value: Float

struct Outer:
    inner: Inner

fn get_value(o: Outer) -> Float:
    return o.inner.value
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Both levels should use dot notation (value types)
    assert!(
        cpp.contains("o.inner.value"),
        "Should use dot notation for nested value types"
    );
    assert!(
        !cpp.contains("->"),
        "Should NOT use pointer notation anywhere"
    );
}

// ============================================================================
// B. UOBJECT-DERIVED TYPE MEMBER ACCESS (should use ->)
// ============================================================================

#[test]
fn test_component_member_access_uses_arrow() {
    let source = r#"
@component
struct HealthComponent:
    current: Float = 0.0
    max: Float = 100.0

actor Player:
    state health: HealthComponent = HealthComponent()

    fn get_health() -> Float:
        return self.health.current
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // HealthComponent is UObject-derived, should use ->
    assert!(
        cpp.contains("health->current") || cpp.contains("this->health->current"),
        "Should use arrow notation for component field access"
    );
}

#[test]
fn test_actor_reference_member_access() {
    let source = r#"
actor Enemy:
    state health: Float = 100.0

actor Player:
    state target: Enemy = Enemy()

    fn get_target_health() -> Float:
        return self.target.health
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Enemy is an actor (AActor-derived), should use ->
    assert!(
        cpp.contains("target->health") || cpp.contains("this->target->health"),
        "Should use arrow notation for actor field access"
    );
}

#[test]
fn test_subsystem_member_access() {
    let source = r#"
@subsystem
struct GameManager:
    score: Int

fn get_score(manager: GameManager) -> Int:
    return manager.score
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Subsystems are UObject-derived, should use ->
    assert!(
        cpp.contains("manager->score"),
        "Should use arrow notation for subsystem field access"
    );
}

// ============================================================================
// C. MIXED TYPE MEMBER ACCESS
// ============================================================================

#[test]
fn test_component_with_value_type_field() {
    let source = r#"
@component
struct TransformComponent:
    position: Vec3 = vec3(0.0, 0.0, 0.0)
    rotation: Vec3 = vec3(0.0, 0.0, 0.0)

actor Player:
    state transform: TransformComponent = TransformComponent()

    fn get_x() -> Float:
        return self.transform.position.x
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // transform is a component (pointer), position is Vec3 (value)
    // Should be: transform->position.X
    assert!(
        cpp.contains("transform->position.X") || cpp.contains("this->transform->position.X"),
        "Should use arrow for component, dot for Vec3"
    );
}

#[test]
fn test_struct_with_component_field() {
    let source = r#"
@component
struct HealthComponent:
    current: Float

struct PlayerData:
    health: HealthComponent
    name: String

fn get_health(data: PlayerData) -> Float:
    return data.health.current
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // data is a struct (value), health is a component (pointer)
    // Should be: data.health->current
    assert!(
        cpp.contains("data.health->current"),
        "Should use dot for struct, arrow for component"
    );
}

#[test]
fn test_deep_nesting_mixed_types() {
    let source = r#"
struct Vec3Wrapper:
    vec: Vec3 = vec3(0.0, 0.0, 0.0)

@component
struct TransformComponent:
    position: Vec3Wrapper = Vec3Wrapper()

actor Player:
    state transform: TransformComponent = TransformComponent()

    fn get_x() -> Float:
        return self.transform.position.vec.x
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // transform (component, pointer) -> position (struct, value) . vec (Vec3, value) . x
    // Should be: transform->position.vec.X
    assert!(
        cpp.contains("transform->position.vec.X")
            || cpp.contains("this->transform->position.vec.X"),
        "Should correctly alternate between arrow and dot based on type"
    );
}

// ============================================================================
// D. ENGINE TYPE MEMBER ACCESS
// ============================================================================

#[test]
fn test_engine_uobject_types() {
    let source = r#"
actor Player:
    state mesh: UStaticMeshComponent = UStaticMeshComponent()
    state my_texture: UTexture2D = UTexture2D()

    fn setup():
        self.mesh.SetVisibility(true)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // UStaticMeshComponent and UTexture2D are UObject-derived, should use ->
    assert!(
        cpp.contains("mesh->SetVisibility") || cpp.contains("this->mesh->SetVisibility"),
        "Should use arrow notation for engine UObject types"
    );
}

#[test]
fn test_engine_value_types() {
    let source = r#"
struct Transform:
    location: Vec3 = vec3(0.0, 0.0, 0.0)
    rotation: Vec3 = vec3(0.0, 0.0, 0.0)

fn get_x(t: Transform) -> Float:
    return t.location.x
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Vec3 (FVector) is a value type, should use .
    assert!(
        cpp.contains("t.location.X"),
        "Should use dot notation for FVector"
    );
    assert!(
        !cpp.contains("t.location->X"),
        "Should NOT use arrow for FVector"
    );
}

// ============================================================================
// E. SELF MEMBER ACCESS
// ============================================================================

#[test]
fn test_self_value_field_access() {
    let source = r#"
actor Player:
    state health: Float = 100.0
    state position: Vec3 = vec3(0.0, 0.0, 0.0)

    fn get_health() -> Float:
        return self.health
    
    fn get_x() -> Float:
        return self.position.x
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // self.health and self.position are value types, should use ->
    // (because self is a pointer in UE5 actors)
    assert!(
        cpp.contains("this->health") || cpp.contains("health"),
        "Should access actor state fields"
    );
    assert!(
        cpp.contains("this->position") || cpp.contains("position.X"),
        "Should access Vec3 state field"
    );
}

#[test]
fn test_self_component_field_access() {
    let source = r#"
@component
struct HealthComponent:
    current: Float = 0.0

actor Player:
    state health: HealthComponent = HealthComponent()

    fn get_health() -> Float:
        return self.health.current
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // self.health is a component (pointer), should use ->
    assert!(
        cpp.contains("health->current") || cpp.contains("this->health->current"),
        "Should use arrow for component field"
    );
}

// ============================================================================
// F. ARRAY AND COLLECTION MEMBER ACCESS
// ============================================================================

#[test]
fn test_array_length_access() {
    let source = r#"
fn get_length(arr: Array<Int>) -> Int:
    return arr.length
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Array length should be mapped to .Num()
    assert!(
        cpp.contains("arr.Num()"),
        "Should map array.length to .Num()"
    );
}

#[test]
fn test_component_array_access() {
    let source = r#"
@component
struct HealthComponent:
    current: Float = 0.0

actor Player:
    state components: Array<HealthComponent> = Array()

    fn count() -> Int:
        return self.components.length
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Array is a value type, should use .Num()
    assert!(
        cpp.contains("components.Num()") || cpp.contains("this->components.Num()"),
        "Should use dot notation for array method"
    );
}
