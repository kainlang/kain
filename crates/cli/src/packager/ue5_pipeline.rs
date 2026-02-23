use std::fs;
use std::collections::HashMap;
use std::path::PathBuf;
use crate::error::{KainError, KainResult};
use kain_core::diagnostics::{SpanMapper, enhance_error_with_location};
use super::config::Ue5Config;
use super::post_process;

/// Helper function to enhance codegen errors with file:line:col location information
/// Takes a Result from codegen and enhances any KainError::Codegen with location data
fn enhance_codegen_result<T>(
    result: KainResult<T>,
    span_mapper: &SpanMapper,
    file_path: &str,
) -> KainResult<T> {
    result.map_err(|e| enhance_error_with_location(e, span_mapper, file_path))
}

/// Metadata for a single generated binary asset, collected during the build
/// for bulk-registration into AssetRegistry.bin.
#[cfg(feature = "ue5")]
struct GeneratedAsset {
    /// Full UE package name, e.g. `/Game/Materials/M_Fire`
    package_name: String,
    /// Short asset name, e.g. `M_Fire`
    asset_name: String,
    /// Full UE class path, e.g. `/Script/Engine.Material`
    class_path: &'static str,
}

/// Build UE5 plugin from KAIN.toml configuration
pub fn build_ue5_plugin() -> KainResult<()> {
    build_ue5_plugin_with_options(false)
}

/// Build UE5 plugin with options (embed KAIN markers, etc.)
pub fn build_ue5_plugin_with_options(embed_kain: bool) -> KainResult<()> {
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
    let (typed_program, all_shader_names, stdlib_files, user_source_files, symbol_source_map, material_graphs, material_functions, graph_editors, graph_runtimes) =
        load_and_parse_sources(&ue5_config, manifest.as_ref(), &cwd)?;

    // STEP 2: Setup plugin directory structure
    let layout = super::plugin_layout::setup(&ue5_config, &cwd, &typed_program, !all_shader_names.is_empty(), !graph_editors.is_empty())?;

    // Use shader names from config or auto-detected
    let shader_names = if ue5_config.shaders.is_empty() {
        all_shader_names
    } else {
        ue5_config.shaders.clone()
    };

    // STEP 3: Compile shaders (optional)
    super::codegen::compile_shaders(&layout, &ue5_config, &typed_program, &shader_names)?;

    // Accumulate all successfully written binary assets so we can stamp AssetRegistry.bin
    // in a single pass at the end. This is data-driven: each step appends to this Vec.
    #[cfg(feature = "ue5")]
    let mut generated_assets: Vec<GeneratedAsset> = Vec::new();

    // STEP 3.5: Generate Materials (Binary .uasset + C++ Factory fallback)
    //
    // Shader→Material bridge: before converting each material graph we inject
    // all Surface-stage shaders from the typed program as CustomHLSL nodes.
    // This means any KAIN `shader foo { stage = Surface; … }` whose name
    // matches a node reference in the graph gets embedded as a
    // UMaterialExpressionCustom node rather than requiring a separate .usf file.
    #[cfg(feature = "ue5")]
    if !material_graphs.is_empty() {
        println!();
        println!("🎨 Generating {} materials...", material_graphs.len());

        // Build a pre-baked map of Surface shader name → CustomHLSL body.
        // We call emit_shader_body once per shader and cache the result so it
        // isn't re-generated for every material graph that references the same shader.
        let surface_shader_hlsl: std::collections::HashMap<String, String> = {
            let mirrors = ue5_shaders::codegen_usf::CachedMirrors::from_program(&typed_program);
            let type_db = ue5_shaders::codegen_usf::build_struct_map_pub(&typed_program);
            typed_program.items.iter()
                .filter_map(|item| {
                    if let kain_core::types::TypedItem::Shader(shader) = item {
                        if matches!(shader.ast.stage, kain_core::ast::ShaderStage::Surface) {
                            let name = shader.ast.name.clone();
                            let body = ue5_shaders::codegen_usf::emit_shader_body(
                                shader, &mirrors, type_db.clone(),
                            ).unwrap_or_default();
                            return Some((name, body));
                        }
                    }
                    None
                })
                .collect()
        };

        if !surface_shader_hlsl.is_empty() {
            println!("   🔗 Shader bridge: {} Surface shader(s) available for material injection",
                surface_shader_hlsl.len());
        }

        // Ensure Content/Materials exists for binary output
        let mat_content_dir = layout.plugin_root.join("Content").join("Materials");
        if let Err(e) = fs::create_dir_all(&mat_content_dir) {
            eprintln!("   ⚠️  Failed to create materials content dir: {}", e);
        }

        let mut converted_graphs = Vec::new();
        for mat_def in &material_graphs {
            match convert_material_graph(mat_def, &surface_shader_hlsl) {
                Ok(graph) => {
                    // Attempt binary .uasset generation first
                    match ue5_materials::material_serializer::serialize_material_graph(&graph, ue5_asset_utils::KainEngineTarget::default()) {
                        Ok(bytes) => {
                            let path = mat_content_dir.join(format!("{}.uasset", graph.name));
                            if let Err(e) = fs::write(&path, &bytes) {
                                eprintln!("   ⚠️  Failed to write .uasset for {}: {}", graph.name, e);
                            } else {
                                println!("   ✓ Binary material asset: {} ({} bytes)", graph.name, bytes.len());
                                generated_assets.push(GeneratedAsset {
                                    package_name: format!("/Game/Materials/{}", graph.name),
                                    asset_name: graph.name.clone(),
                                    class_path: "/Script/Engine.Material",
                                });
                            }
                        }
                        Err(e) => {
                            println!("   ℹ️  Binary generation failed for {}: {}", graph.name, e);
                            println!("       Falling back to C++ factory...");
                        }
                    }
                    converted_graphs.push(graph);
                }
                Err(e) => {
                    eprintln!("   ⚠️  Failed to convert material {}: {}", mat_def.name, e);
                }
            }
        }

        // Always generate C++ factories as a safety net (editor can re-import).
        // Route through layout.private_dir so the files land in the correct
        // Source/<Module>/Private/Generated/ path, not at the project root.
        if !converted_graphs.is_empty() {
            super::material_gen::generate_material_factories(&ue5_config.plugin_name, &converted_graphs, &layout.private_dir)?;
        }
        println!();
    }

    // STEP 2.5: Generate Material Functions (.uasset files)
    #[cfg(feature = "ue5")]
    if !material_functions.is_empty() {
        println!();
        println!("🔧 Generating {} material functions...", material_functions.len());

        // Ensure Content/Materials/Functions exists for binary output
        let func_content_dir = layout.plugin_root.join("Content").join("Materials").join("Functions");
        if let Err(e) = fs::create_dir_all(&func_content_dir) {
            eprintln!("   ⚠️  Failed to create material functions content dir: {}", e);
        }

        for func_def in &material_functions {
            match convert_material_function(func_def) {
                Ok(func_ir) => {
                    // Generate .uasset file
                    match ue5_materials::material_function_builder::serialize_material_function(&func_ir) {
                        Ok(bytes) => {
                            let path = func_content_dir.join(format!("MF_{}.uasset", func_def.name));
                            if let Err(e) = fs::write(&path, &bytes) {
                                eprintln!("   ⚠️  Failed to write .uasset for {}: {}", func_def.name, e);
                            } else {
                                println!("   ✓ Material function: MF_{} ({} bytes)", func_def.name, bytes.len());
                                generated_assets.push(GeneratedAsset {
                                    package_name: format!("/Game/Materials/Functions/MF_{}", func_def.name),
                                    asset_name: format!("MF_{}", func_def.name),
                                    class_path: "/Script/Engine.MaterialFunction",
                                });
                            }
                        }
                        Err(e) => {
                            eprintln!("   ⚠️  Failed to serialize material function {}: {}", func_def.name, e);
                        }
                    }
                }
                Err(e) => {
                    eprintln!("   ⚠️  Failed to convert material function {}: {}", func_def.name, e);
                }
            }
        }
        println!();
    }

    // STEP 2.6: Generate Graph Editors (.uasset + C++ Factory)
    #[cfg(feature = "ue5")]
    if !graph_editors.is_empty() {
        println!();
        println!("📊 Generating {} graph editors...", graph_editors.len());

        // Ensure Content/Graphs exists for binary output
        let graph_content_dir = layout.plugin_root.join("Content").join("Graphs");
        if let Err(e) = fs::create_dir_all(&graph_content_dir) {
            eprintln!("   ⚠️  Failed to create graphs content dir: {}", e);
        }

        // Separate runtime graphs from editor graphs
        let (runtime_graphs, editor_graphs): (Vec<_>, Vec<_>) = graph_editors.iter()
            .partition(|g| g.attributes.iter().any(|a| a.name == "runtime_graph"));

        // Generate editor graphs (traditional UEdGraph-based)
        for graph_def in &editor_graphs {
            match ue5_graphs::generate_graph_editor(graph_def, &ue5_config.plugin_name) {
                Ok(output) => {
                    // Write .uasset binary
                    if !output.uasset.is_empty() {
                        let uasset_path = graph_content_dir.join(format!("{}.uasset", graph_def.name));
                        if let Err(e) = fs::write(&uasset_path, &output.uasset) {
                            eprintln!("   ⚠️  Failed to write .uasset for {}: {}", graph_def.name, e);
                        } else {
                            println!("   ✓ Graph editor asset: {} ({} bytes)", graph_def.name, output.uasset.len());
                            generated_assets.push(GeneratedAsset {
                                package_name: format!("/Game/Graphs/{}", graph_def.name),
                                asset_name: graph_def.name.clone(),
                                class_path: "/Script/Engine.EdGraph",
                            });
                        }
                    }

                    // Write factory header (if generated)
                    // Factory files are editor-only and should go in the editor module
                    if !output.header.is_empty() {
                        let header_path = if let Some(ref editor_pub) = layout.editor_public_dir {
                            // Split mode: write to editor module Public directory
                            editor_pub.join(format!("{}Factory.h", graph_def.name))
                        } else {
                            // Single module: write to resolved public directory
                            layout.public_dir.join(format!("{}Factory.h", graph_def.name))
                        };
                        if let Err(e) = fs::write(&header_path, &output.header) {
                            eprintln!("   ⚠️  Failed to write factory header for {}: {}", graph_def.name, e);
                        } else {
                            println!("   ✓ Factory header: {}Factory.h", graph_def.name);
                        }
                    }

                    // Write factory source (if generated)
                    if !output.source.is_empty() {
                        let source_path = if let Some(ref editor_priv) = layout.editor_private_dir {
                            // Split mode: write to editor module Private directory
                            editor_priv.join(format!("{}Factory.cpp", graph_def.name))
                        } else {
                            // Single module: write to resolved private directory
                            layout.private_dir.join(format!("{}Factory.cpp", graph_def.name))
                        };
                        if let Err(e) = fs::write(&source_path, &output.source) {
                            eprintln!("   ⚠️  Failed to write factory source for {}: {}", graph_def.name, e);
                        } else {
                            println!("   ✓ Factory source: {}Factory.cpp", graph_def.name);
                        }
                    }
                }
                Err(e) => {
                    eprintln!("   ⚠️  Failed to generate graph editor {}: {}", graph_def.name, e);
                }
            }
        }

        // Generate runtime graphs (C++ classes in Source/ directory)
        if !runtime_graphs.is_empty() {
            println!();
            println!("⚡ Generating {} runtime graph(s)...", runtime_graphs.len());

            for graph_def in &runtime_graphs {
                match ue5_graphs::convert_runtime_graph(graph_def) {
                    Ok(runtime_ir) => {
                        // Convert runtime IR to GraphEditor IR for codegen
                        let mut graph_editor = ue5_graphs::GraphEditor::new(&runtime_ir.name);
                        
                        for node_data in &runtime_ir.node_types {
                            let node_type = ue5_graphs::NodeType {
                                name: node_data.name.clone(),
                                category: node_data.category.clone(),
                                inputs: node_data.input_pins.iter().map(|p| {
                                    ue5_graphs::PinDefinition {
                                        name: p.name.clone(),
                                        pin_type: match &p.pin_type {
                                            ue5_graphs::RuntimePinType::Exec => ue5_graphs::PinType::Exec,
                                            ue5_graphs::RuntimePinType::Bool => ue5_graphs::PinType::Bool,
                                            ue5_graphs::RuntimePinType::Int | ue5_graphs::RuntimePinType::Int64 => ue5_graphs::PinType::Int,
                                            ue5_graphs::RuntimePinType::Float => ue5_graphs::PinType::Float,
                                            ue5_graphs::RuntimePinType::String | ue5_graphs::RuntimePinType::Name | ue5_graphs::RuntimePinType::Text => ue5_graphs::PinType::String,
                                            ue5_graphs::RuntimePinType::Object(name) => ue5_graphs::PinType::Object(name.clone()),
                                            ue5_graphs::RuntimePinType::Struct(name) => ue5_graphs::PinType::Struct(name.clone()),
                                            ue5_graphs::RuntimePinType::Enum(name) => ue5_graphs::PinType::Enum(name.clone()),
                                            _ => ue5_graphs::PinType::Wildcard,
                                        },
                                        is_array: p.is_array,
                                        default_value: p.default_value.clone(),
                                        tooltip: p.tooltip.clone(),
                                    }
                                }).collect(),
                                outputs: node_data.output_pins.iter().map(|p| {
                                    ue5_graphs::PinDefinition {
                                        name: p.name.clone(),
                                        pin_type: match &p.pin_type {
                                            ue5_graphs::RuntimePinType::Exec => ue5_graphs::PinType::Exec,
                                            ue5_graphs::RuntimePinType::Bool => ue5_graphs::PinType::Bool,
                                            ue5_graphs::RuntimePinType::Int | ue5_graphs::RuntimePinType::Int64 => ue5_graphs::PinType::Int,
                                            ue5_graphs::RuntimePinType::Float => ue5_graphs::PinType::Float,
                                            ue5_graphs::RuntimePinType::String | ue5_graphs::RuntimePinType::Name | ue5_graphs::RuntimePinType::Text => ue5_graphs::PinType::String,
                                            ue5_graphs::RuntimePinType::Object(name) => ue5_graphs::PinType::Object(name.clone()),
                                            ue5_graphs::RuntimePinType::Struct(name) => ue5_graphs::PinType::Struct(name.clone()),
                                            ue5_graphs::RuntimePinType::Enum(name) => ue5_graphs::PinType::Enum(name.clone()),
                                            _ => ue5_graphs::PinType::Wildcard,
                                        },
                                        is_array: p.is_array,
                                        default_value: p.default_value.clone(),
                                        tooltip: p.tooltip.clone(),
                                    }
                                }).collect(),
                                properties: node_data.properties.iter().map(|prop| {
                                    ue5_graphs::PropertyDefinition {
                                        name: prop.name.clone(),
                                        property_type: format!("{:?}", prop.property_type),
                                        default_value: prop.default_value.clone(),
                                    }
                                }).collect(),
                                color: node_data.color,
                                icon: node_data.icon.clone(),
                                tooltip: node_data.tooltip.clone(),
                                execution_logic: None,
                            };
                            graph_editor.add_node_type(node_type);
                        }
                        
                        // Generate NodeData classes
                        let node_gen = ue5_graphs::NodeDataGenerator::new(
                            &graph_editor,
                            &ue5_config.plugin_name,
                        );

                        match node_gen.generate() {
                            Ok(node_output) => {
                                // Write NodeData headers to resolved public dir
                                let public_dir = layout.public_dir.clone();
                                if let Err(e) = fs::create_dir_all(&public_dir) {
                                    eprintln!("   ⚠️  Failed to create Public dir: {}", e);
                                }

                                let pin_data_path = public_dir.join(&node_output.pin_data_header.0);
                                if let Err(e) = fs::write(&pin_data_path, &node_output.pin_data_header.1) {
                                    eprintln!("   ⚠️  Failed to write pin data header: {}", e);
                                } else {
                                    println!("   ✓ Pin data: {}", node_output.pin_data_header.0);
                                }

                                for (filename, header) in &node_output.node_data_headers {
                                    let path = public_dir.join(filename);
                                    if let Err(e) = fs::write(&path, header) {
                                        eprintln!("   ⚠️  Failed to write node data header {}: {}", filename, e);
                                    } else {
                                        println!("   ✓ Node data: {}", filename);
                                    }
                                }

                                // Write NodeData sources to resolved private dir
                                let private_dir = layout.private_dir.clone();
                                if let Err(e) = fs::create_dir_all(&private_dir) {
                                    eprintln!("   ⚠️  Failed to create Private dir: {}", e);
                                }

                                for (filename, source) in &node_output.node_data_sources {
                                    let path = private_dir.join(filename);
                                    if let Err(e) = fs::write(&path, source) {
                                        eprintln!("   ⚠️  Failed to write node data source {}: {}", filename, e);
                                    } else {
                                        println!("   ✓ Node data: {}", filename);
                                    }
                                }
                            }
                            Err(e) => {
                                eprintln!("   ⚠️  Failed to generate node data for {}: {}", runtime_ir.name, e);
                            }
                        }

                        // Generate GraphInstance class
                        let instance_gen = ue5_graphs::InstanceGenerator::new(
                            &graph_editor,
                            &ue5_config.plugin_name,
                        );

                        match instance_gen.generate() {
                            Ok(instance_output) => {
                                let public_dir = layout.public_dir.clone();
                                let private_dir = layout.private_dir.clone();

                                let header_path = public_dir.join(&instance_output.instance_header.0);
                                if let Err(e) = fs::write(&header_path, &instance_output.instance_header.1) {
                                    eprintln!("   ⚠️  Failed to write instance header: {}", e);
                                } else {
                                    println!("   ✓ Instance: {}", instance_output.instance_header.0);
                                }

                                let source_path = private_dir.join(&instance_output.instance_source.0);
                                if let Err(e) = fs::write(&source_path, &instance_output.instance_source.1) {
                                    eprintln!("   ⚠️  Failed to write instance source: {}", e);
                                } else {
                                    println!("   ✓ Instance: {}", instance_output.instance_source.0);
                                }
                            }
                            Err(e) => {
                                eprintln!("   ⚠️  Failed to generate instance for {}: {}", runtime_ir.name, e);
                            }
                        }

                        // Generate GraphData class
                        match ue5_graphs::generate_graph_data_header(
                            &graph_editor,
                            &ue5_config.plugin_name,
                        ) {
                            Ok(header) => {
                                let public_dir = layout.public_dir.clone();
                                let path = public_dir.join(format!("{}GraphData.h", runtime_ir.name));
                                if let Err(e) = fs::write(&path, &header) {
                                    eprintln!("   ⚠️  Failed to write graph data header: {}", e);
                                } else {
                                    println!("   ✓ Graph data: {}GraphData.h", runtime_ir.name);
                                }
                            }
                            Err(e) => {
                                eprintln!("   ⚠️  Failed to generate graph data header: {}", e);
                            }
                        }

                        match ue5_graphs::generate_graph_data_source(
                            &graph_editor,
                            &ue5_config.plugin_name,
                        ) {
                            Ok(source) => {
                                let private_dir = layout.private_dir.clone();
                                let path = private_dir.join(format!("{}GraphData.cpp", runtime_ir.name));
                                if let Err(e) = fs::write(&path, &source) {
                                    eprintln!("   ⚠️  Failed to write graph data source: {}", e);
                                } else {
                                    println!("   ✓ Graph data: {}GraphData.cpp", runtime_ir.name);
                                }
                            }
                            Err(e) => {
                                eprintln!("   ⚠️  Failed to generate graph data source: {}", e);
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("   ⚠️  Failed to convert runtime graph {}: {}", graph_def.name, e);
                    }
                }
            }
        }

        println!();
    }

    // STEP 2.7: Generate Graph Runtime Systems (C++ classes for runtime graph execution)
    //
    // Graph runtime definitions (@graph_runtime) generate C++ classes for runtime graph execution:
    // - NodeData classes (base class for all node types in the graph)
    // - GraphInstance class (manages graph execution state and node instances)
    // - GraphData class (optional asset data container)
    //
    // These are placed in Source/{PluginName}/Public/ and Source/{PluginName}/Private/
    #[cfg(feature = "ue5")]
    if !graph_runtimes.is_empty() {
        println!();
        println!("⚡ Generating {} runtime graph system(s)...", graph_runtimes.is_empty());

        for graph_runtime_def in &graph_runtimes {
            println!("   📊 Processing runtime graph: {}", graph_runtime_def.name);

            // Convert GraphRuntimeDef AST to RuntimeGraph IR
            match ue5_graphs::convert_graph_runtime_to_ir(graph_runtime_def) {
                Ok(runtime_ir) => {
                    // Convert RuntimeGraph IR to GraphEditor IR (needed for codegen)
                    let mut graph_editor = ue5_graphs::GraphEditor::new(&runtime_ir.name);
                    
                    // Convert node types from RuntimeNodeData to NodeType
                    for node_data in &runtime_ir.node_types {
                        let node_type = ue5_graphs::NodeType {
                            name: node_data.name.clone(),
                            category: node_data.category.clone(),
                            inputs: node_data.input_pins.iter().map(|p| {
                                ue5_graphs::PinDefinition {
                                    name: p.name.clone(),
                                    pin_type: match &p.pin_type {
                                        ue5_graphs::RuntimePinType::Exec => ue5_graphs::PinType::Exec,
                                        ue5_graphs::RuntimePinType::Bool => ue5_graphs::PinType::Bool,
                                        ue5_graphs::RuntimePinType::Int | ue5_graphs::RuntimePinType::Int64 => ue5_graphs::PinType::Int,
                                        ue5_graphs::RuntimePinType::Float => ue5_graphs::PinType::Float,
                                        ue5_graphs::RuntimePinType::String | ue5_graphs::RuntimePinType::Name | ue5_graphs::RuntimePinType::Text => ue5_graphs::PinType::String,
                                        ue5_graphs::RuntimePinType::Vector => ue5_graphs::PinType::Struct("FVector".to_string()),
                                        ue5_graphs::RuntimePinType::Rotator => ue5_graphs::PinType::Struct("FRotator".to_string()),
                                        ue5_graphs::RuntimePinType::Transform => ue5_graphs::PinType::Struct("FTransform".to_string()),
                                        ue5_graphs::RuntimePinType::Color => ue5_graphs::PinType::Struct("FLinearColor".to_string()),
                                        ue5_graphs::RuntimePinType::Object(name) => ue5_graphs::PinType::Object(name.clone()),
                                        ue5_graphs::RuntimePinType::Struct(name) => ue5_graphs::PinType::Struct(name.clone()),
                                        ue5_graphs::RuntimePinType::Enum(name) => ue5_graphs::PinType::Enum(name.clone()),
                                        _ => ue5_graphs::PinType::Wildcard,
                                    },
                                    is_array: p.is_array,
                                    default_value: p.default_value.clone(),
                                    tooltip: p.tooltip.clone(),
                                }
                            }).collect(),
                            outputs: node_data.output_pins.iter().map(|p| {
                                ue5_graphs::PinDefinition {
                                    name: p.name.clone(),
                                    pin_type: match &p.pin_type {
                                        ue5_graphs::RuntimePinType::Exec => ue5_graphs::PinType::Exec,
                                        ue5_graphs::RuntimePinType::Bool => ue5_graphs::PinType::Bool,
                                        ue5_graphs::RuntimePinType::Int | ue5_graphs::RuntimePinType::Int64 => ue5_graphs::PinType::Int,
                                        ue5_graphs::RuntimePinType::Float => ue5_graphs::PinType::Float,
                                        ue5_graphs::RuntimePinType::String | ue5_graphs::RuntimePinType::Name | ue5_graphs::RuntimePinType::Text => ue5_graphs::PinType::String,
                                        ue5_graphs::RuntimePinType::Vector => ue5_graphs::PinType::Struct("FVector".to_string()),
                                        ue5_graphs::RuntimePinType::Rotator => ue5_graphs::PinType::Struct("FRotator".to_string()),
                                        ue5_graphs::RuntimePinType::Transform => ue5_graphs::PinType::Struct("FTransform".to_string()),
                                        ue5_graphs::RuntimePinType::Color => ue5_graphs::PinType::Struct("FLinearColor".to_string()),
                                        ue5_graphs::RuntimePinType::Object(name) => ue5_graphs::PinType::Object(name.clone()),
                                        ue5_graphs::RuntimePinType::Struct(name) => ue5_graphs::PinType::Struct(name.clone()),
                                        ue5_graphs::RuntimePinType::Enum(name) => ue5_graphs::PinType::Enum(name.clone()),
                                        _ => ue5_graphs::PinType::Wildcard,
                                    },
                                    is_array: p.is_array,
                                    default_value: p.default_value.clone(),
                                    tooltip: p.tooltip.clone(),
                                }
                            }).collect(),
                            properties: node_data.properties.iter().map(|prop| {
                                ue5_graphs::PropertyDefinition {
                                    name: prop.name.clone(),
                                    property_type: format!("{:?}", prop.property_type),
                                    default_value: prop.default_value.clone(),
                                }
                            }).collect(),
                            color: node_data.color,
                            icon: node_data.icon.clone(),
                            tooltip: node_data.tooltip.clone(),
                            execution_logic: None,
                        };
                        graph_editor.add_node_type(node_type);
                    }
                    
                    // Generate NodeData classes
                    let node_gen = ue5_graphs::NodeDataGenerator::new(
                        &graph_editor,
                        &ue5_config.plugin_name,
                    );

                    match node_gen.generate() {
                        Ok(node_output) => {
                            // Write PinData header
                            let pin_data_path = layout.public_dir.join(&node_output.pin_data_header.0);
                            if let Err(e) = fs::write(&pin_data_path, &node_output.pin_data_header.1) {
                                eprintln!("      ⚠️  Failed to write pin data header: {}", e);
                            } else {
                                println!("      ✓ {}", node_output.pin_data_header.0);
                            }

                            // Write PinData source
                            let pin_data_src_path = layout.private_dir.join(&node_output.pin_data_source.0);
                            if let Err(e) = fs::write(&pin_data_src_path, &node_output.pin_data_source.1) {
                                eprintln!("      ⚠️  Failed to write pin data source: {}", e);
                            } else {
                                println!("      ✓ {}", node_output.pin_data_source.0);
                            }

                            // Write base NodeData header
                            let base_header_path = layout.public_dir.join(&node_output.base_header.0);
                            if let Err(e) = fs::write(&base_header_path, &node_output.base_header.1) {
                                eprintln!("      ⚠️  Failed to write base node data header: {}", e);
                            } else {
                                println!("      ✓ {}", node_output.base_header.0);
                            }

                            // Write base NodeData source
                            let base_source_path = layout.private_dir.join(&node_output.base_source.0);
                            if let Err(e) = fs::write(&base_source_path, &node_output.base_source.1) {
                                eprintln!("      ⚠️  Failed to write base node data source: {}", e);
                            } else {
                                println!("      ✓ {}", node_output.base_source.0);
                            }

                            // Write NodeData subclass headers
                            for (filename, header) in &node_output.node_data_headers {
                                let path = layout.public_dir.join(filename);
                                if let Err(e) = fs::write(&path, header) {
                                    eprintln!("      ⚠️  Failed to write node data header {}: {}", filename, e);
                                } else {
                                    println!("      ✓ {}", filename);
                                }
                            }

                            // Write NodeData subclass sources
                            for (filename, source) in &node_output.node_data_sources {
                                let path = layout.private_dir.join(filename);
                                if let Err(e) = fs::write(&path, source) {
                                    eprintln!("      ⚠️  Failed to write node data source {}: {}", filename, e);
                                } else {
                                    println!("      ✓ {}", filename);
                                }
                            }
                        }
                        Err(e) => {
                            eprintln!("      ⚠️  Failed to generate node data for {}: {}", runtime_ir.name, e);
                        }
                    }

                    // Generate GraphInstance class
                    let instance_gen = ue5_graphs::InstanceGenerator::new(
                        &graph_editor,
                        &ue5_config.plugin_name,
                    );

                    match instance_gen.generate() {
                        Ok(instance_output) => {
                            // Write instance header
                            let header_path = layout.public_dir.join(&instance_output.instance_header.0);
                            if let Err(e) = fs::write(&header_path, &instance_output.instance_header.1) {
                                eprintln!("      ⚠️  Failed to write instance header: {}", e);
                            } else {
                                println!("      ✓ {}", instance_output.instance_header.0);
                            }

                            // Write instance source
                            let source_path = layout.private_dir.join(&instance_output.instance_source.0);
                            if let Err(e) = fs::write(&source_path, &instance_output.instance_source.1) {
                                eprintln!("      ⚠️  Failed to write instance source: {}", e);
                            } else {
                                println!("      ✓ {}", instance_output.instance_source.0);
                            }

                            // Note: NodeData files are already written above from node_gen.generate()
                        }
                        Err(e) => {
                            eprintln!("      ⚠️  Failed to generate instance for {}: {}", runtime_ir.name, e);
                        }
                    }

                    // Generate GraphAsset class
                    let asset_gen = ue5_graphs::AssetGenerator::new(
                        &graph_editor,
                        &ue5_config.plugin_name,
                    );

                    match asset_gen.generate() {
                        Ok(asset_output) => {
                            // Write asset header
                            let asset_header_path = layout.public_dir.join(&asset_output.asset_header.0);
                            if let Err(e) = fs::write(&asset_header_path, &asset_output.asset_header.1) {
                                eprintln!("      ⚠️  Failed to write asset header: {}", e);
                            } else {
                                println!("      ✓ {}", asset_output.asset_header.0);
                            }

                            // Write asset source
                            let asset_source_path = layout.private_dir.join(&asset_output.asset_source.0);
                            if let Err(e) = fs::write(&asset_source_path, &asset_output.asset_source.1) {
                                eprintln!("      ⚠️  Failed to write asset source: {}", e);
                            } else {
                                println!("      ✓ {}", asset_output.asset_source.0);
                            }
                        }
                        Err(e) => {
                            eprintln!("      ⚠️  Failed to generate asset for {}: {}", runtime_ir.name, e);
                        }
                    }

                    // Generate GraphData class
                    match ue5_graphs::generate_graph_data_header(
                        &graph_editor,
                        &ue5_config.plugin_name,
                    ) {
                        Ok(header) => {
                            let path = layout.public_dir.join(format!("{}GraphData.h", runtime_ir.name));
                            if let Err(e) = fs::write(&path, &header) {
                                eprintln!("      ⚠️  Failed to write graph data header: {}", e);
                            } else {
                                println!("      ✓ {}GraphData.h", runtime_ir.name);
                            }
                        }
                        Err(e) => {
                            eprintln!("      ⚠️  Failed to generate graph data header: {}", e);
                        }
                    }

                    match ue5_graphs::generate_graph_data_source(
                        &graph_editor,
                        &ue5_config.plugin_name,
                    ) {
                        Ok(source) => {
                            let path = layout.private_dir.join(format!("{}GraphData.cpp", runtime_ir.name));
                            if let Err(e) = fs::write(&path, &source) {
                                eprintln!("      ⚠️  Failed to write graph data source: {}", e);
                            } else {
                                println!("      ✓ {}GraphData.cpp", runtime_ir.name);
                            }
                        }
                        Err(e) => {
                            eprintln!("      ⚠️  Failed to generate graph data source: {}", e);
                        }
                    }

                    println!("      ✅ Runtime graph system complete: {}", runtime_ir.name);
                }
                Err(e) => {
                    eprintln!("      ❌ Failed to convert runtime graph {} to IR: {}", graph_runtime_def.name, e);
                    eprintln!("         Error: {}", e);
                }
            }
        }

        println!();
    }

    // STEP 3.6: Generate Blueprints (Binary .uasset + C++ Factory fallback)
    #[cfg(feature = "ue5")]
    {
        let actors: Vec<&kain_core::ast::Actor> = typed_program.items.iter()
            .filter_map(|item| {
                if let kain_core::types::TypedItem::Actor(typed_actor) = item {
                    Some(&typed_actor.ast)
                } else {
                    None
                }
            })
            .collect();

        if !actors.is_empty() {
            println!("📐 Generating Blueprints for {} actors...", actors.len());

            // Ensure Content/Blueprints exists
            let bp_content_dir = layout.plugin_root.join("Content").join("Blueprints");
            if let Err(e) = fs::create_dir_all(&bp_content_dir) {
                 eprintln!("   ⚠️  Failed to create blueprints content dir: {}", e);
            }

            // Ensure Generated/Factories exists for fallback
            let factory_dir = layout.private_dir.join("Generated").join("Factories");
            if let Err(e) = fs::create_dir_all(&factory_dir) {
                 eprintln!("   ⚠️  Failed to create factory dir: {}", e);
            }

            for actor in actors {
                match ue5_blueprints::conversion::from_ast(actor) {
                    Ok(bp_ir) => {
                        // Attempt binary .uasset generation
                        match ue5_blueprints::generate_uasset(&bp_ir) {
                             Ok(Some(bytes)) => {
                                 let path = bp_content_dir.join(format!("{}.uasset", bp_ir.name));
                                 match fs::write(&path, &bytes) {
                                     Ok(_) => {
                                         println!("   ✓ Binary blueprint: {} ({} bytes)", bp_ir.name, bytes.len());
                                         generated_assets.push(GeneratedAsset {
                                             package_name: format!("/Game/Blueprints/{}", bp_ir.name),
                                             asset_name: bp_ir.name.clone(),
                                             class_path: "/Script/Engine.Blueprint",
                                         });
                                     }
                                     Err(e) => eprintln!("   ⚠️  Failed to write .uasset for {}: {}", bp_ir.name, e),
                                 }
                             }
                             // Fallback to C++ Factory
                             Ok(None) => {
                                 println!("   ℹ️  {} has event graph, using C++ factory fallback.", bp_ir.name);
                                 let (header, source) = ue5_blueprints::generate_factory(&bp_ir);
                                 let h_path = factory_dir.join(format!("{}Factory.h", bp_ir.name));
                                 let cpp_path = factory_dir.join(format!("{}Factory.cpp", bp_ir.name));
                                 let _ = fs::write(&h_path, header);
                                 let _ = fs::write(&cpp_path, source);
                             }
                             Err(e) => {
                                 eprintln!("   ❌ Blueprint generation error for {}: {}", bp_ir.name, e);
                             }
                        }
                    }
                    Err(e) => {
                         eprintln!("   ❌ Failed to convert actor {} to blueprint IR: {}", actor.name, e);
                    }
                }
            }
            println!();
        }
    }

    // STEP 3.7: Generate DataAssets (Binary .uasset for every @data_asset struct)
    //
    // Every KAIN struct tagged with `@data_asset` (or `@data_asset("ClassName")`) is
    // serialised into a UDataAsset-compatible .uasset and placed in Content/DataAssets/.
    // The class path resolves via the same alias table as write_data_asset.
    #[cfg(feature = "ue5")]
    {
        use ue5_editor::data_asset_writer::{
            fields_from_struct, resolve_data_asset_class, write_data_asset,
        };

        let data_asset_structs: Vec<&kain_core::ast::Struct> = typed_program.items.iter()
            .filter_map(|item| {
                if let kain_core::types::TypedItem::Struct(ts) = item {
                    // Only emit if the struct carries a @data_asset attribute
                    if ts.ast.attributes.iter().any(|a| a.name == "data_asset") {
                        return Some(&ts.ast);
                    }
                }
                None
            })
            .collect();

        if !data_asset_structs.is_empty() {
            println!("📦 Generating {} DataAsset(s)...", data_asset_structs.len());

            let da_content_dir = layout.plugin_root.join("Content").join("DataAssets");
            if let Err(e) = fs::create_dir_all(&da_content_dir) {
                eprintln!("   ⚠️  Failed to create DataAssets content dir: {}", e);
            }

            // Derive UE5 engine version from the config engine_version field (if present)
            // or fall back to whatever KainEngineTarget::default() maps to.
            // This keeps the fallback in sync with a single definition (engine_target.rs).
            let engine_ver = ue5_config.engine_version
                .as_deref()
                .and_then(parse_engine_version)
                .unwrap_or_else(|| ue5_asset_utils::KainEngineTarget::default().as_serializer_version());

            for st in data_asset_structs {
                // Resolve optional class argument: @data_asset("MyPlugin.UMyClass")
                let class_path = resolve_data_asset_class(&st.attributes);

                // Convert struct fields → PropertyDef list
                let fields = fields_from_struct(st);

                // Asset name = struct name, e.g. `DA_ItemTable`
                let asset_name = &st.name;

                match write_data_asset(asset_name, &class_path, &fields, engine_ver) {
                    Ok(bytes) => {
                        let path = da_content_dir.join(format!("{}.uasset", asset_name));
                        match fs::write(&path, &bytes) {
                            Ok(_) => {
                                println!("   ✓ DataAsset: {} ({} bytes, class: {})",
                                    asset_name, bytes.len(), class_path);
                                generated_assets.push(GeneratedAsset {
                                    package_name: format!("/Game/DataAssets/{}", asset_name),
                                    asset_name: asset_name.clone(),
                                    class_path: "/Script/Engine.DataAsset",
                                });
                            }
                            Err(e) => eprintln!("   ⚠️  Failed to write DataAsset {}: {}", asset_name, e),
                        }
                    }
                    Err(e) => eprintln!("   ⚠️  Failed to generate DataAsset {}: {}", asset_name, e),
                }
            }
            println!();
        }
    }

    // STEP 4: Generate main plugin files
    println!();

    if ue5_config.modular_output {
        // MODULAR MODE: Generate separate .h/.cpp for each source file
        println!("🎯 Generating modular plugin files (per-file output)...");

        // Generate headers (master, delegates, EditorTypes)
        let (master_header_path, _delegate_count, mut type_headers) =
            super::codegen::generate_headers(&layout, &ue5_config, &typed_program)?;

        // Register runtime graph aliases so regular UE5 codegen can resolve types used
        // by subsystems/components/functions (e.g., DialogueGraph -> DialogueGraphInstance.h).
        for runtime_def in &graph_runtimes {
            type_headers.insert(runtime_def.name.clone(), format!("{}Instance.h", runtime_def.name));
        }

        // Generate per-item runtime files
        let runtime_manifest = super::codegen::generate_runtime_items(
            &layout,
            &ue5_config,
            &typed_program,
            &shader_names,
            &type_headers,
            &master_header_path,
            &symbol_source_map,
            embed_kain,
        )?;

        // Generate stdlib functions
        super::codegen::generate_stdlib_functions(&layout, &ue5_config, &typed_program, &type_headers, &master_header_path)?;

        // Generate blueprint function library
        super::codegen::generate_blueprint_library(&layout, &ue5_config, &typed_program, &type_headers, &master_header_path)?;

        // Generate editor tools
        let editor_manifest = super::codegen::generate_editor_items(
            &layout,
            &ue5_config,
            &typed_program,
            &master_header_path,
            &symbol_source_map,
        )?;

        println!();
        println!("   ✅ Master header finalized with all module includes");

        // NOTE: Do NOT add .generated.h to the master header.
        // The master header is a forward-declaration + include aggregation file, NOT a UHT-processed type.
        // Individual type headers (EHealthStatus.h, ADiagnosticPreviewActor.h, etc.) already have their
        // own .generated.h includes where needed (alongside UCLASS/USTRUCT/UENUM macros).

        // Detect if program has shaders (needed for module registration)
        let has_shaders = !shader_names.is_empty();
        let has_material_factories = !material_graphs.is_empty();

        // Generate module registration
        super::codegen::generate_module_registration(
            &layout,
            &ue5_config,
            &typed_program,
            has_shaders,
            has_material_factories,
        )?;

        super::codegen::write_cross_module_manifests(
            &layout,
            &ue5_config,
            &runtime_manifest,
            &editor_manifest,
        )?;

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

    // STEP 6: Stamp AssetRegistry.bin
    //
    // After all binary assets have been written, update (or create) the plugin's
    // AssetRegistry.bin so the Unreal Editor sees all generated content immediately
    // in the Content Browser — no full asset scan needed on first open.
    //
    // This step is intentionally non-fatal: a registry failure never blocks the build.
    #[cfg(feature = "ue5")]
    {
        use super::registry_writer::{register_assets, AssetEntry};

        if !generated_assets.is_empty() {
            println!("📋 Updating AssetRegistry.bin ({} assets)...", generated_assets.len());

            // AssetRegistry.bin must live inside Content/ — that is the path
            // UE's plugin asset-registry scanner reads on startup.
            // (plugin_root/AssetRegistry.bin is NOT scanned by the editor.)
            let registry_path = layout.plugin_root.join("Content").join("AssetRegistry.bin");

            // Derive UE5 engine version (same logic as DataAsset step)
            let engine_ver = ue5_config.engine_version
                .as_deref()
                .and_then(parse_engine_version)
                .unwrap_or_else(|| ue5_asset_utils::KainEngineTarget::default().as_serializer_version());

            // Build data-driven AssetEntry descriptors from the accumulated list
            let entries: Vec<AssetEntry> = generated_assets
                .iter()
                .map(|a| AssetEntry::new(&a.package_name, &a.asset_name, a.class_path))
                .collect();

            match register_assets(&registry_path, &entries, engine_ver) {
                Ok(()) => println!("   ✓ AssetRegistry.bin updated: {}", registry_path.display()),
                Err(e) => eprintln!("   ⚠️  AssetRegistry update failed (non-fatal): {}", e),
            }
            println!();
        }
    }

    // STEP 7: Validate generated C++ code
    println!();
    println!("🔬 Validating generated C++ code...");
    match super::cpp_validator::validate_cpp_with_resharper(&layout.plugin_root) {
        Ok(()) => {
            // ReSharper validation succeeded or was skipped
        }
        Err(e) => {
            // ReSharper failed, try basic validation
            eprintln!("   ⚠️  ReSharper validation failed: {}", e);
            println!("   🔄 Falling back to basic validation...");
            super::cpp_validator::validate_cpp_basic(&layout.plugin_root)?;
        }
    }
    println!();

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
    #[cfg(feature = "ue5")]
    println!("📦 Binary assets stamped: {}", generated_assets.len());
    Ok(())
}

/// Load stdlib + user source files, parse, validate, and type-check.
/// Returns (typed_program, shader_names, stdlib_files, user_source_files, symbol_source_map, material_graphs, material_functions, graph_editors, graph_runtimes).
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: Option<&super::config::PackageManifest>,
    cwd: &PathBuf,
) -> KainResult<(kain_core::types::TypedProgram, Vec<String>, Vec<PathBuf>, Vec<PathBuf>, HashMap<String, PathBuf>, Vec<kain_core::ast::MaterialGraphDef>, Vec<kain_core::ast::MaterialFunctionDef>, Vec<kain_core::ast::GraphEditorDef>, Vec<kain_core::ast::GraphRuntimeDef>)> {
    // STEP 1: Load stdlib files FIRST (they contain type definitions)
    let mut all_source_files = Vec::new();
    let mut stdlib_files = Vec::new();
    
    // Determine stdlib path with prioritized search strategy
    let stdlib_search_paths: Vec<PathBuf> = if let Some(custom_path) = &ue5_config.stdlib_path {
        // Priority 1: Explicit path from KAIN.toml (highest priority)
        vec![custom_path.clone()]
    } else if let Ok(env_path) = std::env::var("KAIN_STDLIB_PATH") {
        // Priority 2: KAIN_STDLIB_PATH environment variable
        vec![PathBuf::from(env_path)]
    } else {
        // Priority 3: Auto-discovery via filesystem walking
        let mut roots = Vec::new();
        
        // Walk up from current working directory
        if let Ok(mut current) = std::env::current_dir() {
            loop {
                // Check for Kain/stdlib first (highest priority for sibling pattern)
                let kain_stdlib = current.join("Kain").join("stdlib");
                if kain_stdlib.exists() && kain_stdlib.is_dir() {
                    roots.push(kain_stdlib);
                    break;
                }
                
                // Check for stdlib in current directory (but only if it has ue5/ subdirectory)
                let stdlib_dir = current.join("stdlib");
                if stdlib_dir.exists() && stdlib_dir.is_dir() {
                    let ue5_subdir = stdlib_dir.join("ue5");
                    if ue5_subdir.exists() && ue5_subdir.is_dir() {
                        roots.push(stdlib_dir);
                        break;
                    }
                }
                
                // Move to parent directory
                if let Some(parent) = current.parent() {
                    current = parent.to_path_buf();
                } else {
                    break; // Reached filesystem root
                }
            }
        }
        
        roots
    };
    
    // Try each search path, checking ue5/ subdirectory first
    for stdlib_root in stdlib_search_paths {
        // Try <root>/ue5/ first (UE5-specific stdlib)
        let ue5_path = stdlib_root.join("ue5");
        let search_path = if ue5_path.exists() && ue5_path.is_dir() {
            ue5_path
        } else {
            stdlib_root.clone()
        };
        
        if search_path.exists() {
            if let Ok(entries) = fs::read_dir(&search_path) {
                for entry in entries {
                    if let Ok(entry) = entry {
                        let path = entry.path();
                        if path.extension().map_or(false, |e| e == "kn") {
                            // Skip README files (case-insensitive)
                            if let Some(name) = path.file_name() {
                                if name.to_string_lossy().to_lowercase().contains("readme") {
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
                println!("📚 Loaded stdlib from: {}", search_path.display());
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
    let mut symbol_source_map: HashMap<String, PathBuf> = HashMap::new();
    let mut all_span_mappers: Vec<(String, kain_core::diagnostics::SpanMapper)> = Vec::new();
    
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
        
        let span_mapper = kain_core::diagnostics::SpanMapper::new(&file_source);
        let file_name = source_path.file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("<unknown>");
        
        let ast = match kain_core::Parser::new(&tokens, &span_mapper, file_name).parse() {
            Ok(a) => a,
            Err(e) => {
                let file_name_str = source_path.file_name()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_else(|| source_path.display().to_string());
                let formatted_error = post_process::format_error_with_location(
                    &file_source, &e.to_string(), file_name_str
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

        for item in &ast.items {
            if let Some(symbol) = ast_item_symbol_name(item) {
                symbol_source_map.insert(symbol, source_path.clone());
            }
        }
        
        all_span_mappers.push((file_name.to_string(), span_mapper));
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
    
    // Extract material functions BEFORE type checking
    let material_functions: Vec<kain_core::ast::MaterialFunctionDef> = merged.items.iter()
        .filter_map(|item| {
            if let kain_core::ast::Item::MaterialFunction(def) = item {
                Some(def.clone())
            } else {
                None
            }
        })
        .collect();
    
    // Extract graph editors BEFORE type checking
    let graph_editors: Vec<kain_core::ast::GraphEditorDef> = merged.items.iter()
        .filter_map(|item| {
            if let kain_core::ast::Item::GraphEditor(def) = item {
                Some(def.clone())
            } else {
                None
            }
        })
        .collect();
    
    // Extract graph runtimes BEFORE type checking
    let graph_runtimes: Vec<kain_core::ast::GraphRuntimeDef> = merged.items.iter()
        .filter_map(|item| {
            if let kain_core::ast::Item::GraphRuntime(def) = item {
                Some(def.clone())
            } else {
                None
            }
        })
        .collect();
    
    // Filter out material graphs, material functions, graph editors, and graph runtimes from the program before type checking
    // (they will be processed separately for generation)
    merged.items.retain(|item| !matches!(item, 
        kain_core::ast::Item::MaterialGraph(_) | 
        kain_core::ast::Item::MaterialFunction(_) |
        kain_core::ast::Item::GraphEditor(_) |
        kain_core::ast::Item::GraphRuntime(_)
    ));
    
    // Type-check the MERGED program
    println!("🔍 Type checking merged program...");
    
    // Use first span_mapper for merged program type checking
    let (first_filename, first_span_mapper) = all_span_mappers.first()
        .ok_or_else(|| KainError::runtime("No source files to compile"))?;
    
    let typed_program = match kain_core::types::check(&merged, first_span_mapper, first_filename) {
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
    
    // Monomorphize (instantiate generic functions with concrete types)
    println!("🔄 Monomorphizing generic functions...");
    let mono_ast = match kain_core::monomorphize::monomorphize(&typed_program) {
        Ok(ma) => {
            println!("   ✓ Monomorphization complete");
            ma
        }
        Err(e) => {
            return Err(KainError::runtime(format!(
                "❌ Monomorphization error: {}", e
            )));
        }
    };
    // Convert MonomorphizedProgram back to TypedProgram for codegen
    let typed_program = kain_core::types::TypedProgram { items: mono_ast.items };
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
    match ue5::ue5::oracle::validate_program_full(&typed_program, &kb, &uht, first_span_mapper, first_filename) {
        Ok(()) => {
            println!("   ✓ Oracle validation passed");
        }
        Err(e) => {
            return Err(e);
        }
    }
    println!();
    
    Ok((typed_program, all_shader_names, stdlib_files, user_source_files, symbol_source_map, material_graphs, material_functions, graph_editors, graph_runtimes))
}

fn ast_item_symbol_name(item: &kain_core::ast::Item) -> Option<String> {
    match item {
        kain_core::ast::Item::Function(v) => Some(v.name.clone()),
        kain_core::ast::Item::Component(v) => Some(v.name.clone()),
        kain_core::ast::Item::Shader(v) => Some(v.name.clone()),
        kain_core::ast::Item::Actor(v) => Some(v.name.clone()),
        kain_core::ast::Item::Struct(v) => Some(v.name.clone()),
        kain_core::ast::Item::Enum(v) => Some(v.name.clone()),
        kain_core::ast::Item::Trait(v) => Some(v.name.clone()),
        kain_core::ast::Item::TypeAlias(v) => Some(v.name.clone()),
        kain_core::ast::Item::Const(v) => Some(v.name.clone()),
        kain_core::ast::Item::MaterialGraph(v) => Some(v.name.clone()),
        kain_core::ast::Item::MaterialFunction(v) => Some(v.name.clone()),
        kain_core::ast::Item::GraphEditor(v) => Some(v.name.clone()),
        kain_core::ast::Item::GraphRuntime(v) => Some(v.name.clone()),
        kain_core::ast::Item::StateMachine(v) => Some(v.name.clone()),
        kain_core::ast::Item::AsyncTask(v) => Some(v.name.clone()),
        kain_core::ast::Item::EditorModule(v) => Some(v.name.clone()),
        _ => None,
    }
}

/// Map an engine version string from KAIN.toml to a raw `EngineVersion` value.
///
/// Delegates through [`KainEngineTarget`] so that each supported version
/// (5.0 – 5.7) maps to its **true native** `EngineVersion`, not a capped
/// fallback. Accepts formats like `"5.4"`, `"UE5_4"`, `"VER_UE5_4"`.
#[cfg(feature = "ue5")]
fn parse_engine_version(s: &str) -> Option<unreal_asset_base::engine_version::EngineVersion> {
    use ue5_asset_utils::KainEngineTarget;
    // Strip optional "UE" prefix and normalise ("UE5_4" → "5.4", etc.)
    let s = s.trim();
    let normalised = if let Some(rest) = s.strip_prefix("VER_UE") {
        rest.replace('_', ".")
    } else if let Some(rest) = s.strip_prefix("UE") {
        rest.replace('_', ".")
    } else {
        s.to_string()
    };
    KainEngineTarget::from_str(normalised.trim())
        .map(|t| t.as_serializer_version())
}

/// Convert AST MaterialGraphDef to IR MaterialGraph.
///
/// `surface_shaders` maps shader name → pre-emitted HLSL body for Surface-stage
/// shaders. When a `call(shader_name)` node is encountered in the graph, the
/// corresponding HLSL is embedded as a `MaterialNodeType::CustomHLSL` node
/// instead of resolving as an unknown function.
#[cfg(feature = "ue5")]
fn convert_material_graph(
    def: &kain_core::ast::MaterialGraphDef,
    surface_shaders: &std::collections::HashMap<String, String>,
) -> KainResult<ue5_materials::MaterialGraph> {
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
            is_dynamic: false, // Phase 7.1: dynamic marking happens post-construction
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
            let node_id = emit_expr(value, x, y, &mut node_counter, &mut nodes, &scope, surface_shaders);
            scope.insert(name.clone(), node_id);
        }
    }

    // Resolve outputs: each output.value is an Ident referring to a let-binding or input
    let mut outputs = MaterialOutputs::default();
    for output in &def.outputs {
        let node_id = emit_expr(&output.value, 400, 0, &mut node_counter, &mut nodes, &scope, surface_shaders);
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
        expose_parameters: false, // Phase 7.1: off by default; set by @dynamic attribute
    };

    // Phase 7.5: detect vertex shader usage from whether world_position_offset is connected
    let uses_vertex_shader = outputs.world_position_offset.is_some();

    Ok(MaterialGraph {
        name: def.name.clone(),
        inputs,
        outputs,
        properties,
        nodes,
        is_dynamic: false,               // Phase 7: set true when Time nodes are detected
        dynamic_parameters: Vec::new(),  // Phase 7.1: populated by mark_parameter_dynamic()
        uses_vertex_shader,              // Phase 7.5: auto-detected from output connections
        vertex_displacement_scale: None, // Phase 7.5: no explicit scale by default
    })
}

/// Recursively emit material nodes for an expression, returning the node_id of the root.
/// If the expression is a simple identifier already in scope, returns that node_id directly.
///
/// `surface_shaders` maps KAIN Surface shader names to their pre-emitted HLSL bodies.
/// When a function call matches a shader name, it is injected as a `CustomHLSL` node.
#[cfg(feature = "ue5")]
fn emit_expr(
    expr: &kain_core::ast::Expr,
    x: i32,
    y: i32,
    counter: &mut usize,
    nodes: &mut Vec<ue5_materials::MaterialNode>,
    scope: &std::collections::HashMap<String, String>,
    surface_shaders: &std::collections::HashMap<String, String>,
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
            let a = emit_expr(left, x - 200, y - 100, counter, nodes, scope, surface_shaders);
            let b = emit_expr(right, x - 200, y + 100, counter, nodes, scope, surface_shaders);
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
                emit_expr(&arg.value, ax, ay, counter, nodes, scope, surface_shaders)
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
                // ── Shader→Material bridge ───────────────────────────
                // When a call refers to a known Surface-stage shader name,
                // embed its pre-emitted HLSL body as a CustomHLSL node.
                // Each positional argument becomes a Float1 input pin named
                // after its node_id so the material graph can wire it up.
                name if surface_shaders.contains_key(name) => {
                    use ue5_materials::{CustomOutputType, CustomInput};
                    let hlsl_body = surface_shaders.get(name).cloned().unwrap_or_default();
                    // Map each positional argument to a typed input pin.
                    // The arg child nodes are already in `nodes`; CustomHLSL
                    // references them via their sequential "In0", "In1", … names.
                    let inputs: Vec<CustomInput> = arg_ids
                        .iter()
                        .enumerate()
                        .map(|(i, _node_id)| CustomInput {
                            name: format!("In{}", i),
                            input_type: CustomOutputType::Float3,
                        })
                        .collect();
                    MaterialNodeType::CustomHLSL {
                        code: hlsl_body,
                        output_type: CustomOutputType::Float3,
                        inputs,
                    }
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
            let input = emit_expr(object, x - 200, y, counter, nodes, scope, surface_shaders);
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
/// Convert AST MaterialFunctionDef to IR MaterialFunction.
/// Material functions are reusable node graphs that can be called from materials.
#[cfg(feature = "ue5")]
fn convert_material_function(
    def: &kain_core::ast::MaterialFunctionDef,
) -> KainResult<ue5_materials::MaterialFunction> {
    use ue5_materials::{MaterialFunction, MaterialFunctionInput, MaterialInputType,
                        MaterialNode, MaterialNodeType};
    use std::collections::HashMap;

    // Convert inputs
    let inputs: Vec<MaterialFunctionInput> = def.inputs.iter().map(|input| {
        MaterialFunctionInput {
            name: input.name.clone(),
            input_type: map_material_input_type(&input.ty),
            default_value: input.default.as_ref().map(|e| format!("{:?}", e)),
        }
    }).collect();

    // --- Expression → Node conversion ---
    let mut node_counter: usize = 0;
    let mut nodes: Vec<MaterialNode> = Vec::new();
    let mut scope: HashMap<String, String> = HashMap::new();

    // Grid layout
    let col_width = 300i32;
    let row_height = 200i32;

    // Create input nodes for each declared parameter
    for (row, input) in def.inputs.iter().enumerate() {
        let x = -800;
        let y = row as i32 * row_height;
        let id = format!("input_{}", input.name);
        
        // Material function inputs are represented as FunctionInput nodes
        // These will be converted to MaterialExpressionFunctionInput in the builder
        let node_type = MaterialNodeType::ConstantFloat { value: 0.0 }; // Placeholder - will be replaced by FunctionInput
        
        nodes.push(MaterialNode { id: id.clone(), node_type, position: (x, y) });
        scope.insert(input.name.clone(), id);
    }

    // Walk body let-bindings
    for (stmt_idx, stmt) in def.body.iter().enumerate() {
        if let kain_core::ast::MaterialStatement::Let { name, value, .. } = stmt {
            let x = -500 + stmt_idx as i32 * col_width;
            let y = 0;
            let surface_shaders = HashMap::new(); // Functions don't use surface shaders
            let node_id = emit_expr(value, x, y, &mut node_counter, &mut nodes, &scope, &surface_shaders);
            scope.insert(name.clone(), node_id);
        }
    }

    // Resolve output expression
    let surface_shaders = HashMap::new();
    let output_node_id = emit_expr(&def.output, 400, 0, &mut node_counter, &mut nodes, &scope, &surface_shaders);

    Ok(MaterialFunction {
        name: def.name.clone(),
        inputs,
        nodes,
        output: output_node_id,
        description: format!("Material function: {}", def.name),
    })
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
        engine_version: None,  // Use default (5.2)
        modules: Vec::new(),
        plugin_dependencies: Vec::new(),
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