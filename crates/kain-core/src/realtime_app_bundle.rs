use serde::{Deserialize, Serialize};

use crate::ast::{ComputeMetadata, ShaderStage, Type, COMPUTE_PLAN_CAPABILITY_KEY};
use crate::{CompileTarget, TypedItem, TypedProgram, TypedShader};
use kain_ui::{
    UiBuildOutput, UiSurfaceCompositionMode, UiSurfaceKind, UiSurfaceRendererPreference,
};

pub const REALTIME_APP_BUNDLE_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeAppBundle {
    pub schema_version: u32,
    pub target: String,
    pub render: RenderSceneBundle,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub shader_canvases: Vec<RealtimeShaderCanvasBinding>,
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
pub struct RealtimeShaderCanvasBinding {
    pub surface_id: String,
    pub shader_ref: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub shader_bundle_ref_key: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub shader_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub module_name: Option<String>,
    pub stage: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub entry_point: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub derived_format: Option<String>,
    pub renderer_preference: String,
    pub composition_mode: String,
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
    pub execution_domain: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub workgroup_size: Option<[u32; 3]>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub dispatch_size: Option<[u32; 3]>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub resource_bindings: Vec<RealtimeResourceBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub tensor_bindings: Vec<RealtimeTensorBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub stream_bindings: Vec<RealtimeStreamBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub neural_nodes: Vec<RealtimeNeuralNode>,
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
pub struct RealtimeTensorBinding {
    pub key: String,
    pub element_type: String,
    pub shape: Vec<String>,
    pub role: String,
    pub contract: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeStreamBinding {
    pub key: String,
    pub direction: String,
    pub cadence: String,
    pub contract: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeNeuralNode {
    pub key: String,
    pub op: String,
    pub inputs: Vec<String>,
    pub outputs: Vec<String>,
    pub stateful: bool,
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
    let shader_canvases = collect_shader_canvas_bindings(ui_output, &shader_bundle_refs);
    let scenes = collect_scene_bindings(ui_output, &shader_bundle_refs);
    let materials = collect_materials(program, &scenes);
    let assets = collect_assets(ui_output);
    let has_explicit_compute_metadata = program_has_explicit_compute_metadata(program);
    let tool_caps = collect_tool_caps(
        program,
        ui_output,
        &shader_canvases,
        has_explicit_compute_metadata,
    );
    let requirements = collect_requirements(
        target,
        &scenes,
        &shader_canvases,
        &shader_bundle_refs,
        &tool_caps,
        has_explicit_compute_metadata,
    );

    RealtimeAppBundle {
        schema_version: REALTIME_APP_BUNDLE_SCHEMA_VERSION,
        target: compile_target_name(target).to_string(),
        render: RenderSceneBundle { scenes, materials },
        shader_canvases,
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

fn collect_shader_canvas_bindings(
    ui_output: Option<&UiBuildOutput>,
    shader_bundle_refs: &[RealtimeShaderBundleRef],
) -> Vec<RealtimeShaderCanvasBinding> {
    let Some(output) = ui_output else {
        return Vec::new();
    };

    let mut bindings = output
        .systems
        .surfaces
        .iter()
        .filter_map(|surface| {
            let shader = surface.shader.as_ref()?;
            let resolved_shader_ref =
                resolve_surface_shader_ref(shader.shader_ref.as_str(), shader_bundle_refs);
            Some(RealtimeShaderCanvasBinding {
                surface_id: surface.id.clone(),
                shader_ref: shader.shader_ref.clone(),
                shader_bundle_ref_key: resolved_shader_ref.as_ref().map(|entry| entry.key.clone()),
                shader_name: resolved_shader_ref.as_ref().map(|entry| entry.shader.clone()),
                module_name: resolved_shader_ref
                    .as_ref()
                    .map(|entry| entry.module_name.clone()),
                stage: shader
                    .stage
                    .clone()
                    .or_else(|| resolved_shader_ref.as_ref().map(|entry| entry.stage.clone()))
                    .unwrap_or_else(|| "fragment".to_string()),
                entry_point: shader.entry_point.clone().or_else(|| {
                    resolved_shader_ref
                        .as_ref()
                        .map(|entry| entry.entry_point.clone())
                }),
                derived_format: shader.derived_format.clone(),
                renderer_preference: surface_renderer_preference_name(surface.renderer_preference)
                    .to_string(),
                composition_mode: surface_composition_mode_name(surface.composition_mode)
                    .to_string(),
            })
        })
        .collect::<Vec<_>>();

    bindings.sort_by(|left, right| left.surface_id.cmp(&right.surface_id));
    bindings
}

fn resolve_surface_shader_ref<'a>(
    shader_ref: &str,
    shader_bundle_refs: &'a [RealtimeShaderBundleRef],
) -> Option<&'a RealtimeShaderBundleRef> {
    let trimmed = shader_ref.trim();
    if trimmed.is_empty() {
        return None;
    }

    shader_bundle_refs.iter().find(|entry| entry.key == trimmed).or_else(|| {
        shader_bundle_refs
            .iter()
            .find(|entry| entry.module_name == trimmed || entry.shader == trimmed)
    })
}

fn surface_renderer_preference_name(renderer: UiSurfaceRendererPreference) -> &'static str {
    match renderer {
        UiSurfaceRendererPreference::Auto => "auto",
        UiSurfaceRendererPreference::Native => "native",
        UiSurfaceRendererPreference::Dom => "dom",
        UiSurfaceRendererPreference::Wgpu => "wgpu",
        UiSurfaceRendererPreference::Shader => "shader",
    }
}

fn surface_composition_mode_name(mode: UiSurfaceCompositionMode) -> &'static str {
    match mode {
        UiSurfaceCompositionMode::Host => "host",
        UiSurfaceCompositionMode::LayeredGpu => "layered-gpu",
        UiSurfaceCompositionMode::Viewport => "viewport",
        UiSurfaceCompositionMode::ShaderCanvas => "shader-canvas",
    }
}
fn shader_bundle_ref(shader: &TypedShader) -> RealtimeShaderBundleRef {
    let stage = shader_ref_stage_name(shader.ast.stage).to_string();
    let key = format!("shader::{}::{}", shader.ast.name, stage);
    let resource_bindings = collect_shader_resource_bindings(shader, &stage);
    let explicit_compute_metadata = shader.ast.explicit_compute_metadata().ok().flatten();
    let tensor_bindings =
        collect_tensor_bindings(&resource_bindings, explicit_compute_metadata.as_ref());
    let stream_bindings =
        collect_stream_bindings(&resource_bindings, explicit_compute_metadata.as_ref());
    let neural_nodes = collect_neural_nodes(
        shader,
        &resource_bindings,
        explicit_compute_metadata.as_ref(),
    );
    let (execution_domain, workgroup_size, dispatch_size) =
        if matches!(shader.ast.stage, ShaderStage::Compute) {
            (
                Some(compute_execution_domain(&resource_bindings)),
                Some(
                    explicit_compute_metadata
                        .as_ref()
                        .and_then(|metadata| metadata.workgroup_size)
                        .unwrap_or_else(|| compute_workgroup_size(shader)),
                ),
                Some(
                    explicit_compute_metadata
                        .as_ref()
                        .map(|metadata| metadata.dispatch_size)
                        .unwrap_or_else(default_compute_dispatch_size),
                ),
            )
        } else {
            (None, None, None)
        };

    RealtimeShaderBundleRef {
        key,
        shader: shader.ast.name.clone(),
        module_name: shader.ast.name.clone(),
        stage,
        entry_point: "main".to_string(),
        source: "kain-core".to_string(),
        execution_domain,
        workgroup_size,
        dispatch_size,
        resource_bindings,
        tensor_bindings,
        stream_bindings,
        neural_nodes,
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
        let access = shader_resource_access(&uniform.ty, stage, &uniform.name).to_string();
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

fn shader_resource_access(ty: &Type, stage: &str, name: &str) -> &'static str {
    match ty {
        Type::Named {
            name: type_name, ..
        } if type_name == "Sampler2D" => "sample",
        Type::Named {
            name: type_name, ..
        } if type_name == "StorageBuffer" && stage == "compute" => {
            infer_storage_buffer_access(name)
        }
        Type::Named {
            name: type_name, ..
        } if type_name == "StorageBuffer" => "read",
        _ => "read",
    }
}

fn infer_storage_buffer_access(name: &str) -> &'static str {
    let lower = name.to_ascii_lowercase();
    if lower.starts_with("out")
        || lower.ends_with("_out")
        || lower.contains("dst")
        || lower.contains("dest")
        || lower.contains("output")
    {
        "write"
    } else if lower.starts_with("in")
        || lower.contains("src")
        || lower.contains("input")
        || lower.contains("weight")
        || lower.contains("bias")
        || lower.contains("activation")
    {
        "read"
    } else {
        "read_write"
    }
}

fn collect_tensor_bindings(
    bindings: &[RealtimeResourceBinding],
    explicit_metadata: Option<&ComputeMetadata>,
) -> Vec<RealtimeTensorBinding> {
    if let Some(metadata) = explicit_metadata {
        if !metadata.tensor_plans.is_empty() {
            return metadata
                .tensor_plans
                .iter()
                .map(|plan| RealtimeTensorBinding {
                    key: plan.key.clone(),
                    element_type: plan.element_type.clone(),
                    shape: plan.shape.clone(),
                    role: plan.role.clone(),
                    contract: plan.contract.clone(),
                })
                .collect();
        }
    }

    bindings
        .iter()
        .filter(|binding| binding.resource_type == "storage_buffer")
        .map(|binding| RealtimeTensorBinding {
            key: binding.key.clone(),
            element_type: infer_tensor_element_type(binding).to_string(),
            shape: vec!["dispatch.x".to_string()],
            role: tensor_role_from_access(&binding.access).to_string(),
            contract: "kain.shared.buffer".to_string(),
        })
        .collect()
}

fn collect_stream_bindings(
    bindings: &[RealtimeResourceBinding],
    explicit_metadata: Option<&ComputeMetadata>,
) -> Vec<RealtimeStreamBinding> {
    if let Some(metadata) = explicit_metadata {
        if let Some(stream_plans) = &metadata.stream_plans {
            return stream_plans
                .iter()
                .map(|plan| RealtimeStreamBinding {
                    key: plan.key.clone(),
                    direction: plan.direction.clone(),
                    cadence: plan.cadence.clone(),
                    contract: plan.contract.clone(),
                })
                .collect();
        }
    }

    bindings
        .iter()
        .filter(|binding| binding.resource_type == "storage_buffer")
        .map(|binding| RealtimeStreamBinding {
            key: binding.key.clone(),
            direction: stream_direction_from_access(&binding.access).to_string(),
            cadence: "continuous".to_string(),
            contract: "kain.shared.buffer".to_string(),
        })
        .collect()
}

fn collect_neural_nodes(
    shader: &TypedShader,
    bindings: &[RealtimeResourceBinding],
    explicit_metadata: Option<&ComputeMetadata>,
) -> Vec<RealtimeNeuralNode> {
    if let Some(metadata) = explicit_metadata {
        if !metadata.neural_node_plans.is_empty() {
            return metadata
                .neural_node_plans
                .iter()
                .map(|plan| RealtimeNeuralNode {
                    key: plan.key.clone(),
                    op: plan.op.clone(),
                    inputs: plan.inputs.clone(),
                    outputs: plan.outputs.clone(),
                    stateful: plan.stateful,
                })
                .collect();
        }
    }

    let storage_bindings = bindings
        .iter()
        .filter(|binding| binding.resource_type == "storage_buffer")
        .collect::<Vec<_>>();
    if !matches!(shader.ast.stage, ShaderStage::Compute) || storage_bindings.is_empty() {
        return Vec::new();
    }

    let inputs = storage_bindings
        .iter()
        .filter(|binding| binding.access == "read" || binding.access == "read_write")
        .map(|binding| binding.key.clone())
        .collect::<Vec<_>>();
    let outputs = storage_bindings
        .iter()
        .filter(|binding| binding.access == "write" || binding.access == "read_write")
        .map(|binding| binding.key.clone())
        .collect::<Vec<_>>();

    vec![RealtimeNeuralNode {
        key: shader.ast.name.clone(),
        op: infer_neural_op_name(&shader.ast.name).to_string(),
        inputs,
        outputs,
        stateful: storage_bindings
            .iter()
            .any(|binding| binding.access == "read_write"),
    }]
}

fn compute_execution_domain(bindings: &[RealtimeResourceBinding]) -> String {
    if bindings
        .iter()
        .any(|binding| binding.resource_type == "storage_buffer")
    {
        "tensor-stream".to_string()
    } else {
        "compute".to_string()
    }
}

fn tensor_role_from_access(access: &str) -> &'static str {
    match access {
        "read" => "input",
        "write" => "output",
        _ => "state",
    }
}

fn stream_direction_from_access(access: &str) -> &'static str {
    match access {
        "read" => "ingress",
        "write" => "egress",
        _ => "bidirectional",
    }
}

fn infer_neural_op_name(shader_name: &str) -> &'static str {
    let lower = shader_name.to_ascii_lowercase();
    if lower.contains("conv") {
        "conv"
    } else if lower.contains("attention") {
        "attention"
    } else if lower.contains("blend") {
        "blend"
    } else {
        "gpu.compute"
    }
}

fn infer_tensor_element_type(binding: &RealtimeResourceBinding) -> &'static str {
    let lower = binding.key.to_ascii_lowercase();
    if lower.contains("vec4") || lower.contains("rgba") {
        "vec4<f32>"
    } else if lower.contains("u32") || lower.contains("index") {
        "u32"
    } else {
        "f32"
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

fn program_has_explicit_compute_metadata(program: &TypedProgram) -> bool {
    program_items_have_explicit_compute_metadata(&program.items)
}

fn program_items_have_explicit_compute_metadata(items: &[TypedItem]) -> bool {
    items.iter().any(|item| match item {
        TypedItem::Shader(shader) => {
            matches!(shader.ast.stage, ShaderStage::Compute)
                && shader
                    .ast
                    .explicit_compute_metadata()
                    .ok()
                    .flatten()
                    .is_some()
        }
        TypedItem::Mod(module) => program_items_have_explicit_compute_metadata(&module.items),
        _ => false,
    })
}

fn collect_tool_caps(
    program: &TypedProgram,
    ui_output: Option<&UiBuildOutput>,
    shader_canvases: &[RealtimeShaderCanvasBinding],
    has_explicit_compute_metadata: bool,
) -> Vec<String> {
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
    if !shader_canvases.is_empty() {
        caps.push("ui.shader-canvas".to_string());
    }

    for item in &program.items {
        match item {
            TypedItem::Actor(_) => caps.push("scene.actors".to_string()),
            TypedItem::Shader(shader) => {
                caps.push("gpu.shader".to_string());
                if matches!(shader.ast.stage, ShaderStage::Compute) {
                    caps.push("gpu.compute".to_string());
                    if has_explicit_compute_metadata {
                        caps.push(COMPUTE_PLAN_CAPABILITY_KEY.to_string());
                    }
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
    shader_canvases: &[RealtimeShaderCanvasBinding],
    shader_bundle_refs: &[RealtimeShaderBundleRef],
    tool_caps: &[String],
    has_explicit_compute_metadata: bool,
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
    if !shader_canvases.is_empty() {
        requirements.push("ui.shader-canvas".to_string());
        requirements.push("ui.shader-canvas.presented-or-fallback".to_string());
    }
    if has_compute_refs {
        requirements.push("gpu.compute-dispatch".to_string());
        requirements.push("interop.shared-buffer".to_string());
        requirements.push("data.continuous-stream".to_string());
    }
    if has_explicit_compute_metadata {
        requirements.push(COMPUTE_PLAN_CAPABILITY_KEY.to_string());
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
    fn emits_shader_canvas_bindings_for_native_runtime_consumers() {
        let source = r#"
shader fragment hero_surface(uv: Vec2) -> Vec4:
    return vec4(uv.x, uv.y, 1.0, 1.0)

component App():
    render <panel>
        <canvas title="Hero Surface" shader_ref="hero_surface" shader_stage="fragment" shader_format="spirv" />
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
        assert_eq!(bundle.shader_canvases.len(), 1);
        assert_eq!(bundle.shader_canvases[0].surface_id, "surface.node.2");
        assert_eq!(bundle.shader_canvases[0].shader_ref, "hero_surface");
        assert_eq!(
            bundle.shader_canvases[0].shader_bundle_ref_key.as_deref(),
            Some("shader::hero_surface::fragment")
        );
        assert_eq!(bundle.shader_canvases[0].composition_mode, "shader-canvas");
        assert!(bundle.tool_caps.iter().any(|entry| entry == "ui.shader-canvas"));
        assert!(bundle
            .requirements
            .iter()
            .any(|entry| entry == "ui.shader-canvas.presented-or-fallback"));
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
        assert_eq!(
            bundle.shader_bundle_refs[0].execution_domain.as_deref(),
            Some("tensor-stream")
        );
        assert_eq!(bundle.shader_bundle_refs[0].workgroup_size, Some([8, 8, 1]));
        assert_eq!(bundle.shader_bundle_refs[0].dispatch_size, Some([1, 1, 1]));
        assert_eq!(bundle.shader_bundle_refs[0].resource_bindings.len(), 2);
        assert_eq!(bundle.shader_bundle_refs[0].tensor_bindings.len(), 2);
        assert_eq!(bundle.shader_bundle_refs[0].stream_bindings.len(), 2);
        assert_eq!(bundle.shader_bundle_refs[0].neural_nodes.len(), 1);
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

    #[test]
    fn emits_explicit_compute_plan_metadata_when_authored() {
        let source = r#"
shader compute TensorBlend() -> Void:
    comptime:
        let compute = (
            [8, 4, 1],
            [16, 8, 1],
            [
                ("src", "f32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "f32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [
                ("src", "ingress", "per-dispatch", "kain.shared.buffer"),
                ("dst", "egress", "per-dispatch", "kain.shared.buffer"),
            ],
            [
                ("TensorBlend", "blend", ["src"], ["dst"], false),
            ],
        )

    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

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
        assert_eq!(bundle.shader_bundle_refs[0].workgroup_size, Some([8, 4, 1]));
        assert_eq!(bundle.shader_bundle_refs[0].dispatch_size, Some([16, 8, 1]));
        assert_eq!(
            bundle.shader_bundle_refs[0].tensor_bindings[0].shape,
            vec!["dispatch.x".to_string()]
        );
        assert_eq!(bundle.shader_bundle_refs[0].stream_bindings.len(), 2);
        assert_eq!(
            bundle.shader_bundle_refs[0].stream_bindings[0].direction,
            "ingress"
        );
        assert_eq!(
            bundle.shader_bundle_refs[0].stream_bindings[0].cadence,
            "per-dispatch"
        );
        assert_eq!(bundle.shader_bundle_refs[0].neural_nodes[0].op, "blend");
        assert!(bundle
            .requirements
            .iter()
            .any(|entry| entry == "gpu.compute-plan"));
        assert!(bundle
            .tool_caps
            .iter()
            .any(|entry| entry == "gpu.compute-plan"));
    }
}








