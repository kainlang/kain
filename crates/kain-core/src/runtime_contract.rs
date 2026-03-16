use std::collections::BTreeSet;

use serde::{Deserialize, Serialize};

use crate::low_level_memory::backend_memory_capabilities;
use crate::{CompileTarget, TypedItem, TypedProgram};

pub const RUNTIME_CONTRACT_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeContractBundle {
    pub schema_version: u32,
    pub target: String,
    pub required_capabilities: Vec<RuntimeCapability>,
    pub service_bindings: Vec<RuntimeServiceBinding>,
    pub items: Vec<RuntimeContractItem>,
    pub reflection: RuntimeReflectionSummary,
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
pub struct RuntimeContractItem {
    pub id: String,
    pub name: String,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeReflectionSummary {
    pub emitted: bool,
    pub schema_names: Vec<String>,
    pub notes: Vec<String>,
}

pub fn emit_runtime_contract_bundle(
    program: &TypedProgram,
    target: CompileTarget,
) -> RuntimeContractBundle {
    let mut items = Vec::new();
    let mut reflection_names = BTreeSet::new();
    collect_runtime_items(&program.items, &mut items, &mut reflection_names);
    items.sort_by(|left, right| left.id.cmp(&right.id));

    let mut required_capabilities = collect_runtime_capabilities(program, target);
    required_capabilities.sort_by(|left, right| left.key.cmp(&right.key));

    let mut service_bindings = runtime_service_bindings_for_target(target);
    service_bindings.sort_by(|left, right| left.service.cmp(&right.service));

    RuntimeContractBundle {
        schema_version: RUNTIME_CONTRACT_SCHEMA_VERSION,
        target: compile_target_name(target).to_string(),
        required_capabilities,
        service_bindings,
        items,
        reflection: RuntimeReflectionSummary {
            emitted: false,
            schema_names: reflection_names.into_iter().collect(),
            notes: vec![
                "Runtime contract scaffolding emitted from kain-core.".to_string(),
                "Reflection payloads are not emitted yet; schema_names are placeholders for future reflection integration."
                    .to_string(),
            ],
        },
    }
}

pub fn runtime_contract_bundle_to_json(
    bundle: &RuntimeContractBundle,
) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(bundle)
}

fn collect_runtime_capabilities(
    program: &TypedProgram,
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

    let summary = summarize_items(&program.items);
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
    if summary.shaders > 0 || summary.material_graphs > 0 || summary.material_functions > 0 {
        capabilities.push(runtime_capability(
            "gpu.programs",
            "kain-core",
            Some("Program declares GPU or material-oriented items."),
        ));
    }
    if summary.editor_modules > 0 || summary.graph_editors > 0 || summary.graph_runtimes > 0 {
        capabilities.push(runtime_capability(
            "tooling.editor-surfaces",
            "kain-core",
            Some("Program declares editor or graph tooling surfaces."),
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

fn runtime_service_bindings_for_target(target: CompileTarget) -> Vec<RuntimeServiceBinding> {
    match target {
        CompileTarget::Rust => vec![
            runtime_service_binding("driver.bundle", "kain-driver", "rust-native"),
            runtime_service_binding("ui.runtime-bundle", "kain-ui", "rust-native"),
            runtime_service_binding("host.ui-native", "kain-ui-native", "rust-native"),
        ],
        CompileTarget::Llvm => vec![
            runtime_service_binding("native.app-host", "runtime/native", "raw-native"),
            runtime_service_binding("native.input", "runtime/native", "raw-native"),
            runtime_service_binding("native.viewport", "runtime/native", "raw-native"),
            runtime_service_binding("native.asset.gltf", "runtime/native", "raw-native"),
            runtime_service_binding("native.ui.compiled-bundle", "runtime/native", "raw-native"),
        ],
        CompileTarget::Js | CompileTarget::Ts | CompileTarget::Wasm | CompileTarget::Hybrid => {
            vec![runtime_service_binding("host.web", "web", "web")]
        }
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => {
            vec![runtime_service_binding("host.ue5", "ue5", "ue5")]
        }
        _ => Vec::new(),
    }
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

#[derive(Default)]
struct ItemSummary {
    components: usize,
    actors: usize,
    shaders: usize,
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
            TypedItem::Shader(_) => summary.shaders += 1,
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::diagnostics::SpanMapper;
    use crate::{types, Lexer, Parser};

    #[test]
    fn emits_service_bindings_for_rust_lane() {
        let bundle = emit_runtime_contract_bundle(&TypedProgram { items: Vec::new() }, CompileTarget::Rust);
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
        let ast = Parser::new(&tokens, &span_mapper, "<test>").parse().expect("parse");
        let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

        let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);
        assert!(bundle
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "ui.runtime-bundle"));
        assert!(bundle.items.iter().any(|item| item.id == "component:App"));
    }
}
