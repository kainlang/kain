use kain_core::ast::{
    Actor, Block, Component, Const, Field, Function, Impl, Item, Param, Program, Shader, Stmt,
    Struct, Type, TypeAlias,
};
use kain_core::lexer::{Lexer, Token, TokenKind};
use kain_core::span::Span;
use std::collections::{BTreeMap, HashMap};
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Position {
    pub line: u32,
    pub column: u32,
    pub offset: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Range {
    pub start: Position,
    pub end: Position,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SymbolKind {
    Function,
    Method,
    Struct,
    Enum,
    EnumMember,
    Trait,
    Field,
    Constant,
    Module,
    Actor,
    Component,
    Shader,
    TypeAlias,
    Variable,
}

impl SymbolKind {
    pub fn code(self) -> u32 {
        match self {
            Self::Function => 1,
            Self::Method => 2,
            Self::Struct => 3,
            Self::Enum => 4,
            Self::EnumMember => 5,
            Self::Trait => 6,
            Self::Field => 7,
            Self::Constant => 8,
            Self::Module => 9,
            Self::Actor => 10,
            Self::Component => 11,
            Self::Shader => 12,
            Self::TypeAlias => 13,
            Self::Variable => 14,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CompletionKind {
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
    Keyword,
    Effect,
    Type,
    Stdlib,
}

impl CompletionKind {
    pub fn code(self) -> u32 {
        match self {
            Self::Function => 1,
            Self::Method => 2,
            Self::Struct => 3,
            Self::Enum => 4,
            Self::EnumMember => 5,
            Self::Trait => 6,
            Self::Variable => 7,
            Self::Field => 8,
            Self::Constant => 9,
            Self::Module => 10,
            Self::Keyword => 11,
            Self::Effect => 12,
            Self::Type => 13,
            Self::Stdlib => 14,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SymbolRecord {
    pub name: String,
    pub detail: String,
    pub kind: SymbolKind,
    pub path: String,
    pub range: Range,
    pub name_range: Range,
    pub container: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LocationResult {
    pub path: String,
    pub range: Range,
    pub name: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SymbolResult {
    pub name: String,
    pub detail: String,
    pub kind: SymbolKind,
    pub location: LocationResult,
    pub container: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CompletionResult {
    pub label: String,
    pub detail: String,
    pub kind: CompletionKind,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SemanticTokenResult {
    pub range: Range,
    pub token_type: u32,
    pub token_modifiers: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OccurrenceRecord {
    pub name: String,
    pub path: String,
    pub range: Range,
}

#[derive(Debug, Clone, Default)]
pub struct DocumentAnalysis {
    pub path: String,
    pub symbols: Vec<SymbolRecord>,
    pub occurrences: Vec<OccurrenceRecord>,
    pub semantic_tokens: Vec<SemanticTokenResult>,
}

#[derive(Debug, Clone, Default)]
pub struct WorkspaceIndex {
    symbols_by_name: HashMap<String, Vec<SymbolRecord>>,
    occurrences_by_name: HashMap<String, Vec<OccurrenceRecord>>,
    symbols_by_path: HashMap<String, Vec<SymbolRecord>>,
    tokens_by_path: HashMap<String, Vec<SemanticTokenResult>>,
}

impl WorkspaceIndex {
    pub fn rebuild<'a>(documents: impl IntoIterator<Item = (&'a Path, &'a str)>) -> Self {
        let mut index = WorkspaceIndex::default();
        for (path, source) in documents {
            if let Some(analysis) = analyze_document(path, source) {
                index.insert_analysis(analysis);
            }
        }
        index
    }

    pub fn insert_analysis(&mut self, analysis: DocumentAnalysis) {
        self.remove_path(&analysis.path);
        for symbol in &analysis.symbols {
            self.symbols_by_name
                .entry(symbol.name.clone())
                .or_default()
                .push(symbol.clone());
        }
        for occurrence in &analysis.occurrences {
            self.occurrences_by_name
                .entry(occurrence.name.clone())
                .or_default()
                .push(occurrence.clone());
        }
        self.symbols_by_path
            .insert(analysis.path.clone(), analysis.symbols);
        self.tokens_by_path
            .insert(analysis.path, analysis.semantic_tokens);
    }

    pub fn remove_path(&mut self, path: &str) {
        if let Some(symbols) = self.symbols_by_path.remove(path) {
            for symbol in symbols {
                if let Some(items) = self.symbols_by_name.get_mut(&symbol.name) {
                    items.retain(|item| item.path != path);
                    if items.is_empty() {
                        self.symbols_by_name.remove(&symbol.name);
                    }
                }
            }
        }
        self.tokens_by_path.remove(path);
        for items in self.occurrences_by_name.values_mut() {
            items.retain(|item| item.path != path);
        }
        self.occurrences_by_name
            .retain(|_, items| !items.is_empty());
    }

    pub fn definitions(&self, name: &str) -> Vec<LocationResult> {
        self.symbols_by_name
            .get(name)
            .into_iter()
            .flatten()
            .map(|symbol| LocationResult {
                path: symbol.path.clone(),
                range: symbol.name_range,
                name: symbol.name.clone(),
            })
            .collect()
    }

    pub fn references(&self, name: &str) -> Vec<LocationResult> {
        self.occurrences_by_name
            .get(name)
            .into_iter()
            .flatten()
            .map(|occurrence| LocationResult {
                path: occurrence.path.clone(),
                range: occurrence.range,
                name: occurrence.name.clone(),
            })
            .collect()
    }

    pub fn document_symbols(&self, path: &str) -> Vec<SymbolResult> {
        self.symbols_by_path
            .get(path)
            .into_iter()
            .flatten()
            .map(symbol_to_result)
            .collect()
    }

    pub fn workspace_symbols(&self, query: &str) -> Vec<SymbolResult> {
        let query = query.to_ascii_lowercase();
        let mut ordered = BTreeMap::new();
        for symbols in self.symbols_by_name.values() {
            for symbol in symbols {
                if !query.is_empty() && !symbol.name.to_ascii_lowercase().contains(&query) {
                    continue;
                }
                ordered.insert(
                    (
                        symbol.name.clone(),
                        symbol.path.clone(),
                        symbol.name_range.start.offset,
                    ),
                    symbol_to_result(symbol),
                );
            }
        }
        ordered.into_values().collect()
    }

    pub fn completions(&self, prefix: &str) -> Vec<CompletionResult> {
        let lowered = prefix.to_ascii_lowercase();
        let mut items = BTreeMap::new();
        for symbols in self.symbols_by_name.values() {
            for symbol in symbols {
                if !matches_prefix(&symbol.name, &lowered) {
                    continue;
                }
                items
                    .entry(symbol.name.clone())
                    .or_insert_with(|| CompletionResult {
                        label: symbol.name.clone(),
                        detail: symbol.detail.clone(),
                        kind: completion_kind_for_symbol(symbol.kind),
                    });
            }
        }
        for (label, detail, kind) in inventory_items() {
            if matches_prefix(label, &lowered) {
                items
                    .entry(label.to_string())
                    .or_insert_with(|| CompletionResult {
                        label: label.to_string(),
                        detail: detail.to_string(),
                        kind,
                    });
            }
        }
        items.into_values().collect()
    }

    pub fn semantic_tokens(&self, path: &str) -> Vec<SemanticTokenResult> {
        self.tokens_by_path.get(path).cloned().unwrap_or_default()
    }

    pub fn symbol_at(&self, path: &str, source: &str, line: u32, column: u32) -> Option<String> {
        let offset = position_to_offset(source, line, column)?;
        identifier_at(source, offset)
            .map(|(name, _)| name)
            .or_else(|| {
                self.symbols_by_path.get(path).and_then(|symbols| {
                    symbols
                        .iter()
                        .find(|symbol| range_contains(symbol.name_range, offset))
                        .map(|symbol| symbol.name.clone())
                })
            })
    }

    pub fn first_symbol(&self, name: &str) -> Option<SymbolRecord> {
        self.symbols_by_name
            .get(name)
            .and_then(|symbols| symbols.first())
            .cloned()
    }
}

pub fn analyze_document(path: &Path, source: &str) -> Option<DocumentAnalysis> {
    let tokens = Lexer::new(source).tokenize().ok()?;
    let span_mapper = kain_core::diagnostics::SpanMapper::new(source);
    let filename = path.display().to_string();
    let program = kain_core::parser::Parser::new(&tokens, &span_mapper, &filename)
        .parse()
        .ok()?;
    Some(DocumentAnalysis {
        path: filename,
        symbols: collect_symbols(source, &program, path),
        occurrences: collect_occurrences(source, path, &tokens),
        semantic_tokens: collect_semantic_tokens(source, &tokens),
    })
}

pub fn position_to_offset(source: &str, line: u32, column: u32) -> Option<usize> {
    let mut current_line = 0u32;
    let mut current_col = 0u32;
    for (offset, ch) in source.char_indices() {
        if current_line == line && current_col == column {
            return Some(offset);
        }
        if ch == '\n' {
            current_line += 1;
            current_col = 0;
        } else {
            current_col += 1;
        }
    }
    if current_line == line && current_col == column {
        Some(source.len())
    } else {
        None
    }
}

pub fn offset_to_position(source: &str, offset: usize) -> Position {
    let mut line = 0u32;
    let mut column = 0u32;
    let clamped = offset.min(source.len());
    for (idx, ch) in source.char_indices() {
        if idx >= clamped {
            break;
        }
        if ch == '\n' {
            line += 1;
            column = 0;
        } else {
            column += 1;
        }
    }
    Position {
        line,
        column,
        offset: clamped,
    }
}

pub fn span_to_range(source: &str, span: Span) -> Range {
    Range {
        start: offset_to_position(source, span.start),
        end: offset_to_position(source, span.end),
    }
}

pub fn identifier_at(source: &str, offset: usize) -> Option<(String, Range)> {
    if offset > source.len() {
        return None;
    }
    let bytes = source.as_bytes();
    let mut start = offset;
    while start > 0 && is_ident_byte(bytes[start - 1]) {
        start -= 1;
    }
    let mut end = offset;
    while end < bytes.len() && is_ident_byte(bytes[end]) {
        end += 1;
    }
    if start == end || !is_ident_start(bytes[start]) {
        return None;
    }
    let name = source.get(start..end)?.to_string();
    Some((
        name,
        Range {
            start: offset_to_position(source, start),
            end: offset_to_position(source, end),
        },
    ))
}

fn collect_symbols(source: &str, program: &Program, path: &Path) -> Vec<SymbolRecord> {
    let mut out = Vec::new();
    for item in &program.items {
        collect_item_symbols(source, path, item, None, &mut out);
    }
    out
}

fn collect_item_symbols(
    source: &str,
    path: &Path,
    item: &Item,
    container: Option<&str>,
    out: &mut Vec<SymbolRecord>,
) {
    match item {
        Item::Function(function) => {
            push_function_symbol(source, path, function, SymbolKind::Function, container, out);
            collect_block_symbols(source, path, &function.body, Some(&function.name), out);
        }
        Item::Patch(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("patch {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::Law(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("law {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::Axiom(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("axiom {}", value.name),
            SymbolKind::Constant,
            value.span,
            container,
            out,
        ),
        Item::Converge(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("converge {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::World(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("world {}", value.name),
            SymbolKind::Module,
            value.span,
            container,
            out,
        ),
        Item::Orchestrate(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("orchestrate {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::Pulse(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("pulse {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::Component(component) => collect_component_symbols(source, path, component, out),
        Item::Shader(shader) => collect_shader_symbols(source, path, shader, out),
        Item::Actor(actor) => collect_actor_symbols(source, path, actor, out),
        Item::Struct(value) => collect_struct_symbols(source, path, value, out),
        Item::Enum(value) => {
            push_symbol(
                source,
                path,
                &value.name,
                format!("enum {}", value.name),
                SymbolKind::Enum,
                value.span,
                container,
                out,
            );
            for variant in &value.variants {
                push_symbol(
                    source,
                    path,
                    &variant.name,
                    format!("{}::{}", value.name, variant.name),
                    SymbolKind::EnumMember,
                    variant.span,
                    Some(&value.name),
                    out,
                );
            }
        }
        Item::Trait(value) => {
            push_symbol(
                source,
                path,
                &value.name,
                format!("trait {}", value.name),
                SymbolKind::Trait,
                value.span,
                container,
                out,
            );
            for method in &value.methods {
                push_symbol(
                    source,
                    path,
                    &method.name,
                    format!("trait method {}.{}", value.name, method.name),
                    SymbolKind::Method,
                    method.span,
                    Some(&value.name),
                    out,
                );
            }
        }
        Item::Impl(value) => collect_impl_symbols(source, path, value, out),
        Item::TypeAlias(value) => collect_type_alias_symbol(source, path, value, out),
        Item::Const(value) => collect_const_symbol(source, path, value, out),
        Item::Mod(value) => {
            push_symbol(
                source,
                path,
                &value.name,
                format!("mod {}", value.name),
                SymbolKind::Module,
                value.span,
                container,
                out,
            );
            if let Some(items) = &value.inline {
                for item in items {
                    collect_item_symbols(source, path, item, Some(&value.name), out);
                }
            }
        }
        Item::Macro(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("macro {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        Item::Test(value) => push_symbol(
            source,
            path,
            &value.name,
            format!("test {}", value.name),
            SymbolKind::Function,
            value.span,
            container,
            out,
        ),
        _ => {}
    }
}

fn collect_component_symbols(
    source: &str,
    path: &Path,
    component: &Component,
    out: &mut Vec<SymbolRecord>,
) {
    push_symbol(
        source,
        path,
        &component.name,
        format!("component {}", component.name),
        SymbolKind::Component,
        component.span,
        None,
        out,
    );
    for param in &component.props {
        push_param_symbol(source, path, param, Some(&component.name), out);
    }
    for state in &component.state {
        push_symbol(
            source,
            path,
            &state.name,
            format!("state {}", state.name),
            SymbolKind::Field,
            state.span,
            Some(&component.name),
            out,
        );
    }
    for method in &component.methods {
        push_function_symbol(
            source,
            path,
            method,
            SymbolKind::Method,
            Some(&component.name),
            out,
        );
    }
}

fn collect_shader_symbols(source: &str, path: &Path, shader: &Shader, out: &mut Vec<SymbolRecord>) {
    push_symbol(
        source,
        path,
        &shader.name,
        format!("shader {}", shader.name),
        SymbolKind::Shader,
        shader.span,
        None,
        out,
    );
    for input in &shader.inputs {
        push_param_symbol(source, path, input, Some(&shader.name), out);
    }
    for uniform in &shader.uniforms {
        push_symbol(
            source,
            path,
            &uniform.name,
            format!("uniform {}", uniform.name),
            SymbolKind::Field,
            uniform.span,
            Some(&shader.name),
            out,
        );
    }
}

fn collect_actor_symbols(source: &str, path: &Path, actor: &Actor, out: &mut Vec<SymbolRecord>) {
    push_symbol(
        source,
        path,
        &actor.name,
        format!("actor {}", actor.name),
        SymbolKind::Actor,
        actor.span,
        None,
        out,
    );
    for state in &actor.state {
        push_symbol(
            source,
            path,
            &state.name,
            format!("state {}", state.name),
            SymbolKind::Field,
            state.span,
            Some(&actor.name),
            out,
        );
    }
    for handler in &actor.handlers {
        push_symbol(
            source,
            path,
            &handler.message_type,
            format!("handler {}", handler.message_type),
            SymbolKind::Method,
            handler.span,
            Some(&actor.name),
            out,
        );
    }
    for method in &actor.methods {
        push_function_symbol(
            source,
            path,
            method,
            SymbolKind::Method,
            Some(&actor.name),
            out,
        );
    }
}

fn collect_struct_symbols(source: &str, path: &Path, value: &Struct, out: &mut Vec<SymbolRecord>) {
    push_symbol(
        source,
        path,
        &value.name,
        format!("struct {}", value.name),
        SymbolKind::Struct,
        value.span,
        None,
        out,
    );
    for field in &value.fields {
        collect_field_symbol(source, path, field, &value.name, out);
    }
    for method in &value.methods {
        push_function_symbol(
            source,
            path,
            method,
            SymbolKind::Method,
            Some(&value.name),
            out,
        );
    }
}

fn collect_impl_symbols(source: &str, path: &Path, value: &Impl, out: &mut Vec<SymbolRecord>) {
    let container = render_type(&value.target_type);
    for method in &value.methods {
        push_function_symbol(
            source,
            path,
            method,
            SymbolKind::Method,
            Some(&container),
            out,
        );
    }
}

fn collect_type_alias_symbol(
    source: &str,
    path: &Path,
    value: &TypeAlias,
    out: &mut Vec<SymbolRecord>,
) {
    push_symbol(
        source,
        path,
        &value.name,
        format!("type {}", value.name),
        SymbolKind::TypeAlias,
        value.span,
        None,
        out,
    );
}

fn collect_const_symbol(source: &str, path: &Path, value: &Const, out: &mut Vec<SymbolRecord>) {
    push_symbol(
        source,
        path,
        &value.name,
        format!("const {}: {}", value.name, render_type(&value.ty)),
        SymbolKind::Constant,
        value.span,
        None,
        out,
    );
}

fn collect_field_symbol(
    source: &str,
    path: &Path,
    field: &Field,
    container: &str,
    out: &mut Vec<SymbolRecord>,
) {
    push_symbol(
        source,
        path,
        &field.name,
        format!("field {}: {}", field.name, render_type(&field.ty)),
        SymbolKind::Field,
        field.span,
        Some(container),
        out,
    );
}

fn push_function_symbol(
    source: &str,
    path: &Path,
    function: &Function,
    kind: SymbolKind,
    container: Option<&str>,
    out: &mut Vec<SymbolRecord>,
) {
    let params = function
        .params
        .iter()
        .map(|param| format!("{}: {}", param.name, render_type(&param.ty)))
        .collect::<Vec<_>>()
        .join(", ");
    let ret = function
        .return_type
        .as_ref()
        .map(render_type)
        .unwrap_or_else(|| "Unit".to_string());
    push_symbol(
        source,
        path,
        &function.name,
        format!("fn {}({params}) -> {ret}", function.name),
        kind,
        function.span,
        container,
        out,
    );
    for param in &function.params {
        push_param_symbol(source, path, param, Some(&function.name), out);
    }
}

fn push_param_symbol(
    source: &str,
    path: &Path,
    param: &Param,
    container: Option<&str>,
    out: &mut Vec<SymbolRecord>,
) {
    push_symbol(
        source,
        path,
        &param.name,
        format!("param {}: {}", param.name, render_type(&param.ty)),
        SymbolKind::Variable,
        param.span,
        container,
        out,
    );
}

fn collect_block_symbols(
    source: &str,
    path: &Path,
    block: &Block,
    container: Option<&str>,
    out: &mut Vec<SymbolRecord>,
) {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Let { pattern, span, .. } => {
                if let Some(name) = pattern_name(pattern) {
                    push_symbol(
                        source,
                        path,
                        &name,
                        format!("let {name}"),
                        SymbolKind::Variable,
                        *span,
                        container,
                        out,
                    );
                }
            }
            Stmt::Item(item) => collect_item_symbols(source, path, item, container, out),
            _ => {}
        }
    }
}

fn pattern_name(pattern: &kain_core::ast::Pattern) -> Option<String> {
    match pattern {
        kain_core::ast::Pattern::Binding { name, .. } => Some(name.clone()),
        _ => None,
    }
}

fn push_symbol(
    source: &str,
    path: &Path,
    name: &str,
    detail: String,
    kind: SymbolKind,
    span: Span,
    container: Option<&str>,
    out: &mut Vec<SymbolRecord>,
) {
    let range = span_to_range(source, span);
    let name_range = name_range_for_span(source, span, name).unwrap_or(range);
    out.push(SymbolRecord {
        name: name.to_string(),
        detail,
        kind,
        path: path.display().to_string(),
        range,
        name_range,
        container: container.map(str::to_string),
    });
}

fn name_range_for_span(source: &str, span: Span, name: &str) -> Option<Range> {
    let slice = source.get(span.start.min(source.len())..span.end.min(source.len()))?;
    let local = slice.find(name)?;
    let start = span.start + local;
    let end = start + name.len();
    Some(Range {
        start: offset_to_position(source, start),
        end: offset_to_position(source, end),
    })
}

fn collect_occurrences(source: &str, path: &Path, tokens: &[Token]) -> Vec<OccurrenceRecord> {
    tokens
        .iter()
        .filter_map(|token| match &token.kind {
            TokenKind::Ident(name) => Some(OccurrenceRecord {
                name: name.clone(),
                path: path.display().to_string(),
                range: span_to_range(source, token.span),
            }),
            _ => None,
        })
        .collect()
}

fn collect_semantic_tokens(source: &str, tokens: &[Token]) -> Vec<SemanticTokenResult> {
    tokens
        .iter()
        .filter_map(|token| semantic_token_kind(&token.kind).map(|kind| (token, kind)))
        .map(|(token, token_type)| SemanticTokenResult {
            range: span_to_range(source, token.span),
            token_type,
            token_modifiers: 0,
        })
        .collect()
}

fn semantic_token_kind(kind: &TokenKind) -> Option<u32> {
    match kind {
        TokenKind::Ident(_) => Some(5),
        TokenKind::String(_) | TokenKind::FString(_) | TokenKind::Char(_) => Some(1),
        TokenKind::Int(_) | TokenKind::Float(_) => Some(2),
        TokenKind::Eof | TokenKind::Newline(_) | TokenKind::Indent | TokenKind::Dedent => None,
        _ if is_keyword_token(kind) => Some(0),
        _ => Some(3),
    }
}

fn is_keyword_token(kind: &TokenKind) -> bool {
    matches!(
        kind,
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
            | TokenKind::Defer
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
            | TokenKind::Collapse
            | TokenKind::Observe
            | TokenKind::Decay
            | TokenKind::Share
            | TokenKind::Fanout
            | TokenKind::Test
            | TokenKind::Pure
            | TokenKind::Io
            | TokenKind::AsyncKw
            | TokenKind::Async
            | TokenKind::Gpu
            | TokenKind::Reactive
            | TokenKind::Unsafe
    )
}

fn symbol_to_result(symbol: &SymbolRecord) -> SymbolResult {
    SymbolResult {
        name: symbol.name.clone(),
        detail: symbol.detail.clone(),
        kind: symbol.kind,
        location: LocationResult {
            path: symbol.path.clone(),
            range: symbol.name_range,
            name: symbol.name.clone(),
        },
        container: symbol.container.clone(),
    }
}

fn completion_kind_for_symbol(kind: SymbolKind) -> CompletionKind {
    match kind {
        SymbolKind::Function => CompletionKind::Function,
        SymbolKind::Method => CompletionKind::Method,
        SymbolKind::Struct => CompletionKind::Struct,
        SymbolKind::Enum => CompletionKind::Enum,
        SymbolKind::EnumMember => CompletionKind::EnumMember,
        SymbolKind::Trait => CompletionKind::Trait,
        SymbolKind::Field => CompletionKind::Field,
        SymbolKind::Constant => CompletionKind::Constant,
        SymbolKind::Module => CompletionKind::Module,
        SymbolKind::Actor | SymbolKind::Component | SymbolKind::Shader => CompletionKind::Type,
        SymbolKind::TypeAlias => CompletionKind::Type,
        SymbolKind::Variable => CompletionKind::Variable,
    }
}

fn matches_prefix(value: &str, lowered_prefix: &str) -> bool {
    lowered_prefix.is_empty() || value.to_ascii_lowercase().starts_with(lowered_prefix)
}

fn inventory_items() -> impl Iterator<Item = (&'static str, &'static str, CompletionKind)> {
    KEYWORD_ITEMS
        .iter()
        .map(|item| (*item, "Kain keyword", CompletionKind::Keyword))
        .chain(
            EFFECT_ITEMS
                .iter()
                .map(|item| (*item, "Kain effect", CompletionKind::Effect)),
        )
        .chain(
            TYPE_ITEMS
                .iter()
                .map(|item| (*item, "Kain built-in type", CompletionKind::Type)),
        )
        .chain(
            STDLIB_ITEMS
                .iter()
                .map(|item| (*item, "Kain stdlib item", CompletionKind::Stdlib)),
        )
}

fn render_type(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } if generics.is_empty() => name.clone(),
        Type::Named { name, generics, .. } => format!(
            "{}<{}>",
            name,
            generics
                .iter()
                .map(render_type)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Type::Tuple(items, _) => format!(
            "({})",
            items.iter().map(render_type).collect::<Vec<_>>().join(", ")
        ),
        Type::Array(inner, len, _) => format!("[{}; {}]", render_type(inner), len),
        Type::Slice(inner, _) => format!("[{}]", render_type(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", render_type(inner))
            } else {
                format!("&{}", render_type(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", render_type(inner))
            } else {
                format!("ptr<{}>", render_type(inner))
            }
        }
        Type::Function {
            params,
            return_type,
            ..
        } => format!(
            "fn({}) -> {}",
            params
                .iter()
                .map(render_type)
                .collect::<Vec<_>>()
                .join(", "),
            render_type(return_type)
        ),
        Type::Option(inner, _) => format!("Option<{}>", render_type(inner)),
        Type::Result(ok, err, _) => format!("Result<{}, {}>", render_type(ok), render_type(err)),
        Type::Impl {
            trait_name,
            generics,
            ..
        } if generics.is_empty() => {
            format!("impl {trait_name}")
        }
        Type::Impl {
            trait_name,
            generics,
            ..
        } => format!(
            "impl {}<{}>",
            trait_name,
            generics
                .iter()
                .map(render_type)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "Never".to_string(),
        Type::Unit(_) => "Unit".to_string(),
    }
}

fn range_contains(range: Range, offset: usize) -> bool {
    range.start.offset <= offset && offset <= range.end.offset
}

fn is_ident_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn is_ident_start(byte: u8) -> bool {
    byte.is_ascii_alphabetic() || byte == b'_'
}

const KEYWORD_ITEMS: &[&str] = &[
    "actor",
    "and",
    "as",
    "await",
    "axiom",
    "break",
    "capability",
    "collapse",
    "component",
    "comptime",
    "const",
    "continue",
    "converge",
    "decay",
    "defer",
    "dispatch",
    "elif",
    "else",
    "emit",
    "entangle",
    "enum",
    "every",
    "fallback",
    "false",
    "fanout",
    "fast",
    "fn",
    "for",
    "fragment",
    "from",
    "guarantee",
    "if",
    "impl",
    "import",
    "in",
    "include",
    "jitter",
    "law",
    "let",
    "loop",
    "macro",
    "match",
    "mod",
    "mut",
    "none",
    "not",
    "observe",
    "on",
    "or",
    "orchestrate",
    "patch",
    "policy",
    "pub",
    "pulse",
    "receive",
    "residency",
    "resonate",
    "return",
    "self",
    "Self",
    "send",
    "shader",
    "share",
    "shatter",
    "single_writer",
    "spawn",
    "spec",
    "stage",
    "state",
    "struct",
    "teleport",
    "test",
    "to",
    "trait",
    "transfer",
    "true",
    "type",
    "uniform",
    "use",
    "var",
    "verify",
    "vertex",
    "via",
    "while",
    "with",
    "world",
];

const EFFECT_ITEMS: &[&str] = &["Pure", "IO", "Async", "GPU", "Reactive", "Unsafe"];

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
    "len",
    "push",
    "pop",
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
    "addr_of",
    "ptr_offset",
    "mem_load",
    "mem_store",
    "sizeof_type",
    "alignof_type",
    "alloc",
    "alloc_zeroed",
    "realloc_mem",
    "decay",
    "os_getcwd",
    "os_listdir",
    "os_stat",
];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn index_collects_symbols_and_references() {
        let source =
            "fn helper(v: Int) -> Int:\n    return v\n\nfn main() -> Int:\n    return helper(4)\n";
        let path = Path::new("probe.kn");
        let analysis = analyze_document(path, source).expect("analysis");
        let mut index = WorkspaceIndex::default();
        index.insert_analysis(analysis);

        assert_eq!(index.definitions("helper").len(), 1);
        assert!(index.references("helper").len() >= 2);
        assert!(index
            .completions("hel")
            .iter()
            .any(|item| item.label == "helper"));
    }
}
