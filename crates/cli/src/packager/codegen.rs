use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use chrono::Datelike;
use serde::Serialize;
use crate::error::{KainError, KainResult};
use kain_core::diagnostics::{SpanMapper, enhance_error_with_location};
use super::config::Ue5Config;
use super::plugin_layout::PluginLayout;

extern crate ue5;
extern crate ue5_editor;
extern crate ue5_shaders;

/// Helper function to enhance codegen errors with file:line:col location information
fn enhance_codegen_result<T>(
    result: KainResult<T>,
    span_mapper: &SpanMapper,
    file_path: &str,
) -> KainResult<T> {
    result.map_err(|e| enhance_error_with_location(e, span_mapper, file_path))
}

#[derive(Debug, Clone, Serialize, Default)]
pub struct SymbolRoutingManifest {
    pub symbol_owner: HashMap<String, String>,
    pub symbol_header: HashMap<String, String>,
}

impl SymbolRoutingManifest {
    fn register(&mut self, symbol: &str, module: &str, include_path: &str) {
        self.symbol_owner.insert(symbol.to_string(), module.to_string());
        self.symbol_header
            .insert(symbol.to_string(), include_path.replace('\\', "/"));
    }

    fn extend_from(&mut self, other: &SymbolRoutingManifest) {
        for (k, v) in &other.symbol_owner {
            self.symbol_owner.insert(k.clone(), v.clone());
        }
        for (k, v) in &other.symbol_header {
            self.symbol_header.insert(k.clone(), v.clone());
        }
    }
}

#[derive(Debug, Clone)]
struct ItemRoute {
    module_name: String,
    public_dir: PathBuf,
    private_dir: PathBuf,
    include_prefix: String,
}

fn normalize_path_for_match(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn wildcard_match(pattern: &str, text: &str) -> bool {
    let p: Vec<char> = pattern.chars().collect();
    let t: Vec<char> = text.chars().collect();
    let (mut pi, mut ti, mut star_idx, mut match_idx) = (0usize, 0usize, None::<usize>, 0usize);

    while ti < t.len() {
        if pi < p.len() && (p[pi] == '?' || p[pi] == t[ti]) {
            pi += 1;
            ti += 1;
        } else if pi < p.len() && p[pi] == '*' {
            star_idx = Some(pi);
            match_idx = ti;
            pi += 1;
        } else if let Some(star) = star_idx {
            pi = star + 1;
            match_idx += 1;
            ti = match_idx;
        } else {
            return false;
        }
    }

    while pi < p.len() && p[pi] == '*' {
        pi += 1;
    }
    pi == p.len()
}

fn source_matches_glob(source_path: &Path, glob: &Path) -> bool {
    let source = normalize_path_for_match(source_path);
    let pat = normalize_path_for_match(glob);
    wildcard_match(&pat, &source)
}

fn default_runtime_module_name(config: &Ue5Config) -> Option<String> {
    config
        .modules
        .iter()
        .find(|m| {
            matches!(m.module_type, super::config::Ue5ModuleType::Runtime) && m.name == config.plugin_name
        })
        .or_else(|| {
            config
                .modules
                .iter()
                .find(|m| matches!(m.module_type, super::config::Ue5ModuleType::Runtime))
        })
        .or_else(|| config.modules.first())
        .map(|m| m.name.clone())
}

fn default_editor_module_name(config: &Ue5Config) -> Option<String> {
    config
        .modules
        .iter()
        .find(|m| m.module_type.is_editorish() && m.name == format!("{}Editor", config.plugin_name))
        .or_else(|| config.modules.iter().find(|m| m.module_type.is_editorish()))
        .map(|m| m.name.clone())
}

fn resolve_item_module(
    config: &Ue5Config,
    symbol_name: &str,
    is_editor_item: bool,
    symbol_source_map: &HashMap<String, PathBuf>,
) -> Option<String> {
    if !config.has_module_plan() {
        return None;
    }

    let source_path = symbol_source_map.get(symbol_name);
    if let Some(src) = source_path {
        for module in &config.modules {
            if module.source_globs.is_empty() {
                continue;
            }
            if module
                .source_globs
                .iter()
                .any(|glob| source_matches_glob(src, glob) || src.ends_with(glob))
            {
                return Some(module.name.clone());
            }
        }
    }

    if is_editor_item {
        default_editor_module_name(config).or_else(|| default_runtime_module_name(config))
    } else {
        default_runtime_module_name(config)
    }
}

fn resolve_bucket_folder(module: &super::config::Ue5ModuleConfig, bucket: &str) -> Option<PathBuf> {
    module.folders.get(bucket).cloned()
}

fn item_route(
    layout: &PluginLayout,
    config: &Ue5Config,
    symbol_name: &str,
    is_editor_item: bool,
    bucket: &str,
    symbol_source_map: &HashMap<String, PathBuf>,
) -> ItemRoute {
    if !config.has_module_plan() {
        if is_editor_item {
            let public = layout
                .editor_public_dir
                .clone()
                .unwrap_or_else(|| layout.public_dir.join("Editor"));
            let private = layout
                .editor_private_dir
                .clone()
                .unwrap_or_else(|| layout.private_dir.join("Editor"));
            return ItemRoute {
                module_name: format!("{}Editor", config.plugin_name),
                public_dir: public,
                private_dir: private,
                include_prefix: "Editor".to_string(),
            };
        }

        return ItemRoute {
            module_name: config.plugin_name.clone(),
            public_dir: layout.public_dir.clone(),
            private_dir: layout.private_dir.clone(),
            include_prefix: String::new(),
        };
    }

    let module_name = resolve_item_module(config, symbol_name, is_editor_item, symbol_source_map)
        .unwrap_or_else(|| config.plugin_name.clone());

    let (mut public_dir, mut private_dir) = layout
        .module_dirs
        .get(&module_name)
        .cloned()
        .unwrap_or_else(|| (layout.public_dir.clone(), layout.private_dir.clone()));

    if let Some(module_cfg) = config.modules.iter().find(|m| m.name == module_name) {
        if let Some(folder) = resolve_bucket_folder(module_cfg, bucket) {
            public_dir = public_dir.join(&folder);
            private_dir = private_dir.join(folder);
        }
    }

    ItemRoute {
        include_prefix: module_name.clone(),
        module_name,
        public_dir,
        private_dir,
    }
}

fn build_include_path(route: &ItemRoute, filename: &str, consumer_module: Option<&str>) -> String {
    let mut include = filename.replace('\\', "/");
    let same_module = consumer_module
        .map(|m| m == route.module_name)
        .unwrap_or(false);
    if !route.include_prefix.is_empty() && !same_module {
        include = format!("{}/{}", route.include_prefix, include);
    }
    include
}

/// Compile shaders from the typed program into .usf/.h/.cpp files.
pub fn compile_shaders(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    shader_names: &[String],
) -> KainResult<()> {
    if !shader_names.is_empty() {
        eprintln!("⚡ [PACKAGER] Found {} shaders:", shader_names.len());
        for name in shader_names {
            eprintln!("   - {}", name);
        }
        println!("⚡ Found {} shaders:", shader_names.len());
        for name in shader_names {
            println!("   - {}", name);
        }
        println!();
        
        // Bug-1 fix: generate the shared POD types header once before individual shaders.
        // Individual shader headers now #include "{Plugin}ShaderTypes.h" instead of
        // inlining struct definitions, preventing C2011 redefinition errors.
        if let Some(types_content) = ue5_shaders::generate_shared_types_header(program, &config.plugin_name) {
            let types_path = layout.public_dir.join(format!("{}ShaderTypes.h", config.plugin_name));
            fs::write(&types_path, types_content).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}ShaderTypes.h (shared POD mirror types)", config.plugin_name);
        }

        // Generate shared shader library (.ush) with common HLSL helpers.
        // This analyzes all shaders for common patterns (bounds checks, UV setup,
        // noise functions) and extracts them into a reusable include file.
        let has_shared_library = if let Some(ush_content) = ue5_shaders::generate_shared_shader_library(program, &config.plugin_name) {
            let ush_path = layout.shaders_dir.join(format!("{}Common.ush", config.plugin_name));
            fs::write(&ush_path, ush_content).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}Common.ush (shared shader helpers)", config.plugin_name);
            true
        } else {
            false
        };

        // The #include line to inject into .usf files that use the shared library
        let ush_include = format!(
            "#include \"/Plugin/{}/Shaders/{}Common.ush\"\n",
            config.plugin_name, config.plugin_name
        );

        // Compile each shader using the merged typed program
        for shader_name in shader_names {
            eprintln!("🔨 [PACKAGER] Compiling shader: {}", shader_name);
            println!("🔨 Compiling shader: {}", shader_name);
            
            // Generate all three shader artifacts in one pass (mirrors computed once).
            match ue5_shaders::compile_shader_artifacts(program, shader_name, &config.plugin_name) {
                Ok(artifacts) => {
                    // Inject shared library #include into .usf if this shader uses shared helpers
                    let usf_content = if has_shared_library {
                        // Insert #include after the Platform.ush include
                        let platform_include = "#include \"/Engine/Public/Platform.ush\"\n";
                        if artifacts.usf.contains(platform_include) {
                            artifacts.usf.replacen(
                                platform_include,
                                &format!("{}{}", platform_include, &ush_include),
                                1
                            )
                        } else {
                            // Fallback: prepend at top
                            format!("{}{}", ush_include, artifacts.usf)
                        }
                    } else {
                        artifacts.usf
                    };

                    let usf_path = layout.shaders_dir.join(format!("{}.usf", shader_name));
                    fs::write(&usf_path, usf_content).map_err(|e| KainError::Io(e))?;
                    println!("   ✓ {}.usf", shader_name);

                    let header_path = layout.public_dir.join(format!("{}.h", shader_name));
                    fs::write(&header_path, artifacts.header).map_err(|e| KainError::Io(e))?;
                    println!("   ✓ {}.h", shader_name);

                    let cpp_path = layout.private_dir.join(format!("{}.cpp", shader_name));
                    fs::write(&cpp_path, artifacts.cpp).map_err(|e| KainError::Io(e))?;
                    println!("   ✓ {}.cpp", shader_name);
                }
                Err(e) => {
                    eprintln!("   ✗ Failed to compile shader artifacts for {}: {}", shader_name, e);
                    continue;
                }
            }
        }
    } else {
        println!("ℹ️  No shaders detected - skipping shader compilation");
        println!();
    }
    Ok(())
}

/// Generate master header, delegate header, and EditorTypes header.
/// Returns (master_header_path, delegate_count, type_headers).
pub fn generate_headers(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
) -> KainResult<(PathBuf, usize, HashMap<String, String>)> {
    // STEP 1: Generate master header FIRST (forward declarations only)
    println!("   📦 Generating master header with forward declarations...");
    let mut master_header = String::new();
    master_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
        chrono::Utc::now().year(),
        config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
    master_header.push_str("// Generated by KAIN-PRO - Godmode v3 (Modular Output)\n");
    master_header.push_str("// Master header - forward declarations and includes\n\n");
    master_header.push_str("#pragma once\n\n");
    master_header.push_str("#include \"CoreMinimal.h\"\n\n");
    
    // Add forward declarations for all types.
    // In split mode (runtime + editor modules), skip editor-only items from the runtime master header.
    // Editor items belong in the editor module and should not pollute the runtime module.
    master_header.push_str("// Forward declarations\n");
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Struct(s) => {
                // Skip editor-only structs in split mode — they go in the editor module
                let is_editor = s.ast.attributes.iter().any(|a| ue5_editor::is_editor_attribute(&a.name));
                if is_editor && layout.needs_split {
                    continue;
                }
                // Slate widgets use class S{name}, not struct F{name}
                if s.ast.attributes.iter().any(|a| a.name == "slate") {
                    let widget_name = format!("S{}", s.ast.name);
                    master_header.push_str(&format!("class {};\n", widget_name));
                } else {
                    let struct_name = ue5::naming::to_struct_name(&s.ast.name);
                    master_header.push_str(&format!("struct {};\n", struct_name));
                }
            }
            kain_core::types::TypedItem::Enum(e) => {
                let enum_name = ue5::naming::to_enum_name(&e.ast.name);
                master_header.push_str(&format!("enum class {} : uint8;\n", enum_name));
            }
            kain_core::types::TypedItem::Actor(a) => {
                let actor_name = ue5::naming::to_actor_name(&a.ast.name);
                master_header.push_str(&format!("class {};\n", actor_name));
            }
            kain_core::types::TypedItem::Component(c) => {
                let comp_name = ue5::naming::to_component_name(&c.ast.name);
                master_header.push_str(&format!("class {};\n", comp_name));
            }
            _ => {}
        }
    }
    
    // STEP 1.5: Generate delegate header
    let delegate_count = generate_delegate_header(layout, config, program)?;
    
    // STEP 1.6: Generate EditorTypes header
    generate_editor_types_header(layout, config, program, delegate_count)?;
    
    master_header.push_str("\n// Module includes\n");
    
    // Include delegate header FIRST if we have delegates (solves circular dependency!)
    if delegate_count > 0 {
        master_header.push_str(&format!("#include \"{}Delegates.h\"\n", config.plugin_name));
    }
    
    let master_header_path = layout.public_dir.join(format!("{}.h", config.plugin_name));
    fs::write(&master_header_path, &master_header).map_err(|e| KainError::Io(e))?;
    println!("      ✓ {}.h (master header with forward decls)", config.plugin_name);
    
    // Build Global Type Registry
    let mut type_headers = HashMap::new();
    for item in &program.items {
        let (item_name, output_name) = match item {
            kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
            kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
            kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
            kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
            kain_core::types::TypedItem::StateMachine(sm) => (&sm.name, sm.name.clone()),
            kain_core::types::TypedItem::AsyncTask(at) => (&at.name, at.name.clone()),
            kain_core::types::TypedItem::TypeAlias(a) => {
                // Delegates go in master header, not separate files
                (&a.ast.name, format!("{}", config.plugin_name))
            },
            _ => continue,
        };
        type_headers.insert(item_name.clone(), format!("{}.h", output_name));
    }
    
    Ok((master_header_path, delegate_count, type_headers))
}

/// Generate the delegate header file. Returns the number of delegates generated.
fn generate_delegate_header(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
) -> KainResult<usize> {
    let mut delegate_header_content = String::new();
    let mut delegate_count = 0;
    let mut delegate_type_dependencies: std::collections::HashSet<String> = std::collections::HashSet::new();
    
    // Create TypeMapper with registered types for correct prefix detection
    let mut type_mapper = ue5::ue5::types::TypeMapper::new();
    
    // Register all types in the program so TypeMapper can apply correct prefixes
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Enum(e) => {
                type_mapper.register_enum(e.ast.name.clone());
            }
            kain_core::types::TypedItem::Struct(s) => {
                if s.ast.attributes.iter().any(|a| a.name == "component") {
                    type_mapper.register_component(s.ast.name.clone());
                } else {
                    type_mapper.register_struct(s.ast.name.clone());
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                type_mapper.register_actor(a.ast.name.clone());
            }
            kain_core::types::TypedItem::Component(c) => {
                type_mapper.register_component(c.ast.name.clone());
            }
            kain_core::types::TypedItem::TypeAlias(alias) => {
                if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                    type_mapper.register_delegate(alias.ast.name.clone());
                }
            }
            _ => {}
        }
    }
    
    // Collect all delegate declarations and their type dependencies
    for item in &program.items {
        if let kain_core::types::TypedItem::TypeAlias(alias) = item {
            // Check if this is a delegate (function type)
            if let kain_core::ast::Type::Function { params, .. } = &alias.ast.target {
                let delegate_name = format!("F{}", alias.ast.name);
                
                // Helper function to map KAIN types to UE5 types using TypeMapper
                let mut map_type = |ty: &kain_core::ast::Type| -> String {
                    let mapped = type_mapper.map_type_string(ty);
                    
                    // Track header dependencies for user-defined types
                    if let kain_core::ast::Type::Named { name, .. } = ty {
                        // Check if it's a user-defined type that needs a header
                        if type_mapper.needs_forward_decl(ty) {
                            delegate_type_dependencies.insert(format!("{}.h", mapped.trim_end_matches('*')));
                        }
                    }
                    
                    mapped
                };
                
                // Generate delegate declaration based on parameter count
                let delegate_decl = if params.is_empty() {
                    format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE({});", delegate_name)
                } else if params.len() == 1 {
                    let param_type = map_type(&params[0]);
                    format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam({}, {}, Param0);", 
                        delegate_name, param_type)
                } else if params.len() == 2 {
                    let param1_type = map_type(&params[0]);
                    let param2_type = map_type(&params[1]);
                    format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams({}, {}, Param0, {}, Param1);", 
                        delegate_name, param1_type, param2_type)
                } else if params.len() == 3 {
                    let param1_type = map_type(&params[0]);
                    let param2_type = map_type(&params[1]);
                    let param3_type = map_type(&params[2]);
                    format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams({}, {}, Param0, {}, Param1, {}, Param2);", 
                        delegate_name, param1_type, param2_type, param3_type)
                } else if params.len() <= 9 {
                    // UE5 supports up to 9 parameters
                    let param_types: Vec<String> = params.iter().map(|p| map_type(p)).collect();
                    let param_list: Vec<String> = param_types.iter().enumerate()
                        .map(|(i, ty)| format!("{}, Param{}", ty, i))
                        .collect();
                    let macro_name = match params.len() {
                        4 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams",
                        5 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams",
                        6 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams",
                        7 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_SevenParams",
                        8 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams",
                        9 => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_NineParams",
                        _ => unreachable!(),
                    };
                    format!("{}({}, {});", macro_name, delegate_name, param_list.join(", "))
                } else {
                    format!("// ERROR: Delegate {} has {} parameters - UE5 supports up to 9", 
                        delegate_name, params.len())
                };
                
                delegate_header_content.push_str(&delegate_decl);
                delegate_header_content.push('\n');
                delegate_header_content.push('\n');
                delegate_count += 1;
            }
        }
    }
    
    // Generate complete delegate header file if we have any delegates
    if delegate_count > 0 {
        let mut full_delegate_header = String::new();
        full_delegate_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
            chrono::Utc::now().year(),
            config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
        full_delegate_header.push_str("// Generated by KAIN-PRO - Delegate Declarations\n");
        full_delegate_header.push_str("// This file contains ONLY delegate declarations to avoid circular dependencies\n\n");
        full_delegate_header.push_str("#pragma once\n\n");
        full_delegate_header.push_str("#include \"CoreMinimal.h\"\n");
        
        // Include type dependencies (enums, structs that delegates reference)
        let mut sorted_deps: Vec<String> = delegate_type_dependencies.into_iter().collect();
        sorted_deps.sort();
        for dep in &sorted_deps {
            full_delegate_header.push_str(&format!("#include \"{}\"\n", dep));
        }
        full_delegate_header.push_str("\n");
        
        // CRITICAL: .generated.h MUST come before GENERATED_BODY() and delegate macros
        full_delegate_header.push_str(&format!("#include \"{}Delegates.generated.h\"\n", config.plugin_name));
        full_delegate_header.push_str("\n");
        
        // Generate complete delegate header with ACTUAL declarations
        full_delegate_header.push_str("// Delegate declarations\n");
        full_delegate_header.push_str(&delegate_header_content);
        
        // Dummy USTRUCT to force UHT to process this header and generate .generated.h
        full_delegate_header.push_str("// Internal struct for UHT processing\n");
        full_delegate_header.push_str("USTRUCT()\n");
        full_delegate_header.push_str(&format!("struct F{}Delegates_Internal\n", config.plugin_name));
        full_delegate_header.push_str("{\n");
        full_delegate_header.push_str("    GENERATED_BODY()\n");
        full_delegate_header.push_str("};\n");
        
        // Write delegate header file
        let delegate_header_path = layout.public_dir.join(format!("{}Delegates.h", config.plugin_name));
        fs::write(&delegate_header_path, full_delegate_header).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}Delegates.h ({} delegate declarations - ARCHITECTURAL IMPROVEMENT!)", config.plugin_name, delegate_count);
    }
    
    Ok(delegate_count)
}

/// Generate the EditorTypes header that provides all runtime types + delegates for editor code.
fn generate_editor_types_header(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    delegate_count: usize,
) -> KainResult<()> {
    let mut editor_types_header = String::new();
    editor_types_header.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
        chrono::Utc::now().year(),
        config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
    editor_types_header.push_str("// Generated by KAIN-PRO - Editor Types Header\n");
    editor_types_header.push_str("// This file provides ALL runtime types + delegates for editor code\n");
    editor_types_header.push_str("// Slate widgets, Details customizations, and Viewports should include this\n\n");
    editor_types_header.push_str("#pragma once\n\n");
    editor_types_header.push_str("#include \"CoreMinimal.h\"\n\n");
    
    // Include all runtime type headers (enums, structs, actors, components)
    editor_types_header.push_str("// Runtime types (enums, structs, actors, components)\n");
    for item in &program.items {
        // Skip editor-only items
        if let kain_core::types::TypedItem::Struct(s) = item {
            let is_editor_struct = s.ast.attributes.iter().any(|a| 
                ue5_editor::is_editor_attribute(&a.name)
            );
            if is_editor_struct {
                continue;
            }
        }
        
        // Skip all type aliases (delegates are in delegate header, type mappings have no headers)
        if matches!(item, kain_core::types::TypedItem::TypeAlias(_)) {
            continue;
        }
        
        let header_name = match item {
            kain_core::types::TypedItem::Actor(a) => Some(format!("{}.h", ue5::naming::to_actor_name(&a.ast.name))),
            kain_core::types::TypedItem::Component(c) => Some(format!("{}.h", ue5::naming::to_component_name(&c.ast.name))),
            kain_core::types::TypedItem::Struct(s) => Some(format!("{}.h", ue5::naming::to_struct_name(&s.ast.name))),
            kain_core::types::TypedItem::Enum(e) => Some(format!("{}.h", ue5::naming::to_enum_name(&e.ast.name))),
            _ => None,
        };
        
        if let Some(header) = header_name {
            editor_types_header.push_str(&format!("#include \"{}\"\n", header));
        }
    }
    editor_types_header.push_str("\n");
    
    // Include delegates if we have any
    if delegate_count > 0 {
        editor_types_header.push_str("// Delegates\n");
        editor_types_header.push_str(&format!("#include \"{}Delegates.h\"\n", config.plugin_name));
        editor_types_header.push_str("\n");
    }
    
    // Forward declare all Slate widgets to prevent circular dependencies
    editor_types_header.push_str("// Forward declarations for Slate widgets (prevents circular dependencies)\n");
    for item in &program.items {
        if let kain_core::types::TypedItem::Struct(s) = item {
            if s.ast.attributes.iter().any(|a| a.name == "slate") {
                let widget_name = format!("S{}", s.ast.name);
                editor_types_header.push_str(&format!("class {};\n", widget_name));
            }
        }
    }
    editor_types_header.push_str("\n");
    
    // Write EditorTypes header file
    let editor_types_path = layout.public_dir.join(format!("{}EditorTypes.h", config.plugin_name));
    fs::write(&editor_types_path, editor_types_header).map_err(|e| KainError::Io(e))?;
    println!("      ✓ {}EditorTypes.h (complete type definitions for editor code - OPTION 3!)", config.plugin_name);
    
    Ok(())
}

/// Generate per-item runtime codegen files (actors, structs, enums, components).
pub fn generate_runtime_items(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    shader_names: &[String],
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
    symbol_source_map: &HashMap<String, PathBuf>,
    embed_kain: bool,
) -> KainResult<SymbolRoutingManifest> {
    let mut routing_manifest = SymbolRoutingManifest::default();
    for item in &program.items {
        // Skip editor-only structs (handled by ue5-editor crate)
        if let kain_core::types::TypedItem::Struct(s) = item {
            let is_editor_struct = s.ast.attributes.iter().any(|a| 
                ue5_editor::is_editor_attribute(&a.name)
            );
            if is_editor_struct {
                continue; // Skip - will be generated by editor codegen
            }
        }
        
        // Skip ALL type aliases — delegates are in the delegate header, and non-delegate
        // type aliases (e.g. `type Vec2 = vec2`) are pure codegen-time type mappings that
        // resolve to built-in UE5 types like FVector2D. They produce no UHT-annotated content
        // (no USTRUCT/GENERATED_BODY), so UHT never creates a .generated.h for them, causing
        // fatal C1083 "Cannot open include file" errors.
        if matches!(item, kain_core::types::TypedItem::TypeAlias(_)) {
            continue;
        }

        let is_state_machine = matches!(item, kain_core::types::TypedItem::StateMachine(_));
        let is_async_task = matches!(item, kain_core::types::TypedItem::AsyncTask(_));
        
        let (item_name, output_name) = match item {
            kain_core::types::TypedItem::Actor(a) => (&a.ast.name, ue5::naming::to_actor_name(&a.ast.name)),
            kain_core::types::TypedItem::Component(c) => (&c.ast.name, ue5::naming::to_component_name(&c.ast.name)),
            kain_core::types::TypedItem::Struct(s) => (&s.ast.name, ue5::naming::to_struct_name(&s.ast.name)),
            kain_core::types::TypedItem::Enum(e) => (&e.ast.name, ue5::naming::to_enum_name(&e.ast.name)),
            kain_core::types::TypedItem::StateMachine(sm) => (&sm.name, sm.name.clone()),
            kain_core::types::TypedItem::AsyncTask(at) => (&at.name, at.name.clone()),
            _ => continue,
        };

        let bucket = match item {
            kain_core::types::TypedItem::Actor(_) => "actors",
            kain_core::types::TypedItem::Component(_) => "components",
            kain_core::types::TypedItem::Struct(_) => "structs",
            kain_core::types::TypedItem::Enum(_) => "enums",
            kain_core::types::TypedItem::StateMachine(_) => "state_machines",
            kain_core::types::TypedItem::AsyncTask(_) => "async_tasks",
            _ => "runtime",
        };
        let route = item_route(layout, config, item_name, false, bucket, symbol_source_map);
        fs::create_dir_all(&route.public_dir).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&route.private_dir).map_err(|e| KainError::Io(e))?;

        println!("   📄 Slicing item: {} → {}.h/cpp", item_name, output_name);

        // Generate filtered output for this specific item using the FULL program shared state and type map
        match ue5::generate_filtered_typed(program, &config.plugin_name, Some(&output_name), Some(item_name.clone()), config.copyright.as_deref(), type_headers.clone(), Some(shader_names.to_vec()), embed_kain) {
            Ok(ue5_output) => {
                if is_state_machine || is_async_task {
                    let expected_files: Vec<String> = if is_state_machine {
                        vec![
                            format!("{}.h", output_name),
                            format!("{}.cpp", output_name),
                        ]
                    } else {
                        vec![
                            format!("{}.h", output_name),
                            format!("{}.cpp", output_name),
                            format!("{}TaskQueue.h", output_name),
                            format!("{}TaskQueue.cpp", output_name),
                        ]
                    };

                    let mut wrote_count = 0usize;
                    let mut include_lines = Vec::new();
                    for expected in &expected_files {
                        if let Some((filename, content)) = ue5_output
                            .shader_files
                            .iter()
                            .find(|(name, _)| name == expected)
                        {
                            let is_header = filename.ends_with(".h");
                            let output_path = if is_header {
                                route.public_dir.join(filename)
                            } else {
                                route.private_dir.join(filename)
                            };
                            fs::write(&output_path, content).map_err(|e| KainError::Io(e))?;
                            println!("      ✓ {}", filename);
                            wrote_count += 1;

                            if is_header {
                                include_lines.push(format!(
                                    "#include \"{}\"\n",
                                    build_include_path(&route, filename, Some(&config.plugin_name))
                                ));
                            }
                        }
                    }

                    if wrote_count == 0 {
                        eprintln!(
                            "      ✗ Failed to generate sidecar artifacts for {} (no expected files found)",
                            item_name
                        );
                    } else {
                        let mut master = fs::read_to_string(master_header_path).map_err(|e| KainError::Io(e))?;
                        for include_line in include_lines {
                            if !master.contains(&include_line) {
                                master.push_str(&include_line);
                            }
                        }
                        fs::write(master_header_path, master).map_err(|e| KainError::Io(e))?;
                    }

                    continue;
                }

                // Write header
                let header_filename = format!("{}.h", output_name);
                let header_path = route.public_dir.join(&header_filename);
                fs::write(&header_path, &ue5_output.header).map_err(|e| KainError::Io(e))?;
                println!("      ✓ {}.h", output_name);
                let include_path = build_include_path(&route, &header_filename, Some(&config.plugin_name));
                routing_manifest.register(item_name, &route.module_name, &include_path);
                
                // Only write .cpp if it has meaningful content (not just includes)
                let has_implementation = ue5_output.source.lines()
                    .any(|line| {
                        let trimmed = line.trim();
                        !trimmed.is_empty() && 
                        !trimmed.starts_with("//") && 
                        !trimmed.starts_with("#include")
                    });
                
                if has_implementation {
                    let cpp_path = route.private_dir.join(format!("{}.cpp", output_name));
                    fs::write(&cpp_path, &ue5_output.source).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ {}.cpp", output_name);
                } else {
                    println!("      ⊘ {}.cpp (skipped - no implementation needed)", output_name);
                }
                
                // Append this item's include to master header
                let mut master = fs::read_to_string(master_header_path).map_err(|e| KainError::Io(e))?;
                master.push_str(&format!("#include \"{}\"\n", include_path));
                fs::write(master_header_path, master).map_err(|e| KainError::Io(e))?;
            }
            Err(e) => {
                eprintln!("      ✗ Failed to generate {}: {}", output_name, e);
            }
        }
    }
    Ok(routing_manifest)
}

/// Generate stdlib functions header if any exist.
pub fn generate_stdlib_functions(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
) -> KainResult<()> {
    println!("   📦 Generating stdlib functions header...");
    match ue5::generate_stdlib_functions(program, &config.plugin_name, config.copyright.as_deref(), type_headers.clone()) {
        Ok(stdlib_output) => {
            // Check if there are any functions in the output (look for "static inline" which indicates functions)
            if stdlib_output.header.contains("static inline") {
                let stdlib_header_path = layout.public_dir.join("KainStdlib.h");
                fs::write(&stdlib_header_path, &stdlib_output.header).map_err(|e| KainError::Io(e))?;
                println!("      ✓ KainStdlib.h (stdlib utility functions)");
                
                // Add include to master header.
                // Keep this after module includes to avoid circular include-order issues
                // when stdlib helpers reference generated runtime types.
                let mut master = fs::read_to_string(master_header_path).map_err(|e| KainError::Io(e))?;
                if !master.contains("#include \"KainStdlib.h\"") {
                    master.push_str("\n// Stdlib functions\n#include \"KainStdlib.h\"\n");
                }
                fs::write(master_header_path, master).map_err(|e| KainError::Io(e))?;
            } else {
                println!("      ℹ️  No stdlib functions to generate (skipped)");
            }
        }
        Err(e) => {
            eprintln!("      ✗ Failed to generate stdlib functions: {}", e);
        }
    }
    Ok(())
}

/// Generate blueprint function library if any @blueprint functions exist.
pub fn generate_blueprint_library(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    type_headers: &HashMap<String, String>,
    master_header_path: &PathBuf,
) -> KainResult<()> {
    let has_blueprint_funcs = program.items.iter().any(|item| {
        if let kain_core::types::TypedItem::Function(f) = item {
            f.ast.attributes.iter().any(|a| a.name == "blueprint" || a.name == "ue5")
        } else {
            false
        }
    });
    
    if has_blueprint_funcs {
        println!("   📦 Generating blueprint function library...");
        // Generate blueprint functions with special target to skip type definitions
        let bp_lib_name = format!("{}BlueprintLibrary", config.plugin_name);
        match ue5::generate_filtered_typed(program, &config.plugin_name, Some(&bp_lib_name), Some("__BLUEPRINT_LIBRARY_ONLY__".to_string()), config.copyright.as_deref(), type_headers.clone(), None, false) {
            Ok(bp_output) => {
                let bp_header_path = layout.public_dir.join(format!("{}.h", bp_lib_name));
                fs::write(&bp_header_path, &bp_output.header).map_err(|e| KainError::Io(e))?;
                println!("      ✓ {}.h", bp_lib_name);
                
                let bp_cpp_path = layout.private_dir.join(format!("{}.cpp", bp_lib_name));
                fs::write(&bp_cpp_path, &bp_output.source).map_err(|e| KainError::Io(e))?;
                println!("      ✓ {}.cpp", bp_lib_name);
                
                // Add include to master header
                let mut master = fs::read_to_string(master_header_path).map_err(|e| KainError::Io(e))?;
                master.push_str(&format!("#include \"{}.h\"\n", bp_lib_name));
                fs::write(master_header_path, master).map_err(|e| KainError::Io(e))?;
            }
            Err(e) => {
                eprintln!("      ✗ Failed to generate blueprint library: {}", e);
            }
        }
    }
    Ok(())
}

/// Generate editor tools (Slate UI, Details, Viewport, Toolbar...).
pub fn generate_editor_items(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    master_header_path: &PathBuf,
    symbol_source_map: &HashMap<String, PathBuf>,
) -> KainResult<SymbolRoutingManifest> {
    if !layout.has_editor_items {
        println!("   ℹ️  No editor items detected - skipping editor codegen");
        return Ok(SymbolRoutingManifest::default());
    }
    let mut routing_manifest = SymbolRoutingManifest::default();
    
    println!("   🎨 Generating editor tools (Slate UI, Details, Viewport, Toolbar...)...");
    
    // Determine where editor files go based on split mode
    let (ed_pub_dir, ed_priv_dir) = if layout.needs_split {
        // Split mode: editor files go to separate module directory
        let ed_pub = layout.editor_public_dir.as_ref()
            .ok_or_else(|| KainError::codegen_error("Editor public directory not set in split mode"))?;
        let ed_priv = layout.editor_private_dir.as_ref()
            .ok_or_else(|| KainError::codegen_error("Editor private directory not set in split mode"))?;
        (ed_pub.clone(), ed_priv.clone()) // No prefix — files are at root of editor module
    } else {
        // Single module: editor files go to Editor/ subdirectory
        let ed_pub = layout.public_dir.join("Editor");
        let ed_priv = layout.private_dir.join("Editor");
        fs::create_dir_all(&ed_pub).map_err(|e| KainError::Io(e))?;
        fs::create_dir_all(&ed_priv).map_err(|e| KainError::Io(e))?;
        (ed_pub, ed_priv)
    };
    
    // In split mode, generate a master header for the editor module
    let editor_master_header_path = if layout.needs_split {
        let ed_pub = layout.editor_public_dir.as_ref()
            .ok_or_else(|| KainError::codegen_error("Editor public directory not set in split mode"))?;
        let editor_module_name = format!("{}Editor", config.plugin_name);
        let mut ed_master = String::new();
        ed_master.push_str(&format!("// Copyright {} {}. All Rights Reserved.\n", 
            chrono::Utc::now().year(),
            config.copyright.as_deref().unwrap_or("Epic Games, Inc.")));
        ed_master.push_str("// Generated by KAIN-PRO - Editor Module Master Header\n");
        ed_master.push_str("#pragma once\n\n");
        ed_master.push_str("#include \"CoreMinimal.h\"\n");
        // Include the runtime module's master header for type access
        ed_master.push_str(&format!("#include \"{}.h\"\n\n", config.plugin_name));
        ed_master.push_str("// Editor module includes\n");
        let path = ed_pub.join(format!("{}.h", editor_module_name));
        fs::write(&path, &ed_master).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}.h (editor module master header)", editor_module_name);
        Some(path)
    } else {
        None
    };
    
    // Generate per-item editor files (modular output).
    // First, collect what will be generated so we can clean up stale files.
    match ue5_editor::generate_per_item(program, &config.plugin_name, config.copyright.as_deref()) {
        Ok(editor_items) => {
            // Collect the set of expected output file names (without extension)
            let expected_names: std::collections::HashSet<String> = editor_items.iter()
                .map(|item| item.name.clone())
                .collect();
            
            // Clean up stale .h files in editor public dir
            if let Ok(entries) = fs::read_dir(&ed_pub_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().map_or(false, |e| e == "h") {
                        if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                            // Keep the editor master header, factory files, and any non-generated files
                            let is_master = stem == format!("{}Editor", config.plugin_name);
                            let is_factory = stem.ends_with("Factory");
                            if !is_master && !is_factory && !expected_names.contains(stem) {
                                let _ = fs::remove_file(&path);
                                println!("   🧹 Removed stale {}", path.file_name().unwrap_or_default().to_string_lossy());
                            }
                        }
                    }
                }
            }
            // Clean up stale .cpp files in editor private dir
            if let Ok(entries) = fs::read_dir(&ed_priv_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().map_or(false, |e| e == "cpp") {
                        if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                            let is_factory = stem.ends_with("Factory");
                            if !is_factory && !expected_names.contains(stem) {
                                let _ = fs::remove_file(&path);
                                println!("   🧹 Removed stale {}", path.file_name().unwrap_or_default().to_string_lossy());
                            }
                        }
                    }
                }
            }

            for editor_item in &editor_items {
                println!("   📄 Editor item: {} [{}] → {}.h/cpp", editor_item.name, editor_item.kind, editor_item.name);
                
                let route = item_route(
                    layout,
                    config,
                    &editor_item.name,
                    true,
                    "editor",
                    symbol_source_map,
                );
                fs::create_dir_all(&route.public_dir).map_err(|e| KainError::Io(e))?;
                fs::create_dir_all(&route.private_dir).map_err(|e| KainError::Io(e))?;

                // Write header
                let header_filename = format!("{}.h", editor_item.name);
                let header_path = route.public_dir.join(&header_filename);
                fs::write(&header_path, &editor_item.header).map_err(|e| KainError::Io(e))?;
                println!("      ✓ {}.h", editor_item.name);
                let editor_consumer_module = if layout.needs_split {
                    format!("{}Editor", config.plugin_name)
                } else {
                    config.plugin_name.clone()
                };
                let include_path = build_include_path(
                    &route,
                    &header_filename,
                    Some(&editor_consumer_module),
                );
                routing_manifest.register(&editor_item.name, &route.module_name, &include_path);
                
                // Only write .cpp if it has meaningful content (not just includes)
                let has_implementation = editor_item.source.lines()
                    .any(|line| {
                        let trimmed = line.trim();
                        !trimmed.is_empty() && 
                        !trimmed.starts_with("//") && 
                        !trimmed.starts_with("#include") &&
                        !trimmed.starts_with("BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION") &&
                        !trimmed.starts_with("END_SLATE_FUNCTION_BUILD_OPTIMIZATION") &&
                        trimmed != "{"  &&
                        trimmed != "}" &&
                        trimmed != "];" &&
                        trimmed != "[" &&
                        !trimmed.starts_with("ChildSlot") &&
                        !trimmed.starts_with("SNew(S") ||
                        (trimmed.starts_with("SNew(S") && trimmed.contains("+"))  // Has actual slots
                    });
                
                if has_implementation {
                    let cpp_path = route.private_dir.join(format!("{}.cpp", editor_item.name));
                    fs::write(&cpp_path, &editor_item.source).map_err(|e| KainError::Io(e))?;
                    println!("      ✓ {}.cpp", editor_item.name);
                } else {
                    println!("      ⊘ {}.cpp (skipped - no implementation needed)", editor_item.name);
                }
                
                // Add include to appropriate master header
                if let Some(ref ed_master_path) = editor_master_header_path {
                    // Split mode: add to editor module master header (no prefix)
                    let mut master = fs::read_to_string(ed_master_path).map_err(|e| KainError::Io(e))?;
                    master.push_str(&format!("#include \"{}\"\n", include_path));
                    fs::write(ed_master_path, master).map_err(|e| KainError::Io(e))?;
                } else {
                    // Single module: add to runtime master header with Editor/ prefix
                    let mut master = fs::read_to_string(master_header_path).map_err(|e| KainError::Io(e))?;
                    master.push_str(&format!("#include \"{}\"\n", include_path));
                    fs::write(master_header_path, master).map_err(|e| KainError::Io(e))?;
                }
            }
            println!("   ✅ {} editor items generated", editor_items.len());
        }
        Err(e) => {
            eprintln!("      ✗ Failed to generate editor tools: {}", e);
        }
    }
    
    Ok(routing_manifest)
}

#[derive(Debug, Serialize)]
struct SymbolManifestEntry {
    symbol: String,
    module: String,
    include: String,
}

#[derive(Debug, Serialize)]
struct SymbolManifestFile {
    plugin: String,
    symbols: Vec<SymbolManifestEntry>,
}

#[derive(Debug, Serialize)]
struct ModuleIncludeManifestFile {
    plugin: String,
    module_headers: HashMap<String, Vec<String>>,
}

pub fn write_cross_module_manifests(
    layout: &PluginLayout,
    config: &Ue5Config,
    runtime_manifest: &SymbolRoutingManifest,
    editor_manifest: &SymbolRoutingManifest,
) -> KainResult<()> {
    let mut merged = SymbolRoutingManifest::default();
    merged.extend_from(runtime_manifest);
    merged.extend_from(editor_manifest);

    let mut symbols: Vec<SymbolManifestEntry> = merged
        .symbol_owner
        .iter()
        .filter_map(|(symbol, module)| {
            let include = merged.symbol_header.get(symbol)?;
            Some(SymbolManifestEntry {
                symbol: symbol.clone(),
                module: module.clone(),
                include: include.clone(),
            })
        })
        .collect();
    symbols.sort_by(|a, b| a.symbol.cmp(&b.symbol));

    let mut module_headers: HashMap<String, Vec<String>> = HashMap::new();
    for entry in &symbols {
        module_headers
            .entry(entry.module.clone())
            .or_default()
            .push(entry.include.clone());
    }
    for headers in module_headers.values_mut() {
        headers.sort();
        headers.dedup();
    }

    let out_dir = layout.plugin_root.join("Intermediate").join("Kain");
    fs::create_dir_all(&out_dir).map_err(|e| KainError::Io(e))?;

    let symbol_file = SymbolManifestFile {
        plugin: config.plugin_name.clone(),
        symbols,
    };
    let include_file = ModuleIncludeManifestFile {
        plugin: config.plugin_name.clone(),
        module_headers,
    };

    let symbol_json = serde_json::to_string_pretty(&symbol_file)
        .map_err(|e| KainError::runtime(format!("Failed to serialize symbol manifest: {}", e)))?;
    let include_json = serde_json::to_string_pretty(&include_file)
        .map_err(|e| KainError::runtime(format!("Failed to serialize include manifest: {}", e)))?;

    fs::write(out_dir.join("symbol_ownership_manifest.json"), symbol_json)
        .map_err(|e| KainError::Io(e))?;
    fs::write(out_dir.join("module_include_manifest.json"), include_json)
        .map_err(|e| KainError::Io(e))?;

    println!("   ✓ cross-module manifests emitted to {}/Intermediate/Kain", config.plugin_name);
    Ok(())
}

/// Generate module registration files (IMPLEMENT_MODULE).
pub fn generate_module_registration(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
    has_shaders: bool,
    has_material_factories: bool,
) -> KainResult<()> {
    // Detect if an @editor_module exists in the program.
    let has_editor_module = program.items.iter().any(|item| {
        if let kain_core::types::TypedItem::Struct(s) = item {
            s.ast.attributes.iter().any(|a| a.name == "editor_module")
        } else {
            false
        }
    });
    
    if layout.needs_split {
        // SPLIT MODE: Runtime module ALWAYS needs its own IMPLEMENT_MODULE
        eprintln!("📦 [PACKAGER] Split mode — generating runtime module registration");
        
        // Build includes based on features
        let mut includes = vec![
            format!("#include \"{}.h\"", config.plugin_name),
            "#include \"Modules/ModuleManager.h\"".to_string(),
        ];

        if has_material_factories {
            includes.push("#include \"Generated/MaterialFactories.h\"".to_string());
            includes.push("#include \"Misc/CoreDelegates.h\"".to_string());
        }
        
        if has_shaders {
            includes.push("#include \"Interfaces/IPluginManager.h\"".to_string());
            includes.push("#include \"ShaderCore.h\"".to_string());
        }
        
        let includes_str = includes.join("\n");
        
        // Build StartupModule body
        let mut startup_body = String::from("        // Runtime module startup\n");
        
        if has_shaders {
            startup_body.push_str(&format!(r#"
        // Register shader virtual path mapping
        // Maps /Plugin/{} to physical Shaders/ directory
        if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Plugin/{}")))
        {{
            FString PluginShaderDir = FPaths::Combine(
                IPluginManager::Get().FindPlugin(TEXT("{}"))->GetBaseDir(),
                TEXT("Shaders")
            );
            AddShaderSourceDirectoryMapping(TEXT("/Plugin/{}"), PluginShaderDir);
        }}
"#, config.plugin_name, config.plugin_name, config.plugin_name, config.plugin_name));
        }
        
        if has_material_factories {
            // Defer material generation until engine init has completed.
            // Running CreatePackage/NewObject directly in StartupModule can be too early
            // on some boots and may crash before UObject systems are fully ready.
            startup_body.push_str(&format!(r#"
        #if WITH_EDITOR
        // Generate editor materials once engine startup has completed.
        FCoreDelegates::OnPostEngineInit.AddStatic(&F{}MaterialFactory::GenerateMaterials);
        #endif
"#, config.plugin_name));
        }
        
        let module_cpp = format!(r#"// Generated by KAIN-PRO - Runtime Module Registration
{}

class F{}Module : public IModuleInterface
{{
public:
    virtual void StartupModule() override
    {{
{}    }}

    virtual void ShutdownModule() override
    {{
        // Runtime module shutdown
    }}
}};

IMPLEMENT_MODULE(F{}Module, {})
"#, includes_str, config.plugin_name, startup_body, config.plugin_name, config.plugin_name);
        
        let module_cpp_path = layout.private_dir.join(format!("{}.cpp", config.plugin_name));
        fs::write(&module_cpp_path, module_cpp).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}.cpp (runtime module registration)", config.plugin_name);
        
        // Editor module's IMPLEMENT_MODULE comes from @editor_module codegen
        if has_editor_module {
            println!("      ✓ @editor_module provides IMPLEMENT_MODULE for {}Editor", config.plugin_name);
        } else {
            // Generate a default editor module registration if no @editor_module exists
            let editor_module_name = format!("{}Editor", config.plugin_name);
            
            // Build includes based on features
            let mut includes = vec![
                format!("#include \"{}.h\"", editor_module_name),
                "#include \"Modules/ModuleManager.h\"".to_string(),
            ];
            
            if has_shaders {
                includes.push("#include \"Interfaces/IPluginManager.h\"".to_string());
                includes.push("#include \"ShaderCore.h\"".to_string());
            }
            
            let includes_str = includes.join("\n");
            
            // Build StartupModule body
            let mut startup_body = String::from("        // Editor module startup\n");
            
            if has_shaders {
                startup_body.push_str(&format!(r#"
        // Register shader virtual path mapping
        // Maps /Plugin/{} to physical Shaders/ directory
        if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Plugin/{}")))
        {{
            FString PluginShaderDir = FPaths::Combine(
                IPluginManager::Get().FindPlugin(TEXT("{}"))->GetBaseDir(),
                TEXT("Shaders")
            );
            AddShaderSourceDirectoryMapping(TEXT("/Plugin/{}"), PluginShaderDir);
        }}
"#, config.plugin_name, config.plugin_name, config.plugin_name, config.plugin_name));
            }
            
            let ed_module_cpp = format!(r#"// Generated by KAIN-PRO - Editor Module Registration
{}

class F{}Module : public IModuleInterface
{{
public:
    virtual void StartupModule() override
    {{
{}    }}

    virtual void ShutdownModule() override
    {{
        // Editor module shutdown
    }}
}};

IMPLEMENT_MODULE(F{}Module, {})
"#, includes_str, editor_module_name, startup_body, editor_module_name, editor_module_name);
            
            let ed_priv = layout.editor_private_dir.as_ref()
                .ok_or_else(|| KainError::codegen_error("Editor private directory not set in split mode"))?;
            let ed_module_cpp_path = ed_priv.join(format!("{}.cpp", editor_module_name));
            fs::write(&ed_module_cpp_path, ed_module_cpp).map_err(|e| KainError::Io(e))?;
            println!("      ✓ {}.cpp (editor module registration)", editor_module_name);
        }
    } else if has_editor_module {
        // SINGLE MODULE: @editor_module provides IMPLEMENT_MODULE
        eprintln!("📦 [PACKAGER] @editor_module detected — skipping default module registration");
        println!("   ℹ️  @editor_module provides IMPLEMENT_MODULE — skipping default {}.cpp", config.plugin_name);
    } else {
        // SINGLE MODULE: Generate default IMPLEMENT_MODULE
        eprintln!("📦 [PACKAGER] Generating module registration file...");
        
        // Build includes based on features
        let mut includes = vec![
            format!("#include \"{}.h\"", config.plugin_name),
            "#include \"Modules/ModuleManager.h\"".to_string(),
        ];

        if has_material_factories {
            includes.push("#include \"Generated/MaterialFactories.h\"".to_string());
            includes.push("#include \"Misc/CoreDelegates.h\"".to_string());
        }
        
        if has_shaders {
            includes.push("#include \"Interfaces/IPluginManager.h\"".to_string());
            includes.push("#include \"ShaderCore.h\"".to_string());
        }
        
        let includes_str = includes.join("\n");
        
        // Build StartupModule body
        let mut startup_body = String::from("        // Module startup\n");
        
        if has_shaders {
            startup_body.push_str(&format!(r#"
        // Register shader virtual path mapping
        // Maps /Plugin/{} to physical Shaders/ directory
        if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Plugin/{}")))
        {{
            FString PluginShaderDir = FPaths::Combine(
                IPluginManager::Get().FindPlugin(TEXT("{}"))->GetBaseDir(),
                TEXT("Shaders")
            );
            AddShaderSourceDirectoryMapping(TEXT("/Plugin/{}"), PluginShaderDir);
        }}
"#, config.plugin_name, config.plugin_name, config.plugin_name, config.plugin_name));
        }
        
        if has_material_factories {
            // Defer material generation until engine init has completed.
            // Running CreatePackage/NewObject directly in StartupModule can be too early
            // on some boots and may crash before UObject systems are fully ready.
            startup_body.push_str(&format!(r#"
        #if WITH_EDITOR
        // Generate editor materials once engine startup has completed.
        FCoreDelegates::OnPostEngineInit.AddStatic(&F{}MaterialFactory::GenerateMaterials);
        #endif
"#, config.plugin_name));
        }
        
        let module_cpp = format!(r#"// Generated by KAIN-PRO - Module Registration
{}

class F{}Module : public IModuleInterface
{{
public:
    virtual void StartupModule() override
    {{
{}    }}

    virtual void ShutdownModule() override
    {{
        // Module shutdown
    }}
}};

IMPLEMENT_MODULE(F{}Module, {})
"#, includes_str, config.plugin_name, startup_body, config.plugin_name, config.plugin_name);
        
        let module_cpp_path = layout.private_dir.join(format!("{}.cpp", config.plugin_name));
        fs::write(&module_cpp_path, module_cpp).map_err(|e| KainError::Io(e))?;
        println!("      ✓ {}.cpp (module registration)", config.plugin_name);
    }
    
    Ok(())
}

/// Generate monolithic output (single .h/.cpp with all types merged).
pub fn generate_monolithic(
    layout: &PluginLayout,
    config: &Ue5Config,
    program: &kain_core::types::TypedProgram,
) -> KainResult<()> {
    println!("🎯 Generating main plugin files from merged program...");
    
    match ue5::generate_from_typed(program, Some(&config.plugin_name), config.copyright.as_deref()) {
        Ok(ue5_output) => {
            // Write header
            let main_header_path = layout.public_dir.join(format!("{}.h", config.plugin_name));
            fs::write(&main_header_path, &ue5_output.header).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}.h (actors, structs, enums, components)", config.plugin_name);
            
            // Write source
            let main_cpp_path = layout.private_dir.join(format!("{}.cpp", config.plugin_name));
            fs::write(&main_cpp_path, &ue5_output.source).map_err(|e| KainError::Io(e))?;
            println!("   ✓ {}.cpp (implementations + module registration)", config.plugin_name);
        }
        Err(e) => {
            eprintln!("   ✗ Failed to generate UE5 code: {}", e);
        }
    }
    Ok(())
}

/// Extract all UE5 type names referenced in a KAIN program (base classes, field types, etc.).
/// These are used to resolve module dependencies via the module graph.
fn extract_referenced_types(program: &kain_core::types::TypedProgram) -> Vec<String> {
    let mut types: std::collections::HashSet<String> = std::collections::HashSet::new();
    
    // Built-in KAIN types that don't map to UE5 modules — skip these
    let builtins: std::collections::HashSet<&str> = [
        "Int", "Float", "Bool", "String", "Char",
        "Vec2", "Vec3", "Vec4", "Array", "Map", "Option", "Result",
        "Actor", "Component",
    ].iter().copied().collect();
    
    for item in &program.items {
        match item {
            kain_core::types::TypedItem::Struct(s) => {
                // Collect field types from struct
                for field in &s.ast.fields {
                    collect_ast_type_names(&field.ty, &builtins, &mut types);
                }
            }
            kain_core::types::TypedItem::Actor(a) => {
                // Collect state field types
                for state in &a.ast.state {
                    collect_ast_type_names(&state.ty, &builtins, &mut types);
                }
                // Collect method param/return types
                for method in &a.ast.methods {
                    for param in &method.params {
                        collect_ast_type_names(&param.ty, &builtins, &mut types);
                    }
                    if let Some(ref ret) = method.return_type {
                        collect_ast_type_names(ret, &builtins, &mut types);
                    }
                }
                // Collect handler param types
                for handler in &a.ast.handlers {
                    for param in &handler.params {
                        collect_ast_type_names(&param.ty, &builtins, &mut types);
                    }
                }
            }
            kain_core::types::TypedItem::Component(c) => {
                // Collect prop types (Component.props is Vec<Param>)
                for prop in &c.ast.props {
                    collect_ast_type_names(&prop.ty, &builtins, &mut types);
                }
            }
            kain_core::types::TypedItem::Function(f) => {
                for param in &f.ast.params {
                    collect_ast_type_names(&param.ty, &builtins, &mut types);
                }
                if let Some(ref ret) = f.ast.return_type {
                    collect_ast_type_names(ret, &builtins, &mut types);
                }
            }
            _ => {}
        }
    }
    
    types.into_iter().collect()
}

/// Recursively collect named type strings from a KAIN AST `Type`, skipping builtins.
fn collect_ast_type_names(
    ty: &kain_core::ast::Type,
    builtins: &std::collections::HashSet<&str>,
    out: &mut std::collections::HashSet<String>,
) {
    match ty {
        kain_core::ast::Type::Named { name, generics, .. } => {
            if !builtins.contains(name.as_str()) {
                out.insert(name.clone());
            }
            for g in generics {
                collect_ast_type_names(g, builtins, out);
            }
        }
        kain_core::ast::Type::Tuple(inner, _) => {
            for t in inner { collect_ast_type_names(t, builtins, out); }
        }
        kain_core::ast::Type::Array(inner, _, _) => collect_ast_type_names(inner, builtins, out),
        kain_core::ast::Type::Slice(inner, _) => collect_ast_type_names(inner, builtins, out),
        kain_core::ast::Type::Option(inner, _) => collect_ast_type_names(inner, builtins, out),
        kain_core::ast::Type::Result(ok, err, _) => {
            collect_ast_type_names(ok, builtins, out);
            collect_ast_type_names(err, builtins, out);
        }
        kain_core::ast::Type::Ref { inner, .. } => collect_ast_type_names(inner, builtins, out),
        _ => {}
    }
}

/// Compute the extra module dependencies needed for runtime code.
/// Uses the module graph when available, falls back to feature-based detection.
fn compute_runtime_deps(
    has_shaders: bool,
    has_gas_features: bool,
    module_graph: &ue5::ue5::module_graph::ModuleGraph,
    program: &kain_core::types::TypedProgram,
) -> Vec<String> {
    let base_modules = ["Core", "CoreUObject", "Engine", "Projects"];

    let mut deps = if module_graph.is_loaded() {
        // Data-driven: extract all types referenced in the program and resolve via graph
        let referenced = extract_referenced_types(program);
        let type_refs: Vec<&str> = referenced.iter().map(|s| s.as_str()).collect();

        let mut apis: Vec<&str> = Vec::new();
        if has_shaders {
            apis.push("AddShaderSourceDirectoryMapping");
            apis.push("AllShaderSourceDirectoryMappings");
            apis.push("IMPLEMENT_GLOBAL_SHADER");
        }

        module_graph.resolve_deps_for_types(&type_refs, &[], &apis, &base_modules)
    } else {
        // Fallback: feature-based (legacy behavior)
        Vec::new()
    };

    // Shader compilation always requires RHI (FGlobalShader) and Renderer
    // (IMPLEMENT_GLOBAL_SHADER). RenderCore is transitively pulled in by RHI,
    // but all three must be listed explicitly for UnrealBuildTool to pick them up.
    if has_shaders {
        for module in &["RenderCore", "RHI", "Renderer"] {
            let s = module.to_string();
            if !deps.contains(&s) {
                deps.push(s);
            }
        }
    }

    // GAS features require GameplayTags and GameplayAbilities modules
    if has_gas_features {
        for module in &["GameplayTags", "GameplayAbilities"] {
            let s = module.to_string();
            if !deps.contains(&s) {
                deps.push(s);
            }
        }
    }

    deps
}

/// Compute the extra module dependencies needed for editor code.
/// Uses the module graph when available, falls back to feature-based detection.
fn compute_editor_deps(
    has_shaders: bool,
    module_graph: &ue5::ue5::module_graph::ModuleGraph,
    program: &kain_core::types::TypedProgram,
) -> (Vec<String>, Vec<String>) {
    let base_modules = ["Core", "CoreUObject", "Engine"];
    
    if module_graph.is_loaded() {
        // Data-driven: extract all types referenced in the program and resolve via graph
        let referenced = extract_referenced_types(program);
        let type_refs: Vec<&str> = referenced.iter().map(|s| s.as_str()).collect();
        
        let mut apis: Vec<&str> = Vec::new();
        if has_shaders {
            apis.push("AddShaderSourceDirectoryMapping");
            apis.push("AllShaderSourceDirectoryMappings");
        }
        
        // Editor module gets the same type resolution — editor items reference runtime types too
        let public_extra = module_graph.resolve_deps_for_types(&type_refs, &[], &apis, &base_modules);
        (public_extra, Vec::new())
    } else {
        // Fallback: feature-based
        let mut public_extra = Vec::new();
        if has_shaders {
            public_extra.extend(["RenderCore", "RHI", "Renderer"].iter().map(|s| s.to_string()));
        }
        (public_extra, Vec::new())
    }
}

/// Write .uplugin and .Build.cs files.
pub fn write_plugin_files(
    layout: &PluginLayout,
    config: &Ue5Config,
    description: &Option<String>,
    has_shaders: bool,
    has_gas_features: bool,
    module_graph: &ue5::ue5::module_graph::ModuleGraph,
    program: &kain_core::types::TypedProgram,
) -> KainResult<()> {
    // Detect program features needed for correct Build.cs generation
    let has_datatable = super::build_cs_gen::has_datatable_structs(program);
    let has_viewport = super::build_cs_gen::has_viewport_items(program);
    let has_toolbar = super::build_cs_gen::has_toolbar_items(program);
    // Generate .uplugin file (ALWAYS regenerate to ensure it's up-to-date)
    let uplugin_path = layout.plugin_root.join(format!("{}.uplugin", config.plugin_name));
    println!();
    println!("📦 Generating .uplugin file...");
    let uplugin_content = if config.has_module_plan() {
        super::uplugin_gen::generate_uplugin_file_from_modules(
            &config.plugin_name,
            description,
            has_shaders,
            &config.modules,
            &config.plugin_dependencies,
        )
    } else {
        super::uplugin_gen::generate_uplugin_file(
            &config.plugin_name,
            description,
            layout.has_editor_items,
            layout.needs_split,
            has_shaders,
            &config.plugin_dependencies,
        )
    };
    fs::write(&uplugin_path, uplugin_content).map_err(|e| KainError::Io(e))?;
    println!("   ✓ {}.uplugin", config.plugin_name);
    if has_shaders {
        println!("   ℹ️  CanContainContent: true (required for shader loading)");
    }
    
    if module_graph.is_loaded() {
        let (mods, types, headers) = module_graph.stats();
        println!("   📊 Module graph: {} modules, {} types, {} headers", mods, types, headers);
    }
    
    // Generate .Build.cs file(s)
    println!();
    println!("🔨 Generating .Build.cs file(s)...");

    if config.has_module_plan() {
        for module in &config.modules {
            let (mut module_public_deps, module_private_deps): (Vec<String>, Vec<String>) = match module.module_type {
                super::config::Ue5ModuleType::Runtime => {
                    let mut deps = compute_runtime_deps(has_shaders, module_graph, program);
                    if !deps.contains(&"Core".to_string()) { deps.push("Core".to_string()); }
                    if !deps.contains(&"CoreUObject".to_string()) { deps.push("CoreUObject".to_string()); }
                    if !deps.contains(&"Engine".to_string()) { deps.push("Engine".to_string()); }
                    if !deps.contains(&"Projects".to_string()) { deps.push("Projects".to_string()); }
                    (deps, Vec::new())
                }
                _ => {
                    let (pub_extra, priv_extra) = compute_editor_deps(has_shaders, module_graph, program);
                    (pub_extra, priv_extra)
                }
            };

            // Merge explicit per-module deps from KAIN.toml.
            for dep in &module.public_deps {
                if !module_public_deps.contains(dep) {
                    module_public_deps.push(dep.clone());
                }
            }
            let mut merged_private = module_private_deps;
            for dep in &module.private_deps {
                if !merged_private.contains(dep) {
                    merged_private.push(dep.clone());
                }
            }

            // Auto-wire inter-module dependencies declared in depends_on.
            for dep in &module.depends_on {
                if !module_public_deps.contains(dep) {
                    module_public_deps.push(dep.clone());
                }
            }

            let module_dir = layout.source_dir.join(&module.name);
            fs::create_dir_all(&module_dir).map_err(|e| KainError::Io(e))?;
            let build_cs_path = module_dir.join(format!("{}.Build.cs", module.name));
            let build_cs_content = super::build_cs_gen::generate_build_cs_module(
                &module.name,
                &module_public_deps,
                &merged_private,
            );
            fs::write(&build_cs_path, build_cs_content).map_err(|e| KainError::Io(e))?;

            println!(
                "   ✓ {}.Build.cs (data-driven module: {}, type={})",
                module.name,
                module.name,
                module.module_type.as_uplugin_type()
            );
        }

        return Ok(());
    }
    
    if layout.needs_split {
        // SPLIT MODE: Two separate .Build.cs files
        let rt_extra = compute_runtime_deps(has_shaders, has_gas_features, module_graph, program);
        let rt_build_cs_path = layout.source_dir.join(&config.plugin_name).join(format!("{}.Build.cs", config.plugin_name));
        let rt_build_cs = super::build_cs_gen::generate_build_cs_runtime(&config.plugin_name, &rt_extra, has_datatable);
        fs::write(&rt_build_cs_path, rt_build_cs).map_err(|e| KainError::Io(e))?;
        if !rt_extra.is_empty() {
            println!("   ✓ {}.Build.cs (runtime) + auto-resolved: {}", config.plugin_name, rt_extra.join(", "));
        } else {
            println!("   ✓ {}.Build.cs (runtime module)", config.plugin_name);
        }
        
        let (ed_pub_extra, ed_priv_extra) = compute_editor_deps(has_shaders, module_graph, program);
        let editor_module_name = format!("{}Editor", config.plugin_name);
        let ed_build_cs_path = layout.source_dir.join(&editor_module_name).join(format!("{}.Build.cs", editor_module_name));
        let ed_build_cs = super::build_cs_gen::generate_build_cs_editor(&config.plugin_name, &ed_pub_extra, &ed_priv_extra, has_viewport, has_toolbar);
        fs::write(&ed_build_cs_path, ed_build_cs).map_err(|e| KainError::Io(e))?;
        if !ed_pub_extra.is_empty() {
            println!("   ✓ {}.Build.cs (editor) + auto-resolved: {}", editor_module_name, ed_pub_extra.join(", "));
        } else {
            println!("   ✓ {}.Build.cs (editor module)", editor_module_name);
        }
    } else {
        // SINGLE MODULE: One .Build.cs
        let rt_extra = compute_runtime_deps(has_shaders, has_gas_features, module_graph, program);
        let build_cs_path = layout.source_dir.join(format!("{}.Build.cs", config.plugin_name));
        let build_cs_content = super::build_cs_gen::generate_build_cs(&config.plugin_name, layout.has_editor_items, &rt_extra, &[]);
        fs::write(&build_cs_path, build_cs_content).map_err(|e| KainError::Io(e))?;
        if !rt_extra.is_empty() {
            println!("   ✓ {}.Build.cs + auto-resolved: {}", config.plugin_name, rt_extra.join(", "));
        } else {
            println!("   ✓ {}.Build.cs", config.plugin_name);
        }
    }
    
    Ok(())
}
