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

use kain_core::{TypedProgram, MonomorphizedProgram};
use kain_core::types::{TypedItem, TypedShader};
use kain_core::error::KainResult;
use kain_core::ast::{
    Type, Expr, Stmt, Block, BinaryOp, UnaryOp, Pattern, Function, Struct, Enum,
    Field, Variant, VariantFields, Impl, Param, Actor, MessageHandler,
    ElseBranch, EnumVariantFields, TypeAlias, ShaderStage,
};
use std::collections::HashSet;

// Import the UE5 support library
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
use std::time::{SystemTime, UNIX_EPOCH};
use crate::ue5::{
    to_actor_name, to_struct_name, to_enum_name, to_component_name, to_uobject_name, to_pascal_case,
    TypeMapConfig, map_type as ue5_map_type,
    get_ue_log_format_spec, escape_string as ue5_escape_string,
    PropertyBuilder, FunctionBuilder,
};


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
pub fn generate_from_typed(program: &TypedProgram, output_name: Option<&str>, copyright: Option<&str>) -> KainResult<Ue5Output> {
    let module_name = output_name.unwrap_or("Kain");
    generate_filtered_typed(program, module_name, output_name, None, copyright, std::collections::HashMap::new(), None)
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
pub fn generate(program: &MonomorphizedProgram, output_name: Option<&str>, copyright: Option<&str>) -> KainResult<Ue5Output> {
    let module_name = output_name.unwrap_or("Kain");
    generate_filtered(program, module_name, output_name, None, copyright, std::collections::HashMap::new(), None)
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
    context: &Ue5Context
) -> KainResult<Ue5Output> {
    let module_name = output_name.unwrap_or("Kain");
    let mut gen = Ue5Gen::new(module_name, output_name, copyright, None, std::collections::HashMap::new());
    
    // Use the provided context instead of creating a new one
    gen.context = context.clone();
    
    // PRE-PASS: Register all types
    for item in program.items() {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            },
            kain_core::types::TypedItem::Struct(st) => {
                let prefixed_name = to_struct_name(&st.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_struct(st.ast.name.clone(), header.clone());
                if st.ast.attributes.iter().any(|a| a.name == "component") {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            },
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            },
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            },
            _ => {}
        }
    }
    
    Ok(gen.gen_program(program))
}

/// Legacy version for TypedProgram (packager compatibility)
pub fn generate_with_context_typed(
    program: &TypedProgram, 
    output_name: Option<&str>, 
    copyright: Option<&str>,
    context: &Ue5Context
) -> KainResult<Ue5Output> {
    let module_name = output_name.unwrap_or("Kain");
    let mut gen = Ue5Gen::new(module_name, output_name, copyright, None, std::collections::HashMap::new());
    
    // Use the provided context instead of creating a new one
    gen.context = context.clone();
    
    // PRE-PASS: Register all types
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            },
            kain_core::types::TypedItem::Struct(st) => {
                let prefixed_name = to_struct_name(&st.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_struct(st.ast.name.clone(), header.clone());
                if st.ast.attributes.iter().any(|a| a.name == "component") {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            },
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            },
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            },
            kain_core::types::TypedItem::TypeAlias(a) => {
                // Delegates use F prefix like structs
                let prefixed_name = to_struct_name(&a.ast.name);
                let header = format!("{}.h", prefixed_name);
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            },
            _ => {}
        }
    }
    
    Ok(gen.gen_program(program))
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
    module_name: &str,  // Plugin name for API macro (e.g., "UltimateTest")
    output_name: Option<&str>,  // Per-item name for file naming (e.g., "AMaterialTestActor")
    target_item: Option<String>, 
    copyright: Option<&str>,
    type_to_header: std::collections::HashMap<String, String>,
    shader_file_names: Option<Vec<String>>,  // Shader file names from toml (without underscores)
) -> KainResult<Ue5Output> {
    let mut gen = Ue5Gen::new(module_name, output_name, copyright, target_item, type_to_header.clone());
    gen.shader_file_names = shader_file_names.unwrap_or_default();
    
    // PRE-PASS: Register all types so type lookups work during codegen
    // This is CRITICAL for modular output so each file knows about other types (e.g. if it's a delegate)
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header.get(&en.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            },
            kain_core::types::TypedItem::Struct(st) => {
                let prefixed_name = to_struct_name(&st.ast.name);
                let header = type_to_header.get(&st.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_struct(st.ast.name.clone(), header.clone());
                if st.ast.attributes.iter().any(|a| a.name == "component") {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            },
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header.get(&a.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            },
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = type_to_header.get(&c.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            },
            kain_core::types::TypedItem::TypeAlias(a) => {
                let prefixed_name = to_struct_name(&a.ast.name);
                let header = type_to_header.get(&a.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            },
            // Traits are filtered out during type checking - should not appear here
            kain_core::types::TypedItem::Impl(impl_block) => {
                // Register trait implementations for interface inheritance
                if let Some(ref trait_name) = impl_block.ast.trait_name {
                    if let kain_core::ast::Type::Named { name: class_name, .. } = &impl_block.ast.target_type {
                        gen.context.register_trait_impl(class_name, trait_name);
                    }
                }
            },
            _ => {}
        }
    }
    
    Ok(gen.gen_program(program))
}

/// Legacy function for compatibility with TypedProgram (packager)
/// 
/// # Note
/// This is for the packager which still works with TypedProgram.
/// The packager converts MonomorphizedProgram back to TypedProgram after monomorphization.
pub fn generate_filtered_typed(
    program: &TypedProgram, 
    module_name: &str,
    output_name: Option<&str>,
    target_item: Option<String>, 
    copyright: Option<&str>,
    type_to_header: std::collections::HashMap<String, String>,
    shader_file_names: Option<Vec<String>>,
) -> KainResult<Ue5Output> {
    let mut gen = Ue5Gen::new(module_name, output_name, copyright, target_item, type_to_header.clone());
    gen.shader_file_names = shader_file_names.unwrap_or_default();
    
    // PRE-PASS: Register all types
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header.get(&en.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            },
            kain_core::types::TypedItem::Struct(st) => {
                let prefixed_name = to_struct_name(&st.ast.name);
                let header = type_to_header.get(&st.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_struct(st.ast.name.clone(), header.clone());
                if st.ast.attributes.iter().any(|a| a.name == "component") {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            },
            kain_core::types::TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header.get(&a.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            },
            kain_core::types::TypedItem::Component(c) => {
                let prefixed_name = to_component_name(&c.ast.name);
                let header = type_to_header.get(&c.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_component(c.ast.name.clone(), header);
                gen.type_mapper.register_component(c.ast.name.clone());
            },
            kain_core::types::TypedItem::TypeAlias(a) => {
                let prefixed_name = to_struct_name(&a.ast.name);  // Delegates use F prefix
                let header = type_to_header.get(&a.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_delegate(a.ast.name.clone(), header);
                gen.type_mapper.register_delegate(a.ast.name.clone());
            },
            _ => {}
        }
    }
    
    // Convert TypedProgram to MonomorphizedProgram for gen_program
    Ok(gen.gen_program(&kain_core::monomorphize::MonomorphizedProgram {
        items: program.items.clone(),
    }))
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
    
    let output = generate_filtered(&mono_program, "Kain", None, None, copyright, std::collections::HashMap::new(), None)?;
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
) -> KainResult<Ue5Output> {
    use kain_core::types::TypedItem;
    
    let mut gen = Ue5Gen::new(module_name, Some("KainStdlib"), copyright, None, type_to_header.clone());
    
    // PRE-PASS: Register all types so type lookups work during codegen
    // Use the type_to_header map that was passed in (which has correct prefixed names)
    for item in &program.items {
        match item {
            TypedItem::Enum(en) => {
                let prefixed_name = to_enum_name(&en.ast.name);
                let header = type_to_header.get(&en.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_enum(en.ast.name.clone(), header);
                gen.type_mapper.register_enum(en.ast.name.clone());
            },
            TypedItem::Struct(st) => {
                let prefixed_name = to_struct_name(&st.ast.name);
                let header = type_to_header.get(&st.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_struct(st.ast.name.clone(), header.clone());
                if st.ast.attributes.iter().any(|a| a.name == "component") {
                    gen.context.register_component(st.ast.name.clone(), header);
                    gen.type_mapper.register_component(st.ast.name.clone());
                } else {
                    gen.type_mapper.register_struct(st.ast.name.clone());
                }
            },
            TypedItem::Actor(a) => {
                let prefixed_name = to_actor_name(&a.ast.name);
                let header = type_to_header.get(&a.ast.name).cloned().unwrap_or(format!("{}.h", prefixed_name));
                gen.context.register_actor(a.ast.name.clone(), header);
                gen.type_mapper.register_actor(a.ast.name.clone());
            },
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
    
    // Include all type headers so stdlib functions can reference them
    let mut type_headers: Vec<String> = gen.context.type_to_header.values().cloned().collect();
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
            let is_blueprint = f.ast.attributes.iter().any(|a| a.name == "blueprint");
            if is_blueprint {
                continue;
            }
            
            // ONLY generate functions that have a body (ignore intrinsics/externs)
            if !f.ast.body.stmts.is_empty() {
                // Clone the function and strip blueprint/ue5 attributes so it generates as a free function
                let mut free_func = f.ast.clone();
                free_func.attributes.retain(|a| a.name != "blueprint" && a.name != "ue5");
                
                gen.gen_ufunction(&free_func);
                func_count += 1;
            }
        }
    }
    
    if func_count > 0 {
        eprintln!("   📦 Generated {} stdlib functions → KainStdlib.h", func_count);
    }
    
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
}

impl Ue5Gen {
    fn new(module_name: &str, output_name: Option<&str>, copyright: Option<&str>, target_item: Option<String>, type_to_header: std::collections::HashMap<String, String>) -> Self {
        // module_name = plugin name for API macro (e.g., "UltimateTest" → "ULTIMATETEST_API")
        // output_name = per-item name for file naming (e.g., "AMaterialTestActor")
        let name = output_name.unwrap_or(module_name);
        let mut context = Ue5Context::new(module_name, copyright);
        context.output_name = name.to_string();  // Set output name for file naming
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
            fmt.push_str(&current[..start]);
            current = &current[start + 1..];
            if let Some(end) = current.find('}') {
                let ident = &current[..end];
                fmt.push_str("%s");
                args.push(format!("*LexToString({})", self.remap_ident(ident)));
                current = &current[end + 1..];
            } else {
                fmt.push('{');
            }
        }
        fmt.push_str(current);
        (fmt, args)
    }

    fn remap_ident(&self, name: &str) -> String {
        self.context.remap_ident(name)
    }

    /// Check if an expression refers to a pointer type (component, actor, UObject-derived)
    /// Used to determine whether to use `->`(pointer) or `.`(value) for member access.
    /// In UE5, components (U*Component*), actors (A*), and UObject-derived types
    /// (UMaterialInstanceDynamic*, UTexture2D*, etc.) are always heap-allocated pointers.
    fn is_pointer_receiver(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Ident(name, _) => {
                self.is_pointer_type_by_name(name)
            }
            Expr::Field { object, field, .. } => {
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

    fn gen_program<P: ProgramItems>(&mut self, program: &P) -> Ue5Output {
        // Check if we need replication support
        let needs_replication = program.items().iter().any(|item| {
            match item {
                TypedItem::Actor(a) => a.ast.state.iter().any(|s| {
                    s.attributes.iter().any(|a| a.name == "replicated")
                }),
                TypedItem::Struct(s) => s.ast.fields.iter().any(|f| {
                    f.attributes.iter().any(|a| a.name == "replicated")
                }),
                _ => false
            }
        });
        
        // Track replication feature
        if needs_replication {
            self.context.use_feature("Replication");
        }
        
        // Collect all shaders for actor integration
        let shaders: Vec<&TypedShader> = program.items().iter()
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
        
        // Determine what kind of item we're generating (for smart includes)
        let target_item_kind = if let Some(target) = &self.target_item {
            program.items().iter().find_map(|item| {
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
                        },
                        TypedItem::Enum(_) => "enum",
                        TypedItem::Component(_) => "component",
                        TypedItem::TypeAlias(_) => "delegate",
                        _ => "unknown",
                    })
                } else {
                    None
                }
            }).unwrap_or("unknown")
        } else {
            "all" // No target = generating everything
        };
        
        // Initialize includes based on item type (CoreMinimal.h is already in the template)
        let mut includes: Vec<&str> = Vec::new();
        match target_item_kind {
            "actor" => {
                includes.push("GameFramework/Actor.h");
                if needs_replication {
                    includes.push("Net/UnrealNetwork.h");
                }
            },
            "component" => {
                includes.push("Components/ActorComponent.h");
                if needs_replication {
                    includes.push("Net/UnrealNetwork.h");
                }
            },
            "datatable" => {
                includes.push("Engine/DataTable.h");
            },
            "struct" | "enum" | "delegate" => {
                // Minimal includes — CoreMinimal.h from template is sufficient
            },
            _ => {
                // Full includes for combined/unknown output
                includes.push("GameFramework/Actor.h");
                includes.push("Components/ActorComponent.h");
                includes.push("Kismet/BlueprintFunctionLibrary.h");
                if needs_replication {
                    includes.push("Net/UnrealNetwork.h");
                }
            },
        }

        // DISCOVERY PASS: If we are targeting a specific item, scan it for dependencies first
        if self.target_item.is_some() {
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
        for header in self.context.get_needed_headers() {
            if !includes.contains(&header.as_str()) {
                includes.push(Box::leak(header.into_boxed_str())); // Keep it simple for now
            }
        }

        // Bug-4 fix: if we're in blueprint-library-only mode, only include headers for
        // types that are actually referenced in @blueprint function signatures.
        // This replaces the old hardcoded skip list (plugin-specific names like
        // "FCosmosDashboard", "FSovereignDetailsPanel", etc.).
        let blueprint_library_only = self.target_item.as_ref()
            .map(|t| t == "__BLUEPRINT_LIBRARY_ONLY__").unwrap_or(false);
        if blueprint_library_only {
            // type_to_header keys are raw KAIN type names; collect only those used.
            let used = &self.blueprint_used_types;
            for (type_name, type_header) in &self.context.type_to_header {
                if used.is_empty() || used.contains(type_name.as_str()) {
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
        self.source.push_line("// Generated by KAIN Compiler - UE5 C++ Codegen");
        self.source.push_line("// Do not edit - regenerate from .kn source");
        self.write_blank_source();
        self.source.push_line(&format!("#include \"{}.h\"", self.context.output_name));
        if !shaders.is_empty() {
            self.source.push_line("#include \"RenderGraph.h\"");
            self.source.push_line("#include \"RenderGraphBuilder.h\"");
            self.source.push_line("#include \"RenderGraphResources.h\"");
            self.source.push_line("#include \"RenderGraphUtils.h\"");
            self.source.push_line("#include \"RenderTargetPool.h\"");
            self.source.push_line("#include \"Engine/TextureRenderTarget2D.h\"");
            // Use shader file names (from toml or auto-detected) instead of AST names
            // This ensures correct casing (e.g., K12SovereignPBR.h not K12SovereignPbr.h)
            for shader_file_name in &self.shader_file_names {
                eprintln!("   📄 [CODEGEN] Including shader header: {}.h", shader_file_name);
                let shader_header = format!("{}.h", shader_file_name);
                self.source.push_line(&format!("#include \"{}\"", shader_header));
            }
        }
        self.write_blank_source();

        // PRE-PASS: Collect all enum, struct, and component names BEFORE generating any code
        // This ensures that delegate parameter types can be correctly resolved
        let default_header = format!("{}.h", self.context.output_name);
        for item in program.items() {
            let item_header = match item {
                TypedItem::Enum(en) => self.context.type_to_header.get(&en.ast.name).cloned().unwrap_or(default_header.clone()),
                TypedItem::Struct(st) => self.context.type_to_header.get(&st.ast.name).cloned().unwrap_or(default_header.clone()),
                _ => default_header.clone(),
            };

            match item {
                TypedItem::Enum(en) => {
                    self.context.register_enum(en.ast.name.clone(), item_header);
                },
                TypedItem::Struct(st) => {
                    self.context.register_struct(st.ast.name.clone(), item_header.clone());
                    if st.ast.attributes.iter().any(|a| a.name == "component") {
                        self.context.register_component(st.ast.name.clone(), item_header);
                    }
                },
                TypedItem::Component(c) => {
                    let h = self.context.type_to_header.get(&c.ast.name).cloned().unwrap_or(default_header.clone());
                    self.context.register_component(c.ast.name.clone(), h);
                },
                TypedItem::Actor(a) => {
                    let h = self.context.type_to_header.get(&a.ast.name).cloned().unwrap_or(default_header.clone());
                    self.context.register_actor(a.ast.name.clone(), h);
                },
                _ => {}
            }
        }

        // Pre-compute POD mirrors so the actor dispatch code can reference them
        // without needing the full TypedProgram.
        // Convert MonomorphizedProgram to TypedProgram for shader codegen
        let typed_program = kain_core::types::TypedProgram {
            items: program.items().to_vec(),
        };
        self.component_mirrors = match ue5_shaders::pod_mirror::collect_component_mirrors(&typed_program) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("[KAIN ue5 codegen] POD mirror error: {}", e);
                std::collections::HashMap::new()
            }
        };

        // Build a map from every named type to its field/state list for depth-1
        // uniform path resolution (e.g. HyperFluidSimulationCore → [(physics, ...), ...]).
        for item in program.items() {
            match item {
                TypedItem::Struct(st) => {
                    let fields: Vec<(String, Type)> = st.ast.fields
                        .iter()
                        .map(|f| (f.name.clone(), f.ty.clone()))
                        .collect();
                    self.type_fields_map.insert(st.ast.name.clone(), fields);
                }
                TypedItem::Actor(a) => {
                    let fields: Vec<(String, Type)> = a.ast.state
                        .iter()
                        .map(|s| (s.name.clone(), s.ty.clone()))
                        .collect();
                    self.type_fields_map.insert(a.ast.name.clone(), fields);
                }
                TypedItem::Function(f) => {
                    if f.ast.attributes.iter().any(|a| a.name == "blueprint") {
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
        
        // Separate items by type for proper ordering
        let mut delegates = Vec::new();
        let mut blueprint_funcs = Vec::new();
        let mut other_items = Vec::new();
        
        // Check if we're in blueprint-library-only mode
        let blueprint_library_only = self.target_item.as_ref().map(|t| t == "__BLUEPRINT_LIBRARY_ONLY__").unwrap_or(false);
        
        for item in program.items() {
            let item_name = match item {
                TypedItem::Actor(a) => &a.ast.name,
                TypedItem::Struct(s) => &s.ast.name,
                TypedItem::Enum(e) => &e.ast.name,
                TypedItem::Function(f) => &f.ast.name,
                TypedItem::Component(c) => &c.ast.name,
                TypedItem::TypeAlias(a) => &a.ast.name,
                _ => "",
            };

            if let Some(target) = &self.target_item {
                // If we're in blueprint-library-only mode, only collect blueprint functions
                if blueprint_library_only {
                    if let TypedItem::Function(f) = item {
                        if f.ast.attributes.iter().any(|a| a.name == "blueprint" || a.name == "ue5") {
                            blueprint_funcs.push(item);
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
                        delegates.push(item);
                    } else {
                        other_items.push(item);
                    }
                },
                TypedItem::Function(f) => {
                    if f.ast.attributes.iter().any(|a| a.name == "blueprint" || a.name == "ue5") {
                        blueprint_funcs.push(item);
                    } else {
                        other_items.push(item);
                    }
                },
                _ => other_items.push(item),
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
        if !blueprint_library_only {
            for item in &other_items {
                match item {
                    TypedItem::Actor(actor_typed) => {
                        // Only pass COMPUTE shaders to actor codegen for RDG dispatch.
                        // Fragment/vertex shaders are used via materials, not direct dispatch.
                        let compute_shaders: Vec<&TypedShader> = shaders.iter()
                            .filter(|s| s.ast.stage == kain_core::ast::ShaderStage::Compute)
                            .copied()
                            .collect();
                        let compute_shader_names: Vec<String> = compute_shaders.iter()
                            .map(|s| s.ast.name.clone())
                            .collect();
                        self.gen_actor_with_shaders(&actor_typed.ast, &compute_shaders, &compute_shader_names)
                    },
                    TypedItem::Struct(st) => {
                        let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
                        if is_component {
                            self.gen_ucomponent(&st.ast);
                        } else {
                            self.gen_ustruct(&st.ast);
                        }
                    },
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
            let lib_class = format!("U{}FunctionLibrary", self.context.output_name);
            self.write_header(&format!("class {} {} : public UBlueprintFunctionLibrary", self.context.module_api, lib_class));
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
            eprintln!("   📦 [CODEGEN] Generating IMPLEMENT_MODULE (monolithic mode)");
            self.write_blank_source();
            self.write_source("// Module implementation");
            self.write_source("// Shader directory registration is handled by static initializers in shader .cpp files");
            self.write_blank_source();
            self.write_source(&format!("class F{}Module : public IModuleInterface", self.context.output_name));
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
            self.write_source(&format!("IMPLEMENT_MODULE(F{}Module, {})", self.context.output_name, self.context.output_name));
        }
        
        // Return separate files including shaders
        let mut shader_files = Vec::new();
        
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
                    eprintln!("Warning: Failed to generate USF for {}: {}", shader_name, e);
                }
            }
            
            // Generate C++ header for shader
            let header_code = ue5_shaders::codegen_usf::generate_cpp_header(&typed_program, shader_name);
            shader_files.push((format!("{}.h", shader_name), header_code));
            
            // Generate C++ implementation for shader
            let cpp_code = ue5_shaders::codegen_usf::generate_cpp_implementation(&typed_program, shader_name, "YourPlugin");
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
            },
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
            },
            _ => {} // Skip shaders and traits
        }
    }

    /// Generate UE5 AActor subclass from KAIN actor
    /// If shaders exist in the program, auto-wires shader dispatch to Tick()
    fn gen_actor(&mut self, actor: &Actor) {
        self.gen_actor_with_shaders(actor, &[], &[]);
    }
    
    fn gen_actor_with_shaders(&mut self, actor: &Actor, shaders: &[&TypedShader], shader_file_names: &[String]) {
        let class_name = to_actor_name(&actor.name);
        
        eprintln!("🎯 [CODEGEN] Generating actor: {}", class_name);
        eprintln!("   📊 Shaders: {} (file names: {})", shaders.len(), shader_file_names.len());
        
        // --- 1. Header Generation ---
        
        // Get interface inheritance list
        let interface_list = self.context.get_interface_list(&actor.name);
        
        self.header.push_line(&format!("UCLASS()"));
        self.write_header(&format!("class {} {} : public AActor{}", self.context.module_api, class_name, interface_list));
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
        let has_replicated_state = actor.state.iter().any(|s| {
            s.attributes.iter().any(|a| a.name == "replicated")
        });

        for state_decl in &actor.state {
            let mut props: Vec<&str> = vec!["EditAnywhere", "BlueprintReadWrite"];
            let mut category = "Simulation Settings".to_string();
            for attr in &state_decl.attributes {
                match attr.name.as_str() {
                    "replicated" => props.push("Replicated"),
                    "savegame"   => props.push("SaveGame"),
                    "transient"  => props.push("Transient"),
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
            let cpp_type = self.map_type(&state_decl.ty);
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
            if handler.message_type != "begin_play" && handler.message_type != "BeginPlay" &&
               handler.message_type != "tick" && handler.message_type != "Tick" {
                self.gen_message_handler_decl(handler);
            }
        }

        // Actor Methods Declarations
        for method in &actor.methods {
            self.gen_actor_method_decl(method);
        }

        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();
        
        // --- 2. Source Implementation ---
        
        // Constructor
        self.source.push_line(&format!("{}::{}()", class_name, class_name));
        self.source.push_line("{");
        self.source.push_line("\tPrimaryActorTick.bCanEverTick = true;");
        
        // Initialize component state fields with CreateDefaultSubobject
        for state_decl in &actor.state {
            let cpp_type = self.map_type(&state_decl.ty);
            
            // Check if this is a component type (ends with "Component*" or is a UActorComponent subclass)
            if cpp_type.contains("Component*") {
                // Extract the component class name (e.g., "USovereignComponent*" -> "USovereignComponent")
                let component_class = cpp_type.trim_end_matches('*').trim();
                
                // Generate CreateDefaultSubobject call
                // Format: sovereignty = CreateDefaultSubobject<USovereignComponent>(TEXT("SovereignComponent"));
                let component_name = &state_decl.name;
                let text_name = to_pascal_case(component_name); // Convert snake_case to PascalCase for TEXT() name
                
                self.source.push_line(&format!(
                    "\t{} = CreateDefaultSubobject<{}>(TEXT(\"{}\"));",
                    component_name,
                    component_class,
                    text_name
                ));
            }
        }
        
        self.source.push_line("}");
        self.write_blank_source();

        // GetLifetimeReplicatedProps implementation for @replicated state fields
        if has_replicated_state {
            self.source.push_line(&format!("void {}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const", class_name));
            self.source.push_line("{");
            self.source.push_line("\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);");
            self.source.push_line("");
            for s in &actor.state {
                if s.attributes.iter().any(|a| a.name == "replicated") {
                    self.source.push_line(&format!("\tDOREPLIFETIME({}, {});", class_name, s.name));
                }
            }
            self.source.push_line("}");
            self.write_blank_source();
        }

        // BeginPlay
        self.source.push_line(&format!("void {}::BeginPlay()", class_name));
        self.source.push_line("{");
        self.source.push_line("\tSuper::BeginPlay();");
        
        if !shaders.is_empty() {
            // Bug-14 fix: Transient UTextureRenderTarget2D* members are null by default.
            // Without explicit init in BeginPlay(), the Bug-13 null guard fires every frame
            // and the simulation never runs.
            for rt_name in &["PositionRT_A", "PositionRT_B", "VelocityRT_A", "VelocityRT_B"] {
                self.source.push_line(&format!("\tif (!{})", rt_name));
                self.source.push_line("\t{");
                self.source.push_line(&format!("\t\t{} = NewObject<UTextureRenderTarget2D>(this);", rt_name));
                self.source.push_line(&format!("\t\t{}->bAutoGenerateMips = false;", rt_name));
                self.source.push_line(&format!("\t\t{}->RenderTargetFormat = RTF_RGBA32f;", rt_name));
                self.source.push_line(&format!("\t\t{}->InitAutoFormat(512, 512);", rt_name));
                self.source.push_line(&format!("\t\t{}->UpdateResourceImmediate(true);", rt_name));
                self.source.push_line("\t}");
            }
        }
        
        // User defined begin_play logic
        if let Some(handler) = actor.handlers.iter().find(|h| h.message_type == "begin_play" || h.message_type == "BeginPlay") {
            self.gen_block_source(&handler.body);
        }
        
        self.source.push_line("}");
        self.write_blank_source();
        
        // Tick
        self.source.push_line(&format!("void {}::Tick(float DeltaTime)", class_name));
        self.source.push_line("{");
        self.source.push_line("\tSuper::Tick(DeltaTime);");
        
        // User defined tick logic
        if let Some(handler) = actor.handlers.iter().find(|h| h.message_type == "tick" || h.message_type == "Tick") {
            for param in &handler.params {
                if param.name == "delta_time" || param.name == "dt" || param.name == "delta" {
                    self.context.add_ident_remap(param.name.clone(), "DeltaTime".to_string());
                }
            }
            self.gen_block_source(&handler.body);
            self.context.clear_ident_remaps();
        }

        if !shaders.is_empty() {
            self.source.push_line("");
            self.source.push_line("\t// Enqueue Simulation on Render Thread");
            self.source.push_line("\tENQUEUE_RENDER_COMMAND(SimulationTick)(");
            self.source.push_line("\t\t[this, DeltaTime](FRHICommandListImmediate& RHICmdList) {");
            // Bug-13 fix: null checks MUST come before FRDGBuilder construction.
            // Creating the builder and then returning early without calling Execute()
            // triggers ensure(bHasExecuted) in RenderGraphValidation.cpp.
            self.source.push_line("\t\t\tif (!PositionRT_A || !PositionRT_B || !VelocityRT_A || !VelocityRT_B) { return; }");
            self.source.push_line("");
            self.source.push_line("\t\t\tFRDGBuilder GraphBuilder(RHICmdList);");
            self.source.push_line("");
            
            // Resource Registration Logic
            self.source.push_line("\t\t\t// 1. Register External Textures (Ping-Pong Logic)");
            self.source.push_line("\t\t\tbool bOddFrame = GFrameNumberRenderThread % 2 != 0;");

            // Static helper for RT to RDG conversion
            self.source.push_line("\t\t\tauto CreateRenderTarget = [&](FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* RT, const TCHAR* Name) -> TRefCountPtr<IPooledRenderTarget> {");
            self.source.push_line("\t\t\t\tif (!RT || !RT->GetResource()) return nullptr;");
            self.source.push_line("\t\t\t\tFTexture2DRHIRef TextureRHI = RT->GetResource()->GetTexture2DRHI();");
            self.source.push_line("\t\t\t\tFPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(");
            self.source.push_line("\t\t\t\t\tFIntPoint(RT->SizeX, RT->SizeY),");
            self.source.push_line("\t\t\t\t\tTextureRHI->GetFormat(),");
            self.source.push_line("\t\t\t\t\tFClearValueBinding::None,");
            self.source.push_line("\t\t\t\t\tTexCreate_None,");
            self.source.push_line("\t\t\t\t\tTexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable,");
            self.source.push_line("\t\t\t\t\tfalse");
            self.source.push_line("\t\t\t\t);");
            self.source.push_line("\t\t\t\tTRefCountPtr<IPooledRenderTarget> PooledRT;");
            self.source.push_line("\t\t\t\tFSceneRenderTargetItem Item;");
            self.source.push_line("\t\t\t\tItem.TargetableTexture = (FTextureRHIRef)TextureRHI;");
            self.source.push_line("\t\t\t\tItem.ShaderResourceTexture = (FTextureRHIRef)TextureRHI;");
            self.source.push_line("\t\t\t\tGRenderTargetPool.CreateUntrackedElement(Desc, PooledRT, Item);");
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
                        if name == "Sampler2D" || name == "Texture2D" {
                            // This shader needs an input texture - mark it as needed
                            needed_intermediates.insert(uniform.name.clone());
                        }
                    }
                }
            }
            
            // Create intermediate render targets for shader pipeline chaining
            if !needed_intermediates.is_empty() {
                self.source.push_line("\t\t\t// Create intermediate render targets for shader pipeline");
                self.source.push_line("\t\t\tFRDGTextureDesc IntermediateDesc = FRDGTextureDesc::Create2D(");
                self.source.push_line("\t\t\t\tFIntPoint(1024, 1024),");
                self.source.push_line("\t\t\t\tPF_FloatRGBA,");
                self.source.push_line("\t\t\t\tFClearValueBinding::Black,");
                self.source.push_line("\t\t\t\tTexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable");
                self.source.push_line("\t\t\t);");
                self.source.push_line("");
                
                // Create intermediate RTs for common shader outputs
                for intermediate_name in &needed_intermediates {
                    let rt_name = format!("{}RT", intermediate_name.chars().next().unwrap().to_uppercase().to_string() + &intermediate_name[1..]);
                    self.source.push_line(&format!("\t\t\tFRDGTextureRef {} = GraphBuilder.CreateTexture(IntermediateDesc, TEXT(\"{}\"));", rt_name, intermediate_name));
                }
                self.source.push_line("");
            }

            // Dispatch Calls
            // Zip shaders with their file names to ensure correct casing in function calls
            for (shader, shader_file_name) in shaders.iter().zip(shader_file_names.iter()) {
                let shader_name = &shader.ast.name;
                
                eprintln!("   🔧 [CODEGEN] Generating AddPass call for: {} (file: {})", shader_name, shader_file_name);
                
                // Classify uniforms to split Scalars vs Textures params
                let mut texture_args = Vec::new();
                let mut scalar_args = Vec::new();
                // POD population lines emitted just before the AddPass_ call.
                let mut pod_prep_lines: Vec<String> = Vec::new();
                
                for uniform in &shader.ast.uniforms {
                    let name_lower = uniform.name.to_lowercase();
                    
                    // Check if this is a Sampler2D type (texture uniform)
                    let is_sampler = if let Type::Named { name, .. } = &uniform.ty {
                        name == "Sampler2D" || name == "Texture2D" || name == "RWTexture2D"
                    } else {
                        false
                    };
                    
                    let is_texture = is_sampler || name_lower.contains("texture") || name_lower.contains("sampler") || name_lower.contains("output") || name_lower.contains("tex");
                    
                    if is_texture {
                         // Texture Param Logic - Modular mapping by name
                         let mut matched_texture = false;
                         if name_lower.contains("position") {
                             if name_lower.contains("output") || name_lower.contains("write") {
                                texture_args.push("PositionOutput".to_string());
                             } else {
                                texture_args.push("PositionInput".to_string());
                             }
                             matched_texture = true;
                         } else if name_lower.contains("velocity") {
                             if name_lower.contains("output") || name_lower.contains("write") {
                                texture_args.push("VelocityOutput".to_string());
                             } else {
                                texture_args.push("VelocityInput".to_string());
                             }
                             matched_texture = true;
                         } else {
                             // For fragment shaders with Sampler2D uniforms, try to match to intermediate RTs
                             // Pattern: thermal -> ThermalRT, moisture -> MoistureRT, albedo -> AlbedoRT, etc.
                             let rt_name = format!("{}RT", uniform.name.chars().next().unwrap().to_uppercase().to_string() + &uniform.name[1..]);
                             
                             // Check if this intermediate RT was created
                             if needed_intermediates.contains(&uniform.name) {
                                 eprintln!("   ✅ [CODEGEN] Mapped texture uniform '{}' to intermediate RT '{}'", uniform.name, &rt_name);
                                 texture_args.push(rt_name);
                                 matched_texture = true;
                             }
                         }
                         
                         if !matched_texture {
                             // For unmatched texture uniforms, use PositionOutput as a generic fallback
                             // This allows fragment shaders to compile even without explicit RT mappings
                             eprintln!("   ⚠️  [CODEGEN] Texture uniform '{}' has no matching RT, using PositionOutput as fallback", uniform.name);
                             texture_args.push("PositionOutput".to_string());
                         }
                    } else {
                        // Check first if this uniform is a @component type that needs a POD mirror.
                        let component_type_name = if let Type::Named { name, generics, .. } = &uniform.ty {
                            if generics.is_empty() && self.component_mirrors.contains_key(name.as_str()) {
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
                            let mut state_var = actor.state.iter()
                                .find(|s| s.name.eq_ignore_ascii_case(&uniform.name))
                                .map(|s| format!("this->{}", s.name));

                            // Level 2: depth-1 path — walk each actor state field whose type
                            // has a sub-field matching the uniform name.
                            // e.g. HyperFluidController.world (HyperFluidSimulationCore)
                            //        └── world.physics  (PhysicalPropertiesComponent) ✓
                            if state_var.is_none() {
                                 'outer: for st in &actor.state {
                                    if let Type::Named { name: type_name, .. } = &st.ty {
                                        if let Some(sub_fields) = self.type_fields_map.get(type_name.as_str()) {
                                            for (field_name, _) in sub_fields {
                                                if field_name.eq_ignore_ascii_case(&uniform.name) {
                                                    state_var = Some(format!("this->{}->{}" , st.name, uniform.name));
                                                    break 'outer;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            let state_var = state_var.unwrap_or_else(|| "nullptr".to_string());
                            pod_prep_lines.push(
                                mirror.generate_population_code(&state_var, &pod_var, "\t\t\t")
                            );
                            scalar_args.push(pod_var);
                        } else {
                        // Scalar Param - Modular exact name matching with common aliases
                        let mut found_match = false;
                        for state in &actor.state {
                             let is_match = match uniform.name.to_lowercase().as_str() {
                                 "dt" => state.name.to_lowercase().contains("time_step") || state.name.eq_ignore_ascii_case("dt"),
                                 "res" => state.name.to_lowercase().contains("resolution") || state.name.eq_ignore_ascii_case("res"),
                                 "source_pos" | "pos" => state.name.to_lowercase().contains("position") || state.name.eq_ignore_ascii_case("pos"),
                                 "source_val" | "vel" => (state.name.to_lowercase().contains("velocity") || state.name.eq_ignore_ascii_case("vel") || state.name.to_lowercase().contains("value")) 
                                                         && !state.name.to_lowercase().contains("damping"),
                                 _ => state.name.eq_ignore_ascii_case(&uniform.name)
                             };

                             if is_match {
                                 // Cast to the right type (usually FVector -> FVector3f)
                                 let cast = match self.map_type(&state.ty).as_str() {
                                     "FVector" => "FVector3f",
                                     "FVector2D" => "FVector2f",
                                     "FVector4" => "FVector4f",
                                     _ => ""
                                 };
                                 // Handle Enum casting
                                 let is_enum = if let Type::Named { name, .. } = &state.ty {
                                     self.context.enum_names.contains(name)
                                 } else {
                                     false
                                 };

                                 if !cast.is_empty() {
                                     scalar_args.push(format!("{}(this->{})", cast, state.name));
                                 } else if is_enum {
                                     scalar_args.push(format!("static_cast<int32>(this->{})", state.name));
                                 } else {
                                     scalar_args.push(format!("this->{}", state.name));
                                 }
                                 found_match = true;
                                 break;
                             }
                        }
                        if !found_match {
                             // Fallback with correct type initialization
                             let fallback = match self.map_type(&uniform.ty).as_str() {
                                 "FVector" | "FVector3f" => "FVector3f(0.0f, 0.0f, 0.0f)".to_string(),
                                 "FVector2D" | "FVector2f" => "FVector2f(0.0f, 0.0f)".to_string(),
                                 "FVector4" | "FVector4f" => "FVector4f(0.0f, 0.0f, 0.0f, 0.0f)".to_string(),
                                 "FIntVector" => "FIntVector(0, 0, 0)".to_string(),
                                 _ => "0.0f".to_string()
                             };
                             scalar_args.push(fallback);
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
                    let already_has_output = texture_args.iter()
                        .any(|a| a.contains("Output") || a.ends_with("RT"));

                    if !already_has_output {
                        let shader_name_lower = shader_name.to_lowercase();
                        if shader_name_lower.contains("position") {
                            texture_args.push("PositionOutput".to_string());
                        } else if shader_name_lower.contains("velocity") {
                            texture_args.push("VelocityOutput".to_string());
                        } else if shader_name_lower.contains("thermal") {
                            texture_args.push("ThermalRT".to_string());
                        } else if shader_name_lower.contains("moisture") {
                            texture_args.push("MoistureRT".to_string());
                        } else if shader_name_lower.contains("albedo") {
                            texture_args.push("AlbedoRT".to_string());
                        } else if shader_name_lower.contains("lights") || shader_name_lower.contains("city") {
                            texture_args.push("LightsRT".to_string());
                        } else {
                            texture_args.push("PositionOutput".to_string());
                        }
                    }
                    
                    // GroupCount is always the final arg for compute shaders
                    texture_args.push("FIntVector(32, 32, 1)".to_string());
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
                self.source.push_line(&format!("\t\t\tAddPass_{}(GraphBuilder, {});", shader_file_name, [scalar_args, texture_args].concat().join(", ")));

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
            if handler.message_type != "begin_play" && handler.message_type != "BeginPlay" &&
               handler.message_type != "tick" && handler.message_type != "Tick" {
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
        let is_rpc = msg_lower.starts_with("server_") || 
                     msg_lower.starts_with("client_") || 
                     msg_lower.starts_with("multicast_");
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
            
            self.write_header(&format!("UFUNCTION({}, BlueprintCallable, Category = \"{}\")", rpc_type, handler.message_type));
        } else {
            self.write_header(&format!("UFUNCTION(BlueprintCallable, Category = \"{}\")", handler.message_type));
        }
        
        self.write_header(&format!("{} {}({});", ret_type, handler.message_type, params));
        self.write_blank_header();
        
        // Server RPCs need _Validate declaration too
        if is_server_rpc {
            self.write_header(&format!("bool {}_Validate({});", handler.message_type, params));
            self.write_blank_header();
        }
    }

    fn gen_message_handler_impl(&mut self, class_name: &str, handler: &MessageHandler) {
        // MessageHandler has no return type in KAIN - message handlers return void
        let ret_type = "void".to_string();

        // Check if this is an RPC to use proper parameter passing
        let msg_lower = handler.message_type.to_lowercase();
        let is_rpc = msg_lower.starts_with("server_") || 
                     msg_lower.starts_with("client_") || 
                     msg_lower.starts_with("multicast_");
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
        
        self.write_source(&format!("{} {}::{}({})", ret_type, class_name, method_name, params));
        self.write_source("{");
        self.push_indent();
        self.gen_block_source(&handler.body);
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        // Server RPCs also need _Validate method
        if is_server_rpc {
            self.write_source(&format!("bool {}::{}_Validate({})", class_name, handler.message_type, params));
            self.write_source("{");
            self.push_indent();
            self.write_source("return true; // Add validation logic here");
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
    }

    /// Generate actor method declaration in header
    fn gen_actor_method_decl(&mut self, method: &Function) {
        // Check for @blueprint_pure or @blueprint_callable attributes
        let is_pure = method.attributes.iter().any(|a| a.name == "blueprint_pure" || a.name == "pure");
        let is_callable = method.attributes.iter().any(|a| a.name == "blueprint_callable" || a.name == "blueprint");
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
        let category = method.attributes.iter()
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
        
        let meta = method.attributes.iter()
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
            self.write_header(&format!("UFUNCTION(BlueprintPure, Category = \"{}\"{})", category, meta));
            if is_inline {
                // Inline body in header
                self.write_header(&format!("{} {}({}) const {{ {} }}", ret_type, method.name, params, self.gen_inline_body(&method.body)));
            } else {
                self.write_header(&format!("{} {}({}) const;", ret_type, method.name, params));
            }
        } else if is_callable {
            self.write_header(&format!("UFUNCTION(BlueprintCallable, Category = \"{}\"{})", category, meta));
            if is_inline {
                // Inline body in header
                self.write_header(&format!("{} {}({}) {{ {} }}", ret_type, method.name, params, self.gen_inline_body(&method.body)));
            } else {
                self.write_header(&format!("{} {}({});", ret_type, method.name, params));
            }
        } else {
            // Regular C++ method (no UFUNCTION)
            if is_inline {
                self.write_header(&format!("{} {}({}) {{ {} }}", ret_type, method.name, params, self.gen_inline_body(&method.body)));
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
            Expr::Ident(name, _) => {
                match name.as_str() {
                    "null" => "nullptr".to_string(),
                    "None" => "NAME_None".to_string(),
                    _ => name.clone(),
                }
            }
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => format!("{:.6}f", f),
            Expr::Bool(b, _) => b.to_string(),
            Expr::String(s, _) => format!("TEXT(\"{}\")", self.escape_string(s)),
            Expr::None(_) => "nullptr".to_string(),
            Expr::Field { object, field, .. } => {
                // Remap vector component field names: .x -> .X, .y -> .Y, etc.
                let ue5_field = match field.as_str() {
                    "x" => "X", "y" => "Y", "z" => "Z", "w" => "W",
                    "r" => "X", "g" => "Y", "b" => "Z", "a" => "W",
                    _ => field.as_str(),
                };
                format!("{}.{}", self.gen_expr_string(object), ue5_field)
            }
            Expr::Binary { left, op, right, .. } => {
                format!("({} {} {})", self.gen_expr_string(left), self.gen_binop_string(*op), self.gen_expr_string(right))
            }
            Expr::Unary { op, operand, .. } => {
                let o = self.gen_expr_string(operand);
                let op_str = match op {
                    UnaryOp::Neg => "-",
                    UnaryOp::Not => "!",
                    _ => "?",
                };
                format!("({}{})", op_str, o)
            }
            Expr::MethodCall { receiver, method, args, .. } => {
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr_string(&a.value)).collect();
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
                format!("{}.{}({})", self.gen_expr_string(receiver), ue5_method, arg_strs.join(", "))
            }
            Expr::EnumVariant { enum_name, variant, .. } => {
                let ue_name = to_enum_name(enum_name);
                format!("{}::{}", ue_name, variant)
            }
            Expr::Call { callee, args, .. } => {
                let fn_name = self.gen_expr_string(callee);
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr_string(&a.value)).collect();
                // Handle vector constructors
                match fn_name.as_str() {
                    "vec2" => format!("FVector2f({})", arg_strs.join(", ")),
                    "vec3" => format!("FVector3f({})", arg_strs.join(", ")),
                    "vec4" => format!("FVector4f({})", arg_strs.join(", ")),
                    "Vec2" => format!("FVector2f({})", arg_strs.join(", ")),
                    "Vec3" => format!("FVector3f({})", arg_strs.join(", ")),
                    "Vec4" => format!("FVector4f({})", arg_strs.join(", ")),
                    _ => format!("{}({})", fn_name, arg_strs.join(", "))
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
        // Skip inline methods (already in header)
        let is_inline = method.attributes.iter().any(|a| a.name == "inline");
        if is_inline {
            return;
        }
        
        // Check if this is a pure method (const)
        let is_pure = method.attributes.iter().any(|a| a.name == "blueprint_pure" || a.name == "pure");
        
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
            self.write_source(&format!("{} {}::{}({}) const", ret_type, class_name, method.name, params));
        } else {
            self.write_source(&format!("{} {}::{}({})", ret_type, class_name, method.name, params));
        }
        
        self.write_source("{");
        self.push_indent();
        self.gen_block_source(&method.body);
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
    }

    /// Generate USTRUCT from KAIN struct
    fn gen_ustruct(&mut self, struct_def: &Struct) {
        // Track struct name
        self.context.register_struct(struct_def.name.clone(), format!("{}.h", self.context.output_name));
        
        let struct_name = to_struct_name(&struct_def.name);
        
        let is_datatable = struct_def.attributes.iter().any(|a| a.name == "datatable");
        let is_blueprint_type = true; // All KAIN structs are BlueprintType by default
        
        if is_datatable {
            self.header.push_line("USTRUCT(BlueprintType)");
            self.write_header(&format!("struct {} {} : public FTableRowBase", self.context.module_api, struct_name));
        } else {
            self.header.push_line("USTRUCT(BlueprintType)");
            self.write_header(&format!("struct {} {}", self.context.module_api, struct_name));
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
        self.write_header("};");
    }

    /// Generate UActorComponent class from KAIN struct
    fn gen_ucomponent(&mut self, struct_def: &Struct) {
        // Track component name
        self.context.register_component(struct_def.name.clone(), format!("{}.h", self.context.output_name));
        
        let class_name = to_component_name(&struct_def.name);
        
        // Check if component has replicated fields
        let has_replicated = struct_def.fields.iter().any(|f| {
            f.attributes.iter().any(|a| a.name == "replicated")
        });
        
        // Get interface inheritance list
        let interface_list = self.context.get_interface_list(&struct_def.name);
        
        self.header.push_line("UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))");
        self.write_header(&format!("class {} {} : public UActorComponent{}", self.context.module_api, class_name, interface_list));
        self.write_header("{");
        self.push_indent();
        self.write_header("GENERATED_BODY()");
        self.write_blank_header();

        self.write_header("public:");
        self.write_blank_header();
        self.push_indent();
        self.write_header(&format!("{}();", class_name));
        self.write_blank_header();

        // Fields as UPROPERTY - components don't need default Category
        for field in &struct_def.fields {
            self.gen_uproperty_with_context(field, None, false);
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

        // Implementation
        self.write_source(&format!("{}::{}()", class_name, class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source("PrimaryComponentTick.bCanEverTick = false;");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        // Implement GetLifetimeReplicatedProps if needed
        if has_replicated {
            self.write_source(&format!("void {}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source("Super::GetLifetimeReplicatedProps(OutLifetimeProps);");
            self.write_blank_source();
            
            // Add DOREPLIFETIME for each replicated field
            for field in &struct_def.fields {
                if field.attributes.iter().any(|a| a.name == "replicated") {
                    self.write_source(&format!("DOREPLIFETIME({}, {});", class_name, field.name));
                }
            }
            
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
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
    fn gen_uproperty_with_context(&mut self, field: &Field, parent_struct_name: Option<&str>, is_blueprint_type: bool) {
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
                                        meta_tags.push(format!("ReplicationCondition = {}", ue_condition));
                                    }
                                }
                            }
                        }
                    }
                },
                "savegame" => props.push("SaveGame"),
                "transient" => props.push("Transient"),
                "editdefaults" => {
                    props.retain(|&p| p != "EditAnywhere");
                    props.push("EditDefaultsOnly");
                },
                "visibleonly" => {
                    props.retain(|&p| p != "EditAnywhere");
                    props.push("VisibleAnywhere");
                },
                "blueprint_assignable" => {
                    props.push("BlueprintAssignable");
                },
                "blueprint_callable" => {
                    props.push("BlueprintCallable");
                },
                "category" => {
                    has_explicit_category = true;
                    if !attr.args.is_empty() {
                        if let Expr::String(cat, _) = &attr.args[0] {
                            category = cat.clone();
                        }
                    }
                },
                "edit_condition" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(condition, _) = &attr.args[0] {
                            meta_tags.push(format!("EditCondition = \"{}\"", condition));
                        }
                    }
                },
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
                },
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
                },
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
                },
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
                },
                "units" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(unit, _) = &attr.args[0] {
                            meta_tags.push(format!("Units = \"{}\"", unit));
                        }
                    }
                },
                "tooltip" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(tip, _) = &attr.args[0] {
                            meta_tags.push(format!("ToolTip = \"{}\"", tip));
                        }
                    }
                },
                "display_name" => {
                    if !attr.args.is_empty() {
                        if let Expr::String(name, _) = &attr.args[0] {
                            meta_tags.push(format!("DisplayName = \"{}\"", name));
                        }
                    }
                },
                _ => {}
            }
        }
        
        // Build UPROPERTY macro
        let mut uproperty_parts: Vec<String> = props.iter().map(|s| s.to_string()).collect();
        
        // Add default Category for BlueprintType structs if no explicit category was set
        if is_blueprint_type && !has_explicit_category && category.is_empty() {
            if let Some(struct_name) = parent_struct_name {
                category = struct_name.to_string();
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
        self.context.register_enum(enum_def.name.clone(), format!("{}.h", self.context.output_name));
        
        // Check if simple enum (all Unit variants)
        let is_simple = enum_def.variants.iter().all(|v| matches!(v.fields, VariantFields::Unit));

        if is_simple {
            let enum_name = to_enum_name(&enum_def.name);
            
            self.header.push_line("UENUM(BlueprintType)");
            self.write_header(&format!("enum class {} : uint8", enum_name));
            self.write_header("{");
            self.push_indent();

            for variant in &enum_def.variants {
                self.write_header(&format!("{} UMETA(DisplayName = \"{}\"),", variant.name, variant.name));
            }

            self.pop_indent();
            self.write_header("};");
        } else {
            // Complex enums need to be represented as structs with a type tag
            self.write_header(&format!("// Complex enum {} - represented as tagged struct", enum_def.name));
        }
    }

    /// Generate UFUNCTION from KAIN function
    fn gen_ufunction(&mut self, func: &Function) {
        // Check for @ue5 attribute to determine if this should be a UFUNCTION
        let has_ue5_attr = func.attributes.iter().any(|a| a.name == "ue5" || a.name == "blueprint");
        
        if has_ue5_attr {
            let ret_type = func.return_type
                .as_ref()
                .map(|t| self.map_type(t))
                .unwrap_or_else(|| "void".to_string());

            let params = self.gen_params_for_blueprint(&func.params);
            let has_return = func.return_type.is_some();
            
            // Use a proper function library class name based on output name
            let class_name = format!("U{}FunctionLibrary", self.context.output_name);
            
            // Pure functions (no side effects) should use BlueprintPure
            let is_pure = func.attributes.iter().any(|a| a.name == "pure" || a.name == "const");
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
            self.write_source(&format!("{} {}::{}({})", ret_type, class_name, func.name, self.gen_params(&func.params)));
            self.write_source("{");
            self.push_indent();
            self.gen_block_source_with_implicit_return(&func.body, has_return);
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        } else {
            // Regular C++ function
            let ret_type = func.return_type
                .as_ref()
                .map(|t| self.map_type(t))
                .unwrap_or_else(|| "void".to_string());

            let params = self.gen_params(&func.params);
            let has_return = func.return_type.is_some();
            
            self.write_header(&format!("{} {}({});", ret_type, func.name, params));
            
            self.write_source(&format!("{} {}({})", ret_type, func.name, params));
            self.write_source("{");
            self.push_indent();
            self.gen_block_source_with_implicit_return(&func.body, has_return);
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
                let needs_ref = is_rpc && (ty_str.starts_with("FString") || ty_str.starts_with("TArray"));
                
                let param_decl = if p.mutable {
                    // Output parameter - use UPARAM(ref) for Blueprint visibility
                    if is_blueprint {
                        format!("UPARAM(ref) {}& {}", ty_str, p.name)
                    } else {
                        format!("{}& {}", ty_str, p.name)
                    }
                } else if needs_ref {
                    format!("const {}& {}", ty_str, p.name)
                } else {
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
            Expr::EnumVariant { enum_name, variant, .. } => {
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
                    if !matches!(expr, Expr::Return(_, _) | Expr::Break(_, _) | Expr::Continue(_)) {
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
            Stmt::Let { pattern, ty, value, .. } => {
                if let Pattern::Binding { name, mutable, .. } = pattern {
                    let ty_str = ty.as_ref()
                        .map(|t| self.map_type(t))
                        .unwrap_or_else(|| "auto".to_string());
                    
                    if let Some(val) = value {
                        if *mutable {
                            self.write_source(&format!("{} {} = {};", ty_str, name, self.gen_expr(val)));
                        } else {
                            self.write_source(&format!("const {} {} = {};", ty_str, name, self.gen_expr(val)));
                        }
                    } else {
                        self.write_source(&format!("{} {};", ty_str, name));
                    }
                }
            }

            Stmt::Return(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    self.write_source(&format!("return {};", self.gen_expr(expr)));
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

            Stmt::For { binding, iter, body, .. } => {
                if let Pattern::Binding { name, .. } = binding {
                    self.write_source(&format!("for (auto {} : {})", name, self.gen_expr(iter)));
                    self.write_source("{");
                    self.push_indent();
                    self.gen_block_source(body);
                    self.pop_indent();
                    self.write_source("}");
                }
            }

            Stmt::While { condition, body, .. } => {
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
                if let Expr::If { condition, then_branch, else_branch, .. } = expr {
                    self.gen_if_stmt(condition, then_branch, else_branch);
                } else if let Expr::Assign { target, value, .. } = expr {
                    self.write_source(&format!("{} = {};", self.gen_expr(target), self.gen_expr(value)));
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

    fn gen_if_stmt(&mut self, condition: &Expr, then_branch: &Block, else_branch: &Option<Box<ElseBranch>>) {
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
                            fmt_str.push_str(&self.escape_string(s));
                        }
                        _ => {
                            let expr_code = self.gen_expr(part);
                            // Determine format specifier based on expression
                            // For now use %s with LexToString for general case
                            fmt_str.push_str("%s");
                            fmt_args.push(format!("*LexToString({})", expr_code));
                        }
                    }
                }
                if fmt_args.is_empty() {
                    format!("TEXT(\"{}\")", fmt_str)
                } else {
                    format!("FString::Printf(TEXT(\"{}\"), {})", fmt_str, fmt_args.join(", "))
                }
            }
            Expr::Bool(b, _) => if *b { "true".to_string() } else { "false".to_string() },
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

            Expr::Binary { left, op, right, .. } => {
                let l = self.gen_expr(left);
                let r = self.gen_expr(right);
                let op_str = self.map_binop(op);
                format!("({} {} {})", l, op_str, r)
            }

            Expr::Unary { op, operand, .. } => {
                let o = self.gen_expr(operand);
                let op_str = self.map_unaryop(op);
                format!("({}{})", op_str, o)
            }

            Expr::Call { callee, args, .. } => {
                let fn_name = self.gen_expr(callee);
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();

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
                                return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"), {})", fmt_str, fmt_args.join(", "));
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
                                        // Determine proper UE_LOG format specifier based on type
                                        let (spec, arg) = get_ue_log_format_spec(part, &expr_code);
                                        fmt_str.push_str(&spec);
                                        fmt_args.push(arg);
                                    }
                                }
                            }
                            
                            if fmt_args.is_empty() {
                                return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"))", fmt_str);
                            } else {
                                return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"), {})", fmt_str, fmt_args.join(", "));
                            }
                        }
                    }
                    
                    // For multiple args or non-string, convert all to FString
                    let arg_strs: Vec<String> = args.iter().map(|a| {
                        let expr = self.gen_expr(&a.value);
                        // Wrap non-string types in FString conversion for formatting
                        if expr.starts_with("TEXT(") {
                            // Don't wrap TEXT() in FString - just use it directly
                            expr
                        } else if expr.starts_with("FString") {
                            expr
                        } else {
                            format!("LexToString({})", expr)
                        }
                    }).collect();
                    
                    // Special case: single TEXT() literal should use direct logging
                    if arg_strs.len() == 1 {
                        let arg = &arg_strs[0];
                        if arg.starts_with("FString(TEXT(") {
                            // Extract just the TEXT(...) part
                            let inner = &arg[8..arg.len()-1];
                            return format!("UE_LOG(LogTemp, Warning, {})", inner);
                        } else if arg.starts_with("TEXT(") {
                            return format!("UE_LOG(LogTemp, Warning, {})", arg);
                        } else if arg.starts_with("\"") {
                            // Raw string literal, wrap in TEXT
                            return format!("UE_LOG(LogTemp, Warning, TEXT(\"{}\"))", self.escape_string(arg.trim_matches('\"')));
                        }
                    }

                    // Join args with space separator for simple multi-arg logging
                    // Use FString concatenation with proper wrapping
                    let joined_args = arg_strs.iter().map(|s| {
                        if s.starts_with("TEXT(") {
                            format!("FString({})", s)
                        } else if s.starts_with("FString") {
                            s.clone()
                        } else {
                            s.clone()
                        }
                    }).collect::<Vec<_>>().join(" + FString(TEXT(\" \")) + ");
                    
                    return format!("UE_LOG(LogTemp, Warning, TEXT(\"%s\"), *({}))", joined_args);
                }
                
                // Handle vector constructors (vec2, vec3, vec4 and PascalCase variants)
                match fn_name.as_str() {
                    "vec2" | "Vec2" => return format!("FVector2D({})", arg_strs.join(", ")),
                    "vec3" | "Vec3" => return format!("FVector({})", arg_strs.join(", ")),
                    "vec4" | "Vec4" => return format!("FVector4({})", arg_strs.join(", ")),
                    "Color" | "color" => return format!("FLinearColor({})", arg_strs.join(", ")),
                    "rotation" | "Rotation" | "rotator" | "Rotator" => return format!("FRotator({})", arg_strs.join(", ")),
                    "transform" | "Transform" => return format!("FTransform({})", arg_strs.join(", ")),
                    _ => {}
                }
                
                // Check if this is a KNOWN struct constructor (registered in context or EngineKnowledge)
                // Only add F-prefix if we can confirm this is actually a struct type.
                // Do NOT blindly prefix all PascalCase calls — that breaks actor method calls
                // like SetStatus(), UpdateMaterial(), CreateDynamicMaterialInstance(), etc.
                if fn_name.chars().next().map_or(false, |c| c.is_uppercase()) 
                    && !fn_name.starts_with("Server_")
                    && !fn_name.starts_with("Client_")
                    && !fn_name.starts_with("Multicast_") {
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
                        let ue_name = if fn_name.starts_with('F') { fn_name.clone() } else { format!("F{}", fn_name) };
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
                    "dot" => return {
                        // Use component-wise dot product for float vectors
                        if arg_strs.len() == 2 {
                            // For Vec2, use manual dot product: a.X * b.X + a.Y * b.Y
                            // For Vec3, use FVector::DotProduct
                            format!("FVector::DotProduct({}, {})", arg_strs[0], arg_strs[1])
                        } else {
                            format!("FVector::DotProduct({})", arg_strs.join(", "))
                        }
                    },
                    "cross" => "FVector::CrossProduct",
                    "normalize" => return {
                        if arg_strs.len() == 1 {
                            format!("{}.GetSafeNormal()", arg_strs[0])
                        } else {
                            format!("FVector::GetSafeNormal({})", arg_strs.join(", "))
                        }
                    },
                    "length" => return {
                        if arg_strs.len() == 1 {
                            format!("{}.Size()", arg_strs[0])
                        } else {
                            format!("FVector::Size({})", arg_strs.join(", "))
                        }
                    },
                    "distance" | "dist" => return {
                        // Use appropriate Dist function based on vector type
                        if arg_strs.len() == 2 {
                            format!("FVector2D::Dist({}, {})", arg_strs[0], arg_strs[1])
                        } else {
                            format!("FVector::Dist({})", arg_strs.join(", "))
                        }
                    },
                    
                    // Interp functions
                    "FInterpTo" => "FMath::FInterpTo",
                    "VInterpTo" => "FMath::VInterpTo",
                    "RInterpTo" => "FMath::RInterpTo",
                    
                    // Material creation — route through mesh component for fragment shader actors
                    // CreateDynamicMaterialInstance(ShaderName) → mesh_component->CreateDynamicMaterialInstance(0)
                    // Fragment shaders use materials (assigned in editor), not direct shader references
                    "CreateDynamicMaterialInstance" => return {
                        // Find the first mesh component field from var_types
                        let mesh_field = self.var_types.iter()
                            .find(|(_, ty)| ty.contains("MeshComponent") || ty.contains("StaticMesh") || ty.contains("SkeletalMesh"))
                            .map(|(name, _)| name.clone());
                        if let Some(mesh) = mesh_field {
                            format!("{}->CreateDynamicMaterialInstance(0)", mesh)
                        } else {
                            // Fallback: assume mesh_component exists
                            "mesh_component->CreateDynamicMaterialInstance(0)".to_string()
                        }
                    },

                    // UE5 specific mappings from stdlib extern functions
                    "GetWorldDeltaSeconds" => return "GetWorld()->GetDeltaSeconds()".to_string(),
                    "GetWorldTimeSeconds" => return "GetWorld()->GetTimeSeconds()".to_string(),
                    "GetWorldRealTimeSeconds" => return "GetWorld()->GetRealTimeSeconds()".to_string(),
                    "IsServer" => return "HasAuthority()".to_string(),
                    "IsClient" => return "!HasAuthority()".to_string(),
                    "IsStandalone" => return "GetNetMode() == NM_Standalone".to_string(),
                    "PrintToScreen" => return {
                        if arg_strs.len() >= 3 {
                            format!("GEngine->AddOnScreenDebugMessage(-1, {}, {}, {})", 
                                arg_strs.get(1).map(|s| s.as_str()).unwrap_or("5.0f"),
                                arg_strs.get(2).map(|s| s.as_str()).unwrap_or("FColor::White"),
                                arg_strs[0])
                        } else if arg_strs.len() == 1 {
                            format!("GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::White, {})", arg_strs[0])
                        } else {
                            format!("GEngine->AddOnScreenDebugMessage(-1, {}, FColor::White, {})", 
                                arg_strs.get(1).map(|s| s.as_str()).unwrap_or("5.0f"),
                                arg_strs[0])
                        }
                    },
                    
                    _ => fn_name.as_str()
                };
                
                // BUG-008: qualify @blueprint fn calls with U{Plugin}FunctionLibrary::
                if self.blueprint_fn_names.contains(ue5_fn_name) {
                    let lib_class = format!("U{}FunctionLibrary", to_pascal_case(&self.module_name));
                    return format!("{}::{}({})", lib_class, ue5_fn_name, arg_strs.join(", "));
                }

                format!("{}({})", ue5_fn_name, arg_strs.join(", "))
            }

            Expr::MethodCall { receiver, method, args, .. } => {
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
                    format!("{}{}{}({})", recv, access_op, ue5_method, arg_strs.join(", "))
                }
            }

            Expr::Field { object, field, .. } => {
                let obj = self.gen_expr(object);
                
                // Remap vector component field names: .x -> .X, .y -> .Y, etc.
                let ue5_field = match field.as_str() {
                    "x" => "X",
                    "y" => "Y",
                    "z" => "Z",
                    "w" => "W",
                    "r" => "X",  // Color aliases
                    "g" => "Y",
                    "b" => "Z",
                    "a" => "W",
                    _ => field.as_str(),
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
                // Use brace initialization - compiler will deduce element type from context
                format!("{{{}}}", elems.join(", "))
            }

            Expr::Struct { name, fields, .. } => {
                let ue_name = to_struct_name(name);
                let field_strs: Vec<String> = fields
                    .iter()
                    .map(|(_, fval)| self.gen_expr(fval))
                    .collect();
                format!("{}{{{}}}", ue_name, field_strs.join(", "))
            }

            Expr::EnumVariant { enum_name, variant, .. } => {
                let ue_name = to_enum_name(enum_name);
                format!("{}::{}", ue_name, variant)
            }

            Expr::If { condition, then_branch, else_branch, .. } => {
                // Ternary for simple cases
                if then_branch.stmts.len() == 1 && else_branch.is_some() {
                    if let Stmt::Expr(then_expr) = &then_branch.stmts[0] {
                        if let Some(ElseBranch::Else(else_block)) = else_branch.as_ref().map(|b| b.as_ref()) {
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
                let param_strs: Vec<String> = params.iter().map(|p| format!("auto {}", p.name)).collect();
                format!("[this]({}){{{}}}", param_strs.join(", "), self.gen_expr(body))
            }

            Expr::Cast { value, target, .. } => {
                format!("static_cast<{}>({})", self.map_type(target), self.gen_expr(value))
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

            Expr::Match { scrutinee, arms, .. } => {
                // Check if any arm body is an assignment - if so, this is a statement-level match
                let has_assignment = arms.iter().any(|arm| {
                    matches!(&arm.body, Expr::Assign { .. })
                });
                
                // Detect if the match can be represented as ternary (simple patterns only)
                let has_complex_patterns = arms.iter().any(|arm| {
                    matches!(&arm.pattern, 
                        Pattern::Tuple(_, _) | 
                        Pattern::Struct { .. } | 
                        Pattern::Slice { .. } |
                        Pattern::Range { .. } |
                        Pattern::Or(_, _)
                    )
                });
                
                let scrut = self.gen_expr(scrutinee);
                
                // If arms contain assignments, generate as statement-level if/else
                if has_assignment {
                    // Generate if/else chain for assignments
                    let mut result = String::new();
                    let mut first = true;
                    
                    for arm in arms {
                        let is_wildcard = matches!(&arm.pattern, Pattern::Wildcard(_)) || 
                            matches!(&arm.pattern, Pattern::Binding { name, .. } if name == "_");
                        
                        match &arm.pattern {
                            Pattern::Wildcard(_) => {
                                // Default case
                                if !first {
                                    result.push_str(" else ");
                                }
                                result.push_str("{ ");
                                if let Expr::Assign { target, value, .. } = &arm.body {
                                    result.push_str(&format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value)));
                                } else {
                                    result.push_str(&format!("{}; ", self.gen_expr(&arm.body)));
                                }
                                result.push_str("}");
                            }
                            Pattern::Binding { name, .. } if name == "_" => {
                                // Default case with _ binding
                                if !first {
                                    result.push_str(" else ");
                                }
                                result.push_str("{ ");
                                if let Expr::Assign { target, value, .. } = &arm.body {
                                    result.push_str(&format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value)));
                                } else {
                                    result.push_str(&format!("{}; ", self.gen_expr(&arm.body)));
                                }
                                result.push_str("}");
                            }
                            Pattern::Variant { enum_name, variant, .. } => {
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
                                if let Expr::Assign { target, value, .. } = &arm.body {
                                    result.push_str(&format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value)));
                                } else {
                                    result.push_str(&format!("{}; ", self.gen_expr(&arm.body)));
                                }
                                result.push_str("}");
                            }
                            Pattern::Literal(lit) => {
                                let lit_str = self.gen_expr(lit);
                                
                                if first {
                                    result.push_str(&format!("if ({} == {}) ", scrut, lit_str));
                                    first = false;
                                } else {
                                    result.push_str(&format!("else if ({} == {}) ", scrut, lit_str));
                                }
                                
                                result.push_str("{ ");
                                if let Expr::Assign { target, value, .. } = &arm.body {
                                    result.push_str(&format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value)));
                                } else {
                                    result.push_str(&format!("{}; ", self.gen_expr(&arm.body)));
                                }
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
                                if let Expr::Assign { target, value, .. } = &arm.body {
                                    result.push_str(&format!("{} = {}; ", self.gen_expr(target), self.gen_expr(value)));
                                } else {
                                    result.push_str(&format!("{}; ", self.gen_expr(&arm.body)));
                                }
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
                            Pattern::Variant { enum_name, variant, .. } => {
                                let path = if let Some(en) = enum_name {
                                    format!("{}::{}", to_enum_name(en), variant)
                                } else {
                                    variant.clone()
                                };
                                parts.push(format!("{} == {}", scrut, path));
                                parts.push(arm_value);
                            }
                            Pattern::Range { start, end, inclusive, .. } => {
                                // Range pattern: 1..10 or 1..=10
                                let start_str = start.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                                let end_str = end.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                                let op = if *inclusive { "<=" } else { "<" };
                                
                                if start_str.is_empty() {
                                    parts.push(format!("{} {} {}", scrut, op, end_str));
                                } else if end_str.is_empty() {
                                    parts.push(format!("{} >= {}", scrut, start_str));
                                } else {
                                    parts.push(format!("({} >= {} && {} {} {})", scrut, start_str, scrut, op, end_str));
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
                            result.push_str(&format!("if ({}) return {}; ", parts[i], parts[i + 1]));
                        } else {
                            result.push_str(&format!("else if ({}) return {}; ", parts[i], parts[i + 1]));
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
                                result.push_str(&format!("({} == {}) ? {}", scrut, lit_str, arm_value));
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
                                result.push_str(&format!("({} == {}) ? {}", scrut, name, arm_value));
                                first = false;
                            }
                            Pattern::Variant { enum_name, variant, .. } => {
                                let path = if let Some(en) = enum_name {
                                    format!("{}::{}", to_enum_name(en), variant)
                                } else {
                                    variant.clone()
                                };
                                if !first {
                                    result.push_str(" : ");
                                }
                                result.push_str(&format!("({} == {}) ? {}", scrut, path, arm_value));
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
        if let Type::Function { params, return_type, .. } = &alias.target {
            let delegate_name = format!("F{}", alias.name);
            
            // Track this delegate name
            self.context.register_delegate(alias.name.clone(), format!("{}.h", self.context.output_name));
            
            // Determine if it has parameters
            if params.is_empty() {
                // No parameters - simple delegate
                self.write_header(&format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE({});", delegate_name));
            } else if params.len() == 1 {
                // One parameter
                let param_type = self.map_type(&params[0]);
                let param_name = format!("Param{}", 0);
                self.write_header(&format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam({}, {}, {});", 
                    delegate_name, param_type, param_name));
            } else if params.len() == 2 {
                // Two parameters
                let param1_type = self.map_type(&params[0]);
                let param1_name = format!("Param{}", 0);
                let param2_type = self.map_type(&params[1]);
                let param2_name = format!("Param{}", 1);
                self.write_header(&format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams({}, {}, {}, {}, {});", 
                    delegate_name, param1_type, param1_name, param2_type, param2_name));
            } else if params.len() == 3 {
                // Three parameters
                let param1_type = self.map_type(&params[0]);
                let param1_name = format!("Param{}", 0);
                let param2_type = self.map_type(&params[1]);
                let param2_name = format!("Param{}", 1);
                let param3_type = self.map_type(&params[2]);
                let param3_name = format!("Param{}", 2);
                self.write_header(&format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams({}, {}, {}, {}, {}, {}, {});", 
                    delegate_name, param1_type, param1_name, param2_type, param2_name, param3_type, param3_name));
            } else {
                // More than 3 parameters - use generic macro
                self.write_header(&format!("// TODO: Delegate {} has {} parameters - UE5 supports up to 9 with DECLARE_DYNAMIC_MULTICAST_DELEGATE_<N>Params", 
                    delegate_name, params.len()));
            }
            
            self.write_blank_header();
        }
    }
    
    /// Generate regular delegate declaration (non-multicast)
    fn gen_delegate(&mut self, alias: &kain_core::ast::TypeAlias) {
        if let Type::Function { params, return_type, .. } = &alias.target {
            let delegate_name = format!("F{}", alias.name);
            
            // Track this delegate name
            self.context.register_delegate(alias.name.clone(), format!("{}.h", self.context.output_name));
            
            // Similar to multicast but use DECLARE_DYNAMIC_DELEGATE
            if params.is_empty() {
                self.write_header(&format!("DECLARE_DYNAMIC_DELEGATE({});", delegate_name));
            } else if params.len() == 1 {
                let param_type = self.map_type(&params[0]);
                let param_name = format!("Param{}", 0);
                self.write_header(&format!("DECLARE_DYNAMIC_DELEGATE_OneParam({}, {}, {});", 
                    delegate_name, param_type, param_name));
            } else if params.len() == 2 {
                let param1_type = self.map_type(&params[0]);
                let param1_name = format!("Param{}", 0);
                let param2_type = self.map_type(&params[1]);
                let param2_name = format!("Param{}", 1);
                self.write_header(&format!("DECLARE_DYNAMIC_DELEGATE_TwoParams({}, {}, {}, {}, {});", 
                    delegate_name, param1_type, param1_name, param2_type, param2_name));
            } else {
                self.write_header(&format!("// TODO: Delegate {} has {} parameters", delegate_name, params.len()));
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
                }
                for method in &a.ast.methods {
                    if let Some(ret) = &method.return_type {
                        self.map_type(ret);
                    }
                    for param in &method.params {
                        self.map_type(&param.ty);
                    }
                }
            },
            TypedItem::Struct(s) => {
                for field in &s.ast.fields {
                    self.map_type(&field.ty);
                }
            },
            TypedItem::Component(c) => {
                for state in &c.ast.state {
                    self.map_type(&state.ty);
                }
            },
            TypedItem::TypeAlias(a) => {
                self.map_type(&a.ast.target);
            },
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
            for g in generics { collect_type_names(g, out); }
        }
        Type::Array(inner, _, _) | Type::Slice(inner, _) => collect_type_names(inner, out),
        Type::Option(inner, _) => collect_type_names(inner, out),
        Type::Result(ok, err, _) => { collect_type_names(ok, out); collect_type_names(err, out); }
        Type::Ref { inner, .. } => collect_type_names(inner, out),
        Type::Tuple(items, _) => { for t in items { collect_type_names(t, out); } }
        Type::Function { params, return_type, .. } => {
            for p in params { collect_type_names(p, out); }
            collect_type_names(return_type, out);
        }
        _ => {}
    }
}

/// Return a C++11 member initializer value for common UE5 C++ types.
/// Used in USTRUCT field declarations to satisfy UE5.4 `LogClass` strictness
/// (`Property ... is not initialized properly`).
/// Returns `None` for self-initializing types (FString, TArray, etc.) where
/// an explicit initializer is unnecessary.
fn default_cpp_value(ty_str: &str) -> Option<&'static str> {
    match ty_str {
        "int8" | "int16" | "int32" | "int64"
        | "uint8" | "uint16" | "uint32" | "uint64" => Some("0"),
        "float"  => Some("0.0f"),
        "double" => Some("0.0"),
        "bool"   => Some("false"),
        "FName"  => Some("NAME_None"),
        "FVector"  | "FVector3f"  => Some("FVector::ZeroVector"),
        "FVector2D" | "FVector2f" => Some("FVector2D::ZeroVector"),
        "FVector4"  | "FVector4f" => Some("FVector4(ForceInitToZero)"),
        "FRotator"  => Some("FRotator::ZeroRotator"),
        "FQuat" | "FQuat4f" => Some("FQuat::Identity"),
        "FTransform" => Some("FTransform::Identity"),
        "FLinearColor" => Some("FLinearColor::White"),
        "FColor"  => Some("FColor::White"),
        // Pointer types must always be nullptr
        ty if ty.ends_with('*') => Some("nullptr"),
        // FString, FText, TArray, TMap, TSet — default-construct to empty, no initializer needed.
        _ => None,
    }
}
