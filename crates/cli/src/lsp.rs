use std::collections::{hash_map::Entry, HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};

use crate::ast::{
    Actor, Component, Const, EntangleDef, Enum, Field, Function, Impl, Item, MacroDef,
    MessageHandler, Param, Program, Shader, StateDecl, Struct, TestDef, Trait, TraitMethod, Type,
    Uniform, Variant, Visibility,
};
use crate::diagnostic_registry::spec_for_code;
use crate::error::KainError;
use crate::lexer::{Lexer, Token, TokenKind};
use crate::packager::{load_manifest, PackageManifest};
use crate::parser::Parser;
use crate::span::Span;
use crate::types;
use crate::ErrorKind;
use tower_lsp::jsonrpc::Result;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

#[cfg(feature = "gpu")]
use gpu;
#[cfg(feature = "ue5")]
use ue5;
#[cfg(feature = "ue5")]
use ue5_shaders;
#[cfg(feature = "web")]
use web;

const TOKEN_TYPES: &[SemanticTokenType] = &[
    SemanticTokenType::KEYWORD,
    SemanticTokenType::STRING,
    SemanticTokenType::NUMBER,
    SemanticTokenType::OPERATOR,
    SemanticTokenType::FUNCTION,
    SemanticTokenType::METHOD,
    SemanticTokenType::PARAMETER,
    SemanticTokenType::VARIABLE,
    SemanticTokenType::PROPERTY,
    SemanticTokenType::STRUCT,
    SemanticTokenType::ENUM,
    SemanticTokenType::ENUM_MEMBER,
    SemanticTokenType::INTERFACE,
    SemanticTokenType::TYPE,
    SemanticTokenType::TYPE_PARAMETER,
    SemanticTokenType::DECORATOR,
    SemanticTokenType::MACRO,
    SemanticTokenType::NAMESPACE,
];

const TOKEN_MODIFIERS: &[SemanticTokenModifier] = &[
    SemanticTokenModifier::DECLARATION,
    SemanticTokenModifier::READONLY,
    SemanticTokenModifier::DEFAULT_LIBRARY,
];

const MOD_DECLARATION: u32 = 1 << 0;
const MOD_READONLY: u32 = 1 << 1;
const MOD_DEFAULT_LIBRARY: u32 = 1 << 2;

const KEYWORD_ITEMS: &[&str] = &[
    "fn",
    "let",
    "mut",
    "var",
    "const",
    "if",
    "else",
    "elif",
    "match",
    "for",
    "while",
    "loop",
    "break",
    "continue",
    "return",
    "await",
    "in",
    "with",
    "as",
    "type",
    "struct",
    "enum",
    "trait",
    "impl",
    "pub",
    "mod",
    "use",
    "self",
    "Self",
    "true",
    "false",
    "none",
    "component",
    "patch",
    "law",
    "converge",
    "world",
    "orchestrate",
    "entangle",
    "shader",
    "actor",
    "state",
    "spawn",
    "send",
    "receive",
    "emit",
    "comptime",
    "macro",
    "vertex",
    "fragment",
    "test",
];

const EFFECT_ITEMS: &[&str] = &[
    "Pure", "IO", "Async", "GPU", "Reactive", "Unsafe", "Alloc", "Panic",
];

const TYPE_ITEMS: &[&str] = &[
    "Int",
    "Float",
    "Bool",
    "String",
    "Char",
    "Unit",
    "Never",
    "Option",
    "Result",
    "Array",
    "Slice",
    "Tuple",
    "Map",
    "Set",
    "Vec2",
    "Vec3",
    "Vec4",
    "Mat2",
    "Mat3",
    "Mat4",
    "Sampler2D",
    "Sampler3D",
    "Ptr",
    "PtrMut",
    "i8",
    "i16",
    "i32",
    "i64",
    "i128",
    "u8",
    "u16",
    "u32",
    "u64",
    "u128",
    "isize",
    "usize",
    "f32",
    "f64",
];

const STDLIB_ITEMS: &[&str] = &[
    "print",
    "println",
    "read_line",
    "read_file",
    "write_file",
    "push",
    "pop",
    "len",
    "map",
    "filter",
    "reduce",
    "sort",
    "reverse",
    "abs",
    "min",
    "max",
    "sqrt",
    "pow",
    "sin",
    "cos",
    "tan",
    "floor",
    "ceil",
    "round",
    "split",
    "join",
    "trim",
    "replace",
    "substring",
    "to_upper",
    "to_lower",
    "json_parse",
    "json_stringify",
    "http_get",
    "http_post",
    "py_call",
    "sample",
    "perlin_noise",
    "simplex_noise",
    "worley_noise",
    "fresnel_schlick",
    "ggx_distribution",
    "smith_geometry",
    "rgb_to_hsv",
    "hsv_to_rgb",
    "color_grade",
    "uv_scroll",
    "uv_scale",
    "uv_rotate",
    "apply_damage",
    "calculate_xp",
    "check_cooldown",
    "roll_loot",
    "addr_of",
    "ptr_offset",
    "mem_load",
    "mem_store",
    "sizeof_type",
    "alignof_type",
    "alloca",
    "uninit",
    "alloc",
    "realloc",
    "free",
];

#[derive(Debug, Clone)]
struct Document {
    text: String,
    version: i32,
    analysis: Option<DocumentAnalysis>,
}

#[derive(Debug, Default)]
struct DocumentStore {
    docs: tokio::sync::RwLock<HashMap<Url, Document>>,
}

impl DocumentStore {
    async fn upsert(&self, uri: Url, text: String, version: i32) {
        let mut guard = self.docs.write().await;
        guard.insert(
            uri,
            Document {
                text,
                version,
                analysis: None,
            },
        );
    }

    async fn remove(&self, uri: &Url) {
        let mut guard = self.docs.write().await;
        guard.remove(uri);
    }

    async fn get_text(&self, uri: &Url) -> Option<String> {
        let guard = self.docs.read().await;
        guard.get(uri).map(|doc| doc.text.clone())
    }

    async fn get_version(&self, uri: &Url) -> Option<i32> {
        let guard = self.docs.read().await;
        guard.get(uri).map(|doc| doc.version)
    }

    async fn get_analysis(&self, uri: &Url) -> Option<DocumentAnalysis> {
        let guard = self.docs.read().await;
        guard.get(uri).and_then(|doc| doc.analysis.clone())
    }

    async fn update_analysis(&self, uri: &Url, analysis: Option<DocumentAnalysis>) {
        let mut guard = self.docs.write().await;
        if let Some(doc) = guard.get_mut(uri) {
            doc.analysis = analysis;
        }
    }

    async fn apply_changes(
        &self,
        uri: &Url,
        version: i32,
        changes: &[TextDocumentContentChangeEvent],
    ) -> Option<String> {
        if changes.is_empty() {
            return self.get_text(uri).await;
        }

        let mut guard = self.docs.write().await;
        match guard.entry(uri.clone()) {
            Entry::Occupied(mut entry) => {
                let mut current = entry.get().text.clone();
                for change in changes {
                    current = if let Some(range) = &change.range {
                        apply_change(&current, range, &change.text)?
                    } else {
                        change.text.clone()
                    };
                }

                let doc = entry.get_mut();
                doc.text = current.clone();
                doc.version = version;
                doc.analysis = None;
                Some(current)
            }
            Entry::Vacant(entry) => {
                let full_text_change =
                    changes.iter().rev().find(|change| change.range.is_none())?;
                let text = full_text_change.text.clone();
                entry.insert(Document {
                    text: text.clone(),
                    version,
                    analysis: None,
                });
                Some(text)
            }
        }
    }
}

#[derive(Debug, Clone)]
struct DocumentAnalysis {
    symbols: HashMap<String, Vec<SymbolInfo>>,
    document_symbols: Vec<DocumentSymbol>,
    semantic_tokens: Vec<SemanticToken>,
    occurrences: HashMap<String, Vec<Range>>,
    signatures: HashMap<String, SignatureInfo>,
}

impl DocumentAnalysis {
    fn from_program(text: &str, program: &Program, tokens: &[Token]) -> Self {
        let mut builder = AnalysisBuilder::new(text);
        builder.collect_program(program);
        let occurrences = collect_occurrences(text, tokens);
        let symbols = builder.symbols.clone();
        let document_symbols = builder.document_symbols.clone();
        let signatures = builder.signatures.clone();
        let semantic_tokens = builder.build_semantic_tokens(tokens);

        Self {
            symbols,
            document_symbols,
            semantic_tokens,
            occurrences,
            signatures,
        }
    }

    fn lookup(&self, ident: &str) -> Option<&[SymbolInfo]> {
        self.symbols.get(ident).map(|items| items.as_slice())
    }
}

#[derive(Debug, Clone)]
struct SymbolInfo {
    range: Range,
    detail: Option<String>,
    completion_kind: AnalysisCompletionKind,
}

#[derive(Debug, Clone)]
struct SignatureInfo {
    label: String,
    parameters: Vec<String>,
    documentation: Option<String>,
}

#[derive(Debug, Clone)]
struct WorkspaceEntry {
    text: String,
    analysis: DocumentAnalysis,
}

#[derive(Debug, Clone, Copy)]
enum AnalysisCompletionKind {
    Function,
    Method,
    Struct,
    Enum,
    EnumMember,
    Trait,
    Variable,
    Field,
    Constant,
    Module,
    Macro,
    TypeAlias,
}

impl AnalysisCompletionKind {
    fn completion_item_kind(self) -> CompletionItemKind {
        match self {
            AnalysisCompletionKind::Function => CompletionItemKind::FUNCTION,
            AnalysisCompletionKind::Method => CompletionItemKind::METHOD,
            AnalysisCompletionKind::Struct => CompletionItemKind::STRUCT,
            AnalysisCompletionKind::Enum => CompletionItemKind::ENUM,
            AnalysisCompletionKind::EnumMember => CompletionItemKind::ENUM_MEMBER,
            AnalysisCompletionKind::Trait => CompletionItemKind::INTERFACE,
            AnalysisCompletionKind::Variable => CompletionItemKind::VARIABLE,
            AnalysisCompletionKind::Field => CompletionItemKind::FIELD,
            AnalysisCompletionKind::Constant => CompletionItemKind::CONSTANT,
            AnalysisCompletionKind::Module => CompletionItemKind::MODULE,
            AnalysisCompletionKind::Macro => CompletionItemKind::FUNCTION,
            AnalysisCompletionKind::TypeAlias => CompletionItemKind::TYPE_PARAMETER,
        }
    }
}

#[derive(Debug, Clone)]
struct SemanticTokenAbsolute {
    start: usize,
    end: usize,
    token_type: u32,
    token_modifiers: u32,
    priority: u8,
}

struct AnalysisBuilder<'a> {
    text: &'a str,
    symbols: HashMap<String, Vec<SymbolInfo>>,
    document_symbols: Vec<DocumentSymbol>,
    absolute_tokens: Vec<SemanticTokenAbsolute>,
    signatures: HashMap<String, SignatureInfo>,
}

impl<'a> AnalysisBuilder<'a> {
    fn new(text: &'a str) -> Self {
        Self {
            text,
            symbols: HashMap::new(),
            document_symbols: Vec::new(),
            absolute_tokens: Vec::new(),
            signatures: HashMap::new(),
        }
    }

    fn collect_program(&mut self, program: &Program) {
        for item in &program.items {
            if let Some(symbol) = self.collect_item(item) {
                self.document_symbols.push(symbol);
            }
        }
    }

    fn collect_item(&mut self, item: &Item) -> Option<DocumentSymbol> {
        match item {
            Item::Function(function) => Some(self.collect_function_symbol(function, false)),
            Item::Patch(patch) => self.simple_named_item_symbol(
                &patch.name,
                patch.span,
                Some(format!("patch {}", patch.name)),
                SymbolKind::FUNCTION,
                AnalysisCompletionKind::Function,
            ),
            Item::Law(law) => self.simple_named_item_symbol(
                &law.name,
                law.span,
                Some(format!("law {}", law.name)),
                SymbolKind::FUNCTION,
                AnalysisCompletionKind::Function,
            ),
            Item::Axiom(axiom) => self.simple_named_item_symbol(
                &axiom.name,
                axiom.span,
                Some(format!("axiom {}", axiom.name)),
                SymbolKind::CONSTANT,
                AnalysisCompletionKind::Function,
            ),
            Item::Converge(converge) => self.simple_named_item_symbol(
                &converge.name,
                converge.span,
                Some(format!("converge {}", converge.name)),
                SymbolKind::FUNCTION,
                AnalysisCompletionKind::Function,
            ),
            Item::World(world) => self.simple_named_item_symbol(
                &world.name,
                world.span,
                Some(format!("world {}", world.name)),
                SymbolKind::MODULE,
                AnalysisCompletionKind::Module,
            ),
            Item::Entangle(entangle) => self.collect_entangle_symbol(entangle),
            Item::Pulse(pulse) => self.simple_named_item_symbol(
                &pulse.name,
                pulse.span,
                Some(format!("pulse {}", pulse.name)),
                SymbolKind::EVENT,
                AnalysisCompletionKind::Function,
            ),
            Item::Orchestrate(orchestrate) => self.simple_named_item_symbol(
                &orchestrate.name,
                orchestrate.span,
                Some(format!("orchestrate {}", orchestrate.name)),
                SymbolKind::FUNCTION,
                AnalysisCompletionKind::Function,
            ),
            Item::Component(component) => Some(self.collect_component_symbol(component)),
            Item::Shader(shader) => Some(self.collect_shader_symbol(shader)),
            Item::Actor(actor) => Some(self.collect_actor_symbol(actor)),
            Item::Struct(struct_def) => Some(self.collect_struct_symbol(struct_def)),
            Item::Enum(enum_def) => Some(self.collect_enum_symbol(enum_def)),
            Item::Trait(trait_def) => Some(self.collect_trait_symbol(trait_def)),
            Item::Impl(impl_def) => Some(self.collect_impl_symbol(impl_def)),
            Item::TypeAlias(alias) => self.simple_item_symbol(
                &alias.name,
                alias.span,
                Some(format!(
                    "type {} = {}",
                    alias.name,
                    format_type(&alias.target)
                )),
                SymbolKind::TYPE_PARAMETER,
                AnalysisCompletionKind::TypeAlias,
                semantic_type_index(SemanticTokenType::TYPE),
                MOD_DECLARATION,
            ),
            Item::Mod(module) => self.simple_item_symbol(
                &module.name,
                module.span,
                Some(format_visibility(
                    module.visibility,
                    &format!("mod {}", module.name),
                )),
                SymbolKind::MODULE,
                AnalysisCompletionKind::Module,
                semantic_type_index(SemanticTokenType::NAMESPACE),
                MOD_DECLARATION,
            ),
            Item::Const(const_def) => self.collect_const_symbol(const_def),
            Item::Macro(mac) => self.collect_macro_symbol(mac),
            Item::Test(test) => self.collect_test_symbol(test),
            Item::MaterialGraph(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@material_graph {}", def.name)),
                SymbolKind::OBJECT,
                AnalysisCompletionKind::Struct,
            ),
            Item::MaterialFunction(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@material_function {}", def.name)),
                SymbolKind::FUNCTION,
                AnalysisCompletionKind::Function,
            ),
            Item::GraphEditor(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@graph_editor {}", def.name)),
                SymbolKind::OBJECT,
                AnalysisCompletionKind::Struct,
            ),
            Item::GraphRuntime(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@graph_runtime {}", def.name)),
                SymbolKind::OBJECT,
                AnalysisCompletionKind::Struct,
            ),
            Item::StateMachine(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@state_machine {}", def.name)),
                SymbolKind::OBJECT,
                AnalysisCompletionKind::Struct,
            ),
            Item::AsyncTask(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@async_task {}", def.name)),
                SymbolKind::OBJECT,
                AnalysisCompletionKind::Struct,
            ),
            Item::EditorModule(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@editor_module {}", def.name)),
                SymbolKind::MODULE,
                AnalysisCompletionKind::Module,
            ),
            Item::GameplayTags(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@gameplay_tags namespace {}", def.name)),
                SymbolKind::MODULE,
                AnalysisCompletionKind::Module,
            ),
            Item::GameplayAbility(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@ability struct {}", def.name)),
                SymbolKind::STRUCT,
                AnalysisCompletionKind::Struct,
            ),
            Item::GameplayEffect(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@gameplay_effect struct {}", def.name)),
                SymbolKind::STRUCT,
                AnalysisCompletionKind::Struct,
            ),
            Item::GameplayCue(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@gameplay_cue struct {}", def.name)),
                SymbolKind::STRUCT,
                AnalysisCompletionKind::Struct,
            ),
            Item::AbilityTask(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@ability_task struct {}", def.name)),
                SymbolKind::STRUCT,
                AnalysisCompletionKind::Struct,
            ),
            Item::TargetActor(def) => self.simple_named_item_symbol(
                &def.name,
                def.span,
                Some(format!("@target_actor struct {}", def.name)),
                SymbolKind::STRUCT,
                AnalysisCompletionKind::Struct,
            ),
            Item::Use(_) | Item::Comptime(_) => None,
        }
    }

    fn collect_function_symbol(&mut self, function: &Function, method: bool) -> DocumentSymbol {
        let detail = Some(format_visibility(
            function.visibility,
            &format_fn_signature(function),
        ));
        self.signatures
            .entry(function.name.clone())
            .or_insert_with(|| SignatureInfo {
                label: format_fn_signature(function),
                parameters: function
                    .params
                    .iter()
                    .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
                    .collect(),
                documentation: detail.clone(),
            });
        let (selection_range, full_range) = self
            .name_and_full_ranges(&function.name, function.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &function.name,
            selection_range,
            detail.clone(),
            if method {
                AnalysisCompletionKind::Method
            } else {
                AnalysisCompletionKind::Function
            },
        );
        self.add_semantic_token_from_range(
            selection_range,
            if method {
                SemanticTokenType::METHOD
            } else {
                SemanticTokenType::FUNCTION
            },
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for generic in &function.generics {
            if let Some(range) = find_identifier_range(self.text, &generic.name, Some(generic.span))
            {
                self.add_symbol(
                    &generic.name,
                    range,
                    Some(format!("generic {}", generic.name)),
                    AnalysisCompletionKind::TypeAlias,
                );
                self.add_semantic_token_from_range(
                    range,
                    SemanticTokenType::TYPE_PARAMETER,
                    MOD_DECLARATION,
                    5,
                );
            }
        }
        for param in &function.params {
            if let Some(child) = self.collect_param_symbol(param) {
                children.push(child);
            }
        }

        DocumentSymbol {
            name: function.name.clone(),
            detail,
            kind: SymbolKind::FUNCTION,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_component_symbol(&mut self, component: &Component) -> DocumentSymbol {
        let detail = Some(format_visibility(
            component.visibility,
            &format!("component {} -> UI", component.name),
        ));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&component.name, component.span)
            .unwrap_or_else(default_range_pair);
        self.add_symbol(
            &component.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Struct,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::STRUCT,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for prop in &component.props {
            if let Some(symbol) = self.collect_param_symbol(prop) {
                children.push(symbol);
            }
        }
        for state in &component.state {
            if let Some(symbol) = self.collect_state_symbol(state) {
                children.push(symbol);
            }
        }
        for method in &component.methods {
            children.push(self.collect_function_symbol(method, true));
        }

        DocumentSymbol {
            name: component.name.clone(),
            detail,
            kind: SymbolKind::STRUCT,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_shader_symbol(&mut self, shader: &Shader) -> DocumentSymbol {
        let detail = Some(format!(
            "shader {} {} -> {}",
            format_shader_stage(shader.stage),
            shader.name,
            format_type(&shader.outputs)
        ));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&shader.name, shader.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &shader.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Function,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::FUNCTION,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for input in &shader.inputs {
            if let Some(symbol) = self.collect_param_symbol(input) {
                children.push(symbol);
            }
        }
        for uniform in &shader.uniforms {
            if let Some(symbol) = self.collect_uniform_symbol(uniform) {
                children.push(symbol);
            }
        }

        DocumentSymbol {
            name: shader.name.clone(),
            detail,
            kind: SymbolKind::FUNCTION,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_actor_symbol(&mut self, actor: &Actor) -> DocumentSymbol {
        let detail = Some(format!("actor {}", actor.name));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&actor.name, actor.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &actor.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Struct,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::STRUCT,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for state in &actor.state {
            if let Some(symbol) = self.collect_state_symbol(state) {
                children.push(symbol);
            }
        }
        for handler in &actor.handlers {
            if let Some(symbol) = self.collect_handler_symbol(handler) {
                children.push(symbol);
            }
        }
        for method in &actor.methods {
            children.push(self.collect_function_symbol(method, true));
        }

        DocumentSymbol {
            name: actor.name.clone(),
            detail,
            kind: SymbolKind::STRUCT,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_struct_symbol(&mut self, struct_def: &Struct) -> DocumentSymbol {
        let detail = Some(format_visibility(
            struct_def.visibility,
            &format!(
                "struct {}",
                format_type_name(&struct_def.name, &struct_def.generics)
            ),
        ));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&struct_def.name, struct_def.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &struct_def.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Struct,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::STRUCT,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for generic in &struct_def.generics {
            if let Some(range) = find_identifier_range(self.text, &generic.name, Some(generic.span))
            {
                self.add_symbol(
                    &generic.name,
                    range,
                    Some(format!("generic {}", generic.name)),
                    AnalysisCompletionKind::TypeAlias,
                );
                self.add_semantic_token_from_range(
                    range,
                    SemanticTokenType::TYPE_PARAMETER,
                    MOD_DECLARATION,
                    5,
                );
            }
        }
        for field in &struct_def.fields {
            if let Some(symbol) = self.collect_field_symbol(field) {
                children.push(symbol);
            }
        }
        for method in &struct_def.methods {
            children.push(self.collect_function_symbol(method, true));
        }

        DocumentSymbol {
            name: struct_def.name.clone(),
            detail,
            kind: SymbolKind::STRUCT,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_enum_symbol(&mut self, enum_def: &Enum) -> DocumentSymbol {
        let detail = Some(format_visibility(
            enum_def.visibility,
            &format!(
                "enum {}",
                format_type_name(&enum_def.name, &enum_def.generics)
            ),
        ));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&enum_def.name, enum_def.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &enum_def.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Enum,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::ENUM,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for variant in &enum_def.variants {
            if let Some(symbol) = self.collect_variant_symbol(variant) {
                children.push(symbol);
            }
        }

        DocumentSymbol {
            name: enum_def.name.clone(),
            detail,
            kind: SymbolKind::ENUM,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_trait_symbol(&mut self, trait_def: &Trait) -> DocumentSymbol {
        let detail = Some(format_visibility(
            trait_def.visibility,
            &format!(
                "trait {}",
                format_type_name(&trait_def.name, &trait_def.generics)
            ),
        ));
        let (selection_range, full_range) = self
            .name_and_full_ranges(&trait_def.name, trait_def.span)
            .unwrap_or_else(default_range_pair);

        self.add_symbol(
            &trait_def.name,
            selection_range,
            detail.clone(),
            AnalysisCompletionKind::Trait,
        );
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::INTERFACE,
            MOD_DECLARATION,
            5,
        );

        let mut children = Vec::new();
        for method in &trait_def.methods {
            if let Some(symbol) = self.collect_trait_method_symbol(method) {
                children.push(symbol);
            }
        }

        DocumentSymbol {
            name: trait_def.name.clone(),
            detail,
            kind: SymbolKind::INTERFACE,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_impl_symbol(&mut self, impl_def: &Impl) -> DocumentSymbol {
        let name = if let Some(trait_name) = &impl_def.trait_name {
            format!(
                "impl {} for {}",
                trait_name,
                format_type(&impl_def.target_type)
            )
        } else {
            format!("impl {}", format_type(&impl_def.target_type))
        };
        let full_range = span_to_range(self.text, impl_def.span);
        let selection_range = full_range;

        let mut children = Vec::new();
        for method in &impl_def.methods {
            children.push(self.collect_function_symbol(method, true));
        }

        DocumentSymbol {
            name,
            detail: None,
            kind: SymbolKind::OBJECT,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: Some(children),
        }
    }

    fn collect_const_symbol(&mut self, const_def: &Const) -> Option<DocumentSymbol> {
        self.simple_item_symbol(
            &const_def.name,
            const_def.span,
            Some(format_visibility(
                const_def.visibility,
                &format!("const {}: {}", const_def.name, format_type(&const_def.ty)),
            )),
            SymbolKind::CONSTANT,
            AnalysisCompletionKind::Constant,
            semantic_type_index(SemanticTokenType::VARIABLE),
            MOD_DECLARATION | MOD_READONLY,
        )
    }

    fn collect_macro_symbol(&mut self, mac: &MacroDef) -> Option<DocumentSymbol> {
        self.simple_item_symbol(
            &mac.name,
            mac.span,
            Some(format!("macro {}!", mac.name)),
            SymbolKind::FUNCTION,
            AnalysisCompletionKind::Macro,
            semantic_type_index(SemanticTokenType::MACRO),
            MOD_DECLARATION,
        )
    }

    fn collect_test_symbol(&mut self, test: &TestDef) -> Option<DocumentSymbol> {
        self.simple_item_symbol(
            &test.name,
            test.span,
            Some(format!("test \"{}\"", test.name)),
            SymbolKind::EVENT,
            AnalysisCompletionKind::Function,
            semantic_type_index(SemanticTokenType::FUNCTION),
            MOD_DECLARATION,
        )
    }

    fn collect_param_symbol(&mut self, param: &Param) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &param.name, Some(param.span))?;
        let detail = Some(format!(
            "{}param {}: {}",
            if param.mutable { "mut " } else { "" },
            param.name,
            format_type(&param.ty)
        ));
        self.add_symbol(
            &param.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::Variable,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::PARAMETER, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: param.name.clone(),
            detail,
            kind: SymbolKind::VARIABLE,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_uniform_symbol(&mut self, uniform: &Uniform) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &uniform.name, Some(uniform.span))?;
        let detail = Some(format!(
            "uniform {}: {} @{}",
            uniform.name,
            format_type(&uniform.ty),
            uniform.binding
        ));
        self.add_symbol(
            &uniform.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::Field,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::PROPERTY, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: uniform.name.clone(),
            detail,
            kind: SymbolKind::PROPERTY,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_state_symbol(&mut self, state: &StateDecl) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &state.name, Some(state.span))?;
        let detail = Some(format!("state {}: {}", state.name, format_type(&state.ty)));
        self.add_symbol(
            &state.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::Field,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::PROPERTY, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: state.name.clone(),
            detail,
            kind: SymbolKind::PROPERTY,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_field_symbol(&mut self, field: &Field) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &field.name, Some(field.span))?;
        let detail = Some(format_visibility(
            field.visibility,
            &format!("{}: {}", field.name, format_type(&field.ty)),
        ));
        self.add_symbol(
            &field.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::Field,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::PROPERTY, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: field.name.clone(),
            detail,
            kind: SymbolKind::FIELD,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_variant_symbol(&mut self, variant: &Variant) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &variant.name, Some(variant.span))?;
        let detail = Some(format!("variant {}", variant.name));
        self.add_symbol(
            &variant.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::EnumMember,
        );
        self.add_semantic_token_from_range(
            range,
            SemanticTokenType::ENUM_MEMBER,
            MOD_DECLARATION,
            5,
        );
        Some(DocumentSymbol {
            name: variant.name.clone(),
            detail,
            kind: SymbolKind::ENUM_MEMBER,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_trait_method_symbol(&mut self, method: &TraitMethod) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &method.name, Some(method.span))?;
        let detail = Some(format_trait_method_signature(method));
        self.signatures
            .entry(method.name.clone())
            .or_insert_with(|| SignatureInfo {
                label: format_trait_method_signature(method),
                parameters: method
                    .params
                    .iter()
                    .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
                    .collect(),
                documentation: detail.clone(),
            });
        self.add_symbol(
            &method.name,
            range,
            detail.clone(),
            AnalysisCompletionKind::Method,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::METHOD, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: method.name.clone(),
            detail,
            kind: SymbolKind::METHOD,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn collect_entangle_symbol(&mut self, entangle: &EntangleDef) -> Option<DocumentSymbol> {
        let selection_range = find_identifier_range(self.text, "entangle", Some(entangle.span))?;
        let full_range = span_to_range(self.text, entangle.span);
        let authority = entangle.left.authored_path();
        let mirror = entangle.right.authored_path();
        let name = format!("{authority} <-> {mirror}");
        let detail = Some(format!(
            "entangle {authority} <-> {mirror} with {}",
            entangle.policy.as_str()
        ));
        self.add_semantic_token_from_range(
            selection_range,
            SemanticTokenType::KEYWORD,
            MOD_DECLARATION,
            5,
        );
        Some(DocumentSymbol {
            name,
            detail,
            kind: SymbolKind::PROPERTY,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: None,
        })
    }

    fn collect_handler_symbol(&mut self, handler: &MessageHandler) -> Option<DocumentSymbol> {
        let range = find_identifier_range(self.text, &handler.message_type, Some(handler.span))?;
        let detail = Some(format!(
            "on {}({})",
            handler.message_type,
            handler
                .params
                .iter()
                .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
                .collect::<Vec<_>>()
                .join(", ")
        ));
        self.signatures
            .entry(handler.message_type.clone())
            .or_insert_with(|| SignatureInfo {
                label: detail.clone().unwrap_or_default(),
                parameters: handler
                    .params
                    .iter()
                    .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
                    .collect(),
                documentation: detail.clone(),
            });
        self.add_symbol(
            &handler.message_type,
            range,
            detail.clone(),
            AnalysisCompletionKind::Method,
        );
        self.add_semantic_token_from_range(range, SemanticTokenType::METHOD, MOD_DECLARATION, 5);
        Some(DocumentSymbol {
            name: handler.message_type.clone(),
            detail,
            kind: SymbolKind::METHOD,
            tags: None,
            deprecated: None,
            range,
            selection_range: range,
            children: None,
        })
    }

    fn simple_named_item_symbol(
        &mut self,
        name: &str,
        span: Span,
        detail: Option<String>,
        kind: SymbolKind,
        completion_kind: AnalysisCompletionKind,
    ) -> Option<DocumentSymbol> {
        self.simple_item_symbol(
            name,
            span,
            detail,
            kind,
            completion_kind,
            semantic_type_index(SemanticTokenType::STRUCT),
            MOD_DECLARATION,
        )
    }

    fn simple_item_symbol(
        &mut self,
        name: &str,
        span: Span,
        detail: Option<String>,
        kind: SymbolKind,
        completion_kind: AnalysisCompletionKind,
        semantic_type: u32,
        semantic_modifiers: u32,
    ) -> Option<DocumentSymbol> {
        let (selection_range, full_range) = self.name_and_full_ranges(name, span)?;
        self.add_symbol(name, selection_range, detail.clone(), completion_kind);
        self.absolute_tokens.push(SemanticTokenAbsolute {
            start: range_start_offset(self.text, selection_range),
            end: range_end_offset(self.text, selection_range),
            token_type: semantic_type,
            token_modifiers: semantic_modifiers,
            priority: 5,
        });
        Some(DocumentSymbol {
            name: name.to_string(),
            detail,
            kind,
            tags: None,
            deprecated: None,
            range: full_range,
            selection_range,
            children: None,
        })
    }

    fn name_and_full_ranges(&self, name: &str, span: Span) -> Option<(Range, Range)> {
        let selection_range = find_identifier_range(self.text, name, Some(span))?;
        let full_range = span_to_range(self.text, span);
        Some((selection_range, full_range))
    }

    fn add_symbol(
        &mut self,
        name: &str,
        range: Range,
        detail: Option<String>,
        completion_kind: AnalysisCompletionKind,
    ) {
        self.symbols
            .entry(name.to_string())
            .or_default()
            .push(SymbolInfo {
                range,
                detail,
                completion_kind,
            });
    }

    fn add_semantic_token_from_range(
        &mut self,
        range: Range,
        token_type: SemanticTokenType,
        token_modifiers: u32,
        priority: u8,
    ) {
        let start = range_start_offset(self.text, range);
        let end = range_end_offset(self.text, range);
        if start >= end {
            return;
        }
        self.absolute_tokens.push(SemanticTokenAbsolute {
            start,
            end,
            token_type: semantic_type_index(token_type),
            token_modifiers,
            priority,
        });
    }

    fn build_semantic_tokens(mut self, tokens: &[Token]) -> Vec<SemanticToken> {
        for token in tokens {
            if let Some(token_type) = semantic_token_type_for_lexical(&token.kind) {
                self.absolute_tokens.push(SemanticTokenAbsolute {
                    start: token.span.start,
                    end: token.span.end,
                    token_type,
                    token_modifiers: 0,
                    priority: 1,
                });
            }
        }

        self.absolute_tokens
            .sort_by_key(|token| (token.start, token.end, token.priority));

        let mut filtered = Vec::new();
        let mut last_end = 0usize;
        for token in self.absolute_tokens {
            if token.start < last_end {
                continue;
            }
            last_end = token.end;
            filtered.push(token);
        }

        encode_semantic_tokens(self.text, &filtered)
    }
}

#[derive(Debug)]
struct Backend {
    client: Client,
    docs: DocumentStore,
    workspace_roots: tokio::sync::RwLock<Vec<PathBuf>>,
    workspace_index: tokio::sync::RwLock<HashMap<Url, WorkspaceEntry>>,
}

#[tower_lsp::async_trait]
impl LanguageServer for Backend {
    async fn initialize(&self, params: InitializeParams) -> Result<InitializeResult> {
        let mut roots = Vec::new();
        if let Some(folders) = params.workspace_folders {
            for folder in folders {
                if let Ok(path) = folder.uri.to_file_path() {
                    roots.push(path);
                }
            }
        } else if let Some(root_uri) = params.root_uri {
            if let Ok(path) = root_uri.to_file_path() {
                roots.push(path);
            }
        }
        *self.workspace_roots.write().await = roots;

        Ok(InitializeResult {
            server_info: Some(ServerInfo {
                name: "KAIN Language Server".to_string(),
                version: Some(env!("CARGO_PKG_VERSION").to_string()),
            }),
            capabilities: ServerCapabilities {
                text_document_sync: Some(TextDocumentSyncCapability::Options(
                    TextDocumentSyncOptions {
                        open_close: Some(true),
                        change: Some(TextDocumentSyncKind::INCREMENTAL),
                        will_save: None,
                        will_save_wait_until: None,
                        save: Some(TextDocumentSyncSaveOptions::Supported(true)),
                    },
                )),
                hover_provider: Some(HoverProviderCapability::Simple(true)),
                definition_provider: Some(OneOf::Left(true)),
                references_provider: Some(OneOf::Left(true)),
                rename_provider: Some(OneOf::Left(true)),
                document_symbol_provider: Some(OneOf::Left(true)),
                workspace_symbol_provider: Some(OneOf::Left(true)),
                document_formatting_provider: Some(OneOf::Left(true)),
                code_action_provider: Some(CodeActionProviderCapability::Simple(true)),
                signature_help_provider: Some(SignatureHelpOptions {
                    trigger_characters: Some(vec!["(".to_string(), ",".to_string()]),
                    retrigger_characters: Some(vec![",".to_string()]),
                    work_done_progress_options: WorkDoneProgressOptions::default(),
                }),
                completion_provider: Some(CompletionOptions {
                    resolve_provider: Some(false),
                    trigger_characters: Some(vec![
                        ".".to_string(),
                        ":".to_string(),
                        "@".to_string(),
                    ]),
                    ..CompletionOptions::default()
                }),
                semantic_tokens_provider: Some(
                    SemanticTokensServerCapabilities::SemanticTokensOptions(
                        SemanticTokensOptions {
                            legend: SemanticTokensLegend {
                                token_types: TOKEN_TYPES.to_vec(),
                                token_modifiers: TOKEN_MODIFIERS.to_vec(),
                            },
                            full: Some(SemanticTokensFullOptions::Bool(true)),
                            range: None,
                            work_done_progress_options: WorkDoneProgressOptions::default(),
                        },
                    ),
                ),
                ..Default::default()
            },
        })
    }

    async fn initialized(&self, _: InitializedParams) {
        self.client
            .log_message(
                MessageType::INFO,
                "KAIN Language Server initialized with live diagnostics, outline symbols, and semantic tokens.",
            )
            .await;
        self.refresh_workspace_index().await;
    }

    async fn shutdown(&self) -> Result<()> {
        Ok(())
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        let uri = params.text_document.uri;
        let text = params.text_document.text;
        let version = params.text_document.version;
        self.docs.upsert(uri.clone(), text.clone(), version).await;
        self.validate_document(uri, text, Some(version)).await;
    }

    async fn did_change(&self, params: DidChangeTextDocumentParams) {
        if params.content_changes.is_empty() {
            return;
        }

        let uri = params.text_document.uri;
        let version = params.text_document.version;
        match self
            .docs
            .apply_changes(&uri, version, &params.content_changes)
            .await
        {
            Some(text) => {
                self.validate_document(uri, text, Some(version)).await;
            }
            None => {
                self.client
                    .log_message(
                        MessageType::ERROR,
                        format!("Failed to apply text changes for {}", uri),
                    )
                    .await;
            }
        }
    }

    async fn did_close(&self, params: DidCloseTextDocumentParams) {
        self.docs.remove(&params.text_document.uri).await;
        self.client
            .publish_diagnostics(params.text_document.uri, vec![], None)
            .await;
    }

    async fn did_save(&self, params: DidSaveTextDocumentParams) {
        if let Some(text) = self.docs.get_text(&params.text_document.uri).await {
            let version = self.docs.get_version(&params.text_document.uri).await;
            self.validate_document(params.text_document.uri.clone(), text, version)
                .await;
        }
        self.refresh_workspace_index().await;
    }

    async fn hover(&self, params: HoverParams) -> Result<Option<Hover>> {
        let uri = params.text_document_position_params.text_document.uri;
        let position = params.text_document_position_params.position;
        let text = match self.docs.get_text(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };

        let analysis = match self.docs.get_analysis(&uri).await {
            Some(analysis) => analysis,
            None => return Ok(None),
        };

        let offset = match position_to_offset(&text, &position) {
            Some(offset) => offset,
            None => return Ok(None),
        };

        let (ident, range) = match find_ident_at_offset(&text, offset) {
            Some(value) => value,
            None => return Ok(None),
        };

        if let Some(symbols) = analysis.lookup(&ident) {
            if let Some(info) = symbols.first() {
                let value = info.detail.clone().unwrap_or_else(|| ident.clone());
                return Ok(Some(Hover {
                    contents: HoverContents::Markup(MarkupContent {
                        kind: MarkupKind::Markdown,
                        value: format!("```kain\n{}\n```", value),
                    }),
                    range: Some(info.range),
                }));
            }
        }

        if KEYWORD_ITEMS.contains(&ident.as_str()) || EFFECT_ITEMS.contains(&ident.as_str()) {
            return Ok(Some(Hover {
                contents: HoverContents::Markup(MarkupContent {
                    kind: MarkupKind::Markdown,
                    value: format!("```text\nKAIN keyword: {}\n```", ident),
                }),
                range: Some(range),
            }));
        }

        if TYPE_ITEMS.contains(&ident.as_str()) {
            return Ok(Some(Hover {
                contents: HoverContents::Markup(MarkupContent {
                    kind: MarkupKind::Markdown,
                    value: format!("```text\nKAIN built-in type: {}\n```", ident),
                }),
                range: Some(range),
            }));
        }

        Ok(None)
    }

    async fn goto_definition(
        &self,
        params: GotoDefinitionParams,
    ) -> Result<Option<GotoDefinitionResponse>> {
        let uri = params.text_document_position_params.text_document.uri;
        let position = params.text_document_position_params.position;
        let text = match self.docs.get_text(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };
        let analysis = match self.docs.get_analysis(&uri).await {
            Some(analysis) => analysis,
            None => return Ok(None),
        };
        let offset = match position_to_offset(&text, &position) {
            Some(offset) => offset,
            None => return Ok(None),
        };
        let (ident, _) = match find_ident_at_offset(&text, offset) {
            Some(item) => item,
            None => return Ok(None),
        };

        if let Some(items) = analysis.lookup(&ident) {
            if let Some(item) = items.first() {
                return Ok(Some(GotoDefinitionResponse::Scalar(Location::new(
                    uri, item.range,
                ))));
            }
        }

        Ok(None)
    }

    async fn completion(&self, params: CompletionParams) -> Result<Option<CompletionResponse>> {
        let uri = params.text_document_position.text_document.uri;
        let position = params.text_document_position.position;
        let text = self.docs.get_text(&uri).await.unwrap_or_default();
        let analysis = self.docs.get_analysis(&uri).await;

        let prefix = position_to_offset(&text, &position)
            .and_then(|offset| find_completion_prefix(&text, offset))
            .unwrap_or_default();

        let mut items = Vec::new();

        if let Some(analysis) = analysis {
            for (name, infos) in &analysis.symbols {
                if let Some(info) = infos.first() {
                    items.push(CompletionItem {
                        label: name.clone(),
                        kind: Some(info.completion_kind.completion_item_kind()),
                        detail: info.detail.clone(),
                        ..CompletionItem::default()
                    });
                }
            }
        }

        items.extend(keyword_completion_items());
        items.extend(effect_completion_items());
        items.extend(type_completion_items());
        items.extend(stdlib_completion_items());

        dedupe_completion_items(&mut items);

        if !prefix.is_empty() {
            let lowered = prefix.to_ascii_lowercase();
            items.retain(|item| item.label.to_ascii_lowercase().starts_with(&lowered));
        }

        Ok(Some(CompletionResponse::Array(items)))
    }

    async fn references(&self, params: ReferenceParams) -> Result<Option<Vec<Location>>> {
        let uri = params.text_document_position.text_document.uri;
        let position = params.text_document_position.position;
        let text = match self.text_for_uri(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };
        let offset = match position_to_offset(&text, &position) {
            Some(offset) => offset,
            None => return Ok(None),
        };
        let (ident, _) = match find_ident_at_offset(&text, offset) {
            Some(value) => value,
            None => return Ok(None),
        };

        let locations = self.workspace_occurrences(&ident).await;
        Ok(Some(locations))
    }

    async fn rename(&self, params: RenameParams) -> Result<Option<WorkspaceEdit>> {
        let uri = params.text_document_position.text_document.uri;
        let position = params.text_document_position.position;
        let text = match self.text_for_uri(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };
        let offset = match position_to_offset(&text, &position) {
            Some(offset) => offset,
            None => return Ok(None),
        };
        let (ident, _) = match find_ident_at_offset(&text, offset) {
            Some(value) => value,
            None => return Ok(None),
        };

        let mut changes: HashMap<Url, Vec<TextEdit>> = HashMap::new();
        for location in self.workspace_occurrences(&ident).await {
            changes.entry(location.uri).or_default().push(TextEdit {
                range: location.range,
                new_text: params.new_name.clone(),
            });
        }

        Ok(Some(WorkspaceEdit {
            changes: Some(changes),
            document_changes: None,
            change_annotations: None,
        }))
    }

    async fn signature_help(&self, params: SignatureHelpParams) -> Result<Option<SignatureHelp>> {
        let uri = params.text_document_position_params.text_document.uri;
        let position = params.text_document_position_params.position;
        let text = match self.text_for_uri(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };
        let offset = match position_to_offset(&text, &position) {
            Some(offset) => offset,
            None => return Ok(None),
        };
        let Some((callee, active_parameter)) = find_call_context(&text, offset) else {
            return Ok(None);
        };
        let Some(signature) = self.lookup_signature(&callee).await else {
            return Ok(None);
        };

        let signatures = vec![SignatureInformation {
            label: signature.label.clone(),
            documentation: signature.documentation.clone().map(Documentation::String),
            parameters: Some(
                signature
                    .parameters
                    .iter()
                    .map(|label| ParameterInformation {
                        label: ParameterLabel::Simple(label.clone()),
                        documentation: None,
                    })
                    .collect(),
            ),
            active_parameter: None,
        }];

        Ok(Some(SignatureHelp {
            signatures,
            active_signature: Some(0),
            active_parameter: Some(active_parameter as u32),
        }))
    }

    async fn document_symbol(
        &self,
        params: DocumentSymbolParams,
    ) -> Result<Option<DocumentSymbolResponse>> {
        let uri = params.text_document.uri;
        let analysis = match self.docs.get_analysis(&uri).await {
            Some(analysis) => analysis,
            None => return Ok(None),
        };

        Ok(Some(DocumentSymbolResponse::Nested(
            analysis.document_symbols,
        )))
    }

    async fn symbol(
        &self,
        params: WorkspaceSymbolParams,
    ) -> Result<Option<Vec<SymbolInformation>>> {
        let query = params.query.to_ascii_lowercase();
        let mut results = Vec::new();
        for (uri, entry) in self.all_workspace_entries().await {
            for (name, infos) in entry.analysis.symbols {
                if !query.is_empty() && !name.to_ascii_lowercase().contains(&query) {
                    continue;
                }
                if let Some(info) = infos.first() {
                    results.push(SymbolInformation {
                        name,
                        kind: completion_kind_to_symbol_kind(info.completion_kind),
                        tags: None,
                        deprecated: None,
                        location: Location {
                            uri: uri.clone(),
                            range: info.range,
                        },
                        container_name: None,
                    });
                }
            }
        }
        Ok(Some(results))
    }

    async fn formatting(&self, params: DocumentFormattingParams) -> Result<Option<Vec<TextEdit>>> {
        let uri = params.text_document.uri;
        let text = match self.text_for_uri(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };
        let formatted = format_document_text(&text);
        if formatted == text {
            return Ok(Some(vec![]));
        }
        let end = offset_to_position(&text, text.len());
        Ok(Some(vec![TextEdit {
            range: Range {
                start: Position::default(),
                end,
            },
            new_text: formatted,
        }]))
    }

    async fn code_action(&self, params: CodeActionParams) -> Result<Option<CodeActionResponse>> {
        let uri = params.text_document.uri;
        let text = match self.text_for_uri(&uri).await {
            Some(text) => text,
            None => return Ok(None),
        };

        let mut actions = Vec::new();
        for diagnostic in params.context.diagnostics {
            if let Some(action) = missing_colon_code_action(&uri, &text, &diagnostic) {
                actions.push(CodeActionOrCommand::CodeAction(action));
            }
        }

        let formatted = format_document_text(&text);
        if formatted != text {
            let end = offset_to_position(&text, text.len());
            actions.push(CodeActionOrCommand::CodeAction(CodeAction {
                title: "Format KAIN document".to_string(),
                kind: Some(CodeActionKind::SOURCE),
                diagnostics: None,
                edit: Some(WorkspaceEdit {
                    changes: Some(HashMap::from([(
                        uri.clone(),
                        vec![TextEdit {
                            range: Range {
                                start: Position::default(),
                                end,
                            },
                            new_text: formatted,
                        }],
                    )])),
                    document_changes: None,
                    change_annotations: None,
                }),
                command: None,
                is_preferred: Some(false),
                disabled: None,
                data: None,
            }));
        }

        Ok(Some(actions))
    }

    async fn semantic_tokens_full(
        &self,
        params: SemanticTokensParams,
    ) -> Result<Option<SemanticTokensResult>> {
        let uri = params.text_document.uri;
        let analysis = match self.docs.get_analysis(&uri).await {
            Some(analysis) => analysis,
            None => return Ok(None),
        };
        let result_id = self
            .docs
            .get_version(&uri)
            .await
            .map(|version| version.to_string());

        Ok(Some(SemanticTokensResult::Tokens(SemanticTokens {
            result_id,
            data: analysis.semantic_tokens,
        })))
    }
}

impl Backend {
    async fn text_for_uri(&self, uri: &Url) -> Option<String> {
        if let Some(text) = self.docs.get_text(uri).await {
            return Some(text);
        }
        let guard = self.workspace_index.read().await;
        guard.get(uri).map(|entry| entry.text.clone())
    }

    async fn all_workspace_entries(&self) -> Vec<(Url, WorkspaceEntry)> {
        let mut entries = Vec::new();
        let open_docs = self.docs.docs.read().await;
        for (uri, doc) in open_docs.iter() {
            if let Some(analysis) = &doc.analysis {
                entries.push((
                    uri.clone(),
                    WorkspaceEntry {
                        text: doc.text.clone(),
                        analysis: analysis.clone(),
                    },
                ));
            }
        }
        drop(open_docs);

        let indexed = self.workspace_index.read().await;
        for (uri, entry) in indexed.iter() {
            if entries.iter().any(|(existing_uri, _)| existing_uri == uri) {
                continue;
            }
            entries.push((uri.clone(), entry.clone()));
        }
        entries
    }

    async fn workspace_occurrences(&self, ident: &str) -> Vec<Location> {
        let mut results = Vec::new();
        for (uri, entry) in self.all_workspace_entries().await {
            if let Some(ranges) = entry.analysis.occurrences.get(ident) {
                for range in ranges {
                    results.push(Location {
                        uri: uri.clone(),
                        range: *range,
                    });
                }
            }
        }
        results
    }

    async fn lookup_signature(&self, name: &str) -> Option<SignatureInfo> {
        for (_, entry) in self.all_workspace_entries().await {
            if let Some(signature) = entry.analysis.signatures.get(name) {
                return Some(signature.clone());
            }
        }
        None
    }

    async fn refresh_workspace_index(&self) {
        let roots = self.workspace_roots.read().await.clone();
        if roots.is_empty() {
            return;
        }

        let mut updated = HashMap::new();
        let open_docs = self.docs.docs.read().await;
        let open_uris: HashSet<Url> = open_docs.keys().cloned().collect();
        drop(open_docs);

        for root in roots {
            for file in collect_kain_files(&root) {
                let Ok(uri) = Url::from_file_path(&file) else {
                    continue;
                };
                if open_uris.contains(&uri) {
                    continue;
                }
                let Ok(text) = fs::read_to_string(&file) else {
                    continue;
                };
                if let Some(analysis) = self.analyze_source(&uri, &text).await {
                    updated.insert(uri, WorkspaceEntry { text, analysis });
                }
            }
        }

        *self.workspace_index.write().await = updated;
    }

    async fn analyze_source(&self, uri: &Url, text: &str) -> Option<DocumentAnalysis> {
        let lexer = Lexer::new(text);
        let tokens = lexer.tokenize().ok()?;
        let span_mapper = crate::diagnostics::SpanMapper::new(text);
        let file_name = uri.path();
        let mut parser = Parser::new(&tokens, &span_mapper, file_name);
        let program = parser.parse().ok()?;
        types::check(&program, &span_mapper, file_name).ok()?;
        Some(DocumentAnalysis::from_program(text, &program, &tokens))
    }

    async fn validate_document(&self, uri: Url, text: String, version: Option<i32>) {
        let lexer = Lexer::new(&text);
        let tokens = match lexer.tokenize() {
            Ok(tokens) => tokens,
            Err(error) => {
                let diagnostics = diagnostics_from_error(&text, &error);
                self.client
                    .publish_diagnostics(uri.clone(), diagnostics, version)
                    .await;
                self.docs.update_analysis(&uri, None).await;
                return;
            }
        };

        let span_mapper = crate::diagnostics::SpanMapper::new(&text);
        let file_name = uri.path();
        let mut parser = Parser::new(&tokens, &span_mapper, file_name);

        let program = match parser.parse() {
            Ok(program) => program,
            Err(error) => {
                let diagnostics = diagnostics_from_error(&text, &error);
                self.client
                    .publish_diagnostics(uri.clone(), diagnostics, version)
                    .await;
                self.docs.update_analysis(&uri, None).await;
                return;
            }
        };

        let typed_program = match types::check(&program, &span_mapper, file_name) {
            Ok(typed_program) => typed_program,
            Err(error) => {
                let diagnostics = diagnostics_from_error(&text, &error);
                self.client
                    .publish_diagnostics(uri.clone(), diagnostics, version)
                    .await;
                self.docs.update_analysis(&uri, None).await;
                return;
            }
        };

        let mut diagnostics = target_specific_diagnostics(&uri, &text, &typed_program);
        if !diagnostics.is_empty() {
            self.client
                .publish_diagnostics(uri.clone(), diagnostics.clone(), version)
                .await;
        }

        let analysis = DocumentAnalysis::from_program(&text, &program, &tokens);
        self.docs.update_analysis(&uri, Some(analysis)).await;
        if let Some(analysis) = self.docs.get_analysis(&uri).await {
            self.workspace_index.write().await.insert(
                uri.clone(),
                WorkspaceEntry {
                    text: text.clone(),
                    analysis,
                },
            );
        }
        if diagnostics.is_empty() {
            diagnostics = vec![];
        }
        self.client
            .publish_diagnostics(uri, diagnostics, version)
            .await;
    }
}

pub async fn run_server() {
    let stdin = tokio::io::stdin();
    let stdout = tokio::io::stdout();

    let (service, socket) = LspService::new(|client| Backend {
        client,
        docs: DocumentStore::default(),
        workspace_roots: tokio::sync::RwLock::new(Vec::new()),
        workspace_index: tokio::sync::RwLock::new(HashMap::new()),
    });
    Server::new(stdin, stdout, socket).serve(service).await;
}

fn diagnostics_from_error(text: &str, error: &KainError) -> Vec<Diagnostic> {
    match error {
        KainError::Lexer { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Parser { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Type { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Effect { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Borrow { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Codegen { message, span } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::CodegenWithLocation { message, span, .. } => vec![basic_diagnostic(
            text,
            *span,
            message.clone(),
            DiagnosticSeverity::ERROR,
            None,
        )],
        KainError::Runtime { message } => vec![Diagnostic {
            range: Range::default(),
            severity: Some(DiagnosticSeverity::ERROR),
            code: None,
            code_description: None,
            source: Some("KAIN".to_string()),
            message: message.clone(),
            related_information: None,
            tags: None,
            data: None,
        }],
        KainError::Io(_) => vec![],
        KainError::Multi(errors) => errors
            .iter()
            .flat_map(|inner| diagnostics_from_error(text, inner))
            .collect(),
        KainError::Enhanced {
            kind,
            code,
            location,
            context,
            message,
            suggestion,
            ..
        } => {
            let spec = spec_for_code(*code);
            let severity = match kind {
                ErrorKind::Validation => DiagnosticSeverity::WARNING,
                _ => DiagnosticSeverity::ERROR,
            };
            let span = if let Some((line, column)) = location {
                line_col_to_span(text, *line, *column)
            } else {
                Span::default()
            };
            let mut lines = vec![message.clone()];
            if !context.is_empty() {
                lines.push(format!("Context: {}", context));
            }
            let suggestion_text = suggestion
                .clone()
                .or_else(|| spec.default_suggestion.map(|value| value.to_string()));
            if let Some(suggestion) = suggestion_text {
                lines.push(format!("Help: {}", suggestion));
            }
            if let Some(docs_key) = spec.docs_key {
                lines.push(format!("Reference: {}", docs_key));
            }

            vec![Diagnostic {
                range: span_to_range(text, span),
                severity: Some(severity),
                code: Some(NumberOrString::String(spec.code_str.to_string())),
                code_description: None,
                source: Some("KAIN".to_string()),
                message: lines.join("\n"),
                related_information: None,
                tags: None,
                data: None,
            }]
        }
    }
}

fn basic_diagnostic(
    text: &str,
    span: Span,
    message: String,
    severity: DiagnosticSeverity,
    code: Option<NumberOrString>,
) -> Diagnostic {
    Diagnostic {
        range: span_to_range(text, span),
        severity: Some(severity),
        code,
        code_description: None,
        source: Some("KAIN".to_string()),
        message,
        related_information: None,
        tags: None,
        data: None,
    }
}

fn apply_change(text: &str, range: &Range, new_text: &str) -> Option<String> {
    let start = position_to_offset(text, &range.start)?;
    let end = position_to_offset(text, &range.end)?;
    if start > end || end > text.len() {
        return None;
    }

    let mut result = String::with_capacity(text.len() + new_text.len());
    result.push_str(&text[..start]);
    result.push_str(new_text);
    result.push_str(&text[end..]);
    Some(result)
}

fn position_to_offset(text: &str, position: &Position) -> Option<usize> {
    let mut line = 0u32;
    let mut character = 0u32;

    for (offset, ch) in text.char_indices() {
        if line == position.line && character == position.character {
            return Some(offset);
        }

        if ch == '\n' {
            if line == position.line && position.character == character {
                return Some(offset);
            }
            line += 1;
            character = 0;
        } else {
            character += ch.len_utf16() as u32;
        }
    }

    if line == position.line && character == position.character {
        Some(text.len())
    } else if line == position.line && position.character >= character {
        Some(text.len())
    } else {
        None
    }
}

fn offset_to_position(text: &str, offset: usize) -> Position {
    let mut line = 0u32;
    let mut character = 0u32;

    for (idx, ch) in text.char_indices() {
        if idx >= offset {
            break;
        }
        if ch == '\n' {
            line += 1;
            character = 0;
        } else {
            character += ch.len_utf16() as u32;
        }
    }

    Position { line, character }
}

fn span_to_range(text: &str, span: Span) -> Range {
    Range {
        start: offset_to_position(text, span.start.min(text.len())),
        end: offset_to_position(text, span.end.min(text.len())),
    }
}

fn line_col_to_span(text: &str, line: usize, column: usize) -> Span {
    let zero_based_line = line.saturating_sub(1) as u32;
    let zero_based_column = column.saturating_sub(1) as u32;
    let start = position_to_offset(
        text,
        &Position {
            line: zero_based_line,
            character: zero_based_column,
        },
    )
    .unwrap_or(0);
    Span::new(start, start)
}

fn find_identifier_range(text: &str, name: &str, span_hint: Option<Span>) -> Option<Range> {
    let bytes = text.as_bytes();
    if bytes.is_empty() || name.is_empty() {
        return None;
    }

    let (start, end) = if let Some(span) = span_hint {
        (span.start.min(text.len()), span.end.min(text.len()))
    } else {
        (0, text.len())
    };

    let window = &text[start..end];
    if let Some(index) = window.find(name) {
        let absolute = start + index;
        return Some(span_to_range(
            text,
            Span::new(absolute, absolute + name.len()),
        ));
    }

    if let Some(index) = text.find(name) {
        return Some(span_to_range(text, Span::new(index, index + name.len())));
    }

    None
}

fn find_ident_at_offset(text: &str, offset: usize) -> Option<(String, Range)> {
    if offset > text.len() {
        return None;
    }

    let bytes = text.as_bytes();
    let mut start = offset;
    let mut end = offset;

    while start > 0 && is_ident_char(bytes[start - 1] as char) {
        start -= 1;
    }
    while end < bytes.len() && is_ident_char(bytes[end] as char) {
        end += 1;
    }

    if start == end {
        return None;
    }

    let ident = String::from_utf8_lossy(&bytes[start..end]).to_string();
    Some((ident, span_to_range(text, Span::new(start, end))))
}

fn find_completion_prefix(text: &str, offset: usize) -> Option<String> {
    if offset > text.len() {
        return None;
    }
    let bytes = text.as_bytes();
    let mut start = offset;
    while start > 0 && is_ident_char(bytes[start - 1] as char) {
        start -= 1;
    }
    Some(String::from_utf8_lossy(&bytes[start..offset]).to_string())
}

fn is_ident_char(c: char) -> bool {
    c.is_alphanumeric() || c == '_'
}

fn collect_occurrences(text: &str, tokens: &[Token]) -> HashMap<String, Vec<Range>> {
    let mut occurrences = HashMap::new();
    for token in tokens {
        if let TokenKind::Ident(name) = &token.kind {
            occurrences
                .entry(name.clone())
                .or_insert_with(Vec::new)
                .push(span_to_range(text, token.span));
        }
    }
    occurrences
}

fn collect_kain_files(root: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(dir) = stack.pop() {
        let Ok(entries) = fs::read_dir(&dir) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                let name = path
                    .file_name()
                    .and_then(|v| v.to_str())
                    .unwrap_or_default();
                if matches!(name, "target" | ".git" | "node_modules" | ".vs" | ".idea") {
                    continue;
                }
                stack.push(path);
            } else if path
                .extension()
                .and_then(|ext| ext.to_str())
                .map(|ext| ext.eq_ignore_ascii_case("kn"))
                .unwrap_or(false)
            {
                files.push(path);
            }
        }
    }
    files
}

fn find_manifest_dir_for_path(path: &Path) -> Option<PathBuf> {
    let mut current = path.parent();
    while let Some(dir) = current {
        if dir.join("KAIN.toml").exists() || dir.join("kain.toml").exists() {
            return Some(dir.to_path_buf());
        }
        current = dir.parent();
    }
    None
}

fn target_specific_diagnostics(
    uri: &Url,
    text: &str,
    typed_program: &crate::types::TypedProgram,
) -> Vec<Diagnostic> {
    let Ok(path) = uri.to_file_path() else {
        return vec![];
    };
    let Some(manifest_dir) = find_manifest_dir_for_path(&path) else {
        return vec![];
    };
    let Ok(manifest) = load_manifest(&manifest_dir) else {
        return vec![];
    };

    let mut diagnostics = Vec::new();
    let span_mapper = crate::diagnostics::SpanMapper::new(text);
    let filename = path.to_string_lossy().to_string();
    let targets = manifest_targets(&manifest);
    let has_shaders = typed_program
        .items
        .iter()
        .any(|item| matches!(item, crate::types::TypedItem::Shader(_)));

    if manifest.ue5.is_some() || targets.contains("ue5") || targets.contains("ue5editor") {
        #[cfg(feature = "ue5")]
        if let Err(error) = ue5::validate_program(typed_program, &span_mapper, &filename) {
            diagnostics.extend(diagnostics_from_error(text, &error));
        }
    }

    if has_shaders && targets.contains("usf") {
        #[cfg(feature = "ue5")]
        if let Err(error) = ue5_shaders::generate_usf(typed_program) {
            diagnostics.extend(diagnostics_from_error(text, &error));
        }
    }

    if has_shaders && targets.contains("hlsl") {
        #[cfg(feature = "gpu")]
        if let Err(error) = gpu::generate_hlsl(typed_program) {
            diagnostics.extend(diagnostics_from_error(text, &error));
        }
    }

    if has_shaders && targets.contains("spirv") {
        #[cfg(feature = "gpu")]
        if let Err(error) = gpu::generate_spirv(typed_program) {
            diagnostics.extend(diagnostics_from_error(text, &error));
        }
    }

    if targets.contains("wasm") {
        #[cfg(feature = "web")]
        if let Err(error) = web::generate_wasm(typed_program) {
            diagnostics.extend(diagnostics_from_error(text, &error));
        }
    }

    diagnostics
}

fn manifest_targets(manifest: &PackageManifest) -> HashSet<String> {
    let mut targets = HashSet::new();
    for target in &manifest.build.targets {
        targets.insert(target.to_ascii_lowercase());
    }
    if manifest.ue5.is_some() {
        targets.insert("ue5".to_string());
    }
    targets
}

fn find_call_context(text: &str, offset: usize) -> Option<(String, usize)> {
    if offset > text.len() {
        return None;
    }
    let bytes = text.as_bytes();
    let mut depth = 0i32;
    let mut idx = offset;
    while idx > 0 {
        idx -= 1;
        match bytes[idx] as char {
            ')' => depth += 1,
            '(' => {
                if depth == 0 {
                    let callee = extract_callee_name(text, idx)?;
                    let active = active_argument_index(&text[idx + 1..offset]);
                    return Some((callee, active));
                }
                depth -= 1;
            }
            _ => {}
        }
    }
    None
}

fn extract_callee_name(text: &str, paren_offset: usize) -> Option<String> {
    if paren_offset == 0 {
        return None;
    }
    let bytes = text.as_bytes();
    let mut end = paren_offset;
    while end > 0 && bytes[end - 1].is_ascii_whitespace() {
        end -= 1;
    }
    let mut start = end;
    while start > 0 {
        let c = bytes[start - 1] as char;
        if is_ident_char(c) || c == ':' || c == '.' {
            start -= 1;
        } else {
            break;
        }
    }
    if start == end {
        return None;
    }
    let raw = &text[start..end];
    raw.split(&[':', '.'][..])
        .filter(|part| !part.is_empty())
        .next_back()
        .map(|part| part.to_string())
}

fn active_argument_index(slice: &str) -> usize {
    let mut depth_paren = 0i32;
    let mut depth_brace = 0i32;
    let mut depth_bracket = 0i32;
    let mut count = 0usize;
    for ch in slice.chars() {
        match ch {
            '(' => depth_paren += 1,
            ')' => depth_paren -= 1,
            '{' => depth_brace += 1,
            '}' => depth_brace -= 1,
            '[' => depth_bracket += 1,
            ']' => depth_bracket -= 1,
            ',' if depth_paren == 0 && depth_brace == 0 && depth_bracket == 0 => count += 1,
            _ => {}
        }
    }
    count
}

fn completion_kind_to_symbol_kind(kind: AnalysisCompletionKind) -> SymbolKind {
    match kind {
        AnalysisCompletionKind::Function => SymbolKind::FUNCTION,
        AnalysisCompletionKind::Method => SymbolKind::METHOD,
        AnalysisCompletionKind::Struct => SymbolKind::STRUCT,
        AnalysisCompletionKind::Enum => SymbolKind::ENUM,
        AnalysisCompletionKind::EnumMember => SymbolKind::ENUM_MEMBER,
        AnalysisCompletionKind::Trait => SymbolKind::INTERFACE,
        AnalysisCompletionKind::Variable => SymbolKind::VARIABLE,
        AnalysisCompletionKind::Field => SymbolKind::FIELD,
        AnalysisCompletionKind::Constant => SymbolKind::CONSTANT,
        AnalysisCompletionKind::Module => SymbolKind::MODULE,
        AnalysisCompletionKind::Macro => SymbolKind::FUNCTION,
        AnalysisCompletionKind::TypeAlias => SymbolKind::TYPE_PARAMETER,
    }
}

fn format_document_text(text: &str) -> String {
    let mut output = Vec::new();
    let mut indent = 0usize;
    for raw_line in text.lines() {
        let trimmed_end = raw_line.trim_end();
        let trimmed = trimmed_end.trim_start();
        if trimmed.is_empty() {
            output.push(String::new());
            continue;
        }

        if starts_dedent_line(trimmed) {
            indent = indent.saturating_sub(1);
        }

        let prefix = "    ".repeat(indent);
        output.push(format!("{}{}", prefix, trimmed));

        if opens_block_line(trimmed) {
            indent += 1;
        }
    }

    let mut formatted = output.join("\n");
    if text.ends_with('\n') || !formatted.is_empty() {
        formatted.push('\n');
    }
    formatted
}

fn starts_dedent_line(line: &str) -> bool {
    line.starts_with("else") || line.starts_with("elif")
}

fn opens_block_line(line: &str) -> bool {
    let prefixes = [
        "fn ",
        "if ",
        "elif ",
        "else",
        "match ",
        "for ",
        "while ",
        "loop",
        "component ",
        "shader ",
        "actor ",
        "struct ",
        "enum ",
        "trait ",
        "impl",
        "comptime",
        "test ",
        "@graph_editor",
        "@graph_runtime",
        "@state_machine",
        "@async_task",
        "@editor_module",
    ];
    line.ends_with(':') && prefixes.iter().any(|prefix| line.starts_with(prefix))
}

fn missing_colon_code_action(uri: &Url, text: &str, diagnostic: &Diagnostic) -> Option<CodeAction> {
    let line_index = diagnostic.range.start.line as usize;
    let line_text = text.lines().nth(line_index)?.trim_end();
    let trimmed = line_text.trim_start();
    if trimmed.ends_with(':') || !opens_block_line_candidate(trimmed) {
        return None;
    }
    if !diagnostic.message.to_ascii_lowercase().contains("expected") {
        return None;
    }
    let insert_at = Position {
        line: diagnostic.range.start.line,
        character: line_text.encode_utf16().count() as u32,
    };
    Some(CodeAction {
        title: "Insert missing ':'".to_string(),
        kind: Some(CodeActionKind::QUICKFIX),
        diagnostics: Some(vec![diagnostic.clone()]),
        edit: Some(WorkspaceEdit {
            changes: Some(HashMap::from([(
                uri.clone(),
                vec![TextEdit {
                    range: Range {
                        start: insert_at,
                        end: insert_at,
                    },
                    new_text: ":".to_string(),
                }],
            )])),
            document_changes: None,
            change_annotations: None,
        }),
        command: None,
        is_preferred: Some(true),
        disabled: None,
        data: None,
    })
}

fn opens_block_line_candidate(line: &str) -> bool {
    let prefixes = [
        "fn ",
        "if ",
        "elif ",
        "else",
        "match ",
        "for ",
        "while ",
        "loop",
        "component ",
        "shader ",
        "actor ",
        "struct ",
        "enum ",
        "trait ",
        "impl",
        "comptime",
        "test ",
        "@graph_editor",
        "@graph_runtime",
        "@state_machine",
        "@async_task",
        "@editor_module",
    ];
    prefixes.iter().any(|prefix| line.starts_with(prefix))
}

fn keyword_completion_items() -> Vec<CompletionItem> {
    KEYWORD_ITEMS
        .iter()
        .map(|label| CompletionItem {
            label: (*label).to_string(),
            kind: Some(CompletionItemKind::KEYWORD),
            detail: Some("KAIN keyword".to_string()),
            ..CompletionItem::default()
        })
        .collect()
}

fn effect_completion_items() -> Vec<CompletionItem> {
    EFFECT_ITEMS
        .iter()
        .map(|label| CompletionItem {
            label: (*label).to_string(),
            kind: Some(CompletionItemKind::ENUM_MEMBER),
            detail: Some("KAIN effect".to_string()),
            ..CompletionItem::default()
        })
        .collect()
}

fn type_completion_items() -> Vec<CompletionItem> {
    TYPE_ITEMS
        .iter()
        .map(|label| CompletionItem {
            label: (*label).to_string(),
            kind: Some(CompletionItemKind::CLASS),
            detail: Some("KAIN built-in type".to_string()),
            ..CompletionItem::default()
        })
        .collect()
}

fn stdlib_completion_items() -> Vec<CompletionItem> {
    STDLIB_ITEMS
        .iter()
        .map(|label| CompletionItem {
            label: (*label).to_string(),
            kind: Some(CompletionItemKind::FUNCTION),
            detail: Some("KAIN stdlib".to_string()),
            ..CompletionItem::default()
        })
        .collect()
}

fn dedupe_completion_items(items: &mut Vec<CompletionItem>) {
    let mut seen = HashMap::<String, usize>::new();
    items.retain(|item| match seen.entry(item.label.clone()) {
        Entry::Vacant(entry) => {
            entry.insert(1);
            true
        }
        Entry::Occupied(_) => false,
    });
}

fn semantic_token_type_for_lexical(token: &TokenKind) -> Option<u32> {
    let token_type = match token {
        TokenKind::Fn
        | TokenKind::Let
        | TokenKind::Mut
        | TokenKind::Var
        | TokenKind::Const
        | TokenKind::If
        | TokenKind::Else
        | TokenKind::Elif
        | TokenKind::Match
        | TokenKind::For
        | TokenKind::While
        | TokenKind::Loop
        | TokenKind::Break
        | TokenKind::Continue
        | TokenKind::Return
        | TokenKind::Await
        | TokenKind::In
        | TokenKind::With
        | TokenKind::As
        | TokenKind::TypeKw
        | TokenKind::Struct
        | TokenKind::Enum
        | TokenKind::Trait
        | TokenKind::Impl
        | TokenKind::Pub
        | TokenKind::Mod
        | TokenKind::Use
        | TokenKind::SelfLower
        | TokenKind::SelfUpper
        | TokenKind::True
        | TokenKind::False
        | TokenKind::None
        | TokenKind::Component
        | TokenKind::Shader
        | TokenKind::Actor
        | TokenKind::State
        | TokenKind::Spawn
        | TokenKind::Send
        | TokenKind::Receive
        | TokenKind::Emit
        | TokenKind::Comptime
        | TokenKind::Macro
        | TokenKind::Vertex
        | TokenKind::Fragment
        | TokenKind::Test
        | TokenKind::Pure
        | TokenKind::Io
        | TokenKind::AsyncKw
        | TokenKind::Async
        | TokenKind::Gpu
        | TokenKind::Reactive
        | TokenKind::Unsafe => SemanticTokenType::KEYWORD,
        TokenKind::String(_) | TokenKind::FString(_) | TokenKind::Char(_) => {
            SemanticTokenType::STRING
        }
        TokenKind::Int(_) | TokenKind::Float(_) => SemanticTokenType::NUMBER,
        TokenKind::Plus
        | TokenKind::Minus
        | TokenKind::Star
        | TokenKind::Slash
        | TokenKind::Percent
        | TokenKind::Power
        | TokenKind::EqEq
        | TokenKind::NotEq
        | TokenKind::Lt
        | TokenKind::Gt
        | TokenKind::LtEq
        | TokenKind::GtEq
        | TokenKind::And
        | TokenKind::Or
        | TokenKind::Not
        | TokenKind::Amp
        | TokenKind::Pipe
        | TokenKind::Caret
        | TokenKind::Tilde
        | TokenKind::Shl
        | TokenKind::Shr
        | TokenKind::Eq
        | TokenKind::PlusEq
        | TokenKind::MinusEq
        | TokenKind::StarEq
        | TokenKind::SlashEq
        | TokenKind::PercentEq
        | TokenKind::AmpEq
        | TokenKind::PipeEq
        | TokenKind::CaretEq
        | TokenKind::ShlEq
        | TokenKind::ShrEq
        | TokenKind::Dot
        | TokenKind::DotDot
        | TokenKind::DotDotDot
        | TokenKind::Colon
        | TokenKind::ColonColon
        | TokenKind::Arrow
        | TokenKind::FatArrow
        | TokenKind::At
        | TokenKind::QuestionQuestion
        | TokenKind::QuestionDot
        | TokenKind::Question => SemanticTokenType::OPERATOR,
        _ => return None,
    };
    Some(semantic_type_index(token_type))
}

fn encode_semantic_tokens(text: &str, tokens: &[SemanticTokenAbsolute]) -> Vec<SemanticToken> {
    let mut encoded = Vec::new();
    let mut previous_line = 0u32;
    let mut previous_start = 0u32;

    for token in tokens {
        let start = offset_to_position(text, token.start);
        let end = offset_to_position(text, token.end);
        if start.line != end.line {
            continue;
        }
        let length = end.character.saturating_sub(start.character);
        if length == 0 {
            continue;
        }

        let delta_line = start.line.saturating_sub(previous_line);
        let delta_start = if delta_line == 0 {
            start.character.saturating_sub(previous_start)
        } else {
            start.character
        };

        encoded.push(SemanticToken {
            delta_line,
            delta_start,
            length,
            token_type: token.token_type,
            token_modifiers_bitset: token.token_modifiers,
        });

        previous_line = start.line;
        previous_start = start.character;
    }

    encoded
}

fn semantic_type_index(target: SemanticTokenType) -> u32 {
    TOKEN_TYPES
        .iter()
        .position(|item| *item == target)
        .unwrap_or(0) as u32
}

fn range_start_offset(text: &str, range: Range) -> usize {
    position_to_offset(text, &range.start).unwrap_or(0)
}

fn range_end_offset(text: &str, range: Range) -> usize {
    position_to_offset(text, &range.end).unwrap_or(0)
}

fn format_fn_signature(function: &Function) -> String {
    let params = function
        .params
        .iter()
        .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
        .collect::<Vec<_>>()
        .join(", ");

    let ret = function
        .return_type
        .as_ref()
        .map(format_type)
        .unwrap_or_else(|| "()".to_string());

    let effects = if function.effects.is_empty() {
        String::new()
    } else {
        format!(
            " with {}",
            function
                .effects
                .iter()
                .map(|effect| format!("{:?}", effect))
                .collect::<Vec<_>>()
                .join(", ")
        )
    };

    format!("fn {}({}) -> {}{}", function.name, params, ret, effects)
}

fn format_trait_method_signature(method: &TraitMethod) -> String {
    let params = method
        .params
        .iter()
        .map(|param| format!("{}: {}", param.name, format_type(&param.ty)))
        .collect::<Vec<_>>()
        .join(", ");

    let ret = method
        .return_type
        .as_ref()
        .map(format_type)
        .unwrap_or_else(|| "()".to_string());

    format!("fn {}({}) -> {}", method.name, params, ret)
}

fn format_type(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                format!(
                    "{}<{}>",
                    name,
                    generics
                        .iter()
                        .map(format_type)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
        Type::Tuple(items, _) => format!(
            "({})",
            items.iter().map(format_type).collect::<Vec<_>>().join(", ")
        ),
        Type::Array(inner, size, _) => format!("[{}; {}]", format_type(inner), size),
        Type::Slice(inner, _) => format!("[{}]", format_type(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", format_type(inner))
            } else {
                format!("&{}", format_type(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("PtrMut<{}>", format_type(inner))
            } else {
                format!("Ptr<{}>", format_type(inner))
            }
        }
        Type::Function {
            params,
            return_type,
            effects,
            ..
        } => {
            let effects = if effects.is_empty() {
                String::new()
            } else {
                format!(
                    " with {}",
                    effects
                        .iter()
                        .map(|effect| format!("{:?}", effect))
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            };
            format!(
                "fn({}) -> {}{}",
                params
                    .iter()
                    .map(format_type)
                    .collect::<Vec<_>>()
                    .join(", "),
                format_type(return_type),
                effects
            )
        }
        Type::Option(inner, _) => format!("Option<{}>", format_type(inner)),
        Type::Result(ok, err, _) => format!("Result<{}, {}>", format_type(ok), format_type(err)),
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "Never".to_string(),
        Type::Unit(_) => "Unit".to_string(),
        Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                format!(
                    "impl {}<{}>",
                    trait_name,
                    generics
                        .iter()
                        .map(format_type)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
    }
}

fn format_type_name(name: &str, generics: &[crate::ast::Generic]) -> String {
    if generics.is_empty() {
        name.to_string()
    } else {
        format!(
            "{}<{}>",
            name,
            generics
                .iter()
                .map(|generic| generic.name.clone())
                .collect::<Vec<_>>()
                .join(", ")
        )
    }
}

fn format_shader_stage(stage: crate::ast::ShaderStage) -> &'static str {
    match stage {
        crate::ast::ShaderStage::Vertex => "vertex",
        crate::ast::ShaderStage::Fragment => "fragment",
        crate::ast::ShaderStage::Compute => "compute",
        crate::ast::ShaderStage::Surface => "surface",
    }
}

fn format_visibility(visibility: Visibility, detail: &str) -> String {
    match visibility {
        Visibility::Public => format!("pub {}", detail),
        Visibility::Private => detail.to_string(),
        Visibility::Crate => format!("crate {}", detail),
        Visibility::Super => format!("super {}", detail),
    }
}

fn default_range_pair() -> (Range, Range) {
    (Range::default(), Range::default())
}
