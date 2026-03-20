use serde::{Deserialize, Serialize};

use crate::ast::{ShaderStage, Type};
use crate::{CompileTarget, TypedItem, TypedProgram, TypedShader};
use kain_ui::{UiBuildOutput, UiSurfaceKind};

pub const REALTIME_APP_BUNDLE_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeAppBundle {
    pub schema_version: u32,
    pub target: String,
    pub render: RenderSceneBundle,
    pub shader_bundle_refs: Vec<RealtimeShaderBundleRef>,
    pub assets: Vec<RealtimeAssetBinding>,
    pub tool_caps: Vec<String>,
    pub requirements: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RenderSceneBundle {
    pub scenes: Vec<RealtimeSceneBinding>,
    pub materials: Vec<CompiledMaterialDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeSceneBinding {
    pub viewport_node: String,
    pub viewport_kind: String,
    pub scene: String,
    pub title: Option<String>,
    pub material_refs: Vec<String>,
    pub shader_bundle_ref_keys: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CompiledMaterialDefinition {
    pub id: String,
    pub source: String,
    pub shader_bundle_ref_keys: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CompiledMaterialInstance {
    pub material: String,
    pub scene: Option<String>,
    pub viewport_node: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeShaderBundleRef {
    pub key: String,
    pub shader: String,
    pub module_name: String,
    pub stage: String,
    pub entry_point: String,
    pub source: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub workgroup_size: Option<[u32; 3]>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub dispatch_size: Option<[u32; 3]>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub resource_bindings: Vec<RealtimeResourceBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeResourceBinding {
    pub key: String,
    #[serde(rename = "type")]
    pub resource_type: String,
    pub stage: String,
    pub access: String,
    pub slot: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeAssetBinding {
    pub key: String,
    pub kind: String,
    pub source: String,
}

pub fn emit_realtime_app_bundle(
    program: &TypedProgram,
    ui_output: Option<&UiBuildOutput>,
    target: CompileTarget,
) -> RealtimeAppBundle {
    let shader_bundle_refs = collect_shader_bundle_refs(program);
    let scenes = collect_scene_bindings(ui_output, &shader_bundle_refs);
    let materials = collect_materials(program, &scenes);
    let assets = collect_assets(ui_output);
    let tool_caps = collect_tool_caps(program, ui_output);
    let requirements = collect_requirements(target, &scenes, &shader_bundle_refs, &tool_caps);

    RealtimeAppBundle {
        schema_version: REALTIME_APP_BUNDLE_SCHEMA_VERSION,
        target: compile_target_name(target).to_string(),
        render: RenderSceneBundle { scenes, materials },
        shader_bundle_refs,
        assets,
        tool_caps,
        requirements,
    }
}

pub fn realtime_app_bundle_to_json(
    bundle: &RealtimeAppBundle,
) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(bundle)
}

pub fn realtime_app_bundle_from_json(json: &str) -> Result<RealtimeAppBundle, serde_json::Error> {
    serde_json::from_str(json)
}

fn collect_shader_bundle_refs(program: &TypedProgram) -> Vec<RealtimeShaderBundleRef> {
    let mut refs = Vec::new();
    collect_shader_bundle_refs_into(&program.items, &mut refs);
    refs.sort_by(|left, right| left.key.cmp(&right.key));
    refs
}

fn collect_shader_bundle_refs_into(items: &[TypedItem], output: &mut Vec<RealtimeShaderBundleRef>) {
    for item in items {
        match item {
            TypedItem::Shader(shader) => {
                output.push(shader_bundle_ref(shader));
            }
            TypedItem::Mod(module) => collect_shader_bundle_refs_into(&module.items, output),
            _ => {}
        }
    }
}

fn shader_bundle_ref(shader: &TypedShader) -> RealtimeShaderBundleRef {
    let stage = shader_ref_stage_name(shader.ast.stage).to_string();
    let key = format!("shader::{}::{}", shader.ast.name, stage);
    let resource_bindings = collect_shader_resource_bindings(shader, &stage);
    let (workgroup_size, dispatch_size) = if matches!(shader.ast.stage, ShaderStage::Compute) {
        (
            Some(compute_workgroup_size(shader)),
            Some(default_compute_dispatch_size()),
        )
    } else {
        (None, None)
    };

    RealtimeShaderBundleRef {
        key,
        shader: shader.ast.name.clone(),
        module_name: shader.ast.name.clone(),
        stage,
        entry_point: "main".to_string(),
        source: "kain-core".to_string(),
        workgroup_size,
        dispatch_size,
        resource_bindings,
    }
}

fn shader_ref_stage_name(stage: ShaderStage) -> &'static str {
    match stage {
        ShaderStage::Vertex => "vertex",
        ShaderStage::Fragment | ShaderStage::Surface => "fragment",
        ShaderStage::Compute => "compute",
    }
}

fn compute_workgroup_size(shader: &TypedShader) -> [u32; 3] {
    let mut size = [8, 8, 1];
    for uniform in &shader.ast.uniforms {
        match uniform.name.as_str() {
            "LOCAL_SIZE_X" => size[0] = 8,
            "LOCAL_SIZE_Y" => size[1] = 8,
            "LOCAL_SIZE_Z" => size[2] = 1,
            _ => {}
        }
    }
    size
}

fn default_compute_dispatch_size() -> [u32; 3] {
    [1, 1, 1]
}

fn collect_shader_resource_bindings(
    shader: &TypedShader,
    stage: &str,
) -> Vec<RealtimeResourceBinding> {
    let mut bindings = Vec::new();

    for uniform in &shader.ast.uniforms {
        if matches!(shader.ast.stage, ShaderStage::Compute)
            && matches!(
                uniform.name.as_str(),
                "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z"
            )
        {
            continue;
        }

        let resource_type = shader_resource_type(&uniform.ty).to_string();
        let access = shader_resource_access(&uniform.ty, stage).to_string();
        bindings.push(RealtimeResourceBinding {
            key: uniform.name.clone(),
            resource_type,
            stage: stage.to_string(),
            access,
            slot: uniform.binding,
        });
    }

    bindings.sort_by(|left, right| left.slot.cmp(&right.slot).then(left.key.cmp(&right.key)));
    bindings
}

fn shader_resource_type(ty: &Type) -> &'static str {
    match ty {
        Type::Named { name, .. } if name == "Sampler2D" => "sampler2d",
        Type::Named { name, .. } if name == "StorageBuffer" => "storage_buffer",
        _ => "uniform",
    }
}

fn shader_resource_access(ty: &Type, stage: &str) -> &'static str {
    match ty {
        Type::Named { name, .. } if name == "Sampler2D" => "sample",
        Type::Named { name, .. } if name == "StorageBuffer" && stage == "compute" => "read_write",
        Type::Named { name, .. } if name == "StorageBuffer" => "read",
        _ => "read",
    }
}

fn collect_scene_bindings(
    ui_output: Option<&UiBuildOutput>,
    shader_bundle_refs: &[RealtimeShaderBundleRef],
) -> Vec<RealtimeSceneBinding> {
    let Some(output) = ui_output else {
        return Vec::new();
    };

    let default_shader_keys = shader_bundle_refs
        .iter()
        .map(|entry| entry.key.clone())
        .collect::<Vec<_>>();
    let mut scenes = Vec::new();

    for surface in &output.systems.surfaces {
        let Some(node) = output.tree.node(surface.node) else {
            continue;
        };
        let viewport_kind = match surface.kind {
            UiSurfaceKind::Viewport3D => "viewport3d",
            UiSurfaceKind::Viewport2D => "viewport2d",
            _ => continue,
        };
        let scene = node
            .props
            .get("scene")
            .and_then(|value| value.as_str())
            .map(str::trim)
            .filter(|value| !value.is_empty())
            .unwrap_or("default")
            .to_string();
        let material_refs = node
            .props
            .get("material")
            .and_then(|value| value.as_str())
            .map(|value| vec![value.to_string()])
            .unwrap_or_default();

        scenes.push(RealtimeSceneBinding {
            viewport_node: surface.id.clone(),
            viewport_kind: viewport_kind.to_string(),
            scene,
            title: surface.title.clone(),
            material_refs,
            shader_bundle_ref_keys: default_shader_keys.clone(),
        });
    }

    scenes.sort_by(|left, right| left.viewport_node.cmp(&right.viewport_node));
    scenes
}

fn collect_materials(
    program: &TypedProgram,
    scenes: &[RealtimeSceneBinding],
) -> Vec<CompiledMaterialDefinition> {
    let mut material_ids = scenes
        .iter()
        .flat_map(|scene| scene.material_refs.iter().cloned())
        .collect::<Vec<_>>();

    for item in &program.items {
        match item {
            TypedItem::MaterialGraph(graph) => material_ids.push(graph.name.clone()),
            TypedItem::MaterialFunction(function) => material_ids.push(function.name.clone()),
            _ => {}
        }
    }

    material_ids.sort();
    material_ids.dedup();

    material_ids
        .into_iter()
        .map(|id| CompiledMaterialDefinition {
            id,
            source: "kain-core".to_string(),
            shader_bundle_ref_keys: Vec::new(),
        })
        .collect()
}

fn collect_assets(ui_output: Option<&UiBuildOutput>) -> Vec<RealtimeAssetBinding> {
    let Some(output) = ui_output else {
        return Vec::new();
    };

    let mut assets = Vec::new();
    for node in output.tree.nodes.values() {
        if let Some(asset) = node.props.get("asset").and_then(|value| value.as_str()) {
            let asset = asset.trim();
            if asset.is_empty() {
                continue;
            }
            assets.push(RealtimeAssetBinding {
                key: format!("asset::{asset}"),
                kind: "runtime".to_string(),
                source: asset.to_string(),
            });
        }
    }
    assets.sort_by(|left, right| left.key.cmp(&right.key));
    assets.dedup_by(|left, right| left.key == right.key);
    assets
}

fn collect_tool_caps(program: &TypedProgram, ui_output: Option<&UiBuildOutput>) -> Vec<String> {
    let mut caps = Vec::new();
    if let Some(output) = ui_output {
        if output
            .systems
            .surfaces
            .iter()
            .any(|surface| surface.kind == UiSurfaceKind::Viewport3D)
        {
            caps.push("viewport.3d".to_string());
        }
        if output
            .systems
            .surfaces
            .iter()
            .any(|surface| surface.kind == UiSurfaceKind::Graph)
        {
            caps.push("tool.graph".to_string());
        }
        if output
            .systems
            .surfaces
            .iter()
            .any(|surface| surface.kind == UiSurfaceKind::Timeline)
        {
            caps.push("tool.timeline".to_string());
        }
    }

    for item in &program.items {
        match item {
            TypedItem::Actor(_) => caps.push("scene.actors".to_string()),
            TypedItem::Shader(shader) => {
                caps.push("gpu.shader".to_string());
                if matches!(shader.ast.stage, ShaderStage::Compute) {
                    caps.push("gpu.compute".to_string());
                }
            }
            TypedItem::GraphEditor(_) => caps.push("tool.graph-editor".to_string()),
            TypedItem::GraphRuntime(_) => caps.push("tool.graph-runtime".to_string()),
            _ => {}
        }
    }

    caps.sort();
    caps.dedup();
    caps
}

fn collect_requirements(
    target: CompileTarget,
    scenes: &[RealtimeSceneBinding],
    shader_bundle_refs: &[RealtimeShaderBundleRef],
    tool_caps: &[String],
) -> Vec<String> {
    let has_compute_refs = shader_bundle_refs
        .iter()
        .any(|entry| entry.stage == "compute");
    let mut requirements = vec![
        "runtime.contract.bundle".to_string(),
        "shader.bundle.metadata".to_string(),
    ];
    if !shader_bundle_refs.is_empty() {
        requirements.push("gpu.shader-bundles".to_string());
    }
    if has_compute_refs {
        requirements.push("gpu.compute-dispatch".to_string());
        requirements.push("interop.shared-buffer".to_string());
        requirements.push("data.continuous-stream".to_string());
    }
    if !scenes.is_empty() {
        requirements.push("scene.runtime-bundle".to_string());
    }
    if tool_caps.iter().any(|entry| entry == "viewport.3d") {
        requirements.push("viewport.presented-or-fallback".to_string());
    }
    match target {
        CompileTarget::Rust => requirements.push("host.native-ui".to_string()),
        CompileTarget::Llvm => requirements.push("host.raw-native".to_string()),
        _ => {}
    }
    requirements.sort();
    requirements.dedup();
    requirements
}

fn compile_target_name(target: CompileTarget) -> &'static str {
    match target {
        CompileTarget::Wasm => "wasm",
        CompileTarget::Js => "js",
        CompileTarget::Ts => "ts",
        CompileTarget::Hybrid => "hybrid",
        CompileTarget::Llvm => "llvm",
        CompileTarget::Rust => "rust",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Ue5 => "ue5",
        CompileTarget::Ue5Editor => "ue5-editor",
        CompileTarget::Usf => "usf",
        CompileTarget::Spirv => "spirv",
        CompileTarget::Hlsl => "hlsl",
        CompileTarget::Interpret => "interpret",
        CompileTarget::Test => "test",
        CompileTarget::Ks => "ks",
    }
}

#[cfg(test)]
mod tests {
    use crate::ui::build_ui_output_from_source;
    use crate::{
        diagnostics, emit_realtime_app_bundle, realtime_app_bundle_from_json, types, CompileTarget,
        Lexer, Parser,
    };

    #[test]
    fn emits_realtime_bundle_with_viewport_scene_binding() {
        let source = r#"
component App():
    render <panel>
        <viewport3d title="Hero" scene="magma_terraces" material="terrain" />
    </panel>
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = diagnostics::SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<input>")
            .parse()
            .expect("ast");
        let typed = types::check(&ast, &span_mapper, "<input>").expect("typed");
        let ui = build_ui_output_from_source(source, "App").expect("ui");

        let bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Rust);
        assert_eq!(bundle.target, "rust");
        assert_eq!(bundle.render.scenes.len(), 1);
        assert_eq!(bundle.render.scenes[0].scene, "magma_terraces");
        assert_eq!(
            bundle.render.scenes[0].material_refs,
            vec!["terrain".to_string()]
        );
        assert!(bundle
            .requirements
            .iter()
            .any(|entry| entry == "scene.runtime-bundle"));
    }

    #[test]
    fn deserializes_realtime_bundle_with_scene_binding() {
        let json = r#"{
  "schema_version": 1,
  "target": "rust",
  "render": {
    "scenes": [
      {
        "viewport_node": "surface.node.6",
        "viewport_kind": "viewport3d",
        "scene": "magma_terraces",
        "title": "Viewport",
        "material_refs": ["terrain"],
        "shader_bundle_ref_keys": ["shader::terrain::fragment"]
      }
    ],
    "materials": [
      {
        "id": "terrain",
        "source": "kain-core",
        "shader_bundle_ref_keys": ["shader::terrain::fragment"]
      }
    ]
  },
  "shader_bundle_refs": [],
  "assets": [],
  "tool_caps": ["viewport.3d"],
  "requirements": ["scene.runtime-bundle"]
}"#;

        let bundle =
            realtime_app_bundle_from_json(json).expect("realtime bundle should deserialize");
        assert_eq!(bundle.render.scenes[0].scene, "magma_terraces");
        assert_eq!(bundle.render.materials[0].id, "terrain");
    }

    #[test]
    fn emits_compute_bundle_metadata_for_native_runtime_consumers() {
        let source = r#"
shader compute TensorBlend() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    dst[idx] = src[idx]
    return
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = diagnostics::SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<input>")
            .parse()
            .expect("ast");
        let typed = types::check(&ast, &span_mapper, "<input>").expect("typed");

        let bundle = emit_realtime_app_bundle(&typed, None, CompileTarget::Llvm);
        assert_eq!(bundle.shader_bundle_refs.len(), 1);
        assert_eq!(bundle.shader_bundle_refs[0].stage, "compute");
        assert_eq!(bundle.shader_bundle_refs[0].workgroup_size, Some([8, 8, 1]));
        assert_eq!(bundle.shader_bundle_refs[0].dispatch_size, Some([1, 1, 1]));
        assert_eq!(bundle.shader_bundle_refs[0].resource_bindings.len(), 2);
        assert!(bundle
            .requirements
            .iter()
            .any(|entry| entry == "gpu.compute-dispatch"));
        assert!(bundle
            .requirements
            .iter()
            .any(|entry| entry == "interop.shared-buffer"));
        assert!(bundle.tool_caps.iter().any(|entry| entry == "gpu.compute"));
    }
}
