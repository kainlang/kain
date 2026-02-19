use std::fs;
use std::path::PathBuf;
use crate::error::{KainError, KainResult};
use super::config::Ue5Config;
use super::post_process;

/// Build UE5 plugin from KAIN.toml configuration
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    
    // Try to load KAIN.toml, but don't fail if it doesn't exist
    let (manifest, ue5_config) = match super::load_manifest(&cwd) {
        Ok(manifest) => {
            // KAIN.toml exists - use it
            let ue5_config = manifest.ue5.as_ref()
                .ok_or_else(|| KainError::runtime("No [ue5] section in KAIN.toml"))?
                .clone();
            (Some(manifest), ue5_config)
        }
        Err(_) => {
            // KAIN.toml not found - auto-detect
            println!("ℹ️  No KAIN.toml found, using auto-detection...");
            println!();
            let config = create_default_config(&cwd)?;
            (None, config)
        }
    };
    
    println!("🚀 Building UE5 Plugin: {}", ue5_config.plugin_name);
    println!("📍 Plugin directory: {}", ue5_config.plugin_dir.display());
    println!();

    // STEP 1: Load and parse source files
    let (typed_program, all_shader_names, stdlib_files, user_source_files, material_graphs) =
        load_and_parse_sources(&ue5_config, manifest.as_ref(), &cwd)?;

    // STEP 2: Setup plugin directory structure
    let layout = super::plugin_layout::setup(&ue5_config, &cwd, &typed_program, !all_shader_names.is_empty())?;

    // Use shader names from config or auto-detected
    let shader_names = if ue5_config.shaders.is_empty() {
        all_shader_names
    } else {
        ue5_config.shaders.clone()
    };

    // STEP 3: Compile shaders (optional)
    super::codegen::compile_shaders(&layout, &ue5_config, &typed_program, &shader_names)?;

    // STEP 3.5: Extract and generate material graphs
    if !material_graphs.is_empty() {
        println!();
        println!("🎨 Found {} material graphs:", material_graphs.len());
        for mat in &material_graphs {
            println!("   - {}", mat.name);
        }

        #[cfg(feature = "ue5")]
        {
            use ue5_materials::MaterialGraph;

            let mut converted_graphs = Vec::new();
            for mat_def in &material_graphs {
                // Convert AST MaterialGraphDef to IR MaterialGraph
                match convert_material_graph(mat_def) {
                    Ok(graph) => {
                        println!("   ✓ Converted material: {}", graph.name);
                        converted_graphs.push(graph);
                    }
                    Err(e) => {
                        eprintln!("   ⚠️  Failed to convert material {}: {}", mat_def.name, e);
                    }
                }
            }

            if !converted_graphs.is_empty() {
                super::material_gen::generate_material_factories(&ue5_config.plugin_name, &converted_graphs, &cwd)?;
            }
        }
        println!();
    }

    // STEP 4: Generate main plugin files
    println!();

    if ue5_config.modular_output {
        // MODULAR MODE: Generate separate .h/.cpp for each source file
        println!("🎯 Generating modular plugin files (per-file output)...");

        // Generate headers (master, delegates, EditorTypes)
        let (master_header_path, _delegate_count, type_headers) =
            super::codegen::generate_headers(&layout, &ue5_config, &typed_program)?;

        // Generate per-item runtime files
        super::codegen::generate_runtime_items(&layout, &ue5_config, &typed_program, &shader_names, &type_headers, &master_header_path)?;

        // Generate stdlib functions
        super::codegen::generate_stdlib_functions(&layout, &ue5_config, &typed_program, &type_headers, &master_header_path)?;

        // Generate blueprint function library
        super::codegen::generate_blueprint_library(&layout, &ue5_config, &typed_program, &type_headers, &master_header_path)?;

        // Generate editor tools
        super::codegen::generate_editor_items(&layout, &ue5_config, &typed_program, &master_header_path)?;

        println!();
        println!("   ✅ Master header finalized with all module includes");

        // NOTE: Do NOT add .generated.h to the master header.
        // The master header is a forward-declaration + include aggregation file, NOT a UHT-processed type.
        // Individual type headers (EHealthStatus.h, ADiagnosticPreviewActor.h, etc.) already have their
        // own .generated.h includes where needed (alongside UCLASS/USTRUCT/UENUM macros).

        // Detect if program has shaders (needed for module registration)
        let has_shaders = !shader_names.is_empty();

        // Generate module registration
        super::codegen::generate_module_registration(&layout, &ue5_config, &typed_program, has_shaders)?;

    } else {
        // MONOLITHIC MODE: Generate single .h/.cpp with all types merged
        super::codegen::generate_monolithic(&layout, &ue5_config, &typed_program)?;
    }

    // STEP 5: Write .uplugin and .Build.cs (with data-driven module dependency resolution)
    let has_shaders = !shader_names.is_empty();
    let mut module_graph = ue5::ue5::module_graph::ModuleGraph::new();

    // Resolve module_graph.json using a data-driven search order:
    //   1. KAIN_ROOT env var (explicit override — set once, works everywhere)
    //   2. Walk up from CWD (finds kain/unreal/metadata/ from any plugin subdir)
    //   3. CWD-relative fallback (works if run directly from kain/ root)
    let module_graph_path = {
        let relative = std::path::Path::new("unreal").join("metadata").join("module_graph.json");

        // 1. Explicit env var — highest priority
        let from_env = std::env::var("KAIN_ROOT").ok()
            .map(|root| std::path::PathBuf::from(root).join(&relative))
            .filter(|p| p.exists());

        // 2. Walk up from CWD — works when running `kain build --ue5` from any plugin dir.
        //    Looks for an ancestor directory that contains unreal/metadata/module_graph.json.
        //    This correctly finds the KAIN root (e.g. M:\Kain-Lang\kain-private\kain\) even
        //    when CWD is M:\Kain-Lang\kain-private\kain\testing\Phase3\SlateTest4\.
        let from_cwd_walk = {
            let mut dir = cwd.clone();
            let mut found = None;
            for _ in 0..10 {  // walk up at most 10 levels
                let candidate = dir.join(&relative);
                if candidate.exists() {
                    found = Some(candidate);
                    break;
                }
                match dir.parent() {
                    Some(p) => dir = p.to_path_buf(),
                    None => break,
                }
            }
            found
        };

        from_env
            .or(from_cwd_walk)
            .unwrap_or_else(|| cwd.join(&relative))
    };


    if module_graph_path.exists() {
        match fs::read_to_string(&module_graph_path) {
            Ok(data) => {
                match module_graph.load(&data) {
                    Ok(()) => {
                        let (mods, types, headers) = module_graph.stats();
                        println!("📊 Module graph loaded: {} modules, {} types, {} headers",
                            mods, types, headers);
                        println!("   📍 From: {}", module_graph_path.display());
                    }
                    Err(e) => eprintln!("⚠️  module_graph.json parse error: {}", e),
                }
            }
            Err(e) => eprintln!("⚠️  Could not read module_graph.json: {}", e),
        }
    } else {
        println!("ℹ️  module_graph.json not found — Build.cs will use feature-based fallback");
        println!("   Run: python unreal/scripts/module_graph_extractor.py <UE_SOURCE_DIR>");
    }

    // Get description from manifest or use default
    let description = manifest.as_ref()
        .and_then(|m| m.package.description.clone());

    super::codegen::write_plugin_files(&layout, &ue5_config, &description, has_shaders, &module_graph, &typed_program)?;

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
/// Returns (typed_program, shader_names, stdlib_files, user_source_files, material_graphs).
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: Option<&super::config::PackageManifest>,
    cwd: &PathBuf,
) -> KainResult<(kain_core::types::TypedProgram, Vec<String>, Vec<PathBuf>, Vec<PathBuf>, Vec<kain_core::ast::MaterialGraphDef>)> {
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
        // Fallback to single entry file from manifest (if available)
        if let Some(m) = manifest {
            vec![m.build.entry.clone()]
        } else {
            // No manifest and no sources - error
            return Err(KainError::runtime("No source files specified and no KAIN.toml found"));
        }
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
    
    // Extract material graphs BEFORE type checking (since MaterialGraph not yet in TypedItem)
    let material_graphs: Vec<kain_core::ast::MaterialGraphDef> = merged.items.iter()
        .filter_map(|item| {
            if let kain_core::ast::Item::MaterialGraph(def) = item {
                Some(def.clone())
            } else {
                None
            }
        })
        .collect();
    
    // Filter out material graphs from the program before type checking
    // (they will be processed separately for material generation)
    merged.items.retain(|item| !matches!(item, kain_core::ast::Item::MaterialGraph(_)));
    
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
    
    Ok((typed_program, all_shader_names, stdlib_files, user_source_files, material_graphs))
}

/// Convert AST MaterialGraphDef to IR MaterialGraph
#[cfg(feature = "ue5")]
fn convert_material_graph(def: &kain_core::ast::MaterialGraphDef) -> KainResult<ue5_materials::MaterialGraph> {
    use ue5_materials::{MaterialGraph, MaterialInput, MaterialInputType, MaterialProperties,
                        MaterialOutputs, BlendMode, ShadingModel, MaterialDomain,
                        MaterialNode, MaterialNodeType};
    use std::collections::HashMap;

    // Extract blend_mode and shading_model from attributes
    let mut blend_mode = BlendMode::Opaque;
    let mut shading_model = ShadingModel::DefaultLit;

    for attr in &def.attributes {
        if attr.name == "material_graph" {
            for arg in &attr.args {
                if let kain_core::ast::Expr::Binary { left, right, .. } = arg {
                    if let kain_core::ast::Expr::Ident(name, _) = &**left {
                        if let kain_core::ast::Expr::Ident(value, _) = &**right {
                            match name.as_str() {
                                "blend_mode" => {
                                    blend_mode = match value.as_str() {
                                        "Opaque" => BlendMode::Opaque,
                                        "Masked" => BlendMode::Masked,
                                        "Translucent" => BlendMode::Translucent,
                                        "Additive" => BlendMode::Additive,
                                        "Modulate" => BlendMode::Modulate,
                                        _ => BlendMode::Opaque,
                                    };
                                }
                                "shading_model" => {
                                    shading_model = match value.as_str() {
                                        "DefaultLit" => ShadingModel::DefaultLit,
                                        "Unlit" => ShadingModel::Unlit,
                                        _ => ShadingModel::DefaultLit,
                                    };
                                }
                                _ => {}
                            }
                        }
                    }
                }
            }
        }
    }

    // Convert inputs
    let inputs: Vec<MaterialInput> = def.inputs.iter().map(|input| {
        MaterialInput {
            name: input.name.clone(),
            input_type: map_material_input_type(&input.ty),
            default_value: input.default.as_ref().map(|e| format!("{:?}", e)),
        }
    }).collect();

    // --- Expression → Node conversion ---
    // node_counter: monotonically increasing ID for generated nodes
    let mut node_counter: usize = 0;
    // nodes: ordered list of generated MaterialNodes
    let mut nodes: Vec<MaterialNode> = Vec::new();
    // scope: maps let-binding name → node_id of the expression that produced it
    // Also pre-populate with input parameter names so they resolve to parameter nodes.
    let mut scope: HashMap<String, String> = HashMap::new();

    // Grid layout: inputs on the left, each let-binding one column to the right
    let col_width = 300i32;
    let row_height = 200i32;

    // Create parameter nodes for each declared input
    for (row, input) in def.inputs.iter().enumerate() {
        let x = -800;
        let y = row as i32 * row_height;
        let id = format!("param_{}", input.name);
        let node_type = match &input.ty {
            kain_core::ast::Type::Named { name, .. } => match name.as_str() {
                "Texture2D" => MaterialNodeType::TextureSampleParameter2D {
                    param_name: input.name.clone(),
                    default_texture: None,
                    uv_input: None,
                },
                "Float" => MaterialNodeType::ScalarParameter {
                    name: input.name.clone(),
                    default: input.default.as_ref().and_then(|e| {
                        if let kain_core::ast::Expr::Float(v, _) = e { Some(*v as f32) } else { None }
                    }).unwrap_or(0.0),
                },
                "Vec3" | "Vec4" => MaterialNodeType::VectorParameter {
                    name: input.name.clone(),
                    default: [1.0, 1.0, 1.0],
                },
                _ => MaterialNodeType::ScalarParameter { name: input.name.clone(), default: 0.0 },
            },
            _ => MaterialNodeType::ScalarParameter { name: input.name.clone(), default: 0.0 },
        };
        nodes.push(MaterialNode { id: id.clone(), node_type, position: (x, y) });
        scope.insert(input.name.clone(), id);
    }

    // Walk body let-bindings
    for (stmt_idx, stmt) in def.body.iter().enumerate() {
        if let kain_core::ast::MaterialStatement::Let { name, value, .. } = stmt {
            let x = -500 + stmt_idx as i32 * col_width;
            let y = 0;
            let node_id = emit_expr(value, x, y, &mut node_counter, &mut nodes, &scope);
            scope.insert(name.clone(), node_id);
        }
    }

    // Resolve outputs: each output.value is an Ident referring to a let-binding or input
    let mut outputs = MaterialOutputs::default();
    for output in &def.outputs {
        let node_id = emit_expr(&output.value, 400, 0, &mut node_counter, &mut nodes, &scope);
        match output.name.as_str() {
            "base_color"           => outputs.base_color = Some(node_id),
            "metallic"             => outputs.metallic = Some(node_id),
            "specular"             => outputs.specular = Some(node_id),
            "roughness"            => outputs.roughness = Some(node_id),
            "emissive"             => outputs.emissive = Some(node_id),
            "opacity"              => outputs.opacity = Some(node_id),
            "normal"               => outputs.normal = Some(node_id),
            "ambient_occlusion"    => outputs.ambient_occlusion = Some(node_id),
            "world_position_offset"=> outputs.world_position_offset = Some(node_id),
            _ => {}
        }
    }

    let properties = MaterialProperties {
        domain: MaterialDomain::Surface,
        blend_mode,
        shading_model,
        two_sided: false,
    };

    Ok(MaterialGraph {
        name: def.name.clone(),
        inputs,
        outputs,
        properties,
        nodes,
    })
}

/// Recursively emit material nodes for an expression, returning the node_id of the root.
/// If the expression is a simple identifier already in scope, returns that node_id directly.
#[cfg(feature = "ue5")]
fn emit_expr(
    expr: &kain_core::ast::Expr,
    x: i32,
    y: i32,
    counter: &mut usize,
    nodes: &mut Vec<ue5_materials::MaterialNode>,
    scope: &std::collections::HashMap<String, String>,
) -> String {
    use ue5_materials::{MaterialNode, MaterialNodeType};
    use kain_core::ast::{Expr, BinaryOp};

    match expr {
        // Identifier — look up in scope (input param or let binding)
        Expr::Ident(name, _) => {
            if let Some(id) = scope.get(name.as_str()) {
                return id.clone();
            }
            // Unknown ident: emit a scalar constant 0 as a placeholder
            let id = format!("node_{}", counter);
            *counter += 1;
            nodes.push(MaterialNode {
                id: id.clone(),
                node_type: MaterialNodeType::ScalarParameter { name: name.clone(), default: 0.0 },
                position: (x, y),
            });
            id
        }

        // Float literal → ConstantFloat
        Expr::Float(v, _) => {
            let id = format!("node_{}", counter);
            *counter += 1;
            nodes.push(MaterialNode {
                id: id.clone(),
                node_type: MaterialNodeType::ConstantFloat { value: *v as f32 },
                position: (x, y),
            });
            id
        }

        // Int literal → ConstantFloat
        Expr::Int(v, _) => {
            let id = format!("node_{}", counter);
            *counter += 1;
            nodes.push(MaterialNode {
                id: id.clone(),
                node_type: MaterialNodeType::ConstantFloat { value: *v as f32 },
                position: (x, y),
            });
            id
        }

        // Binary ops: *, +, -, /
        Expr::Binary { left, op, right, .. } => {
            let a = emit_expr(left, x - 200, y - 100, counter, nodes, scope);
            let b = emit_expr(right, x - 200, y + 100, counter, nodes, scope);
            let id = format!("node_{}", counter);
            *counter += 1;
            let node_type = match op {
                BinaryOp::Mul => MaterialNodeType::Multiply { a, b },
                BinaryOp::Add => MaterialNodeType::Add { a, b },
                BinaryOp::Sub => MaterialNodeType::Subtract { a, b },
                BinaryOp::Div => MaterialNodeType::Divide { a, b },
                _ => MaterialNodeType::Multiply { a, b }, // fallback
            };
            nodes.push(MaterialNode { id: id.clone(), node_type, position: (x, y) });
            id
        }

        // Function calls: lerp(), clamp(), sample(), multiply(), add(), etc.
        Expr::Call { callee, args, .. } => {
            let func_name = if let Expr::Ident(n, _) = &**callee { n.as_str() } else { "" };

            // texture_coordinate / uv — read index directly from literal, emit no child nodes
            if func_name == "texture_coordinate" || func_name == "uv" {
                let index = if let Some(first) = args.first() {
                    if let Expr::Int(v, _) = &first.value { *v as u32 } else { 0 }
                } else { 0 };
                let id = format!("node_{}", counter);
                *counter += 1;
                nodes.push(MaterialNode {
                    id: id.clone(),
                    node_type: MaterialNodeType::TextureCoordinate { index, tiling: [1.0, 1.0] },
                    position: (x, y),
                });
                return id;
            }

            let arg_ids: Vec<String> = args.iter().enumerate().map(|(i, arg)| {
                let ax = x - 200;
                let ay = y + i as i32 * 150 - (args.len() as i32 * 75);
                emit_expr(&arg.value, ax, ay, counter, nodes, scope)
            }).collect();

            // sample(tex, uv) — patch uv onto the existing texture param node, return tex id
            if func_name == "sample" {
                if let (Some(tex_id), Some(uv_id)) = (arg_ids.get(0), arg_ids.get(1)) {
                    for n in nodes.iter_mut() {
                        if &n.id == tex_id {
                            if let MaterialNodeType::TextureSampleParameter2D { ref mut uv_input, .. } = n.node_type {
                                *uv_input = Some(uv_id.clone());
                            }
                            break;
                        }
                    }
                    return tex_id.clone();
                }
            }

            let id = format!("node_{}", counter);
            *counter += 1;

            let node_type = match func_name {
                "lerp" | "mix" => {
                    let a = arg_ids.get(0).cloned().unwrap_or_default();
                    let b = arg_ids.get(1).cloned().unwrap_or_default();
                    let alpha = arg_ids.get(2).cloned().unwrap_or_default();
                    MaterialNodeType::Lerp { a, b, alpha }
                }
                "clamp" | "saturate" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    let min = arg_ids.get(1).cloned().unwrap_or(input.clone());
                    let max = arg_ids.get(2).cloned().unwrap_or(input.clone());
                    MaterialNodeType::Clamp { input, min, max }
                }
                "multiply" => {
                    let a = arg_ids.get(0).cloned().unwrap_or_default();
                    let b = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Multiply { a, b }
                }
                "add" => {
                    let a = arg_ids.get(0).cloned().unwrap_or_default();
                    let b = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Add { a, b }
                }
                "dot" => {
                    let a = arg_ids.get(0).cloned().unwrap_or_default();
                    let b = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Dot { a, b }
                }
                "power" | "pow" => {
                    let base = arg_ids.get(0).cloned().unwrap_or_default();
                    let exponent = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Power { base, exponent }
                }
                "mask_r" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    MaterialNodeType::ComponentMask { input, mask: "R".to_string() }
                }
                "mask_g" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    MaterialNodeType::ComponentMask { input, mask: "G".to_string() }
                }
                "mask_b" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    MaterialNodeType::ComponentMask { input, mask: "B".to_string() }
                }
                "mask_a" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    MaterialNodeType::ComponentMask { input, mask: "A".to_string() }
                }
                "mask_rgb" => {
                    let input = arg_ids.get(0).cloned().unwrap_or_default();
                    MaterialNodeType::ComponentMask { input, mask: "RGB".to_string() }
                }
                "append" => {
                    let a = arg_ids.get(0).cloned().unwrap_or_default();
                    let b = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Append { a, b }
                }
                "fresnel" => {
                    let exponent = arg_ids.get(0).cloned().unwrap_or_default();
                    let base_reflect_fraction = arg_ids.get(1).cloned().unwrap_or_default();
                    MaterialNodeType::Fresnel { exponent, base_reflect_fraction }
                }
                _ => {
                    // Unknown function — emit a scalar param as placeholder
                    MaterialNodeType::ScalarParameter { name: func_name.to_string(), default: 0.0 }
                }
            };

            nodes.push(MaterialNode { id: id.clone(), node_type, position: (x, y) });
            id
        }

        // Field access: expr.rgb, expr.r, etc. — emit a ComponentMask
        Expr::Field { object, field, .. } => {
            let input = emit_expr(object, x - 200, y, counter, nodes, scope);
            let id = format!("node_{}", counter);
            *counter += 1;
            let mask = match field.as_str() {
                "r" | "x" => "R",
                "g" | "y" => "G",
                "b" | "z" => "B",
                "a" | "w" => "A",
                "rg" | "xy" => "RG",
                "rgb" | "xyz" => "RGB",
                "rgba" | "xyzw" => "RGBA",
                _ => "RGB", // default rgb
            };
            nodes.push(MaterialNode {
                id: id.clone(),
                node_type: MaterialNodeType::ComponentMask { input, mask: mask.to_string() },
                position: (x, y),
            });
            id
        }

        // Fallback: emit a constant 0
        _ => {
            let id = format!("node_{}", counter);
            *counter += 1;
            nodes.push(MaterialNode {
                id: id.clone(),
                node_type: MaterialNodeType::ConstantFloat { value: 0.0 },
                position: (x, y),
            });
            id
        }
    }
}

/// Map KAIN type to material input type
#[cfg(feature = "ue5")]
fn map_material_input_type(ty: &kain_core::ast::Type) -> ue5_materials::MaterialInputType {
    use ue5_materials::MaterialInputType;
    match ty {
        kain_core::ast::Type::Named { name, .. } => {
            match name.as_str() {
                "Float" => MaterialInputType::Float,
                "Vec2" => MaterialInputType::Vec2,
                "Vec3" => MaterialInputType::Vec3,
                "Vec4" => MaterialInputType::Vec4,
                _ => MaterialInputType::Float,
            }
        }
        _ => MaterialInputType::Float,
    }
}

/// Create default UE5 config by auto-detecting .kn files and plugin name
fn create_default_config(cwd: &PathBuf) -> KainResult<Ue5Config> {
    // Detect plugin name from .uplugin or directory name
    let plugin_name = detect_plugin_name_from_dir(cwd)?;
    
    // Find all .kn files in current directory (non-recursive)
    let sources = find_kn_files(cwd)?;
    
    if sources.is_empty() {
        return Err(KainError::runtime(
            "No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration."
        ));
    }
    
    println!("📁 Found {} .kn file(s):", sources.len());
    for src in &sources {
        if let Some(name) = src.file_name() {
            println!("   - {}", name.to_string_lossy());
        }
    }
    println!();
    
    Ok(Ue5Config {
        plugin_name,
        plugin_dir: cwd.to_path_buf(),
        sources,
        shaders: vec![],
        copyright: None,
        modular_output: true,  // Default to modular output
        stdlib_path: None,     // No stdlib by default
    })
}

/// Detect plugin name from .uplugin file or directory name
fn detect_plugin_name_from_dir(cwd: &PathBuf) -> KainResult<String> {
    // Look for .uplugin file
    if let Ok(entries) = fs::read_dir(cwd) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().map_or(false, |e| e == "uplugin") {
                if let Some(stem) = path.file_stem() {
                    let name = stem.to_string_lossy().to_string();
                    println!("🔍 Detected plugin name from .uplugin: {}", name);
                    return Ok(name);
                }
            }
        }
    }
    
    // Fallback to directory name
    if let Some(dir_name) = cwd.file_name() {
        let name = dir_name.to_string_lossy().to_string();
        println!("🔍 Using directory name as plugin name: {}", name);
        return Ok(name);
    }
    
    Err(KainError::runtime(
        "Could not determine plugin name. Please create a KAIN.toml or .uplugin file."
    ))
}

/// Find all .kn files in the current directory (non-recursive)
fn find_kn_files(cwd: &PathBuf) -> KainResult<Vec<PathBuf>> {
    let mut kn_files = Vec::new();
    
    if let Ok(entries) = fs::read_dir(cwd) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_file() && path.extension().map_or(false, |e| e == "kn") {
                // Skip README files
                if let Some(name) = path.file_name() {
                    let name_str = name.to_string_lossy().to_uppercase();
                    if name_str.contains("README") {
                        continue;
                    }
                }
                kn_files.push(path);
            }
        }
    }
    
    // Sort for consistent ordering
    kn_files.sort();
    
    Ok(kn_files)
}
