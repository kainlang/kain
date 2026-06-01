//! UE5 C++ Code Generation - Transpiles KAIN AST to Unreal Engine 5 C++
//!
//! Generates UCLASS, USTRUCT, UPROPERTY, UFUNCTION annotated code.
//! Auto-prefixes based on UE5 naming conventions:
//! - Actors get 'A' prefix (actor Player -> APlayer)
//! - Structs get 'F' prefix (struct Transform -> FTransform)
//! - UObjects get 'U' prefix
//!
//! ## Standard Library Support
//! Maps KAIN stdlib functions to UE5 equivalents via `StdLibResolver`:
//! - Math functions: abs, sqrt, sin, cos, min, max → FMath::Abs, FMath::Sqrt, etc.
//! - Vector functions: vec3, dot, cross, normalize → FVector constructors and methods
//! - Collection functions: len, push, pop → TArray methods
//! - String functions: split, join, trim → FString methods
//!
//! Compile with: kain file.kn -t ue5 -o Output.generated.h

#![allow(dead_code, unused_assignments, unused_mut, unused_variables)]

use kain_core::ast::{
    Actor, BinaryOp, Block, ElseBranch, Enum, EnumVariantFields, Expr, Field, Function, Impl,
    MessageHandler, Param, Pattern, ShaderStage, Stmt, Struct, Type, UnaryOp, VariantFields,
};
use kain_core::error::KainResult;
use kain_core::types::{TypedItem, TypedShader};
use kain_core::{
    lower_monomorphized_program_memory_for_target, lower_typed_program_memory_for_target,
    validate_typed_program_memory_support, CompileTarget,
};
use kain_core::{MonomorphizedProgram, TypedProgram};
use std::collections::HashSet;

// Import the UE5 support library
use crate::network_sync_codegen::generate_network_sync_code;
use crate::network_sync_ir::convert_to_network_sync_ir;
use crate::ue5::{Ue5Context, TEMPLATES};
use serde_json::json;

// Trait to abstract over TypedProgram and MonomorphizedProgram
trait ProgramItems {
    fn items(&self) -> &[TypedItem];
}

impl ProgramItems for TypedProgram {
    fn items(&self) -> &[TypedItem] {
        &self.items
    }
}

impl ProgramItems for MonomorphizedProgram {
    fn items(&self) -> &[TypedItem] {
        &self.items
    }
}
use crate::ue5::{
    escape_string as ue5_escape_string, get_ue_log_format_spec, to_actor_name, to_component_name,
    to_enum_name, to_pascal_case, to_struct_name, to_subsystem_name, to_uobject_name,
};
use std::time::{SystemTime, UNIX_EPOCH};

/// Module name for API export macro (derive from KAIN.toml or use default)
const DEFAULT_MODULE_API: &str = "GAME_API";

/// Output from UE5 codegen - contains header, source, and optional shader files
pub struct Ue5Output {
    pub header: String,
    pub source: String,
    pub shader_files: Vec<(String, String)>, // Vec<(filename, content)>
}

/// Generate UE5 C++ code from a typed program (legacy, for packager)
/// Returns separate .h and .cpp file contents, plus any shader files
///
/// # Note
/// This is a compatibility function for the packager which still works with TypedProgram.
/// New code should use `generate()` which accepts MonomorphizedProgram.
pub fn generate_from_typed(
    program: &TypedProgram,
    output_name: Option<&str>,
    copyright: Option<&str>,
) -> KainResult<Ue5Output> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Ue5)?;
    validate_typed_program_memory_support(&lowered, CompileTarget::Ue5)?;
    let module_name = output_name.unwrap_or("Kain");
    generate_filtered_typed(
        &lowered,
        module_name,
        output_name,
        None,
        copyright,
        std::collections::HashMap::new(),
        None,
        false,
        false,
    )
}

/// Generate UE5 C++ code from a monomorphized program
/// Returns separate .h and .cpp file contents, plus any shader files
///
/// # Arguments
/// * `program` - Monomorphized program with all generic functions instantiated
/// * `output_name` - Optional module name (defaults to "Kain")
/// * `copyright` - Optional copyright header text
///
/// # Note
/// This function expects a `MonomorphizedProgram` where all generic types have been
/// resolved to concrete types. Generic functions like `identity<T>` will have been
/// instantiated as `identity_Int`, `identity_Float`, etc.
pub fn generate(
    program: &MonomorphizedProgram,
    output_name: Option<&str>,
    copyright: Option<&str>,
) -> KainResult<Ue5Output> {
    let lowered = lower_monomorphized_program_memory_for_target(program, CompileTarget::Ue5)?;
    validate_typed_program_memory_support(
        &TypedProgram {
            items: lowered.items.clone(),
        },
        CompileTarget::Ue5,
    )?;
    let module_name = output_name.unwrap_or("Kain");
    generate_filtered(
        &lowered,
        module_name,
        output_name,
        None,
        copyright,
        std::collections::HashMap::new(),
        None,
    )
}

/// Generate UE5 C++ code with a pre-configured context (includes metadata)
/// This is used by the CLI when compiling with `-t ue5` to enable full pipeline features
///
/// # Arguments
/// * `program` - Monomorphized program with all generic functions instantiated
/// * `output_name` - Optional module name
/// * `copyright` - Optional copyright header text
/// * `context` - Pre-configured Ue5Context with EngineKnowledge and type registry
///
/// # Note
/// This is the preferred entry point when you have a pre-built Ue5Context with
/// metadata loaded. The context includes EngineKnowledge for type resolution.
pub fn generate_with_context(
    program: &MonomorphizedProgram,
    output_name: Option<&str>,
    copyright: Option<&str>,
    context: &Ue5Context,
) -> KainResult<Ue5Output> {
    let lowered = lower_monomorphized_program_memory_for_target(program, CompileTarget::Ue5)?;
    validate_typed_program_memory_support(
        &TypedProgram {
            items: lowered.items.clone(),
        },
        CompileTarget::Ue5,
    )?;
    let module_name = output_name.unwrap_or("Kain");
    let mut gen = Ue5Gen::new(
        module_name,
        output_name,
        copyright,
        None,
        std::collections::HashMap::new(),
    );

    // Use the provided context instead of creating a new one
    gen.context = context.clone();

    // PRE-PASS: Register all types
    for item in lowered.items() {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            }
            kain_core::types::TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");

                if is_component {
                    let prefixed_name = to_component_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else if is_subsystem {
                    let prefixed_name = to_subsystem_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.context.register_subsystem(st.ast.name.clone(), header);
                    gen.type_mapper.register_subsystem(st.ast.name.clone());
                } else {
                    let prefixed_name = to_struct_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            }
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            }
            _ => {}
        }
    }

    Ok(gen.gen_program(&lowered))
}

/// Legacy version for TypedProgram (packager compatibility)
pub fn generate_with_context_typed(
    program: &TypedProgram,
    output_name: Option<&str>,
    copyright: Option<&str>,
    context: &Ue5Context,
) -> KainResult<Ue5Output> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Ue5)?;
    let module_name = output_name.unwrap_or("Kain");
    let mut gen = Ue5Gen::new(
        module_name,
        output_name,
        copyright,
        None,
        std::collections::HashMap::new(),
    );

    // Use the provided context instead of creating a new one
    gen.context = context.clone();

    // PRE-PASS: Register all types
    for item in &lowered.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            }
            kain_core::types::TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");

                if is_component {
                    let prefixed_name = to_component_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else if is_subsystem {
                    let prefixed_name = to_subsystem_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.context.register_subsystem(st.ast.name.clone(), header);
                    gen.type_mapper.register_subsystem(st.ast.name.clone());
                } else {
                    let prefixed_name = to_struct_name(&st.ast.name);
                    let header = format!("{}.h", prefixed_name);
                    gen.context
                        .register_struct(st.ast.name.clone(), header.clone());
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            }
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            }
            kain_core::types::TypedItem::TypeAlias(a) => {
                // Delegates use F prefix like structs
                let prefixed_name = to_struct_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            }
            _ => {}
        }
    }

    Ok(gen.gen_program(&lowered))
}

/// Generate UE5 C++ code limited to a specific item
///
/// # Arguments
/// * `program` - Monomorphized program with concrete types only
/// * `module_name` - Plugin name for API macro (e.g., "UltimateTest")
/// * `output_name` - Optional output file name
/// * `filter_item` - Optional item name to generate (None = all items)
/// * `copyright` - Optional copyright header
/// * `type_to_header` - Map of type names to their header files
/// * `context` - Optional pre-configured Ue5Context
///
/// # Returns
/// Ue5Output with header, source, and shader files
///
/// # Note
/// This is the main internal codegen function. It receives a MonomorphizedProgram
/// where all generic functions have been instantiated with concrete types.
pub fn generate_filtered(
    program: &MonomorphizedProgram,
    module_name: &str,         // Plugin name for API macro (e.g., "UltimateTest")
    output_name: Option<&str>, // Per-item name for file naming (e.g., "AMaterialTestActor")
    target_item: Option<String>,
    copyright: Option<&str>,
    type_to_header: std::collections::HashMap<String, String>,
    shader_file_names: Option<Vec<String>>, // Shader file names from toml (without underscores)
) -> KainResult<Ue5Output> {
    let mut gen = Ue5Gen::new(
        module_name,
        output_name,
        copyright,
        target_item,
        type_to_header.clone(),
    );
    gen.shader_file_names = shader_file_names.unwrap_or_default();

    // PRE-PASS: Register all types so type lookups work during codegen
    // This is CRITICAL for modular output so each file knows about other types (e.g. if it's a delegate)
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header
                    .get(&en.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            }
            kain_core::types::TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");
                let prefixed_name = if is_subsystem {
                    to_subsystem_name(&st.ast.name)
                } else {
                    to_struct_name(&st.ast.name)
                };
                let header = type_to_header
                    .get(&st.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context
                    .register_struct(st.ast.name.clone(), header.clone());
                if is_component {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else if is_subsystem {
                    gen.context.register_subsystem(st.ast.name.clone(), header);
                    gen.type_mapper.register_subsystem(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header
                    .get(&a.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            }
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = type_to_header
                    .get(&c.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            }
            kain_core::types::TypedItem::TypeAlias(a) => {
                let prefixed_name = to_struct_name(&a.ast.name);
                let header = type_to_header
                    .get(&a.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            }
            // Traits are filtered out during type checking - should not appear here
            kain_core::types::TypedItem::Impl(impl_block) => {
                // Register trait implementations for interface inheritance
                if let Some(ref trait_name) = impl_block.ast.trait_name {
                    if let kain_core::ast::Type::Named {
                        name: class_name, ..
                    } = &impl_block.ast.target_type
                    {
                        gen.context.register_trait_impl(class_name, trait_name);
                    }
                }
            }
            _ => {}
        }
    }

    Ok(gen.gen_program(program))
}

/// Legacy function for compatibility with TypedProgram (packager)
///
/// # Note
/// Generate UE5 C++ code from a typed program with optional filtering and KAIN source markers.
///
/// This is for the packager which still works with TypedProgram.
/// The packager converts MonomorphizedProgram back to TypedProgram after monomorphization.
///
/// # Parameters
/// * `program` - The typed program to generate code from
/// * `module_name` - Name of the UE5 module
/// * `output_name` - Optional output name override
/// * `target_item` - Optional specific item to generate (for modular output)
/// * `copyright` - Optional copyright header
/// * `type_to_header` - Map of type names to their header files
/// * `shader_file_names` - Optional list of shader file names
/// * `embed_kain` - If true, embeds original KAIN source as comments in generated C++ for round-trip compilation
///
/// # Round-Trip Compilation
/// When `embed_kain` is true, the generated C++ includes KAIN source markers:
/// ```cpp
/// // KAIN_BEGIN: actor Player
/// // KAIN: actor Player:
/// // KAIN:     state health: Float = 100.0
/// class APlayer : public AActor { ... }
/// // KAIN_END: actor Player
/// ```
///
/// These markers enable:
/// - Extracting KAIN source from compiled plugins (`cpp_to_kain.py`)
/// - Validating codegen determinism (round-trip testing)
/// - Generating LLM training examples from working plugins
/// - Debugging what KAIN generated for specific C++ patterns
///
/// See `Kain/scripts/docs/ROUND_TRIP_README.md` for usage details.
pub fn generate_filtered_typed(
    program: &TypedProgram,
    module_name: &str,
    output_name: Option<&str>,
    target_item: Option<String>,
    copyright: Option<&str>,
    type_to_header: std::collections::HashMap<String, String>,
    shader_file_names: Option<Vec<String>>,
    embed_kain: bool,
    has_gas_features: bool,
) -> KainResult<Ue5Output> {
    let mut gen = Ue5Gen::new(
        module_name,
        output_name,
        copyright,
        target_item,
        type_to_header.clone(),
    );
    gen.shader_file_names = shader_file_names.unwrap_or_default();
    gen.has_gas_features = has_gas_features;

    // Enable KAIN markers if requested
    if embed_kain {
        gen.context.enable_markers(crate::ue5::MarkerStyle::Block);
    }

    // PRE-PASS: Register all types
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header
                    .get(&en.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            }
            kain_core::types::TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");
                let prefixed_name = if is_subsystem {
                    to_subsystem_name(&st.ast.name)
                } else {
                    to_struct_name(&st.ast.name)
                };
                let header = type_to_header
                    .get(&st.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context
                    .register_struct(st.ast.name.clone(), header.clone());
                if is_component {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else if is_subsystem {
                    gen.context.register_subsystem(st.ast.name.clone(), header);
                    gen.type_mapper.register_subsystem(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header
                    .get(&a.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            }
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = type_to_header
                    .get(&c.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            }
            kain_core::types::TypedItem::TypeAlias(a) => {
                let prefixed_name = to_struct_name(&a.ast.name); // Delegates use F prefix
                let header = type_to_header
                    .get(&a.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            }
            _ => {}
        }
    }

    // Convert TypedProgram to MonomorphizedProgram for gen_program
    Ok(
        gen.gen_program(&kain_core::monomorphize::MonomorphizedProgram {
            items: program.items.clone(),
        }),
    )
}
///
/// # Deprecated
/// This function still accepts `TypedProgram` for backward compatibility.
/// New code should use `generate()` with a `MonomorphizedProgram` instead.
///
/// # Note
/// This function internally monomorphizes the program before codegen.
/// Generic functions will be instantiated automatically.
pub fn generate_combined(program: &TypedProgram, copyright: Option<&str>) -> KainResult<String> {
    // Convert TypedProgram to MonomorphizedProgram
    use kain_core::monomorphize;
    let mono_program = monomorphize::monomorphize(program)?;

    let output = generate_filtered(
        &mono_program,
        "Kain",
        None,
        None,
        copyright,
        std::collections::HashMap::new(),
        None,
    )?;
    let mut combined = output.header;
    combined.push_str("\n// === IMPLEMENTATION (.cpp) ===\n\n");
    combined.push_str(&output.source);
    Ok(combined)
}

/// Generate a standalone header containing ALL stdlib utility functions as static inline implementations.
/// This is used in modular output mode to create a shared KainStdlib.h that all actor modules include.
///
/// # Deprecated
/// This function still accepts `TypedProgram` for backward compatibility.
/// New code should ensure monomorphization happens before calling this.
///
/// # Arguments
/// * `program` - TypedProgram (should be monomorphized first for generic stdlib functions)
/// * `module_name` - Module name for API macros
/// * `copyright` - Optional copyright header
/// * `type_to_header` - Map of type names to header files
pub fn generate_stdlib_functions(
    program: &TypedProgram,
    module_name: &str,
    copyright: Option<&str>,
    type_to_header: std::collections::HashMap<String, String>,
    has_gas_features: bool,
) -> KainResult<Ue5Output> {
    use kain_core::types::TypedItem;

    let mut gen = Ue5Gen::new(
        module_name,
        Some("KainStdlib"),
        copyright,
        None,
        type_to_header.clone(),
    );
    gen.has_gas_features = has_gas_features;

    // PRE-PASS: Register all types so type lookups work during codegen
    // Use the type_to_header map that was passed in (which has correct prefixed names)
    for item in &program.items {
        match item {
            TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header
                    .get(&en.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            }
            TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");
                let prefixed_name = if is_subsystem {
                    to_subsystem_name(&st.ast.name)
                } else {
                    to_struct_name(&st.ast.name)
                };
                let header = type_to_header
                    .get(&st.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context
                    .register_struct(st.ast.name.clone(), header.clone());
                if is_component {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else if is_subsystem {
                    gen.context.register_subsystem(st.ast.name.clone(), header);
                    gen.type_mapper.register_subsystem(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            }
            TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header
                    .get(&a.ast.name)
                    .cloned()
                    .unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            }
            _ => {}
        }
    }

    // Build preamble directly into header
    let mut output = String::new();
    output.push_str("// Generated by KAIN Compiler - Stdlib Function Definitions\n");
    output.push_str("// Do not edit - regenerate from .kn source\n\n");
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Kismet/KismetMathLibrary.h\"\n");

    // Include only headers for types actually used by generated stdlib function signatures.
    // This avoids pulling stale/editor-only headers that may not exist in runtime output.
    let mut stdlib_used_types: std::collections::HashSet<String> = std::collections::HashSet::new();
    for item in &program.items {
        if let TypedItem::Function(f) = item {
            let is_blueprint =
                f.ast.attributes.iter().any(|a| {
                    a.name == "blueprint" || a.name == "blueprint_pure" || a.name == "ue5"
                });
            if is_blueprint || f.ast.body.stmts.is_empty() {
                continue;
            }
            for param in &f.ast.params {
                collect_type_names(&param.ty, &mut stdlib_used_types);
            }
            if let Some(ret) = &f.ast.return_type {
                collect_type_names(ret, &mut stdlib_used_types);
            }
        }
    }

    let mut type_headers: Vec<String> = stdlib_used_types
        .iter()
        .filter_map(|t| gen.context.type_to_header.get(t).cloned())
        .collect();
    type_headers.sort();
    type_headers.dedup();
    for type_header in &type_headers {
        output.push_str(&format!("#include \"{}\"\n", type_header));
    }
    output.push_str("\n");

    // Generate each function using existing gen_ufunction machinery
    let mut func_count = 0;
    for item in &program.items {
        if let TypedItem::Function(f) = item {
            // SKIP blueprint functions - they go in the blueprint library, not stdlib
            let is_blueprint =
                f.ast.attributes.iter().any(|a| {
                    a.name == "blueprint" || a.name == "blueprint_pure" || a.name == "ue5"
                });
            if is_blueprint {
                continue;
            }

            // ONLY generate functions that have a body (ignore intrinsics/externs)
            if !f.ast.body.stmts.is_empty() {
                // Clone the function and strip blueprint/ue5 attributes so it generates as a free function
                let mut free_func = f.ast.clone();
                free_func.attributes.retain(|a| {
                    a.name != "blueprint" && a.name != "blueprint_pure" && a.name != "ue5"
                });

                gen.gen_ufunction(&free_func);
                func_count += 1;
            }
        }
    }

    // Stdlib functions generated silently

    // gen_ufunction puts declarations in header and implementations in source.
    // For header-only output, we only need the implementations with 'static inline' prefix.
    let implementations = gen.source.build();

    let mut processed = String::new();
    for line in implementations.lines() {
        let trimmed = line.trim();
        // Detect function definition lines: they start at col 0, contain '(',
        // and are not control flow/noise.
        if !trimmed.is_empty()
            && !trimmed.starts_with("//")
            && !trimmed.starts_with("#")
            && !trimmed.starts_with("{")
            && !trimmed.starts_with("}")
            && line.chars().next().map_or(false, |c| !c.is_whitespace())
            && trimmed.contains('(')
            && !trimmed.starts_with("static ")
        {
            processed.push_str(&format!("static inline {}\n", line));
        } else {
            processed.push_str(line);
            processed.push('\n');
        }
    }

    output.push_str(&processed);

    Ok(Ue5Output {
        header: output,
        source: String::new(), // Header-only file
        shader_files: Vec::new(),
    })
}

struct StringBuilder {
    lines: Vec<String>,
}

impl StringBuilder {
    fn new() -> Self {
        Self { lines: Vec::new() }
    }

    fn push_line(&mut self, text: &str) {
        self.lines.push(format!("{}\n", text));
    }

    fn build(&self) -> String {
        self.lines.join("")
    }
}

struct Ue5Gen {
    header: StringBuilder,
    source: StringBuilder,
    indent: usize,
    context: Ue5Context,
    target_item: Option<String>,
    /// Maps variable names to their type names (e.g., "health" -> "HealthComponent")
    /// Used to determine pointer access (-> vs .) for component variables
    var_types: std::collections::HashMap<String, String>,
    /// Shader file names from toml (without underscores, correct casing)
    shader_file_names: Vec<String>,
    /// POD mirror structs for @component types used in shader uniforms.
    /// Populated during the program pre-pass so the dispatch code can generate
    /// POD population lines without needing access to the full TypedProgram.
    component_mirrors: std::collections::HashMap<String, ue5_shaders::PodMirrorStruct>,
    /// Maps every named type (struct, actor) to its field list: (field_name, field_type).
    /// Used for depth-1 path resolution during shader dispatch: when a component uniform
    /// (e.g. `physics: PhysicalPropertiesComponent`) is not a direct actor state field,
    /// we walk the actor's state fields and check if any sub-type contains the uniform
    /// field, generating `this->world->physics` instead of a null fallback.
    type_fields_map: std::collections::HashMap<String, Vec<(String, Type)>>,
    /// Names of all @blueprint fns in the program, used to qualify calls in actor methods.
    blueprint_fn_names: std::collections::HashSet<String>,
    /// Raw KAIN type names referenced in @blueprint function parameter/return types.
    /// Used in the blueprint-library-only preamble to emit only the headers that are
    /// actually needed (Bug-4 fix: avoids including Slate/editor widget headers).
    blueprint_used_types: std::collections::HashSet<String>,
    /// Raw plugin/module name for deriving FunctionLibrary class names.
    module_name: String,
    /// Centralized type mapper - single source of truth for type mapping
    type_mapper: crate::ue5::types::TypeMapper,
    /// Standard library function resolver (maps KAIN stdlib to UE5 FMath::)
    stdlib_resolver: crate::ue5::stdlib_resolver::StdLibResolver,
    /// Methods from `impl` blocks, keyed by target type name.
    /// Populated during gen_program so gen_ucomponent/gen_usubsystem can look up
    /// lifecycle method bodies (begin_play, tick, etc.) and emit real implementations.
    impl_methods: std::collections::HashMap<String, Vec<kain_core::ast::Function>>,
    /// True only when the packaged program actually uses Gameplay Ability System features.
    has_gas_features: bool,
}

impl Ue5Gen {
    fn item_symbol_name<'a>(&self, item: &'a TypedItem) -> &'a str {
        match item {
            TypedItem::Actor(a) => &a.ast.name,
            TypedItem::Struct(s) => &s.ast.name,
            TypedItem::Enum(e) => &e.ast.name,
            TypedItem::Function(f) => &f.ast.name,
            TypedItem::Component(c) => &c.ast.name,
            TypedItem::TypeAlias(a) => &a.ast.name,
            _ => "",
        }
    }

    fn item_symbol_kind(&self, item: &TypedItem) -> &'static str {
        match item {
            TypedItem::Actor(_) => "actor",
            TypedItem::Struct(_) => "struct",
            TypedItem::Enum(_) => "enum",
            TypedItem::Function(_) => "function",
            TypedItem::Component(_) => "component",
            TypedItem::TypeAlias(_) => "type_alias",
            TypedItem::Impl(_) => "impl",
            TypedItem::Shader(_) => "shader",
            _ => "other",
        }
    }

    fn item_specificity_score(&self, item: &TypedItem) -> usize {
        match item {
            TypedItem::Struct(st) => {
                st.ast.fields.len() * 16 + st.ast.methods.len() * 8 + st.ast.attributes.len() * 4
            }
            TypedItem::Enum(en) => en.ast.variants.len() * 16,
            TypedItem::Actor(a) => {
                a.ast.state.len() * 16
                    + a.ast.handlers.len() * 8
                    + a.ast.methods.len() * 8
                    + a.ast.attributes.len() * 4
            }
            TypedItem::Component(c) => c.ast.state.len() * 16 + c.ast.attributes.len() * 4,
            TypedItem::TypeAlias(alias) => alias.ast.generics.len() * 4 + 1,
            TypedItem::Function(f) => {
                f.ast.params.len() * 4
                    + usize::from(!f.ast.body.stmts.is_empty()) * 8
                    + f.ast.attributes.len() * 2
            }
            _ => 0,
        }
    }

    fn push_unique_item<'a>(
        &self,
        items: &mut Vec<&'a TypedItem>,
        indices: &mut std::collections::HashMap<(String, &'static str), usize>,
        item: &'a TypedItem,
    ) {
        let name = self.item_symbol_name(item);
        if name.is_empty() {
            items.push(item);
            return;
        }

        let key = (name.to_string(), self.item_symbol_kind(item));
        let score = self.item_specificity_score(item);

        match indices.entry(key) {
            std::collections::hash_map::Entry::Vacant(vacant) => {
                let idx = items.len();
                items.push(item);
                vacant.insert(idx);
            }
            std::collections::hash_map::Entry::Occupied(occupied) => {
                let idx = *occupied.get();
                if score > self.item_specificity_score(items[idx]) {
                    items[idx] = item;
                }
            }
        }
    }

    fn should_emit_kain_runtime_helpers<P: ProgramItems>(&self, program: &P) -> bool {
        let blueprint_library_only = self
            .target_item
            .as_ref()
            .map(|t| t == "__BLUEPRINT_LIBRARY_ONLY__")
            .unwrap_or(false);

        for item in program.items() {
            if let Some(target) = &self.target_item {
                if blueprint_library_only {
                    if let TypedItem::Function(f) = item {
                        let is_blueprint_fn = f.ast.attributes.iter().any(|a| {
                            a.name == "blueprint" || a.name == "blueprint_pure" || a.name == "ue5"
                        });
                        if is_blueprint_fn && item_uses_kain_runtime(item) {
                            return true;
                        }
                    }
                    continue;
                }

                let item_name = self.item_symbol_name(item);
                if !item_name.is_empty() && item_name != target {
                    continue;
                }
            }

            if item_uses_kain_runtime(item) {
                return true;
            }
        }

        false
    }

    fn new(
        module_name: &str,
        output_name: Option<&str>,
        copyright: Option<&str>,
        target_item: Option<String>,
        type_to_header: std::collections::HashMap<String, String>,
    ) -> Self {
        // module_name = plugin name for API macro (e.g., "UltimateTest" → "ULTIMATETEST_API")
        // output_name = per-item name for file naming (e.g., "AMaterialTestActor")
        let name = output_name.unwrap_or(module_name);
        let mut context = Ue5Context::new(module_name, copyright);
        context.output_name = name.to_string(); // Set output name for file naming
        context.set_type_to_header(type_to_header);

        // Create TypeMapper with EngineKnowledge from context
        let type_mapper = crate::ue5::types::TypeMapper::with_knowledge(context.knowledge.clone());

        // Create StdLibResolver for math function mapping
        let stdlib_resolver = crate::ue5::stdlib_resolver::StdLibResolver::new();

        Self {
            header: StringBuilder::new(),
            source: StringBuilder::new(),
            indent: 0,
            context,
            target_item,
            var_types: std::collections::HashMap::new(),
            shader_file_names: Vec::new(),
            component_mirrors: std::collections::HashMap::new(),
            type_fields_map: std::collections::HashMap::new(),
            blueprint_fn_names: std::collections::HashSet::new(),
            blueprint_used_types: std::collections::HashSet::new(),
            module_name: module_name.to_string(),
            type_mapper,
            stdlib_resolver,
            impl_methods: std::collections::HashMap::new(),
            has_gas_features: false,
        }
    }

    fn push_indent(&mut self) {
        self.indent += 1;
    }

    fn pop_indent(&mut self) {
        if self.indent > 0 {
            self.indent -= 1;
        }
    }

    fn indent_str(&self) -> String {
        "\t".repeat(self.indent)
    }

    fn write_header(&mut self, line: &str) {
        let indented = format!("{}{}", self.indent_str(), line);
        self.header.push_line(&indented);
    }

    fn write_source(&mut self, line: &str) {
        let indented = format!("{}{}", self.indent_str(), line);
        self.source.push_line(&indented);
    }

    fn write_blank_header(&mut self) {
        self.header.push_line("");
    }

    fn write_blank_source(&mut self) {
        self.source.push_line("");
    }

    fn interpolate_raw_string(&self, s: &str) -> (String, Vec<String>) {
        let mut fmt = String::new();
        let mut args = Vec::new();
        let mut current = s;
        while let Some(start) = current.find('{') {
            fmt.push_str(&current[..start].replace('%', "%%"));
            current = &current[start + 1..];
            if let Some(end) = current.find('}') {
                let ident = &current[..end];
                let remapped = self.remap_collection_length_access(
                    &self.remap_pointer_member_access(&self.remap_ident(ident)),
                );
                if self.is_enum_ident_name(ident) {
                    fmt.push_str("%d");
                    args.push(format!("static_cast<int32>({})", remapped));
                } else {
                    fmt.push_str("%s");
                    // UObject/actor/component pointers should be logged by name, not via
                    // LexToString(pointer), which can resolve through bool conversions.
                    if self.is_pointer_type_by_name(ident) {
                        args.push(format!("*GetNameSafe({})", remapped));
                    } else {
                        args.push(format!("*LexToString({})", remapped));
                    }
                }
                current = &current[end + 1..];
            } else {
                fmt.push('{');
            }
        }
        fmt.push_str(&current.replace('%', "%%"));
        (fmt, args)
    }

    fn remap_ident(&self, name: &str) -> String {
        self.context.remap_ident(name)
    }

    /// Convert `ptr.member` into `ptr->member` for raw-string interpolation placeholders.
    /// This is intentionally conservative and only rewrites the first access segment.
    fn remap_pointer_member_access(&self, expr: &str) -> String {
        if let Some((head, tail)) = expr.split_once('.') {
            if self.is_pointer_type_by_name(head) {
                return format!("{}->{}", head, tail);
            }
        }
        expr.to_string()
    }

    /// Normalize collection length access inside interpolation placeholders.
    /// Supports `arr.length`, `arr.length()`, `arr.len`, and `arr.len()`.
    fn remap_collection_length_access(&self, expr: &str) -> String {
        for suffix in [".length()", ".len()", ".count()", ".size()"] {
            if let Some(base) = expr.strip_suffix(suffix) {
                return format!("{}.Num()", base);
            }
        }
        for suffix in [".length", ".len", ".count", ".size"] {
            if let Some(base) = expr.strip_suffix(suffix) {
                return format!("{}.Num()", base);
            }
        }
        expr.to_string()
    }

    /// Best-effort enum detection for an identifier by name.
    fn is_enum_ident_name(&self, name: &str) -> bool {
        if let Some(type_name) = self.var_types.get(name) {
            self.context.is_enum(type_name)
        } else {
            self.context.is_enum(name)
        }
    }

    /// Check if an expression refers to a pointer type (component, actor, UObject-derived)
    /// Used to determine whether to use `->`(pointer) or `.`(value) for member access.
    /// In UE5, components (U*Component*), actors (A*), and UObject-derived types
    /// (UMaterialInstanceDynamic*, UTexture2D*, etc.) are always heap-allocated pointers.
    ///
    /// This method determines whether to use -> or . for member access by checking
    /// if the expression evaluates to a pointer type.
    fn is_pointer_receiver(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Ident(name, _) => self.is_pointer_type_by_name(name),
            Expr::Field { object, field, .. } => {
                // For field access, we need to determine the type of the field being accessed
                // First, resolve the type of the object
                if let Some(obj_type) = self.infer_expr_type(object) {
                    // Look up the field type in type_fields_map
                    if let Some(fields) = self.type_fields_map.get(&obj_type) {
                        for (field_name, field_type) in fields {
                            if field_name == field {
                                // Found the field - check if its type is a pointer type
                                if let Type::Named { name, .. } = field_type {
                                    return self.is_pointer_type_by_name(name);
                                }
                            }
                        }
                    }
                }

                // Fallback: handle self.field case
                if let Expr::Ident(ref obj_name, _) = object.as_ref() {
                    if obj_name == "self" {
                        return self.is_pointer_type_by_name(field);
                    }
                }
                false
            }
            _ => false,
        }
    }

    /// Infer the type name of an expression for member access resolution.
    /// Returns the KAIN type name (e.g., "HealthComponent", "Vec3", "Actor").
    fn infer_expr_type(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::Ident(name, _) => {
                // Check if it's a variable with a known type
                if let Some(type_name) = self.var_types.get(name) {
                    return Some(type_name.clone());
                }
                // Check if it's "self" - need to determine current context type
                if name == "self" {
                    // This would require tracking the current actor/struct being generated
                    // For now, return None and rely on fallback logic
                    return None;
                }
                None
            }
            Expr::Field { object, field, .. } => {
                // Recursively resolve the object type, then look up the field type
                if let Some(obj_type) = self.infer_expr_type(object) {
                    if let Some(fields) = self.type_fields_map.get(&obj_type) {
                        for (field_name, field_type) in fields {
                            if field_name == field {
                                if let Type::Named { name, .. } = field_type {
                                    return Some(name.clone());
                                }
                            }
                        }
                    }
                }
                None
            }
            _ => None,
        }
    }

    /// Register parameter types in the local type table for best-effort inference
    /// (enum logging, pointer access, float modulo lowering).
    fn register_param_types(&mut self, params: &[Param]) {
        for p in params {
            match &p.ty {
                Type::Named { name, .. } => {
                    self.var_types.insert(p.name.clone(), name.clone());
                }
                // Preserve primitive float information for `%` lowering.
                Type::Ref { inner, .. } => {
                    if let Type::Named { name, .. } = inner.as_ref() {
                        self.var_types.insert(p.name.clone(), name.clone());
                    }
                }
                _ => {}
            }
        }
    }

    /// Register field types in the local type table for better member access inference
    /// (pointer receiver detection, enum detection on field-owned values, etc.).
    fn register_field_types(&mut self, fields: &[Field]) {
        for f in fields {
            match &f.ty {
                Type::Named { name, .. } => {
                    self.var_types.insert(f.name.clone(), name.clone());
                }
                Type::Ref { inner, .. } => {
                    if let Type::Named { name, .. } = inner.as_ref() {
                        self.var_types.insert(f.name.clone(), name.clone());
                    }
                }
                _ => {}
            }
        }
    }

    /// Execute codegen inside a temporary var type scope.
    fn with_var_type_scope<F: FnOnce(&mut Self)>(&mut self, f: F) {
        let saved = self.var_types.clone();
        f(self);
        self.var_types = saved;
    }

    /// Check if a variable name refers to a pointer type by looking up its KAIN type
    /// in var_types and checking whether that type is a UObject-derived pointer in UE5.
    ///
    /// Now delegates to TypeMapper for centralized pointer type detection
    fn is_pointer_type_by_name(&self, name: &str) -> bool {
        // PRIMARY: Check var_types map (populated from actor state declarations)
        if let Some(type_name) = self.var_types.get(name) {
            // Use TypeMapper to check if this is a pointer type
            return self.type_mapper.is_pointer_type_by_name(type_name);
        }

        // FALLBACK: Check if the identifier itself IS a known type name
        if self.context.is_component(name) || self.context.is_actor(name) {
            return true;
        }

        // Use TypeMapper for final check
        self.type_mapper.is_pointer_type_by_name(name)
    }

    /// Best-effort enum detection for log formatting and conversions.
    /// Uses collected variable type info and enum registry.
    fn is_enum_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::EnumVariant { .. } => true,
            Expr::Ident(name, _) => {
                if let Some(type_name) = self.var_types.get(name) {
                    self.context.is_enum(type_name)
                } else {
                    self.context.is_enum(name)
                }
            }
            Expr::Field { object, field, .. } => {
                if let Expr::Ident(obj, _) = object.as_ref() {
                    if obj == "self" {
                        if let Some(type_name) = self.var_types.get(field) {
                            return self.context.is_enum(type_name);
                        }
                    }
                }
                false
            }
            _ => false,
        }
    }

    /// Heuristic float-expression detection for `%` lowering.
    fn is_likely_float_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Float(_, _) => true,
            Expr::Ident(name, _) => {
                if let Some(type_name) = self.var_types.get(name) {
                    type_name == "Float"
                } else {
                    false
                }
            }
            Expr::Field { object, field, .. } => {
                if let Expr::Ident(obj, _) = object.as_ref() {
                    if obj == "self" {
                        if let Some(type_name) = self.var_types.get(field) {
                            return type_name == "Float";
                        }
                    }
                }
                false
            }
            _ => false,
        }
    }

    fn gen_program<P: ProgramItems>(&mut self, program: &P) -> Ue5Output {
        // Check if we need replication support
        let needs_replication = program.items().iter().any(|item| match item {
            TypedItem::Actor(a) => a
                .ast
                .state
                .iter()
                .any(|s| s.attributes.iter().any(|a| a.name == "replicated")),
            TypedItem::Struct(s) => s
                .ast
                .fields
                .iter()
                .any(|f| f.attributes.iter().any(|a| a.name == "replicated")),
            _ => false,
        });

        // Track replication feature
        if needs_replication {
            self.context.use_feature("Replication");
        }

        // Collect all shaders for actor integration
        let shaders: Vec<&TypedShader> = program
            .items()
            .iter()
            .filter_map(|item| {
                if let TypedItem::Shader(shader) = item {
                    // Track shader feature
                    self.context.use_feature("Shader");
                    Some(shader)
                } else {
                    None
                }
            })
            .collect();

        // Pre-collect Blueprint function signature types BEFORE include generation.
        // The blueprint-library-only include pass depends on this set.
        self.blueprint_used_types.clear();
        for item in program.items() {
            if let TypedItem::Function(f) = item {
                if f.ast
                    .attributes
                    .iter()
                    .any(|a| a.name == "blueprint" || a.name == "blueprint_pure")
                {
                    for param in &f.ast.params {
                        collect_type_names(&param.ty, &mut self.blueprint_used_types);
                    }
                    if let Some(ret) = &f.ast.return_type {
                        collect_type_names(ret, &mut self.blueprint_used_types);
                    }
                }
            }
        }

        // Special mode: generate only BlueprintFunctionLibrary declarations/defs.
        let blueprint_library_only = self
            .target_item
            .as_ref()
            .map(|t| t == "__BLUEPRINT_LIBRARY_ONLY__")
            .unwrap_or(false);

        // Check whether the current generation target dispatches shaders.
        // If true, actor headers need UTextureRenderTarget2D declarations/includes.
        let target_dispatches_shaders = if let Some(target) = &self.target_item {
            program.items().iter().any(|item| {
                if let TypedItem::Actor(a) = item {
                    a.ast.name == *target
                        && a.ast.attributes.iter().any(|attr| attr.name == "dispatch")
                } else {
                    false
                }
            })
        } else {
            !shaders.is_empty() // monolithic mode: include if any shaders exist
        };

        // Compute shader headers to include for this translation unit.
        // In sliced actor mode, include only headers explicitly requested via @dispatch(...).
        // In monolithic mode, include all discovered shader file names.
        let normalize_shader_name = |name: &str| name.replace('_', "").to_ascii_lowercase();
        let available_shader_bases: Vec<String> = self
            .shader_file_names
            .iter()
            .map(|shader_file_name| {
                shader_file_name
                    .rsplit(['/', '\\'])
                    .next()
                    .unwrap_or(shader_file_name)
                    .strip_suffix(".usf")
                    .unwrap_or_else(|| {
                        shader_file_name
                            .rsplit(['/', '\\'])
                            .next()
                            .unwrap_or(shader_file_name)
                    })
                    .to_string()
            })
            .collect();
        let mut selected_shader_bases: Vec<String> = if let Some(target) = &self.target_item {
            if let Some(actor) = program.items().iter().find_map(|item| {
                if let TypedItem::Actor(a) = item {
                    if a.ast.name == *target {
                        return Some(a);
                    }
                }
                None
            }) {
                let requested: Vec<String> = actor
                    .ast
                    .attributes
                    .iter()
                    .find(|a| a.name == "dispatch")
                    .map(|attr| {
                        attr.args
                            .iter()
                            .filter_map(|arg| {
                                if let Expr::String(s, _) = arg {
                                    Some(s.clone())
                                } else {
                                    None
                                }
                            })
                            .collect::<Vec<_>>()
                    })
                    .unwrap_or_default();

                if requested.is_empty() {
                    Vec::new()
                } else {
                    requested
                        .into_iter()
                        .map(|req| {
                            available_shader_bases
                                .iter()
                                .find(|base| {
                                    normalize_shader_name(base) == normalize_shader_name(&req)
                                })
                                .cloned()
                                .unwrap_or(req)
                        })
                        .collect()
                }
            } else {
                Vec::new()
            }
        } else {
            available_shader_bases
        };
        let mut deduped_shader_bases: Vec<String> = Vec::new();
        let mut seen_shader_bases: HashSet<String> = HashSet::new();
        for shader_base in selected_shader_bases.drain(..) {
            if seen_shader_bases.insert(shader_base.clone()) {
                deduped_shader_bases.push(shader_base);
            }
        }

        // Determine what kind of item we're generating (for smart includes)
        let target_item_kind = if let Some(target) = &self.target_item {
            program
                .items()
                .iter()
                .find_map(|item| {
                    let name = match item {
                        TypedItem::Actor(a) => &a.ast.name,
                        TypedItem::Struct(s) => &s.ast.name,
                        TypedItem::Enum(e) => &e.ast.name,
                        TypedItem::Component(c) => &c.ast.name,
                        TypedItem::TypeAlias(a) => &a.ast.name,
                        _ => return None,
                    };
                    if name == target {
                        Some(match item {
                            TypedItem::Actor(_) => "actor",
                            TypedItem::Struct(s) => {
                                if s.ast.attributes.iter().any(|a| a.name == "component") {
                                    "component"
                                } else if s.ast.attributes.iter().any(|a| a.name == "datatable") {
                                    "datatable"
                                } else {
                                    "struct"
                                }
                            }
                            TypedItem::Enum(_) => "enum",
                            TypedItem::Component(_) => "component",
                            TypedItem::TypeAlias(_) => "delegate",
                            _ => "unknown",
                        })
                    } else {
                        None
                    }
                })
                .unwrap_or("unknown")
        } else {
            "all" // No target = generating everything
        };

        let mut target_is_subsystem = false;
        let mut target_is_tick_subsystem = false;
        if let Some(target) = &self.target_item {
            for item in program.items() {
                if let TypedItem::Struct(s) = item {
                    if &s.ast.name == target {
                        target_is_subsystem =
                            s.ast.attributes.iter().any(|a| a.name == "subsystem");
                        target_is_tick_subsystem = target_is_subsystem
                            && s.ast.attributes.iter().any(|a| a.name == "tick");
                        break;
                    }
                }
            }
        }

        // Initialize includes based on item type (CoreMinimal.h is already in the template)
        let mut includes: Vec<&str> = Vec::new();
        if blueprint_library_only {
            // Start minimal; we'll add only needed signature type headers below.
            includes.push("Kismet/BlueprintFunctionLibrary.h");
        } else {
            match target_item_kind {
                "actor" => {
                    includes.push("GameFramework/Actor.h");
                    if needs_replication {
                        includes.push("Net/UnrealNetwork.h");
                    }
                }
                "component" => {
                    includes.push("Components/ActorComponent.h");
                    if needs_replication {
                        includes.push("Net/UnrealNetwork.h");
                    }
                }
                "datatable" => {
                    includes.push("Engine/DataTable.h");
                }
                "struct" | "enum" | "delegate" => {
                    // Minimal includes — CoreMinimal.h from template is sufficient
                    if target_is_subsystem {
                        includes.push("Subsystems/WorldSubsystem.h");
                    }
                    if target_is_tick_subsystem {
                        includes.push("Tickable.h");
                    }
                }
                _ => {
                    // Full includes for combined/unknown output
                    includes.push("GameFramework/Actor.h");
                    includes.push("Components/ActorComponent.h");
                    includes.push("Kismet/BlueprintFunctionLibrary.h");
                    if needs_replication {
                        includes.push("Net/UnrealNetwork.h");
                    }
                }
            }
        }

        // DISCOVERY PASS: If we are targeting a specific item, scan it for dependencies first
        if self.target_item.is_some() && !blueprint_library_only {
            for item in program.items() {
                let item_name = match item {
                    TypedItem::Actor(a) => &a.ast.name,
                    TypedItem::Struct(s) => &s.ast.name,
                    TypedItem::Enum(e) => &e.ast.name,
                    TypedItem::Component(c) => &c.ast.name,
                    TypedItem::TypeAlias(a) => &a.ast.name,
                    _ => "",
                };

                if let Some(target) = &self.target_item {
                    if item_name == target {
                        // This is our guy - run map_type on all its internal types to populate needed_headers
                        self.discover_item_dependencies(item);
                    }
                }
            }
        }

        // Add discovered headers to includes list
        if !blueprint_library_only {
            for header in self.context.get_needed_headers() {
                // Never include the module master header from generated leaf headers.
                // It causes recursive include cycles (leaf -> Plugin.h -> leafs) which
                // can surface as unknown type / missing ';' pointer errors.
                if header == format!("{}.h", self.module_name) {
                    continue;
                }
                if !includes.contains(&header.as_str()) {
                    includes.push(Box::leak(header.into_boxed_str())); // Keep it simple for now
                }
            }

            if target_dispatches_shaders && !includes.contains(&"Engine/TextureRenderTarget2D.h") {
                includes.push("Engine/TextureRenderTarget2D.h");
            }
        }

        // Bug-4 fix: if we're in blueprint-library-only mode, only include headers for
        // types that are actually referenced in @blueprint function signatures.
        // This replaces the old hardcoded skip list (plugin-specific names like
        // "FCosmosDashboard", "FSovereignDetailsPanel", etc.).
        if blueprint_library_only {
            // type_to_header keys are raw KAIN type names; collect only those used.
            let used = &self.blueprint_used_types;
            for (type_name, type_header) in &self.context.type_to_header {
                if used.contains(type_name.as_str()) {
                    let header_ref: &str = type_header;
                    if !includes.iter().any(|h| *h == header_ref) {
                        includes.push(Box::leak(type_header.clone().into_boxed_str()));
                    }
                }
            }
        }
        // ── old blueprint_library_only block replaced above ──

        // Check if this is a delegate-only file (delegates don't need .generated.h)
        let is_delegate_only = if let Some(target) = &self.target_item {
            // Check if the target item is a delegate
            program.items().iter().any(|item| {
                if let TypedItem::TypeAlias(alias) = item {
                    if alias.ast.name == *target {
                        // Check if it's a function type (delegate)
                        matches!(alias.ast.target, Type::Function { .. })
                    } else {
                        false
                    }
                } else {
                    false
                }
            })
        } else {
            false
        };

        let header_ctx = json!({
            "copyright": self.context.copyright,
            "timestamp": SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs(),
            "source_file": "KAIN Source",
            "output_name": self.context.output_name,
            "includes": includes,
            "forward_decls": self.context.get_forward_decls(),
            "needs_generated_h": !is_delegate_only,
        });

        if let Ok(preamble) = TEMPLATES.render("header_preamble", header_ctx) {
            self.header.push_line(&preamble);
        }

        // Source preamble
        self.source
            .push_line("// Generated by KAIN Compiler - UE5 C++ Codegen");
        self.source
            .push_line("// Do not edit - regenerate from .kn source");
        self.write_blank_source();
        self.source
            .push_line(&format!("#include \"{}.h\"", self.context.output_name));
        // If this file may call @blueprint static helpers, include the generated
        // module-level blueprint library header so U{Module}FunctionLibrary symbols resolve.
        let module_blueprint_header = format!("{}BlueprintLibrary", self.module_name);
        if self.context.output_name != module_blueprint_header {
            self.source.push_line(&format!(
                "#if __has_include(\"{}.h\")",
                module_blueprint_header
            ));
            self.source
                .push_line(&format!("#include \"{}.h\"", module_blueprint_header));
            self.source.push_line("#endif");
        }
        if self.has_gas_features {
            self.source
                .push_line("#include \"AbilitySystemBlueprintLibrary.h\"");
            self.source.push_line("#include \"GameplayTagContainer.h\"");
        }
        self.source.push_line("#include \"Containers/Map.h\"");
        self.source.push_line("#include \"Containers/Array.h\"");
        self.source.push_line("#include \"HAL/Platform.h\"");
        // Only include RenderGraph/shader headers if this actor actually dispatches shaders.
        // In sliced mode, check for @dispatch attribute on the target actor.
        // In monolithic mode (no target_item), include if any shaders exist.
        if target_dispatches_shaders {
            self.source.push_line("#include \"RenderGraph.h\"");
            self.source.push_line("#include \"RenderGraphBuilder.h\"");
            self.source.push_line("#include \"RenderGraphResources.h\"");
            self.source.push_line("#include \"RenderGraphUtils.h\"");
            self.source.push_line("#include \"RenderTargetPool.h\"");
            self.source
                .push_line("#include \"Engine/TextureRenderTarget2D.h\"");
            self.source.push_line("#include \"TextureResource.h\"");
            for shader_base in &deduped_shader_bases {
                let shader_header = format!("{}.h", shader_base);
                self.source
                    .push_line(&format!("#include \"{}\"", shader_header));
            }
        }

        if self.should_emit_kain_runtime_helpers(program) {
            self.source.push_line("");
            self.source.push_line("namespace {");
            self.source.push_line("struct FKainMemoryValue {");
            self.source.push_line("    TArray<uint8> Bytes;");
            self.source
                .push_line("    template<typename T> operator T() const");
            self.source.push_line("    {");
            self.source.push_line("        T Result{};");
            self.source.push_line("        const int32 CopySize = FMath::Min<int32>(Bytes.Num(), static_cast<int32>(sizeof(T)));");
            self.source.push_line("        if (CopySize > 0) { FMemory::Memcpy(&Result, Bytes.GetData(), CopySize); }");
            self.source.push_line("        return Result;");
            self.source.push_line("    }");
            self.source.push_line("};");
            self.source
                .push_line("static TMap<int64, FKainMemoryValue> GKainMemory;");
            self.source.push_line("static int64 GKainNextPtr = 1;");
            self.source
                .push_line("template<typename T> int64 __kain_addr_of(const T& Value)");
            self.source.push_line("{");
            self.source
                .push_line("    const int64 Ptr = GKainNextPtr++;");
            self.source.push_line("    FKainMemoryValue Cell;");
            self.source
                .push_line("    Cell.Bytes.SetNumUninitialized(sizeof(T));");
            self.source
                .push_line("    FMemory::Memcpy(Cell.Bytes.GetData(), &Value, sizeof(T));");
            self.source
                .push_line("    GKainMemory.Add(Ptr, MoveTemp(Cell));");
            self.source.push_line("    return Ptr;");
            self.source.push_line("}");
            self.source
                .push_line("template<typename T> int64 __kain_bind_local(const T& Value)");
            self.source.push_line("{");
            self.source.push_line("    return __kain_addr_of(Value);");
            self.source.push_line("}");
            self.source
                .push_line("static int64 __kain_field_ptr(int64 Ptr, const TCHAR*, int64 Offset)");
            self.source.push_line("{");
            self.source.push_line("    return Ptr + Offset;");
            self.source.push_line("}");
            self.source
                .push_line("static int64 __kain_index_ptr(int64 Ptr, int64 Index, int64 Stride)");
            self.source.push_line("{");
            self.source
                .push_line("    return Ptr + (Index * FMath::Max<int64>(Stride, 1));");
            self.source.push_line("}");
            self.source
                .push_line("static int64 __kain_ptr_offset(int64 Ptr, int64 Offset, int64 Stride)");
            self.source.push_line("{");
            self.source
                .push_line("    return Ptr + (Offset * FMath::Max<int64>(Stride, 1));");
            self.source.push_line("}");
            self.source
                .push_line("static FKainMemoryValue __kain_mem_load(int64 Ptr)");
            self.source.push_line("{");
            self.source.push_line(
                "    if (const FKainMemoryValue* Value = GKainMemory.Find(Ptr)) { return *Value; }",
            );
            self.source.push_line("    return FKainMemoryValue{};");
            self.source.push_line("}");
            self.source
                .push_line("template<typename T> T __kain_mem_store(int64 Ptr, const T& Value)");
            self.source.push_line("{");
            self.source.push_line("    FKainMemoryValue Cell;");
            self.source
                .push_line("    Cell.Bytes.SetNumUninitialized(sizeof(T));");
            self.source
                .push_line("    FMemory::Memcpy(Cell.Bytes.GetData(), &Value, sizeof(T));");
            self.source
                .push_line("    GKainMemory.Add(Ptr, MoveTemp(Cell));");
            self.source.push_line("    return Value;");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject, typename TValue> TObject __kain_union_wrap(TObject Value, const TCHAR*, const TCHAR*, int64 ByteSize, int64 UnionSize, const TValue& ActiveValue)");
            self.source.push_line("{");
            self.source.push_line("    const int64 CopySpan = FMath::Min<int64>(FMath::Min<int64>(ByteSize, UnionSize), FMath::Min<int64>(static_cast<int64>(sizeof(TObject)), static_cast<int64>(sizeof(TValue))));");
            self.source.push_line("    if (UnionSize > 0)");
            self.source.push_line("    {");
            self.source.push_line("        FMemory::Memzero(&Value, FMath::Min<int64>(UnionSize, static_cast<int64>(sizeof(TObject))));");
            self.source.push_line("    }");
            self.source.push_line("    if (CopySpan > 0)");
            self.source.push_line("    {");
            self.source
                .push_line("        FMemory::Memcpy(&Value, &ActiveValue, CopySpan);");
            self.source.push_line("    }");
            self.source.push_line("    return Value;");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject, typename TValue> TValue __kain_union_get(const TObject& Value, const TCHAR*, const TCHAR*, int64 ByteSize, int64 UnionSize, const TValue& Fallback)");
            self.source.push_line("{");
            self.source.push_line("    TValue Result = Fallback;");
            self.source.push_line("    const int64 CopySpan = FMath::Min<int64>(FMath::Min<int64>(ByteSize, UnionSize), FMath::Min<int64>(static_cast<int64>(sizeof(TObject)), static_cast<int64>(sizeof(TValue))));");
            self.source.push_line("    if (CopySpan > 0)");
            self.source.push_line("    {");
            self.source
                .push_line("        FMemory::Memcpy(&Result, &Value, CopySpan);");
            self.source.push_line("    }");
            self.source.push_line("    return Result;");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject, typename TValue> TValue __kain_union_set(TObject& Value, const TCHAR*, const TCHAR*, int64 ByteSize, int64 UnionSize, const TValue& Next)");
            self.source.push_line("{");
            self.source.push_line("    const int64 ZeroSpan = FMath::Min<int64>(UnionSize, static_cast<int64>(sizeof(TObject)));");
            self.source.push_line("    if (ZeroSpan > 0)");
            self.source.push_line("    {");
            self.source
                .push_line("        FMemory::Memzero(&Value, ZeroSpan);");
            self.source.push_line("    }");
            self.source.push_line("    const int64 CopySpan = FMath::Min<int64>(FMath::Min<int64>(ByteSize, UnionSize), FMath::Min<int64>(static_cast<int64>(sizeof(TObject)), static_cast<int64>(sizeof(TValue))));");
            self.source.push_line("    if (CopySpan > 0)");
            self.source.push_line("    {");
            self.source
                .push_line("        FMemory::Memcpy(&Value, &Next, CopySpan);");
            self.source.push_line("    }");
            self.source.push_line("    return Next;");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject> uint64 __kain_load_bitfield_unit(const TObject& Value, int64 UnitOffset)");
            self.source.push_line("{");
            self.source.push_line(
                "    if (UnitOffset < 0 || UnitOffset >= static_cast<int64>(sizeof(TObject)))",
            );
            self.source.push_line("    {");
            self.source.push_line("        return 0;");
            self.source.push_line("    }");
            self.source.push_line("    uint64 Unit = 0;");
            self.source.push_line("    const int64 Available = FMath::Min<int64>(8, static_cast<int64>(sizeof(TObject)) - UnitOffset);");
            self.source.push_line("    FMemory::Memcpy(&Unit, reinterpret_cast<const uint8*>(&Value) + UnitOffset, Available);");
            self.source.push_line("    return Unit;");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject> void __kain_store_bitfield_unit(TObject& Value, int64 UnitOffset, uint64 Unit)");
            self.source.push_line("{");
            self.source.push_line(
                "    if (UnitOffset < 0 || UnitOffset >= static_cast<int64>(sizeof(TObject)))",
            );
            self.source.push_line("    {");
            self.source.push_line("        return;");
            self.source.push_line("    }");
            self.source.push_line("    const int64 Available = FMath::Min<int64>(8, static_cast<int64>(sizeof(TObject)) - UnitOffset);");
            self.source.push_line("    FMemory::Memcpy(reinterpret_cast<uint8*>(&Value) + UnitOffset, &Unit, Available);");
            self.source.push_line("}");
            self.source
                .push_line("static uint64 __kain_bitfield_mask(int64 Width)");
            self.source.push_line("{");
            self.source.push_line("    if (Width <= 0) { return 0; }");
            self.source
                .push_line("    if (Width >= 64) { return MAX_uint64; }");
            self.source
                .push_line("    return (uint64(1) << Width) - 1ULL;");
            self.source.push_line("}");
            self.source
                .push_line("static int64 __kain_sign_extend(uint64 Value, int64 Width)");
            self.source.push_line("{");
            self.source.push_line("    if (Width <= 0) { return 0; }");
            self.source
                .push_line("    if (Width >= 64) { return static_cast<int64>(Value); }");
            self.source
                .push_line("    const uint64 SignBit = uint64(1) << (Width - 1);");
            self.source
                .push_line("    if ((Value & SignBit) == 0) { return static_cast<int64>(Value); }");
            self.source
                .push_line("    const uint64 FullMask = ~__kain_bitfield_mask(Width);");
            self.source
                .push_line("    return static_cast<int64>(Value | FullMask);");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject> int64 __kain_bitfield_get(const TObject& Value, const TCHAR*, int64 UnitOffset, int64 BitOffset, int64 Width, bool bSigned)");
            self.source.push_line("{");
            self.source
                .push_line("    const uint64 Mask = __kain_bitfield_mask(Width);");
            self.source
                .push_line("    const uint64 Unit = __kain_load_bitfield_unit(Value, UnitOffset);");
            self.source.push_line(
                "    const uint64 Shifted = BitOffset <= 0 ? Unit : (Unit >> BitOffset);",
            );
            self.source
                .push_line("    const uint64 Encoded = Shifted & Mask;");
            self.source.push_line("    return bSigned ? __kain_sign_extend(Encoded, Width) : static_cast<int64>(Encoded);");
            self.source.push_line("}");
            self.source.push_line("template<typename TObject, typename TValue> TValue __kain_bitfield_set(TObject& Value, const TCHAR*, int64 UnitOffset, int64 BitOffset, int64 Width, bool bSigned, const TValue& Next)");
            self.source.push_line("{");
            self.source
                .push_line("    const uint64 Mask = __kain_bitfield_mask(Width);");
            self.source
                .push_line("    uint64 Unit = __kain_load_bitfield_unit(Value, UnitOffset);");
            self.source.push_line(
                "    const uint64 Encoded = static_cast<uint64>(static_cast<int64>(Next)) & Mask;",
            );
            self.source.push_line(
                "    const uint64 ShiftedMask = BitOffset <= 0 ? Mask : (Mask << BitOffset);",
            );
            self.source.push_line("    Unit = (Unit & ~ShiftedMask) | (BitOffset <= 0 ? Encoded : (Encoded << BitOffset));");
            self.source
                .push_line("    __kain_store_bitfield_unit(Value, UnitOffset, Unit);");
            self.source.push_line("    return static_cast<TValue>(__kain_bitfield_get(Value, TEXT(\"\"), UnitOffset, BitOffset, Width, bSigned));");
            self.source.push_line("}");
            self.source.push_line("template<typename T> int64 __kain_alloc(int64 Size, int64 Stride, bool, const T& Seed)");
            self.source.push_line("{");
            self.source
                .push_line("    const int64 Step = FMath::Max<int64>(Stride, 1);");
            self.source.push_line("    const int64 Count = FMath::Max<int64>(1, (FMath::Max<int64>(Size, Step) + Step - 1) / Step);");
            self.source
                .push_line("    const int64 Base = GKainNextPtr;");
            self.source
                .push_line("    for (int64 Index = 0; Index < Count; ++Index)");
            self.source.push_line("    {");
            self.source
                .push_line("        __kain_mem_store(Base + (Index * Step), Seed);");
            self.source.push_line("    }");
            self.source
                .push_line("    GKainNextPtr = Base + (Count * Step);");
            self.source.push_line("    return Base;");
            self.source.push_line("}");
            self.source.push_line("template<typename T> int64 __kain_realloc(int64 Ptr, int64 Size, int64 Stride, const T& Seed)");
            self.source.push_line("{");
            self.source.push_line("    if (Ptr == 0)");
            self.source.push_line("    {");
            self.source
                .push_line("        return __kain_alloc(Size, Stride, false, Seed);");
            self.source.push_line("    }");
            self.source
                .push_line("    const int64 Step = FMath::Max<int64>(Stride, 1);");
            self.source.push_line("    const int64 Count = FMath::Max<int64>(1, (FMath::Max<int64>(Size, Step) + Step - 1) / Step);");
            self.source
                .push_line("    for (int64 Index = 0; Index < Count; ++Index)");
            self.source.push_line("    {");
            self.source
                .push_line("        const int64 NextPtr = Ptr + (Index * Step);");
            self.source
                .push_line("        if (!GKainMemory.Contains(NextPtr))");
            self.source.push_line("        {");
            self.source
                .push_line("            __kain_mem_store(NextPtr, Seed);");
            self.source.push_line("        }");
            self.source.push_line("    }");
            self.source.push_line("    return Ptr;");
            self.source.push_line("}");
            self.source.push_line("}");
        }

        if !shaders.is_empty() {
            self.source.push_line("");
            self.source.push_line("namespace {");
            self.source.push_line("struct FKainShaderDispatchShim {");
            self.source
                .push_line("    template<typename... TArgs> void Dispatch(TArgs&&...) const {}");
            self.source
                .push_line("    template<typename... TArgs> void Emit(TArgs&&...) const {}");
            self.source
                .push_line("    template<typename... TArgs> void EmitBurst(TArgs&&...) const {}");
            self.source.push_line("};");
            let mut emitted_shim_names: HashSet<String> = HashSet::new();
            for shader in &shaders {
                if emitted_shim_names.insert(shader.ast.name.clone()) {
                    self.source.push_line(&format!(
                        "static constexpr FKainShaderDispatchShim {}{{}};",
                        shader.ast.name
                    ));
                }
            }
            self.source.push_line("}");
        }
        self.write_blank_source();

        // PRE-PASS: Collect all enum, struct, and component names BEFORE generating any code
        // This ensures that delegate parameter types can be correctly resolved
        let default_header = format!("{}.h", self.context.output_name);
        for item in program.items() {
            let item_header = match item {
                TypedItem::Enum(en) => self
                    .context
                    .type_to_header
                    .get(&en.ast.name)
                    .cloned()
                    .unwrap_or(default_header.clone()),
                TypedItem::Struct(st) => self
                    .context
                    .type_to_header
                    .get(&st.ast.name)
                    .cloned()
                    .unwrap_or(default_header.clone()),
                _ => default_header.clone(),
            };

            match item {
                TypedItem::Enum(en) => {
                    self.context.register_enum(en.ast.name.clone(), item_header);
                }
                TypedItem::Struct(st) => {
                    self.context
                        .register_struct(st.ast.name.clone(), item_header.clone());
                    if st.ast.attributes.iter().any(|a| a.name == "component") {
                        self.context
                            .register_component(st.ast.name.clone(), item_header);
                    }
                }
                TypedItem::Component(c) => {
                    let h = self
                        .context
                        .type_to_header
                        .get(&c.ast.name)
                        .cloned()
                        .unwrap_or(default_header.clone());
                    self.context.register_component(c.ast.name.clone(), h);
                }
                TypedItem::Actor(a) => {
                    let h = self
                        .context
                        .type_to_header
                        .get(&a.ast.name)
                        .cloned()
                        .unwrap_or(default_header.clone());
                    self.context.register_actor(a.ast.name.clone(), h);
                }
                _ => {}
            }
        }

        // Pre-compute POD mirrors so the actor dispatch code can reference them
        // without needing the full TypedProgram.
        // Convert MonomorphizedProgram to TypedProgram for shader codegen
        let typed_program = kain_core::types::TypedProgram {
            items: program.items().to_vec(),
        };
        self.component_mirrors =
            match ue5_shaders::pod_mirror::collect_component_mirrors(&typed_program) {
                Ok(m) => m,
                Err(e) => {
                    // POD mirror error - using empty map
                    std::collections::HashMap::new()
                }
            };

        // Build a map from every named type to its field/state list for depth-1
        // uniform path resolution (e.g. HyperFluidSimulationCore → [(physics, ...), ...]).
        for item in program.items() {
            match item {
                TypedItem::Struct(st) => {
                    let fields: Vec<(String, Type)> = st
                        .ast
                        .fields
                        .iter()
                        .map(|f| (f.name.clone(), f.ty.clone()))
                        .collect();
                    self.type_fields_map.insert(st.ast.name.clone(), fields);
                }
                TypedItem::Actor(a) => {
                    let fields: Vec<(String, Type)> = a
                        .ast
                        .state
                        .iter()
                        .map(|s| (s.name.clone(), s.ty.clone()))
                        .collect();
                    self.type_fields_map.insert(a.ast.name.clone(), fields);
                }
                TypedItem::Function(f) => {
                    if f.ast
                        .attributes
                        .iter()
                        .any(|a| a.name == "blueprint" || a.name == "blueprint_pure")
                    {
                        self.blueprint_fn_names.insert(f.ast.name.clone());
                        // Bug-4 fix: record every named type appearing in this fn's signature
                        // so the blueprint-library preamble can emit only the needed headers.
                        for param in &f.ast.params {
                            collect_type_names(&param.ty, &mut self.blueprint_used_types);
                        }
                        if let Some(ret) = &f.ast.return_type {
                            collect_type_names(ret, &mut self.blueprint_used_types);
                        }
                    }
                }
                _ => {}
            }
        }

        // Collect impl block methods keyed by target type name.
        // This allows gen_ucomponent/gen_usubsystem to look up lifecycle method bodies
        // (begin_play, tick, initialize, etc.) and emit real C++ implementations.
        self.impl_methods.clear();
        for item in program.items() {
            if let TypedItem::Impl(imp) = item {
                if let Type::Named { name, .. } = &imp.ast.target_type {
                    self.impl_methods
                        .entry(name.clone())
                        .or_insert_with(Vec::new)
                        .extend(imp.ast.methods.iter().cloned());
                }
            }
        }

        // Separate items by type for proper ordering
        let mut delegates = Vec::new();
        let mut blueprint_funcs = Vec::new();
        let mut other_items = Vec::new();
        let mut delegate_indices: std::collections::HashMap<(String, &'static str), usize> =
            std::collections::HashMap::new();
        let mut blueprint_indices: std::collections::HashMap<(String, &'static str), usize> =
            std::collections::HashMap::new();
        let mut other_indices: std::collections::HashMap<(String, &'static str), usize> =
            std::collections::HashMap::new();

        // Check if we're in blueprint-library-only mode
        let blueprint_library_only = self
            .target_item
            .as_ref()
            .map(|t| t == "__BLUEPRINT_LIBRARY_ONLY__")
            .unwrap_or(false);

        for item in program.items() {
            let item_name = self.item_symbol_name(item);

            // STDLIB POLLUTION FIX: Skip items marked @stdlib_optional unless they're explicitly referenced
            // This prevents stdlib pattern types (EBuffType, ELootRarity, etc.) from polluting every plugin
            let has_stdlib_optional = match item {
                TypedItem::Struct(s) => {
                    s.ast.attributes.iter().any(|a| a.name == "stdlib_optional")
                }
                // Note: Enum doesn't have attributes field in AST, so we can't filter enums this way
                // TODO: Add attributes support to Enum in parser
                _ => false,
            };

            if has_stdlib_optional {
                // Check if this type is actually referenced by user code
                // For now, skip all @stdlib_optional types (they'll be added back if needed via tree-shaking)
                continue;
            }

            if let Some(target) = &self.target_item {
                // If we're in blueprint-library-only mode, only collect blueprint functions
                if blueprint_library_only {
                    if let TypedItem::Function(f) = item {
                        if f.ast.attributes.iter().any(|a| {
                            a.name == "blueprint" || a.name == "blueprint_pure" || a.name == "ue5"
                        }) {
                            self.push_unique_item(
                                &mut blueprint_funcs,
                                &mut blueprint_indices,
                                item,
                            );
                        }
                    }
                    continue;
                }

                // Otherwise, normal filtering by item name
                if !item_name.is_empty() && item_name != target {
                    continue;
                }
            }

            match item {
                TypedItem::TypeAlias(alias) => {
                    // Check if this is a delegate (function type)
                    if let Type::Function { .. } = &alias.ast.target {
                        self.push_unique_item(&mut delegates, &mut delegate_indices, item);
                    } else {
                        self.push_unique_item(&mut other_items, &mut other_indices, item);
                    }
                }
                TypedItem::Function(f) => {
                    if f.ast.attributes.iter().any(|a| {
                        a.name == "blueprint" || a.name == "blueprint_pure" || a.name == "ue5"
                    }) {
                        self.push_unique_item(&mut blueprint_funcs, &mut blueprint_indices, item);
                    } else {
                        self.push_unique_item(&mut other_items, &mut other_indices, item);
                    }
                }
                _ => self.push_unique_item(&mut other_items, &mut other_indices, item),
            }
        }

        // Generate delegates FIRST (before any structs/components that use them)
        // Skip if we're in blueprint-library-only mode
        if !blueprint_library_only {
            for item in &delegates {
                self.gen_item(item);
            }
            if !delegates.is_empty() {
                self.write_blank_header();
            }
        }

        // Generate non-function items (actors, structs, enums, components)
        // Skip if we're in blueprint-library-only mode
        // Count actors from the FULL program (not filtered other_items) so sliced mode
        // doesn't falsely think there's only 1 actor and auto-wire all compute shaders.
        let actor_count = program
            .items()
            .iter()
            .filter(|i| matches!(i, TypedItem::Actor(_)))
            .count();
        if !blueprint_library_only {
            for item in &other_items {
                match item {
                    TypedItem::Actor(actor_typed) => {
                        // Only pass COMPUTE shaders to actor codegen for RDG dispatch.
                        // Fragment/vertex shaders are used via materials, not direct dispatch.
                        let all_compute: Vec<&TypedShader> = shaders
                            .iter()
                            .filter(|s| s.ast.stage == kain_core::ast::ShaderStage::Compute)
                            .copied()
                            .collect();

                        // Data-driven: check for @dispatch("ShaderA", "ShaderB") attribute
                        let dispatch_attr = actor_typed
                            .ast
                            .attributes
                            .iter()
                            .find(|a| a.name == "dispatch");

                        let (actor_shaders, actor_shader_names): (Vec<&TypedShader>, Vec<String>) =
                            if let Some(attr) = dispatch_attr {
                                // Explicit opt-in: only wire named shaders
                                let requested: Vec<String> = attr
                                    .args
                                    .iter()
                                    .filter_map(|arg| {
                                        if let kain_core::ast::Expr::String(s, _) = arg {
                                            Some(s.clone())
                                        } else {
                                            None
                                        }
                                    })
                                    .collect();
                                let filtered: Vec<&TypedShader> = all_compute
                                    .iter()
                                    .filter(|s| {
                                        requested.iter().any(|req| {
                                            req.eq_ignore_ascii_case(&s.ast.name)
                                                || req.replace("_", "").eq_ignore_ascii_case(
                                                    &s.ast.name.replace("_", ""),
                                                )
                                        })
                                    })
                                    .copied()
                                    .collect();
                                let names: Vec<String> = filtered
                                    .iter()
                                    .map(|s| {
                                        requested
                                            .iter()
                                            .find(|req| {
                                                req.eq_ignore_ascii_case(&s.ast.name)
                                                    || req.replace("_", "").eq_ignore_ascii_case(
                                                        &s.ast.name.replace("_", ""),
                                                    )
                                            })
                                            .cloned()
                                            .unwrap_or_else(|| s.ast.name.clone())
                                    })
                                    .collect();
                                (filtered, names)
                            } else {
                                // No @dispatch attribute → don't wire any shaders.
                                // Actors must explicitly opt-in via @dispatch("ShaderA", "ShaderB").
                                (Vec::new(), Vec::new())
                            };

                        self.gen_actor_with_shaders(
                            &actor_typed.ast,
                            &actor_shaders,
                            &actor_shader_names,
                        )
                    }
                    TypedItem::Struct(st) => {
                        let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                        let is_subsystem = st.ast.attributes.iter().any(|a| a.name == "subsystem");
                        if is_component {
                            self.gen_ucomponent(&st.ast);
                        } else if is_subsystem {
                            self.gen_usubsystem(&st.ast);
                        } else {
                            self.gen_ustruct(&st.ast);
                        }
                    }
                    TypedItem::Enum(en) => self.gen_uenum(&en.ast),
                    TypedItem::Function(fn_typed) => self.gen_ufunction(&fn_typed.ast),
                    TypedItem::Impl(im) => self.gen_impl(&im.ast),
                    _ => {} // Skip shaders and type aliases (already handled)
                }
                self.write_blank_header();
            }
        }

        // Generate function library class if we have blueprint functions
        if !blueprint_funcs.is_empty() {
            self.header.push_line("UCLASS()");
            let lib_class = format!("U{}FunctionLibrary", self.module_name);
            self.write_header(&format!(
                "class {} {} : public UBlueprintFunctionLibrary",
                self.context.module_api, lib_class
            ));
            self.write_header("{");
            self.push_indent();
            self.write_header("GENERATED_BODY()");
            self.write_blank_header();
            self.write_header("public:");
            self.push_indent();

            for item in &blueprint_funcs {
                self.gen_item(item);
            }

            self.pop_indent();
            self.pop_indent();
            self.write_header("};");
            self.write_blank_header();
        }

        // Generate module registration code if we have shaders
        // NOTE: Shader directory registration is handled by static initializers in each shader .cpp file
        // This avoids the "already registered" assertion error
        // IMPORTANT: Only generate IMPLEMENT_MODULE in monolithic mode (when target_item is None)
        // In modular mode, the packager generates a separate module file
        let is_modular_mode = self.target_item.is_some();
        if !shaders.is_empty() && !is_modular_mode {
            self.write_blank_source();
            self.write_source("// Module implementation");
            self.write_source("// Shader directory registration is handled by static initializers in shader .cpp files");
            self.write_blank_source();
            self.write_source(&format!(
                "class F{}Module : public IModuleInterface",
                self.context.output_name
            ));
            self.write_source("{");
            self.write_source("public:");
            self.push_indent();
            self.write_source("virtual void StartupModule() override");
            self.write_source("{");
            self.push_indent();
            self.write_source("// Shader directory mapping is registered by static initializers");
            self.write_source("// in each shader .cpp file before IMPLEMENT_GLOBAL_SHADER runs");
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
            self.write_source("virtual void ShutdownModule() override");
            self.write_source("{");
            self.push_indent();
            self.write_source("// Cleanup if needed");
            self.pop_indent();
            self.write_source("}");
            self.pop_indent();
            self.write_source("};");
            self.write_blank_source();
            self.write_source(&format!(
                "IMPLEMENT_MODULE(F{}Module, {})",
                self.context.output_name, self.context.output_name
            ));
        }

        // Return separate files including shaders and other auxiliary generated artifacts.
        // NOTE: `shader_files` is currently the generic sidecar file channel consumed by
        // callers, so async/state-machine artifacts are emitted here as well.
        let mut shader_files = Vec::new();

        // Generate state machine artifacts
        for item in program.items() {
            if let TypedItem::StateMachine(state_machine_def) = item {
                // In sliced mode, only emit artifacts for the requested target item.
                if let Some(target) = &self.target_item {
                    if &state_machine_def.name != target {
                        continue;
                    }
                }

                match crate::state_machine_ir::convert_to_state_machine_ir(
                    state_machine_def,
                    &self.context,
                ) {
                    Ok(ir) => {
                        let output = crate::state_machine_codegen::generate_state_machine_code(
                            &ir,
                            &self.module_name,
                        );
                        shader_files.push((format!("{}.h", ir.name), output.header));
                        shader_files.push((format!("{}.cpp", ir.name), output.source));
                    }
                    Err(e) => {
                        // State machine generation failed - skipping
                    }
                }
            }
        }

        // Generate async task artifacts
        for item in program.items() {
            if let TypedItem::AsyncTask(async_task_def) = item {
                // In sliced mode, only emit artifacts for the requested target item.
                if let Some(target) = &self.target_item {
                    if &async_task_def.name != target {
                        continue;
                    }
                }

                match crate::async_task_ir::convert_to_async_task_ir(async_task_def, &self.context)
                {
                    Ok(ir) => {
                        let task_name = ir.task_name.clone();
                        let output = crate::async_task_codegen::generate_async_task_code(
                            &ir,
                            &self.module_name,
                        );
                        shader_files.push((format!("{}.h", task_name), output.task_header));
                        shader_files.push((format!("{}.cpp", task_name), output.task_source));
                        shader_files
                            .push((format!("{}TaskQueue.h", task_name), output.queue_header));
                        shader_files
                            .push((format!("{}TaskQueue.cpp", task_name), output.queue_source));
                    }
                    Err(e) => {
                        // Async task generation failed - skipping
                    }
                }
            }
        }

        // Generate USF files for each shader
        // Note: Shader codegen still uses TypedProgram, so we create a temporary one
        let typed_program = kain_core::types::TypedProgram {
            items: program.items().to_vec(),
        };

        for shader in &shaders {
            let shader_name = &shader.ast.name;

            // Generate USF shader code
            match ue5_shaders::generate_usf(&typed_program) {
                Ok(usf_code) => {
                    shader_files.push((format!("{}.usf", shader_name), usf_code));
                }
                Err(e) => {
                    // USF generation failed - skipping
                }
            }

            // Generate C++ header for shader
            let header_code =
                ue5_shaders::codegen_usf::generate_cpp_header(&typed_program, shader_name);
            shader_files.push((format!("{}.h", shader_name), header_code));

            // Generate C++ implementation for shader
            let cpp_code = ue5_shaders::codegen_usf::generate_cpp_implementation(
                &typed_program,
                shader_name,
                "YourPlugin",
            );
            shader_files.push((format!("{}.cpp", shader_name), cpp_code));
        }

        Ue5Output {
            header: self.header.build(),
            source: self.source.build(),
            shader_files,
        }
    }

    fn gen_item(&mut self, item: &TypedItem) {
        match item {
            TypedItem::Actor(actor_typed) => self.gen_actor(&actor_typed.ast),
            TypedItem::Struct(st) => {
                let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                if is_component {
                    self.gen_ucomponent(&st.ast);
                } else {
                    self.gen_ustruct(&st.ast);
                }
            }
            TypedItem::Enum(en) => self.gen_uenum(&en.ast),
            TypedItem::Function(fn_typed) => self.gen_ufunction(&fn_typed.ast),
            // Traits are filtered out during type checking - should not appear here
            TypedItem::Impl(im) => self.gen_impl(&im.ast),
            TypedItem::TypeAlias(alias) => {
                // Check if this is a delegate by naming convention or type
                // Delegates are function types: type OnEvent = fn(...)
                if let Type::Function { .. } = &alias.ast.target {
                    // Check naming convention: starts with "On" or ends with "Delegate"
                    if alias.ast.name.starts_with("On") || alias.ast.name.ends_with("Delegate") {
                        self.gen_multicast_delegate(&alias.ast);
                    } else {
                        self.gen_delegate(&alias.ast);
                    }
                }
            }
            _ => {} // Skip shaders and traits
        }
    }

    /// Generate UE5 AActor subclass from KAIN actor
    /// If shaders exist in the program, auto-wires shader dispatch to Tick()
    fn gen_actor(&mut self, actor: &Actor) {
        self.gen_actor_with_shaders(actor, &[], &[]);
    }

    fn gen_actor_with_shaders(
        &mut self,
        actor: &Actor,
        shaders: &[&TypedShader],
        shader_file_names: &[String],
    ) {
        let class_name = to_actor_name(&actor.name);

        // Actor generation

        // --- 1. Header Generation ---

        // KAIN MARKER: Embed original KAIN source as comment (if enabled)
        if self.context.marker_config.style != crate::ue5::MarkerStyle::None {
            let marker = crate::ue5::kain_markers::actor_marker(actor, &self.context.marker_config);
            if !marker.is_empty() {
                self.write_header(&marker);
            }
        }

        // Get interface inheritance list
        let interface_list = self.context.get_interface_list(&actor.name);

        // Build UCLASS specifiers — data-driven from attributes
        let mut uclass_specs = Vec::new();
        if let Some(attr) = actor.attributes.iter().find(|a| a.name == "uclass") {
            for arg in &attr.args {
                if let Expr::String(s, _) = arg {
                    uclass_specs.push(s.clone());
                }
            }
        }
        // Default HideCategories for cleaner details panel
        if !uclass_specs.iter().any(|s| s.contains("HideCategories")) {
            uclass_specs.push("HideCategories=(Input, Collision, LOD)".to_string());
        }
        if uclass_specs.is_empty() {
            self.header.push_line("UCLASS()");
        } else {
            self.header
                .push_line(&format!("UCLASS({})", uclass_specs.join(", ")));
        }

        // Determine base class — @base("ACineCameraActor") overrides default AActor
        let base_class = actor
            .attributes
            .iter()
            .find(|a| a.name == "base")
            .and_then(|a| a.args.first())
            .and_then(|arg| {
                if let Expr::String(s, _) = arg {
                    Some(s.clone())
                } else {
                    None
                }
            })
            .unwrap_or_else(|| "AActor".to_string());

        self.write_header(&format!(
            "class {} {} : public {}{}",
            self.context.module_api, class_name, base_class, interface_list
        ));
        self.write_header("{");
        self.push_indent();
        self.write_header("GENERATED_BODY()");
        self.write_blank_header();

        self.write_header("public:");
        self.push_indent();
        self.write_header(&format!("{}();", class_name));
        self.write_header("virtual void BeginPlay() override;");
        self.write_header("virtual void Tick(float DeltaTime) override;");
        self.write_blank_header();

        // State variables — BUG-005/006 fix: read @replicated, @category etc. from attributes
        let has_replicated_state = actor
            .state
            .iter()
            .any(|s| s.attributes.iter().any(|a| a.name == "replicated"));

        for state_decl in &actor.state {
            let mut props: Vec<&str> = vec!["EditAnywhere", "BlueprintReadWrite"];
            let mut category = "Simulation Settings".to_string();
            for attr in &state_decl.attributes {
                match attr.name.as_str() {
                    "replicated" => props.push("Replicated"),
                    "savegame" => props.push("SaveGame"),
                    "transient" => props.push("Transient"),
                    "editdefaults" => {
                        props.retain(|&p| p != "EditAnywhere");
                        props.push("EditDefaultsOnly");
                    }
                    "visibleonly" => {
                        props.retain(|&p| p != "EditAnywhere");
                        props.push("VisibleAnywhere");
                    }
                    "category" => {
                        if let Some(Expr::String(cat, _)) = attr.args.first() {
                            category = cat.clone();
                        }
                    }
                    _ => {}
                }
            }
            self.write_header(&format!(
                "UPROPERTY({}, Category = \"{}\")",
                props.join(", "),
                category
            ));

            let mut cpp_type = self.map_type(&state_decl.ty);

            // Components must be pointers in actor state
            // Use to_component_name for consistent naming
            if let Type::Named { name, .. } = &state_decl.ty {
                if self.context.is_component(name) || name.ends_with("Component") {
                    cpp_type = format!("{}*", to_component_name(name));
                }
            }

            self.write_header(&format!("{} {};", cpp_type, state_decl.name));
            if let Type::Named { name, .. } = &state_decl.ty {
                self.var_types.insert(state_decl.name.clone(), name.clone());
            }
        }

        // Declare GetLifetimeReplicatedProps if any state is @replicated
        if has_replicated_state {
            self.write_blank_header();
            self.write_header("virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;");
        }

        // Double-Buffering Resources
        // We assume ANY actor generated with shaders needs this common pattern for now
        if !shaders.is_empty() {
            self.write_blank_header();
            self.write_header("// Double-buffered Simulation Resources");
            self.write_header("UPROPERTY(Transient)");
            self.write_header("UTextureRenderTarget2D* PositionRT_A;");
            self.write_header("UPROPERTY(Transient)");
            self.write_header("UTextureRenderTarget2D* PositionRT_B;");
            self.write_header("UPROPERTY(Transient)");
            self.write_header("UTextureRenderTarget2D* VelocityRT_A;");
            self.write_header("UPROPERTY(Transient)");
            self.write_header("UTextureRenderTarget2D* VelocityRT_B;");
        }

        // Additional Handlers Declarations
        for handler in &actor.handlers {
            if handler.message_type != "begin_play"
                && handler.message_type != "BeginPlay"
                && handler.message_type != "tick"
                && handler.message_type != "Tick"
            {
                self.gen_message_handler_decl(handler);
            }
        }

        // Actor Methods Declarations
        for method in &actor.methods {
            self.gen_actor_method_decl(method, &class_name);
        }

        self.pop_indent();

        // KAIN MARKER: End marker for actor (if enabled)
        if self.context.marker_config.style != crate::ue5::MarkerStyle::None {
            let end_marker =
                crate::ue5::kain_markers::actor_end_marker(actor, &self.context.marker_config);
            if !end_marker.is_empty() {
                self.write_header(&end_marker);
            }
        }

        self.write_header("};");
        self.write_blank_header();

        // --- 2. Source Implementation ---

        // Constructor
        self.source
            .push_line(&format!("{}::{}()", class_name, class_name));
        self.source.push_line("{");
        self.source
            .push_line("\tPrimaryActorTick.bCanEverTick = true;");

        // Collect component state fields for initialization
        let component_fields: Vec<_> = actor
            .state
            .iter()
            .filter(|state_decl| {
                matches!(&state_decl.ty, Type::Named { name, .. }
                if self.context.is_component(name) || name.ends_with("Component"))
            })
            .collect();

        // Create a default scene root if actor has any component state fields.
        // This ensures the actor has a proper scene hierarchy for attachment.
        if !component_fields.is_empty() {
            self.source.push_line("");
            self.source.push_line("\tRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT(\"DefaultSceneRoot\"));");
        }

        // Initialize component state fields with CreateDefaultSubobject
        for state_decl in &actor.state {
            let cpp_type = self.map_type(&state_decl.ty);

            let is_component_state = matches!(&state_decl.ty, Type::Named { name, .. }
                if self.context.is_component(name) || name.ends_with("Component"));

            if is_component_state {
                let component_class = cpp_type.trim_end_matches('*').trim();
                let component_name = &state_decl.name;
                let text_name = to_pascal_case(component_name);

                self.source.push_line(&format!(
                    "\t{} = CreateDefaultSubobject<{}>(TEXT(\"{}\"));",
                    component_name, component_class, text_name
                ));
            }
        }

        // Set replication on actor if it has replicated state
        if has_replicated_state {
            self.source.push_line("");
            self.source.push_line("\tbReplicates = true;");
        }

        self.source.push_line("}");
        self.write_blank_source();

        // GetLifetimeReplicatedProps implementation for @replicated state fields
        if has_replicated_state {
            self.source.push_line(&format!("void {}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const", class_name));
            self.source.push_line("{");
            self.source
                .push_line("\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);");
            self.source.push_line("");
            for s in &actor.state {
                if s.attributes.iter().any(|a| a.name == "replicated") {
                    self.source
                        .push_line(&format!("\tDOREPLIFETIME({}, {});", class_name, s.name));
                }
            }
            self.source.push_line("}");
            self.write_blank_source();
        }

        // BeginPlay
        self.source
            .push_line(&format!("void {}::BeginPlay()", class_name));
        self.source.push_line("{");
        self.source.push_line("\tSuper::BeginPlay();");

        if !shaders.is_empty() {
            // Bug-14 fix: Transient UTextureRenderTarget2D* members are null by default.
            // Without explicit init in BeginPlay(), the Bug-13 null guard fires every frame
            // and the simulation never runs.
            for rt_name in &[
                "PositionRT_A",
                "PositionRT_B",
                "VelocityRT_A",
                "VelocityRT_B",
            ] {
                self.source.push_line(&format!("\tif (!{})", rt_name));
                self.source.push_line("\t{");
                self.source.push_line(&format!(
                    "\t\t{} = NewObject<UTextureRenderTarget2D>(this);",
                    rt_name
                ));
                self.source
                    .push_line(&format!("\t\t{}->bAutoGenerateMips = false;", rt_name));
                self.source.push_line(&format!(
                    "\t\t{}->RenderTargetFormat = RTF_RGBA32f;",
                    rt_name
                ));
                self.source
                    .push_line(&format!("\t\t{}->InitAutoFormat(512, 512);", rt_name));
                self.source
                    .push_line(&format!("\t\t{}->UpdateResourceImmediate(true);", rt_name));
                self.source.push_line("\t}");
            }
        }

        // User defined begin_play logic
        if let Some(handler) = actor
            .handlers
            .iter()
            .find(|h| h.message_type == "begin_play" || h.message_type == "BeginPlay")
        {
            self.with_var_type_scope(|this| {
                this.register_param_types(&handler.params);
                this.gen_block_source(&handler.body);
            });
        }

        self.source.push_line("}");
        self.write_blank_source();

        // Tick
        self.source
            .push_line(&format!("void {}::Tick(float DeltaTime)", class_name));
        self.source.push_line("{");
        self.source.push_line("\tSuper::Tick(DeltaTime);");

        // User defined tick logic
        if let Some(handler) = actor
            .handlers
            .iter()
            .find(|h| h.message_type == "tick" || h.message_type == "Tick")
        {
            for param in &handler.params {
                if param.name == "delta_time" || param.name == "dt" || param.name == "delta" {
                    self.context
                        .add_ident_remap(param.name.clone(), "DeltaTime".to_string());
                }
            }
            self.with_var_type_scope(|this| {
                this.register_param_types(&handler.params);
                this.gen_block_source(&handler.body);
            });
            self.context.clear_ident_remaps();
        }

        if !shaders.is_empty() {
            self.source.push_line("");
            self.source
                .push_line("\t// Enqueue Simulation on Render Thread");
            self.source
                .push_line("\tENQUEUE_RENDER_COMMAND(SimulationTick)(");
            self.source
                .push_line("\t\t[this, DeltaTime](FRHICommandListImmediate& RHICmdList) {");
            // Bug-13 fix: null checks MUST come before FRDGBuilder construction.
            // Creating the builder and then returning early without calling Execute()
            // triggers ensure(bHasExecuted) in RenderGraphValidation.cpp.
            self.source.push_line("\t\t\tif (!PositionRT_A || !PositionRT_B || !VelocityRT_A || !VelocityRT_B) { return; }");
            self.source.push_line("");
            self.source
                .push_line("\t\t\tFRDGBuilder GraphBuilder(RHICmdList);");
            self.source.push_line("");

            // Resource Registration Logic
            self.source
                .push_line("\t\t\t// 1. Register External Textures (Ping-Pong Logic)");
            self.source
                .push_line("\t\t\tbool bOddFrame = GFrameNumberRenderThread % 2 != 0;");

            // Static helper for RT to RDG conversion
            self.source.push_line("\t\t\tauto CreateRenderTarget = [&](FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* RT, const TCHAR* Name) -> TRefCountPtr<IPooledRenderTarget> {");
            self.source
                .push_line("\t\t\t\tif (!RT || !RT->GetResource()) return nullptr;");
            self.source.push_line(
                "\t\t\t\tFTexture2DRHIRef TextureRHI = RT->GetResource()->GetTexture2DRHI();",
            );
            self.source.push_line(
                "\t\t\t\tFPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(",
            );
            self.source
                .push_line("\t\t\t\t\tFIntPoint(RT->SizeX, RT->SizeY),");
            self.source.push_line("\t\t\t\t\tTextureRHI->GetFormat(),");
            self.source.push_line("\t\t\t\t\tFClearValueBinding::None,");
            self.source.push_line("\t\t\t\t\tTexCreate_None,");
            self.source.push_line(
                "\t\t\t\t\tTexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable,",
            );
            self.source.push_line("\t\t\t\t\tfalse");
            self.source.push_line("\t\t\t\t);");
            self.source
                .push_line("\t\t\t\tTRefCountPtr<IPooledRenderTarget> PooledRT;");
            self.source
                .push_line("\t\t\t\tFSceneRenderTargetItem Item;");
            self.source
                .push_line("\t\t\t\tItem.TargetableTexture = (FTextureRHIRef)TextureRHI;");
            self.source
                .push_line("\t\t\t\tItem.ShaderResourceTexture = (FTextureRHIRef)TextureRHI;");
            self.source.push_line(
                "\t\t\t\tGRenderTargetPool.CreateUntrackedElement(Desc, PooledRT, Item);",
            );
            self.source.push_line("\t\t\t\treturn PooledRT;");
            self.source.push_line("\t\t\t};");
            self.source.push_line("");

            self.source.push_line("\t\t\tFRDGTextureRef PositionInput = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHICmdList, bOddFrame ? PositionRT_A : PositionRT_B, TEXT(\"PosIn\")));");
            self.source.push_line("\t\t\tFRDGTextureRef PositionOutput = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHICmdList, bOddFrame ? PositionRT_B : PositionRT_A, TEXT(\"PosOut\")));");
            self.source.push_line("\t\t\tFRDGTextureRef VelocityInput = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHICmdList, bOddFrame ? VelocityRT_A : VelocityRT_B, TEXT(\"VelIn\")));");
            self.source.push_line("\t\t\tFRDGTextureRef VelocityOutput = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RHICmdList, bOddFrame ? VelocityRT_B : VelocityRT_A, TEXT(\"VelOut\")));");
            self.source.push_line("");

            // Analyze shader pipeline to determine which intermediate RTs are needed
            let mut needed_intermediates = std::collections::HashSet::new();
            for shader in shaders.iter() {
                for uniform in &shader.ast.uniforms {
                    if let Type::Named { name, .. } = &uniform.ty {
                        if name == "Sampler2D"
                            || name == "Texture2D"
                            || name == "Texture3D"
                            || name == "TextureCube"
                        {
                            // This shader needs an input texture - mark it as needed
                            needed_intermediates.insert(uniform.name.clone());
                        }
                    }
                }
            }

            // Create intermediate render targets for shader pipeline chaining
            if !needed_intermediates.is_empty() {
                self.source
                    .push_line("\t\t\t// Create intermediate render targets for shader pipeline");
                self.source.push_line(
                    "\t\t\tFRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create2D(",
                );
                self.source.push_line("\t\t\t\tFIntPoint(1024, 1024),");
                self.source.push_line("\t\t\t\tPF_FloatRGBA,");
                self.source.push_line("\t\t\t\tFClearValueBinding::Black,");
                self.source.push_line(
                    "\t\t\t\tTexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable",
                );
                self.source.push_line("\t\t\t);");
                self.source.push_line("");

                // Create intermediate RTs for common shader outputs
                for intermediate_name in &needed_intermediates {
                    let rt_name = format!(
                        "{}RT",
                        intermediate_name
                            .chars()
                            .next()
                            .unwrap()
                            .to_uppercase()
                            .to_string()
                            + &intermediate_name[1..]
                    );
                    self.source.push_line(&format!("\t\t\tFRDGTextureRef {} = GraphBuilder.CreateTexture(IntermediateDesc, TEXT(\"{}\"));", rt_name, intermediate_name));
                }
                self.source.push_line("");
            }

            // Dispatch Calls
            // Zip shaders with their file names to ensure correct casing in function calls
            for (shader, shader_file_name) in shaders.iter().zip(shader_file_names.iter()) {
                let shader_name = &shader.ast.name;

                // Keep AddPass_* call argument order aligned with ue5-shaders helper signatures:
                // scalars first, then textures/UAVs (same contract used by generated .h/.cpp wrappers).
                let mut call_args: Vec<String> = Vec::new();
                let mut has_output_texture_arg = false;
                let mut has_explicit_texture_uniform = false;
                // POD population lines emitted just before the AddPass_ call.
                let mut pod_prep_lines: Vec<String> = Vec::new();

                let mut ordered_uniforms: Vec<&kain_core::ast::Uniform> = Vec::new();
                for uniform in &shader.ast.uniforms {
                    let is_permutation_uniform = {
                        let n = uniform.name.as_str();
                        n.starts_with("CFG_")
                            || n.starts_with("ENABLE_")
                            || n.starts_with("USE_")
                            || n.starts_with("WITH_")
                            || n.starts_with("HAS_")
                            || n.starts_with("ALLOW_")
                            || n.starts_with("SUPPORT_")
                    };
                    if is_permutation_uniform {
                        continue;
                    }
                    let uniform_type_name = if let Type::Named { name, .. } = &uniform.ty {
                        Some(name.as_str())
                    } else {
                        None
                    };
                    let is_texture = matches!(
                        uniform_type_name,
                        Some(
                            "Sampler2D"
                                | "Sampler3D"
                                | "SamplerCube"
                                | "Texture2D"
                                | "RWTexture2D"
                                | "Texture3D"
                                | "RWTexture3D"
                                | "Image2D"
                                | "Image3D"
                                | "Buffer"
                                | "RWBuffer"
                                | "StructuredBuffer"
                                | "RWStructuredBuffer"
                                | "TextureCube"
                                | "RWTextureCube"
                        )
                    );
                    let is_buffer_srv = matches!(
                        uniform_type_name,
                        Some("Buffer" | "RWBuffer" | "StructuredBuffer" | "RWStructuredBuffer")
                    );
                    if !is_texture {
                        ordered_uniforms.push(uniform);
                    }
                }
                for uniform in &shader.ast.uniforms {
                    let is_permutation_uniform = {
                        let n = uniform.name.as_str();
                        n.starts_with("CFG_")
                            || n.starts_with("ENABLE_")
                            || n.starts_with("USE_")
                            || n.starts_with("WITH_")
                            || n.starts_with("HAS_")
                            || n.starts_with("ALLOW_")
                            || n.starts_with("SUPPORT_")
                    };
                    if is_permutation_uniform {
                        continue;
                    }
                    let uniform_type_name = if let Type::Named { name, .. } = &uniform.ty {
                        Some(name.as_str())
                    } else {
                        None
                    };
                    let is_texture = matches!(
                        uniform_type_name,
                        Some(
                            "Sampler2D"
                                | "Sampler3D"
                                | "SamplerCube"
                                | "Texture2D"
                                | "RWTexture2D"
                                | "Texture3D"
                                | "RWTexture3D"
                                | "Image2D"
                                | "Image3D"
                                | "Buffer"
                                | "RWBuffer"
                                | "StructuredBuffer"
                                | "RWStructuredBuffer"
                                | "TextureCube"
                                | "RWTextureCube"
                        )
                    );
                    let is_buffer_srv = matches!(
                        uniform_type_name,
                        Some("Buffer" | "RWBuffer" | "StructuredBuffer" | "RWStructuredBuffer")
                    );
                    if is_texture {
                        ordered_uniforms.push(uniform);
                    }
                }

                for uniform in ordered_uniforms {
                    let is_permutation_uniform = {
                        let n = uniform.name.as_str();
                        n.starts_with("CFG_")
                            || n.starts_with("ENABLE_")
                            || n.starts_with("USE_")
                            || n.starts_with("WITH_")
                            || n.starts_with("HAS_")
                            || n.starts_with("ALLOW_")
                            || n.starts_with("SUPPORT_")
                    };
                    if is_permutation_uniform {
                        continue;
                    }
                    let name_lower = uniform.name.to_lowercase();

                    // Texture uniforms should be classified by declared type, not name heuristics.
                    // Name-based checks (e.g. "tex") misclassify fields like `vertex_count`.
                    let uniform_type_name = if let Type::Named { name, .. } = &uniform.ty {
                        Some(name.as_str())
                    } else {
                        None
                    };
                    let is_texture = matches!(
                        uniform_type_name,
                        Some(
                            "Sampler2D"
                                | "Sampler3D"
                                | "SamplerCube"
                                | "Texture2D"
                                | "RWTexture2D"
                                | "Texture3D"
                                | "RWTexture3D"
                                | "Image2D"
                                | "Image3D"
                                | "Buffer"
                                | "RWBuffer"
                                | "StructuredBuffer"
                                | "RWStructuredBuffer"
                                | "TextureCube"
                                | "RWTextureCube"
                        )
                    );
                    let is_buffer_srv = matches!(
                        uniform_type_name,
                        Some("Buffer" | "RWBuffer" | "StructuredBuffer" | "RWStructuredBuffer")
                    );

                    if is_texture {
                        has_explicit_texture_uniform = true;
                        // Texture Param Logic - Modular mapping by name
                        let mut matched_texture = false;
                        if is_buffer_srv {
                            // Generated AddPass_* helpers use FRHIShaderResourceView* for buffer-like
                            // inputs; avoid passing FRDGTextureRef placeholders.
                            call_args.push("nullptr".to_string());
                            matched_texture = true;
                        } else if name_lower.contains("position") {
                            if name_lower.contains("output") || name_lower.contains("write") {
                                call_args.push("PositionOutput".to_string());
                                has_output_texture_arg = true;
                            } else {
                                call_args.push("PositionInput".to_string());
                            }
                            matched_texture = true;
                        } else if name_lower.contains("velocity") {
                            if name_lower.contains("output") || name_lower.contains("write") {
                                call_args.push("VelocityOutput".to_string());
                                has_output_texture_arg = true;
                            } else {
                                call_args.push("VelocityInput".to_string());
                            }
                            matched_texture = true;
                        } else {
                            // For fragment shaders with Sampler2D uniforms, try to match to intermediate RTs
                            // Pattern: thermal -> ThermalRT, moisture -> MoistureRT, albedo -> AlbedoRT, etc.
                            let rt_name = format!(
                                "{}RT",
                                uniform
                                    .name
                                    .chars()
                                    .next()
                                    .unwrap()
                                    .to_uppercase()
                                    .to_string()
                                    + &uniform.name[1..]
                            );

                            // Check if this intermediate RT was created
                            if needed_intermediates.contains(&uniform.name) {
                                if rt_name.contains("Output") {
                                    has_output_texture_arg = true;
                                }
                                call_args.push(rt_name);
                                matched_texture = true;
                            }
                        }

                        if !matched_texture {
                            // For unmatched texture uniforms, use PositionOutput as a generic fallback
                            // This allows fragment shaders to compile even without explicit RT mappings
                            call_args.push("PositionOutput".to_string());
                            has_output_texture_arg = true;
                        }
                    } else {
                        // Check first if this uniform is a @component type that needs a POD mirror.
                        let component_type_name =
                            if let Type::Named { name, generics, .. } = &uniform.ty {
                                if generics.is_empty()
                                    && self.component_mirrors.contains_key(name.as_str())
                                {
                                    Some(name.clone())
                                } else {
                                    None
                                }
                            } else {
                                None
                            };

                        if let Some(comp_name) = component_type_name {
                            // Component uniform: generate POD population code and pass the POD var.
                            let mirror = &self.component_mirrors[&comp_name];
                            let pod_var = format!("{}_pod", uniform.name);

                            // Level 1: direct actor state field with matching name.
                            let mut state_var = actor
                                .state
                                .iter()
                                .find(|s| s.name.eq_ignore_ascii_case(&uniform.name))
                                .map(|s| format!("this->{}", s.name));

                            // Level 2: depth-1 path — walk each actor state field whose type
                            // has a sub-field matching the uniform name.
                            // e.g. HyperFluidController.world (HyperFluidSimulationCore)
                            //        └── world.physics  (PhysicalPropertiesComponent) ✓
                            if state_var.is_none() {
                                'outer: for st in &actor.state {
                                    if let Type::Named {
                                        name: type_name, ..
                                    } = &st.ty
                                    {
                                        if let Some(sub_fields) =
                                            self.type_fields_map.get(type_name.as_str())
                                        {
                                            for (field_name, _) in sub_fields {
                                                if field_name.eq_ignore_ascii_case(&uniform.name) {
                                                    state_var = Some(format!(
                                                        "this->{}->{}",
                                                        st.name, uniform.name
                                                    ));
                                                    break 'outer;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            let state_var = state_var.unwrap_or_else(|| "nullptr".to_string());
                            pod_prep_lines.push(
                                mirror.generate_population_code(&state_var, &pod_var, "\t\t\t"),
                            );
                            call_args.push(pod_var);
                        } else {
                            // Scalar Param - Modular exact name matching with common aliases
                            let mut found_match = false;
                            for state in &actor.state {
                                let is_match = match uniform.name.to_lowercase().as_str() {
                                    "dt" => {
                                        state.name.to_lowercase().contains("time_step")
                                            || state.name.eq_ignore_ascii_case("dt")
                                    }
                                    "res" => {
                                        state.name.to_lowercase().contains("resolution")
                                            || state.name.eq_ignore_ascii_case("res")
                                    }
                                    "source_pos" | "pos" => {
                                        state.name.to_lowercase().contains("position")
                                            || state.name.eq_ignore_ascii_case("pos")
                                    }
                                    "source_val" | "vel" => {
                                        (state.name.to_lowercase().contains("velocity")
                                            || state.name.eq_ignore_ascii_case("vel")
                                            || state.name.to_lowercase().contains("value"))
                                            && !state.name.to_lowercase().contains("damping")
                                    }
                                    _ => state.name.eq_ignore_ascii_case(&uniform.name),
                                };

                                if is_match {
                                    // Cast to the right type (usually FVector -> FVector3f)
                                    let cast = match self.map_type(&state.ty).as_str() {
                                        "FVector" => "FVector3f",
                                        "FVector2D" => "FVector2f",
                                        "FVector4" => "FVector4f",
                                        _ => "",
                                    };
                                    // Handle Enum casting
                                    let is_enum = if let Type::Named { name, .. } = &state.ty {
                                        self.context.enum_names.contains(name)
                                    } else {
                                        false
                                    };

                                    if !cast.is_empty() {
                                        call_args.push(format!("{}(this->{})", cast, state.name));
                                    } else if is_enum {
                                        call_args.push(format!(
                                            "static_cast<int32>(this->{})",
                                            state.name
                                        ));
                                    } else {
                                        call_args.push(format!("this->{}", state.name));
                                    }
                                    found_match = true;
                                    break;
                                }
                            }
                            if !found_match {
                                // Fallback with correct type initialization
                                let fallback = match self.map_type(&uniform.ty).as_str() {
                                    "FVector" | "FVector3f" => {
                                        "FVector3f(0.0f, 0.0f, 0.0f)".to_string()
                                    }
                                    "FVector2D" | "FVector2f" => {
                                        "FVector2f(0.0f, 0.0f)".to_string()
                                    }
                                    "FVector4" | "FVector4f" => {
                                        "FVector4f(0.0f, 0.0f, 0.0f, 0.0f)".to_string()
                                    }
                                    "FIntVector" => "FIntVector(0, 0, 0)".to_string(),
                                    _ => "0.0f".to_string(),
                                };
                                call_args.push(fallback);
                            }
                        } // end scalar (non-component) branch
                    }
                }

                // Output UAV Handling - ALL compute shaders need OutputTexture
                // Check if this is a compute shader
                let is_compute = matches!(shader.ast.stage, ShaderStage::Compute);

                if is_compute {
                    // Bug-5 fix: the uniforms loop above may have already added the output
                    // texture when the shader declares it explicitly (e.g. `position_output:
                    // Sampler2D`). Only apply the name-heuristic fallback when the loop
                    // didn't already add ANY output/RT texture — prevents duplicate args that
                    // cause a C++ "too many arguments" error on the AddPass_ call site.
                    // Also skip this fallback when the shader already has explicit texture/UAV
                    // uniforms, because helper signatures already account for those slots.
                    if !has_output_texture_arg && !has_explicit_texture_uniform {
                        let shader_name_lower = shader_name.to_lowercase();
                        if shader_name_lower.contains("position") {
                            call_args.push("PositionOutput".to_string());
                        } else if shader_name_lower.contains("velocity") {
                            call_args.push("VelocityOutput".to_string());
                        } else if shader_name_lower.contains("thermal") {
                            call_args.push("ThermalRT".to_string());
                        } else if shader_name_lower.contains("moisture") {
                            call_args.push("MoistureRT".to_string());
                        } else if shader_name_lower.contains("albedo") {
                            call_args.push("AlbedoRT".to_string());
                        } else if shader_name_lower.contains("lights")
                            || shader_name_lower.contains("city")
                        {
                            call_args.push("LightsRT".to_string());
                        } else {
                            call_args.push("PositionOutput".to_string());
                        }
                    }

                    // GroupCount is always the final arg for compute shaders
                    call_args.push("FIntVector(32, 32, 1)".to_string());
                }

                // Don't add nullptr for output textures - the shader functions don't expect them
                // The USF codegen handles output textures internally

                // Open a nested block scope so each shader's POD variables are isolated.
                // This prevents redeclaration errors when multiple shaders share the same
                // uniform name (e.g. "physics_pod") inside the same render lambda.
                let needs_scope = !pod_prep_lines.is_empty();
                if needs_scope {
                    self.source.push_line("\t\t\t{");
                }

                // Emit POD population code for any component uniforms before the dispatch call.
                for prep in &pod_prep_lines {
                    // generate_population_code already includes the indent prefix.
                    self.source.push_line(prep.trim_end());
                }

                // Use shader_file_name (from toml) instead of AST name to preserve correct casing
                self.source.push_line(&format!(
                    "\t\t\tAddPass_{}(GraphBuilder, {});",
                    shader_file_name,
                    call_args.join(", ")
                ));

                if needs_scope {
                    self.source.push_line("\t\t\t}");
                }
            }

            self.source.push_line("");
            self.source.push_line("\t\t\tGraphBuilder.Execute();");
            self.source.push_line("\t\t}");
            self.source.push_line("\t);");
        }

        self.source.push_line("}");
        self.write_blank_source();

        // Implement extra handlers
        for handler in &actor.handlers {
            if handler.message_type != "begin_play"
                && handler.message_type != "BeginPlay"
                && handler.message_type != "tick"
                && handler.message_type != "Tick"
            {
                self.gen_message_handler_impl(&class_name, handler);
            }
        }

        // Implement methods
        for method in &actor.methods {
            self.gen_actor_method_impl(&class_name, method);
        }
    }

    /// Get default value for a type (used when shader uniform has no matching actor state)
    fn get_default_value_for_type(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, .. } => {
                match name.as_str() {
                    "Float" => "0.0f".to_string(),
                    "Int" => "0".to_string(),
                    "Bool" => "false".to_string(),
                    "Vec2" => "FVector2f(0.0f, 0.0f)".to_string(),
                    "Vec3" => "FVector3f(0.0f, 0.0f, 0.0f)".to_string(),
                    "Vec4" => "FVector4f(0.0f, 0.0f, 0.0f, 0.0f)".to_string(),
                    "Sampler2D" | "Texture2D" => "nullptr".to_string(),
                    "RWBuffer" | "RWTexture2D" => "nullptr".to_string(),
                    _ => format!("{}()", self.map_type(ty)), // Use explicit default constructor
                }
            }
            _ => "0".to_string(), // Safe numeric default for unknown types
        }
    }

    fn gen_message_handler_decl(&mut self, handler: &MessageHandler) {
        // MessageHandler has no return type in KAIN - message handlers return void
        let ret_type = "void".to_string();

        // Check if this is an RPC - determined by naming convention
        let msg_lower = handler.message_type.to_lowercase();
        let is_rpc = msg_lower.starts_with("server_")
            || msg_lower.starts_with("client_")
            || msg_lower.starts_with("multicast_");
        let is_server_rpc = msg_lower.starts_with("server_");

        let params = if is_rpc {
            self.gen_params_for_rpc(&handler.params)
        } else {
            self.gen_params(&handler.params)
        };

        if is_rpc {
            let rpc_type = if is_server_rpc {
                "Server, Reliable"
            } else if msg_lower.starts_with("client_") {
                "Client, Reliable"
            } else {
                "NetMulticast, Reliable"
            };

            self.write_header(&format!(
                "UFUNCTION({}, BlueprintCallable, Category = \"{}\")",
                rpc_type, handler.message_type
            ));
        } else {
            self.write_header(&format!(
                "UFUNCTION(BlueprintCallable, Category = \"{}\")",
                handler.message_type
            ));
        }

        self.write_header(&format!(
            "{} {}({});",
            ret_type, handler.message_type, params
        ));
        self.write_blank_header();

        // Server RPCs need _Validate declaration too
        if is_server_rpc {
            self.write_header(&format!(
                "bool {}_Validate({});",
                handler.message_type, params
            ));
            self.write_blank_header();
        }
    }

    fn gen_message_handler_impl(&mut self, class_name: &str, handler: &MessageHandler) {
        // MessageHandler has no return type in KAIN - message handlers return void
        let ret_type = "void".to_string();

        // Check if this is an RPC to use proper parameter passing
        let msg_lower = handler.message_type.to_lowercase();
        let is_rpc = msg_lower.starts_with("server_")
            || msg_lower.starts_with("client_")
            || msg_lower.starts_with("multicast_");
        let is_server_rpc = msg_lower.starts_with("server_");

        let params = if is_rpc {
            self.gen_params_for_rpc(&handler.params)
        } else {
            self.gen_params(&handler.params)
        };

        // For RPCs, UE5 requires _Implementation suffix
        let method_name = if is_rpc {
            format!("{}_Implementation", handler.message_type)
        } else {
            handler.message_type.clone()
        };

        self.write_source(&format!(
            "{} {}::{}({})",
            ret_type, class_name, method_name, params
        ));
        self.write_source("{");
        self.push_indent();
        self.with_var_type_scope(|this| {
            this.register_param_types(&handler.params);
            this.gen_block_source(&handler.body);
        });
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // Server RPCs also need _Validate method
        if is_server_rpc {
            self.write_source(&format!(
                "bool {}::{}_Validate({})",
                class_name, handler.message_type, params
            ));
            self.write_source("{");
            self.push_indent();
            self.write_source("return true; // Add validation logic here");
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
    }

    /// Generate actor method declaration in header
    fn gen_actor_method_decl(&mut self, method: &Function, class_name: &str) {
        // Check for @blueprint_event attribute first (takes precedence)
        let is_blueprint_event = method.attributes.iter().any(|a| {
            a.name == "blueprint_event"
                || a.name == "blueprint_native_event"
                || a.name == "blueprint_implementable_event"
        });

        if is_blueprint_event {
            // Use blueprint_event codegen for BlueprintNativeEvent functions
            use crate::blueprint_codegen::generate_blueprint_event_code;
            use crate::blueprint_ir::convert_to_blueprint_event_ir;

            match convert_to_blueprint_event_ir(method, &self.context) {
                Ok(event_ir) => {
                    let output = generate_blueprint_event_code(
                        &event_ir,
                        class_name,
                        &self.context.module_api,
                    );
                    // Write the header declaration (UFUNCTION + virtual declaration)
                    self.write_header(&output.header_declaration);
                    self.write_blank_header();
                    return;
                }
                Err(_e) => {
                    // Silently fall through to regular method generation
                }
            }
        }

        // Check for @blueprint_pure or @blueprint_callable attributes
        let is_pure = method
            .attributes
            .iter()
            .any(|a| a.name == "blueprint_pure" || a.name == "pure");
        let is_callable = method
            .attributes
            .iter()
            .any(|a| a.name == "blueprint_callable" || a.name == "blueprint");
        let is_inline = method.attributes.iter().any(|a| a.name == "inline");

        // Determine return type
        let ret_type = if let Some(ref ty) = method.return_type {
            self.map_type(ty)
        } else {
            "void".to_string()
        };

        // Generate parameters
        let params = self.gen_params(&method.params);

        // Extract category and meta specifiers
        let category = method
            .attributes
            .iter()
            .find(|a| a.name == "category")
            .and_then(|a| a.args.first())
            .and_then(|e| {
                if let Expr::String(s, _) = e {
                    Some(s.as_str())
                } else {
                    None
                }
            })
            .unwrap_or("Actor");

        let meta = method
            .attributes
            .iter()
            .find(|a| a.name == "meta")
            .and_then(|a| a.args.first())
            .and_then(|e| {
                if let Expr::String(s, _) = e {
                    Some(format!(", meta = ({})", s))
                } else {
                    None
                }
            })
            .unwrap_or_default();

        // Generate UFUNCTION macro
        if is_pure {
            self.write_header(&format!(
                "UFUNCTION(BlueprintPure, Category = \"{}\"{})",
                category, meta
            ));
            if is_inline {
                // Inline body in header
                self.write_header(&format!(
                    "{} {}({}) const {{ {} }}",
                    ret_type,
                    method.name,
                    params,
                    self.gen_inline_body(&method.body)
                ));
            } else {
                self.write_header(&format!("{} {}({}) const;", ret_type, method.name, params));
            }
        } else if is_callable {
            self.write_header(&format!(
                "UFUNCTION(BlueprintCallable, Category = \"{}\"{})",
                category, meta
            ));
            if is_inline {
                // Inline body in header
                self.write_header(&format!(
                    "{} {}({}) {{ {} }}",
                    ret_type,
                    method.name,
                    params,
                    self.gen_inline_body(&method.body)
                ));
            } else {
                self.write_header(&format!("{} {}({});", ret_type, method.name, params));
            }
        } else {
            // Regular C++ method (no UFUNCTION)
            if is_inline {
                self.write_header(&format!(
                    "{} {}({}) {{ {} }}",
                    ret_type,
                    method.name,
                    params,
                    self.gen_inline_body(&method.body)
                ));
            } else {
                self.write_header(&format!("{} {}({});", ret_type, method.name, params));
            }
        }

        self.write_blank_header();
    }

    /// Generate inline method body for header
    fn gen_inline_body(&self, block: &Block) -> String {
        // For simple return statements, extract the expression
        if block.stmts.len() == 1 {
            if let Stmt::Return(Some(expr), _) = &block.stmts[0] {
                return format!("return {};", self.gen_expr_string(expr));
            }
        }
        // For more complex bodies, we'd need full codegen (skip for now)
        "/* complex body */".to_string()
    }

    /// Generate expression as string (for inline bodies)
    fn gen_expr_string(&self, expr: &Expr) -> String {
        match expr {
            Expr::Ident(name, _) => match name.as_str() {
                "null" => "nullptr".to_string(),
                "None" => "NAME_None".to_string(),
                _ => name.clone(),
            },
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => format!("{:.6}f", f),
            Expr::Bool(b, _) => b.to_string(),
            Expr::String(s, _) => format!("TEXT(\"{}\")", self.escape_string(s)),
            Expr::None(_) => "nullptr".to_string(),
            Expr::Field { object, field, .. } => {
                // KAIN property-style length access: arr.length / arr.len -> arr.Num()
                if field == "length" || field == "len" || field == "count" || field == "size" {
                    return format!("{}.Num()", self.gen_expr_string(object));
                }

                // Remap vector component field names ONLY for UE5 vector types
                // Check if the object is a known vector type before capitalizing
                let should_capitalize = if let Expr::Ident(obj_name, _) = object.as_ref() {
                    // Check if this is a variable with a vector type
                    if let Some(type_name) = self.var_types.get(obj_name) {
                        matches!(
                            type_name.as_str(),
                            "Vec2"
                                | "Vec3"
                                | "Vec4"
                                | "FVector"
                                | "FVector2D"
                                | "FVector4"
                                | "FIntVector"
                                | "FIntPoint"
                        )
                    } else {
                        false
                    }
                } else {
                    false
                };

                let ue5_field = if should_capitalize {
                    match field.as_str() {
                        "x" => "X",
                        "y" => "Y",
                        "z" => "Z",
                        "w" => "W",
                        "r" => "X",
                        "g" => "Y",
                        "b" => "Z",
                        "a" => "W",
                        _ => field.as_str(),
                    }
                } else {
                    field.as_str()
                };

                let access_op = if self.is_pointer_receiver(object) {
                    "->"
                } else {
                    "."
                };
                format!("{}{}{}", self.gen_expr_string(object), access_op, ue5_field)
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let l = self.gen_expr_string(left);
                let r = self.gen_expr_string(right);
                if *op == BinaryOp::Mod
                    && (self.is_likely_float_expr(left) || self.is_likely_float_expr(right))
                {
                    format!(
                        "FMath::Fmod(static_cast<double>({}), static_cast<double>({}))",
                        l, r
                    )
                } else {
                    format!("({} {} {})", l, self.gen_binop_string(*op), r)
                }
            }
            Expr::Unary { op, operand, .. } => {
                let o = self.gen_expr_string(operand);
                let op_str = match op {
                    UnaryOp::Neg => "-",
                    UnaryOp::Not => "!",
                    _ => "?",
                };
                if matches!(op, UnaryOp::Not) {
                    if let Expr::Ident(name, _) = operand.as_ref() {
                        if let Some(ty_name) = self.var_types.get(name) {
                            let is_bool = ty_name == "bool" || ty_name == "Bool";
                            let is_pointer_like = ty_name.ends_with('*')
                                || ty_name.contains('*')
                                || ty_name == "nullptr";
                            if !is_bool && !is_pointer_like {
                                // KAIN truthiness on value-structs has no direct C++ equivalent.
                                // Emit a safe default false guard instead of invalid `!StructValue`.
                                return "false".to_string();
                            }
                        }
                    }
                }
                format!("({}{})", op_str, o)
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let arg_strs: Vec<String> = args
                    .iter()
                    .map(|a| self.gen_expr_string(&a.value))
                    .collect();
                // Map array methods to UE5 equivalents
                let ue5_method = match method.as_str() {
                    "len" | "length" | "count" | "size" => "Num",
                    "push" | "append" | "add" => "Add",
                    "pop" => "Pop",
                    "clear" | "empty" => "Empty",
                    "remove" => "RemoveAt",
                    "contains" => "Contains",
                    _ => method.as_str(),
                };
                format!(
                    "{}.{}({})",
                    self.gen_expr_string(receiver),
                    ue5_method,
                    arg_strs.join(", ")
                )
            }
            Expr::EnumVariant {
                enum_name, variant, ..
            } => {
                let ue_name = to_enum_name(enum_name);
                format!("{}::{}", ue_name, variant)
            }
            Expr::Call { callee, args, .. } => {
                let fn_name = self.gen_expr_string(callee);
                let arg_strs: Vec<String> = args
                    .iter()
                    .map(|a| self.gen_expr_string(&a.value))
                    .collect();
                if fn_name == "not" && arg_strs.len() == 1 {
                    return format!("(!{})", arg_strs[0]);
                }
                // Handle vector constructors
                match fn_name.as_str() {
                    "vec2" => format!("FVector2D({})", arg_strs.join(", ")),
                    "vec3" => format!("FVector({})", arg_strs.join(", ")),
                    "vec4" => format!("FVector4({})", arg_strs.join(", ")),
                    "Vec2" => format!("FVector2D({})", arg_strs.join(", ")),
                    "Vec3" => format!("FVector({})", arg_strs.join(", ")),
                    "Vec4" => format!("FVector4({})", arg_strs.join(", ")),
                    _ => format!("{}({})", fn_name, arg_strs.join(", ")),
                }
            }
            // Delegate to full gen_expr for anything else
            _ => self.gen_expr(expr),
        }
    }

    /// Generate binary operator as string
    fn gen_binop_string(&self, op: BinaryOp) -> &'static str {
        match op {
            BinaryOp::Add => "+",
            BinaryOp::Sub => "-",
            BinaryOp::Mul => "*",
            BinaryOp::Div => "/",
            BinaryOp::Mod => "%",
            BinaryOp::Eq => "==",
            BinaryOp::Ne => "!=",
            BinaryOp::Lt => "<",
            BinaryOp::Gt => ">",
            BinaryOp::Le => "<=",
            BinaryOp::Ge => ">=",
            BinaryOp::And => "&&",
            BinaryOp::Or => "||",
            _ => "?",
        }
    }

    fn infer_array_element_cpp_type(&self, elements: &[Expr]) -> &'static str {
        if elements.is_empty() {
            return "float";
        }

        let mut saw_float_like = false;
        let mut saw_int = false;
        let mut saw_bool = false;
        let mut saw_string = false;

        for e in elements {
            match e {
                Expr::Float(_, _) => saw_float_like = true,
                Expr::Int(_, _) => saw_int = true,
                Expr::Bool(_, _) => saw_bool = true,
                Expr::String(_, _) => saw_string = true,
                Expr::Ident(name, _) => {
                    if let Some(type_name) = self.var_types.get(name) {
                        match type_name.as_str() {
                            "Float" | "float" | "double" => saw_float_like = true,
                            "Int" | "int" | "int32" | "int64" | "u32" | "u64" | "usize" => {
                                saw_int = true
                            }
                            "Bool" | "bool" => saw_bool = true,
                            "String" | "FString" => saw_string = true,
                            _ => {}
                        }
                    }
                }
                // Field/call/index frequently produce FVector component scalars;
                // prefer float to avoid narrowing failures in TArray<float> params.
                Expr::Field { .. }
                | Expr::Call { .. }
                | Expr::MethodCall { .. }
                | Expr::Index { .. } => {
                    saw_float_like = true;
                }
                _ => {}
            }
        }

        if saw_string {
            "FString"
        } else if saw_bool && !saw_float_like && !saw_int {
            "bool"
        } else if saw_float_like {
            "float"
        } else if saw_int {
            "int64"
        } else {
            "float"
        }
    }

    /// Generate UCLASS specifiers from actor attributes
    fn gen_uclass_specifiers(&self, actor: &Actor) -> String {
        // Check for @uclass attribute with specifiers
        if let Some(attr) = actor.attributes.iter().find(|a| a.name == "uclass") {
            let mut specs = Vec::new();

            // Extract specifiers from attribute args
            for arg in &attr.args {
                if let Expr::String(s, _) = arg {
                    specs.push(s.clone());
                }
            }

            specs.join(", ")
        } else {
            "BlueprintType, Blueprintable".to_string()
        }
    }

    /// Generate actor method implementation in source
    fn gen_actor_method_impl(&mut self, class_name: &str, method: &Function) {
        // Check for @blueprint_event attribute first
        let is_blueprint_event = method.attributes.iter().any(|a| {
            a.name == "blueprint_event"
                || a.name == "blueprint_native_event"
                || a.name == "blueprint_implementable_event"
        });

        if is_blueprint_event {
            // Use blueprint_event codegen for _Implementation methods
            use crate::blueprint_codegen::generate_blueprint_event_code;
            use crate::blueprint_ir::convert_to_blueprint_event_ir;

            match convert_to_blueprint_event_ir(method, &self.context) {
                Ok(event_ir) => {
                    let output = generate_blueprint_event_code(
                        &event_ir,
                        class_name,
                        &self.context.module_api,
                    );
                    // Write the source implementation (_Implementation method)
                    self.write_source(&output.source_implementation);
                    self.write_blank_source();
                    return;
                }
                Err(e) => {
                    // Fall through to regular method generation
                }
            }
        }

        // Skip inline methods (already in header)
        let is_inline = method.attributes.iter().any(|a| a.name == "inline");
        if is_inline {
            return;
        }

        // Check if this is a pure method (const)
        let is_pure = method
            .attributes
            .iter()
            .any(|a| a.name == "blueprint_pure" || a.name == "pure");

        // Determine return type
        let ret_type = if let Some(ref ty) = method.return_type {
            self.map_type(ty)
        } else {
            "void".to_string()
        };

        // Generate parameters
        let params = self.gen_params(&method.params);

        // Generate method signature
        if is_pure {
            self.write_source(&format!(
                "{} {}::{}({}) const",
                ret_type, class_name, method.name, params
            ));
        } else {
            self.write_source(&format!(
                "{} {}::{}({})",
                ret_type, class_name, method.name, params
            ));
        }

        self.write_source("{");
        self.push_indent();
        self.with_var_type_scope(|this| {
            this.register_param_types(&method.params);
            this.gen_block_source(&method.body);
        });
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
    }

    /// Generate USTRUCT from KAIN struct
    fn gen_ustruct(&mut self, struct_def: &Struct) {
        // Track struct name
        self.context.register_struct(
            struct_def.name.clone(),
            format!("{}.h", self.context.output_name),
        );

        let struct_name = to_struct_name(&struct_def.name);

        // KAIN MARKER: Embed original KAIN source (if enabled)
        if self.context.marker_config.style != crate::ue5::MarkerStyle::None {
            let marker =
                crate::ue5::kain_markers::struct_marker(struct_def, &self.context.marker_config);
            if !marker.is_empty() {
                self.write_header(&marker);
            }
        }

        let is_datatable = struct_def.attributes.iter().any(|a| a.name == "datatable");
        let is_blueprint_type = true; // All KAIN structs are BlueprintType by default

        if is_datatable {
            self.header.push_line("USTRUCT(BlueprintType)");
            self.write_header(&format!(
                "struct {} {} : public FTableRowBase",
                self.context.module_api, struct_name
            ));
        } else {
            self.header.push_line("USTRUCT(BlueprintType)");
            self.write_header(&format!(
                "struct {} {}",
                self.context.module_api, struct_name
            ));
        }

        self.write_header("{");
        self.push_indent();
        self.write_header("GENERATED_BODY()");
        self.write_blank_header();

        // Fields as UPROPERTY - pass struct name for default Category
        for field in &struct_def.fields {
            self.gen_uproperty_with_context(field, Some(&struct_def.name), is_blueprint_type);
        }

        self.pop_indent();

        // KAIN MARKER: End marker for struct (if enabled)
        if self.context.marker_config.style != crate::ue5::MarkerStyle::None {
            let end_marker = crate::ue5::kain_markers::struct_end_marker(
                struct_def,
                &self.context.marker_config,
            );
            if !end_marker.is_empty() {
                self.write_header(&end_marker);
            }
        }

        self.write_header("};");
    }

    /// Generate UActorComponent class from KAIN struct
    ///
    /// Supports lifecycle attributes:
    /// - `@tick` → enables TickComponent() override with PrimaryComponentTick.bCanEverTick = true
    /// - `@beginplay` → enables BeginPlay() override
    fn gen_ucomponent(&mut self, struct_def: &Struct) {
        // Track component name
        self.context.register_component(
            struct_def.name.clone(),
            format!("{}.h", self.context.output_name),
        );

        let class_name = to_component_name(&struct_def.name);

        // Check for lifecycle attributes
        let has_tick = struct_def.attributes.iter().any(|a| a.name == "tick");
        let has_beginplay = struct_def.attributes.iter().any(|a| a.name == "beginplay");

        // Check if component has replicated fields
        let has_replicated = struct_def
            .fields
            .iter()
            .any(|f| f.attributes.iter().any(|a| a.name == "replicated"));

        // Check if component has advanced network sync (interpolation, extrapolation, compression)
        let has_network_sync = has_replicated
            && struct_def.fields.iter().any(|f| {
                f.attributes.iter().any(|a| {
                    if a.name == "replicated" {
                        // Check if it has mode parameter (indicates advanced sync)
                        a.args.iter().any(|arg| {
                            if let Expr::Binary { op, left, .. } = arg {
                                if *op == BinaryOp::Assign {
                                    if let Expr::Ident(name, _) = &**left {
                                        return name == "mode";
                                    }
                                }
                            }
                            false
                        })
                    } else {
                        false
                    }
                })
            });

        // Generate network sync code if needed
        let network_sync_output = if has_network_sync {
            match convert_to_network_sync_ir(struct_def, &self.context) {
                Ok(ir) => Some(generate_network_sync_code(&ir, &class_name)),
                Err(e) => None,
            }
        } else {
            None
        };

        // Get interface inheritance list
        let interface_list = self.context.get_interface_list(&struct_def.name);

        self.header
            .push_line("UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))");
        self.write_header(&format!(
            "class {} {} : public UActorComponent{}",
            self.context.module_api, class_name, interface_list
        ));
        self.write_header("{");
        self.push_indent();
        self.write_header("GENERATED_BODY()");
        self.write_blank_header();

        self.write_header("public:");
        self.write_blank_header();
        self.push_indent();
        self.write_header(&format!("{}();", class_name));

        // Lifecycle method declarations
        if has_beginplay {
            self.write_header("virtual void BeginPlay() override;");
        }
        if has_tick || has_network_sync {
            self.write_header("virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;");
        }
        self.write_blank_header();

        // Fields as UPROPERTY
        for field in &struct_def.fields {
            self.gen_uproperty_with_context(field, None, false);
        }

        // Additional component methods declared directly on the struct or provided via impl blocks
        // (exclude lifecycle methods handled by dedicated BeginPlay/TickComponent overrides).
        let mut component_methods: Vec<Function> = struct_def.methods.clone();
        if let Some(impl_block_methods) = self.impl_methods.get(&struct_def.name) {
            for method in impl_block_methods {
                if !component_methods.iter().any(|m| m.name == method.name) {
                    component_methods.push(method.clone());
                }
            }
        }
        for method in &component_methods {
            let is_lifecycle = matches!(
                method.name.as_str(),
                "begin_play" | "BeginPlay" | "tick" | "Tick" | "on_tick" | "OnTick"
            );
            if is_lifecycle {
                continue;
            }
            self.gen_actor_method_decl(method, &class_name);
        }

        // Add network sync state buffers and helper fields
        if let Some(ref sync_output) = network_sync_output {
            if !sync_output.header_declarations.is_empty() {
                self.write_blank_header();
                self.write_header("// Network synchronization state");
                for line in sync_output.header_declarations.lines() {
                    self.write_header(line);
                }
            }
        }

        // Add GetLifetimeReplicatedProps if needed
        if has_replicated {
            self.pop_indent();
            self.write_blank_header();
            self.write_header("virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;");
            self.push_indent();
        }

        self.pop_indent();
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();

        // Implementation - Constructor
        self.write_source(&format!("{}::{}()", class_name, class_name));
        self.write_source("{");
        self.push_indent();
        if has_tick || has_network_sync {
            self.write_source("PrimaryComponentTick.bCanEverTick = true;");
        } else {
            self.write_source("PrimaryComponentTick.bCanEverTick = false;");
        }

        // Add network sync constructor initialization
        if let Some(ref sync_output) = network_sync_output {
            if !sync_output.constructor_body.is_empty() {
                self.write_blank_source();
                self.write_source("// Network synchronization initialization");
                for line in sync_output.constructor_body.lines() {
                    self.write_source(line);
                }
            }
        } else if has_replicated {
            self.write_source("SetIsReplicatedByDefault(true);");
        }

        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // BeginPlay implementation — wire impl block body if available
        if has_beginplay {
            self.write_source(&format!("void {}::BeginPlay()", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source("Super::BeginPlay();");

            // Look up begin_play method from impl block
            let begin_play_body = self
                .impl_methods
                .get(&struct_def.name)
                .and_then(|methods| {
                    methods
                        .iter()
                        .find(|m| m.name == "begin_play" || m.name == "BeginPlay")
                })
                .cloned();

            if let Some(method) = begin_play_body {
                self.write_blank_source();
                self.with_var_type_scope(|this| {
                    this.register_field_types(&struct_def.fields);
                    this.register_param_types(&method.params);
                    this.gen_block_source(&method.body);
                });
            }

            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }

        // TickComponent implementation — wire impl block body if available
        if has_tick || has_network_sync {
            self.write_source(&format!("void {}::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source("Super::TickComponent(DeltaTime, TickType, ThisTickFunction);");

            // Add network sync tick logic first
            if let Some(ref sync_output) = network_sync_output {
                if !sync_output.tick_body.is_empty() {
                    self.write_blank_source();
                    for line in sync_output.tick_body.lines() {
                        self.write_source(line);
                    }
                }
            }

            // Look up tick method from impl block
            let tick_body = self
                .impl_methods
                .get(&struct_def.name)
                .and_then(|methods| {
                    methods
                        .iter()
                        .find(|m| m.name == "tick" || m.name == "Tick")
                })
                .cloned();

            if let Some(method) = tick_body {
                // Remap common delta time parameter names to UE5's DeltaTime
                for param in &method.params {
                    if param.name == "delta_time" || param.name == "dt" || param.name == "delta" {
                        self.context
                            .add_ident_remap(param.name.clone(), "DeltaTime".to_string());
                    }
                }
                self.write_blank_source();
                self.with_var_type_scope(|this| {
                    this.register_field_types(&struct_def.fields);
                    this.register_param_types(&method.params);
                    this.gen_block_source(&method.body);
                });
                self.context.clear_ident_remaps();
            }

            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }

        // Implement GetLifetimeReplicatedProps if needed
        if has_replicated {
            self.write_source(&format!("void {}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const", class_name));
            self.write_source("{");
            self.push_indent();

            // Use network sync replication setup if available
            if let Some(ref sync_output) = network_sync_output {
                for line in sync_output.replication_body.lines() {
                    self.write_source(line);
                }
            } else {
                // Fallback to simple replication
                self.write_source("Super::GetLifetimeReplicatedProps(OutLifetimeProps);");
                self.write_blank_source();

                // Add DOREPLIFETIME for each replicated field
                for field in &struct_def.fields {
                    if field.attributes.iter().any(|a| a.name == "replicated") {
                        self.write_source(&format!(
                            "DOREPLIFETIME({}, {});",
                            class_name, field.name
                        ));
                    }
                }
            }

            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }

        // Non-lifecycle component method implementations from struct + impl blocks.
        let mut component_methods: Vec<Function> = struct_def.methods.clone();
        if let Some(impl_block_methods) = self.impl_methods.get(&struct_def.name) {
            for method in impl_block_methods {
                if !component_methods.iter().any(|m| m.name == method.name) {
                    component_methods.push(method.clone());
                }
            }
        }
        for method in &component_methods {
            let is_lifecycle = matches!(
                method.name.as_str(),
                "begin_play" | "BeginPlay" | "tick" | "Tick" | "on_tick" | "OnTick"
            );
            if is_lifecycle {
                continue;
            }
            let saved_var_types = self.var_types.clone();
            self.register_field_types(&struct_def.fields);
            self.gen_actor_method_impl(&class_name, method);
            self.var_types = saved_var_types;
        }
    }

    /// Generate UWorldSubsystem class from KAIN struct with @subsystem attribute.
    ///
    /// Generates:
    /// - UCLASS with correct specifiers
    /// - UWorldSubsystem inheritance
    /// - Initialize()/Deinitialize() lifecycle overrides
    /// - ShouldCreateSubsystem() override
    /// - Optional FTickableGameObject interface when @tick attribute present
    /// - UPROPERTY fields from struct fields
    fn gen_usubsystem(&mut self, struct_def: &Struct) {
        let class_name = to_subsystem_name(&struct_def.name);

        // Check for @tick attribute → FTickableGameObject interface
        let has_tick = struct_def.attributes.iter().any(|a| a.name == "tick");

        // Check for @savegame fields
        let has_savegame = struct_def
            .fields
            .iter()
            .any(|f| f.attributes.iter().any(|a| a.name == "savegame"));

        // --- Header ---
        self.header.push_line("UCLASS()");
        if has_tick {
            self.write_header(&format!(
                "class {} {} : public UWorldSubsystem, public FTickableGameObject",
                self.context.module_api, class_name
            ));
        } else {
            self.write_header(&format!(
                "class {} {} : public UWorldSubsystem",
                self.context.module_api, class_name
            ));
        }
        self.write_header("{");
        self.push_indent();
        self.write_header("GENERATED_BODY()");
        self.write_blank_header();

        self.write_header("public:");
        self.write_blank_header();
        self.push_indent();

        // Lifecycle overrides
        self.write_header(
            "virtual void Initialize(FSubsystemCollectionBase& Collection) override;",
        );
        self.write_header("virtual void Deinitialize() override;");
        self.write_header("virtual bool ShouldCreateSubsystem(UObject* Outer) const override;");

        if has_tick {
            self.write_blank_header();
            self.write_header("// FTickableGameObject interface");
            self.write_header("virtual void Tick(float DeltaTime) override;");
            self.write_header("virtual TStatId GetStatId() const override;");
            self.write_header("virtual bool IsTickable() const override;");
        }

        self.write_blank_header();

        // Fields as UPROPERTY
        for field in &struct_def.fields {
            self.gen_uproperty_with_context(field, Some(&struct_def.name), false);
        }

        // Additional subsystem methods declared directly on the struct
        // (exclude lifecycle/tick methods handled by dedicated overrides below)
        for method in &struct_def.methods {
            let is_lifecycle = matches!(
                method.name.as_str(),
                "initialize"
                    | "Initialize"
                    | "deinitialize"
                    | "Deinitialize"
                    | "tick"
                    | "Tick"
                    | "on_tick"
                    | "OnTick"
            );
            if !is_lifecycle {
                self.gen_actor_method_decl(method, &class_name);
            }
        }

        self.pop_indent();
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();

        // --- Source Implementation ---

        // Initialize — wire impl block body if available
        self.write_source(&format!(
            "void {}::Initialize(FSubsystemCollectionBase& Collection)",
            class_name
        ));
        self.write_source("{");
        self.push_indent();
        self.write_source("Super::Initialize(Collection);");

        let init_body = self
            .impl_methods
            .get(&struct_def.name)
            .and_then(|methods| {
                methods
                    .iter()
                    .find(|m| m.name == "initialize" || m.name == "Initialize")
            })
            .cloned();

        if let Some(method) = init_body {
            self.write_blank_source();
            self.with_var_type_scope(|this| {
                this.register_param_types(&method.params);
                this.gen_block_source(&method.body);
            });
        }

        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // Deinitialize — wire impl block body if available
        self.write_source(&format!("void {}::Deinitialize()", class_name));
        self.write_source("{");
        self.push_indent();

        let deinit_body = self
            .impl_methods
            .get(&struct_def.name)
            .and_then(|methods| {
                methods
                    .iter()
                    .find(|m| m.name == "deinitialize" || m.name == "Deinitialize")
            })
            .cloned();

        if let Some(method) = deinit_body {
            self.with_var_type_scope(|this| {
                this.register_param_types(&method.params);
                this.gen_block_source(&method.body);
            });
            self.write_blank_source();
        }

        self.write_source("Super::Deinitialize();");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // ShouldCreateSubsystem
        self.write_source(&format!(
            "bool {}::ShouldCreateSubsystem(UObject* Outer) const",
            class_name
        ));
        self.write_source("{");
        self.push_indent();
        self.write_source("return true;");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // Tick (if @tick) — wire impl block body if available
        if has_tick {
            self.write_source(&format!("void {}::Tick(float DeltaTime)", class_name));
            self.write_source("{");
            self.push_indent();

            let tick_body = self
                .impl_methods
                .get(&struct_def.name)
                .and_then(|methods| {
                    methods
                        .iter()
                        .find(|m| m.name == "tick" || m.name == "Tick")
                })
                .cloned();

            if let Some(method) = tick_body {
                for param in &method.params {
                    if param.name == "delta_time" || param.name == "dt" || param.name == "delta" {
                        self.context
                            .add_ident_remap(param.name.clone(), "DeltaTime".to_string());
                    }
                }
                self.with_var_type_scope(|this| {
                    this.register_param_types(&method.params);
                    this.gen_block_source(&method.body);
                });
                self.context.clear_ident_remaps();
            }

            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();

            self.write_source(&format!("TStatId {}::GetStatId() const", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source(&format!(
                "RETURN_QUICK_DECLARE_CYCLE_STAT({}, STATGROUP_Tickables);",
                class_name
            ));
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();

            self.write_source(&format!("bool {}::IsTickable() const", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source("return true;");
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }

        // Non-lifecycle subsystem method implementations
        for method in &struct_def.methods {
            let is_lifecycle = matches!(
                method.name.as_str(),
                "initialize"
                    | "Initialize"
                    | "deinitialize"
                    | "Deinitialize"
                    | "tick"
                    | "Tick"
                    | "on_tick"
                    | "OnTick"
            );
            if !is_lifecycle {
                self.gen_actor_method_impl(&class_name, method);
            }
        }
    }

    /// Helper to generate UPROPERTY for a field (legacy wrapper)
    fn gen_uproperty(&mut self, field: &Field) {
        self.gen_uproperty_with_context(field, None, false);
    }

    /// Helper to generate UPROPERTY for a field with context about parent struct
    ///
    /// # Arguments
    /// * `field` - The field to generate UPROPERTY for
    /// * `parent_struct_name` - Optional parent struct name for default Category
    /// * `is_blueprint_type` - Whether parent is a BlueprintType struct (needs Category)
    fn gen_uproperty_with_context(
        &mut self,
        field: &Field,
        parent_struct_name: Option<&str>,
        is_blueprint_type: bool,
    ) {
        let mut props = vec!["EditAnywhere", "BlueprintReadWrite"];
        let mut meta_tags: Vec<String> = Vec::new();
        let mut category = String::new();
        let mut has_explicit_category = false;

        for attr in &field.attributes {
            match attr.name.as_str() {
                "replicated" => {
                    props.push("Replicated");
                    // Check for conditional replication
                    if !attr.args.is_empty() {
                        for arg in &attr.args {
                            if let Expr::String(arg_str, _) = arg {
                                if let Some((key, value)) = arg_str.split_once('=') {
                                    let key = key.trim();
                                    let value = value.trim().trim_matches('"');
                                    if key == "condition" {
                                        // Map KAIN condition names to UE5 COND_ enums
                                        let ue_condition = match value {
                                            "OwnerOnly" => "COND_OwnerOnly",
                                            "SkipOwner" => "COND_SkipOwner",
                                            "SimulatedOnly" => "COND_SimulatedOnly",
                                            "AutonomousOnly" => "COND_AutonomousOnly",
                                            "InitialOnly" => "COND_InitialOnly",
                                            "Custom" => "COND_Custom",
                                            "ReplayOrOwner" => "COND_ReplayOrOwner",
                                            "ReplayOnly" => "COND_ReplayOnly",
                                            "SimulatedOrPhysics" => "COND_SimulatedOrPhysics",
                                            "SimulatedOnlyNoReplay" => "COND_SimulatedOnlyNoReplay",
                                            "SkipReplay" => "COND_SkipReplay",
                                            _ => "COND_None",
                                        };
                                        // Note: Conditional replication requires GetLifetimeReplicatedProps override
                                        // We'll add a comment for now
                                        meta_tags.push(format!(
                                            "ReplicationCondition = {}",
                                            ue_condition
                                        ));
                                    }
                                }
                            }
                        }
                    }
                }
                "savegame" => props.push("SaveGame"),
                "transient" => props.push("Transient"),
                "editdefaults" => {
                    props.retain(|&p| p != "EditAnywhere");
                    props.push("EditDefaultsOnly");
                }
                "visibleonly" => {
                    props.retain(|&p| p != "EditAnywhere");
                    props.push("VisibleAnywhere");
                }
                "blueprint_assignable" => {
                    props.push("BlueprintAssignable");
                }
                "blueprint_callable" => {
                    props.push("BlueprintCallable");
                }
                "category" => {
                    has_explicit_category = true;
                    if !attr.args.is_empty() {
                        if let Expr::String(cat, _) = &attr.args[0] {
                            category = cat.clone();
                        }
                    }
                }
                "edit_condition" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(condition, _) = &attr.args[0] {
                            meta_tags.push(format!("EditCondition = \"{}\"", condition));
                        }
                    }
                }
                "clamp_min" => {
                    if !attr.args.is_empty() {
                        let min_val = match &attr.args[0] {
                            Expr::Int(i, _) => i.to_string(),
                            Expr::Float(f, _) => f.to_string(),
                            Expr::String(s, _) => s.clone(),
                            _ => continue,
                        };
                        meta_tags.push(format!("ClampMin = \"{}\"", min_val));
                    }
                }
                "clamp_max" => {
                    if !attr.args.is_empty() {
                        let max_val = match &attr.args[0] {
                            Expr::Int(i, _) => i.to_string(),
                            Expr::Float(f, _) => f.to_string(),
                            Expr::String(s, _) => s.clone(),
                            _ => continue,
                        };
                        meta_tags.push(format!("ClampMax = \"{}\"", max_val));
                    }
                }
                "ui_min" => {
                    if !attr.args.is_empty() {
                        let min_val = match &attr.args[0] {
                            Expr::Int(i, _) => i.to_string(),
                            Expr::Float(f, _) => f.to_string(),
                            Expr::String(s, _) => s.clone(),
                            _ => continue,
                        };
                        meta_tags.push(format!("UIMin = \"{}\"", min_val));
                    }
                }
                "ui_max" => {
                    if !attr.args.is_empty() {
                        let max_val = match &attr.args[0] {
                            Expr::Int(i, _) => i.to_string(),
                            Expr::Float(f, _) => f.to_string(),
                            Expr::String(s, _) => s.clone(),
                            _ => continue,
                        };
                        meta_tags.push(format!("UIMax = \"{}\"", max_val));
                    }
                }
                "units" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(unit, _) = &attr.args[0] {
                            meta_tags.push(format!("Units = \"{}\"", unit));
                        }
                    }
                }
                "tooltip" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(tip, _) = &attr.args[0] {
                            meta_tags.push(format!("ToolTip = \"{}\"", tip));
                        }
                    }
                }
                "display_name" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(name, _) = &attr.args[0] {
                            meta_tags.push(format!("DisplayName = \"{}\"", name));
                        }
                    }
                }
                _ => {}
            }
        }

        // Build UPROPERTY macro
        let mut uproperty_parts: Vec<String> = props.iter().map(|s| s.to_string()).collect();

        // Add default Category if no explicit category was set.
        // UE5/UHT requires explicit Category for editor/Blueprint-exposed properties.
        if !has_explicit_category && category.is_empty() {
            if is_blueprint_type {
                if let Some(struct_name) = parent_struct_name {
                    category = struct_name.to_string();
                }
            } else {
                // Component/legacy path: still expose as EditAnywhere/BlueprintReadWrite,
                // so provide a stable fallback category.
                category = "Component".to_string();
            }
        }

        if !category.is_empty() {
            uproperty_parts.push(format!("Category = \"{}\"", category));
        }

        if !meta_tags.is_empty() {
            uproperty_parts.push(format!("meta = ({})", meta_tags.join(", ")));
        }

        self.write_header(&format!("UPROPERTY({})", uproperty_parts.join(", ")));
        let ty_str = self.map_type(&field.ty);
        // Bug-12 fix: UE5.4 requires UPROPERTY fields in USTRUCTs to be explicitly
        // initialised or a LogClass error is emitted. Use C++11 member initializers.
        // Only applied to struct fields (parent_struct_name.is_some()); component
        // fields are zero-initialised inside the generated constructor.
        if parent_struct_name.is_some() {
            // Bug-15: prefer explicit field default from KAIN source over type-based default.
            if let Some(default_expr) = &field.default {
                let default_str = self.gen_default_value(default_expr, &ty_str);
                self.write_header(&format!("{} {} = {};", ty_str, field.name, default_str));
            } else if ty_str.starts_with('E') {
                // UE5 runtime LogClass requires explicit enum initialization in USTRUCT fields.
                self.write_header(&format!(
                    "{} {} = static_cast<{}>(0);",
                    ty_str, field.name, ty_str
                ));
            } else if let Some(init) = default_cpp_value(&ty_str) {
                self.write_header(&format!("{} {} = {};", ty_str, field.name, init));
            } else {
                self.write_header(&format!("{} {};", ty_str, field.name));
            }
        } else {
            self.write_header(&format!("{} {};", ty_str, field.name));
        }
        self.write_blank_header();
    }

    /// Generate UENUM from KAIN enum
    fn gen_uenum(&mut self, enum_def: &Enum) {
        // Track enum name
        self.context.register_enum(
            enum_def.name.clone(),
            format!("{}.h", self.context.output_name),
        );

        // KAIN MARKER: Embed original KAIN source (if enabled)
        if self.context.marker_config.style != crate::ue5::MarkerStyle::None {
            let marker =
                crate::ue5::kain_markers::enum_marker(enum_def, &self.context.marker_config);
            if !marker.is_empty() {
                self.write_header(&marker);
            }
        }

        // Check if simple enum (all Unit variants)
        let is_simple = enum_def
            .variants
            .iter()
            .all(|v| matches!(v.fields, VariantFields::Unit));

        if is_simple {
            let enum_name = to_enum_name(&enum_def.name);

            self.header.push_line("UENUM(BlueprintType)");
            self.write_header(&format!("enum class {} : uint8", enum_name));
            self.write_header("{");
            self.push_indent();

            for variant in &enum_def.variants {
                self.write_header(&format!(
                    "{} UMETA(DisplayName = \"{}\"),",
                    variant.name, variant.name
                ));
            }

            self.pop_indent();
            self.write_header("};");
        } else {
            // Complex enums need to be represented as structs with a type tag
            self.write_header(&format!(
                "// Complex enum {} - represented as tagged struct",
                enum_def.name
            ));
        }
    }

    /// Generate UFUNCTION from KAIN function
    fn gen_ufunction(&mut self, func: &Function) {
        // Check for @ue5 attribute to determine if this should be a UFUNCTION
        let has_ue5_attr = func
            .attributes
            .iter()
            .any(|a| a.name == "ue5" || a.name == "blueprint" || a.name == "blueprint_pure");

        if has_ue5_attr {
            let ret_type = func
                .return_type
                .as_ref()
                .map(|t| self.map_type(t))
                .unwrap_or_else(|| "void".to_string());

            let params = self.gen_params_for_blueprint(&func.params);
            let has_return = func.return_type.is_some();

            // Canonical: Blueprint function library class is based on plugin/module name.
            // Using output_name here regresses to classes like UAeroTunnelBlueprintLibraryFunctionLibrary.
            let class_name = format!("U{}FunctionLibrary", self.module_name);

            // Pure functions (no side effects) should use BlueprintPure
            let is_pure = func
                .attributes
                .iter()
                .any(|a| a.name == "pure" || a.name == "const" || a.name == "blueprint_pure");
            let has_out_params = func.params.iter().any(|p| p.mutable);

            // Determine UFUNCTION specifiers
            let specifiers = if is_pure && !has_out_params {
                "BlueprintPure, Category = \"Kain\""
            } else {
                "BlueprintCallable, Category = \"Kain\""
            };

            // Header declaration
            self.write_header(&format!("UFUNCTION({})", specifiers));
            self.write_header(&format!("static {} {}({});", ret_type, func.name, params));

            // Source implementation
            self.write_source(&format!(
                "{} {}::{}({})",
                ret_type,
                class_name,
                func.name,
                self.gen_params(&func.params)
            ));
            self.write_source("{");
            self.push_indent();
            self.with_var_type_scope(|this| {
                this.register_param_types(&func.params);
                this.gen_block_source_with_implicit_return(&func.body, has_return);
            });
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        } else {
            // Regular C++ function
            let ret_type = func
                .return_type
                .as_ref()
                .map(|t| self.map_type(t))
                .unwrap_or_else(|| "void".to_string());

            let params = self.gen_params(&func.params);
            let has_return = func.return_type.is_some();

            self.write_header(&format!("{} {}({});", ret_type, func.name, params));

            self.write_source(&format!("{} {}({})", ret_type, func.name, params));
            self.write_source("{");
            self.push_indent();
            self.with_var_type_scope(|this| {
                this.register_param_types(&func.params);
                this.gen_block_source_with_implicit_return(&func.body, has_return);
            });
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
    }

    // TODO: Agent 4 will implement trait codegen
    // fn gen_trait(&mut self, trait_def: &Trait) {
    //     use crate::ue5::traits;
    //
    //     // Generate UInterface header
    //     let trait_header = traits::generate_trait_header(trait_def, &self.context.module_api);
    //     self.write_header(&trait_header);
    // }

    fn gen_impl(&mut self, impl_def: &Impl) {
        let target = self.map_type(&impl_def.target_type);
        self.write_header(&format!("// Methods for {}", target));

        for method in &impl_def.methods {
            self.gen_ufunction(method);
        }
    }

    fn gen_params(&self, params: &[Param]) -> String {
        self.gen_params_internal(params, false, false)
    }

    fn gen_params_for_rpc(&self, params: &[Param]) -> String {
        self.gen_params_internal(params, true, false)
    }

    fn gen_params_for_blueprint(&self, params: &[Param]) -> String {
        self.gen_params_internal(params, false, true)
    }

    fn gen_params_internal(&self, params: &[Param], is_rpc: bool, is_blueprint: bool) -> String {
        let parts: Vec<String> = params
            .iter()
            .map(|p| {
                let ty_str = self.map_type(&p.ty);

                // For RPCs, FString and TArray must be passed by const reference
                let needs_ref =
                    is_rpc && (ty_str.starts_with("FString") || ty_str.starts_with("TArray"));

                // Check if this is a UObject-derived type that should be a pointer
                // UObject-derived types start with U or A prefix
                // Also check for specific known types that might be missing the pointer
                let is_uobject_ptr = ty_str.starts_with('U')
                    || ty_str.starts_with('A')
                    || ty_str == "UAnimSequence"
                    || ty_str == "UAnimMontage"
                    || ty_str == "USkeletalMesh"
                    || ty_str == "UStaticMesh"
                    || ty_str == "UMaterialInterface"
                    || ty_str == "UTexture";

                // Special handling for known UObject types that might be missing pointer suffix
                let needs_pointer_fix = is_uobject_ptr && !ty_str.ends_with('*');

                let param_decl = if p.mutable {
                    // Output parameter - use UPARAM(ref) for Blueprint visibility
                    if is_blueprint {
                        format!("UPARAM(ref) {}& {}", ty_str, p.name)
                    } else {
                        format!("{}& {}", ty_str, p.name)
                    }
                } else if needs_ref {
                    format!("const {}& {}", ty_str, p.name)
                } else if ty_str.ends_with('*') {
                    // Already has pointer suffix - don't add const
                    format!("{} {}", ty_str, p.name)
                } else if needs_pointer_fix {
                    // UObject/AActor/UActorComponent types need pointer suffix
                    // Don't add const for pointer types
                    format!("{}* {}", ty_str, p.name)
                } else {
                    // Non-pointer types get const
                    format!("const {} {}", ty_str, p.name)
                };

                // Add default value if present
                if let Some(ref default_expr) = p.default {
                    let default_val = self.gen_default_value(default_expr, &ty_str);
                    format!("{} = {}", param_decl, default_val)
                } else {
                    param_decl
                }
            })
            .collect();
        parts.join(", ")
    }

    /// Generate default parameter value
    fn gen_default_value(&self, expr: &Expr, ty_str: &str) -> String {
        match expr {
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => {
                // Ensure decimal point is always present for proper C++ syntax
                let s = f.to_string();
                if s.contains('.') {
                    format!("{}f", s)
                } else {
                    format!("{}.0f", s)
                }
            }
            Expr::Bool(b, _) => b.to_string(),
            Expr::String(s, _) => {
                if s == "None" && ty_str.contains("FName") {
                    "NAME_None".to_string()
                } else {
                    format!("TEXT(\"{}\")", s)
                }
            }
            Expr::Ident(name, _) => {
                match name.as_str() {
                    "null" => "nullptr".to_string(),
                    "None" => "NAME_None".to_string(),
                    _ => {
                        // Bug-15 fix: enum variant identifiers must be qualified.
                        // e.g. `CelClassic` with type `EToonStyle` -> `EToonStyle::CelClassic`
                        // Detected by UENUM naming convention: type starts with `E` + uppercase.
                        let chars: Vec<char> = ty_str.chars().collect();
                        if chars.len() > 1 && chars[0] == 'E' && chars[1].is_uppercase() {
                            format!("{}::{}", ty_str, name)
                        } else {
                            name.clone()
                        }
                    }
                }
            }
            Expr::Field { object, field, .. } => {
                // Handle enum values like AnimationMode::Loop
                let obj_str = match object.as_ref() {
                    Expr::Ident(name, _) => {
                        // Check if it's an enum
                        if self.context.is_enum(name) {
                            to_enum_name(name)
                        } else {
                            name.clone()
                        }
                    }
                    _ => self.gen_expr_string(object),
                };
                format!("{}::{}", obj_str, field)
            }
            Expr::EnumVariant {
                enum_name, variant, ..
            } => {
                // Handle EnumVariant expressions
                let enum_type = to_enum_name(enum_name);
                format!("{}::{}", enum_type, variant)
            }
            _ => self.gen_expr_string(expr),
        }
    }

    fn gen_block_source(&mut self, block: &Block) {
        for stmt in &block.stmts {
            self.gen_stmt(stmt);
        }
    }

    /// Handle implicit returns for KAIN expression-body functions
    fn gen_block_source_with_implicit_return(&mut self, block: &Block, implicit_return: bool) {
        let len = block.stmts.len();
        for (i, stmt) in block.stmts.iter().enumerate() {
            let is_last = i == len - 1;
            if is_last && implicit_return {
                if let Stmt::Expr(expr) = stmt {
                    if let Expr::If {
                        condition,
                        then_branch,
                        else_branch,
                        ..
                    } = expr
                    {
                        self.gen_if_expr_as_return_stmt(condition, then_branch, else_branch);
                        continue;
                    }
                    // Match expressions with statement-level arms (e.g. nested matches)
                    // must be emitted as if/return chains, not wrapped with `return <expr>`.
                    if let Expr::Match {
                        scrutinee, arms, ..
                    } = expr
                    {
                        let needs_statement_form = arms.iter().any(|arm| {
                            matches!(
                                &arm.body,
                                Expr::Block(_, _)
                                    | Expr::If { .. }
                                    | Expr::Match { .. }
                                    | Expr::Return(_, _)
                                    | Expr::Break(_, _)
                                    | Expr::Continue(_)
                                    | Expr::Assign { .. }
                            )
                        });
                        if needs_statement_form {
                            self.gen_match_as_return_stmt(scrutinee, arms);
                            continue;
                        }
                    }
                    if !matches!(
                        expr,
                        Expr::Return(_, _) | Expr::Break(_, _) | Expr::Continue(_)
                    ) {
                        let expr_str = self.gen_expr(expr);
                        self.write_source(&format!("return {};", expr_str));
                        continue;
                    }
                }
            }
            self.gen_stmt(stmt);
        }
    }

    fn gen_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                if let Pattern::Binding { name, mutable, .. } = pattern {
                    if let Some(Type::Named { name: ty_name, .. }) = ty {
                        self.var_types.insert(name.clone(), ty_name.clone());
                    }

                    let ty_str = ty
                        .as_ref()
                        .map(|t| self.map_type(t))
                        .unwrap_or_else(|| "auto".to_string());

                    if let Some(val) = value {
                        // KAIN `let` bindings are frequently field-mutated later in the same
                        // scope (e.g. settings.foo = ...). Emitting const here causes large
                        // C3892/C2678 cascades in generated UE C++.
                        let _ = mutable; // retained for future mutability-specific lowering
                        self.write_source(&format!(
                            "{} {} = {};",
                            ty_str,
                            name,
                            self.gen_expr(val)
                        ));
                    } else {
                        self.write_source(&format!("{} {};", ty_str, name));
                    }
                }
            }

            Stmt::Return(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    if let Expr::If {
                        condition,
                        then_branch,
                        else_branch,
                        ..
                    } = expr
                    {
                        self.gen_if_expr_as_return_stmt(condition, then_branch, else_branch);
                    } else if let Expr::Match {
                        scrutinee, arms, ..
                    } = expr
                    {
                        let needs_statement_form = arms.iter().any(|arm| {
                            matches!(
                                &arm.body,
                                Expr::Block(_, _)
                                    | Expr::If { .. }
                                    | Expr::Match { .. }
                                    | Expr::Return(_, _)
                                    | Expr::Break(_, _)
                                    | Expr::Continue(_)
                                    | Expr::Assign { .. }
                            )
                        });
                        if needs_statement_form {
                            self.gen_match_as_return_stmt(scrutinee, arms);
                        } else {
                            self.write_source(&format!("return {};", self.gen_expr(expr)));
                        }
                    } else {
                        self.write_source(&format!("return {};", self.gen_expr(expr)));
                    }
                } else {
                    self.write_source("return;");
                }
            }

            Stmt::Break(_, _) => {
                self.write_source("break;");
            }

            Stmt::Continue(_) => {
                self.write_source("continue;");
            }

            Stmt::Defer { .. } => {
                self.write_source(
                    "checkf(false, TEXT(\"Kain defer is not supported on UE5 targets yet\"));",
                );
            }

            Stmt::Dispatch { .. } => {
                self.write_source(
                    "checkf(false, TEXT(\"Kain dispatch statements are only supported by native/GPU targets\"));",
                );
            }

            Stmt::For {
                binding,
                iter,
                body,
                ..
            }
            | Stmt::Fanout {
                binding,
                iter,
                body,
                ..
            } => {
                if let Pattern::Binding { name, .. } = binding {
                    if let Expr::Call { callee, args, .. } = iter {
                        if let Expr::Ident(fn_name, _) = callee.as_ref() {
                            if fn_name == "range" {
                                if args.len() == 2 {
                                    let start = self.gen_expr(&args[0].value);
                                    let end = self.gen_expr(&args[1].value);
                                    self.write_source(&format!(
                                        "for (int64 {} = {}; {} < {}; ++{})",
                                        name, start, name, end, name
                                    ));
                                    self.write_source("{");
                                    self.push_indent();
                                    self.gen_block_source(body);
                                    self.pop_indent();
                                    self.write_source("}");
                                    return;
                                } else if args.len() == 1 {
                                    let end = self.gen_expr(&args[0].value);
                                    self.write_source(&format!(
                                        "for (int64 {} = 0; {} < {}; ++{})",
                                        name, name, end, name
                                    ));
                                    self.write_source("{");
                                    self.push_indent();
                                    self.gen_block_source(body);
                                    self.pop_indent();
                                    self.write_source("}");
                                    return;
                                }
                            }
                        }
                    }

                    self.write_source(&format!("for (auto {} : {})", name, self.gen_expr(iter)));
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source(body);
                    self.pop_indent();
                    self.write_source("}");
                }
            }

            Stmt::While {
                condition, body, ..
            } => {
                self.write_source(&format!("while ({})", self.gen_expr(condition)));
                self.write_source("{");
                self.push_indent();
                self.gen_block_source(body);
                self.pop_indent();
                self.write_source("}");
            }

            Stmt::Loop { body, .. } => {
                self.write_source("while (true)");
                self.write_source("{");
                self.push_indent();
                self.gen_block_source(body);
                self.pop_indent();
                self.write_source("}");
            }

            Stmt::Expr(expr) => {
                if let Expr::Ident(name, _) = expr {
                    if name == "pass" {
                        return;
                    }
                }
                if let Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } = expr
                {
                    self.gen_if_stmt(condition, then_branch, else_branch);
                } else if let Expr::Assign { target, value, .. } = expr {
                    self.write_source(&format!(
                        "{} = {};",
                        self.gen_expr(target),
                        self.gen_expr(value)
                    ));
                } else if let Expr::Match {
                    scrutinee, arms, ..
                } = expr
                {
                    self.gen_match_as_statement(scrutinee, arms);
                } else {
                    let expr_str = self.gen_expr(expr);
                    if !expr_str.is_empty() {
                        self.write_source(&format!("{};", expr_str));
                    }
                }
            }

            Stmt::Item(_) => {}
        }
    }

    fn gen_if_stmt(
        &mut self,
        condition: &Expr,
        then_branch: &Block,
        else_branch: &Option<Box<ElseBranch>>,
    ) {
        self.write_source(&format!("if ({})", self.gen_expr(condition)));
        self.write_source("{");
        self.push_indent();
        self.gen_block_source(then_branch);
        self.pop_indent();

        if let Some(else_br) = else_branch {
            match else_br.as_ref() {
                ElseBranch::Else(block) => {
                    self.write_source("}");
                    self.write_source("else");
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source(block);
                    self.pop_indent();
                    self.write_source("}");
                }
                ElseBranch::ElseIf(cond, block, next_else) => {
                    self.write_source("}");
                    self.write_source(&format!("else if ({})", self.gen_expr(cond)));
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source(block);
                    self.pop_indent();
                    if let Some(next) = next_else {
                        self.gen_else_continuation(next);
                    } else {
                        self.write_source("}");
                    }
                }
            }
        } else {
            self.write_source("}");
        }
    }

    fn gen_if_expr_as_return_stmt(
        &mut self,
        condition: &Expr,
        then_branch: &Block,
        else_branch: &Option<Box<ElseBranch>>,
    ) {
        self.write_source(&format!("if ({})", self.gen_expr(condition)));
        self.write_source("{");
        self.push_indent();
        self.gen_block_source_with_implicit_return(then_branch, true);
        self.pop_indent();

        if let Some(else_br) = else_branch {
            match else_br.as_ref() {
                ElseBranch::Else(block) => {
                    self.write_source("}");
                    self.write_source("else");
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source_with_implicit_return(block, true);
                    self.pop_indent();
                    self.write_source("}");
                }
                ElseBranch::ElseIf(cond, block, next_else) => {
                    self.write_source("}");
                    self.write_source(&format!("else if ({})", self.gen_expr(cond)));
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source_with_implicit_return(block, true);
                    self.pop_indent();
                    if let Some(next) = next_else {
                        self.gen_else_if_return_continuation(next);
                    } else {
                        self.write_source("}");
                    }
                }
            }
        } else {
            self.write_source("}");
        }
    }

    fn gen_else_if_return_continuation(&mut self, else_branch: &ElseBranch) {
        match else_branch {
            ElseBranch::Else(block) => {
                self.write_source("}");
                self.write_source("else");
                self.write_source("{");
                self.push_indent();
                self.gen_block_source_with_implicit_return(block, true);
                self.pop_indent();
                self.write_source("}");
            }
            ElseBranch::ElseIf(cond, block, next_else) => {
                self.write_source("}");
                self.write_source(&format!("else if ({})", self.gen_expr(cond)));
                self.write_source("{");
                self.push_indent();
                self.gen_block_source_with_implicit_return(block, true);
                self.pop_indent();
                if let Some(next) = next_else {
                    self.gen_else_if_return_continuation(next);
                } else {
                    self.write_source("}");
                }
            }
        }
    }

    fn gen_match_as_return_stmt(&mut self, scrutinee: &Expr, arms: &[kain_core::ast::MatchArm]) {
        let scrut = self.gen_expr(scrutinee);
        let mut first = true;
        for arm in arms {
            let is_wildcard = match &arm.pattern {
                Pattern::Wildcard(_) => true,
                Pattern::Binding { name, .. } if name == "_" => true,
                _ => false,
            };

            if is_wildcard {
                if !first {
                    self.write_source("else");
                }
            } else {
                let cond = match &arm.pattern {
                    Pattern::Variant {
                        enum_name, variant, ..
                    } => {
                        let path = if let Some(en) = enum_name {
                            format!("{}::{}", to_enum_name(en), variant)
                        } else {
                            variant.clone()
                        };
                        format!("{} == {}", scrut, path)
                    }
                    Pattern::Literal(lit) => {
                        format!("{} == {}", scrut, self.gen_expr(lit))
                    }
                    Pattern::Binding { name, .. } => {
                        format!("{} == {}", scrut, name)
                    }
                    _ => format!("true /* unsupported pattern */"),
                };
                if first {
                    self.write_source(&format!("if ({})", cond));
                    first = false;
                } else {
                    self.write_source(&format!("else if ({})", cond));
                }
            }

            self.write_source("{");
            self.push_indent();
            match &arm.body {
                // Multi-statement block: emit all stmts via gen_block_source.
                // The last stmt is responsible for its own return (e.g. Stmt::Return).
                Expr::Block(block, _) => {
                    self.gen_block_source(block);
                }
                // Nested match in return position: recurse
                Expr::Match {
                    scrutinee: inner_scrut,
                    arms: inner_arms,
                    ..
                } => {
                    self.gen_match_as_return_stmt(inner_scrut, inner_arms);
                }
                // Single expression arm: wrap in synthetic block for implicit return
                other => {
                    let synthetic_block = kain_core::ast::Block {
                        stmts: vec![kain_core::ast::Stmt::Expr(other.clone())],
                        span: kain_core::span::Span::default(),
                    };
                    self.gen_block_source_with_implicit_return(&synthetic_block, true);
                }
            }
            self.pop_indent();
            self.write_source("}");
        }
    }

    /// Emit a statement-level match expression as an if/else chain writing directly to source.
    /// Uses gen_stmt/gen_block_source for arm bodies so all statement types are handled:
    /// let bindings, nested matches, multi-statement blocks, assignments, returns, etc.
    fn gen_match_as_statement(&mut self, scrutinee: &Expr, arms: &[kain_core::ast::MatchArm]) {
        let scrut = self.gen_expr(scrutinee);
        let mut first = true;

        for arm in arms {
            let is_wildcard = match &arm.pattern {
                Pattern::Wildcard(_) => true,
                Pattern::Binding { name, .. } if name == "_" => true,
                _ => false,
            };

            if is_wildcard {
                if !first {
                    self.write_source("else");
                }
            } else {
                let cond = match &arm.pattern {
                    Pattern::Variant {
                        enum_name, variant, ..
                    } => {
                        let path = if let Some(en) = enum_name {
                            format!("{}::{}", to_enum_name(en), variant)
                        } else {
                            variant.clone()
                        };
                        format!("{} == {}", scrut, path)
                    }
                    Pattern::Literal(lit) => {
                        format!("{} == {}", scrut, self.gen_expr(lit))
                    }
                    Pattern::Binding { name, .. } => {
                        format!("{} == {}", scrut, name)
                    }
                    _ => format!("true /* unsupported pattern */"),
                };
                if first {
                    self.write_source(&format!("if ({})", cond));
                    first = false;
                } else {
                    self.write_source(&format!("else if ({})", cond));
                }
            }

            self.write_source("{");
            self.push_indent();

            // Emit the arm body using gen_stmt so all statement types are handled correctly.
            // Wrap non-block expressions in a synthetic Stmt::Expr for uniform dispatch.
            match &arm.body {
                Expr::Block(block, _) => {
                    self.gen_block_source(block);
                }
                Expr::Match {
                    scrutinee: inner_scrut,
                    arms: inner_arms,
                    ..
                } => {
                    self.gen_match_as_statement(inner_scrut, inner_arms);
                }
                Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } => {
                    self.gen_if_stmt(condition, then_branch, else_branch);
                }
                Expr::Assign { target, value, .. } => {
                    self.write_source(&format!(
                        "{} = {};",
                        self.gen_expr(target),
                        self.gen_expr(value)
                    ));
                }
                Expr::Return(Some(val), _) => {
                    self.write_source(&format!("return {};", self.gen_expr(val)));
                }
                Expr::Return(None, _) => {
                    self.write_source("return;");
                }
                Expr::Ident(name, _) if name == "pass" => {}
                other => {
                    let s = self.gen_expr(other);
                    if !s.is_empty() {
                        self.write_source(&format!("{};", s));
                    }
                }
            }

            self.pop_indent();
            self.write_source("}");
        }
    }

    fn gen_else_continuation(&mut self, branch: &ElseBranch) {
        match branch {
            ElseBranch::Else(block) => {
                self.write_source("}");
                self.write_source("else");
                self.write_source("{");
                self.push_indent();
                self.gen_block_source(block);
                self.pop_indent();
                self.write_source("}");
            }
            ElseBranch::ElseIf(cond, block, next_else) => {
                self.write_source("}");
                self.write_source(&format!("else if ({})", self.gen_expr(cond)));
                self.write_source("{");
                self.push_indent();
                self.gen_block_source(block);
                self.pop_indent();
                if let Some(next) = next_else {
                    self.gen_else_continuation(next);
                } else {
                    self.write_source("}");
                }
            }
        }
    }

    fn gen_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => format!("{:.6}f", f),
            Expr::String(s, _) => format!("TEXT(\"{}\")", self.escape_string(s)),
            Expr::FString(parts, _) => {
                // Build FString::Printf for interpolated strings
                let mut fmt_str = String::new();
                let mut fmt_args: Vec<String> = Vec::new();
                for part in parts {
                    match part {
                        Expr::String(s, _) => {
                            fmt_str.push_str(&self.escape_string(s).replace('%', "%%"));
                        }
                        _ => {
                            let expr_code = self.gen_expr(part);
                            if self.is_enum_expr(part) {
                                fmt_str.push_str("%d");
                                fmt_args.push(format!("static_cast<int32>({})", expr_code));
                            } else {
                                // Determine format specifier based on expression
                                // For now use %s with LexToString for general case
                                fmt_str.push_str("%s");
                                fmt_args.push(format!("*LexToString({})", expr_code));
                            }
                        }
                    }
                }
                if fmt_args.is_empty() {
                    format!("TEXT(\"{}\")", fmt_str)
                } else {
                    format!(
                        "FString::Printf(TEXT(\"{}\"), {})",
                        fmt_str,
                        fmt_args.join(", ")
                    )
                }
            }
            Expr::Bool(b, _) => {
                if *b {
                    "true".to_string()
                } else {
                    "false".to_string()
                }
            }
            Expr::None(_) => "nullptr".to_string(),
            Expr::Ident(name, _) => {
                // Check ident remapping table first (e.g. delta_time -> DeltaTime)
                let remapped = self.context.remap_ident(name.as_str());
                if remapped != name.as_str() {
                    return remapped;
                }
                // Map KAIN constants to C++ equivalents
                match name.as_str() {
                    "null" => "nullptr".to_string(),
                    "None" => "NAME_None".to_string(),
                    _ => name.clone(),
                }
            }

            Expr::Binary {
                left, op, right, ..
            } => {
                let l = self.gen_expr(left);
                let r = self.gen_expr(right);
                if *op == BinaryOp::Mod
                    && (self.is_likely_float_expr(left) || self.is_likely_float_expr(right))
                {
                    format!(
                        "FMath::Fmod(static_cast<double>({}), static_cast<double>({}))",
                        l, r
                    )
                } else {
                    let op_str = self.map_binop(op);
                    format!("({} {} {})", l, op_str, r)
                }
            }

            Expr::Unary { op, operand, .. } => {
                let o = self.gen_expr(operand);
                let op_str = self.map_unaryop(op);
                if matches!(op, UnaryOp::Not) {
                    if let Expr::Ident(name, _) = operand.as_ref() {
                        if let Some(ty_name) = self.var_types.get(name) {
                            let is_bool = ty_name == "bool" || ty_name == "Bool";
                            let is_pointer_like = ty_name.ends_with('*')
                                || ty_name.contains('*')
                                || ty_name == "nullptr";
                            if !is_bool && !is_pointer_like {
                                return "false".to_string();
                            }
                        }
                    }
                }
                format!("({}{})", op_str, o)
            }

            Expr::Call { callee, args, .. } => {
                let fn_name = self.gen_expr(callee);
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();

                if fn_name == "not" && args.len() == 1 {
                    if let Expr::Ident(name, _) = &args[0].value {
                        if let Some(ty_name) = self.var_types.get(name) {
                            let is_bool = ty_name == "bool" || ty_name == "Bool";
                            let is_pointer_like = ty_name.ends_with('*')
                                || ty_name.contains('*')
                                || self.context.is_component(ty_name)
                                || self.context.is_actor(ty_name);
                            if is_bool {
                                return format!("(!{})", arg_strs[0]);
                            }
                            if is_pointer_like {
                                return format!("({} == nullptr)", arg_strs[0]);
                            }
                            return "false".to_string();
                        }
                    }
                    return format!("(!{})", arg_strs[0]);
                }

                // 1. Handle built-in logging (Prioritize specialized UE5 formatting)
                if fn_name == "println" || fn_name == "print" {
                    if args.is_empty() {
                        return "UE_LOG(LogTemp, Warning, TEXT(\"\"))".to_string();
                    }

                    // For single string literal — may contain {identifier} interpolation
                    if args.len() == 1 {
                        if let Expr::String(s, _) = &args[0].value {
                            // Check if the string contains {identifier} interpolation patterns.
                            // KAIN allows `println("Value is {x}")` with a regular string literal
                            // (no `f"..."` prefix required). We apply the same transformation as
                            // for Expr::FString: replace {var} with %s and collect format args.
                            let (fmt_str, fmt_args) = self.interpolate_raw_string(s);
                            if fmt_args.is_empty() {
                                return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"))", fmt_str);
                            } else {
                                return format!(
                                    "UE_LOG(LogTemp, Warning, TEXT(\"{}\"), {})",
                                    fmt_str,
                                    fmt_args.join(", ")
                                );
                            }
                        }
                        // For single FString with interpolation, generate direct UE_LOG format
                        if let Expr::FString(parts, _) = &args[0].value {
                            let mut fmt_str = String::new();
                            let mut fmt_args: Vec<String> = Vec::new();

                            for part in parts {
                                match part {
                                    Expr::String(s, _) => {
                                        fmt_str.push_str(&self.escape_string(s));
                                    }
                                    _ => {
                                        let expr_code = self.gen_expr(part);
                                        if self.is_enum_expr(part) {
                                            fmt_str.push_str("%d");
                                            fmt_args
                                                .push(format!("static_cast<int32>({})", expr_code));
                                        } else if self.is_pointer_receiver(part) {
                                            fmt_str.push_str("%s");
                                            fmt_args.push(format!("*GetNameSafe({})", expr_code));
                                        } else {
                                            // Determine proper UE_LOG format specifier based on type
                                            let (spec, arg) =
                                                get_ue_log_format_spec(part, &expr_code);
                                            fmt_str.push_str(&spec);
                                            fmt_args.push(arg);
                                        }
                                    }
                                }
                            }

                            if fmt_args.is_empty() {
                                return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"))", fmt_str);
                            } else {
                                return format!(
                                    "UE_LOG(LogTemp, Warning, TEXT(\"{}\"), {})",
                                    fmt_str,
                                    fmt_args.join(", ")
                                );
                            }
                        }
                    }

                    // For multiple args or non-string, convert all to FString
                    let arg_strs: Vec<String> = args
                        .iter()
                        .map(|a| {
                            let expr = self.gen_expr(&a.value);
                            if self.is_enum_expr(&a.value) {
                                return format!("FString::FromInt(static_cast<int32>({}))", expr);
                            }
                            if self.is_pointer_receiver(&a.value) {
                                return format!("GetNameSafe({})", expr);
                            }
                            // Wrap non-string types in FString conversion for formatting
                            if expr.starts_with("TEXT(") {
                                // Don't wrap TEXT() in FString - just use it directly
                                expr
                            } else if expr.starts_with("FString") {
                                expr
                            } else {
                                format!("LexToString({})", expr)
                            }
                        })
                        .collect();

                    // Special case: single TEXT() literal should use direct logging
                    if arg_strs.len() == 1 {
                        let arg = &arg_strs[0];
                        if arg.starts_with("FString(TEXT(") {
                            // Extract just the TEXT(...) part
                            let inner = &arg[8..arg.len() - 1];
                            return format!("UE_LOG(LogTemp, Warning, {})", inner);
                        } else if arg.starts_with("TEXT(") {
                            return format!("UE_LOG(LogTemp, Warning, {})", arg);
                        } else if arg.starts_with("\"") {
                            // Raw string literal, wrap in TEXT
                            return format!(
                                "UE_LOG(LogTemp, Warning, TEXT(\"{}\"))",
                                self.escape_string(arg.trim_matches('\"'))
                            );
                        }
                    }

                    // Join args with space separator for simple multi-arg logging
                    // Use FString concatenation with proper wrapping
                    let joined_args = arg_strs
                        .iter()
                        .map(|s| {
                            if s.starts_with("TEXT(") {
                                format!("FString({})", s)
                            } else if s.starts_with("FString") {
                                s.clone()
                            } else {
                                s.clone()
                            }
                        })
                        .collect::<Vec<_>>()
                        .join(" + FString(TEXT(\" \")) + ");

                    return format!("UE_LOG(LogTemp, Warning, TEXT(\"%s\"), *({}))", joined_args);
                }

                // Handle vector constructors (vec2, vec3, vec4 and PascalCase variants)
                match fn_name.as_str() {
                    "vec2" | "Vec2" => return format!("FVector2D({})", arg_strs.join(", ")),
                    "vec3" | "Vec3" => return format!("FVector({})", arg_strs.join(", ")),
                    "vec4" | "Vec4" => return format!("FVector4({})", arg_strs.join(", ")),
                    "Color" | "color" => return format!("FLinearColor({})", arg_strs.join(", ")),
                    "rotation" | "Rotation" | "rotator" | "Rotator" => {
                        return format!("FRotator({})", arg_strs.join(", "))
                    }
                    "transform" | "Transform" => {
                        return format!("FTransform({})", arg_strs.join(", "))
                    }
                    "string" => {
                        return {
                            if arg_strs.len() == 1 {
                                format!("LexToString({})", arg_strs[0])
                            } else {
                                format!("LexToString({})", arg_strs.join(", "))
                            }
                        }
                    }
                    _ => {}
                }

                // Check if this is a KNOWN struct constructor (registered in context or EngineKnowledge)
                // Only add F-prefix if we can confirm this is actually a struct type.
                // Do NOT blindly prefix all PascalCase calls — that breaks actor method calls
                // like SetStatus(), UpdateMaterial(), CreateDynamicMaterialInstance(), etc.
                if fn_name.chars().next().map_or(false, |c| c.is_uppercase())
                    && !fn_name.starts_with("Server_")
                    && !fn_name.starts_with("Client_")
                    && !fn_name.starts_with("Multicast_")
                {
                    // Only prefix if it's a KNOWN struct or component type
                    if self.context.is_struct(&fn_name) {
                        let ue_name = to_struct_name(&fn_name);
                        return format!("{}({})", ue_name, arg_strs.join(", "));
                    }
                    if self.context.is_component(&fn_name) {
                        let ue_name = to_uobject_name(&fn_name);
                        return format!("{}({})", ue_name, arg_strs.join(", "));
                    }
                    // Check EngineKnowledge for engine struct constructors
                    let kb = &self.context.knowledge;
                    if kb.is_engine_struct(&fn_name) {
                        let ue_name = if fn_name.starts_with('F') {
                            fn_name.clone()
                        } else {
                            format!("F{}", fn_name)
                        };
                        return format!("{}({})", ue_name, arg_strs.join(", "));
                    }
                    // Otherwise, emit the call as-is (it's a function/method call, not a constructor)
                }

                // Try stdlib resolver first (centralized math function mapping)
                if let Ok(ue5_code) = self.stdlib_resolver.resolve(&fn_name, &arg_strs) {
                    return ue5_code;
                }

                // Map remaining special functions (vector ops, UE5-specific, etc.)
                let ue5_fn_name = match fn_name.as_str() {
                    "dot" => {
                        return {
                            // Use component-wise dot product for float vectors
                            if arg_strs.len() == 2 {
                                // For Vec2, use manual dot product: a.X * b.X + a.Y * b.Y
                                // For Vec3, use FVector::DotProduct
                                format!("FVector::DotProduct({}, {})", arg_strs[0], arg_strs[1])
                            } else {
                                format!("FVector::DotProduct({})", arg_strs.join(", "))
                            }
                        };
                    }
                    "cross" => "FVector::CrossProduct",
                    "normalize" => {
                        return {
                            if arg_strs.len() == 1 {
                                format!("{}.GetSafeNormal()", arg_strs[0])
                            } else {
                                format!("FVector::GetSafeNormal({})", arg_strs.join(", "))
                            }
                        }
                    }
                    "length" => {
                        return {
                            if arg_strs.len() == 1 {
                                format!("{}.Size()", arg_strs[0])
                            } else {
                                format!("FVector::Size({})", arg_strs.join(", "))
                            }
                        }
                    }
                    "distance" | "dist" => {
                        return {
                            // Use appropriate Dist function based on vector type
                            if arg_strs.len() == 2 {
                                format!("FVector2D::Dist({}, {})", arg_strs[0], arg_strs[1])
                            } else {
                                format!("FVector::Dist({})", arg_strs.join(", "))
                            }
                        };
                    }

                    // Interp functions
                    "FInterpTo" => "FMath::FInterpTo",
                    "VInterpTo" => "FMath::VInterpTo",
                    "RInterpTo" => "FMath::RInterpTo",

                    // Material creation — route through mesh component for fragment shader actors
                    // CreateDynamicMaterialInstance(ShaderName) → mesh_component->CreateDynamicMaterialInstance(0)
                    // Fragment shaders use materials (assigned in editor), not direct shader references
                    "CreateDynamicMaterialInstance" => {
                        return {
                            // Find the first mesh component field from var_types
                            let mesh_field = self
                                .var_types
                                .iter()
                                .find(|(_, ty)| {
                                    ty.contains("MeshComponent")
                                        || ty.contains("StaticMesh")
                                        || ty.contains("SkeletalMesh")
                                })
                                .map(|(name, _)| name.clone());
                            if let Some(mesh) = mesh_field {
                                format!("{}->CreateDynamicMaterialInstance(0)", mesh)
                            } else {
                                // Fallback: assume mesh_component exists
                                "mesh_component->CreateDynamicMaterialInstance(0)".to_string()
                            }
                        };
                    }

                    // UE5 specific mappings from stdlib extern functions
                    "GetWorldDeltaSeconds" => return "GetWorld()->GetDeltaSeconds()".to_string(),
                    "GetWorldTimeSeconds" => return "GetWorld()->GetTimeSeconds()".to_string(),
                    "GetWorldRealTimeSeconds" => {
                        return "GetWorld()->GetRealTimeSeconds()".to_string()
                    }
                    "IsServer" => return "HasAuthority()".to_string(),
                    "IsClient" => return "!HasAuthority()".to_string(),
                    "IsStandalone" => return "GetNetMode() == NM_Standalone".to_string(),
                    "PrintToScreen" => {
                        return {
                            if arg_strs.len() >= 3 {
                                format!(
                                    "GEngine->AddOnScreenDebugMessage(-1, {}, {}, {})",
                                    arg_strs.get(1).map(|s| s.as_str()).unwrap_or("5.0f"),
                                    arg_strs
                                        .get(2)
                                        .map(|s| s.as_str())
                                        .unwrap_or("FColor::White"),
                                    arg_strs[0]
                                )
                            } else if arg_strs.len() == 1 {
                                format!(
                                    "GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, {})",
                                    arg_strs[0]
                                )
                            } else {
                                format!(
                                    "GEngine->AddOnScreenDebugMessage(-1, {}, FColor::White, {})",
                                    arg_strs.get(1).map(|s| s.as_str()).unwrap_or("5.0f"),
                                    arg_strs[0]
                                )
                            }
                        }
                    }
                    "send_gameplay_event" => {
                        return {
                            if let Some(tag_arg) = arg_strs.get(0) {
                                format!(
                                "UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, FGameplayTag::RequestGameplayTag(FName({})), FGameplayEventData())",
                                tag_arg
                            )
                            } else {
                                "/* TODO(kain): send_gameplay_event missing tag argument */"
                                    .to_string()
                            }
                        }
                    }

                    _ => fn_name.as_str(),
                };

                // BUG-008: qualify @blueprint fn calls with U{Plugin}FunctionLibrary::
                if self.blueprint_fn_names.contains(ue5_fn_name) {
                    let lib_class = format!("U{}FunctionLibrary", self.module_name);
                    return format!("{}::{}({})", lib_class, ue5_fn_name, arg_strs.join(", "));
                }

                format!("{}({})", ue5_fn_name, arg_strs.join(", "))
            }

            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let recv = self.gen_expr(receiver);
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();

                // Map KAIN array/collection methods to UE5 TArray equivalents
                let ue5_method = match method.as_str() {
                    "len" | "length" | "count" | "size" => "Num",
                    "push" | "append" | "add" => "Add",
                    "pop" => "Pop",
                    "clear" | "empty" => "Empty",
                    "remove" => "RemoveAt",
                    "contains" => "Contains",
                    "find" => "Find",
                    "insert" => "Insert",
                    "sort" => "Sort",
                    _ => method.as_str(),
                };

                // Handle self.Method() -> Method()
                if recv == "self" {
                    format!("{}({})", ue5_method, arg_strs.join(", "))
                } else {
                    // Check if receiver is a component pointer (needs -> not .)
                    let is_ptr = self.is_pointer_receiver(receiver);
                    let access_op = if is_ptr { "->" } else { "." };
                    format!(
                        "{}{}{}({})",
                        recv,
                        access_op,
                        ue5_method,
                        arg_strs.join(", ")
                    )
                }
            }

            Expr::Field { object, field, .. } => {
                if let Expr::Ident(obj_name, _) = object.as_ref() {
                    if self.context.is_enum(obj_name) {
                        return format!("{}::{}", to_enum_name(obj_name), field);
                    }
                }

                let obj = self.gen_expr(object);

                // KAIN property-style length access: arr.length / arr.len -> arr.Num()
                if field == "length" || field == "len" || field == "count" || field == "size" {
                    return format!("{}.Num()", obj);
                }

                // Remap vector component field names ONLY for UE5 vector types
                // For user-defined structs, keep lowercase field names
                // Check if this is accessing x/y/z/w fields
                let is_component_field = matches!(
                    field.as_str(),
                    "x" | "y" | "z" | "w" | "r" | "g" | "b" | "a"
                );

                let should_capitalize = if is_component_field {
                    if let Expr::Ident(obj_name, _) = object.as_ref() {
                        // Debug logging to file
                        use std::io::Write;
                        if let Ok(mut file) = std::fs::OpenOptions::new()
                            .create(true)
                            .append(true)
                            .open("codegen_debug.log")
                        {
                            let _ = writeln!(file, "Field access: {}.{}", obj_name, field);
                            let _ = writeln!(
                                file,
                                "  var_types contains {} entries",
                                self.var_types.len()
                            );
                            if let Some(type_name) = self.var_types.get(obj_name) {
                                let _ = writeln!(file, "  Found type: {}", type_name);
                            } else {
                                let _ = writeln!(file, "  NOT in var_types");
                                let _ = writeln!(
                                    file,
                                    "  Available vars: {:?}",
                                    self.var_types.keys().collect::<Vec<_>>()
                                );
                            }
                        }

                        // Check if this is a variable with a known type
                        if let Some(type_name) = self.var_types.get(obj_name) {
                            // Only capitalize if it's a known UE5 vector type
                            // If it's a user-defined struct, keep lowercase
                            let is_ue5_vector = matches!(
                                type_name.as_str(),
                                "Vec2"
                                    | "Vec3"
                                    | "Vec4"
                                    | "FVector"
                                    | "FVector2D"
                                    | "FVector4"
                                    | "FIntVector"
                                    | "FIntPoint"
                            );
                            is_ue5_vector
                        } else {
                            // Not in var_types (common for inferred local temporaries like
                            // `let local_pos = ...`). Default to capitalizing component access
                            // to preserve UE vector semantics (.X/.Y/.Z/.W).
                            true
                        }
                    } else {
                        // Complex expression - default to capitalizing for backward compatibility
                        true
                    }
                } else {
                    // Not a component field - keep as-is
                    false
                };

                let ue5_field = if should_capitalize {
                    match field.as_str() {
                        "x" => "X",
                        "y" => "Y",
                        "z" => "Z",
                        "w" => "W",
                        "r" => "X",
                        "g" => "Y",
                        "b" => "Z",
                        "a" => "W",
                        _ => field.as_str(),
                    }
                } else {
                    field.as_str()
                };

                if obj == "self" {
                    ue5_field.to_string()
                } else {
                    // Check if the object is a component pointer (needs -> not .)
                    let is_ptr = self.is_pointer_receiver(object);
                    let access_op = if is_ptr { "->" } else { "." };
                    format!("{}{}{}", obj, access_op, ue5_field)
                }
            }

            Expr::Index { object, index, .. } => {
                format!("{}[{}]", self.gen_expr(object), self.gen_expr(index))
            }

            Expr::Array(elements, _) => {
                let elems: Vec<String> = elements.iter().map(|e| self.gen_expr(e)).collect();
                // Emit a typed TArray literal to avoid narrowing/conversion ambiguity.
                let elem_ty = self.infer_array_element_cpp_type(elements);
                let rendered_elems = if elem_ty == "float" {
                    elems
                        .into_iter()
                        .map(|e| format!("static_cast<float>({})", e))
                        .collect::<Vec<_>>()
                        .join(", ")
                } else {
                    elems.join(", ")
                };
                format!("TArray<{}>{{{}}}", elem_ty, rendered_elems)
            }

            Expr::Struct { name, fields, .. } => {
                let ue_name = to_struct_name(name);
                let field_strs: Vec<String> =
                    fields.iter().map(|(_, fval)| self.gen_expr(fval)).collect();
                format!("{}{{{}}}", ue_name, field_strs.join(", "))
            }

            Expr::EnumVariant {
                enum_name, variant, ..
            } => {
                let ue_name = to_enum_name(enum_name);
                format!("{}::{}", ue_name, variant)
            }

            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                // Ternary for simple cases
                if then_branch.stmts.len() == 1 && else_branch.is_some() {
                    if let Stmt::Expr(then_expr) = &then_branch.stmts[0] {
                        if let Some(ElseBranch::Else(else_block)) =
                            else_branch.as_ref().map(|b| b.as_ref())
                        {
                            if else_block.stmts.len() == 1 {
                                if let Stmt::Expr(else_expr) = &else_block.stmts[0] {
                                    let cond = self.gen_expr(condition);
                                    let then_code = self.gen_expr(then_expr);
                                    let else_code = self.gen_expr(else_expr);
                                    return format!("({} ? {} : {})", cond, then_code, else_code);
                                }
                            }
                        }
                    }
                }
                "/* complex if expr */".to_string()
            }

            Expr::Lambda { params, body, .. } => {
                let param_strs: Vec<String> =
                    params.iter().map(|p| format!("auto {}", p.name)).collect();
                format!(
                    "[this]({}){{{}}}",
                    param_strs.join(", "),
                    self.gen_expr(body)
                )
            }

            Expr::Cast { value, target, .. } => {
                format!(
                    "static_cast<{}>({})",
                    self.map_type(target),
                    self.gen_expr(value)
                )
            }
            Expr::Bitcast { value, target, .. } => {
                format!(
                    "reinterpret_cast<{}>({})",
                    self.map_type(target),
                    self.gen_expr(value)
                )
            }

            Expr::Paren(inner, _) => {
                format!("({})", self.gen_expr(inner))
            }

            Expr::Return(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    format!("return {}", self.gen_expr(expr))
                } else {
                    "return".to_string()
                }
            }

            Expr::Break(_, _) => "break".to_string(),
            Expr::Continue(_) => "continue".to_string(),

            Expr::Match {
                scrutinee, arms, ..
            } => {
                // Check if any arm body is an assignment - if so, this is a statement-level match
                let has_assignment = arms
                    .iter()
                    .any(|arm| matches!(&arm.body, Expr::Assign { .. }));
                // Also force statement mode when arm bodies are block/flow/no-op forms
                // that cannot be represented as a valid C++ ternary expression.
                let has_statement_arms = arms.iter().any(|arm| match &arm.body {
                    Expr::Block(_, _)
                    | Expr::If { .. }
                    | Expr::Match { .. }
                    | Expr::Return(_, _)
                    | Expr::Break(_, _)
                    | Expr::Continue(_) => true,
                    Expr::Ident(name, _) => name == "pass",
                    _ => false,
                });

                // Detect if the match can be represented as ternary (simple patterns only)
                let has_complex_patterns = arms.iter().any(|arm| {
                    matches!(
                        &arm.pattern,
                        Pattern::Tuple(_, _)
                            | Pattern::Struct { .. }
                            | Pattern::Slice { .. }
                            | Pattern::Range { .. }
                            | Pattern::Or(_, _)
                    )
                });

                let scrut = self.gen_expr(scrutinee);

                // If arms contain assignments, generate as statement-level if/else
                if has_assignment || has_statement_arms {
                    // Generate if/else chain for assignments
                    let mut result = String::new();
                    let mut first = true;
                    let mut emit_arm_body = |body: &Expr| -> String {
                        match body {
                            Expr::Assign { target, value, .. } => {
                                format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value))
                            }
                            Expr::Block(block, _) => {
                                let mut out = String::new();
                                for stmt in &block.stmts {
                                    match stmt {
                                        Stmt::Expr(e) => {
                                            let s = self.gen_expr(e);
                                            if !s.is_empty() && s != "pass" {
                                                out.push_str(&format!("{}; ", s));
                                            }
                                        }
                                        Stmt::Return(Some(e), _) => {
                                            out.push_str(&format!("return {}; ", self.gen_expr(e)));
                                        }
                                        Stmt::Return(None, _) => out.push_str("return; "),
                                        _ => {}
                                    }
                                }
                                out
                            }
                            Expr::Ident(name, _) if name == "pass" => String::new(),
                            Expr::Ident(name, _) => format!("{}; ", name),
                            _ => format!("{}; ", self.gen_expr(body)),
                        }
                    };

                    for arm in arms {
                        match &arm.pattern {
                            Pattern::Wildcard(_) => {
                                // Default case
                                if !first {
                                    result.push_str(" else ");
                                }
                                result.push_str("{ ");
                                result.push_str(&emit_arm_body(&arm.body));
                                result.push_str("}");
                            }
                            Pattern::Binding { name, .. } if name == "_" => {
                                // Default case with _ binding
                                if !first {
                                    result.push_str(" else ");
                                }
                                result.push_str("{ ");
                                result.push_str(&emit_arm_body(&arm.body));
                                result.push_str("}");
                            }
                            Pattern::Variant {
                                enum_name, variant, ..
                            } => {
                                let path = if let Some(en) = enum_name {
                                    format!("{}::{}", to_enum_name(en), variant)
                                } else {
                                    variant.clone()
                                };

                                if first {
                                    result.push_str(&format!("if ({} == {}) ", scrut, path));
                                    first = false;
                                } else {
                                    result.push_str(&format!("else if ({} == {}) ", scrut, path));
                                }

                                result.push_str("{ ");
                                result.push_str(&emit_arm_body(&arm.body));
                                result.push_str("}");
                            }
                            Pattern::Literal(lit) => {
                                let lit_str = self.gen_expr(lit);

                                if first {
                                    result.push_str(&format!("if ({} == {}) ", scrut, lit_str));
                                    first = false;
                                } else {
                                    result
                                        .push_str(&format!("else if ({} == {}) ", scrut, lit_str));
                                }

                                result.push_str("{ ");
                                result.push_str(&emit_arm_body(&arm.body));
                                result.push_str("}");
                            }
                            Pattern::Binding { name, .. } => {
                                if first {
                                    result.push_str(&format!("if ({} == {}) ", scrut, name));
                                    first = false;
                                } else {
                                    result.push_str(&format!("else if ({} == {}) ", scrut, name));
                                }

                                result.push_str("{ ");
                                result.push_str(&emit_arm_body(&arm.body));
                                result.push_str("}");
                            }
                            _ => {
                                // Unsupported pattern
                                result.push_str("/* unsupported pattern */");
                            }
                        }
                    }

                    return result;
                }

                if has_complex_patterns {
                    // Complex patterns need lambda-wrapped if/else chain
                    // [&]() { if (cond1) return val1; else if (cond2) return val2; else return default; }()
                    let mut parts = Vec::new();
                    let mut default_value = None;

                    for arm in arms {
                        let arm_value = self.gen_expr(&arm.body);
                        match &arm.pattern {
                            Pattern::Wildcard(_) => {
                                default_value = Some(arm_value);
                            }
                            Pattern::Binding { name, .. } if name == "_" => {
                                default_value = Some(arm_value);
                            }
                            Pattern::Literal(lit) => {
                                let lit_str = self.gen_expr(lit);
                                parts.push(format!("{} == {}", scrut, lit_str));
                                parts.push(arm_value);
                            }
                            Pattern::Variant {
                                enum_name, variant, ..
                            } => {
                                let path = if let Some(en) = enum_name {
                                    format!("{}::{}", to_enum_name(en), variant)
                                } else {
                                    variant.clone()
                                };
                                parts.push(format!("{} == {}", scrut, path));
                                parts.push(arm_value);
                            }
                            Pattern::Range {
                                start,
                                end,
                                inclusive,
                                ..
                            } => {
                                // Range pattern: 1..10 or 1..=10
                                let start_str =
                                    start.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                                let end_str =
                                    end.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                                let op = if *inclusive { "<=" } else { "<" };

                                if start_str.is_empty() {
                                    parts.push(format!("{} {} {}", scrut, op, end_str));
                                } else if end_str.is_empty() {
                                    parts.push(format!("{} >= {}", scrut, start_str));
                                } else {
                                    parts.push(format!(
                                        "({} >= {} && {} {} {})",
                                        scrut, start_str, scrut, op, end_str
                                    ));
                                }
                                parts.push(arm_value);
                            }
                            _ => {
                                // For Struct/Tuple/Slice patterns, we can't easily generate C++ equivalent
                                // Fall through to default or generate a compile warning
                                parts.push(format!("/* unsupported pattern for {} */ true", scrut));
                                parts.push(arm_value);
                            }
                        }
                    }

                    // Generate lambda if/else chain
                    let mut result = String::from("[&]() { ");
                    for i in (0..parts.len()).step_by(2) {
                        if i == 0 {
                            result.push_str(&format!(
                                "if ({}) return {}; ",
                                parts[i],
                                parts[i + 1]
                            ));
                        } else {
                            result.push_str(&format!(
                                "else if ({}) return {}; ",
                                parts[i],
                                parts[i + 1]
                            ));
                        }
                    }
                    if let Some(def) = default_value {
                        result.push_str(&format!("else return {}; ", def));
                    }
                    result.push_str("}()");
                    result
                } else {
                    // Simple ternary chain for literals/enums/bindings
                    let mut result = String::new();
                    let mut first = true;
                    let mut has_wildcard = false;
                    let mut wildcard_value = String::new();

                    for arm in arms {
                        let arm_value = self.gen_expr(&arm.body);

                        match &arm.pattern {
                            Pattern::Wildcard(_) => {
                                has_wildcard = true;
                                wildcard_value = arm_value;
                            }
                            Pattern::Literal(lit) => {
                                let lit_str = self.gen_expr(lit);
                                if !first {
                                    result.push_str(" : ");
                                }
                                result.push_str(&format!(
                                    "({} == {}) ? {}",
                                    scrut, lit_str, arm_value
                                ));
                                first = false;
                            }
                            Pattern::Binding { name, .. } if name == "_" => {
                                has_wildcard = true;
                                wildcard_value = arm_value;
                            }
                            Pattern::Binding { name, .. } => {
                                if !first {
                                    result.push_str(" : ");
                                }
                                result
                                    .push_str(&format!("({} == {}) ? {}", scrut, name, arm_value));
                                first = false;
                            }
                            Pattern::Variant {
                                enum_name, variant, ..
                            } => {
                                let path = if let Some(en) = enum_name {
                                    format!("{}::{}", to_enum_name(en), variant)
                                } else {
                                    variant.clone()
                                };
                                if !first {
                                    result.push_str(" : ");
                                }
                                result
                                    .push_str(&format!("({} == {}) ? {}", scrut, path, arm_value));
                                first = false;
                            }
                            _ => {
                                // Shouldn't hit this in simple path but handle gracefully
                                if !first {
                                    result.push_str(" : ");
                                }
                                result.push_str(&arm_value);
                                first = false;
                            }
                        }
                    }

                    if has_wildcard {
                        if !first {
                            result.push_str(" : ");
                        }
                        result.push_str(&wildcard_value);
                    }

                    if result.contains(" : ") {
                        format!("({})", result)
                    } else if result.is_empty() {
                        "/* empty match */".to_string()
                    } else {
                        result
                    }
                }
            }

            _ => "/* unhandled expr */".to_string(),
        }
    }

    /// Map KAIN types to UE5 C++ types using centralized TypeMapper
    /// This eliminates duplicate type mapping logic and prevents double-prefixing bugs
    fn map_type(&self, ty: &Type) -> String {
        self.type_mapper.map_type_string(ty)
    }

    fn map_binop(&self, op: &BinaryOp) -> &'static str {
        match op {
            BinaryOp::Add => "+",
            BinaryOp::Sub => "-",
            BinaryOp::Mul => "*",
            BinaryOp::Div => "/",
            BinaryOp::Mod => "%",
            BinaryOp::And => "&&",
            BinaryOp::Or => "||",
            BinaryOp::Eq => "==",
            BinaryOp::Ne => "!=",
            BinaryOp::Lt => "<",
            BinaryOp::Le => "<=",
            BinaryOp::Gt => ">",
            BinaryOp::Ge => ">=",
            BinaryOp::BitAnd => "&",
            BinaryOp::BitOr => "|",
            BinaryOp::BitXor => "^",
            BinaryOp::Shl => "<<",
            BinaryOp::Shr => ">>",
            BinaryOp::Assign => "=",
            BinaryOp::AddAssign => "+=",
            BinaryOp::SubAssign => "-=",
            BinaryOp::MulAssign => "*=",
            BinaryOp::DivAssign => "/=",
            _ => "/* unknown op */",
        }
    }

    fn map_unaryop(&self, op: &UnaryOp) -> &'static str {
        match op {
            UnaryOp::Not => "!",
            UnaryOp::Neg => "-",
            UnaryOp::Ref => "&",
            UnaryOp::RefMut => "&",
            UnaryOp::BitNot => "~",
            UnaryOp::Deref => "*",
        }
    }

    fn escape_string(&self, s: &str) -> String {
        ue5_escape_string(s)
    }

    /// Generate multicast delegate declaration
    fn gen_multicast_delegate(&mut self, alias: &kain_core::ast::TypeAlias) {
        // Extract parameter types and return type from the function type
        if let Type::Function {
            params,
            return_type,
            ..
        } = &alias.target
        {
            let delegate_name = format!("F{}", alias.name);

            // Track this delegate name
            self.context.register_delegate(
                alias.name.clone(),
                format!("{}.h", self.context.output_name),
            );

            // Generate native C++ multicast delegate (not dynamic/blueprint)
            // Use DECLARE_MULTICAST_DELEGATE_* macros based on parameter count
            match params.len() {
                0 => {
                    // No parameters
                    self.write_header(&format!("DECLARE_MULTICAST_DELEGATE({});", delegate_name));
                }
                1 => {
                    // One parameter
                    let param_type = self.map_type(&params[0]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_OneParam({}, {});",
                        delegate_name, param_type
                    ));
                }
                2 => {
                    // Two parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_TwoParams({}, {}, {});",
                        delegate_name, param1_type, param2_type
                    ));
                }
                3 => {
                    // Three parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_ThreeParams({}, {}, {}, {});",
                        delegate_name, param1_type, param2_type, param3_type
                    ));
                }
                4 => {
                    // Four parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_FourParams({}, {}, {}, {}, {});",
                        delegate_name, param1_type, param2_type, param3_type, param4_type
                    ));
                }
                5 => {
                    // Five parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_FiveParams({}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type
                    ));
                }
                6 => {
                    // Six parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_SixParams({}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type
                    ));
                }
                7 => {
                    // Seven parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    self.write_header(&format!(
                        "DECLARE_MULTICAST_DELEGATE_SevenParams({}, {}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type,
                        param7_type
                    ));
                }
                8 => {
                    // Eight parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    let param8_type = self.map_type(&params[7]);
                    self.write_header(&format!("DECLARE_MULTICAST_DELEGATE_EightParams({}, {}, {}, {}, {}, {}, {}, {}, {});", 
                        delegate_name, param1_type, param2_type, param3_type, param4_type, param5_type, param6_type, param7_type, param8_type));
                }
                9 => {
                    // Nine parameters (maximum supported by UE5)
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    let param8_type = self.map_type(&params[7]);
                    let param9_type = self.map_type(&params[8]);
                    self.write_header(&format!("DECLARE_MULTICAST_DELEGATE_NineParams({}, {}, {}, {}, {}, {}, {}, {}, {}, {});", 
                        delegate_name, param1_type, param2_type, param3_type, param4_type, param5_type, param6_type, param7_type, param8_type, param9_type));
                }
                _ => {
                    // More than 9 parameters - not supported by UE5 delegate macros
                    self.write_header(&format!(
                        "// Error: Delegate {} has {} parameters - UE5 supports up to 9 parameters",
                        delegate_name,
                        params.len()
                    ));
                    self.write_header(&format!(
                        "// Consider refactoring to use a struct parameter instead"
                    ));
                }
            }

            self.write_blank_header();
        }
    }

    /// Generate regular delegate declaration (non-multicast)
    fn gen_delegate(&mut self, alias: &kain_core::ast::TypeAlias) {
        if let Type::Function {
            params,
            return_type,
            ..
        } = &alias.target
        {
            let delegate_name = format!("F{}", alias.name);

            // Track this delegate name
            self.context.register_delegate(
                alias.name.clone(),
                format!("{}.h", self.context.output_name),
            );

            // Generate native C++ delegate (not dynamic/blueprint)
            // Use DECLARE_DELEGATE_* macros based on parameter count
            match params.len() {
                0 => {
                    // No parameters
                    self.write_header(&format!("DECLARE_DELEGATE({});", delegate_name));
                }
                1 => {
                    // One parameter
                    let param_type = self.map_type(&params[0]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_OneParam({}, {});",
                        delegate_name, param_type
                    ));
                }
                2 => {
                    // Two parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_TwoParams({}, {}, {});",
                        delegate_name, param1_type, param2_type
                    ));
                }
                3 => {
                    // Three parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_ThreeParams({}, {}, {}, {});",
                        delegate_name, param1_type, param2_type, param3_type
                    ));
                }
                4 => {
                    // Four parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_FourParams({}, {}, {}, {}, {});",
                        delegate_name, param1_type, param2_type, param3_type, param4_type
                    ));
                }
                5 => {
                    // Five parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_FiveParams({}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type
                    ));
                }
                6 => {
                    // Six parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_SixParams({}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type
                    ));
                }
                7 => {
                    // Seven parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_SevenParams({}, {}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type,
                        param7_type
                    ));
                }
                8 => {
                    // Eight parameters
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    let param8_type = self.map_type(&params[7]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_EightParams({}, {}, {}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type,
                        param7_type,
                        param8_type
                    ));
                }
                9 => {
                    // Nine parameters (maximum supported by UE5)
                    let param1_type = self.map_type(&params[0]);
                    let param2_type = self.map_type(&params[1]);
                    let param3_type = self.map_type(&params[2]);
                    let param4_type = self.map_type(&params[3]);
                    let param5_type = self.map_type(&params[4]);
                    let param6_type = self.map_type(&params[5]);
                    let param7_type = self.map_type(&params[6]);
                    let param8_type = self.map_type(&params[7]);
                    let param9_type = self.map_type(&params[8]);
                    self.write_header(&format!(
                        "DECLARE_DELEGATE_NineParams({}, {}, {}, {}, {}, {}, {}, {}, {}, {});",
                        delegate_name,
                        param1_type,
                        param2_type,
                        param3_type,
                        param4_type,
                        param5_type,
                        param6_type,
                        param7_type,
                        param8_type,
                        param9_type
                    ));
                }
                _ => {
                    // More than 9 parameters - not supported by UE5 delegate macros
                    self.write_header(&format!(
                        "// Error: Delegate {} has {} parameters - UE5 supports up to 9 parameters",
                        delegate_name,
                        params.len()
                    ));
                    self.write_header(&format!(
                        "// Consider refactoring to use a struct parameter instead"
                    ));
                }
            }

            self.write_blank_header();
        }
    }

    /// Scan an item's types to discover headers that need to be included
    fn discover_item_dependencies(&self, item: &TypedItem) {
        match item {
            TypedItem::Actor(a) => {
                for state in &a.ast.state {
                    self.map_type(&state.ty);
                    self.discover_type_headers(&state.ty);
                }
                for method in &a.ast.methods {
                    if let Some(ret) = &method.return_type {
                        self.map_type(ret);
                        self.discover_type_headers(ret);
                    }
                    for param in &method.params {
                        self.map_type(&param.ty);
                        self.discover_type_headers(&param.ty);
                    }
                }
                for handler in &a.ast.handlers {
                    for param in &handler.params {
                        self.map_type(&param.ty);
                        self.discover_type_headers(&param.ty);
                    }
                }
            }
            TypedItem::Struct(s) => {
                for field in &s.ast.fields {
                    self.map_type(&field.ty);
                    self.discover_type_headers(&field.ty);
                }

                // @subsystem structs generate classes inheriting UWorldSubsystem,
                // and optionally FTickableGameObject when @tick is present.
                // Add required engine headers during dependency discovery so the
                // generated header has all base-class includes.
                if s.ast.attributes.iter().any(|a| a.name == "subsystem") {
                    self.context
                        .need_header("Subsystems/WorldSubsystem.h".to_string());
                    if s.ast.attributes.iter().any(|a| a.name == "tick") {
                        self.context.need_header("Tickable.h".to_string());
                    }
                }
            }
            TypedItem::Component(c) => {
                for state in &c.ast.state {
                    self.map_type(&state.ty);
                    self.discover_type_headers(&state.ty);
                }
            }
            TypedItem::TypeAlias(a) => {
                self.map_type(&a.ast.target);
                self.discover_type_headers(&a.ast.target);
            }
            _ => {}
        }
    }

    /// Recursively walk a Type and call context.need_header() for any type that
    /// has a registered header — either in type_to_header (user-defined types) or
    /// in EngineKnowledge.include_map (engine types like UNiagaraComponent).
    fn discover_type_headers(&self, ty: &Type) {
        match ty {
            Type::Named { name, generics, .. } => {
                // 1. User-defined types registered in type_to_header
                if let Some(header) = self.context.type_to_header.get(name) {
                    self.context.need_header(header.clone());
                }
                // 2. Engine types from EngineKnowledge.include_map
                //    (e.g. UNiagaraComponent -> "NiagaraComponent.h")
                if let Some(header) = self.type_mapper.get_include_path(ty) {
                    self.context.need_header(header);
                }
                for g in generics {
                    self.discover_type_headers(g);
                }
            }
            Type::Array(inner, _, _) | Type::Slice(inner, _) | Type::Option(inner, _) => {
                self.discover_type_headers(inner);
            }
            Type::Result(ok, err, _) => {
                self.discover_type_headers(ok);
                self.discover_type_headers(err);
            }
            Type::Ref { inner, .. } => self.discover_type_headers(inner),
            Type::Tuple(items, _) => {
                for t in items {
                    self.discover_type_headers(t);
                }
            }
            Type::Function {
                params,
                return_type,
                ..
            } => {
                for p in params {
                    self.discover_type_headers(p);
                }
                self.discover_type_headers(return_type);
            }
            _ => {}
        }
    }
}

/// Recursively extract all named KAIN type identifiers from a `Type` node.
/// Used during the `@blueprint` function pre-pass to build `blueprint_used_types`.
fn collect_type_names(ty: &Type, out: &mut std::collections::HashSet<String>) {
    match ty {
        Type::Named { name, generics, .. } => {
            out.insert(name.clone());
            for g in generics {
                collect_type_names(g, out);
            }
        }
        Type::Array(inner, _, _) | Type::Slice(inner, _) => collect_type_names(inner, out),
        Type::Option(inner, _) => collect_type_names(inner, out),
        Type::Result(ok, err, _) => {
            collect_type_names(ok, out);
            collect_type_names(err, out);
        }
        Type::Ref { inner, .. } => collect_type_names(inner, out),
        Type::Tuple(items, _) => {
            for t in items {
                collect_type_names(t, out);
            }
        }
        Type::Function {
            params,
            return_type,
            ..
        } => {
            for p in params {
                collect_type_names(p, out);
            }
            collect_type_names(return_type, out);
        }
        _ => {}
    }
}

fn item_uses_kain_runtime(item: &TypedItem) -> bool {
    match item {
        TypedItem::Function(f) => block_uses_kain_runtime(&f.ast.body),
        TypedItem::Patch(patch) => block_uses_kain_runtime(&patch.ast.body),
        TypedItem::Law(law) => block_uses_kain_runtime(&law.ast.body),
        TypedItem::Axiom(_) => false,
        TypedItem::Pulse(_) => true,
        TypedItem::Converge(converge) => {
            block_uses_kain_runtime(&converge.ast.spec_lane.body)
                || converge
                    .ast
                    .fast_lanes
                    .iter()
                    .any(|lane| block_uses_kain_runtime(&lane.body))
        }
        TypedItem::World(world) => world
            .ast
            .states
            .iter()
            .any(|state| expr_uses_kain_runtime(&state.initial)),
        TypedItem::Orchestrate(orchestrate) => block_uses_kain_runtime(&orchestrate.ast.body),
        TypedItem::Component(c) => {
            c.ast
                .state
                .iter()
                .any(|s| expr_uses_kain_runtime(&s.initial))
                || c.ast
                    .methods
                    .iter()
                    .any(|m| block_uses_kain_runtime(&m.body))
        }
        TypedItem::Shader(s) => block_uses_kain_runtime(&s.ast.body),
        TypedItem::Actor(a) => {
            a.ast
                .state
                .iter()
                .any(|s| expr_uses_kain_runtime(&s.initial))
                || a.ast
                    .handlers
                    .iter()
                    .any(|h| block_uses_kain_runtime(&h.body))
                || a.ast
                    .methods
                    .iter()
                    .any(|m| block_uses_kain_runtime(&m.body))
        }
        TypedItem::Struct(s) => {
            s.ast
                .fields
                .iter()
                .filter_map(|f| f.default.as_ref())
                .any(expr_uses_kain_runtime)
                || s.ast
                    .methods
                    .iter()
                    .any(|m| block_uses_kain_runtime(&m.body))
        }
        TypedItem::Const(c) => expr_uses_kain_runtime(&c.ast.value),
        TypedItem::Comptime(c) => block_uses_kain_runtime(&c.ast),
        TypedItem::Impl(i) => i
            .ast
            .methods
            .iter()
            .any(|m| block_uses_kain_runtime(&m.body)),
        TypedItem::Test(t) => block_uses_kain_runtime(&t.ast.body),
        TypedItem::Mod(module) => module.items.iter().any(item_uses_kain_runtime),
        TypedItem::Trait(_)
        | TypedItem::Entangle(_)
        | TypedItem::TypeAlias(_)
        | TypedItem::Enum(_)
        | TypedItem::Macro(_)
        | TypedItem::Use(_)
        | TypedItem::Import(_)
        | TypedItem::MaterialGraph(_)
        | TypedItem::MaterialFunction(_)
        | TypedItem::GraphEditor(_)
        | TypedItem::GraphRuntime(_)
        | TypedItem::StateMachine(_)
        | TypedItem::AsyncTask(_)
        | TypedItem::EditorModule(_)
        | TypedItem::GameplayTags(_)
        | TypedItem::GameplayAbility(_)
        | TypedItem::GameplayEffect(_)
        | TypedItem::GameplayCue(_) => false,
    }
}

fn block_uses_kain_runtime(block: &Block) -> bool {
    block.stmts.iter().any(stmt_uses_kain_runtime)
}

fn stmt_uses_kain_runtime(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Let { value, .. } => value.as_ref().is_some_and(expr_uses_kain_runtime),
        Stmt::Expr(e) => expr_uses_kain_runtime(e),
        Stmt::Defer { expr, .. } => expr_uses_kain_runtime(expr),
        Stmt::Dispatch { dispatch_size, .. } => dispatch_size.iter().any(expr_uses_kain_runtime),
        Stmt::Return(v, _) | Stmt::Break(v, _) => v.as_ref().is_some_and(expr_uses_kain_runtime),
        Stmt::Continue(_) => false,
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            expr_uses_kain_runtime(iter) || block_uses_kain_runtime(body)
        }
        Stmt::While {
            condition, body, ..
        } => expr_uses_kain_runtime(condition) || block_uses_kain_runtime(body),
        Stmt::Loop { body, .. } => block_uses_kain_runtime(body),
        Stmt::Item(_) => false,
    }
}

fn else_branch_uses_kain_runtime(branch: &ElseBranch) -> bool {
    match branch {
        ElseBranch::Else(block) => block_uses_kain_runtime(block),
        ElseBranch::ElseIf(cond, block, next) => {
            expr_uses_kain_runtime(cond)
                || block_uses_kain_runtime(block)
                || next
                    .as_ref()
                    .is_some_and(|b| else_branch_uses_kain_runtime(b.as_ref()))
        }
    }
}

fn expr_uses_kain_runtime(expr: &Expr) -> bool {
    match expr {
        Expr::Ident(name, _) => name.starts_with("__kain_"),
        Expr::AddrOf { .. } | Expr::Deref(_, _) => true,
        Expr::PtrOffset { .. }
        | Expr::MemLoad { .. }
        | Expr::MemStore { .. }
        | Expr::VolatileLoad { .. }
        | Expr::VolatileStore { .. }
        | Expr::AtomicLoad { .. }
        | Expr::AtomicStore { .. }
        | Expr::AtomicAdd { .. }
        | Expr::AtomicSub { .. }
        | Expr::AtomicAnd { .. }
        | Expr::AtomicOr { .. }
        | Expr::AtomicXor { .. }
        | Expr::AtomicExchange { .. }
        | Expr::AtomicCompareExchange { .. }
        | Expr::AtomicFence { .. } => true,
        Expr::Alloca { .. } | Expr::Uninit { .. } => true,
        Expr::Alloc { .. } | Expr::Realloc { .. } => true,
        Expr::Observe { .. }
        | Expr::Collapse { .. }
        | Expr::Decay { .. }
        | Expr::Share { .. }
        | Expr::Teleport { .. } => true,
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Continue(_) => false,
        Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => {
            parts.iter().any(expr_uses_kain_runtime)
        }
        Expr::MacroCall { args, .. } => args.iter().any(expr_uses_kain_runtime),
        Expr::Binary { left, right, .. } => {
            expr_uses_kain_runtime(left) || expr_uses_kain_runtime(right)
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::Cast { value: operand, .. }
        | Expr::Bitcast { value: operand, .. }
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => expr_uses_kain_runtime(operand),
        Expr::Call { callee, args, .. } => {
            expr_uses_kain_runtime(callee)
                || args.iter().any(|arg| expr_uses_kain_runtime(&arg.value))
        }
        Expr::StageCall { args, .. } => args.iter().any(|arg| expr_uses_kain_runtime(&arg.value)),
        Expr::MethodCall { receiver, args, .. } => {
            expr_uses_kain_runtime(receiver)
                || args.iter().any(|arg| expr_uses_kain_runtime(&arg.value))
        }
        Expr::Field { object, .. } => expr_uses_kain_runtime(object),
        Expr::Index { object, index, .. } => {
            expr_uses_kain_runtime(object) || expr_uses_kain_runtime(index)
        }
        Expr::Assign { target, value, .. } => {
            expr_uses_kain_runtime(target) || expr_uses_kain_runtime(value)
        }
        Expr::Struct { fields, .. } | Expr::AggregateInit { fields, .. } => {
            fields.iter().any(|(_, e)| expr_uses_kain_runtime(e))
        }
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Unit => false,
            EnumVariantFields::Tuple(values) => values.iter().any(expr_uses_kain_runtime),
            EnumVariantFields::Struct(values) => {
                values.iter().any(|(_, e)| expr_uses_kain_runtime(e))
            }
        },
        Expr::Range { start, end, .. } => {
            start
                .as_ref()
                .is_some_and(|e| expr_uses_kain_runtime(e.as_ref()))
                || end
                    .as_ref()
                    .is_some_and(|e| expr_uses_kain_runtime(e.as_ref()))
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            expr_uses_kain_runtime(condition)
                || block_uses_kain_runtime(then_branch)
                || else_branch
                    .as_ref()
                    .is_some_and(|b| else_branch_uses_kain_runtime(b.as_ref()))
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            expr_uses_kain_runtime(scrutinee)
                || arms.iter().any(|arm| {
                    arm.guard.as_ref().is_some_and(expr_uses_kain_runtime)
                        || expr_uses_kain_runtime(&arm.body)
                })
        }
        Expr::Lambda { body, .. } => expr_uses_kain_runtime(body),
        Expr::Spawn { init, .. } => init.iter().any(|(_, e)| expr_uses_kain_runtime(e)),
        Expr::SendMsg { target, data, .. } => {
            expr_uses_kain_runtime(target) || data.iter().any(|(_, e)| expr_uses_kain_runtime(e))
        }
        Expr::Block(block, _) => block_uses_kain_runtime(block),
        Expr::JSX(_, _) => false,
        Expr::Return(value, _) | Expr::Break(value, _) => value
            .as_ref()
            .is_some_and(|v| expr_uses_kain_runtime(v.as_ref())),
        Expr::CpuFence { .. } | Expr::CpuCacheFlush { .. } | Expr::InlineAsm { .. } => true,
    }
}

/// Return a C++11 member initializer value for common UE5 C++ types.
/// Used in USTRUCT field declarations to satisfy UE5.4 `LogClass` strictness
/// (`Property ... is not initialized properly`).
/// Returns `None` for self-initializing types (FString, TArray, etc.) where
/// an explicit initializer is unnecessary.
fn default_cpp_value(ty_str: &str) -> Option<&'static str> {
    match ty_str {
        "int8" | "int16" | "int32" | "int64" | "uint8" | "uint16" | "uint32" | "uint64" => {
            Some("0")
        }
        "float" => Some("0.0f"),
        "double" => Some("0.0"),
        "bool" => Some("false"),
        "FName" => Some("NAME_None"),
        "FVector" => Some("FVector::ZeroVector"),
        "FVector3f" => Some("FVector3f(0.0f, 0.0f, 0.0f)"),
        "FVector2D" => Some("FVector2D::ZeroVector"),
        "FVector2f" => Some("FVector2f(0.0f, 0.0f)"),
        "FVector4" => Some("FVector4(ForceInitToZero)"),
        "FVector4f" => Some("FVector4f(0.0f, 0.0f, 0.0f, 0.0f)"),
        "FRotator" => Some("FRotator::ZeroRotator"),
        "FQuat" | "FQuat4f" => Some("FQuat::Identity"),
        "FTransform" => Some("FTransform::Identity"),
        "FLinearColor" => Some("FLinearColor::White"),
        "FColor" => Some("FColor::White"),
        // Pointer types must always be nullptr
        ty if ty.ends_with('*') => Some("nullptr"),
        // FString, FText, TArray, TMap, TSet — default-construct to empty, no initializer needed.
        _ => None,
    }
}

// ============================================================================
// TESTS
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::Type;
    use kain_core::ast::{TypeAlias, Visibility};
    use kain_core::span::Span;

    /// Helper to create a test TypeAlias for delegate testing
    fn create_delegate_alias(name: &str, param_count: usize) -> TypeAlias {
        let params: Vec<Type> = (0..param_count)
            .map(|i| Type::Named {
                name: if i % 3 == 0 {
                    "Int".to_string()
                } else if i % 3 == 1 {
                    "Float".to_string()
                } else {
                    "Bool".to_string()
                },
                generics: vec![],
                span: Span::default(),
            })
            .collect();

        TypeAlias {
            name: name.to_string(),
            generics: vec![],
            target: Type::Function {
                params,
                return_type: Box::new(Type::Unit(Span::default())),
                effects: vec![],
                span: Span::default(),
            },
            visibility: Visibility::Public,
            span: Span::default(),
        }
    }

    /// Helper to create a Ue5Gen instance for testing
    fn create_test_codegen() -> Ue5Gen {
        Ue5Gen::new(
            "TestModule",
            Some("TestModule"),
            None,
            None,
            std::collections::HashMap::new(),
        )
    }

    #[test]
    fn test_multicast_delegate_zero_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnSimpleEvent", 0);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("DECLARE_MULTICAST_DELEGATE(FOnSimpleEvent);"),
            "Expected DECLARE_MULTICAST_DELEGATE for zero params, got: {}",
            header
        );
    }

    #[test]
    fn test_multicast_delegate_one_param() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnValueChanged", 1);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("DECLARE_MULTICAST_DELEGATE_OneParam(FOnValueChanged, int64);"),
            "Expected DECLARE_MULTICAST_DELEGATE_OneParam, got: {}",
            header
        );
    }

    #[test]
    fn test_multicast_delegate_two_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnPositionUpdate", 2);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header
                .contains("DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPositionUpdate, int64, float);"),
            "Expected DECLARE_MULTICAST_DELEGATE_TwoParams, got: {}",
            header
        );
    }

    #[test]
    fn test_multicast_delegate_three_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnComplexEvent", 3);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains(
                "DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnComplexEvent, int64, float, bool);"
            ),
            "Expected DECLARE_MULTICAST_DELEGATE_ThreeParams, got: {}",
            header
        );
    }

    #[test]
    fn test_multicast_delegate_four_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnFourParamEvent", 4);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_FourParams(FOnFourParamEvent, int64, float, bool, int64);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_FourParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_five_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnFiveParamEvent", 5);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_FiveParams(FOnFiveParamEvent, int64, float, bool, int64, float);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_FiveParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_six_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnSixParamEvent", 6);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_SixParams(FOnSixParamEvent, int64, float, bool, int64, float, bool);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_SixParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_seven_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnSevenParamEvent", 7);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_SevenParams(FOnSevenParamEvent, int64, float, bool, int64, float, bool, int64);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_SevenParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_eight_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnEightParamEvent", 8);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_EightParams(FOnEightParamEvent, int64, float, bool, int64, float, bool, int64, float);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_EightParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_nine_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnNineParamEvent", 9);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(header.contains("DECLARE_MULTICAST_DELEGATE_NineParams(FOnNineParamEvent, int64, float, bool, int64, float, bool, int64, float, bool);"), 
            "Expected DECLARE_MULTICAST_DELEGATE_NineParams, got: {}", header);
    }

    #[test]
    fn test_multicast_delegate_too_many_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnTooManyParams", 10);

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("// Error: Delegate FOnTooManyParams has 10 parameters"),
            "Expected error comment for too many params, got: {}",
            header
        );
        assert!(
            header.contains("// Consider refactoring to use a struct parameter instead"),
            "Expected refactoring suggestion, got: {}",
            header
        );
    }

    #[test]
    fn test_delegate_zero_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("SimpleCallback", 0);

        gen.gen_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("DECLARE_DELEGATE(FSimpleCallback);"),
            "Expected DECLARE_DELEGATE for zero params, got: {}",
            header
        );
    }

    #[test]
    fn test_delegate_one_param() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("ValueCallback", 1);

        gen.gen_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("DECLARE_DELEGATE_OneParam(FValueCallback, int64);"),
            "Expected DECLARE_DELEGATE_OneParam, got: {}",
            header
        );
    }

    #[test]
    fn test_delegate_two_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("TwoParamCallback", 2);

        gen.gen_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("DECLARE_DELEGATE_TwoParams(FTwoParamCallback, int64, float);"),
            "Expected DECLARE_DELEGATE_TwoParams, got: {}",
            header
        );
    }

    #[test]
    fn test_delegate_three_params() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("ThreeParamCallback", 3);

        gen.gen_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header
                .contains("DECLARE_DELEGATE_ThreeParams(FThreeParamCallback, int64, float, bool);"),
            "Expected DECLARE_DELEGATE_ThreeParams, got: {}",
            header
        );
    }

    #[test]
    fn test_delegate_naming_convention() {
        let mut gen = create_test_codegen();

        // Test that delegate names get F prefix
        let alias = create_delegate_alias("MyCustomDelegate", 1);
        gen.gen_delegate(&alias);

        let header = gen.header.build();
        assert!(
            header.contains("FMyCustomDelegate"),
            "Expected F prefix on delegate name, got: {}",
            header
        );
    }

    #[test]
    fn test_multicast_vs_regular_delegate_distinction() {
        let mut gen_multicast = create_test_codegen();
        let mut gen_regular = create_test_codegen();

        let alias = create_delegate_alias("TestEvent", 1);

        gen_multicast.gen_multicast_delegate(&alias);
        gen_regular.gen_delegate(&alias);

        let multicast_header = gen_multicast.header.build();
        let regular_header = gen_regular.header.build();

        assert!(
            multicast_header.contains("DECLARE_MULTICAST_DELEGATE_OneParam"),
            "Multicast should use DECLARE_MULTICAST_DELEGATE_OneParam"
        );
        assert!(
            regular_header.contains("DECLARE_DELEGATE_OneParam"),
            "Regular should use DECLARE_DELEGATE_OneParam"
        );
        assert!(
            !multicast_header.contains("DECLARE_DELEGATE_OneParam("),
            "Multicast should not use regular DECLARE_DELEGATE"
        );
        assert!(
            !regular_header.contains("DECLARE_MULTICAST_DELEGATE_OneParam("),
            "Regular should not use DECLARE_MULTICAST_DELEGATE"
        );
    }

    #[test]
    fn test_delegate_registration() {
        let mut gen = create_test_codegen();
        let alias = create_delegate_alias("OnRegistered", 0);

        gen.gen_multicast_delegate(&alias);

        // Verify delegate was registered in context
        assert!(
            gen.context.delegate_names.contains("OnRegistered"),
            "Delegate should be registered in context"
        );
    }

    #[test]
    fn test_delegate_with_complex_types() {
        let mut gen = create_test_codegen();

        // Create delegate with Vec3 parameter
        let alias = TypeAlias {
            name: "OnPositionChanged".to_string(),
            generics: vec![],
            target: Type::Function {
                params: vec![Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: Span::default(),
                }],
                return_type: Box::new(Type::Unit(Span::default())),
                effects: vec![],
                span: Span::default(),
            },
            visibility: Visibility::Public,
            span: Span::default(),
        };

        gen.gen_multicast_delegate(&alias);

        let header = gen.header.build();
        // Vec3 should map to FVector (double precision by default)
        assert!(
            header.contains("DECLARE_MULTICAST_DELEGATE_OneParam(FOnPositionChanged, FVector);"),
            "Expected Vec3 to map to FVector, got: {}",
            header
        );
    }
}
