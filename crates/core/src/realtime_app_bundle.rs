use serde::{Deserialize, Serialize};

use crate::ast::{
    Block, ComputeMetadata, ConvergeSelector, ElseBranch, Expr, ShaderStage, Stmt, Type,
    WorldSurfaceKind, COMPUTE_PLAN_CAPABILITY_KEY,
};
use crate::types::{
    PatchUndoMode, TypedConverge, TypedEntangle, TypedLaw, TypedOrchestrate, TypedPatch, TypedWorld,
};
use crate::ui::render_authored_expr_contract;
use crate::{CompileTarget, TypedItem, TypedProgram, TypedShader};
use kain_ui::{
    UiBuildOutput, UiDockPlacement, UiHotReloadPlan, UiLayoutKind, UiMotionPolicy, UiNode,
    UiNodeId, UiSurfaceCompositionMode, UiSurfaceKind, UiSurfaceRendererPreference, UiValue,
    UiWidgetKind, UiWorkspaceLayout,
};
use std::collections::{BTreeMap, HashMap, HashSet};

pub const REALTIME_APP_BUNDLE_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeAppBundle {
    pub schema_version: u32,
    pub target: String,
    pub render: RenderSceneBundle,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub patches: Vec<RealtimePatchBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub laws: Vec<RealtimeLawBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub converges: Vec<RealtimeConvergeBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub worlds: Vec<RealtimeWorldBinding>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub active_world: Option<RealtimeWorldBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub entanglements: Vec<RealtimeEntangleBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub orchestrations: Vec<RealtimeOrchestrationBinding>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub shader_canvases: Vec<RealtimeShaderCanvasBinding>,
    pub shader_bundle_refs: Vec<RealtimeShaderBundleRef>,
    pub assets: Vec<RealtimeAssetBinding>,
    pub tool_caps: Vec<String>,
    pub requirements: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ui_contracts: Option<RealtimeUiContractsBundle>,
}

/// Optional compiler-emitted UI contract bundle.
///
/// This is intentionally "verification first": it exists so tools and strong models can validate
/// spatial ownership (workspace graphs, panel/tab containment, anchors) from structure alone.
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq)]
#[serde(default)]
pub struct RealtimeUiContractsBundle {
    pub contract_version: Option<String>,
    pub widget_registry_json: Option<String>,
    pub command_registry_json: Option<String>,
    pub computed_registry_json: Option<String>,
    pub event_routes_json: Option<String>,
    pub hot_reload_json: Option<String>,
    pub focus_graph_json: Option<String>,
    pub selection_model_json: Option<String>,
    pub overlay_stack_json: Option<String>,
    pub motion_policy_json: Option<String>,
    pub paint_registry_json: Option<String>,
    pub motion_registry_json: Option<String>,
    pub workspace_schema_json: Option<String>,
    pub workspace_layout_json: Option<String>,
    pub workspace_layout: Option<UiWorkspaceLayout>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub structure_index: Vec<RealtimeUiStructureNode>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeUiStructureNode {
    pub id: u64,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub identity_key: Option<String>,
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub chrome_role: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub role: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub anchor_zone: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub anchor_target: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub dock_placement: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub persistent_layout_id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tab_group_id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tab_label: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub tab_order: Option<i32>,
    #[serde(default)]
    pub tab_default_active: bool,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub focus_scope: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selection_scope: Option<String>,
    #[serde(default)]
    pub min_width: Option<f32>,
    #[serde(default)]
    pub min_height: Option<f32>,
    #[serde(default)]
    pub max_width: Option<f32>,
    #[serde(default)]
    pub max_height: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub region_hint: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RenderSceneBundle {
    pub scenes: Vec<RealtimeSceneBinding>,
    pub materials: Vec<CompiledMaterialDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeSceneBinding {
    pub viewport_node: String,
    pub viewport_kind: String,
    pub scene: String,
    pub title: Option<String>,
    pub material_refs: Vec<String>,
    pub shader_bundle_ref_keys: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub camera: Option<RealtimeViewportCameraBinding>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub presentation: Option<RealtimeViewportPresentationBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeViewportCameraBinding {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub position: Option<[f32; 3]>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub target: Option<[f32; 3]>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub fov_y_degrees: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub near_plane: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub far_plane: Option<f32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeViewportPresentationBinding {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub profile: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub fog_density: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub particle_budget: Option<u32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub gizmo: Option<RealtimeViewportGizmoBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeViewportGizmoBinding {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub profile_id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub visible: Option<bool>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub default_mode: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub default_space: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub drag_trigger: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selection_required: Option<bool>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub translate_hotkey: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub rotate_hotkey: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub scale_hotkey: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub cycle_space_hotkey: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub toggle_snap_hotkey: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub translate_snap_units: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub rotate_snap_degrees: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub scale_snap_percent: Option<f32>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub snap_default_enabled: Option<bool>,
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
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub font_atlases: Vec<RealtimeShaderCanvasFontAtlas>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub text_runs: Vec<RealtimeShaderCanvasTextRun>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub resource_bindings: Vec<RealtimeShaderCanvasResourceBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeShaderCanvasFontAtlas {
    pub key: String,
    pub family: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub asset_key: Option<String>,
    #[serde(default = "default_shader_canvas_font_atlas_encoding")]
    pub encoding: String,
    #[serde(default = "default_shader_canvas_font_atlas_distance_range_px")]
    pub distance_range_px: u32,
    pub glyphs: String,
    pub cell_size_px: [u32; 2],
    pub texture_size_px: [u32; 2],
    pub columns: u32,
    pub rows: u32,
}

fn default_shader_canvas_font_atlas_encoding() -> String {
    "msdf-rgba".to_string()
}

fn default_shader_canvas_font_atlas_distance_range_px() -> u32 {
    6
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeShaderCanvasTextRun {
    pub id: String,
    pub text: String,
    pub role: String,
    pub atlas_key: String,
    pub origin_px: [u32; 2],
    pub color_token: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RealtimeShaderCanvasResourceBinding {
    pub binding_name: String,
    pub resource_kind: String,
    pub source_kind: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub atlas_key: Option<String>,
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

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimePatchBinding {
    pub name: String,
    pub mutation_paths: Vec<String>,
    pub undo_mode: String,
    pub collaboration_event: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeLawBinding {
    pub name: String,
    pub symbol: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub param_types: Vec<String>,
    pub return_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeConvergeLaneBinding {
    pub lane_name: String,
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selector: Option<String>,
    pub symbol: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeConvergeBinding {
    pub name: String,
    pub dispatcher_symbol: String,
    pub spec_lane: RealtimeConvergeLaneBinding,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub fast_lanes: Vec<RealtimeConvergeLaneBinding>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub verify_random_count: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeWorldStateBinding {
    pub name: String,
    pub type_name: String,
    pub initial_expr: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeWorldSurfaceBinding {
    pub kind: String,
    pub authored_expr: String,
    pub surface_key: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeWorldBinding {
    pub name: String,
    pub state_slots: Vec<RealtimeWorldStateBinding>,
    pub surfaces: Vec<RealtimeWorldSurfaceBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeEntangleBinding {
    pub authority: String,
    pub mirror: String,
    pub policy: String,
    pub type_name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeOrchestrationStageBinding {
    pub runtime: String,
    pub kind: String,
    pub function: String,
    pub binding_name: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selector: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependencies: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub residency: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub transfer: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub guard: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub fallback: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub requires: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub policy: Option<String>,
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub adaptive_policy: bool,
    pub silicon_native: bool,
    pub compatibility_adapter: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct RealtimeOrchestrationBinding {
    pub name: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub return_type: Option<String>,
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub graph_mode: bool,
    #[serde(default, skip_serializing_if = "std::ops::Not::not")]
    pub adaptive_policy: bool,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub stages: Vec<RealtimeOrchestrationStageBinding>,
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
    let patches = collect_patch_bindings(program);
    let laws = collect_law_bindings(program);
    let converges = collect_converge_bindings(program);
    let worlds = collect_world_bindings(program);
    let active_world = if worlds.len() == 1 {
        worlds.first().cloned()
    } else {
        None
    };
    let entanglements = collect_entangle_bindings(program);
    let orchestrations = collect_orchestration_bindings(program);
    let assets = collect_assets(ui_output);
    let has_explicit_compute_metadata = program_has_explicit_compute_metadata(program);
    let tool_caps = collect_tool_caps(
        program,
        ui_output,
        &shader_canvases,
        &worlds,
        !entanglements.is_empty(),
        !patches.is_empty(),
        !laws.is_empty(),
        !converges.is_empty(),
        !orchestrations.is_empty(),
        has_explicit_compute_metadata,
    );
    let requirements = collect_requirements(
        target,
        &scenes,
        &shader_canvases,
        &shader_bundle_refs,
        &worlds,
        &tool_caps,
        !entanglements.is_empty(),
        !patches.is_empty(),
        !laws.is_empty(),
        !converges.is_empty(),
        !orchestrations.is_empty(),
        has_explicit_compute_metadata,
    );
    let ui_contracts = collect_ui_contracts(ui_output);

    RealtimeAppBundle {
        schema_version: REALTIME_APP_BUNDLE_SCHEMA_VERSION,
        target: compile_target_name(target).to_string(),
        render: RenderSceneBundle { scenes, materials },
        patches,
        laws,
        converges,
        worlds,
        active_world,
        entanglements,
        orchestrations,
        shader_canvases,
        shader_bundle_refs,
        assets,
        tool_caps,
        requirements,
        ui_contracts,
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

fn collect_ui_contracts(ui_output: Option<&UiBuildOutput>) -> Option<RealtimeUiContractsBundle> {
    let Some(output) = ui_output else {
        return None;
    };

    let mut bundle = RealtimeUiContractsBundle::default();
    bundle.contract_version = ui_session_state_string(output, "ui.contract.version");
    bundle.widget_registry_json =
        ui_session_state_string(output, "ui.contract.widget_registry.json");
    bundle.command_registry_json =
        ui_session_state_string(output, "ui.contract.command_registry.json");
    bundle.computed_registry_json =
        ui_session_state_string(output, "ui.contract.computed_registry.json");
    bundle.event_routes_json = ui_session_state_string(output, "ui.contract.event_routes.json");
    bundle.motion_policy_json = ui_session_state_string(output, "ui.contract.motion_policy.json");
    bundle.paint_registry_json = ui_session_state_string(output, "ui.contract.paint_registry.json");
    bundle.motion_registry_json =
        ui_session_state_string(output, "ui.contract.motion_registry.json");
    bundle.workspace_schema_json =
        ui_session_state_string(output, "ui.contract.workspace_schema.json");
    bundle.workspace_layout_json =
        ui_session_state_string(output, "ui.contract.workspace_layout.json");

    if !output.systems.workspace_layout.roots.is_empty()
        || output.systems.workspace_layout.persistence_key.is_some()
        || output.systems.workspace_layout.virtualization_enabled
        || !output.systems.workspace_layout.active_tabs.is_empty()
    {
        bundle.workspace_layout = Some(output.systems.workspace_layout.clone());
    }

    if output.systems.hot_reload != UiHotReloadPlan::default() {
        bundle.hot_reload_json = serialize_contract_value(&output.systems.hot_reload);
    }
    if !output.systems.focus_graph.scopes.is_empty()
        || output.systems.focus_graph.default_scope.is_some()
        || !output.systems.focus_graph.focused.is_empty()
        || !output.systems.focus_graph.traversal_edges.is_empty()
    {
        bundle.focus_graph_json = serialize_contract_value(&output.systems.focus_graph);
    }
    if !output.systems.selection_model.scopes.is_empty()
        || output.systems.selection_model.active_scope.is_some()
        || !output.systems.selection_model.primary.is_empty()
        || !output.systems.selection_model.selected.is_empty()
    {
        bundle.selection_model_json = serialize_contract_value(&output.systems.selection_model);
    }
    if !output.systems.overlay_stack.entries.is_empty() {
        bundle.overlay_stack_json = serialize_contract_value(&output.systems.overlay_stack);
    }
    if bundle.motion_policy_json.is_none()
        && output.systems.motion_policy != UiMotionPolicy::default()
    {
        bundle.motion_policy_json = serialize_contract_value(&output.systems.motion_policy);
    }

    bundle.structure_index = build_ui_structure_index(output);

    let has_any = bundle.contract_version.is_some()
        || bundle.widget_registry_json.is_some()
        || bundle.command_registry_json.is_some()
        || bundle.computed_registry_json.is_some()
        || bundle.event_routes_json.is_some()
        || bundle.hot_reload_json.is_some()
        || bundle.focus_graph_json.is_some()
        || bundle.selection_model_json.is_some()
        || bundle.overlay_stack_json.is_some()
        || bundle.motion_policy_json.is_some()
        || bundle.paint_registry_json.is_some()
        || bundle.motion_registry_json.is_some()
        || bundle.workspace_schema_json.is_some()
        || bundle.workspace_layout.is_some()
        || !bundle.structure_index.is_empty();
    if has_any {
        Some(bundle)
    } else {
        None
    }
}

pub(crate) fn ui_session_state_string(output: &UiBuildOutput, key: &str) -> Option<String> {
    output
        .systems
        .session_state
        .get(key)
        .and_then(|value| match value {
            UiValue::String(value) => Some(value.clone()),
            UiValue::Int(value) => Some(value.to_string()),
            UiValue::Float(value) => Some(value.to_string()),
            UiValue::Bool(value) => Some(value.to_string()),
            UiValue::Null => None,
        })
}

fn serialize_contract_value<T: Serialize>(value: &T) -> Option<String> {
    serde_json::to_string_pretty(value).ok()
}

fn build_ui_structure_index(output: &UiBuildOutput) -> Vec<RealtimeUiStructureNode> {
    let mut parents = HashMap::<UiNodeId, UiNodeId>::new();
    for node in output.tree.nodes.values() {
        for child in &node.children {
            parents.insert(*child, node.id);
        }
    }

    let mut nodes = Vec::new();
    for node in output.tree.nodes.values() {
        let role = node
            .props
            .get("role")
            .and_then(|value| value.as_str())
            .map(ToString::to_string);
        let chrome_role = node
            .props
            .get("ui.chrome_role")
            .and_then(|value| value.as_str())
            .map(ToString::to_string);
        let anchor_zone = node
            .props
            .get("ui.anchor_zone")
            .and_then(|value| value.as_str())
            .map(ToString::to_string);
        let anchor_target = node
            .props
            .get("ui.anchor_target")
            .and_then(|value| value.as_str())
            .map(ToString::to_string);

        let include = node.identity_key.is_some()
            || role.is_some()
            || chrome_role.is_some()
            || anchor_zone.is_some()
            || anchor_target.is_some()
            || node.layout.dock.is_some()
            || node.layout.persistent_layout_id.is_some()
            || node.layout.tab_group_id.is_some()
            || node.layout.kind == UiLayoutKind::Dock;
        if !include {
            continue;
        }

        nodes.push(RealtimeUiStructureNode {
            id: node.id.0,
            identity_key: node.identity_key.clone(),
            kind: ui_widget_kind_name(&node.kind),
            chrome_role,
            role,
            anchor_zone,
            anchor_target,
            dock_placement: node
                .layout
                .dock
                .map(dock_placement_name)
                .map(ToString::to_string),
            persistent_layout_id: node.layout.persistent_layout_id.clone(),
            tab_group_id: node.layout.tab_group_id.clone(),
            tab_label: node.layout.tab_label.clone(),
            tab_order: node.layout.tab_order,
            tab_default_active: node.layout.tab_default_active,
            focus_scope: node.focus_scope.clone(),
            selection_scope: node.selection_scope.clone(),
            min_width: node.layout.min_width,
            min_height: node.layout.min_height,
            max_width: node.layout.max_width,
            max_height: node.layout.max_height,
            region_hint: compute_region_hint(node.id, &parents, &output.tree.nodes),
        });
    }

    nodes.sort_by_key(|node| node.id);
    nodes
}

fn compute_region_hint(
    id: UiNodeId,
    parents: &HashMap<UiNodeId, UiNodeId>,
    nodes: &BTreeMap<UiNodeId, UiNode>,
) -> Option<String> {
    let mut cursor = Some(id);
    let mut dock_hint: Option<String> = None;
    let mut chrome_hint: Option<String> = None;
    let mut anchor_hint: Option<String> = None;
    while let Some(current) = cursor {
        if let Some(node) = nodes.get(&current) {
            if dock_hint.is_none() {
                if let Some(placement) = node.layout.dock {
                    dock_hint = Some(format!("dock.{}", dock_placement_name(placement)));
                }
            }
            if chrome_hint.is_none() {
                chrome_hint = node
                    .props
                    .get("ui.chrome_role")
                    .and_then(|value| value.as_str())
                    .map(|value| format!("chrome.{value}"))
                    .or_else(|| {
                        node.props
                            .get("role")
                            .and_then(|value| value.as_str())
                            .map(|value| format!("role.{value}"))
                    });
            }
            if anchor_hint.is_none() {
                anchor_hint = node
                    .props
                    .get("ui.anchor_zone")
                    .and_then(|value| value.as_str())
                    .map(|value| format!("anchor.{value}"));
            }
            if dock_hint.is_some() {
                break;
            }
        }
        cursor = parents.get(&current).copied();
    }

    dock_hint.or(chrome_hint).or(anchor_hint)
}

fn dock_placement_name(value: UiDockPlacement) -> &'static str {
    match value {
        UiDockPlacement::Center => "center",
        UiDockPlacement::Left => "left",
        UiDockPlacement::Right => "right",
        UiDockPlacement::Top => "top",
        UiDockPlacement::Bottom => "bottom",
        UiDockPlacement::Tab => "tab",
    }
}

fn ui_widget_kind_name(kind: &UiWidgetKind) -> String {
    match kind {
        UiWidgetKind::Element(tag) => format!("element:{tag}"),
        UiWidgetKind::ComponentRef(name) => format!("component:{name}"),
        UiWidgetKind::Text => "text".to_string(),
        UiWidgetKind::Panel => "panel".to_string(),
        UiWidgetKind::Inspector => "inspector".to_string(),
        UiWidgetKind::Graph => "graph".to_string(),
        UiWidgetKind::Timeline => "timeline".to_string(),
        UiWidgetKind::Table => "table".to_string(),
        UiWidgetKind::Tree => "tree".to_string(),
        UiWidgetKind::Viewport2D => "viewport2d".to_string(),
        UiWidgetKind::Viewport3D => "viewport3d".to_string(),
        UiWidgetKind::Overlay => "overlay".to_string(),
        UiWidgetKind::Slot => "slot".to_string(),
    }
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

fn collect_patch_bindings(program: &TypedProgram) -> Vec<RealtimePatchBinding> {
    let mut bindings = Vec::new();
    collect_patch_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| left.name.cmp(&right.name));
    bindings
}

fn collect_patch_bindings_into(items: &[TypedItem], output: &mut Vec<RealtimePatchBinding>) {
    for item in items {
        match item {
            TypedItem::Patch(patch) => output.push(realtime_patch_binding(patch)),
            TypedItem::Mod(module) => collect_patch_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_patch_binding(patch: &TypedPatch) -> RealtimePatchBinding {
    RealtimePatchBinding {
        name: patch.ast.name.clone(),
        mutation_paths: patch.mutation_paths.clone(),
        undo_mode: match patch.undo_mode {
            PatchUndoMode::Reversible => "reversible".to_string(),
            PatchUndoMode::BestEffort => "best_effort".to_string(),
        },
        collaboration_event: format!("patch.{}", patch.ast.name),
    }
}

fn collect_law_bindings(program: &TypedProgram) -> Vec<RealtimeLawBinding> {
    let mut bindings = Vec::new();
    collect_law_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| left.name.cmp(&right.name));
    bindings
}

fn collect_law_bindings_into(items: &[TypedItem], output: &mut Vec<RealtimeLawBinding>) {
    for item in items {
        match item {
            TypedItem::Law(law) => output.push(realtime_law_binding(law)),
            TypedItem::Mod(module) => collect_law_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_law_binding(law: &TypedLaw) -> RealtimeLawBinding {
    RealtimeLawBinding {
        name: law.ast.name.clone(),
        symbol: law.ast.name.clone(),
        param_types: law
            .ast
            .params
            .iter()
            .map(|param| render_contract_type_name(&param.ty))
            .collect(),
        return_type: render_contract_type_name(&law.ast.return_type),
    }
}

fn collect_converge_bindings(program: &TypedProgram) -> Vec<RealtimeConvergeBinding> {
    let mut bindings = Vec::new();
    collect_converge_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| left.name.cmp(&right.name));
    bindings
}

fn collect_converge_bindings_into(items: &[TypedItem], output: &mut Vec<RealtimeConvergeBinding>) {
    for item in items {
        match item {
            TypedItem::Converge(converge) => output.push(realtime_converge_binding(converge)),
            TypedItem::Mod(module) => collect_converge_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_converge_binding(converge: &TypedConverge) -> RealtimeConvergeBinding {
    RealtimeConvergeBinding {
        name: converge.ast.name.clone(),
        dispatcher_symbol: converge.ast.name.clone(),
        spec_lane: realtime_converge_lane_binding(&converge.ast.name, &converge.ast.spec_lane),
        fast_lanes: converge
            .ast
            .fast_lanes
            .iter()
            .map(|lane| realtime_converge_lane_binding(&converge.ast.name, lane))
            .collect(),
        verify_random_count: converge.ast.verify_random_count,
    }
}

fn realtime_converge_lane_binding(
    converge_name: &str,
    lane: &crate::ast::ConvergeLane,
) -> RealtimeConvergeLaneBinding {
    let selector = match &lane.selector {
        Some(ConvergeSelector::Target(value)) => Some(format!("target:{value}")),
        Some(ConvergeSelector::Capability(value)) => Some(format!("capability:{value}")),
        None => None,
    };
    RealtimeConvergeLaneBinding {
        lane_name: lane.lane_name.clone(),
        kind: match lane.kind {
            crate::ast::ConvergeLaneKind::Spec => "spec".to_string(),
            crate::ast::ConvergeLaneKind::Fast => "fast".to_string(),
        },
        selector,
        symbol: format!(
            "{}__{}",
            converge_name,
            sanitize_bundle_symbol_ident(&lane.lane_name)
        ),
    }
}

fn collect_world_bindings(program: &TypedProgram) -> Vec<RealtimeWorldBinding> {
    let mut bindings = Vec::new();
    collect_world_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| left.name.cmp(&right.name));
    bindings
}

fn collect_world_bindings_into(items: &[TypedItem], output: &mut Vec<RealtimeWorldBinding>) {
    for item in items {
        match item {
            TypedItem::World(world) => output.push(realtime_world_binding(world)),
            TypedItem::Mod(module) => collect_world_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_world_binding(world: &TypedWorld) -> RealtimeWorldBinding {
    let mut surfaces = world
        .ast
        .surfaces
        .iter()
        .map(|surface| RealtimeWorldSurfaceBinding {
            kind: surface.kind.as_str().to_string(),
            authored_expr: render_authored_expr_contract(&surface.expr),
            surface_key: format!("world.{}.{}", world.ast.name, surface.kind.as_str()),
        })
        .collect::<Vec<_>>();
    surfaces.sort_by(|left, right| left.kind.cmp(&right.kind));
    RealtimeWorldBinding {
        name: world.ast.name.clone(),
        state_slots: world
            .ast
            .states
            .iter()
            .map(|state| RealtimeWorldStateBinding {
                name: state.name.clone(),
                type_name: render_contract_type_name(&state.ty),
                initial_expr: render_authored_expr_contract(&state.initial),
            })
            .collect(),
        surfaces,
    }
}

fn collect_entangle_bindings(program: &TypedProgram) -> Vec<RealtimeEntangleBinding> {
    let mut bindings = Vec::new();
    collect_entangle_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| {
        left.authority
            .cmp(&right.authority)
            .then_with(|| left.mirror.cmp(&right.mirror))
    });
    bindings
}

fn collect_entangle_bindings_into(items: &[TypedItem], output: &mut Vec<RealtimeEntangleBinding>) {
    for item in items {
        match item {
            TypedItem::Entangle(entangle) => output.push(realtime_entangle_binding(entangle)),
            TypedItem::Mod(module) => collect_entangle_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_entangle_binding(entangle: &TypedEntangle) -> RealtimeEntangleBinding {
    RealtimeEntangleBinding {
        authority: entangle.ast.left.authored_path(),
        mirror: entangle.ast.right.authored_path(),
        policy: entangle.ast.policy.as_str().to_string(),
        type_name: entangle.endpoint_type_name.clone(),
    }
}

fn collect_orchestration_bindings(program: &TypedProgram) -> Vec<RealtimeOrchestrationBinding> {
    let mut bindings = Vec::new();
    collect_orchestration_bindings_into(&program.items, &mut bindings);
    bindings.sort_by(|left, right| left.name.cmp(&right.name));
    bindings
}

fn collect_orchestration_bindings_into(
    items: &[TypedItem],
    output: &mut Vec<RealtimeOrchestrationBinding>,
) {
    for item in items {
        match item {
            TypedItem::Orchestrate(orchestrate) => {
                output.push(realtime_orchestration_binding(orchestrate));
            }
            TypedItem::Mod(module) => collect_orchestration_bindings_into(&module.items, output),
            _ => {}
        }
    }
}

fn realtime_orchestration_binding(orchestrate: &TypedOrchestrate) -> RealtimeOrchestrationBinding {
    let graph_mode = orchestrate
        .stages
        .iter()
        .any(|stage| stage.metadata != Default::default());
    let adaptive_policy = orchestrate
        .stages
        .iter()
        .any(|stage| stage.metadata.adaptive());
    RealtimeOrchestrationBinding {
        name: orchestrate.ast.name.clone(),
        return_type: orchestrate
            .ast
            .return_type
            .as_ref()
            .map(render_contract_type_name),
        graph_mode,
        adaptive_policy,
        stages: orchestrate
            .stages
            .iter()
            .map(|stage| RealtimeOrchestrationStageBinding {
                runtime: stage.runtime.as_str().to_string(),
                kind: stage.runtime.as_str().to_string(),
                function: stage.function.clone(),
                binding_name: stage.binding_name.clone(),
                selector: stage.selector.as_ref().map(|selector| selector.authored()),
                dependencies: stage.metadata.dependencies.clone(),
                residency: stage.metadata.residency.map(|value| value.as_str().to_string()),
                transfer: stage.metadata.transfer.map(|value| value.as_str().to_string()),
                guard: stage.metadata.guard.clone(),
                fallback: stage.metadata.fallback.as_ref().map(|value| value.authored()),
                requires: stage.metadata.requires.clone(),
                policy: stage.metadata.policy.map(|value| value.as_str().to_string()),
                adaptive_policy: stage.metadata.adaptive(),
                silicon_native: stage.runtime.is_silicon_native(),
                compatibility_adapter: stage.runtime.is_compat_adapter(),
            })
            .collect(),
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
            let (font_atlases, text_runs, resource_bindings) = output
                .tree
                .nodes
                .get(&surface.node)
                .map(|node| collect_shader_canvas_surface_resources(node, surface.title.as_deref()))
                .unwrap_or_else(|| (Vec::new(), Vec::new(), Vec::new()));
            Some(RealtimeShaderCanvasBinding {
                surface_id: surface.id.clone(),
                shader_ref: shader.shader_ref.clone(),
                shader_bundle_ref_key: resolved_shader_ref.as_ref().map(|entry| entry.key.clone()),
                shader_name: resolved_shader_ref
                    .as_ref()
                    .map(|entry| entry.shader.clone()),
                module_name: resolved_shader_ref
                    .as_ref()
                    .map(|entry| entry.module_name.clone()),
                stage: shader
                    .stage
                    .clone()
                    .or_else(|| {
                        resolved_shader_ref
                            .as_ref()
                            .map(|entry| entry.stage.clone())
                    })
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
                font_atlases,
                text_runs,
                resource_bindings,
            })
        })
        .collect::<Vec<_>>();

    bindings.sort_by(|left, right| left.surface_id.cmp(&right.surface_id));
    bindings
}

fn collect_shader_canvas_surface_resources(
    node: &UiNode,
    surface_title: Option<&str>,
) -> (
    Vec<RealtimeShaderCanvasFontAtlas>,
    Vec<RealtimeShaderCanvasTextRun>,
    Vec<RealtimeShaderCanvasResourceBinding>,
) {
    let title_text = surface_title
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
        .or_else(|| node_prop_string(node, "title"));
    let body_text = first_string_prop(node, &["text", "label", "caption", "subtitle"]);
    let mut text_runs = Vec::new();
    if let Some(title) = title_text {
        text_runs.push(RealtimeShaderCanvasTextRun {
            id: format!("node.{}.title", node.id.0),
            text: title,
            role: "title".to_string(),
            atlas_key: String::new(),
            origin_px: [16, 16],
            color_token: "theme.text.default".to_string(),
        });
    }
    if let Some(body) = body_text
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
    {
        text_runs.push(RealtimeShaderCanvasTextRun {
            id: format!("node.{}.body", node.id.0),
            text: body,
            role: "body".to_string(),
            atlas_key: String::new(),
            origin_px: [16, 38],
            color_token: "theme.text.muted".to_string(),
        });
    }

    let mut font_atlases = Vec::new();
    if !text_runs.is_empty() {
        let atlas_key = format!("surface.node.{}.atlas.default", node.id.0);
        let glyphs = collect_shader_canvas_glyphs(&text_runs);
        let font_asset_source =
            first_string_prop(node, &["font_asset", "font_path", "font_source"])
                .and_then(|value| normalize_asset_source(&value));
        let cell_width_px =
            first_u32_prop(node, &["font_cell_width_px", "font_cell_px"]).unwrap_or(8);
        let cell_height_px =
            first_u32_prop(node, &["font_cell_height_px", "font_cell_px"]).unwrap_or(8);
        let columns = glyphs.chars().count().max(1).min(16) as u32;
        let rows = ((glyphs.chars().count().max(1) as u32) + columns - 1) / columns;
        let family = first_string_prop(node, &["font_family", "font"])
            .unwrap_or_else(|| "kain.default-ui-sans".to_string());
        let encoding = first_string_prop(node, &["font_encoding", "font_atlas_encoding"])
            .unwrap_or_else(|| shader_canvas_font_atlas_default_encoding_for_family(&family));
        let distance_range_px =
            first_u32_prop(node, &["font_distance_range_px", "font_msdf_range_px"])
                .unwrap_or_else(default_shader_canvas_font_atlas_distance_range_px);
        font_atlases.push(RealtimeShaderCanvasFontAtlas {
            key: atlas_key.clone(),
            family,
            asset_key: font_asset_source
                .as_deref()
                .map(|source| realtime_asset_key("font", source)),
            encoding,
            distance_range_px,
            glyphs,
            cell_size_px: [cell_width_px, cell_height_px],
            texture_size_px: [cell_width_px * columns, cell_height_px * rows.max(1)],
            columns,
            rows: rows.max(1),
        });
        for run in &mut text_runs {
            run.atlas_key = atlas_key.clone();
        }
    }

    let mut resource_bindings = vec![
        RealtimeShaderCanvasResourceBinding {
            binding_name: "surface_uniforms".to_string(),
            resource_kind: "uniform-buffer".to_string(),
            source_kind: "runtime.surface.uniforms".to_string(),
            atlas_key: None,
        },
        RealtimeShaderCanvasResourceBinding {
            binding_name: "surface_storage".to_string(),
            resource_kind: "storage-buffer".to_string(),
            source_kind: "runtime.surface.storage".to_string(),
            atlas_key: None,
        },
    ];
    if let Some(atlas) = font_atlases.first() {
        resource_bindings.push(RealtimeShaderCanvasResourceBinding {
            binding_name: "font_atlas".to_string(),
            resource_kind: "texture-2d".to_string(),
            source_kind: "runtime.surface.font-atlas".to_string(),
            atlas_key: Some(atlas.key.clone()),
        });
        resource_bindings.push(RealtimeShaderCanvasResourceBinding {
            binding_name: "font_sampler".to_string(),
            resource_kind: "sampler".to_string(),
            source_kind: "runtime.surface.font-atlas".to_string(),
            atlas_key: Some(atlas.key.clone()),
        });
    }

    (font_atlases, text_runs, resource_bindings)
}

fn shader_canvas_font_atlas_default_encoding_for_family(family: &str) -> String {
    if family
        .trim()
        .eq_ignore_ascii_case("kain.builtin.bitmap_5x7")
    {
        "bitmap-alpha".to_string()
    } else {
        default_shader_canvas_font_atlas_encoding()
    }
}

fn collect_shader_canvas_glyphs(text_runs: &[RealtimeShaderCanvasTextRun]) -> String {
    let mut glyphs = vec![' ', '?'];
    for run in text_runs {
        for ch in run.text.chars() {
            let normalized = normalize_shader_canvas_glyph(ch);
            if !glyphs.contains(&normalized) {
                glyphs.push(normalized);
            }
        }
    }
    glyphs.into_iter().collect()
}

fn normalize_shader_canvas_glyph(ch: char) -> char {
    if ch.is_ascii() {
        ch
    } else {
        '?'
    }
}

fn node_prop_string(node: &UiNode, key: &str) -> Option<String> {
    node.props
        .get(key)
        .and_then(UiValue::as_str)
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
}

fn first_string_prop(node: &UiNode, keys: &[&str]) -> Option<String> {
    keys.iter().find_map(|key| node_prop_string(node, key))
}

fn first_u32_prop(node: &UiNode, keys: &[&str]) -> Option<u32> {
    keys.iter().find_map(|key| {
        node.props.get(*key).and_then(|value| match value {
            UiValue::Int(number) if *number >= 0 => Some(*number as u32),
            UiValue::Float(number) if *number >= 0.0 => Some(*number as u32),
            UiValue::String(number) => number.trim().parse::<u32>().ok(),
            _ => None,
        })
    })
}

fn resolve_surface_shader_ref<'a>(
    shader_ref: &str,
    shader_bundle_refs: &'a [RealtimeShaderBundleRef],
) -> Option<&'a RealtimeShaderBundleRef> {
    let trimmed = shader_ref.trim();
    if trimmed.is_empty() {
        return None;
    }

    shader_bundle_refs
        .iter()
        .find(|entry| entry.key == trimmed)
        .or_else(|| {
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
    let tensor_bindings = collect_tensor_bindings(
        shader,
        &resource_bindings,
        explicit_compute_metadata.as_ref(),
    );
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
                    shader
                        .ast
                        .workgroup_size
                        .or_else(|| {
                            explicit_compute_metadata
                                .as_ref()
                                .and_then(|metadata| metadata.workgroup_size)
                        })
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
    let storage_access = shader_storage_access_map(shader);

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
        let access = shader_resource_access(
            &uniform.ty,
            stage,
            &uniform.name,
            storage_access.get(&uniform.name).copied(),
        )
        .to_string();
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

#[derive(Debug, Clone, Copy, Default)]
struct ShaderStorageAccess {
    reads: bool,
    writes: bool,
}

fn shader_resource_access(
    ty: &Type,
    stage: &str,
    name: &str,
    observed_access: Option<ShaderStorageAccess>,
) -> &'static str {
    match ty {
        Type::Named {
            name: type_name, ..
        } if type_name == "Sampler2D" => "sample",
        Type::Named {
            name: type_name, ..
        } if type_name == "StorageBuffer" && stage == "compute" => observed_access
            .map(storage_access_mode)
            .unwrap_or_else(|| infer_storage_buffer_access(name)),
        Type::Named {
            name: type_name, ..
        } if type_name == "StorageBuffer" => "read",
        _ => "read",
    }
}

fn storage_access_mode(access: ShaderStorageAccess) -> &'static str {
    match (access.reads, access.writes) {
        (true, true) => "read_write",
        (true, false) => "read",
        (false, true) => "write",
        (false, false) => "read_write",
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

fn shader_storage_access_map(shader: &TypedShader) -> HashMap<String, ShaderStorageAccess> {
    if !matches!(shader.ast.stage, ShaderStage::Compute) {
        return HashMap::new();
    }

    let storage_uniforms = shader
        .ast
        .uniforms
        .iter()
        .filter(
            |uniform| matches!(uniform.ty, Type::Named { ref name, .. } if name == "StorageBuffer"),
        )
        .map(|uniform| uniform.name.clone())
        .collect::<HashSet<_>>();
    if storage_uniforms.is_empty() {
        return HashMap::new();
    }

    let mut access = HashMap::new();
    scan_shader_block_for_storage_access(&shader.ast.body, &storage_uniforms, &mut access);
    access
}

fn scan_shader_block_for_storage_access(
    block: &Block,
    storage_uniforms: &HashSet<String>,
    access: &mut HashMap<String, ShaderStorageAccess>,
) {
    for stmt in &block.stmts {
        scan_shader_stmt_for_storage_access(stmt, storage_uniforms, access);
    }
}

fn scan_shader_stmt_for_storage_access(
    stmt: &Stmt,
    storage_uniforms: &HashSet<String>,
    access: &mut HashMap<String, ShaderStorageAccess>,
) {
    match stmt {
        Stmt::Let {
            value: Some(value), ..
        } => {
            scan_shader_expr_for_storage_access(value, storage_uniforms, access);
        }
        Stmt::Expr(expr) => {
            scan_shader_expr_for_storage_access(expr, storage_uniforms, access);
        }
        Stmt::Defer { expr, .. } => {
            scan_shader_expr_for_storage_access(expr, storage_uniforms, access);
        }
        Stmt::Dispatch { dispatch_size, .. } => {
            for expr in dispatch_size {
                scan_shader_expr_for_storage_access(expr, storage_uniforms, access);
            }
        }
        Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
            scan_shader_expr_for_storage_access(expr, storage_uniforms, access);
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            scan_shader_expr_for_storage_access(iter, storage_uniforms, access);
            scan_shader_block_for_storage_access(body, storage_uniforms, access);
        }
        Stmt::While {
            condition, body, ..
        } => {
            scan_shader_expr_for_storage_access(condition, storage_uniforms, access);
            scan_shader_block_for_storage_access(body, storage_uniforms, access);
        }
        Stmt::Loop { body, .. } => {
            scan_shader_block_for_storage_access(body, storage_uniforms, access);
        }
        Stmt::Item(_) | Stmt::Continue(_) | Stmt::Return(None, _) | Stmt::Break(None, _) => {}
        Stmt::Let { value: None, .. } => {}
    }
}

fn scan_shader_expr_for_storage_access(
    expr: &Expr,
    storage_uniforms: &HashSet<String>,
    access: &mut HashMap<String, ShaderStorageAccess>,
) {
    match expr {
        Expr::Assign { target, value, .. } => {
            mark_storage_write_target(target, storage_uniforms, access);
            scan_shader_expr_for_storage_access(value, storage_uniforms, access);
        }
        Expr::Index { object, index, .. } => {
            if let Some(name) = storage_uniform_ident(object, storage_uniforms) {
                access.entry(name.to_string()).or_default().reads = true;
            } else {
                scan_shader_expr_for_storage_access(object, storage_uniforms, access);
            }
            scan_shader_expr_for_storage_access(index, storage_uniforms, access);
        }
        Expr::Binary { left, right, .. } => {
            scan_shader_expr_for_storage_access(left, storage_uniforms, access);
            scan_shader_expr_for_storage_access(right, storage_uniforms, access);
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _) => {
            scan_shader_expr_for_storage_access(operand, storage_uniforms, access);
        }
        Expr::Call { callee, args, .. } => {
            scan_shader_expr_for_storage_access(callee, storage_uniforms, access);
            for arg in args {
                scan_shader_expr_for_storage_access(&arg.value, storage_uniforms, access);
            }
        }
        Expr::StageCall { args, .. } => {
            for arg in args {
                scan_shader_expr_for_storage_access(&arg.value, storage_uniforms, access);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            scan_shader_expr_for_storage_access(receiver, storage_uniforms, access);
            for arg in args {
                scan_shader_expr_for_storage_access(&arg.value, storage_uniforms, access);
            }
        }
        Expr::Field { object, .. } => {
            scan_shader_expr_for_storage_access(object, storage_uniforms, access);
        }
        Expr::Struct { fields, rest, .. } => {
            for (_, value) in fields {
                scan_shader_expr_for_storage_access(value, storage_uniforms, access);
            }
            if let Some(rest) = rest {
                scan_shader_expr_for_storage_access(rest, storage_uniforms, access);
            }
        }
        Expr::AggregateInit { fields, .. } => {
            for (_, value) in fields {
                scan_shader_expr_for_storage_access(value, storage_uniforms, access);
            }
        }
        Expr::EnumVariant { fields, .. } => match fields {
            crate::ast::EnumVariantFields::Tuple(values) => {
                for value in values {
                    scan_shader_expr_for_storage_access(value, storage_uniforms, access);
                }
            }
            crate::ast::EnumVariantFields::Struct(fields) => {
                for (_, value) in fields {
                    scan_shader_expr_for_storage_access(value, storage_uniforms, access);
                }
            }
            crate::ast::EnumVariantFields::Unit => {}
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
            for value in values {
                scan_shader_expr_for_storage_access(value, storage_uniforms, access);
            }
        }
        Expr::Range { start, end, .. } => {
            if let Some(start) = start {
                scan_shader_expr_for_storage_access(start, storage_uniforms, access);
            }
            if let Some(end) = end {
                scan_shader_expr_for_storage_access(end, storage_uniforms, access);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            scan_shader_expr_for_storage_access(condition, storage_uniforms, access);
            scan_shader_block_for_storage_access(then_branch, storage_uniforms, access);
            if let Some(else_branch) = else_branch {
                scan_shader_else_branch_for_storage_access(else_branch, storage_uniforms, access);
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            scan_shader_expr_for_storage_access(scrutinee, storage_uniforms, access);
            for arm in arms {
                if let Some(guard) = &arm.guard {
                    scan_shader_expr_for_storage_access(guard, storage_uniforms, access);
                }
                scan_shader_expr_for_storage_access(&arm.body, storage_uniforms, access);
            }
        }
        Expr::Lambda { body, .. } => {
            scan_shader_expr_for_storage_access(body, storage_uniforms, access);
        }
        Expr::MacroCall { args, .. } => {
            for arg in args {
                scan_shader_expr_for_storage_access(arg, storage_uniforms, access);
            }
        }
        Expr::PtrOffset {
            pointer, offset, ..
        } => {
            scan_shader_expr_for_storage_access(pointer, storage_uniforms, access);
            scan_shader_expr_for_storage_access(offset, storage_uniforms, access);
        }
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _) => {}
        _ => {}
    }
}

fn scan_shader_else_branch_for_storage_access(
    else_branch: &ElseBranch,
    storage_uniforms: &HashSet<String>,
    access: &mut HashMap<String, ShaderStorageAccess>,
) {
    match else_branch {
        ElseBranch::Else(block) => {
            scan_shader_block_for_storage_access(block, storage_uniforms, access);
        }
        ElseBranch::ElseIf(condition, block, next) => {
            scan_shader_expr_for_storage_access(condition, storage_uniforms, access);
            scan_shader_block_for_storage_access(block, storage_uniforms, access);
            if let Some(next) = next {
                scan_shader_else_branch_for_storage_access(next, storage_uniforms, access);
            }
        }
    }
}

fn mark_storage_write_target(
    expr: &Expr,
    storage_uniforms: &HashSet<String>,
    access: &mut HashMap<String, ShaderStorageAccess>,
) {
    match expr {
        Expr::Index { object, index, .. } => {
            if let Some(name) = storage_uniform_ident(object, storage_uniforms) {
                access.entry(name.to_string()).or_default().writes = true;
            } else {
                scan_shader_expr_for_storage_access(object, storage_uniforms, access);
            }
            scan_shader_expr_for_storage_access(index, storage_uniforms, access);
        }
        Expr::Field { object, .. } => mark_storage_write_target(object, storage_uniforms, access),
        Expr::Deref(value, _) => mark_storage_write_target(value, storage_uniforms, access),
        _ => scan_shader_expr_for_storage_access(expr, storage_uniforms, access),
    }
}

fn storage_uniform_ident<'a>(
    expr: &'a Expr,
    storage_uniforms: &HashSet<String>,
) -> Option<&'a str> {
    match expr {
        Expr::Ident(name, _) if storage_uniforms.contains(name) => Some(name.as_str()),
        _ => None,
    }
}

fn collect_tensor_bindings(
    shader: &TypedShader,
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
            element_type: shader
                .ast
                .uniforms
                .iter()
                .find(|uniform| uniform.name == binding.key)
                .and_then(|uniform| storage_buffer_element_type_name(&uniform.ty))
                .unwrap_or_else(|| infer_tensor_element_type(binding).to_string()),
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

fn ui_value_as_f32(value: &UiValue) -> Option<f32> {
    match value {
        UiValue::Float(value) => Some(*value as f32),
        UiValue::Int(value) => Some(*value as f32),
        UiValue::String(value) => value.parse::<f32>().ok(),
        _ => None,
    }
}

fn ui_value_as_u32(value: &UiValue) -> Option<u32> {
    match value {
        UiValue::Int(value) => (*value).try_into().ok(),
        UiValue::Float(value) if *value >= 0.0 => Some(*value as u32),
        UiValue::String(value) => value.parse::<u32>().ok(),
        _ => None,
    }
}

fn ui_value_as_bool(value: &UiValue) -> Option<bool> {
    match value {
        UiValue::Bool(value) => Some(*value),
        UiValue::Int(value) => Some(*value != 0),
        UiValue::Float(value) => Some(value.abs() > f64::EPSILON),
        UiValue::String(value) => match value.trim().to_ascii_lowercase().as_str() {
            "1" | "true" | "yes" | "on" => Some(true),
            "0" | "false" | "no" | "off" => Some(false),
            _ => None,
        },
        _ => None,
    }
}

fn scene_prop_string(
    props: &std::collections::BTreeMap<String, UiValue>,
    key: &str,
) -> Option<String> {
    props
        .get(key)
        .and_then(UiValue::as_str)
        .map(str::trim)
        .filter(|value| !value.is_empty())
        .map(ToString::to_string)
}

fn scene_prop_string_any(
    props: &std::collections::BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<String> {
    keys.iter().find_map(|key| scene_prop_string(props, key))
}

fn scene_prop_f32(
    props: &std::collections::BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<f32> {
    keys.iter()
        .find_map(|key| props.get(*key).and_then(ui_value_as_f32))
}

fn scene_prop_u32(
    props: &std::collections::BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<u32> {
    keys.iter()
        .find_map(|key| props.get(*key).and_then(ui_value_as_u32))
}

fn scene_prop_bool(
    props: &std::collections::BTreeMap<String, UiValue>,
    keys: &[&str],
) -> Option<bool> {
    keys.iter()
        .find_map(|key| props.get(*key).and_then(ui_value_as_bool))
}

fn scene_prop_vec3(
    props: &std::collections::BTreeMap<String, UiValue>,
    key_prefix: &str,
) -> Option<[f32; 3]> {
    let x = scene_prop_f32(props, &[&format!("{key_prefix}.x")])?;
    let y = scene_prop_f32(props, &[&format!("{key_prefix}.y")])?;
    let z = scene_prop_f32(props, &[&format!("{key_prefix}.z")])?;
    Some([x, y, z])
}

fn collect_scene_camera_binding(
    props: &std::collections::BTreeMap<String, UiValue>,
) -> Option<RealtimeViewportCameraBinding> {
    let camera = RealtimeViewportCameraBinding {
        position: scene_prop_vec3(props, "camera.position"),
        target: scene_prop_vec3(props, "camera.target"),
        fov_y_degrees: scene_prop_f32(props, &["camera.fov_y_degrees", "camera.fov_y"]),
        near_plane: scene_prop_f32(props, &["camera.near_plane", "camera.near"]),
        far_plane: scene_prop_f32(props, &["camera.far_plane", "camera.far"]),
    };
    (camera.position.is_some()
        || camera.target.is_some()
        || camera.fov_y_degrees.is_some()
        || camera.near_plane.is_some()
        || camera.far_plane.is_some())
    .then_some(camera)
}

fn collect_scene_presentation_binding(
    props: &std::collections::BTreeMap<String, UiValue>,
) -> Option<RealtimeViewportPresentationBinding> {
    let gizmo = collect_scene_gizmo_binding(props);
    let presentation = RealtimeViewportPresentationBinding {
        profile: scene_prop_string_any(props, &["viewport.profile", "viewport_profile"]),
        fog_density: scene_prop_f32(props, &["viewport.fog_density", "viewport_fog_density"]),
        particle_budget: scene_prop_u32(
            props,
            &["viewport.particle_budget", "viewport_particle_budget"],
        ),
        gizmo,
    };
    (presentation.profile.is_some()
        || presentation.fog_density.is_some()
        || presentation.particle_budget.is_some()
        || presentation.gizmo.is_some())
    .then_some(presentation)
}

fn collect_scene_gizmo_binding(
    props: &std::collections::BTreeMap<String, UiValue>,
) -> Option<RealtimeViewportGizmoBinding> {
    let gizmo = RealtimeViewportGizmoBinding {
        profile_id: scene_prop_string_any(props, &["gizmo.profile", "gizmo_profile"]),
        visible: scene_prop_bool(props, &["gizmo.visible", "gizmo_visible"]),
        default_mode: scene_prop_string_any(props, &["gizmo.default_mode", "gizmo_default_mode"]),
        default_space: scene_prop_string_any(
            props,
            &["gizmo.default_space", "gizmo_default_space"],
        ),
        drag_trigger: scene_prop_string_any(props, &["gizmo.drag_trigger", "gizmo_drag_trigger"]),
        selection_required: scene_prop_bool(
            props,
            &["gizmo.selection_required", "gizmo_selection_required"],
        ),
        translate_hotkey: scene_prop_string_any(
            props,
            &["gizmo.hotkey.translate", "gizmo_hotkey_translate"],
        ),
        rotate_hotkey: scene_prop_string_any(
            props,
            &["gizmo.hotkey.rotate", "gizmo_hotkey_rotate"],
        ),
        scale_hotkey: scene_prop_string_any(props, &["gizmo.hotkey.scale", "gizmo_hotkey_scale"]),
        cycle_space_hotkey: scene_prop_string_any(
            props,
            &["gizmo.hotkey.cycle_space", "gizmo_hotkey_cycle_space"],
        ),
        toggle_snap_hotkey: scene_prop_string_any(
            props,
            &["gizmo.hotkey.toggle_snap", "gizmo_hotkey_toggle_snap"],
        ),
        translate_snap_units: scene_prop_f32(
            props,
            &["gizmo.snap.translate", "gizmo_snap_translate"],
        ),
        rotate_snap_degrees: scene_prop_f32(
            props,
            &["gizmo.snap.rotate_degrees", "gizmo_snap_rotate_degrees"],
        ),
        scale_snap_percent: scene_prop_f32(
            props,
            &["gizmo.snap.scale_percent", "gizmo_snap_scale_percent"],
        ),
        snap_default_enabled: scene_prop_bool(
            props,
            &["gizmo.snap.default_enabled", "gizmo_snap_default_enabled"],
        ),
    };
    (gizmo.profile_id.is_some()
        || gizmo.visible.is_some()
        || gizmo.default_mode.is_some()
        || gizmo.default_space.is_some()
        || gizmo.drag_trigger.is_some()
        || gizmo.selection_required.is_some()
        || gizmo.translate_hotkey.is_some()
        || gizmo.rotate_hotkey.is_some()
        || gizmo.scale_hotkey.is_some()
        || gizmo.cycle_space_hotkey.is_some()
        || gizmo.toggle_snap_hotkey.is_some()
        || gizmo.translate_snap_units.is_some()
        || gizmo.rotate_snap_degrees.is_some()
        || gizmo.scale_snap_percent.is_some()
        || gizmo.snap_default_enabled.is_some())
    .then_some(gizmo)
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
        let camera = collect_scene_camera_binding(&node.props);
        let presentation = collect_scene_presentation_binding(&node.props);

        scenes.push(RealtimeSceneBinding {
            viewport_node: surface.id.clone(),
            viewport_kind: viewport_kind.to_string(),
            scene,
            title: surface.title.clone(),
            material_refs,
            shader_bundle_ref_keys: default_shader_keys.clone(),
            camera,
            presentation,
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
        push_asset_binding_from_prop(&mut assets, node, "asset", "runtime");
        push_asset_binding_from_first_prop(
            &mut assets,
            node,
            &["font_asset", "font_path", "font_source"],
            "font",
        );
    }
    assets.sort_by(|left, right| left.key.cmp(&right.key));
    assets.dedup_by(|left, right| left.key == right.key);
    assets
}

fn push_asset_binding_from_prop(
    assets: &mut Vec<RealtimeAssetBinding>,
    node: &UiNode,
    key: &str,
    kind: &str,
) {
    if let Some(source) =
        node_prop_string(node, key).and_then(|value| normalize_asset_source(&value))
    {
        assets.push(RealtimeAssetBinding {
            key: realtime_asset_key(kind, &source),
            kind: kind.to_string(),
            source,
        });
    }
}

fn push_asset_binding_from_first_prop(
    assets: &mut Vec<RealtimeAssetBinding>,
    node: &UiNode,
    keys: &[&str],
    kind: &str,
) {
    if let Some(source) =
        first_string_prop(node, keys).and_then(|value| normalize_asset_source(&value))
    {
        assets.push(RealtimeAssetBinding {
            key: realtime_asset_key(kind, &source),
            kind: kind.to_string(),
            source,
        });
    }
}

fn normalize_asset_source(source: &str) -> Option<String> {
    let trimmed = source.trim();
    if trimmed.is_empty() {
        None
    } else {
        Some(trimmed.to_string())
    }
}

fn realtime_asset_key(kind: &str, source: &str) -> String {
    format!("{kind}::{source}")
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
    worlds: &[RealtimeWorldBinding],
    has_entanglements: bool,
    has_patches: bool,
    has_laws: bool,
    has_converges: bool,
    has_orchestrations: bool,
    has_explicit_compute_metadata: bool,
) -> Vec<String> {
    let mut caps = Vec::new();
    if has_patches {
        caps.push("patch.transactions".to_string());
    }
    if has_laws {
        caps.push("law.invariants".to_string());
    }
    if has_converges {
        caps.push("converge.dispatch".to_string());
    }
    if has_entanglements {
        caps.push(kain_entangle::STATE_ENTANGLE_CAPABILITY.to_string());
    }
    if has_orchestrations {
        caps.push("orchestrate.pipeline".to_string());
    }
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
    for world in worlds {
        for surface in &world.surfaces {
            match surface.kind.as_str() {
                "native_ui" => caps.push("world.native-ui".to_string()),
                "viewport3d" => {
                    caps.push("world.viewport3d".to_string());
                    caps.push("viewport.3d".to_string());
                }
                "web" => caps.push("world.web".to_string()),
                "ue5" => caps.push("world.ue5".to_string()),
                _ => {}
            }
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
    worlds: &[RealtimeWorldBinding],
    tool_caps: &[String],
    has_entanglements: bool,
    has_patches: bool,
    has_laws: bool,
    has_converges: bool,
    has_orchestrations: bool,
    has_explicit_compute_metadata: bool,
) -> Vec<String> {
    let has_compute_refs = shader_bundle_refs
        .iter()
        .any(|entry| entry.stage == "compute");
    let has_world_native_ui = bundle_has_world_surface(worlds, WorldSurfaceKind::NativeUi);
    let has_world_viewport3d = bundle_has_world_surface(worlds, WorldSurfaceKind::Viewport3d);
    let has_world_web = bundle_has_world_surface(worlds, WorldSurfaceKind::Web);
    let has_world_ue5 = bundle_has_world_surface(worlds, WorldSurfaceKind::Ue5);
    let mut requirements = vec![
        "runtime.contract.bundle".to_string(),
        "shader.bundle.metadata".to_string(),
    ];
    if has_patches {
        requirements.push("patch.transactions".to_string());
    }
    if has_laws {
        requirements.push("law.invariants".to_string());
    }
    if has_converges {
        requirements.push("converge.dispatch".to_string());
    }
    if has_entanglements {
        requirements.push(kain_entangle::STATE_ENTANGLE_CAPABILITY.to_string());
    }
    if has_orchestrations {
        requirements.push("orchestrate.pipeline".to_string());
    }
    if has_world_native_ui {
        requirements.push("world.native-ui".to_string());
    }
    if has_world_viewport3d {
        requirements.push("world.viewport3d".to_string());
    }
    if has_world_web {
        requirements.push("world.web".to_string());
    }
    if has_world_ue5 {
        requirements.push("world.ue5".to_string());
    }
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
        CompileTarget::C | CompileTarget::Llvm => requirements.push("host.raw-native".to_string()),
        CompileTarget::Js | CompileTarget::Ts | CompileTarget::Wasm | CompileTarget::Hybrid
            if has_world_web =>
        {
            requirements.push("host.web".to_string());
        }
        CompileTarget::Ue5 | CompileTarget::Ue5Editor if has_world_ue5 => {
            requirements.push("host.ue5".to_string());
        }
        _ => {}
    }
    requirements.sort();
    requirements.dedup();
    requirements
}

fn compile_target_name(target: CompileTarget) -> &'static str {
    if target == CompileTarget::Wasm {
        "wasm"
    } else if target == CompileTarget::Js {
        "js"
    } else if target == CompileTarget::Ts {
        "ts"
    } else if target == CompileTarget::Hybrid {
        "hybrid"
    } else if target == CompileTarget::C {
        "c"
    } else if target == CompileTarget::Llvm {
        "llvm"
    } else if target == CompileTarget::Rust {
        "rust"
    } else if target == CompileTarget::Cpp {
        "cpp"
    } else if target == CompileTarget::Ue5 {
        "ue5"
    } else if target == CompileTarget::Ue5Editor {
        "ue5-editor"
    } else if target == CompileTarget::Usf {
        "usf"
    } else if target == CompileTarget::Spirv {
        "spirv"
    } else if target == CompileTarget::Hlsl {
        "hlsl"
    } else if target == CompileTarget::Interpret {
        "interpret"
    } else if target == CompileTarget::Test {
        "test"
    } else {
        "ks"
    }
}

fn render_contract_type_name(ty: &Type) -> String {
    match ty {
        Type::Named { name, .. } => name.clone(),
        Type::Tuple(_, _) => "Tuple".to_string(),
        Type::Array(_, _, _) => "Array".to_string(),
        Type::Slice(_, _) => "Slice".to_string(),
        Type::Ref { inner, .. } => format!("&{}", render_contract_type_name(inner)),
        Type::Ptr { inner, .. } => format!("*{}", render_contract_type_name(inner)),
        Type::Option(inner, _) => format!("Option<{}>", render_contract_type_name(inner)),
        Type::Result(ok, _, _) => format!("Result<{}>", render_contract_type_name(ok)),
        Type::Unit(_) => "Unit".to_string(),
        Type::Never(_) => "Never".to_string(),
        _ => "Unknown".to_string(),
    }
}

fn sanitize_bundle_symbol_ident(name: &str) -> String {
    let sanitized = name
        .chars()
        .map(|character| {
            if character.is_ascii_alphanumeric() || character == '_' {
                character
            } else {
                '_'
            }
        })
        .collect::<String>();
    if sanitized.is_empty() {
        "lane".to_string()
    } else {
        sanitized
    }
}

fn bundle_has_world_surface(worlds: &[RealtimeWorldBinding], kind: WorldSurfaceKind) -> bool {
    let expected = kind.as_str();
    worlds.iter().any(|world| {
        world
            .surfaces
            .iter()
            .any(|surface| surface.kind == expected)
    })
}

#[cfg(test)]
mod tests {
    use crate::ui::build_ui_output_from_source;
    use crate::{
        diagnostics, emit_realtime_app_bundle, gpu_storage_element_stride_bytes,
        realtime_app_bundle_from_json, types, CompileTarget, Lexer, Parser,
    };

    #[test]
    fn emits_realtime_bundle_with_viewport_scene_binding() {
        let source = r#"
component App():
    render <panel>
        <viewport3d title="Hero" scene="geometry_fixture" material="terrain" />
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
        assert_eq!(bundle.render.scenes[0].scene, "geometry_fixture");
        assert_eq!(
            bundle.render.scenes[0].material_refs,
            vec!["terrain".to_string()]
        );
        assert_eq!(
            bundle.render.scenes[0].camera, None,
            "legacy viewport bindings should stay sparse when no camera metadata is authored"
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
        "scene": "geometry_fixture",
        "title": "Viewport",
        "material_refs": ["terrain"],
        "shader_bundle_ref_keys": ["shader::terrain::fragment"],
        "camera": {
          "position": [8.0, 4.5, 16.0],
          "target": [0.0, 1.5, 0.0],
          "fov_y_degrees": 58.0,
          "near_plane": 0.05,
          "far_plane": 220.0
        },
        "presentation": {
          "profile": "geometry_fixture",
          "fog_density": 0.018,
          "particle_budget": 192,
          "gizmo": {
            "profile_id": "dcc_transform_universal",
            "visible": true,
            "default_mode": "translate",
            "default_space": "world",
            "drag_trigger": "ctrl_primary_drag",
            "selection_required": true,
            "translate_hotkey": "T",
            "rotate_hotkey": "R",
            "scale_hotkey": "Y",
            "cycle_space_hotkey": "U",
            "toggle_snap_hotkey": "I",
            "translate_snap_units": 0.5,
            "rotate_snap_degrees": 15.0,
            "scale_snap_percent": 10.0,
            "snap_default_enabled": false
          }
        }
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
        assert_eq!(bundle.render.scenes[0].scene, "geometry_fixture");
        assert_eq!(
            bundle.render.scenes[0]
                .camera
                .as_ref()
                .and_then(|camera| camera.position),
            Some([8.0, 4.5, 16.0])
        );
        assert_eq!(
            bundle.render.scenes[0]
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.profile.as_deref()),
            Some("geometry_fixture")
        );
        assert_eq!(
            bundle.render.scenes[0]
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.gizmo.as_ref())
                .and_then(|gizmo| gizmo.profile_id.as_deref()),
            Some("dcc_transform_universal")
        );
        assert_eq!(bundle.render.materials[0].id, "terrain");
    }

    #[test]
    fn emits_bundle_owned_camera_and_presentation_metadata_for_viewports() {
        let source = r#"
component App():
    render <panel>
        <viewport3d
            title="Tensor Probe"
            scene="geometry_fixture"
            camera.position.x={12.0}
            camera.position.y={6.0}
            camera.position.z={18.0}
            camera.target.x={0.0}
            camera.target.y={2.0}
            camera.target.z={0.0}
            camera.fov_y={48.0}
            camera.near_plane={0.05}
            camera.far_plane={320.0}
            viewport.profile="geometry_fixture"
            viewport.fog_density={0.012}
            viewport.particle_budget={288}
            gizmo.profile="dcc_transform_universal"
            gizmo.visible={true}
            gizmo.default_mode="translate"
            gizmo.default_space="world"
            gizmo.drag_trigger="ctrl_primary_drag"
            gizmo.selection_required={true}
            gizmo.hotkey.translate="T"
            gizmo.hotkey.rotate="R"
            gizmo.hotkey.scale="Y"
            gizmo.hotkey.cycle_space="U"
            gizmo.hotkey.toggle_snap="I"
            gizmo.snap.translate={0.5}
            gizmo.snap.rotate_degrees={15.0}
            gizmo.snap.scale_percent={10.0}
            gizmo.snap.default_enabled={false}
        />
    </panel>
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = diagnostics::SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<input>")
            .parse()
            .expect("ast");
        let typed = types::check(&ast, &span_mapper, "<input>").expect("typed");
        let ui = build_ui_output_from_source(source, "App").expect("ui");

        let bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Llvm);
        let scene = &bundle.render.scenes[0];
        assert_eq!(scene.scene, "geometry_fixture");
        assert_eq!(
            scene.camera.as_ref().and_then(|camera| camera.position),
            Some([12.0, 6.0, 18.0])
        );
        assert_eq!(
            scene.camera.as_ref().and_then(|camera| camera.target),
            Some([0.0, 2.0, 0.0])
        );
        assert_eq!(
            scene
                .camera
                .as_ref()
                .and_then(|camera| camera.fov_y_degrees),
            Some(48.0)
        );
        assert_eq!(
            scene
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.profile.as_deref()),
            Some("geometry_fixture")
        );
        assert_eq!(
            scene
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.fog_density),
            Some(0.012)
        );
        assert_eq!(
            scene
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.particle_budget),
            Some(288)
        );
        assert_eq!(
            scene
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.gizmo.as_ref())
                .and_then(|gizmo| gizmo.default_mode.as_deref()),
            Some("translate")
        );
        assert_eq!(
            scene
                .presentation
                .as_ref()
                .and_then(|presentation| presentation.gizmo.as_ref())
                .and_then(|gizmo| gizmo.toggle_snap_hotkey.as_deref()),
            Some("I")
        );
    }

    #[test]
    fn emits_shader_canvas_bindings_for_native_runtime_consumers() {
        let source = r#"
shader fragment hero_surface(uv: Vec2) -> Vec4:
    return vec4(uv.x, uv.y, 1.0, 1.0)

component App():
    render <panel>
        <canvas title="Hero Surface" text="Fast lane" shader_ref="hero_surface" shader_stage="fragment" shader_format="spirv" font_asset="fonts/ui/hero.ttf" />
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
        assert_eq!(bundle.shader_canvases[0].surface_id, "surface.node.1");
        assert_eq!(bundle.shader_canvases[0].shader_ref, "hero_surface");
        assert_eq!(
            bundle.shader_canvases[0].shader_bundle_ref_key.as_deref(),
            Some("shader::hero_surface::fragment")
        );
        assert_eq!(bundle.shader_canvases[0].composition_mode, "shader-canvas");
        assert_eq!(bundle.shader_canvases[0].font_atlases.len(), 1);
        assert_eq!(
            bundle.shader_canvases[0].font_atlases[0]
                .asset_key
                .as_deref(),
            Some("font::fonts/ui/hero.ttf")
        );
        assert_eq!(bundle.shader_canvases[0].text_runs.len(), 2);
        assert_eq!(bundle.shader_canvases[0].resource_bindings.len(), 4);
        assert!(bundle
            .assets
            .iter()
            .any(|asset| asset.key == "font::fonts/ui/hero.ttf" && asset.kind == "font"));
        assert!(bundle
            .tool_caps
            .iter()
            .any(|entry| entry == "ui.shader-canvas"));
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

    #[test]
    fn infers_storage_buffer_access_from_shader_usage_not_names() {
        let source = r#"
shader compute AccessProbe() -> Void:
    uniform table: StorageBuffer<Float> @0
    uniform indices: StorageBuffer<UInt> @1
    uniform scores: StorageBuffer<Float> @2
    uniform count: UInt @3

    let idx = dispatch_thread_id.x
    if idx >= count:
        return
    let source_idx = indices[idx]
    let sample = table[source_idx]
    scores[idx] = sample
    return
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = diagnostics::SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<input>")
            .parse()
            .expect("ast");
        let typed = types::check(&ast, &span_mapper, "<input>").expect("typed");

        let bundle = emit_realtime_app_bundle(&typed, None, CompileTarget::Llvm);
        let shader = &bundle.shader_bundle_refs[0];
        let access = shader
            .resource_bindings
            .iter()
            .map(|binding| (binding.key.clone(), binding.access.clone()))
            .collect::<std::collections::HashMap<_, _>>();

        assert_eq!(access.get("table").map(String::as_str), Some("read"));
        assert_eq!(access.get("indices").map(String::as_str), Some("read"));
        assert_eq!(access.get("scores").map(String::as_str), Some("write"));
        assert_eq!(access.get("count").map(String::as_str), Some("read"));
    }

    #[test]
    fn preserves_storage_buffer_element_types_for_tensor_bindings() {
        let source = r#"
shader compute PackedProbe() -> Void:
    uniform table: StorageBuffer<u8> @0
    uniform indices: StorageBuffer<UInt> @1
    uniform scores: StorageBuffer<Float> @2

    let idx = dispatch_thread_id.x
    let source_idx = indices[idx]
    let _sample = table[source_idx]
    return
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = diagnostics::SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<input>")
            .parse()
            .expect("ast");
        let typed = types::check(&ast, &span_mapper, "<input>").expect("typed");

        let bundle = emit_realtime_app_bundle(&typed, None, CompileTarget::Llvm);
        let shader = &bundle.shader_bundle_refs[0];
        let element_types = shader
            .tensor_bindings
            .iter()
            .map(|binding| (binding.key.clone(), binding.element_type.clone()))
            .collect::<std::collections::HashMap<_, _>>();

        assert_eq!(element_types.get("table").map(String::as_str), Some("u8"));
        assert_eq!(
            element_types.get("indices").map(String::as_str),
            Some("u32")
        );
        assert_eq!(element_types.get("scores").map(String::as_str), Some("f32"));
    }

    #[test]
    fn gpu_storage_element_stride_matches_compiler_layout_truth() {
        assert_eq!(gpu_storage_element_stride_bytes("bool"), Some(4));
        assert_eq!(gpu_storage_element_stride_bytes("u8"), Some(1));
        assert_eq!(gpu_storage_element_stride_bytes("f16"), Some(2));
        assert_eq!(gpu_storage_element_stride_bytes("u32"), Some(4));
        assert_eq!(gpu_storage_element_stride_bytes("vec2<f32>"), Some(8));
        assert_eq!(gpu_storage_element_stride_bytes("vec3<f32>"), Some(16));
        assert_eq!(gpu_storage_element_stride_bytes("vec3<i32>"), Some(16));
        assert_eq!(gpu_storage_element_stride_bytes("uvec3"), Some(16));
        assert_eq!(gpu_storage_element_stride_bytes("vec4<u32>"), Some(16));
    }
}

fn storage_buffer_element_type_name(ty: &Type) -> Option<String> {
    match ty {
        Type::Named { name, generics, .. } if name == "StorageBuffer" => {
            generics.first().and_then(normalize_gpu_element_type_name)
        }
        _ => None,
    }
}

pub fn gpu_storage_element_stride_bytes(element_type: &str) -> Option<usize> {
    // Proof: crates/core/z3/proofs/gpu-storage-element-stride-matches-compiler-layout.yaml
    match element_type.trim().to_ascii_lowercase().as_str() {
        "bool" | "u32" | "uint32" | "uint" | "i32" | "int32" | "int" | "f32" | "float32"
        | "float" => Some(4),
        "u8" | "uint8" | "byte" | "i8" | "int8" | "sbyte" => Some(1),
        "u16" | "uint16" | "i16" | "int16" | "f16" | "half" | "bf16" | "bfloat16" => Some(2),
        "u64" | "uint64" | "ulong" | "i64" | "int64" | "long" | "f64" | "float64" | "double" => {
            Some(8)
        }
        "vec2" | "ivec2" | "uvec2" | "vec2<f32>" | "vec2<float32>" | "vec2<float>"
        | "vec2<i32>" | "vec2<int32>" | "vec2<int>" | "vec2<u32>" | "vec2<uint32>"
        | "vec2<uint>" => Some(8),
        "vec3" | "ivec3" | "uvec3" | "vec3<f32>" | "vec3<float32>" | "vec3<float>"
        | "vec3<i32>" | "vec3<int32>" | "vec3<int>" | "vec3<u32>" | "vec3<uint32>"
        | "vec3<uint>" => Some(16),
        "vec4" | "ivec4" | "uvec4" | "vec4<f32>" | "vec4<float32>" | "vec4<float>"
        | "vec4<i32>" | "vec4<int32>" | "vec4<int>" | "vec4<u32>" | "vec4<uint32>"
        | "vec4<uint>" => Some(16),
        _ => None,
    }
}

fn normalize_gpu_element_type_name(ty: &Type) -> Option<String> {
    match ty {
        Type::Named { name, generics, .. } => match name.as_str() {
            "Bool" | "bool" => Some("bool".to_string()),
            "u8" | "U8" | "Byte" => Some("u8".to_string()),
            "i8" | "I8" | "SByte" => Some("i8".to_string()),
            "u16" | "U16" => Some("u16".to_string()),
            "i16" | "I16" => Some("i16".to_string()),
            "u32" | "U32" | "UInt" => Some("u32".to_string()),
            "i32" | "I32" | "Int" => Some("i32".to_string()),
            "u64" | "U64" | "ULong" => Some("u64".to_string()),
            "i64" | "I64" | "Long" => Some("i64".to_string()),
            "f32" | "Float" => Some("f32".to_string()),
            "f64" | "Double" => Some("f64".to_string()),
            "f16" | "Half" => Some("f16".to_string()),
            "bf16" | "BFloat16" => Some("bf16".to_string()),
            "Vec2" => Some(format!(
                "vec2<{}>",
                vector_scalar_lane_name(generics.first()).unwrap_or("f32")
            )),
            "Vec3" => Some(format!(
                "vec3<{}>",
                vector_scalar_lane_name(generics.first()).unwrap_or("f32")
            )),
            "Vec4" => Some(format!(
                "vec4<{}>",
                vector_scalar_lane_name(generics.first()).unwrap_or("f32")
            )),
            "IVec2" => Some("vec2<i32>".to_string()),
            "IVec3" => Some("vec3<i32>".to_string()),
            "IVec4" => Some("vec4<i32>".to_string()),
            "UVec2" => Some("vec2<u32>".to_string()),
            "UVec3" => Some("vec3<u32>".to_string()),
            "UVec4" => Some("vec4<u32>".to_string()),
            _ => None,
        },
        _ => None,
    }
}

fn vector_scalar_lane_name(ty: Option<&Type>) -> Option<&'static str> {
    match ty {
        Some(Type::Named { name, .. }) => match name.as_str() {
            "Int" | "I32" | "i32" => Some("i32"),
            "UInt" | "U32" | "u32" => Some("u32"),
            _ => Some("f32"),
        },
        _ => None,
    }
}
