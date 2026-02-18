use std::fs;
use std::path::PathBuf;
use crate::error::{KainError, KainResult};
use super::config::Ue5Config;
use super::post_process;

/// Build UE5 plugin from KAIN.toml configuration
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = super::load_manifest(&cwd)?;
    
    let ue5_config = manifest.ue5.as_ref()
        .ok_or_else(|| KainError::runtime("No [ue5] section in KAIN.toml"))?;
    
    println!("🚀 Building UE5 Plugin: {}", ue5_config.plugin_name);
    println!(" Plugin directory: {}", ue5_config.plugin_dir.display());
    println!();
    
    // STEP 1: Load and parse source files
    let (typed_program, all_shader_names, stdlib_files, user_source_files) = 
        load_and_parse_sources(ue5_config, &manifest, &cwd)?;
    
    // STEP 2: Setup plugin directory structure
    let layout = super::plugin_layout::setup(ue5_config, &cwd, &typed_program, !all_shader_names.is_empty())?;
    
    // Use shader names from config or auto-detected
    let shader_names = if ue5_config.shaders.is_empty() {
        all_shader_names
    } else {
        ue5_config.shaders.clone()
    };
    
    // STEP 3: Compile shaders (optional)
    super::codegen::compile_shaders(&layout, ue5_config, &typed_program, &shader_names)?;
    
    // STEP 4: Generate main plugin files
    println!();
    
    if ue5_config.modular_output {
        // MODULAR MODE: Generate separate .h/.cpp for each source file
        println!("🎯 Generating modular plugin files (per-file output)...");
        
        // Generate headers (master, delegates, EditorTypes)
        let (master_header_path, _delegate_count, type_headers) = 
            super::codegen::generate_headers(&layout, ue5_config, &typed_program)?;
        
        // Generate per-item runtime files
        super::codegen::generate_runtime_items(&layout, ue5_config, &typed_program, &shader_names, &type_headers, &master_header_path)?;
        
        // Generate stdlib functions
        super::codegen::generate_stdlib_functions(&layout, ue5_config, &typed_program, &type_headers, &master_header_path)?;
        
        // Generate blueprint function library
        super::codegen::generate_blueprint_library(&layout, ue5_config, &typed_program, &type_headers, &master_header_path)?;
        
        // Generate editor tools
        super::codegen::generate_editor_items(&layout, ue5_config, &typed_program, &master_header_path)?;
        
        println!();
        println!("   ✅ Master header finalized with all module includes");
        
        // NOTE: Do NOT add .generated.h to the master header.
        // The master header is a forward-declaration + include aggregation file, NOT a UHT-processed type.
        // Individual type headers (EHealthStatus.h, ADiagnosticPreviewActor.h, etc.) already have their
        // own .generated.h includes where needed (alongside UCLASS/USTRUCT/UENUM macros).
        
        // Generate module registration
        super::codegen::generate_module_registration(&layout, ue5_config, &typed_program)?;
        
    } else {
        // MONOLITHIC MODE: Generate single .h/.cpp with all types merged
        super::codegen::generate_monolithic(&layout, ue5_config, &typed_program)?;
    }
    
    // STEP 5: Write .uplugin and .Build.cs (with data-driven module dependency resolution)
    let has_shaders = !shader_names.is_empty();
    let mut module_graph = ue5::ue5::module_graph::ModuleGraph::new();
    let module_graph_path = std::path::Path::new("unreal/metadata/module_graph.json");
    if module_graph_path.exists() {
        if let Ok(data) = fs::read_to_string(module_graph_path) {
            let _ = module_graph.load(&data);
        }
    }
    super::codegen::write_plugin_files(&layout, ue5_config, &manifest.package.description, has_shaders, &module_graph)?;
    
    // Summary
    println!();
    println!("✅ Plugin build complete!");
    println!("📍 Location: {}", layout.plugin_root.display());
    println!();
    if ue5_config.modular_output {
        println!("💡 Modular compilation: {} user files + {} stdlib files → {} C++ modules", 
            user_source_files.len(), 
            stdlib_files.len(),
            user_source_files.len()); 
    } else {
        println!("💡 Multi-file compilation: {} user files + {} stdlib files combined", 
            user_source_files.len(),
            stdlib_files.len());
    }
    println!("⚡ Total shaders: {}", shader_names.len());
    Ok(())
}

/// Load stdlib + user source files, parse, validate, and type-check.
/// Returns (typed_program, shader_names, stdlib_files, user_source_files).
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: &super::config::PackageManifest,
    cwd: &PathBuf,
) -> KainResult<(kain_core::types::TypedProgram, Vec<String>, Vec<PathBuf>, Vec<PathBuf>)> {
    // STEP 1: Load stdlib files FIRST (they contain type definitions)
    let mut all_source_files = Vec::new();
    let mut stdlib_files = Vec::new();
    
    // Determine stdlib path from config or use defaults
    // STDLIB IS NOW DISABLED BY DEFAULT - only load if explicitly configured
    let stdlib_search_paths: Vec<PathBuf> = if let Some(custom_path) = &ue5_config.stdlib_path {
        // Use custom path from KAIN.toml (only if user explicitly wants stdlib)
        vec![custom_path.clone()]
    } else {
        // STDLIB DISABLED BY DEFAULT - return empty vec
        vec![]
    };
    
    // Try each search path
    for stdlib_path in stdlib_search_paths {
        if stdlib_path.exists() {
            if let Ok(entries) = fs::read_dir(&stdlib_path) {
                for entry in entries {
                    if let Ok(entry) = entry {
                        let path = entry.path();
                        if path.extension().map_or(false, |e| e == "kn") {
                            // Skip README files
                            if let Some(name) = path.file_name() {
                                if name.to_string_lossy().to_uppercase().contains("README") {
                                    continue;
                                }
                            }
                            stdlib_files.push(path);
                        }
                    }
                }
            }
            // Found stdlib, stop searching
            if !stdlib_files.is_empty() {
                println!("📚 Loaded stdlib from: {}", stdlib_path.display());
                break;
            }
        }
    }
    
    // Sort stdlib files for consistent ordering
    stdlib_files.sort();
    
    // Add stdlib files first
    for stdlib_file in &stdlib_files {
        all_source_files.push(stdlib_file.clone());
    }
    
    // STEP 2: Add user source files
    let user_source_files = if ue5_config.sources.is_empty() {
        // Fallback to single entry file
        vec![manifest.build.entry.clone()]
    } else {
        // Use multiple source files - GODMODE
        ue5_config.sources.clone()
    };
    
    // Add user files (as PathBuf, resolved relative to cwd)
    for user_file in &user_source_files {
        all_source_files.push(cwd.join(user_file));
    }
    
    println!("📁 Source files: {} (stdlib: {}, user: {})", 
        all_source_files.len(), 
        stdlib_files.len(), 
        user_source_files.len()
    );
    println!("   📚 Stdlib files:");
    for (i, file) in stdlib_files.iter().enumerate() {
        if let Some(name) = file.file_name() {
            println!("      {}. {}", i + 1, name.to_string_lossy());
        }
    }
    println!("   📝 User files:");
    for (i, file) in user_source_files.iter().enumerate() {
        println!("      {}. {}", i + 1, file.display());
    }
    println!();
    
    // Parse and validate EACH source file independently (LLM-optimized pipeline)
    let mut all_asts = Vec::new();
    let mut all_shader_names = Vec::new();
    
    println!("🔍 Validating source files...");
    for source_path in &all_source_files {
        if !source_path.exists() {
            return Err(KainError::runtime(format!(
                "Source file not found: {}", source_path.display()
            )));
        }
        
        let file_source = fs::read_to_string(&source_path).map_err(|e| KainError::Io(e))?;
        
        // Parse this file independently - catch errors early with clear file context
        let tokens = match kain_core::Lexer::new(&file_source).tokenize() {
            Ok(t) => t,
            Err(e) => {
                return Err(KainError::runtime(format!(
                    "❌ Syntax error in {}: {}", source_path.display(), e
                )));
            }
        };
        
        let ast = match kain_core::Parser::new(&tokens).parse() {
            Ok(a) => a,
            Err(e) => {
                let file_name = source_path.file_name()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_else(|| source_path.display().to_string());
                let formatted_error = post_process::format_error_with_location(
                    &file_source, &e.to_string(), file_name
                );
                return Err(KainError::runtime(format!(
                    "❌ Parse error in {}:{}", source_path.display(), formatted_error
                )));
            }
        };
        
        // Extract shader names from this file
        if let Ok(names) = post_process::extract_shader_names(&file_source) {
            all_shader_names.extend(names);
        }
        
        if let Some(name) = source_path.file_name() {
            println!("   ✓ {} validated", name.to_string_lossy());
        }
        
        all_asts.push(ast);
    }
    println!();
    
    // MERGE all ASTs into a single program
    let mut merged = kain_core::ast::Program { items: Vec::new(), span: kain_core::Span { start: 0, end: 0 } };
    for ast in all_asts {
        merged.items.extend(ast.items);
    }
    
    // Type-check the MERGED program
    println!("🔍 Type checking merged program...");
    let typed_program = match kain_core::types::check(&merged) {
        Ok(tp) => {
            println!("   ✓ Type checking passed");
            tp
        }
        Err(e) => {
            return Err(KainError::runtime(format!(
                "❌ Type error in merged program: {}", e
            )));
        }
    };
    println!();
    
    // Run Oracle validation (with data-driven UHT rules when available)
    println!("🔬 Running Unreal Semantic Validator (Oracle)...");
    let kb = ue5::ue5::engine_knowledge::EngineKnowledge::new();
    let mut uht = ue5::ue5::uht_rules::UhtRules::new();
    let uht_path = std::path::Path::new("unreal/metadata/uht_rules.json");
    if uht_path.exists() {
        if let Ok(data) = std::fs::read_to_string(uht_path) {
            let _ = uht.load(&data);
        }
    }
    match ue5::ue5::oracle::validate_program_full(&typed_program, &kb, &uht) {
        Ok(()) => {
            println!("   ✓ Oracle validation passed");
        }
        Err(e) => {
            return Err(e);
        }
    }
    println!();
    
    Ok((typed_program, all_shader_names, stdlib_files, user_source_files))
}
