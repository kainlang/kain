use serde::{Deserialize, Serialize};

use crate::{CompileTarget, TypedItem, TypedProgram};
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
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            let shader_name = shader.ast.name.clone();
            refs.push(RealtimeShaderBundleRef {
                key: format!("shader::{shader_name}::vertex"),
                shader: shader_name.clone(),
                module_name: shader_name.clone(),
                stage: "vertex".to_string(),
                entry_point: "main".to_string(),
                source: "kain-core".to_string(),
            });
            refs.push(RealtimeShaderBundleRef {
                key: format!("shader::{shader_name}::fragment"),
                shader: shader_name.clone(),
                module_name: shader_name,
                stage: "fragment".to_string(),
                entry_point: "main".to_string(),
                source: "kain-core".to_string(),
            });
        }
    }
    refs.sort_by(|left, right| left.key.cmp(&right.key));
    refs
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
            TypedItem::Shader(_) => caps.push("gpu.shader".to_string()),
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
    let mut requirements = vec![
        "runtime.contract.bundle".to_string(),
        "shader.bundle.metadata".to_string(),
    ];
    if !shader_bundle_refs.is_empty() {
        requirements.push("gpu.shader-bundles".to_string());
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
}
