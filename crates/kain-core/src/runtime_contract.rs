use std::collections::BTreeSet;

use serde::{Deserialize, Serialize};

use crate::ast::{ConvergeSelector, ShaderStage, Type, WorldSurfaceKind, COMPUTE_PLAN_CAPABILITY_KEY};
use crate::low_level_memory::backend_memory_capabilities;
use crate::types::{PatchUndoMode, TypedConverge, TypedOrchestrate, TypedPatch, TypedWorld};
use crate::ui::render_authored_expr_contract;
use crate::{CompileTarget, TypedItem, TypedProgram};

pub const RUNTIME_CONTRACT_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeContractBundle {
    pub schema_version: u32,
    pub target: String,
    pub required_capabilities: Vec<RuntimeCapability>,
    pub service_bindings: Vec<RuntimeServiceBinding>,
    pub items: Vec<RuntimeContractItem>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub patches: Vec<RuntimePatchContract>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub converges: Vec<RuntimeConvergeContract>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub worlds: Vec<RuntimeWorldContract>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub active_world: Option<RuntimeWorldContract>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub orchestrations: Vec<RuntimeOrchestrationContract>,
    pub reflection: RuntimeReflectionSummary,
    pub compatibility: RuntimeCompatibilityMetadata,
    pub reflection_payload: Option<RuntimeReflectionPayload>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeCapability {
    pub key: String,
    pub source: String,
    pub detail: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeServiceBinding {
    pub service: String,
    pub provider: String,
    pub lane: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimePlatformAvailabilityMetadata {
    pub schema_version: u32,
    pub target_platforms: Vec<String>,
    pub active_platforms: Vec<String>,
    pub runtime_platform: Option<String>,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeVersionRecord {
    pub major: u32,
    pub minor: u32,
    pub patch: u32,
    pub string: String,
}

impl RuntimeVersionRecord {
    pub fn new(major: u32, minor: u32, patch: u32, string: impl Into<String>) -> Self {
        Self {
            major,
            minor,
            patch,
            string: string.into(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeLifecyclePolicy {
    pub supported: bool,
    pub mode: String,
    pub notes: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeCompatibilityMetadata {
    pub schema_version: u32,
    pub bundle_target: String,
    pub bundle_lane: String,
    pub runtime_lane: Option<String>,
    pub compatibility_class: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub platform_availability: Option<RuntimePlatformAvailabilityMetadata>,
    pub runtime_version: Option<RuntimeVersionRecord>,
    pub abi_version: Option<RuntimeVersionRecord>,
    pub install: RuntimeLifecyclePolicy,
    pub update: RuntimeLifecyclePolicy,
    pub uninstall: RuntimeLifecyclePolicy,
    pub migration_hints: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeContractItem {
    pub id: String,
    pub name: String,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimePatchContract {
    pub name: String,
    pub mutation_paths: Vec<String>,
    pub replay_log_schema: Vec<String>,
    pub invalidation_keys: Vec<String>,
    pub collaboration_event: String,
    pub undo_mode: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeConvergeLaneContract {
    pub lane_name: String,
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selector_kind: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub selector_value: Option<String>,
    pub symbol: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeConvergeContract {
    pub name: String,
    pub dispatcher_symbol: String,
    pub spec_lane: RuntimeConvergeLaneContract,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub fast_lanes: Vec<RuntimeConvergeLaneContract>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub verify_random_count: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldStateContract {
    pub name: String,
    pub type_name: String,
    pub initial_expr: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldSurfaceContract {
    pub kind: String,
    pub authored_expr: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldContract {
    pub name: String,
    pub state_slots: Vec<RuntimeWorldStateContract>,
    pub surfaces: Vec<RuntimeWorldSurfaceContract>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeOrchestrationStageContract {
    pub runtime: String,
    pub function: String,
    pub binding_name: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeOrchestrationContract {
    pub name: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub return_type: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub stages: Vec<RuntimeOrchestrationStageContract>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeReflectionSummary {
    pub emitted: bool,
    pub schema_names: Vec<String>,
    pub notes: Vec<String>,
}

/// Full reflection payload with type schemas and item metadata
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeReflectionPayload {
    pub schema_version: u32,
    pub types: Vec<ReflectedType>,
    pub items: Vec<ReflectedItem>,
    pub actors: Vec<ReflectedActor>,
    pub components: Vec<ReflectedComponent>,
    pub messages: Vec<ReflectedMessage>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedType {
    pub type_id: u64,
    pub name: String,
    pub kind: String,
    pub size_hint: Option<usize>,
    pub fields: Vec<ReflectedField>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedField {
    pub name: String,
    pub type_name: String,
    pub offset_hint: Option<usize>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedItem {
    pub item_id: u64,
    pub name: String,
    pub kind: String,
    pub module_path: String,
    pub type_id: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedActor {
    pub item_id: u64,
    pub name: String,
    pub message_types: Vec<String>,
    pub state_type: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedComponent {
    pub item_id: u64,
    pub name: String,
    pub props: Vec<ReflectedField>,
    pub state_type: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ReflectedMessage {
    pub item_id: u64,
    pub name: String,
    pub fields: Vec<ReflectedField>,
}

pub fn emit_runtime_contract_bundle(
    program: &TypedProgram,
    target: CompileTarget,
) -> RuntimeContractBundle {
    let mut items = Vec::new();
    let mut reflection_names = BTreeSet::new();
    collect_runtime_items(&program.items, &mut items, &mut reflection_names);
    items.sort_by(|left, right| left.id.cmp(&right.id));
    let patches = collect_patch_contracts(&program.items);
    let converges = collect_converge_contracts(&program.items);
    let worlds = collect_world_contracts(&program.items);
    let active_world = if worlds.len() == 1 {
        worlds.first().cloned()
    } else {
        None
    };
    let orchestrations = collect_orchestration_contracts(&program.items);

    let summary = summarize_items(&program.items);

    let mut required_capabilities = collect_runtime_capabilities(&summary, target);
    required_capabilities.sort_by(|left, right| left.key.cmp(&right.key));

    let mut service_bindings = runtime_service_bindings_for_target(&summary, target);
    service_bindings.sort_by(|left, right| left.service.cmp(&right.service));

    // Emit full reflection payload for LLVM and Rust targets
    let reflection_payload = if matches!(target, CompileTarget::Llvm | CompileTarget::Rust) {
        Some(emit_reflection_payload(program))
    } else {
        None
    };

    let reflection_emitted = reflection_payload.is_some();

    RuntimeContractBundle {
        schema_version: RUNTIME_CONTRACT_SCHEMA_VERSION,
        target: compile_target_name(target).to_string(),
        required_capabilities,
        service_bindings,
        items,
        patches,
        converges,
        worlds,
        active_world,
        orchestrations,
        reflection: RuntimeReflectionSummary {
            emitted: reflection_emitted,
            schema_names: reflection_names.into_iter().collect(),
            notes: if reflection_emitted {
                vec![
                    "Full reflection payload emitted from kain-core.".to_string(),
                    "Includes type schemas, item metadata, actors, components, and messages."
                        .to_string(),
                ]
            } else {
                vec![
                    "Runtime contract scaffolding emitted from kain-core.".to_string(),
                    "Reflection payloads are not emitted for this target.".to_string(),
                ]
            },
        },
        compatibility: runtime_compatibility_metadata(target, reflection_emitted),
        reflection_payload,
    }
}

pub fn runtime_contract_bundle_to_json(
    bundle: &RuntimeContractBundle,
) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(bundle)
}

fn collect_runtime_capabilities(
    summary: &ItemSummary,
    target: CompileTarget,
) -> Vec<RuntimeCapability> {
    let mut capabilities = vec![
        runtime_capability(
            "compiler.typed-program",
            "kain-core",
            Some("Typed frontend output is available for runtime packaging."),
        ),
        runtime_capability(
            "runtime.contract.bundle",
            "kain-core",
            Some("Compiler-owned runtime contract scaffolding emitted."),
        ),
    ];

    let memory_caps = backend_memory_capabilities(target);
    if memory_caps.raw_pointers {
        capabilities.push(runtime_capability(
            "memory.raw-pointers",
            "kain-core.low_level_memory",
            Some("Target accepts raw pointer lowering."),
        ));
    }
    if memory_caps.raw_memory_ops {
        capabilities.push(runtime_capability(
            "memory.raw-ops",
            "kain-core.low_level_memory",
            Some("Target accepts raw memory operation lowering."),
        ));
    }

    if summary.components > 0 {
        capabilities.push(runtime_capability(
            "ui.components",
            "kain-core",
            Some("Program contains declarative UI components."),
        ));
        capabilities.push(runtime_capability(
            "ui.runtime-bundle",
            "kain-core.ui",
            Some("Program can participate in compiled UI bundle materialization."),
        ));
    }
    if summary.actors > 0 {
        capabilities.push(runtime_capability(
            "actors.syntax",
            "kain-core",
            Some("Program declares actor items that require runtime-backed semantics."),
        ));
    }
    if summary.async_tasks > 0 {
        capabilities.push(runtime_capability(
            "async.runtime",
            "kain-core",
            Some("Program declares async task items that require the async runtime."),
        ));
        capabilities.push(runtime_capability(
            "async.timers",
            "kain-core",
            Some("Program declares async task items that require timer delivery."),
        ));
    }
    if summary.shaders > 0 || summary.material_graphs > 0 || summary.material_functions > 0 {
        capabilities.push(runtime_capability(
            "gpu.programs",
            "kain-core",
            Some("Program declares GPU or material-oriented items."),
        ));
    }
    if summary.compute_shaders > 0 {
        capabilities.push(runtime_capability(
            "gpu.compute",
            "kain-core.shader",
            Some(
                "Program declares compute shaders that should materialize as native compute plans.",
            ),
        ));
        capabilities.push(runtime_capability(
            "gpu.compute-dispatch",
            "kain-core.shader",
            Some("Compute workgroup and dispatch metadata is emitted for runtime consumption."),
        ));
        if summary.compute_plan_shaders > 0 {
            capabilities.push(runtime_capability(
                COMPUTE_PLAN_CAPABILITY_KEY,
                "kain-core.shader",
                Some(
                    "Compute shaders author explicit workgroup, dispatch, tensor, stream, and neural-node plans in shader comptime metadata.",
                ),
            ));
        }
        capabilities.push(runtime_capability(
            "neural.node-plan",
            "kain-core.shader",
            Some("Compute shaders emit experimental neural-node planning metadata for runtime orchestration."),
        ));
    }
    if summary.compute_storage_buffers > 0 {
        capabilities.push(runtime_capability(
            "interop.shared-buffer",
            "kain-core.shader",
            Some("Compute shaders use storage buffers that should flow through neutral shared-buffer contracts."),
        ));
        capabilities.push(runtime_capability(
            "data.tensor-buffer",
            "kain-core.shader",
            Some("Storage-buffer-backed compute lanes can carry tensor-oriented numeric payloads."),
        ));
        capabilities.push(runtime_capability(
            "data.continuous-stream",
            "kain-core.shader",
            Some("Compute shaders participate in continuous stream processing over GPU-visible buffers."),
        ));
    }
    if summary.editor_modules > 0 || summary.graph_editors > 0 || summary.graph_runtimes > 0 {
        capabilities.push(runtime_capability(
            "tooling.editor-surfaces",
            "kain-core",
            Some("Program declares editor or graph tooling surfaces."),
        ));
    }
    if summary.patches > 0 {
        capabilities.push(runtime_capability(
            "patch.transactions",
            "kain-core.runtime",
            Some("Program declares compiler-owned transactional patch semantics."),
        ));
    }
    if summary.converges > 0 {
        capabilities.push(runtime_capability(
            "converge.dispatch",
            "kain-core.runtime",
            Some("Program declares multi-lane converge dispatch semantics."),
        ));
    }
    if summary.orchestrations > 0 {
        capabilities.push(runtime_capability(
            "orchestrate.pipeline",
            "kain-core.runtime",
            Some("Program declares typed polyglot orchestration stages."),
        ));
    }
    if summary.world_native_ui > 0 {
        capabilities.push(runtime_capability(
            "world.native-ui",
            "kain-core.runtime_contract",
            Some("Program declares compiler-owned native UI world projections."),
        ));
    }
    if summary.world_viewport3d > 0 {
        capabilities.push(runtime_capability(
            "world.viewport3d",
            "kain-core.runtime_contract",
            Some("Program declares compiler-owned viewport3d world projections."),
        ));
    }
    if summary.world_web > 0 {
        capabilities.push(runtime_capability(
            "world.web",
            "kain-core.runtime_contract",
            Some("Program declares compiler-owned web world projections."),
        ));
    }
    if summary.world_ue5 > 0 {
        capabilities.push(runtime_capability(
            "world.ue5",
            "kain-core.runtime_contract",
            Some("Program declares compiler-owned UE5 world projections."),
        ));
    }

    match target {
        CompileTarget::Rust => {
            capabilities.push(runtime_capability(
                "driver.native-app-bundle",
                "kain-driver",
                Some("Rust-hosted native app materialization is available."),
            ));
        }
        CompileTarget::Llvm => {
            capabilities.push(runtime_capability(
                "native.raw-runtime",
                "runtime/native",
                Some("Program targets the raw native runtime lane."),
            ));
            capabilities.push(runtime_capability(
                "native.viewport-host",
                "runtime/native",
                Some("Viewport/app-host runtime services are expected in the native lane."),
            ));
        }
        _ => {}
    }

    capabilities
}

fn runtime_service_bindings_for_target(
    summary: &ItemSummary,
    target: CompileTarget,
) -> Vec<RuntimeServiceBinding> {
    let mut bindings = match target {
        CompileTarget::Rust => vec![
            runtime_service_binding("driver.bundle", "kain-driver", "rust-native"),
            runtime_service_binding("ui.runtime-bundle", "kain-ui", "rust-native"),
            runtime_service_binding("host.ui-native", "kain-ui-native", "rust-native"),
        ],
        CompileTarget::Llvm => vec![
            runtime_service_binding("platform.app-host", "runtime/native", "raw-native"),
            runtime_service_binding("platform.input", "runtime/native", "raw-native"),
            runtime_service_binding("gfx.viewport", "runtime/native", "raw-native"),
            runtime_service_binding("asset.gltf", "runtime/native", "raw-native"),
            runtime_service_binding("ui.bundle", "runtime/native", "raw-native"),
        ],
        CompileTarget::Js | CompileTarget::Ts | CompileTarget::Wasm | CompileTarget::Hybrid => {
            vec![runtime_service_binding("host.web", "web", "web")]
        }
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => {
            vec![runtime_service_binding("host.ue5", "ue5", "ue5")]
        }
        _ => Vec::new(),
    };

    if summary.async_tasks > 0 {
        match target {
            CompileTarget::Rust => {
                bindings.push(runtime_service_binding(
                    "async.runtime",
                    "kain-core",
                    "rust-native",
                ));
                bindings.push(runtime_service_binding(
                    "async.timers",
                    "kain-core",
                    "rust-native",
                ));
            }
            CompileTarget::Llvm => {
                bindings.push(runtime_service_binding(
                    "async.runtime",
                    "runtime/native",
                    "raw-native",
                ));
                bindings.push(runtime_service_binding(
                    "async.timers",
                    "runtime/native",
                    "raw-native",
                ));
            }
            _ => {}
        }
    }

    if summary.compute_shaders > 0 && matches!(target, CompileTarget::Llvm) {
        bindings.push(runtime_service_binding(
            "gfx.compute",
            "runtime/native",
            "raw-native",
        ));
    }
    if summary.patches > 0 {
        bindings.push(runtime_service_binding(
            "patch.transactions",
            "kain-core",
            runtime_lane_name(target),
        ));
    }
    if summary.converges > 0 {
        bindings.push(runtime_service_binding(
            "converge.dispatch",
            "kain-core",
            runtime_lane_name(target),
        ));
    }
    if summary.orchestrations > 0 {
        bindings.push(runtime_service_binding(
            "orchestrate.pipeline",
            "kain-core",
            runtime_lane_name(target),
        ));
    }
    if summary.world_native_ui > 0 && matches!(target, CompileTarget::Rust | CompileTarget::Llvm) {
        bindings.push(runtime_service_binding(
            "world.native-ui",
            if matches!(target, CompileTarget::Rust) {
                "kain-ui"
            } else {
                "runtime/native"
            },
            runtime_lane_name(target),
        ));
    }
    if summary.world_viewport3d > 0 && matches!(target, CompileTarget::Rust | CompileTarget::Llvm)
    {
        bindings.push(runtime_service_binding(
            "world.viewport3d",
            if matches!(target, CompileTarget::Rust) {
                "kain-ui-native"
            } else {
                "runtime/native"
            },
            runtime_lane_name(target),
        ));
    }
    if summary.world_web > 0
        && matches!(
            target,
            CompileTarget::Js | CompileTarget::Ts | CompileTarget::Wasm | CompileTarget::Hybrid
        )
    {
        bindings.push(runtime_service_binding("world.web", "web", runtime_lane_name(target)));
    }
    if summary.world_ue5 > 0 && matches!(target, CompileTarget::Ue5 | CompileTarget::Ue5Editor) {
        bindings.push(runtime_service_binding("world.ue5", "ue5", runtime_lane_name(target)));
    }

    bindings
}

fn runtime_capability(key: &str, source: &str, detail: Option<&str>) -> RuntimeCapability {
    RuntimeCapability {
        key: key.to_string(),
        source: source.to_string(),
        detail: detail.map(|value| value.to_string()),
    }
}

fn runtime_service_binding(service: &str, provider: &str, lane: &str) -> RuntimeServiceBinding {
    RuntimeServiceBinding {
        service: service.to_string(),
        provider: provider.to_string(),
        lane: lane.to_string(),
    }
}

fn collect_runtime_items(
    items: &[TypedItem],
    output: &mut Vec<RuntimeContractItem>,
    reflection_names: &mut BTreeSet<String>,
) {
    for item in items {
        match item {
            TypedItem::Function(function) => {
                output.push(runtime_contract_item("function", &function.ast.name));
            }
            TypedItem::Patch(patch) => {
                output.push(runtime_contract_item("patch", &patch.ast.name));
            }
            TypedItem::Converge(converge) => {
                output.push(runtime_contract_item("converge", &converge.ast.name));
            }
            TypedItem::World(world) => {
                output.push(runtime_contract_item("world", &world.ast.name));
                reflection_names.insert(world.ast.name.clone());
            }
            TypedItem::Orchestrate(orchestrate) => {
                output.push(runtime_contract_item("orchestrate", &orchestrate.ast.name));
            }
            TypedItem::Component(component) => {
                output.push(runtime_contract_item("component", &component.ast.name));
                reflection_names.insert(component.ast.name.clone());
            }
            TypedItem::Shader(shader) => {
                output.push(runtime_contract_item("shader", &shader.ast.name));
            }
            TypedItem::Actor(actor) => {
                output.push(runtime_contract_item("actor", &actor.ast.name));
                reflection_names.insert(actor.ast.name.clone());
            }
            TypedItem::Struct(struct_def) => {
                output.push(runtime_contract_item("struct", &struct_def.ast.name));
                reflection_names.insert(struct_def.ast.name.clone());
            }
            TypedItem::Enum(enum_def) => {
                output.push(runtime_contract_item("enum", &enum_def.ast.name));
                reflection_names.insert(enum_def.ast.name.clone());
            }
            TypedItem::Trait(trait_def) => {
                output.push(runtime_contract_item("trait", &trait_def.ast.name));
            }
            TypedItem::Const(const_def) => {
                output.push(runtime_contract_item("const", &const_def.ast.name));
            }
            TypedItem::Macro(macro_def) => {
                output.push(runtime_contract_item("macro", &macro_def.ast.name));
            }
            TypedItem::Use(use_def) => {
                output.push(runtime_contract_item("use", &use_def.ast.path.join("::")));
            }
            TypedItem::Mod(module) => {
                output.push(runtime_contract_item("mod", &module.ast.name));
                collect_runtime_items(&module.items, output, reflection_names);
            }
            TypedItem::Impl(impl_def) => {
                output.push(runtime_contract_item(
                    "impl",
                    &impl_target_name(&impl_def.ast.target_type),
                ));
            }
            TypedItem::Test(test_def) => {
                output.push(runtime_contract_item("test", &test_def.ast.name));
            }
            TypedItem::TypeAlias(type_alias) => {
                output.push(runtime_contract_item("type-alias", &type_alias.ast.name));
                reflection_names.insert(type_alias.ast.name.clone());
            }
            TypedItem::MaterialGraph(graph) => {
                output.push(runtime_contract_item("material-graph", &graph.name));
            }
            TypedItem::MaterialFunction(function) => {
                output.push(runtime_contract_item("material-function", &function.name));
            }
            TypedItem::GraphEditor(editor) => {
                output.push(runtime_contract_item("graph-editor", &editor.name));
            }
            TypedItem::GraphRuntime(runtime) => {
                output.push(runtime_contract_item("graph-runtime", &runtime.name));
            }
            TypedItem::StateMachine(state_machine) => {
                output.push(runtime_contract_item("state-machine", &state_machine.name));
            }
            TypedItem::AsyncTask(task) => {
                output.push(runtime_contract_item("async-task", &task.name));
            }
            TypedItem::EditorModule(module) => {
                output.push(runtime_contract_item("editor-module", &module.name));
            }
            TypedItem::GameplayTags(namespace) => {
                output.push(runtime_contract_item("gameplay-tags", &namespace.name));
            }
            TypedItem::GameplayAbility(ability) => {
                output.push(runtime_contract_item("gameplay-ability", &ability.name));
            }
            TypedItem::GameplayEffect(effect) => {
                output.push(runtime_contract_item("gameplay-effect", &effect.name));
            }
            TypedItem::GameplayCue(cue) => {
                output.push(runtime_contract_item("gameplay-cue", &cue.name));
            }
            TypedItem::Comptime(_) => {
                output.push(runtime_contract_item("comptime", "<comptime>"));
            }
        }
    }
}

fn runtime_contract_item(kind: &str, name: &str) -> RuntimeContractItem {
    RuntimeContractItem {
        id: format!("{kind}:{name}"),
        name: name.to_string(),
        kind: kind.to_string(),
    }
}

fn collect_patch_contracts(items: &[TypedItem]) -> Vec<RuntimePatchContract> {
    let mut contracts = Vec::new();
    collect_patch_contracts_into(items, &mut contracts);
    contracts.sort_by(|left, right| left.name.cmp(&right.name));
    contracts
}

fn collect_patch_contracts_into(items: &[TypedItem], output: &mut Vec<RuntimePatchContract>) {
    for item in items {
        match item {
            TypedItem::Patch(patch) => output.push(runtime_patch_contract(patch)),
            TypedItem::Mod(module) => collect_patch_contracts_into(&module.items, output),
            _ => {}
        }
    }
}

fn runtime_patch_contract(patch: &TypedPatch) -> RuntimePatchContract {
    let mutation_paths = patch.mutation_paths.clone();
    RuntimePatchContract {
        name: patch.ast.name.clone(),
        replay_log_schema: mutation_paths
            .iter()
            .map(|path| format!("set:{path}"))
            .collect(),
        invalidation_keys: patch_invalidation_keys(&mutation_paths),
        collaboration_event: format!("patch.{}", patch.ast.name),
        undo_mode: match patch.undo_mode {
            PatchUndoMode::Reversible => "reversible".to_string(),
            PatchUndoMode::BestEffort => "best_effort".to_string(),
        },
        mutation_paths,
    }
}

fn patch_invalidation_keys(mutation_paths: &[String]) -> Vec<String> {
    let mut keys = mutation_paths
        .iter()
        .map(|path| {
            let cutoff = path
                .find(|character| character == '.' || character == '[')
                .unwrap_or(path.len());
            path[..cutoff].to_string()
        })
        .filter(|entry| !entry.is_empty())
        .collect::<Vec<_>>();
    keys.sort();
    keys.dedup();
    keys
}

fn collect_converge_contracts(items: &[TypedItem]) -> Vec<RuntimeConvergeContract> {
    let mut contracts = Vec::new();
    collect_converge_contracts_into(items, &mut contracts);
    contracts.sort_by(|left, right| left.name.cmp(&right.name));
    contracts
}

fn collect_converge_contracts_into(items: &[TypedItem], output: &mut Vec<RuntimeConvergeContract>) {
    for item in items {
        match item {
            TypedItem::Converge(converge) => output.push(runtime_converge_contract(converge)),
            TypedItem::Mod(module) => collect_converge_contracts_into(&module.items, output),
            _ => {}
        }
    }
}

fn runtime_converge_contract(converge: &TypedConverge) -> RuntimeConvergeContract {
    let fast_lanes = converge
        .ast
        .fast_lanes
        .iter()
        .map(|lane| runtime_converge_lane_contract(&converge.ast.name, lane))
        .collect::<Vec<_>>();
    RuntimeConvergeContract {
        name: converge.ast.name.clone(),
        dispatcher_symbol: converge.ast.name.clone(),
        spec_lane: runtime_converge_lane_contract(&converge.ast.name, &converge.ast.spec_lane),
        fast_lanes,
        verify_random_count: converge.ast.verify_random_count,
    }
}

fn runtime_converge_lane_contract(
    converge_name: &str,
    lane: &crate::ast::ConvergeLane,
) -> RuntimeConvergeLaneContract {
    let (selector_kind, selector_value) = match &lane.selector {
        Some(ConvergeSelector::Target(value)) => {
            (Some("target".to_string()), Some(value.clone()))
        }
        Some(ConvergeSelector::Capability(value)) => {
            (Some("capability".to_string()), Some(value.clone()))
        }
        None => (None, None),
    };
    RuntimeConvergeLaneContract {
        lane_name: lane.lane_name.clone(),
        kind: match lane.kind {
            crate::ast::ConvergeLaneKind::Spec => "spec".to_string(),
            crate::ast::ConvergeLaneKind::Fast => "fast".to_string(),
        },
        selector_kind,
        selector_value,
        symbol: format!(
            "{}__{}",
            converge_name,
            sanitize_contract_ident(&lane.lane_name)
        ),
    }
}

fn collect_world_contracts(items: &[TypedItem]) -> Vec<RuntimeWorldContract> {
    let mut contracts = Vec::new();
    collect_world_contracts_into(items, &mut contracts);
    contracts.sort_by(|left, right| left.name.cmp(&right.name));
    contracts
}

fn collect_world_contracts_into(items: &[TypedItem], output: &mut Vec<RuntimeWorldContract>) {
    for item in items {
        match item {
            TypedItem::World(world) => output.push(runtime_world_contract(world)),
            TypedItem::Mod(module) => collect_world_contracts_into(&module.items, output),
            _ => {}
        }
    }
}

fn runtime_world_contract(world: &TypedWorld) -> RuntimeWorldContract {
    let mut surfaces = world
        .ast
        .surfaces
        .iter()
        .map(|surface| RuntimeWorldSurfaceContract {
            kind: surface.kind.as_str().to_string(),
            authored_expr: render_authored_expr_contract(&surface.expr),
        })
        .collect::<Vec<_>>();
    surfaces.sort_by(|left, right| left.kind.cmp(&right.kind));
    RuntimeWorldContract {
        name: world.ast.name.clone(),
        state_slots: world
            .ast
            .states
            .iter()
            .map(|state| RuntimeWorldStateContract {
                name: state.name.clone(),
                type_name: type_to_string(&state.ty),
                initial_expr: render_authored_expr_contract(&state.initial),
            })
            .collect(),
        surfaces,
    }
}

fn collect_orchestration_contracts(items: &[TypedItem]) -> Vec<RuntimeOrchestrationContract> {
    let mut contracts = Vec::new();
    collect_orchestration_contracts_into(items, &mut contracts);
    contracts.sort_by(|left, right| left.name.cmp(&right.name));
    contracts
}

fn collect_orchestration_contracts_into(
    items: &[TypedItem],
    output: &mut Vec<RuntimeOrchestrationContract>,
) {
    for item in items {
        match item {
            TypedItem::Orchestrate(orchestrate) => {
                output.push(runtime_orchestration_contract(orchestrate));
            }
            TypedItem::Mod(module) => collect_orchestration_contracts_into(&module.items, output),
            _ => {}
        }
    }
}

fn runtime_orchestration_contract(orchestrate: &TypedOrchestrate) -> RuntimeOrchestrationContract {
    RuntimeOrchestrationContract {
        name: orchestrate.ast.name.clone(),
        return_type: orchestrate
            .ast
            .return_type
            .as_ref()
            .map(type_to_string),
        stages: orchestrate
            .stages
            .iter()
            .map(|stage| RuntimeOrchestrationStageContract {
                runtime: stage.runtime.as_str().to_string(),
                function: stage.function.clone(),
                binding_name: stage.binding_name.clone(),
            })
            .collect(),
    }
}

fn sanitize_contract_ident(name: &str) -> String {
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

fn impl_target_name(ty: &crate::ast::Type) -> String {
    match ty {
        crate::ast::Type::Named { name, .. } => name.clone(),
        crate::ast::Type::Tuple(_, _) => "tuple".to_string(),
        crate::ast::Type::Array(_, _, _) => "array".to_string(),
        crate::ast::Type::Slice(_, _) => "slice".to_string(),
        crate::ast::Type::Ref { inner, .. } => format!("ref<{}>", impl_target_name(inner)),
        crate::ast::Type::Ptr { inner, .. } => format!("ptr<{}>", impl_target_name(inner)),
        crate::ast::Type::Option(inner, _) => format!("option<{}>", impl_target_name(inner)),
        crate::ast::Type::Result(ok, _, _) => format!("result<{}>", impl_target_name(ok)),
        crate::ast::Type::Unit(_) => "Unit".to_string(),
        crate::ast::Type::Never(_) => "Never".to_string(),
        _ => "type".to_string(),
    }
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

fn runtime_lane_name(target: CompileTarget) -> &'static str {
    match target {
        CompileTarget::Rust => "rust-native",
        CompileTarget::Llvm => "raw-native",
        _ => compile_target_name(target),
    }
}

fn runtime_compatibility_metadata(
    target: CompileTarget,
    reflection_emitted: bool,
) -> RuntimeCompatibilityMetadata {
    RuntimeCompatibilityMetadata {
        schema_version: RUNTIME_CONTRACT_SCHEMA_VERSION,
        bundle_target: compile_target_name(target).to_string(),
        bundle_lane: runtime_lane_name(target).to_string(),
        runtime_lane: None,
        compatibility_class: None,
        platform_availability: None,
        runtime_version: None,
        abi_version: None,
        install: RuntimeLifecyclePolicy {
            supported: true,
            mode: "materialize".to_string(),
            notes: vec![
                "Bundle sidecars are emitted with the runtime contract.".to_string(),
                "Install can proceed once version and compatibility metadata are present."
                    .to_string(),
            ],
        },
        update: RuntimeLifecyclePolicy {
            supported: true,
            mode: "compatible-replace".to_string(),
            notes: vec![
                "Update is expected to replace a bundle only after compatibility validation passes."
                    .to_string(),
                "Migration data is still optional at the compiler layer.".to_string(),
            ],
        },
        uninstall: RuntimeLifecyclePolicy {
            supported: true,
            mode: "sidecar-remove".to_string(),
            notes: vec![
                "Uninstall removes emitted bundle artifacts together with compatibility metadata."
                    .to_string(),
            ],
        },
        migration_hints: if reflection_emitted {
            vec![
                "Preserve the runtime contract and reflection payload together when updating."
                    .to_string(),
                "Validate bundle and runtime version records before activating a replacement."
                    .to_string(),
            ]
        } else {
            vec![
                "Populate a reflection payload before relying on live migration flows.".to_string(),
                "Carry compatibility metadata forward even when reflection is not emitted."
                    .to_string(),
            ]
        },
    }
}

#[derive(Default)]
struct ItemSummary {
    components: usize,
    actors: usize,
    async_tasks: usize,
    patches: usize,
    converges: usize,
    worlds: usize,
    orchestrations: usize,
    world_native_ui: usize,
    world_viewport3d: usize,
    world_web: usize,
    world_ue5: usize,
    shaders: usize,
    compute_shaders: usize,
    compute_plan_shaders: usize,
    compute_storage_buffers: usize,
    material_graphs: usize,
    material_functions: usize,
    graph_editors: usize,
    graph_runtimes: usize,
    editor_modules: usize,
}

fn summarize_items(items: &[TypedItem]) -> ItemSummary {
    let mut summary = ItemSummary::default();
    summarize_items_into(items, &mut summary);
    summary
}

fn summarize_items_into(items: &[TypedItem], summary: &mut ItemSummary) {
    for item in items {
        match item {
            TypedItem::Component(_) => summary.components += 1,
            TypedItem::Actor(_) => summary.actors += 1,
            TypedItem::AsyncTask(_) => summary.async_tasks += 1,
            TypedItem::Patch(_) => summary.patches += 1,
            TypedItem::Converge(_) => summary.converges += 1,
            TypedItem::World(world) => {
                summary.worlds += 1;
                for surface in &world.ast.surfaces {
                    match surface.kind {
                        WorldSurfaceKind::NativeUi => summary.world_native_ui += 1,
                        WorldSurfaceKind::Viewport3d => summary.world_viewport3d += 1,
                        WorldSurfaceKind::Web => summary.world_web += 1,
                        WorldSurfaceKind::Ue5 => summary.world_ue5 += 1,
                    }
                }
            }
            TypedItem::Orchestrate(_) => summary.orchestrations += 1,
            TypedItem::Shader(shader) => {
                summary.shaders += 1;
                if matches!(shader.ast.stage, ShaderStage::Compute) {
                    summary.compute_shaders += 1;
                    if shader
                        .ast
                        .explicit_compute_metadata()
                        .ok()
                        .flatten()
                        .is_some()
                    {
                        summary.compute_plan_shaders += 1;
                    }
                    summary.compute_storage_buffers += shader
                        .ast
                        .uniforms
                        .iter()
                        .filter(|uniform| is_storage_buffer_type(&uniform.ty))
                        .count();
                }
            }
            TypedItem::MaterialGraph(_) => summary.material_graphs += 1,
            TypedItem::MaterialFunction(_) => summary.material_functions += 1,
            TypedItem::GraphEditor(_) => summary.graph_editors += 1,
            TypedItem::GraphRuntime(_) => summary.graph_runtimes += 1,
            TypedItem::EditorModule(_) => summary.editor_modules += 1,
            TypedItem::Mod(module) => summarize_items_into(&module.items, summary),
            _ => {}
        }
    }
}

fn is_storage_buffer_type(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "StorageBuffer")
}

fn emit_reflection_payload(program: &TypedProgram) -> RuntimeReflectionPayload {
    let mut types = Vec::new();
    let mut items = Vec::new();
    let mut actors = Vec::new();
    let mut components = Vec::new();
    let mut messages = Vec::new();
    let mut type_id_counter = 1u64;
    let mut item_id_counter = 1u64;

    collect_reflection_data(
        &program.items,
        "",
        &mut types,
        &mut items,
        &mut actors,
        &mut components,
        &mut messages,
        &mut type_id_counter,
        &mut item_id_counter,
    );

    RuntimeReflectionPayload {
        schema_version: RUNTIME_CONTRACT_SCHEMA_VERSION,
        types,
        items,
        actors,
        components,
        messages,
    }
}

fn collect_reflection_data(
    typed_items: &[TypedItem],
    module_path: &str,
    types: &mut Vec<ReflectedType>,
    items: &mut Vec<ReflectedItem>,
    actors: &mut Vec<ReflectedActor>,
    components: &mut Vec<ReflectedComponent>,
    messages: &mut Vec<ReflectedMessage>,
    type_id_counter: &mut u64,
    item_id_counter: &mut u64,
) {
    for item in typed_items {
        match item {
            TypedItem::Struct(struct_def) => {
                let type_id = *type_id_counter;
                *type_id_counter += 1;
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                let fields = struct_def
                    .ast
                    .fields
                    .iter()
                    .map(|f| ReflectedField {
                        name: f.name.clone(),
                        type_name: type_to_string(&f.ty),
                        offset_hint: None,
                    })
                    .collect();

                types.push(ReflectedType {
                    type_id,
                    name: struct_def.ast.name.clone(),
                    kind: "struct".to_string(),
                    size_hint: None,
                    fields,
                });

                items.push(ReflectedItem {
                    item_id,
                    name: struct_def.ast.name.clone(),
                    kind: "struct".to_string(),
                    module_path: module_path.to_string(),
                    type_id: Some(type_id),
                });
            }
            TypedItem::Enum(enum_def) => {
                let type_id = *type_id_counter;
                *type_id_counter += 1;
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                types.push(ReflectedType {
                    type_id,
                    name: enum_def.ast.name.clone(),
                    kind: "enum".to_string(),
                    size_hint: None,
                    fields: Vec::new(),
                });

                items.push(ReflectedItem {
                    item_id,
                    name: enum_def.ast.name.clone(),
                    kind: "enum".to_string(),
                    module_path: module_path.to_string(),
                    type_id: Some(type_id),
                });
            }
            TypedItem::World(world) => {
                let type_id = *type_id_counter;
                *type_id_counter += 1;
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                let fields = world
                    .ast
                    .states
                    .iter()
                    .map(|state| ReflectedField {
                        name: state.name.clone(),
                        type_name: type_to_string(&state.ty),
                        offset_hint: None,
                    })
                    .collect();

                types.push(ReflectedType {
                    type_id,
                    name: world.ast.name.clone(),
                    kind: "world".to_string(),
                    size_hint: None,
                    fields,
                });

                items.push(ReflectedItem {
                    item_id,
                    name: world.ast.name.clone(),
                    kind: "world".to_string(),
                    module_path: module_path.to_string(),
                    type_id: Some(type_id),
                });
            }
            TypedItem::Actor(actor) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: actor.ast.name.clone(),
                    kind: "actor".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });

                actors.push(ReflectedActor {
                    item_id,
                    name: actor.ast.name.clone(),
                    message_types: Vec::new(),
                    state_type: None,
                });
            }
            TypedItem::Component(component) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                let props = component
                    .ast
                    .props
                    .iter()
                    .map(|p| ReflectedField {
                        name: p.name.clone(),
                        type_name: type_to_string(&p.ty),
                        offset_hint: None,
                    })
                    .collect();

                items.push(ReflectedItem {
                    item_id,
                    name: component.ast.name.clone(),
                    kind: "component".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });

                components.push(ReflectedComponent {
                    item_id,
                    name: component.ast.name.clone(),
                    props,
                    state_type: None,
                });
            }
            TypedItem::Function(function) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: function.ast.name.clone(),
                    kind: "function".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });
            }
            TypedItem::Patch(patch) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: patch.ast.name.clone(),
                    kind: "patch".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });
            }
            TypedItem::Converge(converge) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: converge.ast.name.clone(),
                    kind: "converge".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });
            }
            TypedItem::Orchestrate(orchestrate) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: orchestrate.ast.name.clone(),
                    kind: "orchestrate".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });
            }
            TypedItem::Mod(module) => {
                let item_id = *item_id_counter;
                *item_id_counter += 1;

                items.push(ReflectedItem {
                    item_id,
                    name: module.ast.name.clone(),
                    kind: "mod".to_string(),
                    module_path: module_path.to_string(),
                    type_id: None,
                });

                let nested_path = if module_path.is_empty() {
                    module.ast.name.clone()
                } else {
                    format!("{}::{}", module_path, module.ast.name)
                };

                collect_reflection_data(
                    &module.items,
                    &nested_path,
                    types,
                    items,
                    actors,
                    components,
                    messages,
                    type_id_counter,
                    item_id_counter,
                );
            }
            _ => {}
        }
    }
}

fn type_to_string(ty: &crate::ast::Type) -> String {
    match ty {
        crate::ast::Type::Named { name, .. } => name.clone(),
        crate::ast::Type::Tuple(_, _) => "Tuple".to_string(),
        crate::ast::Type::Array(_, _, _) => "Array".to_string(),
        crate::ast::Type::Slice(_, _) => "Slice".to_string(),
        crate::ast::Type::Ref { inner, .. } => format!("&{}", type_to_string(inner)),
        crate::ast::Type::Ptr { inner, .. } => format!("*{}", type_to_string(inner)),
        crate::ast::Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        crate::ast::Type::Result(ok, _, _) => format!("Result<{}>", type_to_string(ok)),
        crate::ast::Type::Unit(_) => "Unit".to_string(),
        crate::ast::Type::Never(_) => "Never".to_string(),
        _ => "Unknown".to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ast::AsyncTaskDef;
    use crate::diagnostics::SpanMapper;
    use crate::{types, Lexer, Parser, Span};

    #[test]
    fn emits_service_bindings_for_rust_lane() {
        let bundle =
            emit_runtime_contract_bundle(&TypedProgram { items: Vec::new() }, CompileTarget::Rust);
        assert_eq!(bundle.target, "rust");
        assert!(bundle
            .service_bindings
            .iter()
            .any(|binding| binding.service == "driver.bundle"));
    }

    #[test]
    fn emits_component_capability_for_ui_source() {
        let source = r#"
component App():
    render <panel title="Studio" />
"#;
        let tokens = Lexer::new(source).tokenize().expect("tokens");
        let span_mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("parse");
        let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

        let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "ui.runtime-bundle"));
        assert!(bundle.items.iter().any(|item| item.id == "component:App"));

        // Check reflection payload is emitted for Rust target
        assert!(bundle.reflection_payload.is_some());
        let payload = bundle.reflection_payload.as_ref().unwrap();
        assert_eq!(payload.components.len(), 1);
        assert_eq!(payload.components[0].name, "App");
        assert_eq!(
            bundle.compatibility.schema_version,
            RUNTIME_CONTRACT_SCHEMA_VERSION
        );
        assert_eq!(bundle.compatibility.bundle_target, "rust");
        assert_eq!(bundle.compatibility.bundle_lane, "rust-native");
        assert!(bundle.compatibility.install.supported);
        assert_eq!(bundle.compatibility.update.mode, "compatible-replace");
        assert!(bundle
            .compatibility
            .migration_hints
            .iter()
            .any(|hint| hint.contains("reflection payload")));
    }

    #[test]
    fn emits_async_requirements_for_async_task_items() {
        let bundle = emit_runtime_contract_bundle(
            &TypedProgram {
                items: vec![TypedItem::AsyncTask(AsyncTaskDef {
                    name: "LoadAssets".to_string(),
                    input_fields: Vec::new(),
                    output_fields: Vec::new(),
                    callback: None,
                    do_work: None,
                    priority: Some(1),
                    attributes: Vec::new(),
                    span: Span::default(),
                })],
            },
            CompileTarget::Rust,
        );

        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "async.runtime"));
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "async.timers"));
        assert!(bundle
            .service_bindings
            .iter()
            .any(|binding| binding.service == "async.runtime"));
        assert!(bundle
            .service_bindings
            .iter()
            .any(|binding| binding.service == "async.timers"));
        assert!(bundle
            .items
            .iter()
            .any(|item| item.id == "async-task:LoadAssets"));

        let json = runtime_contract_bundle_to_json(&bundle).expect("runtime contract json");
        assert!(json.contains("\"async.runtime\""));
        assert!(json.contains("\"async.timers\""));
        assert!(json.contains("\"async-task:LoadAssets\""));
    }

    #[test]
    fn emits_compute_runtime_requirements_for_raw_native_lane() {
        let compute_shader = crate::types::TypedShader {
            ast: crate::ast::Shader {
                name: "TensorPass".to_string(),
                stage: ShaderStage::Compute,
                inputs: Vec::new(),
                outputs: crate::ast::Type::Unit(Span::default()),
                uniforms: vec![
                    crate::ast::Uniform {
                        name: "src".to_string(),
                        ty: crate::ast::Type::Named {
                            name: "StorageBuffer".to_string(),
                            generics: vec![crate::ast::Type::Named {
                                name: "Float".to_string(),
                                generics: Vec::new(),
                                span: Span::default(),
                            }],
                            span: Span::default(),
                        },
                        binding: 0,
                        span: Span::default(),
                    },
                    crate::ast::Uniform {
                        name: "dst".to_string(),
                        ty: crate::ast::Type::Named {
                            name: "StorageBuffer".to_string(),
                            generics: vec![crate::ast::Type::Named {
                                name: "Float".to_string(),
                                generics: Vec::new(),
                                span: Span::default(),
                            }],
                            span: Span::default(),
                        },
                        binding: 1,
                        span: Span::default(),
                    },
                ],
                body: crate::ast::Block {
                    stmts: Vec::new(),
                    span: Span::default(),
                },
                span: Span::default(),
            },
            input_types: Vec::new(),
            output_type: crate::types::ResolvedType::Unit,
        };

        let bundle = emit_runtime_contract_bundle(
            &TypedProgram {
                items: vec![TypedItem::Shader(compute_shader)],
            },
            CompileTarget::Llvm,
        );

        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "gpu.compute"));
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "interop.shared-buffer"));
        assert!(bundle
            .service_bindings
            .iter()
            .any(|binding| binding.service == "gfx.compute"));
    }

    #[test]
    fn emits_compute_plan_capability_for_explicit_metadata() {
        let source = r#"
shader compute TensorBlend() -> Void:
    comptime:
        let compute = (
            [16, 8, 1],
            [
                ("src", "f32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "f32", ["dispatch.x"], "output", "kain.shared.buffer"),
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
        let span_mapper = SpanMapper::new(source);
        let ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .expect("parse");
        let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

        let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Llvm);
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == COMPUTE_PLAN_CAPABILITY_KEY));
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "gpu.compute-dispatch"));
    }
}
