// Integration tests for generic function codegen to UE5 C++
// Tests monomorphization → UE5 codegen pipeline

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

    // UE5 codegen (now accepts MonomorphizedProgram directly)
    let output = generate(&mono, None, None)?;

    Ok(output)
}

// ============================================================================
// A. FUNCTION INSTANTIATION TESTS
// ============================================================================

#[test]
fn test_generic_identity_function() {
    let source = r#"
fn identity<T>(x: T) -> T:
    return x

fn main():
    let a = identity(42)
    let b = identity(3.14)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify two instantiations exist
    assert!(
        cpp.contains("identity_Int"),
        "Should have identity_Int instantiation"
    );
    assert!(
        cpp.contains("identity_Float"),
        "Should have identity_Float instantiation"
    );

    // Verify no generic T in output
    assert!(
        !cpp.contains("<T>"),
        "Should not have generic type parameters in C++"
    );
    assert!(!cpp.contains("template"), "Should not have C++ templates");

    // Verify correct UE5 types (KAIN uses int64 for Int)
    assert!(cpp.contains("int64"), "Should use int64 for Int");
    assert!(cpp.contains("float"), "Should use float for Float");
}

#[test]
fn test_generic_max_function() {
    let source = r#"
fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    else:
        return b

fn run_max():
    let x = max(10, 20)
    let y = max(1.5, 2.5)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify instantiations
    assert!(cpp.contains("max_Int"), "Should have max_Int");
    assert!(cpp.contains("max_Float"), "Should have max_Float");

    // Note: Comparison operators may be generated as "/* complex if expr */"
    // This is a known limitation in the current codegen
}

#[test]
fn test_multiple_type_params() {
    let source = r#"
fn pair<T, U>(first: T, second: U) -> T:
    return first

fn run_pair():
    let x = pair(42, "hello")
    let y = pair(3.14, 100)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify multi-param instantiations
    assert!(
        cpp.contains("pair_Int_String") || cpp.contains("pair_Int_FString"),
        "Should have pair_Int_String instantiation"
    );
    assert!(
        cpp.contains("pair_Float_Int"),
        "Should have pair_Float_Int instantiation"
    );
}

#[test]
fn test_nested_generic_calls() {
    let source = r#"
fn identity<T>(x: T) -> T:
    return x

fn double_identity<T>(x: T) -> T:
    return identity(identity(x))

fn run_nested():
    let x = double_identity(42)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify nested calls work
    assert!(cpp.contains("identity_Int"), "Should have identity_Int");
    assert!(
        cpp.contains("double_identity_Int"),
        "Should have double_identity_Int"
    );

    // Note: Nested calls may create intermediate Any types
    // This is a known limitation that needs improvement
}

// ============================================================================
// B. TYPE MAPPING TESTS
// ============================================================================

#[test]
fn test_int_type_mapping() {
    let source = r#"
fn get_int<T>(x: T) -> T:
    return x

fn main():
    let a = get_int(42)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify Int → int64 mapping (KAIN uses int64 for Int)
    assert!(cpp.contains("int64"), "Int should map to int64");
    assert!(cpp.contains("get_int_Int"), "Should have mangled name");
}

#[test]
fn test_float_type_mapping() {
    let source = r#"
fn get_float<T>(x: T) -> T:
    return x

fn main():
    let a = get_float(3.14)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify Float → float mapping
    assert!(
        cpp.contains("float") && !cpp.contains("double"),
        "Float should map to float, not double"
    );
    assert!(cpp.contains("get_float_Float"), "Should have mangled name");
}

#[test]
fn test_string_type_mapping() {
    let source = r#"
fn get_string<T>(x: T) -> T:
    return x

fn main():
    let a = get_string("hello")
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify String → FString mapping
    assert!(cpp.contains("FString"), "String should map to FString");
    assert!(
        cpp.contains("get_string_String") || cpp.contains("get_string_FString"),
        "Should have mangled name"
    );
}

// ============================================================================
// C. NAME MANGLING TESTS
// ============================================================================

#[test]
fn test_single_param_mangling() {
    let source = r#"
fn identity<T>(x: T) -> T:
    return x

fn main():
    let a = identity(42)
    let b = identity(3.14)
    let c = identity("hello")
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify mangling pattern: identity_TypeName
    assert!(cpp.contains("identity_Int"), "Should have identity_Int");
    assert!(cpp.contains("identity_Float"), "Should have identity_Float");
    assert!(
        cpp.contains("identity_String") || cpp.contains("identity_FString"),
        "Should have identity_String"
    );
}

#[test]
fn test_multi_param_mangling() {
    let source = r#"
fn pair<T, U>(first: T, second: U) -> T:
    return first

fn main():
    let a = pair(42, "hello")
    let b = pair("world", 3.14)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify mangling pattern: pair_Type1_Type2
    assert!(
        cpp.contains("pair_Int_String") || cpp.contains("pair_Int_FString"),
        "Should have pair_Int_String"
    );
    assert!(
        cpp.contains("pair_String_Float") || cpp.contains("pair_FString_Float"),
        "Should have pair_String_Float"
    );
}

#[test]
fn test_no_collision() {
    let source = r#"
fn identity<T>(x: T) -> T:
    return x

fn main():
    let a = identity(42)
    let b = identity(3.14)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Count occurrences of each mangled name
    let int_count = cpp.matches("identity_Int").count();
    let float_count = cpp.matches("identity_Float").count();

    // Each should appear at least once (definition)
    assert!(int_count >= 1, "identity_Int should appear at least once");
    assert!(
        float_count >= 1,
        "identity_Float should appear at least once"
    );

    // Verify they are distinct functions
    assert!(
        cpp.contains("int64 identity_Int(const int64"),
        "Should have identity_Int signature"
    );
    assert!(
        cpp.contains("float identity_Float(const float"),
        "Should have identity_Float signature"
    );
}

// ============================================================================
// D. ADDITIONAL INTEGRATION TESTS
// ============================================================================

#[test]
fn test_generic_with_arithmetic() {
    let source = r#"
fn double<T>(x: T) -> T:
    return x + x

fn main():
    let a = double(21)
    let b = double(1.5)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify arithmetic operations work
    assert!(cpp.contains("double_Int"), "Should have double_Int");
    assert!(cpp.contains("double_Float"), "Should have double_Float");
    assert!(cpp.contains("+"), "Should have addition operator");
}

#[test]
fn test_generic_abs_function() {
    let source = r#"
fn abs<T>(x: T) -> T:
    if x < 0:
        return -x
    return x

fn main():
    let int_val = -42
    let float_val = -3.14
    let a = abs(int_val)
    let b = abs(float_val)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify abs works with comparison and negation
    assert!(
        cpp.contains("abs_Int") || cpp.contains("abs_Any"),
        "Should have abs function (may be abs_Any due to literal type inference)"
    );
    // Note: Comparison operators are now properly generated!
    assert!(
        cpp.contains("if ((x < 0))") || cpp.contains("if ((x <"),
        "Should have comparison in if statement"
    );
}

#[test]
fn test_generic_clamp_function() {
    let source = r#"
fn min<T>(a: T, b: T) -> T:
    if a < b:
        return a
    return b

fn max<T>(a: T, b: T) -> T:
    if a > b:
        return a
    return b

fn clamp<T>(x: T, lo: T, hi: T) -> T:
    return min(max(x, lo), hi)

fn main():
    let a = clamp(150, 0, 100)
"#;

    let output = compile_ue5(source).unwrap();
    let cpp = &output.source;

    println!("Generated C++:\n{}", cpp);

    // Verify complex generic composition
    assert!(cpp.contains("max_Int"), "Should have max_Int");
    assert!(cpp.contains("clamp_Int"), "Should have clamp_Int");

    // Note: Nested generic calls may create intermediate Any types (min_Any)
    // This is a known limitation - the monomorphizer doesn't fully propagate
    // type information through nested calls yet
    assert!(
        cpp.contains("min_Any") || cpp.contains("min_Int"),
        "Should have min function (may be min_Any due to nested call limitation)"
    );

    // Verify nested calls are resolved
    assert!(
        cpp.contains("min_") && cpp.contains("max_Int("),
        "Should have nested min/max calls"
    );
}

// ============================================================================
// G. @SUBSYSTEM CODEGEN TESTS
// ============================================================================

#[test]
fn test_subsystem_basic_generation() {
    let source = r#"
@subsystem
struct TickOptimizer:
    frame_budget: Float
    enabled: Bool
"#;

    let output = compile_ue5(source).unwrap();
    let header = &output.header;
    let source_cpp = &output.source;

    // Class name should be UTickOptimizerSubsystem
    assert!(
        header.contains("UTickOptimizerSubsystem"),
        "Should generate UTickOptimizerSubsystem class. Header:\n{}",
        header
    );
    assert!(
        header.contains("UWorldSubsystem"),
        "Should inherit from UWorldSubsystem. Header:\n{}",
        header
    );
    assert!(
        header.contains("UCLASS()"),
        "Should have UCLASS(). Header:\n{}",
        header
    );
    assert!(
        header.contains("GENERATED_BODY()"),
        "Should have GENERATED_BODY(). Header:\n{}",
        header
    );

    // Lifecycle methods
    assert!(
        header.contains("Initialize(FSubsystemCollectionBase& Collection)"),
        "Should declare Initialize. Header:\n{}",
        header
    );
    assert!(
        header.contains("Deinitialize()"),
        "Should declare Deinitialize. Header:\n{}",
        header
    );
    assert!(
        header.contains("ShouldCreateSubsystem(UObject* Outer)"),
        "Should declare ShouldCreateSubsystem. Header:\n{}",
        header
    );

    // Source implementations
    assert!(
        source_cpp.contains("Super::Initialize(Collection)"),
        "Should call Super::Initialize. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("Super::Deinitialize()"),
        "Should call Super::Deinitialize. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("return true"),
        "ShouldCreateSubsystem should return true. Source:\n{}",
        source_cpp
    );

    // Should NOT have tick interface
    assert!(
        !header.contains("FTickableGameObject"),
        "Should NOT have FTickableGameObject without @tick. Header:\n{}",
        header
    );
}

#[test]
fn test_subsystem_with_tick() {
    let source = r#"
@subsystem
@tick
struct FrameProfiler:
    budget_ms: Float
"#;

    let output = compile_ue5(source).unwrap();
    let header = &output.header;
    let source_cpp = &output.source;

    assert!(
        header.contains("UFrameProfilerSubsystem"),
        "Should generate UFrameProfilerSubsystem. Header:\n{}",
        header
    );
    assert!(
        header.contains("FTickableGameObject"),
        "Should inherit FTickableGameObject with @tick. Header:\n{}",
        header
    );
    assert!(
        header.contains("virtual void Tick(float DeltaTime) override"),
        "Should declare Tick. Header:\n{}",
        header
    );
    assert!(
        header.contains("GetStatId"),
        "Should declare GetStatId. Header:\n{}",
        header
    );
    assert!(
        header.contains("IsTickable"),
        "Should declare IsTickable. Header:\n{}",
        header
    );

    // Source should have tick implementations
    assert!(
        source_cpp.contains("RETURN_QUICK_DECLARE_CYCLE_STAT"),
        "Should have stat declaration. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("IsTickable"),
        "Should implement IsTickable. Source:\n{}",
        source_cpp
    );
}

// ============================================================================
// COMPONENT LIFECYCLE TESTS
// ============================================================================

#[test]
fn test_component_with_tick() {
    let source = r#"
@component
@tick
struct MovementComponent:
    speed: Float
    direction: Vec3
"#;
    let output = compile_ue5(source).unwrap();
    let header = &output.header;
    let source_cpp = &output.source;

    // Header should have TickComponent declaration
    assert!(header.contains("virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override"),
        "Should declare TickComponent. Header:\n{}", header);

    // Constructor should enable ticking
    assert!(
        source_cpp.contains("PrimaryComponentTick.bCanEverTick = true"),
        "Should enable ticking in constructor. Source:\n{}",
        source_cpp
    );

    // Source should implement TickComponent
    assert!(
        source_cpp.contains("UMovementComponent::TickComponent"),
        "Should implement TickComponent. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("Super::TickComponent"),
        "Should call Super::TickComponent. Source:\n{}",
        source_cpp
    );
}

#[test]
fn test_component_with_beginplay() {
    let source = r#"
@component
@beginplay
struct InitComponent:
    is_initialized: Bool
"#;
    let output = compile_ue5(source).unwrap();
    let header = &output.header;
    let source_cpp = &output.source;

    // Header should have BeginPlay declaration
    assert!(
        header.contains("virtual void BeginPlay() override"),
        "Should declare BeginPlay. Header:\n{}",
        header
    );

    // Source should implement BeginPlay
    assert!(
        source_cpp.contains("UInitComponent::BeginPlay()"),
        "Should implement BeginPlay. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("Super::BeginPlay()"),
        "Should call Super::BeginPlay. Source:\n{}",
        source_cpp
    );
}

#[test]
fn test_component_with_both_lifecycle_methods() {
    let source = r#"
@component
@tick
@beginplay
struct FullLifecycleComponent:
    value: Float
"#;
    let output = compile_ue5(source).unwrap();
    let header = &output.header;
    let source_cpp = &output.source;

    // Should have both declarations
    assert!(
        header.contains("virtual void BeginPlay() override"),
        "Should declare BeginPlay. Header:\n{}",
        header
    );
    assert!(
        header.contains("virtual void TickComponent"),
        "Should declare TickComponent. Header:\n{}",
        header
    );

    // Should have both implementations
    assert!(
        source_cpp.contains("UFullLifecycleComponent::BeginPlay()"),
        "Should implement BeginPlay. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("UFullLifecycleComponent::TickComponent"),
        "Should implement TickComponent. Source:\n{}",
        source_cpp
    );

    // Should enable ticking
    assert!(
        source_cpp.contains("PrimaryComponentTick.bCanEverTick = true"),
        "Should enable ticking. Source:\n{}",
        source_cpp
    );

    // CRITICAL: No TODOs in generated code
    assert!(
        !source_cpp.contains("TODO"),
        "Generated code must not contain TODO stubs. Source:\n{}",
        source_cpp
    );
}

#[test]
fn test_component_lifecycle_with_impl_body() {
    let source = r#"
@component
@tick
@beginplay
struct PhysicsComponent:
    velocity: Vec3
    gravity: Float

impl PhysicsComponent:
    fn begin_play(self):
        gravity = 9.81

    fn tick(self, dt: Float):
        velocity = velocity + vec3(0.0, 0.0, gravity) * dt
"#;
    let output = compile_ue5(source).unwrap();
    let source_cpp = &output.source;

    // BeginPlay should contain the user's initialization code
    assert!(
        source_cpp.contains("Super::BeginPlay()"),
        "Should call Super::BeginPlay. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("9.81"),
        "BeginPlay should contain gravity initialization from impl block. Source:\n{}",
        source_cpp
    );

    // TickComponent should contain the user's physics code
    assert!(
        source_cpp.contains("Super::TickComponent"),
        "Should call Super::TickComponent. Source:\n{}",
        source_cpp
    );

    // CRITICAL: No TODOs in generated code
    assert!(
        !source_cpp.contains("TODO"),
        "Generated code must not contain TODO stubs. Source:\n{}",
        source_cpp
    );
}

#[test]
fn test_component_no_todo_without_impl() {
    let source = r#"
@component
@tick
@beginplay
struct EmptyLifecycleComponent:
    value: Float
"#;
    let output = compile_ue5(source).unwrap();
    let source_cpp = &output.source;

    // Even without impl blocks, no TODOs should appear
    assert!(
        !source_cpp.contains("TODO"),
        "Generated code must never contain TODO stubs, even without impl blocks. Source:\n{}",
        source_cpp
    );

    // Should still have structurally complete Super calls
    assert!(
        source_cpp.contains("Super::BeginPlay()"),
        "Should call Super::BeginPlay even without impl body. Source:\n{}",
        source_cpp
    );
    assert!(
        source_cpp.contains("Super::TickComponent"),
        "Should call Super::TickComponent even without impl body. Source:\n{}",
        source_cpp
    );
}
