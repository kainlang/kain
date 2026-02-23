//! KAIN Parser - Python-style indentation with Rust semantics

use crate::lexer::{Lexer, Token, TokenKind};
use crate::ast::*;
use crate::span::Span;
use crate::effects::Effect;
use crate::error::{KainError, KainResult};
use crate::diagnostics::SpanMapper;

/// Reserved keywords that cannot be used as identifiers.
/// This includes KAIN keywords, HLSL keywords, C++ keywords, and UE5 macros.
/// Note: Contextual keywords like "state", "compute", "weak" are NOT included here
/// as they are only keywords in specific contexts and can be used as identifiers elsewhere.
const RESERVED_KEYWORDS: &[&str] = &[
    // KAIN core keywords (always reserved)
    "fn", "let", "mut", "var", "const", "if", "else", "elif", "match", "for", "while", "loop",
    "break", "continue", "return", "await", "in", "with", "as", "type", "struct", "enum", "trait",
    "impl", "pub", "mod", "use", "self", "Self", "true", "false", "none",
    "component", "actor", "spawn", "send", "receive", "emit", "comptime", "macro",
    "vertex", "fragment", "test",
    "Pure", "IO", "async", "Async", "GPU", "Reactive", "Unsafe",
    
    // HLSL keywords (from ue5-shaders/src/codegen_usf.rs)
    // Note: HLSL type names like RWBuffer, Texture2D, etc. are NOT reserved keywords
    // because they are only valid as type annotations, not as variable names.
    // The type system will handle validation of type names separately.
    // Note: Shader stage abbreviations (vs, ps, gs, hs, ds, cs) are NOT reserved
    // because they are only meaningful in HLSL shader profile strings, not as variable names.
    "line", "compile", "pass", "technique", "register", "packoffset",
    "typedef", "sampler", "row_major", "column_major", "out", "inout", "inline",
    "cbuffer", "tbuffer", "uniform", "precise", "volatile", "extern",
    "shared", "groupshared", "half", "min16float", "min10float",
    "min16int", "min12int", "min16uint", "interface", "namespace",
    "static", "void", "bool", "int", "uint", "float", "double",
    "float2", "float3", "float4", "int2", "int3", "int4", "uint2", "uint3", "uint4",
    "float2x2", "float3x3", "float4x4", "matrix",
    "numthreads", "SV_Position", "SV_Target", "SV_DispatchThreadID", "SV_GroupID", "SV_GroupThreadID",
    
    // C++ keywords
    "class", "virtual", "override", "final", "explicit", "operator", "template", "typename",
    "private", "protected", "public", "friend", "this", "new", "delete", "nullptr",
    "try", "catch", "throw", "noexcept", "constexpr", "decltype", "auto",
    "signed", "unsigned", "short", "long", "char", "wchar_t", "char16_t", "char32_t",
    "sizeof", "alignof", "alignas", "typeid", "dynamic_cast", "static_cast", "reinterpret_cast", "const_cast",
    "goto", "switch", "case", "default", "do", "volatile", "mutable", "register",
    "union", "asm", "export", "thread_local", "static_assert",
    
    // UE5 macros and types
    // Note: UE5 type names like FVector, TArray, etc. are NOT reserved keywords
    // because they are only valid as type annotations, not as variable names.
    // The type system will handle validation of type names separately.
    "UCLASS", "USTRUCT", "UENUM", "UFUNCTION", "UPROPERTY", "UPARAM", "UMETA",
    "GENERATED_BODY", "GENERATED_USTRUCT_BODY", "GENERATED_UCLASS_BODY",
    "UINTERFACE", "RIGVM_METHOD", "FORCEINLINE", "FORCENOINLINE",
    "TEXT", "LOCTEXT", "NSLOCTEXT", "TEXTVIEW",
];

pub struct Parser<'a> {
    tokens: &'a [Token],
    pos: usize,
    injected_tokens: Vec<Token>, // Buffer for synthetic tokens (e.g., splitting >> into > >)
    span_mapper: &'a SpanMapper,
    filename: &'a str,
    errors: Vec<KainError>,      // Accumulated parse errors for multi-error recovery
}

impl<'a> Parser<'a> {
    pub fn new(tokens: &'a [Token], span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        Self { 
            tokens, 
            pos: 0, 
            injected_tokens: Vec::new(),
            span_mapper,
            filename,
            errors: Vec::new(),
        }
    }
    
    /// Create a parser error with file:line:col format
    fn parser_error(&self, message: impl Into<String>, span: Span) -> KainError {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted_message = format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, message.into());
        KainError::parser(formatted_message, span)
    }
    
    /// Validate that an identifier is not a reserved keyword.
    /// Returns an error if the identifier conflicts with a reserved keyword.
    fn validate_identifier(&self, name: &str, span: Span) -> KainResult<()> {
        if RESERVED_KEYWORDS.contains(&name) {
            return Err(self.parser_error(
                format!(
                    "Identifier '{}' conflicts with reserved keyword. Please choose a different name.\n\
                     Reserved keywords include KAIN keywords (fn, let, struct, etc.), \
                     HLSL keywords (cbuffer, register, etc.), C++ keywords (class, virtual, etc.), \
                     and UE5 macros (UCLASS, UPROPERTY, etc.)",
                    name
                ),
                span
            ));
        }
        Ok(())
    }

    pub fn parse(&mut self) -> KainResult<Program> {
        let mut items = Vec::new();
        let mut top_level_stmts = Vec::new();
        let start = self.current_span();
        
        while !self.at_end() {
            self.skip_newlines();
            if self.at_end() { break; }
            
            match self.peek_kind() {
                // Visibility and attributes
                TokenKind::Pub | 
                TokenKind::At |
                
                // Functions and async
                TokenKind::Fn | 
                TokenKind::AsyncKw |
                
                // First-class citizens (KAIN-specific)
                TokenKind::Component | 
                TokenKind::Shader | 
                TokenKind::Actor |
                
                // Data structures
                TokenKind::Struct | 
                TokenKind::Enum |
                TokenKind::TypeKw |  // Type aliases
                
                // Traits and implementations
                TokenKind::Trait |
                TokenKind::Impl |
                
                // Module system
                TokenKind::Use |
                TokenKind::Mod |
                
                // Compile-time and macros
                TokenKind::Const | 
                TokenKind::Comptime |
                TokenKind::Macro |
                TokenKind::Test => {
                    match self.parse_item() {
                        Ok(item) => items.push(item),
                        Err(e) => {
                            self.errors.push(e);
                            self.synchronize();
                        }
                    }
                }
                
                // TODO: Future token kinds for advanced features:
                // - TokenKind::Interface (for UE5 interfaces)
                // - TokenKind::Delegate (explicit delegate declarations)
                // - TokenKind::Event (event system)
                // - TokenKind::Namespace (code organization)
                // - TokenKind::Class (if we add class keyword separate from struct)
                
                _ => {
                    match self.parse_stmt() {
                        Ok(stmt) => top_level_stmts.push(stmt),
                        Err(e) => {
                            self.errors.push(e);
                            self.synchronize();
                        }
                    }
                }
            }
        }
        
        if !top_level_stmts.is_empty() {
            let main_fn = Item::Function(Function {
                name: "main".to_string(),
                generics: vec![],
                params: vec![],
                return_type: None,
                effects: vec![],
                body: Block { stmts: top_level_stmts, span: start.merge(self.current_span()) },
                visibility: Visibility::Public,
                attributes: vec![],
                span: start.merge(self.current_span()),
            });
            items.push(main_fn);
        }
        
        // If any errors accumulated during parsing, return them all
        if !self.errors.is_empty() {
            return Err(KainError::multi(std::mem::take(&mut self.errors)));
        }

        let end = self.current_span();
        Ok(Program { items, span: start.merge(end) })
    }

    /// Skip tokens until we find a safe synchronization point (next top-level item boundary).
    /// This enables error recovery — after a parse error, we skip the broken item
    /// and resume parsing at the next recognizable top-level construct.
    fn synchronize(&mut self) {
        // First, skip past any remaining indented content to get back to indent level 0.
        // We track indent depth — when we see an item-start token at depth 0, we stop.
        let mut depth: i32 = 0;
        while !self.at_end() {
            match self.peek_kind() {
                TokenKind::Indent => { depth += 1; self.advance(); }
                TokenKind::Dedent => {
                    depth -= 1;
                    self.advance();
                    // If we've returned to the top level, check if next token is an item start
                    if depth <= 0 {
                        self.skip_newlines();
                        if self.is_item_start() || self.at_end() {
                            return;
                        }
                    }
                }
                TokenKind::Newline(_) => {
                    self.advance();
                    // At top level (depth 0), check if the next token starts a new item
                    if depth <= 0 {
                        if self.is_item_start() || self.at_end() {
                            return;
                        }
                    }
                }
                _ => {
                    // At top level and found an item start — stop
                    if depth <= 0 && self.is_item_start() {
                        return;
                    }
                    self.advance();
                }
            }
        }
    }

    /// Check if the current token could start a new top-level item.
    fn is_item_start(&self) -> bool {
        matches!(self.peek_kind(),
            TokenKind::At | TokenKind::Fn | TokenKind::Struct | TokenKind::Enum |
            TokenKind::Actor | TokenKind::Component | TokenKind::Shader |
            TokenKind::Pub | TokenKind::Const | TokenKind::Mod | TokenKind::Use |
            TokenKind::Impl | TokenKind::Macro | TokenKind::Test |
            TokenKind::AsyncKw | TokenKind::TypeKw | TokenKind::Trait |
            TokenKind::Comptime
        )
    }

    fn parse_item(&mut self) -> KainResult<Item> {
        // Collect any @attr decorators first
        let attributes = self.parse_attributes()?;
        
        // Check for @material_graph attribute
        if attributes.iter().any(|a| a.name == "material_graph") {
            return self.parse_material_graph(attributes);
        }
        
        // Check for @material_function attribute
        if attributes.iter().any(|a| a.name == "material_function") {
            return self.parse_material_function(attributes);
        }
        
        // Check for @graph_editor attribute
        if attributes.iter().any(|a| a.name == "graph_editor") {
            return self.parse_graph_editor(attributes);
        }
        
        // Check for @graph_runtime attribute
        if attributes.iter().any(|a| a.name == "graph_runtime") {
            return self.parse_graph_runtime(attributes);
        }
        
        // Check for @state_machine attribute
        if attributes.iter().any(|a| a.name == "state_machine") {
            return self.parse_state_machine(attributes);
        }
        
        // Check for @editor_module attribute
        if attributes.iter().any(|a| a.name == "editor_module") {
            return self.parse_editor_module(attributes);
        }
        
        let vis = self.parse_visibility();
        
        match self.peek_kind() {
            TokenKind::Fn => self.parse_function_with_attrs(vis, attributes),
            TokenKind::AsyncKw => self.parse_async_function(vis),
            TokenKind::Component => self.parse_component_with_attrs(vis, attributes),
            TokenKind::Shader => self.parse_shader(),
            TokenKind::Struct => self.parse_struct_with_attrs(vis, attributes),
            TokenKind::Enum => self.parse_enum(vis),
            TokenKind::Actor => self.parse_actor_with_attrs(attributes),
            TokenKind::Const => self.parse_const(vis),
            TokenKind::Comptime => self.parse_comptime_block(),
            TokenKind::Macro => self.parse_macro(),
            TokenKind::Test => self.parse_test(),
            TokenKind::Use => self.parse_use(),
            // TokenKind::Trait => self.parse_trait(vis), // TODO: Agent 4 will implement this
            TokenKind::Impl => self.parse_impl(),
            TokenKind::TypeKw => self.parse_type_alias(vis),
            _ => Err(self.parser_error("Expected item", self.current_span())),
        }
    }

    // Parse @wasm, @js, @inline etc decorators
    fn parse_attributes(&mut self) -> KainResult<Vec<Attribute>> {
        let mut attrs = Vec::new();
        while self.check(TokenKind::At) {
            let start = self.current_span();
            self.advance(); // consume @
            let name = self.parse_attribute_name()?;
            
            // Optional args: @attr(arg1, arg2) or @attr(name: value, name2: value2)
            let args = if self.check(TokenKind::LParen) {
                self.advance();
                let mut arg_list = Vec::new();
                while !self.check(TokenKind::RParen) && !self.at_end() {
                    // Check if this is a named argument (name: value)
                    if let TokenKind::Ident(param_name) = self.peek_kind() {
                        let saved_pos = self.pos;
                        self.advance(); // consume identifier
                        
                        if self.check(TokenKind::Colon) {
                            // This is a named argument - represent as a tuple (name, value)
                            self.advance(); // consume colon
                            let value = self.parse_expr()?;
                            
                            // Create a tuple expression to represent name: value
                            let name_expr = Expr::Ident(param_name.clone(), self.current_span());
                            arg_list.push(Expr::Tuple(vec![name_expr, value], self.current_span()));
                        } else {
                            // Not a named argument, restore position and parse as normal expression
                            self.pos = saved_pos;
                            arg_list.push(self.parse_expr()?);
                        }
                    } else {
                        // Not an identifier, parse as normal expression
                        arg_list.push(self.parse_expr()?);
                    }
                    
                    if !self.check(TokenKind::RParen) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::RParen)?;
                arg_list
            } else {
                vec![]
            };
            
            attrs.push(Attribute { name, args, span: start.merge(self.current_span()) });
            self.skip_newlines();
        }
        Ok(attrs)
    }

    fn parse_attribute_name(&mut self) -> KainResult<String> {
        match self.peek_kind() {
            TokenKind::Ident(s) => { self.advance(); Ok(s) }
            TokenKind::Component => { self.advance(); Ok("component".to_string()) }
            TokenKind::Shader => { self.advance(); Ok("shader".to_string()) }
            TokenKind::Actor => { self.advance(); Ok("actor".to_string()) }
            TokenKind::State => { self.advance(); Ok("state".to_string()) }
            TokenKind::AsyncKw => { self.advance(); Ok("async".to_string()) }
            TokenKind::Async => { self.advance(); Ok("Async".to_string()) }
            TokenKind::Gpu => { self.advance(); Ok("GPU".to_string()) }
            TokenKind::Reactive => { self.advance(); Ok("Reactive".to_string()) }
            k => Err(self.parser_error(format!("Expected attribute name, got {:?}", k), self.current_span())),
        }
    }

    fn parse_trait(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Trait)?;
        let name = self.parse_ident()?;
        
        // Parse generics: trait Foo<T>
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Parse trait method signature
            self.expect(TokenKind::Fn)?;
            let method_name = self.parse_ident()?;
            
            self.expect(TokenKind::LParen)?;
            let params = self.parse_params()?;
            self.expect(TokenKind::RParen)?;
            
            let return_type = if self.check(TokenKind::Arrow) {
                self.advance();
                Some(self.parse_type()?)
            } else {
                None
            };
            
            let effects = self.parse_effects()?;
            
            // Check for default implementation
            let default_impl = if self.check(TokenKind::Colon) {
                self.advance();
                Some(self.parse_block()?)
            } else {
                None
            };
            
            methods.push(TraitMethod {
                name: method_name,
                params,
                return_type,
                effects,
                default_impl,
                span: self.current_span(),
            });
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::Trait(Trait {
            name,
            generics,
            methods,
            visibility: vis,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_impl(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Impl)?;
        
        // Parse impl-level generics: impl<T>
        let generics = self.parse_generics()?;
        
        // Check for "TraitName for" pattern to support "impl Trait for Type"
        let trait_name = if matches!(self.peek_kind(), TokenKind::Ident(_)) {
            // Look ahead to see if there's a "for" keyword
            let saved_pos = self.pos;
            let potential_trait = self.parse_ident()?;
            
            if self.check(TokenKind::For) {
                // This is "impl Trait for Type" syntax
                self.advance(); // consume "for"
                Some(potential_trait)
            } else {
                // This is just "impl Type" syntax, backtrack
                self.pos = saved_pos;
                None
            }
        } else {
            None
        };
        
        // Parse target type: Option<T>
        let target_type = self.parse_type()?;
        
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let vis = self.parse_visibility();
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(vis)? {
                    methods.push(f);
                }
            } else {
                return Err(self.parser_error("Expected fn in impl block", self.current_span()));
            }
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::Impl(Impl {
            generics,
            trait_name,
            target_type,
            methods,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_use(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Use)?;
        
        let mut path = Vec::new();
        path.push(self.parse_ident()?);
        
        // Parse path: use foo::bar::baz OR use foo/bar/baz
        while self.check(TokenKind::ColonColon) || self.check(TokenKind::Slash) {
            self.advance();
            
            // Check for glob: use foo::*
            if self.check(TokenKind::Star) {
                self.advance();
                return Ok(Item::Use(Use { 
                    path, 
                    alias: None, 
                    glob: true, 
                    span: start.merge(self.current_span()) 
                }));
            }
            
            path.push(self.parse_ident()?);
        }
        
        // Check for alias: use foo::bar as baz
        let alias = if self.check(TokenKind::As) {
            self.advance();
            Some(self.parse_ident()?)
        } else {
            None
        };
        
        Ok(Item::Use(Use { 
            path, 
            alias, 
            glob: false, 
            span: start.merge(self.current_span()) 
        }))
    }

    fn parse_test(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Test)?;
        // Tests can have a string name or identifier
        let name = if let TokenKind::String(s) = self.peek_kind() {
            self.advance();
            s
        } else {
            self.parse_ident()?
        };
        
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Item::Test(TestDef { name, body, span: start.merge(self.current_span()) }))
    }

    fn parse_macro(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Macro)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Not)?; // macro name!
        self.expect(TokenKind::LParen)?;
        
        let mut params = Vec::new();
        while !self.check(TokenKind::RParen) {
            let p_name = self.parse_ident()?;
            self.expect(TokenKind::Colon)?;
            let kind_name = self.parse_ident()?;
            let kind = match kind_name.as_str() {
                "expr" => MacroParamKind::Expr,
                "type" => MacroParamKind::Type,
                "ident" => MacroParamKind::Ident,
                "block" => MacroParamKind::Block,
                "token" => MacroParamKind::Token,
                _ => return Err(self.parser_error("Unknown macro param kind", self.current_span())),
            };
            params.push(MacroParam { name: p_name, kind, span: self.current_span() });
            
            if !self.check(TokenKind::RParen) {
                self.expect(TokenKind::Comma)?;
            }
        }
        self.expect(TokenKind::RParen)?;
        self.expect(TokenKind::Colon)?;
        
        let body = self.parse_block()?;
        
        Ok(Item::Macro(MacroDef {
            name,
            params,
            body: MacroBody::Block(body),
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_function(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Fn)?;
        let name = self.parse_ident()?;
        
        // Parse generics: <T, U: Bound>
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else { None };
        
        let effects = self.parse_effects()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;
        
        Ok(Item::Function(Function {
            name, generics, params, return_type, effects, body, visibility: vis,
            attributes: vec![],
            span: start.merge(body_span),
        }))
    }

    // Wrapper to parse function with pre-collected attributes
    fn parse_function_with_attrs(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Fn)?;
        let name = self.parse_ident()?;
        let generics = self.parse_generics()?;
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else { None };
        let effects = self.parse_effects()?;
        
        // Check if this is an extern function (no body)
        let is_extern = attrs.iter().any(|a| a.name == "extern");
        
        let body = if is_extern && !self.check(TokenKind::Colon) {
            // Extern function without body - create empty block
            Block {
                stmts: vec![],
                span: self.current_span(),
            }
        } else {
            // Regular function with body
            self.expect(TokenKind::Colon)?;
            self.parse_block()?
        };
        
        let body_span = body.span;
        Ok(Item::Function(Function {
            name, generics, params, return_type, effects, body, visibility: vis,
            attributes: attrs,
            span: start.merge(body_span),
        }))
    }

    fn parse_async_function(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::AsyncKw)?; // consume 'async'
        self.expect(TokenKind::Fn)?;     // consume 'fn'
        let name = self.parse_ident()?;
        
        // Parse generics: <T, U: Bound>
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else { None };
        
        // Parse other effects, then add Async
        let mut effects = self.parse_effects()?;
        effects.push(crate::effects::Effect::Async);
        
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;
        
        Ok(Item::Function(Function {
            name, generics, params, return_type, effects, body, visibility: vis,
            attributes: vec![],
            span: start.merge(body_span),
        }))
    }
    fn parse_component(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Component)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let props = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let effects = self.parse_effects()?;
        self.expect(TokenKind::Colon)?;
        
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut state = Vec::new();
        let mut methods = Vec::new();
        let mut body = None;
        
        // Parse component body items
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            if self.check(TokenKind::Fn) {
                // Parse method
                if let Item::Function(f) = self.parse_function(Visibility::Private)? {
                    methods.push(f);
                }
            } else if let TokenKind::Ident(ref s) = self.peek_kind() {
                if s == "state" {
                    self.advance();
                    let name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    self.expect(TokenKind::Eq)?;
                    let initial = self.parse_expr()?;
                    state.push(StateDecl { name, ty, initial, weak: false, attributes: vec![], span: self.current_span() });
                } else if s == "weak" {
                     self.advance();
                     if self.check(TokenKind::Ident("state".to_string())) { // Check specifically for state
                         // "weak state name: Type = ..."
                         self.advance();
                         let name = self.parse_ident()?;
                         self.expect(TokenKind::Colon)?;
                         let ty = self.parse_type()?;
                         self.expect(TokenKind::Eq)?;
                         let initial = self.parse_expr()?;
                         state.push(StateDecl { name, ty, initial, weak: true, attributes: vec![], span: self.current_span() });
                     } else {
                         return Err(self.parser_error("Expected 'state' after 'weak' in component", self.current_span()));
                     }
                } else if s == "render" {
                     self.advance();
                     if self.check(TokenKind::LBrace) {
                         // render { jsx }
                         self.advance();
                         self.skip_newlines();
                         body = Some(self.parse_jsx_element()?);
                         self.skip_newlines();
                         self.expect(TokenKind::RBrace)?;
                     } else if self.check(TokenKind::Colon) {
                         // render:
                         //    jsx
                         self.advance();
                         self.skip_newlines();
                         self.expect(TokenKind::Indent)?;
                         self.skip_newlines();
                         body = Some(self.parse_jsx_element()?);
                         self.skip_newlines();
                         self.expect(TokenKind::Dedent)?;
                     } else {
                         // render <jsx>
                         body = Some(self.parse_jsx_element()?);
                     }
                } else {
                    return Err(self.parser_error(format!("Unexpected identifier in component: {}", s), self.current_span()));
                }
            } else if self.check(TokenKind::Lt) {
                // Direct JSX element (implicit render)
                body = Some(self.parse_jsx_element()?);
            } else {
                return Err(self.parser_error(format!("Unexpected token in component: {:?}", self.peek_kind()), self.current_span()));
            }
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) { self.advance(); }
        
        let body = body.ok_or_else(|| self.parser_error("Component must have a render body (JSX element)", self.current_span()))?;
        
        Ok(Item::Component(Component {
            name, props, state, methods, effects, body, visibility: vis,
            attributes: vec![],
            span: start.merge(self.current_span()),
        }))
    }

    // Wrapper to parse component with pre-collected attributes  
    fn parse_component_with_attrs(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Component)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let props = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let effects = self.parse_effects()?;
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut state = Vec::new();
        let mut methods = Vec::new();
        let mut body = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(Visibility::Private)? {
                    methods.push(f);
                }
            } else if let TokenKind::Ident(ref s) = self.peek_kind() {
                if s == "state" {
                    self.advance();
                    let name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    self.expect(TokenKind::Eq)?;
                    let initial = self.parse_expr()?;
                    state.push(StateDecl { name, ty, initial, weak: false, attributes: vec![], span: self.current_span() });
                } else if s == "render" {
                    self.advance();
                    if self.check(TokenKind::Colon) {
                        self.advance();
                        self.skip_newlines();
                        self.expect(TokenKind::Indent)?;
                        self.skip_newlines();
                        body = Some(self.parse_jsx_element()?);
                        self.skip_newlines();
                        self.expect(TokenKind::Dedent)?;
                    } else {
                        body = Some(self.parse_jsx_element()?);
                    }
                } else {
                    return Err(self.parser_error(format!("Unexpected identifier in component: {}", s), self.current_span()));
                }
            } else if self.check(TokenKind::Lt) {
                body = Some(self.parse_jsx_element()?);
            } else {
                return Err(self.parser_error(format!("Unexpected token in component: {:?}", self.peek_kind()), self.current_span()));
            }
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) { self.advance(); }
        let body = body.ok_or_else(|| self.parser_error("Component must have a render body", self.current_span()))?;
        
        Ok(Item::Component(Component {
            name, props, state, methods, effects, body, visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_shader(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Shader)?;
        
        let stage = if self.check(TokenKind::Vertex) {
            self.advance(); ShaderStage::Vertex
        } else if self.check(TokenKind::Fragment) {
            self.advance(); ShaderStage::Fragment
        } else if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s == "compute" {
                self.advance(); ShaderStage::Compute
            } else {
                ShaderStage::Fragment // Default
            }
        } else {
            ShaderStage::Fragment // Default
        };

        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let inputs = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        self.expect(TokenKind::Arrow)?;
        let outputs = self.parse_type()?;
        self.expect(TokenKind::Colon)?;
        
        // Manual block parsing to support uniforms
        self.skip_newlines();
        let block_start = self.current_span();
        self.expect(TokenKind::Indent)?;

        let mut uniforms = Vec::new();
        let mut stmts = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }

            // Check for "uniform" identifier
            let is_uniform = if let TokenKind::Ident(ref s) = self.peek_kind() {
                s == "uniform"
            } else {
                false
            };

            if is_uniform {
                self.advance(); // consume "uniform"
                let u_name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let u_ty = self.parse_type()?;
                self.expect(TokenKind::At)?;
                
                // Parse integer binding
                let binding = if let TokenKind::Int(n) = self.peek_kind() {
                    self.advance();
                    n as u32
                } else {
                    return Err(self.parser_error("Expected integer binding", self.current_span()));
                };

                uniforms.push(Uniform { name: u_name, ty: u_ty, binding, span: self.current_span() });
            } else {
                stmts.push(self.parse_stmt()?);
            }
            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) { self.advance(); }
        let body = Block { stmts, span: block_start.merge(self.current_span()) };
        let body_span = body.span;
        
        Ok(Item::Shader(Shader {
            name, stage, inputs, outputs, uniforms, body,
            span: start.merge(body_span),
        }))
    }

    fn parse_struct_with_attrs(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Struct)?;
        let name = self.parse_ident()?;
        
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut fields = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let f_attrs = self.parse_attributes()?;
            
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(Visibility::Public)? {
                    methods.push(f);
                }
                self.skip_newlines();
                continue;
            }
            
            // Check for weak
            let weak = if let TokenKind::Ident(s) = self.peek_kind() {
                if s == "weak" {
                    self.advance();
                    true
                } else { false }
            } else { false };
            
            let fname = self.parse_ident()?;
            self.expect(TokenKind::Colon)?;
            let ty = self.parse_type()?;
            
            // Check for default value
            let default = if self.check(TokenKind::Eq) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
            
            fields.push(Field { 
                name: fname, 
                ty, 
                attributes: f_attrs, 
                visibility: Visibility::Public, 
                default, 
                weak, 
                span: self.current_span() 
            });
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) { self.advance(); }
        
        Ok(Item::Struct(Struct { 
            name, 
            generics, 
            fields, 
            methods,
            attributes: attrs, 
            visibility: vis, 
            span: start.merge(self.current_span()) 
        }))
    }

    fn parse_enum(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Enum)?;
        let name = self.parse_ident()?;
        
        // Parse generics: enum Option<T>:
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut variants = Vec::new();
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            let start_span = self.current_span();
            let vname = self.parse_ident()?;
            
            let fields = if self.check(TokenKind::LParen) {
                self.advance(); // consume (
                let mut types = Vec::new();
                while !self.check(TokenKind::RParen) && !self.at_end() {
                    types.push(self.parse_type()?);
                    if !self.check(TokenKind::RParen) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::RParen)?;
                VariantFields::Tuple(types)
            } else if self.check(TokenKind::LBrace) {
                self.advance(); // consume {
                self.skip_newlines();
                let indented = if self.check(TokenKind::Indent) { self.advance(); true } else { false };
                
                let mut fields = Vec::new();
                while !self.check(TokenKind::RBrace) && !self.at_end() {
                    if indented && self.check(TokenKind::Dedent) { break; }
                    if !indented && self.check(TokenKind::RBrace) { break; }
                    
                    self.skip_newlines();
                    if self.check(TokenKind::RBrace) || (indented && self.check(TokenKind::Dedent)) { break; }
                    
                    let f_attrs = self.parse_attributes()?;
                    let fname = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    
                    fields.push(Field { 
                        name: fname, 
                        ty, 
                        attributes: f_attrs,
                        visibility: Visibility::Public, 
                        default: None, 
                        weak: false, 
                        span: self.current_span() 
                    });
                    
                    if !self.check(TokenKind::RBrace) {
                         if self.check(TokenKind::Comma) { self.advance(); }
                    }
                    self.skip_newlines();
                }
                
                if indented { self.expect(TokenKind::Dedent)?; }
                self.expect(TokenKind::RBrace)?;
                VariantFields::Struct(fields)
            } else {
                VariantFields::Unit
            };
            
            // If the next token is a newline/dedent, previous token end is the end of the variant
            // For now, using current_span (next token) is consistent with previous code's behavior for Unit variants
            // but for Tuple/Struct it's better to span the whole thing.
            // Let's use start_span (ident) merged with current_span (after fields).
            let span = if matches!(fields, VariantFields::Unit) {
                 // If Unit, current_span is the one after ident. 
                 // If we used start_span, it would be the ident.
                 // Let's just use start_span for Unit to correct the "bug" of using next token.
                 // But wait, start_span is the Ident span.
                 start_span
            } else {
                 // For fields, we consumed ) or }. current_span is the one after that.
                 // We want to merge start_span with the end of the fields.
                 // But current_span() points to the *next* token.
                 // We can use start_span for the start.
                 // For the end, it's a bit tricky without keeping track of the last consumed token.
                 // We'll just use start_span.merge(self.current_span()) which covers [Ident ... NextToken].
                 // That's acceptable.
                 start_span.merge(self.current_span())
            };

            variants.push(Variant { name: vname, fields, span });
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) { self.advance(); }
        
        Ok(Item::Enum(Enum { name, generics, variants, visibility: vis, span: start.merge(self.current_span()) }))
    }

    fn parse_actor(&mut self) -> KainResult<Item> {
        self.parse_actor_with_attrs(vec![])
    }
    
    fn parse_actor_with_attrs(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Actor)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut state = Vec::new();
        let mut handlers = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Parse attributes first (for methods)
            let method_attributes = self.parse_attributes()?;
            
            // Check for "state", "var", "on", or "fn"
            if self.check(TokenKind::State) {
                self.advance();
                let name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Eq)?;
                let initial = self.parse_expr()?;
                state.push(StateDecl { name, ty, initial, weak: false, attributes: method_attributes, span: self.current_span() });
            } else if self.check(TokenKind::Var) {
                // Support 'var' as alias for 'state' in actors
                self.advance();
                let name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Eq)?;
                let initial = self.parse_expr()?;
                state.push(StateDecl { name, ty, initial, weak: false, attributes: method_attributes, span: self.current_span() });
            } else if self.check(TokenKind::Fn) {
                // Parse method function with pre-parsed attributes
                if let Item::Function(func) = self.parse_function_with_attrs(Visibility::Public, method_attributes)? {
                    methods.push(func);
                }
            } else if let TokenKind::Ident(s) = self.peek_kind() {
                if s == "weak" {
                    self.advance();
                    self.expect(TokenKind::State)?;
                    let name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    self.expect(TokenKind::Eq)?;
                    let initial = self.parse_expr()?;
                    state.push(StateDecl { name, ty, initial, weak: true, attributes: method_attributes, span: self.current_span() });
                } else if s == "on" {
                    self.advance();
                    let message_type = self.parse_ident()?;
                    self.expect(TokenKind::LParen)?;
                    let params = self.parse_params()?;
                    self.expect(TokenKind::RParen)?;
                    self.expect(TokenKind::Colon)?;
                    let body = self.parse_block()?;
                    handlers.push(MessageHandler { message_type, params, body, span: self.current_span() });
                } else {
                     return Err(self.parser_error(format!("Unexpected item in actor: {}", s), self.current_span()));
                }
            } else {
                 return Err(self.parser_error("Expected 'state', 'var', 'fn', or 'on' in actor definition.", self.current_span()));
            }
            
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) { self.advance(); }
        
        let span = start.merge(self.current_span());
        Ok(Item::Actor(Actor { name, state, handlers, methods, attributes, span }))
    }

    fn parse_const(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Const)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        let ty = self.parse_type()?;
        self.expect(TokenKind::Eq)?;
        let value = self.parse_expr()?;
        Ok(Item::Const(Const { name, ty, value, visibility: vis, span: start.merge(self.current_span()) }))
    }

    fn parse_type_alias(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::TypeKw)?;
        let name = self.parse_ident()?;
        
        // Optional generics: type Foo<T>
        let generics = self.parse_generics()?;
        
        self.expect(TokenKind::Eq)?;
        let target = self.parse_type()?;
        
        Ok(Item::TypeAlias(TypeAlias { 
            name, 
            generics, 
            target, 
            visibility: vis, 
            span: start.merge(self.current_span()) 
        }))
    }

    fn parse_comptime_block(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Comptime)?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Item::Comptime(ComptimeBlock { body, span: start.merge(self.current_span()) }))
    }

    fn parse_material_graph(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'material' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "material" {
                return Err(self.parser_error("Expected 'material' keyword after @material_graph", self.current_span()));
            }
            self.advance(); // consume 'material'
        } else {
            return Err(self.parser_error("Expected 'material' keyword after @material_graph", self.current_span()));
        }
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect ':'
        self.expect(TokenKind::Colon)?;
        
        // Parse body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut inputs = Vec::new();
        let mut body = Vec::new();
        let mut outputs = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for "input", "let", or "output"
            if let TokenKind::Ident(ref s) = self.peek_kind() {
                match s.as_str() {
                    "input" => {
                        self.advance(); // consume 'input'
                        
                        let input_name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;
                        let ty = self.parse_type()?;
                        
                        let default = if self.check(TokenKind::Eq) {
                            self.advance();
                            Some(self.parse_expr()?)
                        } else {
                            None
                        };
                        
                        inputs.push(MaterialInput {
                            name: input_name,
                            ty,
                            default,
                            span: self.current_span(),
                        });
                    }
                    "output" => {
                        self.advance(); // consume 'output'
                        
                        let output_name = self.parse_ident()?;
                        self.expect(TokenKind::Eq)?;
                        let value = self.parse_expr()?;
                        
                        outputs.push(MaterialOutput {
                            name: output_name,
                            value,
                            span: self.current_span(),
                        });
                    }
                    _ => {
                        return Err(self.parser_error(
                            format!("Unexpected identifier in material graph: {}. Expected 'input', 'let', or 'output'", s),
                            self.current_span()
                        ));
                    }
                }
            } else if self.check(TokenKind::Let) {
                self.advance(); // consume 'let'
                
                let var_name = self.parse_ident()?;
                self.expect(TokenKind::Eq)?;
                let value = self.parse_expr()?;
                
                body.push(MaterialStatement::Let {
                    name: var_name,
                    value,
                    span: self.current_span(),
                });
            } else {
                return Err(self.parser_error(
                    "Expected 'input', 'let', or 'output' in material graph body",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::MaterialGraph(MaterialGraphDef {
            name,
            attributes,
            inputs,
            body,
            outputs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_material_function(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'fn' keyword — note: 'fn' is lexed as TokenKind::Fn, NOT Ident("fn")
        if self.check(TokenKind::Fn) {
            self.advance(); // consume 'fn'
        } else {
            return Err(self.parser_error("Expected 'fn' keyword after @material_function", self.current_span()));
        }
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect '('
        self.expect(TokenKind::LParen)?;
        
        // Parse inputs (function parameters)
        let mut inputs = Vec::new();
        while !self.check(TokenKind::RParen) && !self.at_end() {
            let input_name = self.parse_ident()?;
            self.expect(TokenKind::Colon)?;
            let ty = self.parse_type()?;
            
            let default = if self.check(TokenKind::Eq) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
            
            inputs.push(MaterialInput {
                name: input_name,
                ty,
                default,
                span: self.current_span(),
            });
            
            if !self.check(TokenKind::RParen) {
                self.expect(TokenKind::Comma)?;
            }
        }
        
        self.expect(TokenKind::RParen)?;
        
        // Expect ':'
        self.expect(TokenKind::Colon)?;
        
        // Parse body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut body = Vec::new();
        let mut output: Option<Expr> = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for "let" or "return"
            if self.check(TokenKind::Let) {
                self.advance(); // consume 'let'
                
                let var_name = self.parse_ident()?;
                self.expect(TokenKind::Eq)?;
                let value = self.parse_expr()?;
                
                body.push(MaterialStatement::Let {
                    name: var_name,
                    value,
                    span: self.current_span(),
                });
            } else if self.check(TokenKind::Return) {
                self.advance(); // consume 'return'
                output = Some(self.parse_expr()?);
                break; // return must be last statement
            } else {
                return Err(self.parser_error(
                    "Expected 'let' or 'return' in material function body",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        // Drain any newlines emitted after the 'return' statement (the break
        // above skips the loop's own trailing skip_newlines call) and then
        // consume the indented block's Dedent token.
        self.skip_newlines();
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        let output = output.ok_or_else(|| {
            self.parser_error("Material function must have a 'return' statement", self.current_span())
        })?;
        
        Ok(Item::MaterialFunction(MaterialFunctionDef {
            name,
            attributes,
            inputs,
            body,
            output,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_graph_editor(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'graph' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "graph" {
                return Err(self.parser_error("Expected 'graph' keyword after @graph_editor", self.current_span()));
            }
            self.advance(); // consume 'graph'
        } else {
            return Err(self.parser_error("Expected 'graph' keyword after @graph_editor", self.current_span()));
        }
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse node types and schema
        let mut node_types = Vec::new();
        let mut schema = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for @node_type or @schema
            let node_attrs = self.parse_attributes()?;
            
            if node_attrs.iter().any(|a| a.name == "node_type") {
                node_types.push(self.parse_node_type(node_attrs)?);
            } else if node_attrs.iter().any(|a| a.name == "schema") {
                schema = Some(self.parse_graph_schema(node_attrs)?);
            } else {
                return Err(self.parser_error("Expected @node_type or @schema in graph editor", self.current_span()));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::GraphEditor(GraphEditorDef {
            name,
            attributes,
            node_types,
            schema,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_node_type(&mut self, attributes: Vec<Attribute>) -> KainResult<NodeTypeDef> {
        let start = self.current_span();
        
        // Expect 'node' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "node" {
                return Err(self.parser_error("Expected 'node' keyword after @node_type", self.current_span()));
            }
            self.advance();
        } else {
            return Err(self.parser_error("Expected 'node' keyword after @node_type", self.current_span()));
        }
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Extract category from attributes
        let category = attributes.iter()
            .find(|a| a.name == "category")
            .and_then(|a| a.args.first())
            .and_then(|arg| {
                if let Expr::String(s, _) = arg {
                    Some(s.clone())
                } else {
                    None
                }
            });
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse inputs, outputs, properties
        let mut inputs = Vec::new();
        let mut outputs = Vec::new();
        let mut properties = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for section keywords
            if let TokenKind::Ident(ref s) = self.peek_kind() {
                match s.as_str() {
                    "inputs" => {
                        self.advance();
                        self.expect(TokenKind::Colon)?;
                        inputs = self.parse_pin_list()?;
                    }
                    "outputs" => {
                        self.advance();
                        self.expect(TokenKind::Colon)?;
                        outputs = self.parse_pin_list()?;
                    }
                    "properties" => {
                        self.advance();
                        self.expect(TokenKind::Colon)?;
                        properties = self.parse_property_list()?;
                    }
                    _ => {
                        return Err(self.parser_error("Expected 'inputs', 'outputs', or 'properties'", self.current_span()));
                    }
                }
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(NodeTypeDef {
            name,
            category,
            inputs,
            outputs,
            properties,
            attributes,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_pin_list(&mut self) -> KainResult<Vec<PinDef>> {
        let mut pins = Vec::new();
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let pin_attrs = self.parse_attributes()?;
            let pin_start = self.current_span();
            
            // Parse pin name
            let name = self.parse_ident()?;
            
            // Expect colon
            self.expect(TokenKind::Colon)?;
            
            // Parse type
            let ty = self.parse_type()?;
            
            // Check for array syntax by inspecting the Type enum
            let is_array = matches!(&ty, Type::Named { name, .. } if name == "Array");
            
            // Check for default value
            let default = if self.check(TokenKind::Eq) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
            
            pins.push(PinDef {
                name,
                ty,
                is_array,
                default,
                attributes: pin_attrs,
                span: pin_start.merge(self.current_span()),
            });
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(pins)
    }

    fn parse_property_list(&mut self) -> KainResult<Vec<PropertyDef>> {
        let mut properties = Vec::new();
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let prop_attrs = self.parse_attributes()?;
            let prop_start = self.current_span();
            
            // Parse property name
            let name = self.parse_ident()?;
            
            // Expect colon
            self.expect(TokenKind::Colon)?;
            
            // Parse type
            let ty = self.parse_type()?;
            
            // Check for default value
            let default = if self.check(TokenKind::Eq) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
            
            properties.push(PropertyDef {
                name,
                ty,
                default,
                attributes: prop_attrs,
                span: prop_start.merge(self.current_span()),
            });
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(properties)
    }

    fn parse_graph_schema(&mut self, _attributes: Vec<Attribute>) -> KainResult<GraphSchemaDef> {
        let start = self.current_span();
        
        // Expect 'schema' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "schema" {
                return Err(self.parser_error("Expected 'schema' keyword after @schema", self.current_span()));
            }
            self.advance();
        }
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse rules (simplified for now)
        let mut rules = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Parse rule name
            let rule_name = self.parse_ident()?;
            
            // Expect colon
            self.expect(TokenKind::Colon)?;
            
            // Parse condition expression
            let condition = self.parse_expr()?;
            
            rules.push(SchemaRule {
                name: rule_name,
                condition,
                span: start.merge(self.current_span()),
            });
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(GraphSchemaDef {
            rules,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_params(&mut self) -> KainResult<Vec<Param>> {
        let mut params = Vec::new();
        self.skip_newlines();
        while !self.check(TokenKind::RParen) && !self.at_end() {
            let mutable = if self.check(TokenKind::Mut) {
                self.advance();
                true
            } else {
                false
            };

            let name = self.parse_ident()?;
            let ty = if self.check(TokenKind::Colon) {
                self.advance();
                self.parse_type()?
            } else {
                Type::Infer(self.current_span())
            };
            
            // Check for default value: param: Type = value
            let default = if self.check(TokenKind::Eq) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
            
            params.push(Param { name, ty, mutable, default, span: self.current_span() });
            
            self.skip_newlines();
            if !self.check(TokenKind::RParen) { 
                self.expect(TokenKind::Comma)?; 
                self.skip_newlines();
            }
        }
        Ok(params)
    }

    /// Parse generic type parameters: <T, U: Bound, V>
    fn parse_generics(&mut self) -> KainResult<Vec<Generic>> {
        let mut generics = Vec::new();
        
        // Check for opening <
        if !self.check(TokenKind::Lt) {
            return Ok(generics);
        }
        self.advance(); // consume <
        
        while !self.check(TokenKind::Gt) && !self.at_end() {
            let start = self.current_span();
            let name = self.parse_ident()?;
            
            // Parse optional bounds: T: Bound1 + Bound2
            let mut bounds = Vec::new();
            if self.check(TokenKind::Colon) {
                self.advance(); // consume :
                loop {
                    let bound_name = self.parse_ident()?;
                    bounds.push(TypeBound { trait_name: bound_name, span: self.current_span() });
                    if !self.check(TokenKind::Plus) { break; }
                    self.advance(); // consume +
                }
            }
            
            generics.push(Generic { name, bounds, span: start.merge(self.current_span()) });
            
            if !self.check(TokenKind::Gt) {
                self.expect(TokenKind::Comma)?;
            }
        }
        
        self.expect(TokenKind::Gt)?; // consume >
        Ok(generics)
    }

    fn parse_effects(&mut self) -> KainResult<Vec<Effect>> {
        let mut effects = Vec::new();
        if self.check(TokenKind::With) {
            self.advance();
            loop {
                // Effects are keywords, not identifiers
                let effect = match self.peek_kind() {
                    TokenKind::Pure => { self.advance(); Some(Effect::Pure) }
                    TokenKind::Io => { self.advance(); Some(Effect::IO) }
                    TokenKind::Async => { self.advance(); Some(Effect::Async) }
                    TokenKind::Gpu => { self.advance(); Some(Effect::GPU) }
                    TokenKind::Reactive => { self.advance(); Some(Effect::Reactive) }
                    TokenKind::Unsafe => { self.advance(); Some(Effect::Unsafe) }
                    TokenKind::Ident(ref s) => {
                        let e = Effect::from_str(s);
                        self.advance();
                        e
                    }
                    _ => None,
                };
                if let Some(e) = effect {
                    effects.push(e);
                }
                if !self.check(TokenKind::Comma) { break; }
                self.advance();
            }
        }
        Ok(effects)
    }

    fn parse_type(&mut self) -> KainResult<Type> {
        let span = self.current_span();
        
        // Handle tuple types: (A, B) or unit type: ()
        if self.check(TokenKind::LParen) {
            self.advance(); // consume (
            
            // Check for unit type ()
            if self.check(TokenKind::RParen) {
                self.advance(); // consume )
                return Ok(Type::Unit(span.merge(self.current_span())));
            }
            
            // Parse tuple elements
            let mut elements = Vec::new();
            elements.push(self.parse_type()?);
            
            while self.check(TokenKind::Comma) {
                self.advance(); // consume ,
                if self.check(TokenKind::RParen) { break; } // trailing comma
                elements.push(self.parse_type()?);
            }
            
            self.expect(TokenKind::RParen)?;
            return Ok(Type::Tuple(elements, span.merge(self.current_span())));
        }
        
        // Handle impl Trait: impl Future, impl Iterator<Item = T>
        if self.check(TokenKind::Impl) {
            self.advance(); // consume impl
            let trait_name = self.parse_ident()?;
            
            // Parse generic arguments if present
            let mut generics = Vec::new();
            if self.check(TokenKind::Lt) {
                self.advance(); // consume <
                while !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) && !self.at_end() {
                    generics.push(self.parse_type()?);
                    if !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                
                // Handle >> for nested generics
                if self.check(TokenKind::Shr) {
                    self.advance();
                } else {
                    self.expect(TokenKind::Gt)?;
                }
            }
            
            return Ok(Type::Impl {
                trait_name,
                generics,
                span: span.merge(self.current_span()),
            });
        }
        
        // Handle function types: fn(T, U) -> R
        if self.check(TokenKind::Fn) {
            self.advance(); // consume fn
            self.expect(TokenKind::LParen)?;
            
            // Parse parameter types
            let mut params = Vec::new();
            while !self.check(TokenKind::RParen) && !self.at_end() {
                params.push(self.parse_type()?);
                if !self.check(TokenKind::RParen) {
                    self.expect(TokenKind::Comma)?;
                }
            }
            self.expect(TokenKind::RParen)?;
            
            // Parse return type (optional)
            let return_type = if self.check(TokenKind::Arrow) {
                self.advance(); // consume ->
                Box::new(self.parse_type()?)
            } else {
                Box::new(Type::Unit(span))
            };
            
            return Ok(Type::Function {
                params,
                return_type,
                effects: vec![],
                span: span.merge(self.current_span()),
            });
        }
        
        // Handle delegate types: delegate(T, U) - same as fn but for UE5 delegates
        // This is syntactic sugar for function types used as delegates
        let mut name = self.parse_ident()?;
        
        // Check if this is delegate(...) syntax
        if name == "delegate" && self.check(TokenKind::LParen) {
            self.advance(); // consume (
            
            // Parse parameter types
            let mut params = Vec::new();
            while !self.check(TokenKind::RParen) && !self.at_end() {
                params.push(self.parse_type()?);
                if !self.check(TokenKind::RParen) {
                    self.expect(TokenKind::Comma)?;
                }
            }
            self.expect(TokenKind::RParen)?;
            
            // Delegates don't have return types (they're void)
            let return_type = Box::new(Type::Unit(span));
            
            return Ok(Type::Function {
                params,
                return_type,
                effects: vec![],
                span: span.merge(self.current_span()),
            });
        }
        
        // Support Module::Type syntax
        while self.check(TokenKind::ColonColon) {
            self.advance(); // consume ::
            let part = self.parse_ident()?;
            name.push_str("::");
            name.push_str(&part);
        }
        
        // Parse generic type arguments: Type<T, U>
        let mut type_args = Vec::new();
        if self.check(TokenKind::Lt) {
            self.advance(); // consume <
            while !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) && !self.at_end() {
                type_args.push(self.parse_type()?);
                if !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) {
                    self.expect(TokenKind::Comma)?;
                }
            }
            
            // Handle >> token for nested generics like Box<Box<Int>>
            // The >> is lexed as a single Shr token, but should be treated as > >
            // When we see >>, we consume it but inject a synthetic > token for the outer level
            if self.check(TokenKind::Shr) {
                // Split the >> into two > tokens
                // Consume the >> and replace it with a single >
                let shr_span = self.current_span();
                self.advance(); // consume >>
                
                // Insert a synthetic > token at the current position
                // This allows the outer generic parser to close properly
                self.inject_token(Token::new(TokenKind::Gt, shr_span));
            } else {
                self.expect(TokenKind::Gt)?; // consume >
            }
        }
        
        Ok(Type::Named { name, generics: type_args, span })
    }

    fn parse_block(&mut self) -> KainResult<Block> {
        self.skip_newlines();
        let start = self.current_span();
        self.expect(TokenKind::Indent)?;
        
        let mut stmts = Vec::new();
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            stmts.push(self.parse_stmt()?);
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) { self.advance(); }
        
        Ok(Block { stmts, span: start.merge(self.current_span()) })
    }

    fn parse_stmt(&mut self) -> KainResult<Stmt> {
        match self.peek_kind() {
            TokenKind::Let => self.parse_let(),
            TokenKind::Var => self.parse_var(),
            TokenKind::Return => self.parse_return(),
            TokenKind::For => self.parse_for(),
            TokenKind::While => self.parse_while(),
            TokenKind::Loop => self.parse_loop(),
            TokenKind::Break => self.parse_break(),
            TokenKind::Continue => self.parse_continue(),
            _ => Ok(Stmt::Expr(self.parse_expr()?)),
        }
    }

    fn parse_let(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Let)?;
        let pattern = self.parse_pattern()?;
        let ty = if self.check(TokenKind::Colon) { self.advance(); Some(self.parse_type()?) } else { None };
        self.expect(TokenKind::Eq)?;
        let value = Some(self.parse_expr()?);
        Ok(Stmt::Let { pattern, ty, value, span: start.merge(self.current_span()) })
    }

    fn parse_var(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Var)?;
        let name = self.parse_ident()?;
        let ty = if self.check(TokenKind::Colon) { self.advance(); Some(self.parse_type()?) } else { None };
        self.expect(TokenKind::Eq)?;
        let value = Some(self.parse_expr()?);
        // var x = val is effectively let mut x = val
        let pattern = Pattern::Binding { name, mutable: true, span: start };
        Ok(Stmt::Let { pattern, ty, value, span: start.merge(self.current_span()) })
    }

    fn parse_return(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Return)?;
        let value = if !self.check_line_end() { Some(self.parse_expr()?) } else { None };
        Ok(Stmt::Return(value, start.merge(self.current_span())))
    }

    fn parse_for(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::For)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::In)?;
        let iter = self.parse_expr()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::For { binding: Pattern::Binding { name, mutable: false, span: start }, iter, body, span: start.merge(self.current_span()) })
    }

    fn parse_while(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::While)?;
        let condition = self.parse_expr()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::While { condition, body, span: start.merge(self.current_span()) })
    }

    fn parse_loop(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Loop)?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::Loop { body, span: start.merge(self.current_span()) })
    }

    fn parse_break(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Break)?;
        // Optional value: break expr
        let value = if !self.check_line_end() && !self.check(TokenKind::Dedent) {
            Some(self.parse_expr()?)
        } else {
            None
        };
        Ok(Stmt::Break(value, start.merge(self.current_span())))
    }

    fn parse_continue(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Continue)?;
        Ok(Stmt::Continue(start.merge(self.current_span())))
    }
    fn parse_expr(&mut self) -> KainResult<Expr> { self.parse_assignment() }

    fn parse_assignment(&mut self) -> KainResult<Expr> {
        let expr = self.parse_binary(0)?;
        
        if self.check(TokenKind::Eq) {
            self.advance();
            let value = self.parse_assignment()?;
            let span = expr.span().merge(value.span());
            Ok(Expr::Assign { target: Box::new(expr), value: Box::new(value), span })
        } else {
            Ok(expr)
        }
    }

    fn parse_binary(&mut self, min_prec: u8) -> KainResult<Expr> {
        let mut left = self.parse_unary()?;
        
        while let Some((op, prec)) = self.get_binary_op() {
            if prec < min_prec { break; }
            self.advance();
            let right = self.parse_binary(prec + 1)?;
            let span = left.span().merge(right.span());
            left = Expr::Binary { left: Box::new(left), op, right: Box::new(right), span };
        }
        Ok(left)
    }

    fn parse_unary(&mut self) -> KainResult<Expr> {
        match self.peek_kind() {
            TokenKind::Minus => { let s = self.current_span(); self.advance(); Ok(Expr::Unary { op: UnaryOp::Neg, operand: Box::new(self.parse_unary()?), span: s }) }
            TokenKind::Not => { let s = self.current_span(); self.advance(); Ok(Expr::Unary { op: UnaryOp::Not, operand: Box::new(self.parse_unary()?), span: s }) }
            TokenKind::Await => {
                let start = self.current_span();
                self.advance();
                let expr = self.parse_unary()?; // Right-associative: await await x
                Ok(Expr::Await(Box::new(expr), start.merge(self.current_span())))
            }
            TokenKind::Send => {
                let start = self.current_span();
                self.advance();
                let expr = self.parse_postfix()?;
                
                if let Expr::Call { callee, args, span } = expr {
                    if let Expr::Field { object, field, span: _ } = *callee {
                        let mut data = Vec::new();
                        for arg in args {
                            if let Some(name) = arg.name {
                                data.push((name, arg.value));
                            } else {
                                return Err(self.parser_error("Send requires named arguments", arg.span));
                            }
                        }
                        Ok(Expr::SendMsg { target: object, message: field, data, span: start.merge(span) })
                    } else {
                        Err(self.parser_error("Expected method call after send (e.g., actor.message())", span))
                    }
                } else {
                    Err(self.parser_error("Expected message call after send", expr.span()))
                }
            }
            _ => self.parse_postfix(),
        }
    }

    fn parse_postfix(&mut self) -> KainResult<Expr> {
        let mut expr = self.parse_primary()?;
        loop {
            match self.peek_kind() {
                TokenKind::LParen => { 
                    self.advance(); 
                    let args = self.parse_call_args()?; 
                    self.expect(TokenKind::RParen)?; 
                    let s = expr.span().merge(self.current_span()); 
                    
                    // Check if this looks like struct initialization with named arguments
                    // Pattern: TypeName(field = val, ...) where TypeName starts with uppercase
                    if let Expr::Ident(name, ident_span) = &expr {
                        // Check if identifier starts with uppercase (likely a type name)
                        let starts_with_uppercase = name.chars().next()
                            .map(|c| c.is_uppercase())
                            .unwrap_or(false);
                        
                        // Check if all arguments are named (using = syntax in KAIN)
                        let all_named = !args.is_empty() && args.iter().all(|arg| arg.name.is_some());
                        
                        if starts_with_uppercase && all_named {
                            // This looks like struct initialization - emit error
                            return Err(self.parser_error(
                                format!(
                                    "Struct initialization with named arguments is not supported in KAIN. Found '{}(...)'.\n\
                                     Use field-by-field assignment instead:\n\
                                     \n\
                                     Example:\n\
                                       let obj = {}()\n\
                                       obj.field1 = value1\n\
                                       obj.field2 = value2",
                                    name, name
                                ),
                                s
                            ));
                        }
                    }
                    
                    if let Expr::Field { object, field, span: _ } = expr {
                        expr = Expr::MethodCall { receiver: object, method: field, args, span: s };
                    } else {
                        expr = Expr::Call { callee: Box::new(expr), args, span: s }; 
                    }
                }
                TokenKind::Dot => { self.advance(); let field = self.parse_ident()?; let s = expr.span().merge(self.current_span()); expr = Expr::Field { object: Box::new(expr), field, span: s }; }
            TokenKind::As => {
                self.advance();
                let target = self.parse_type()?;
                let s = expr.span().merge(self.current_span());
                expr = Expr::Cast { value: Box::new(expr), target, span: s };
            }
            TokenKind::LBracket => { self.advance(); let idx = self.parse_expr()?; self.expect(TokenKind::RBracket)?; let s = expr.span().merge(self.current_span()); expr = Expr::Index { object: Box::new(expr), index: Box::new(idx), span: s }; }
            TokenKind::Question => { self.advance(); let s = expr.span().merge(self.current_span()); expr = Expr::Try(Box::new(expr), s); }
            TokenKind::Not => {
                // Macro invocation: ident!(args)
                if let Expr::Ident(name, _) = &expr {
                    self.advance(); // consume '!'
                    self.expect(TokenKind::LParen)?;
                    let args = if !self.check(TokenKind::RParen) {
                        let mut args = Vec::new();
                        args.push(self.parse_expr()?);
                        while self.check(TokenKind::Comma) {
                            self.advance();
                            if self.check(TokenKind::RParen) { break; }
                            args.push(self.parse_expr()?);
                        }
                        args
                    } else {
                        Vec::new()
                    };
                    self.expect(TokenKind::RParen)?;
                    let s = expr.span().merge(self.current_span());
                    expr = Expr::MacroCall { name: name.clone(), args, span: s };
                } else {
                     // Maybe unary not? But we are in postfix. Unary not is handled in parse_unary.
                     // Postfix ! usually means macro or maybe future features (like factorial?).
                     // For now, only support macros on identifiers.
                     return Err(self.parser_error("Macro invocation only allowed on identifiers", self.current_span()));
                }
            }
            _ => break,
            }
        }
        Ok(expr)
    }

    fn parse_primary(&mut self) -> KainResult<Expr> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Int(n) => { self.advance(); Ok(Expr::Int(n, span)) }
            TokenKind::Float(n) => { self.advance(); Ok(Expr::Float(n, span)) }
            TokenKind::String(ref s) => { let s = s.clone(); self.advance(); Ok(Expr::String(s, span)) }
            TokenKind::FString(ref s) => {
                let s = s.clone();
                self.advance();
                let mut parts = Vec::new();
                let mut last_idx = 0;
                let mut chars = s.char_indices().peekable();
                
                while let Some((idx, c)) = chars.next() {
                    if c == '{' {
                        if idx > last_idx {
                            parts.push(Expr::String(s[last_idx..idx].to_string(), span));
                        }
                        
                        let expr_start = idx + 1;
                        let mut depth = 1;
                        let mut expr_end = expr_start;
                        
                        while let Some((i, c2)) = chars.next() {
                            if c2 == '{' { depth += 1; }
                            else if c2 == '}' {
                                depth -= 1;
                                if depth == 0 {
                                    expr_end = i;
                                    break;
                                }
                            }
                        }
                        
                        if depth == 0 {
                            let expr_str = &s[expr_start..expr_end];
                            let tokens = Lexer::new(expr_str).tokenize()?;
                            let expr_span_mapper = SpanMapper::new(expr_str);
                            let mut parser = Parser::new(&tokens, &expr_span_mapper, "<f-string>");
                            let expr = parser.parse_expr()?;
                            parts.push(expr);
                            last_idx = expr_end + 1;
                        } else {
                             return Err(self.parser_error("Unclosed '{' in f-string", span));
                        }
                    }
                }
                
                if last_idx < s.len() {
                    parts.push(Expr::String(s[last_idx..].to_string(), span));
                }
                
                Ok(Expr::FString(parts, span))
            }
            TokenKind::True => { self.advance(); Ok(Expr::Bool(true, span)) }
            TokenKind::False => { self.advance(); Ok(Expr::Bool(false, span)) }
            TokenKind::None => { self.advance(); Ok(Expr::None(span)) }
            TokenKind::Ident(ref s) => { 
                let name = s.clone(); 
                self.advance();

                if self.check(TokenKind::ColonColon) {
                    self.advance();
                    let variant = self.parse_ident()?;

                    let fields = if self.check(TokenKind::LParen) {
                        self.advance();
                        self.skip_newlines();
                        let mut items = Vec::new();
                        if !self.check(TokenKind::RParen) {
                            items.push(self.parse_expr()?);
                            while self.check(TokenKind::Comma) {
                                self.advance();
                                if self.check(TokenKind::RParen) {
                                    break;
                                }
                                items.push(self.parse_expr()?);
                            }
                        }
                        self.expect(TokenKind::RParen)?;
                        if items.is_empty() {
                            EnumVariantFields::Unit
                        } else {
                            EnumVariantFields::Tuple(items)
                        }
                    } else if self.check(TokenKind::LBrace) {
                        self.advance();
                        let mut fields = Vec::new();

                        self.skip_newlines();
                        let indented = if self.check(TokenKind::Indent) {
                            self.advance();
                            true
                        } else {
                            false
                        };

                        while !self.check(TokenKind::RBrace) && !self.at_end() {
                            if indented && self.check(TokenKind::Dedent) {
                                break;
                            }

                            let field_name = self.parse_ident()?;
                            self.expect(TokenKind::Colon)?;
                            let field_value = self.parse_expr()?;
                            fields.push((field_name, field_value));

                            if !self.check(TokenKind::RBrace) && (!indented || !self.check(TokenKind::Dedent)) {
                                if self.check(TokenKind::Comma) {
                                    self.advance();
                                }
                            }
                            self.skip_newlines();
                        }

                        if indented {
                            self.expect(TokenKind::Dedent)?;
                        }
                        self.expect(TokenKind::RBrace)?;
                        EnumVariantFields::Struct(fields)
                    } else {
                        EnumVariantFields::Unit
                    };

                    return Ok(Expr::EnumVariant {
                        enum_name: name,
                        variant,
                        fields,
                        span: span.merge(self.current_span()),
                    });
                }
                
                // Check if this is a struct literal: Name { field: value, ... }
                // KAIN does not support struct literals - always emit error when we see Ident {
                // Skip formatting (newlines and indents) to handle multi-line struct literals
                let saved_pos_for_check = self.pos;
                self.skip_formatting();
                
                if self.check(TokenKind::LBrace) {
                    // Restore position before emitting error
                    self.pos = saved_pos_for_check;
                    
                    // This looks like a struct literal - emit error
                    return Err(self.parser_error(
                        format!(
                            "Struct literal syntax is not supported in KAIN. Found '{} {{ ... }}'.\n\
                             Use field-by-field assignment instead:\n\
                             \n\
                             Example:\n\
                               let obj = {}()\n\
                               obj.field1 = value1\n\
                               obj.field2 = value2",
                            name, name
                        ),
                        span
                    ));
                }
                
                // Restore position if not a struct literal
                self.pos = saved_pos_for_check;
                
                // Just an identifier
                Ok(Expr::Ident(name, span))
            }
            TokenKind::SelfLower => { 
                self.advance(); 
                Ok(Expr::Ident("self".to_string(), span)) 
            }
            TokenKind::SelfUpper => { 
                self.advance(); 
                Ok(Expr::Ident("Self".to_string(), span)) 
            }
            TokenKind::LParen => { 
                self.advance(); 
                if self.check(TokenKind::RParen) {
                    self.advance();
                    Ok(Expr::Tuple(vec![], span.merge(self.current_span())))
                } else {
                    let first = self.parse_expr()?;
                    if self.check(TokenKind::Comma) {
                        self.advance();
                        let mut items = vec![first];
                        while !self.check(TokenKind::RParen) {
                            items.push(self.parse_expr()?);
                            if !self.check(TokenKind::RParen) { self.expect(TokenKind::Comma)?; }
                        }
                        self.expect(TokenKind::RParen)?;
                        Ok(Expr::Tuple(items, span.merge(self.current_span())))
                    } else {
                        self.expect(TokenKind::RParen)?;
                        Ok(Expr::Paren(Box::new(first), span.merge(self.current_span())))
                    }
                }
            }
            TokenKind::LBracket => { 
                self.advance();
                self.skip_newlines();
                
                // Check for indent (multi-line array)
                let indented = if self.check(TokenKind::Indent) {
                    self.advance();
                    true
                } else {
                    false
                };
                
                let mut items = vec![];
                while !self.check(TokenKind::RBracket) && !self.at_end() {
                    if indented && self.check(TokenKind::Dedent) {
                        break;
                    }
                    self.skip_newlines();
                    if self.check(TokenKind::RBracket) {
                        break;
                    }
                    items.push(self.parse_expr()?);
                    self.skip_newlines();
                    if !self.check(TokenKind::RBracket) && !self.check(TokenKind::Dedent) { 
                        if self.check(TokenKind::Comma) {
                            self.advance();
                            self.skip_newlines();
                        }
                    }
                }
                
                if indented {
                    if self.check(TokenKind::Dedent) {
                        self.advance();
                    }
                }
                self.skip_newlines();
                self.expect(TokenKind::RBracket)?; 
                Ok(Expr::Array(items, span)) 
            }
            TokenKind::Comptime => {
                self.advance();
                self.expect(TokenKind::Colon)?;
                let body = self.parse_block()?;
                Ok(Expr::Comptime(Box::new(Expr::Block(body, span)), span))
            }
            TokenKind::Pipe => {
                self.advance();
                let mut params = Vec::new();
                while !self.check(TokenKind::Pipe) {
                    let name = self.parse_ident()?;
                    params.push(Param {
                        name,
                        ty: Type::Infer(span),
                        mutable: false,
                        default: None,
                        span,
                    });
                    if !self.check(TokenKind::Pipe) { self.expect(TokenKind::Comma)?; }
                }
                self.expect(TokenKind::Pipe)?;
                let body = self.parse_expr()?;
                Ok(Expr::Lambda { params, return_type: None, body: Box::new(body), span: span.merge(self.current_span()) })
            }
            TokenKind::Match => self.parse_match(),
            TokenKind::Spawn => {
                self.advance();
                let actor = self.parse_ident()?;
                self.expect(TokenKind::LParen)?;
                let args = self.parse_call_args()?;
                self.expect(TokenKind::RParen)?;
                
                let mut init = Vec::new();
                for arg in args {
                    if let Some(name) = arg.name {
                        init.push((name, arg.value));
                    } else {
                         return Err(self.parser_error("Spawn requires named arguments", arg.span));
                    }
                }
                Ok(Expr::Spawn { actor, init, span: span.merge(self.current_span()) })
            }
            TokenKind::Return => {
                let start = self.current_span();
                self.advance();
                let value = if !self.check_line_end() 
                    && !self.check(TokenKind::Comma) 
                    && !self.check(TokenKind::RParen) 
                    && !self.check(TokenKind::RBrace) 
                    && !self.check(TokenKind::RBracket) 
                {
                    Some(Box::new(self.parse_expr()?))
                } else {
                    None
                };
                Ok(Expr::Return(value, start.merge(self.current_span())))
            }
            TokenKind::If => self.parse_if(),
            TokenKind::Lt => {
                let jsx = self.parse_jsx_element()?;
                Ok(Expr::JSX(jsx, span.merge(self.current_span())))
            }
            // Lambda with fn syntax: fn(x: Int) -> Int: return x * 2
            // or fn(x: Int): return x * 2
            TokenKind::Fn => {
                self.advance(); // consume fn
                self.expect(TokenKind::LParen)?;
                
                // Parse parameters with types
                let mut params = Vec::new();
                while !self.check(TokenKind::RParen) && !self.at_end() {
                    let p_span = self.current_span();
                    let name = self.parse_ident()?;
                    
                    // Parse type annotation
                    let ty = if self.check(TokenKind::Colon) {
                        self.advance();
                        self.parse_type()?
                    } else {
                        Type::Infer(p_span)
                    };
                    
                    params.push(Param {
                        name,
                        ty,
                        mutable: false,
                        default: None,
                        span: p_span,
                    });
                    
                    if !self.check(TokenKind::RParen) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::RParen)?;
                
                // Parse optional return type
                let return_type = if self.check(TokenKind::Arrow) {
                    self.advance();
                    Some(self.parse_type()?)
                } else {
                    None
                };
                
                self.expect(TokenKind::Colon)?;
                
                // Parse body - can be a single expression or a block
                let body = if self.check(TokenKind::Return) {
                    // Single return statement: fn(x): return x * 2
                    self.advance();
                    self.parse_expr()?
                } else if self.check(TokenKind::Indent) || self.check_newline() {
                    // Block body (multi-line lambda)
                    self.skip_newlines();
                    let block = self.parse_block()?;
                    Expr::Block(block, span)
                } else {
                    // Single expression: fn(x): x * 2
                    self.parse_expr()?
                };
                
                Ok(Expr::Lambda { 
                    params, 
                    return_type, 
                    body: Box::new(body), 
                    span: span.merge(self.current_span()) 
                })
            }
            // Control flow as expressions (for use in match arms, etc.)
            TokenKind::Continue => {
                self.advance();
                // Continue as an expression wraps in a block that continues
                Ok(Expr::Continue(span))
            }
            TokenKind::Break => {
                self.advance();
                // Optional break value
                let value = if !self.check_line_end() && !self.check(TokenKind::Dedent) 
                    && !self.check(TokenKind::Comma) && !self.check(TokenKind::RParen) {
                    Some(Box::new(self.parse_expr()?))
                } else {
                    None
                };
                Ok(Expr::Break(value, span))
            }
            _ => Err(self.parser_error(format!("Unexpected token: {:?}", self.peek_kind()), span)),
        }
    }

    fn parse_match(&mut self) -> KainResult<Expr> {
        let start = self.current_span();
        self.expect(TokenKind::Match)?;
        let scrutinee = Box::new(self.parse_expr()?);
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        let mut arms = Vec::new();
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            let arm_start = self.current_span();
            let pattern = self.parse_pattern()?;
            self.expect(TokenKind::FatArrow)?;
            
            // Parse arm body - check if it starts with newline (multi-line body)
            let body = if matches!(self.peek_kind(), TokenKind::Newline(_)) {
                // Multi-line match arm body
                self.skip_newlines();
                
                if self.check(TokenKind::Indent) {
                    // It's an indented block - parse statements until dedent
                    self.advance(); // consume Indent
                    let mut stmts = Vec::new();
                    
                    while !self.check(TokenKind::Dedent) && !self.at_end() {
                        self.skip_newlines();
                        if self.check(TokenKind::Dedent) { break; }
                        stmts.push(self.parse_stmt()?);
                        self.skip_newlines();
                    }
                    
                    if self.check(TokenKind::Dedent) { 
                        self.advance(); // consume Dedent for arm body
                    }
                    
                    // Convert stmts to expression
                    if stmts.len() == 1 {
                        if let Stmt::Expr(e) = &stmts[0] {
                            e.clone()
                        } else if let Stmt::Return(Some(ref e), _) = &stmts[0] {
                            e.clone()
                        } else {
                            let block = Block { stmts, span: arm_start.merge(self.current_span()) };
                            Expr::Block(block, arm_start.merge(self.current_span()))
                        }
                    } else {
                        let block = Block { stmts, span: arm_start.merge(self.current_span()) };
                        Expr::Block(block, arm_start.merge(self.current_span()))
                    }
                } else {
                    // Just an expression on the next line (no indent)
                    self.parse_expr()?
                }
            } else {
                // Inline expression (same line as =>)
                self.parse_expr()?
            };
            
            arms.push(MatchArm { pattern, guard: None, body, span: self.current_span() });
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) { self.advance(); }
        Ok(Expr::Match { scrutinee, arms, span: start.merge(self.current_span()) })
    }

    fn parse_if(&mut self) -> KainResult<Expr> {
        let start = self.current_span();
        self.expect(TokenKind::If)?;
        let condition = Box::new(self.parse_expr()?);
        self.expect(TokenKind::Colon)?;
        
        // Check if this is an inline if (no newline/indent) or block if
        let is_block = matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent);
        let then_branch = if is_block {
            self.parse_block()?
        } else {
            // Inline if: parse single statement
            let stmt = self.parse_stmt()?;
            Block { stmts: vec![stmt], span: start.merge(self.current_span()) }
        };
        
        let else_branch = if self.check(TokenKind::Else) {
            self.advance();
            
            // Check for 'else if' (elif pattern) - no colon between else and if
            if self.check(TokenKind::If) {
                // Parse the 'if' expression
                let elif_expr = self.parse_if()?;
                
                // Extract the condition, then_branch, and else_branch from the If expression
                if let Expr::If { condition, then_branch, else_branch: nested_else, .. } = elif_expr {
                    Some(Box::new(ElseBranch::ElseIf(condition, then_branch, nested_else)))
                } else {
                    // Shouldn't happen, but fallback
                    return Err(self.parser_error("Expected if expression after else", self.current_span()));
                }
            } else {
                self.expect(TokenKind::Colon)?;
                let is_block = matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent);
                if is_block {
                    Some(Box::new(ElseBranch::Else(self.parse_block()?)))
                } else {
                    let stmt = self.parse_stmt()?;
                    Some(Box::new(ElseBranch::Else(Block { stmts: vec![stmt], span: start.merge(self.current_span()) })))
                }
            }
        } else { None };
        Ok(Expr::If { condition, then_branch, else_branch, span: start.merge(self.current_span()) })
    }

    fn parse_pattern(&mut self) -> KainResult<Pattern> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(ref s) if s == "_" => { self.advance(); Ok(Pattern::Wildcard(span)) }
            TokenKind::Ident(ref s) => { 
                let name = s.clone(); 
                self.advance();
                
                // Validate identifier if it's used as a binding (not an enum name)
                // We'll validate after determining if it's a binding or enum reference
                
                if self.check(TokenKind::ColonColon) {
                    // This is an enum name, not a binding, so no validation needed here
                    self.advance(); // consume ::
                    let variant = self.parse_ident()?;
                    
                    let fields = if self.check(TokenKind::LParen) {
                        self.advance();
                        let mut patterns = Vec::new();
                        while !self.check(TokenKind::RParen) {
                            patterns.push(self.parse_pattern()?);
                            if !self.check(TokenKind::RParen) {
                                self.expect(TokenKind::Comma)?;
                            }
                        }
                        self.expect(TokenKind::RParen)?;
                        VariantPatternFields::Tuple(patterns)
                    } else if self.check(TokenKind::LBrace) {
                        self.advance();
                        let mut fields = Vec::new();
                        while !self.check(TokenKind::RBrace) {
                            let fname = self.parse_ident()?;
                            self.expect(TokenKind::Colon)?;
                            let pat = self.parse_pattern()?;
                            fields.push((fname, pat));
                            if !self.check(TokenKind::RBrace) {
                                self.expect(TokenKind::Comma)?;
                            }
                        }
                        self.expect(TokenKind::RBrace)?;
                        VariantPatternFields::Struct(fields)
                    } else {
                        VariantPatternFields::Unit
                    };
                    
                    Ok(Pattern::Variant {
                        enum_name: Some(name),
                        variant,
                        fields,
                        span: span.merge(self.current_span()),
                    })
                } else if self.check(TokenKind::LParen) {
                    // Unqualified variant pattern: Variant(args) without EnumName::
                    // Common in Python-style pattern matching
                    self.advance(); // consume (
                    let mut patterns = Vec::new();
                    while !self.check(TokenKind::RParen) {
                        patterns.push(self.parse_pattern()?);
                        if !self.check(TokenKind::RParen) {
                            self.expect(TokenKind::Comma)?;
                        }
                    }
                    self.expect(TokenKind::RParen)?;
                    
                    Ok(Pattern::Variant {
                        enum_name: None, // Unqualified - will be resolved at type-check time
                        variant: name,
                        fields: VariantPatternFields::Tuple(patterns),
                        span: span.merge(self.current_span()),
                    })
                } else {
                    // This is a binding, validate it
                    self.validate_identifier(&name, span)?;
                    Ok(Pattern::Binding { name, mutable: false, span }) 
                }
            }
            TokenKind::Mut => {
                self.advance();
                let name = self.parse_ident()?;
                Ok(Pattern::Binding { name, mutable: true, span: span.merge(self.current_span()) })
            }
            TokenKind::Int(n) => { self.advance(); Ok(Pattern::Literal(Expr::Int(n, span))) }
            TokenKind::String(ref s) => { 
                let string_val = s.clone();
                self.advance(); 
                Ok(Pattern::Literal(Expr::String(string_val, span))) 
            }
            TokenKind::True => { self.advance(); Ok(Pattern::Literal(Expr::Bool(true, span))) }
            TokenKind::False => { self.advance(); Ok(Pattern::Literal(Expr::Bool(false, span))) }
            TokenKind::LParen => {
                self.advance();
                let mut patterns = Vec::new();
                while !self.check(TokenKind::RParen) {
                    patterns.push(self.parse_pattern()?);
                    if !self.check(TokenKind::RParen) { self.expect(TokenKind::Comma)?; }
                }
                self.expect(TokenKind::RParen)?;
                Ok(Pattern::Tuple(patterns, span.merge(self.current_span())))
            }
            TokenKind::LBracket => {
                self.advance();
                let mut patterns = Vec::new();
                while !self.check(TokenKind::RBracket) {
                    patterns.push(self.parse_pattern()?);
                    if !self.check(TokenKind::RBracket) { self.expect(TokenKind::Comma)?; }
                }
                self.expect(TokenKind::RBracket)?;
                Ok(Pattern::Slice { patterns, rest: None, span: span.merge(self.current_span()) })
            }
            _ => Err(self.parser_error("Expected pattern", span)),
        }
    }

    #[allow(dead_code)]
    fn parse_jsx(&mut self) -> KainResult<JSXNode> {
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        self.skip_newlines();
        let result = self.parse_jsx_element()?;
        self.skip_newlines();
        if self.check(TokenKind::Dedent) { self.advance(); }
        Ok(result)
    }

    fn parse_jsx_element(&mut self) -> KainResult<JSXNode> {
        let start = self.current_span();
        self.expect(TokenKind::Lt)?;
        let tag = self.parse_ident()?;
        let mut attrs = Vec::new();
        while !self.check(TokenKind::Gt) && !self.check(TokenKind::Slash) {
            let name = self.parse_ident()?;
            self.expect(TokenKind::Eq)?;
            let value = if self.check(TokenKind::LBrace) {
                self.advance();
                let e = self.parse_expr()?;
                self.expect(TokenKind::RBrace)?;
                JSXAttrValue::Expr(e)
            } else if let TokenKind::String(s) = self.peek_kind() {
                self.advance();
                JSXAttrValue::String(s)
            } else {
                return Err(self.parser_error("Expected attribute value", self.current_span()));
            };
            attrs.push(JSXAttribute { name, value, span: self.current_span() });
        }
        
        if self.check(TokenKind::Slash) {
            self.advance();
            self.expect(TokenKind::Gt)?;
            return Ok(JSXNode::Element { tag, attributes: attrs, children: vec![], span: start.merge(self.current_span()) });
        }
        
        self.expect(TokenKind::Gt)?;
        
        let mut children = Vec::new();
        // Track the end of the previous token to detect gaps (whitespace)
        let mut last_end = self.tokens.get(self.pos - 1).map(|t| t.span.end).unwrap_or(0);
        let mut text_buffer = String::new();
        let mut text_start = self.current_span();

        while !self.check(TokenKind::LtSlash) && !self.at_end() {
            let current_span = self.current_span();
            
            // Check for gap (whitespace)
            if current_span.start > last_end {
                // If we have text in buffer, append space. If buffer empty, maybe leading space?
                // For simplicity, just append space if buffer not empty, or if we want to preserve spacing.
                // But JSX usually collapses whitespace.
                // However, "Count is: {count}" needs space.
                // Let's unconditionally add space if gap detected, but handle collapse later?
                // No, let's just add space.
                if !text_buffer.is_empty() {
                    text_buffer.push(' ');
                }
            }

            if self.check(TokenKind::LBrace) {
                if !text_buffer.is_empty() {
                    children.push(JSXNode::Text(text_buffer.clone(), text_start.merge(Span::new(last_end, last_end))));
                    text_buffer.clear();
                }

                self.advance();
                let expr = self.parse_expr()?;
                self.expect(TokenKind::RBrace)?;
                children.push(JSXNode::Expression(Box::new(expr)));
                
                last_end = self.tokens.get(self.pos - 1).map(|t| t.span.end).unwrap_or(0);
                text_start = self.current_span(); // Reset text start for next text run
            } else if self.check(TokenKind::Lt) {
                 if !text_buffer.is_empty() {
                    children.push(JSXNode::Text(text_buffer.clone(), text_start.merge(Span::new(last_end, last_end))));
                    text_buffer.clear();
                }

                 children.push(self.parse_jsx_element()?);
                 
                 last_end = self.tokens.get(self.pos - 1).map(|t| t.span.end).unwrap_or(0);
                 text_start = self.current_span();
            } else {
                 let mut consumed_text = None;
                 match self.peek_kind() {
                     TokenKind::String(s) => consumed_text = Some(s),
                     TokenKind::Ident(s) => consumed_text = Some(s),
                     TokenKind::Int(n) => consumed_text = Some(n.to_string()),
                     TokenKind::Newline(_) | TokenKind::Indent | TokenKind::Dedent => {
                         // Treat newline/indent as whitespace
                         if !text_buffer.is_empty() && !text_buffer.ends_with(' ') {
                             text_buffer.push(' ');
                         }
                         self.advance();
                     }
                     TokenKind::Colon => consumed_text = Some(":".to_string()),
                     TokenKind::Comma => consumed_text = Some(",".to_string()),
                     TokenKind::Dot => consumed_text = Some(".".to_string()),
                     TokenKind::Question => consumed_text = Some("?".to_string()),
                     TokenKind::Not => consumed_text = Some("!".to_string()),
                     TokenKind::Minus => consumed_text = Some("-".to_string()),
                     TokenKind::Eq => consumed_text = Some("=".to_string()),
                     TokenKind::Plus => consumed_text = Some("+".to_string()),
                     TokenKind::Star => consumed_text = Some("*".to_string()),
                     TokenKind::Slash => consumed_text = Some("/".to_string()),
                     TokenKind::Percent => consumed_text = Some("%".to_string()),
                     TokenKind::Amp => consumed_text = Some("&".to_string()),
                     TokenKind::Pipe => consumed_text = Some("|".to_string()),
                     TokenKind::At => consumed_text = Some("@".to_string()),
                     TokenKind::Tilde => consumed_text = Some("~".to_string()),
                     TokenKind::Caret => consumed_text = Some("^".to_string()),
                     TokenKind::LParen => consumed_text = Some("(".to_string()),
                     TokenKind::RParen => consumed_text = Some(")".to_string()),
                     TokenKind::LBracket => consumed_text = Some("[".to_string()),
                     TokenKind::RBracket => consumed_text = Some("]".to_string()),
                     TokenKind::Arrow => consumed_text = Some("->".to_string()),
                     TokenKind::FatArrow => consumed_text = Some("=>".to_string()),
                     TokenKind::ColonColon => consumed_text = Some("::".to_string()),
                     TokenKind::And => consumed_text = Some("and".to_string()),
                     TokenKind::Or => consumed_text = Some("or".to_string()),
                     // Keywords that can appear as text in JSX
                     TokenKind::Gpu => consumed_text = Some("GPU".to_string()),
                     TokenKind::Io => consumed_text = Some("IO".to_string()),
                     TokenKind::Fn => consumed_text = Some("fn".to_string()),
                     TokenKind::Let => consumed_text = Some("let".to_string()),
                     TokenKind::Mut => consumed_text = Some("mut".to_string()),
                     TokenKind::Var => consumed_text = Some("var".to_string()),
                     TokenKind::Const => consumed_text = Some("const".to_string()),
                     TokenKind::If => consumed_text = Some("if".to_string()),
                     TokenKind::Else => consumed_text = Some("else".to_string()),
                     TokenKind::Match => consumed_text = Some("match".to_string()),
                     TokenKind::For => consumed_text = Some("for".to_string()),
                     TokenKind::While => consumed_text = Some("while".to_string()),
                     TokenKind::Loop => consumed_text = Some("loop".to_string()),
                     TokenKind::Break => consumed_text = Some("break".to_string()),
                     TokenKind::Continue => consumed_text = Some("continue".to_string()),
                     TokenKind::Return => consumed_text = Some("return".to_string()),
                     TokenKind::Await => consumed_text = Some("await".to_string()),
                     TokenKind::In => consumed_text = Some("in".to_string()),
                     TokenKind::With => consumed_text = Some("with".to_string()),
                     TokenKind::As => consumed_text = Some("as".to_string()),
                     TokenKind::TypeKw => consumed_text = Some("type".to_string()),
                     TokenKind::Struct => consumed_text = Some("struct".to_string()),
                     TokenKind::Enum => consumed_text = Some("enum".to_string()),
                     TokenKind::Trait => consumed_text = Some("trait".to_string()),
                     TokenKind::Impl => consumed_text = Some("impl".to_string()),
                     TokenKind::Pub => consumed_text = Some("pub".to_string()),
                     TokenKind::Mod => consumed_text = Some("mod".to_string()),
                     TokenKind::Use => consumed_text = Some("use".to_string()),
                     TokenKind::True => consumed_text = Some("true".to_string()),
                     TokenKind::False => consumed_text = Some("false".to_string()),
                     TokenKind::Pure => consumed_text = Some("Pure".to_string()),
                     TokenKind::Async => consumed_text = Some("Async".to_string()),
                     TokenKind::Component => consumed_text = Some("component".to_string()),
                     TokenKind::Shader => consumed_text = Some("shader".to_string()),
                     TokenKind::Actor => consumed_text = Some("actor".to_string()),
                     TokenKind::Spawn => consumed_text = Some("spawn".to_string()),
                     TokenKind::Test => consumed_text = Some("test".to_string()),
                     TokenKind::Reactive => consumed_text = Some("Reactive".to_string()),
                     TokenKind::Unsafe => consumed_text = Some("Unsafe".to_string()),
                     TokenKind::Vertex => consumed_text = Some("vertex".to_string()),
                     TokenKind::Fragment => consumed_text = Some("fragment".to_string()),
                     _ => {
                         return Err(self.parser_error(format!("Unexpected token in JSX child: {:?}. Use strings or {{}} for text.", self.peek_kind()), self.current_span()));
                     }
                 }
                 
                 if let Some(t) = consumed_text {
                     if text_buffer.is_empty() {
                         text_start = self.current_span();
                     }
                     text_buffer.push_str(&t);
                     self.advance();
                 }
                 
                 last_end = self.tokens.get(self.pos - 1).map(|t| t.span.end).unwrap_or(0);
            }
        }
        
        if !text_buffer.is_empty() {
            children.push(JSXNode::Text(text_buffer, text_start.merge(Span::new(last_end, last_end))));
        }
        
        self.expect(TokenKind::LtSlash)?;
        let closing_tag = self.parse_ident()?;
        if closing_tag != tag {
            return Err(self.parser_error(format!("Expected closing tag </{}>, found </{}>", tag, closing_tag), self.current_span()));
        }
        self.expect(TokenKind::Gt)?;
        
        Ok(JSXNode::Element { tag, attributes: attrs, children, span: start.merge(self.current_span()) })
    }

    fn parse_call_args(&mut self) -> KainResult<Vec<CallArg>> {
        let mut args = Vec::new();
        self.skip_formatting();
        while !self.check(TokenKind::RParen) && !self.at_end() {
            let mut name = None;
            // Check for named argument: ident = expr
            if let TokenKind::Ident(s) = self.peek_kind() {
                // Look ahead for '='
                if self.tokens.get(self.pos + 1).map(|t| t.kind == TokenKind::Eq).unwrap_or(false) {
                    name = Some(s);
                    self.advance(); // eat ident
                    self.advance(); // eat =
                }
            }
            
            let value = self.parse_expr()?;
            args.push(CallArg { name, value, span: self.current_span() });
            
            self.skip_formatting();
            if !self.check(TokenKind::RParen) { 
                self.expect(TokenKind::Comma)?; 
                self.skip_formatting();
            }
        }
        Ok(args)
    }

    fn parse_visibility(&mut self) -> Visibility {
        if self.check(TokenKind::Pub) { self.advance(); Visibility::Public } else { Visibility::Private }
    }

    fn parse_ident(&mut self) -> KainResult<String> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(s) => { 
                self.advance(); 
                self.validate_identifier(&s, span)?;
                Ok(s) 
            }
            TokenKind::SelfLower => { self.advance(); Ok("self".to_string()) }
            TokenKind::SelfUpper => { self.advance(); Ok("Self".to_string()) }
            // Contextual keywords - allowed as identifiers in non-declaration contexts
            TokenKind::Component => { self.advance(); Ok("component".to_string()) }
            TokenKind::Shader => { self.advance(); Ok("shader".to_string()) }
            TokenKind::Actor => { self.advance(); Ok("actor".to_string()) }
            TokenKind::State => { self.advance(); Ok("state".to_string()) }
            // Special handling for keyword tokens that users might try to use as identifiers
            // Generate clear error messages for all KAIN keyword tokens
            k @ (TokenKind::Fn | TokenKind::Let | TokenKind::Mut | TokenKind::Var | TokenKind::Const |
                 TokenKind::If | TokenKind::Else | TokenKind::Elif | TokenKind::Match | TokenKind::For |
                 TokenKind::While | TokenKind::Loop | TokenKind::Break | TokenKind::Continue | TokenKind::Return |
                 TokenKind::Await | TokenKind::In | TokenKind::With | TokenKind::As | TokenKind::TypeKw |
                 TokenKind::Struct | TokenKind::Enum | TokenKind::Trait | TokenKind::Impl | TokenKind::Pub |
                 TokenKind::Mod | TokenKind::Use | TokenKind::True | TokenKind::False | TokenKind::None |
                 TokenKind::Spawn | TokenKind::Send | TokenKind::Receive | TokenKind::Emit |
                 TokenKind::Comptime | TokenKind::Macro | TokenKind::Vertex | TokenKind::Fragment |
                 TokenKind::Test | TokenKind::Pure | TokenKind::Io | TokenKind::AsyncKw | TokenKind::Async |
                 TokenKind::Gpu | TokenKind::Reactive | TokenKind::Unsafe) => {
                Err(self.parser_error(
                    format!("{:?} is a reserved keyword and cannot be used as an identifier. Please choose a different name.", k),
                    span
                ))
            }
            k => Err(self.parser_error(format!("Expected identifier, got {:?}", k), span)),
        }
    }

    fn get_binary_op(&self) -> Option<(BinaryOp, u8)> {
        match self.peek_kind() {
            TokenKind::Or => Some((BinaryOp::Or, 1)),
            TokenKind::And => Some((BinaryOp::And, 2)),
            TokenKind::EqEq => Some((BinaryOp::Eq, 3)),
            TokenKind::NotEq => Some((BinaryOp::Ne, 3)),
            TokenKind::Lt => Some((BinaryOp::Lt, 4)),
            TokenKind::Gt => Some((BinaryOp::Gt, 4)),
            TokenKind::LtEq => Some((BinaryOp::Le, 4)),
            TokenKind::GtEq => Some((BinaryOp::Ge, 4)),
            TokenKind::Plus => Some((BinaryOp::Add, 5)),
            TokenKind::Minus => Some((BinaryOp::Sub, 5)),
            TokenKind::Star => Some((BinaryOp::Mul, 6)),
            TokenKind::Slash => Some((BinaryOp::Div, 6)),
            TokenKind::Percent => Some((BinaryOp::Mod, 6)),
            TokenKind::Power => Some((BinaryOp::Pow, 7)),
            _ => None,
        }
    }

    // Helper methods
    fn peek_kind(&self) -> TokenKind { 
        // Check injected tokens first
        if !self.injected_tokens.is_empty() {
            return self.injected_tokens[0].kind.clone();
        }
        self.tokens.get(self.pos).map(|t| t.kind.clone()).unwrap_or(TokenKind::Eof) 
    }
    
    fn current_span(&self) -> Span { 
        // Check injected tokens first
        if !self.injected_tokens.is_empty() {
            return self.injected_tokens[0].span;
        }
        self.tokens.get(self.pos).map(|t| t.span).unwrap_or(Span::new(0, 0)) 
    }
    
    fn at_end(&self) -> bool { matches!(self.peek_kind(), TokenKind::Eof) }
    fn check(&self, k: TokenKind) -> bool { std::mem::discriminant(&self.peek_kind()) == std::mem::discriminant(&k) }
    fn check_line_end(&self) -> bool { matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Dedent | TokenKind::Eof) }
    
    fn advance(&mut self) { 
        // Consume injected tokens first
        if !self.injected_tokens.is_empty() {
            self.injected_tokens.remove(0);
            return;
        }
        if !self.at_end() { self.pos += 1; } 
    }
    
    fn skip_newlines(&mut self) { while let TokenKind::Newline(_) = self.peek_kind() { self.advance(); } }
    fn check_newline(&self) -> bool { matches!(self.peek_kind(), TokenKind::Newline(_)) }
    fn skip_formatting(&mut self) {
        while matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent | TokenKind::Dedent) {
            self.advance();
        }
    }
    
    fn inject_token(&mut self, token: Token) {
        self.injected_tokens.push(token);
    }

    fn expect(&mut self, k: TokenKind) -> KainResult<()> {
        if self.check(k.clone()) { self.advance(); Ok(()) }
        else { Err(self.parser_error(format!("Expected {:?}, got {:?}", k, self.peek_kind()), self.current_span())) }
    }

    // ===== GRAPH RUNTIME PARSING =====
    
    /// Parse @graph_runtime struct definition
    fn parse_graph_runtime(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse graph data, node types, instance, and pin config
        let mut graph_data = None;
        let mut node_types = Vec::new();
        let mut instance = None;
        let mut pin_config = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for nested attributes
            let nested_attrs = self.parse_attributes()?;
            
            if nested_attrs.iter().any(|a| a.name == "graph_data") {
                graph_data = Some(self.parse_graph_data(nested_attrs)?);
            } else if nested_attrs.iter().any(|a| a.name == "node_data") {
                node_types.push(self.parse_node_data(nested_attrs)?);
            } else if nested_attrs.iter().any(|a| a.name == "instance") {
                instance = Some(self.parse_graph_instance(nested_attrs)?);
            } else if nested_attrs.iter().any(|a| a.name == "pin_config") {
                pin_config = Some(self.parse_pin_config(nested_attrs)?);
            } else {
                return Err(self.parser_error(
                    "Expected @graph_data, @node_data, @instance, or @pin_config in graph runtime",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::GraphRuntime(GraphRuntimeDef {
            name,
            attributes,
            graph_data,
            node_types,
            instance,
            pin_config,
            span: start.merge(self.current_span()),
        }))
    }
    
    /// Parse @graph_data struct definition
    fn parse_graph_data(&mut self, attributes: Vec<Attribute>) -> KainResult<GraphDataDef> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Skip name (it's implicit)
        let _ = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse properties and methods
        let mut properties = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let field_attrs = self.parse_attributes()?;
            
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(Visibility::Public)? {
                    methods.push(f);
                }
            } else {
                // Parse property
                let prop_name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let prop_ty = self.parse_type()?;
                
                let default = if self.check(TokenKind::Eq) {
                    self.advance();
                    Some(self.parse_expr()?)
                } else {
                    None
                };
                
                properties.push(Field {
                    name: prop_name,
                    ty: prop_ty,
                    attributes: field_attrs,
                    visibility: Visibility::Public,
                    default,
                    weak: false,
                    span: self.current_span(),
                });
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(GraphDataDef {
            properties,
            methods,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse @node_data struct definition
    fn parse_node_data(&mut self, attributes: Vec<Attribute>) -> KainResult<NodeDataDef> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Check for base class (optional inheritance syntax)
        let base_class = if self.check(TokenKind::LParen) {
            self.advance();
            let base = self.parse_ident()?;
            self.expect(TokenKind::RParen)?;
            Some(base)
        } else {
            None
        };
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse input pins, output pins, properties, methods, and execute logic
        let mut input_pins = Vec::new();
        let mut output_pins = Vec::new();
        let mut properties = Vec::new();
        let mut methods = Vec::new();
        let mut execute_logic = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let field_attrs = self.parse_attributes()?;
            
            if field_attrs.iter().any(|a| a.name == "input_pin") {
                input_pins.push(self.parse_pin_def(field_attrs)?);
            } else if field_attrs.iter().any(|a| a.name == "output_pin") {
                output_pins.push(self.parse_pin_def(field_attrs)?);
            } else if field_attrs.iter().any(|a| a.name == "property") {
                // Parse property
                let prop_name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let prop_ty = self.parse_type()?;
                
                let default = if self.check(TokenKind::Eq) {
                    self.advance();
                    Some(self.parse_expr()?)
                } else {
                    None
                };
                
                properties.push(Field {
                    name: prop_name,
                    ty: prop_ty,
                    attributes: field_attrs,
                    visibility: Visibility::Public,
                    default,
                    weak: false,
                    span: self.current_span(),
                });
            } else if self.check(TokenKind::Fn) {
                // Check if this is the execute function
                if let Item::Function(f) = self.parse_function(Visibility::Public)? {
                    if f.name == "execute" {
                        // Store as execute logic
                        execute_logic = Some(f.body);
                    } else {
                        methods.push(f);
                    }
                }
            } else {
                return Err(self.parser_error(
                    "Expected @input_pin, @output_pin, @property, or fn in node data",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(NodeDataDef {
            name,
            base_class,
            input_pins,
            output_pins,
            properties,
            methods,
            execute_logic,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse pin definition (@input_pin or @output_pin)
    fn parse_pin_def(&mut self, attributes: Vec<Attribute>) -> KainResult<PinDef> {
        let start = self.current_span();
        
        // Parse pin name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Parse type
        let ty = self.parse_type()?;
        
        // Check for array syntax
        let is_array = matches!(&ty, Type::Named { name, .. } if name == "Array");
        
        // Check for default value
        let default = if self.check(TokenKind::Eq) {
            self.advance();
            Some(self.parse_expr()?)
        } else {
            None
        };
        
        Ok(PinDef {
            name,
            ty,
            is_array,
            default,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse @instance struct definition
    fn parse_graph_instance(&mut self, attributes: Vec<Attribute>) -> KainResult<GraphInstanceDef> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Skip name (it's implicit)
        let _ = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse state fields, methods, and delegates
        let mut state = Vec::new();
        let mut methods = Vec::new();
        let mut delegates = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let field_attrs = self.parse_attributes()?;
            
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(Visibility::Public)? {
                    methods.push(f);
                }
            } else if let TokenKind::Ident(ref s) = self.peek_kind() {
                if s == "delegate" {
                    delegates.push(self.parse_delegate_def(field_attrs)?);
                } else {
                    // Parse state field
                    let field_name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let field_ty = self.parse_type()?;
                    
                    let default = if self.check(TokenKind::Eq) {
                        self.advance();
                        Some(self.parse_expr()?)
                    } else {
                        None
                    };
                    
                    state.push(Field {
                        name: field_name,
                        ty: field_ty,
                        attributes: field_attrs,
                        visibility: Visibility::Public,
                        default,
                        weak: false,
                        span: self.current_span(),
                    });
                }
            } else {
                return Err(self.parser_error(
                    "Expected fn, delegate, or field in instance",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(GraphInstanceDef {
            state,
            methods,
            delegates,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse delegate definition
    fn parse_delegate_def(&mut self, attributes: Vec<Attribute>) -> KainResult<DelegateDef> {
        let start = self.current_span();
        
        // Expect 'delegate' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "delegate" {
                return Err(self.parser_error("Expected 'delegate' keyword", self.current_span()));
            }
            self.advance();
        }
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect LParen
        self.expect(TokenKind::LParen)?;
        
        // Parse parameters
        let params = self.parse_params()?;
        
        // Expect RParen
        self.expect(TokenKind::RParen)?;
        
        Ok(DelegateDef {
            name,
            params,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse @pin_config struct definition
    fn parse_pin_config(&mut self, attributes: Vec<Attribute>) -> KainResult<PinConfigDef> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Skip name (it's implicit)
        let _ = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse properties and methods
        let mut properties = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            let field_attrs = self.parse_attributes()?;
            
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(Visibility::Public)? {
                    methods.push(f);
                }
            } else {
                // Parse property
                let prop_name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let prop_ty = self.parse_type()?;
                
                let default = if self.check(TokenKind::Eq) {
                    self.advance();
                    Some(self.parse_expr()?)
                } else {
                    None
                };
                
                properties.push(Field {
                    name: prop_name,
                    ty: prop_ty,
                    attributes: field_attrs,
                    visibility: Visibility::Public,
                    default,
                    weak: false,
                    span: self.current_span(),
                });
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(PinConfigDef {
            properties,
            methods,
            attributes,
            span: start.merge(self.current_span()),
        })
    }

    // ===== STATE MACHINE PARSING =====
    
    /// Parse @state_machine struct definition
    fn parse_state_machine(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Parse name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse states
        let mut states = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for @state attribute
            let state_attrs = self.parse_attributes()?;
            
            if state_attrs.iter().any(|a| a.name == "state") {
                states.push(self.parse_state(state_attrs)?);
            } else {
                return Err(self.parser_error(
                    "Expected @state in state machine definition",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::StateMachine(StateMachineDef {
            name,
            states,
            attributes,
            span: start.merge(self.current_span()),
        }))
    }
    
    /// Parse @state struct definition
    fn parse_state(&mut self, attributes: Vec<Attribute>) -> KainResult<StateDef> {
        let start = self.current_span();
        
        // Check if this is an entry state by looking for entry: true in @state attribute
        let is_entry = attributes.iter().any(|attr| {
            if attr.name == "state" {
                // Check for entry: true parameter (represented as Tuple(Ident("entry"), Bool(true)))
                attr.args.iter().any(|arg| {
                    if let Expr::Tuple(parts, _) = arg {
                        if parts.len() == 2 {
                            if let (Expr::Ident(name, _), Expr::Bool(true, _)) = (&parts[0], &parts[1]) {
                                return name == "entry";
                            }
                        }
                    }
                    false
                })
            } else {
                false
            }
        });
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Parse state name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Expect indent
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        // Parse state body (properties, transitions, on_enter, on_exit)
        let mut animation = None;
        let mut properties = Vec::new();
        let mut transitions = Vec::new();
        let mut on_enter = None;
        let mut on_exit = None;
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Check for attributes
            let field_attrs = self.parse_attributes()?;
            
            // Check for @transition
            if field_attrs.iter().any(|a| a.name == "transition") {
                transitions.push(self.parse_transition(field_attrs)?);
            } else if self.check(TokenKind::Fn) {
                // Parse method (on_enter, on_exit, or transition condition)
                let method_name = self.peek_next_ident()?;
                
                if method_name == "on_enter" {
                    self.advance(); // consume 'fn'
                    self.advance(); // consume 'on_enter'
                    self.expect(TokenKind::LParen)?;
                    self.expect(TokenKind::RParen)?;
                    self.expect(TokenKind::Colon)?;
                    on_enter = Some(self.parse_block()?);
                } else if method_name == "on_exit" {
                    self.advance(); // consume 'fn'
                    self.advance(); // consume 'on_exit'
                    self.expect(TokenKind::LParen)?;
                    self.expect(TokenKind::RParen)?;
                    self.expect(TokenKind::Colon)?;
                    on_exit = Some(self.parse_block()?);
                } else {
                    // Regular property or unknown method
                    return Err(self.parser_error(
                        "Unexpected method in state definition. Use @transition for transitions.",
                        self.current_span()
                    ));
                }
            } else {
                // Parse property (like animation: "Idle_Anim")
                let prop_name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                
                if prop_name == "animation" {
                    // Parse animation string
                    if let Expr::String(anim_name, _) = self.parse_expr()? {
                        animation = Some(anim_name);
                    } else {
                        return Err(self.parser_error(
                            "Expected string literal for animation property",
                            self.current_span()
                        ));
                    }
                } else {
                    // Regular property
                    let prop_ty = self.parse_type()?;
                    let default = if self.check(TokenKind::Eq) {
                        self.advance();
                        Some(self.parse_expr()?)
                    } else {
                        None
                    };
                    
                    properties.push(Field {
                        name: prop_name,
                        ty: prop_ty,
                        attributes: field_attrs,
                        visibility: Visibility::Public,
                        default,
                        weak: false,
                        span: self.current_span(),
                    });
                }
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(StateDef {
            name,
            is_entry,
            animation,
            properties,
            transitions,
            on_enter,
            on_exit,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Parse @transition function definition
    fn parse_transition(&mut self, attributes: Vec<Attribute>) -> KainResult<TransitionDef> {
        let start = self.current_span();
        
        // Extract 'to' parameter from @transition(to: "StateName")
        // The parameter is represented as Tuple(Ident("to"), String("StateName"))
        let to_state = attributes.iter()
            .find(|a| a.name == "transition")
            .and_then(|attr| {
                // Look for 'to' parameter
                attr.args.iter().find_map(|arg| {
                    if let Expr::Tuple(parts, _) = arg {
                        if parts.len() == 2 {
                            if let (Expr::Ident(param_name, _), Expr::String(state_name, _)) = (&parts[0], &parts[1]) {
                                if param_name == "to" {
                                    return Some(state_name.clone());
                                }
                            }
                        }
                    }
                    None
                })
            })
            .ok_or_else(|| self.parser_error(
                "Expected 'to' parameter in @transition attribute",
                self.current_span()
            ))?;
        
        // Expect 'fn' keyword
        self.expect(TokenKind::Fn)?;
        
        // Parse function name (condition name)
        let _condition_name = self.parse_ident()?;
        
        // Parse parameters (should be empty or just self)
        self.expect(TokenKind::LParen)?;
        self.expect(TokenKind::RParen)?;
        
        // Parse return type (should be Bool)
        self.expect(TokenKind::Arrow)?;
        let _return_type = self.parse_type()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Parse condition body
        let condition = Some(self.parse_block()?);
        
        Ok(TransitionDef {
            to_state,
            condition,
            priority: 0, // Default priority
            attributes,
            span: start.merge(self.current_span()),
        })
    }
    
    /// Helper to peek at the next identifier without consuming tokens
    fn peek_next_ident(&self) -> KainResult<String> {
        if self.pos + 1 < self.tokens.len() {
            if let TokenKind::Ident(name) = &self.tokens[self.pos + 1].kind {
                Ok(name.clone())
            } else {
                Err(self.parser_error("Expected identifier", self.current_span()))
            }
        } else {
            Err(self.parser_error("Unexpected end of input", self.current_span()))
        }
    }
    
    /// Parse @editor_module struct definition
    /// Syntax:
    /// ```kain
    /// @editor_module
    /// struct WeaponEditorModule:
    ///     @menu_entry(path: "Tools/Weapons", label: "Open Weapon Editor")
    ///     fn on_open_editor():
    ///         println("Opening weapon editor...")
    ///     
    ///     @toolbar_button(section: "Content", icon: "Icons.Weapon")
    ///     fn on_quick_create():
    ///         println("Quick creating weapon...")
    /// ```
    fn parse_editor_module(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        
        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;
        
        // Parse module name
        let name = self.parse_ident()?;
        
        // Expect colon
        self.expect(TokenKind::Colon)?;
        
        // Parse body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        
        let mut menu_entries = Vec::new();
        let mut toolbar_buttons = Vec::new();
        let mut toolbar_widgets = Vec::new();
        let mut methods = Vec::new();
        
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) { break; }
            
            // Parse attributes for the method
            let method_attrs = self.parse_attributes()?;
            
            // Check if this is a menu entry, toolbar button, or toolbar widget
            let is_menu_entry = method_attrs.iter().any(|a| a.name == "menu_entry");
            let is_toolbar_button = method_attrs.iter().any(|a| a.name == "toolbar_button");
            let is_toolbar_widget = method_attrs.iter().any(|a| a.name == "toolbar_widget");
            
            if is_menu_entry {
                // Parse menu entry method
                let menu_entry = self.parse_menu_entry(method_attrs)?;
                menu_entries.push(menu_entry);
            } else if is_toolbar_button {
                // Parse toolbar button method
                let toolbar_button = self.parse_toolbar_button(method_attrs)?;
                toolbar_buttons.push(toolbar_button);
            } else if is_toolbar_widget {
                // Parse toolbar widget
                let toolbar_widget = self.parse_toolbar_widget(method_attrs)?;
                toolbar_widgets.push(toolbar_widget);
            } else if self.check(TokenKind::Fn) {
                // Regular method
                if let Item::Function(func) = self.parse_function_with_attrs(Visibility::Public, method_attrs)? {
                    methods.push(func);
                }
            } else {
                return Err(self.parser_error(
                    "Expected @menu_entry, @toolbar_button, @toolbar_widget, or fn in editor module",
                    self.current_span()
                ));
            }
            
            self.skip_newlines();
        }
        
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        
        Ok(Item::EditorModule(EditorModuleDef {
            name,
            menu_entries,
            toolbar_buttons,
            toolbar_widgets,
            methods,
            attributes,
            span: start.merge(self.current_span()),
        }))
    }
    
    /// Parse @menu_entry method
    fn parse_menu_entry(&mut self, attributes: Vec<Attribute>) -> KainResult<MenuEntryDef> {
        let start = self.current_span();
        
        // Extract parameters from @menu_entry attribute
        let menu_attr = attributes.iter()
            .find(|a| a.name == "menu_entry")
            .ok_or_else(|| self.parser_error("Expected @menu_entry attribute", self.current_span()))?;
        
        let mut path = None;
        let mut label = None;
        let mut icon = None;
        let mut tooltip = None;
        
        // Parse named arguments from attribute
        for arg in &menu_attr.args {
            if let Expr::Tuple(parts, _) = arg {
                if parts.len() == 2 {
                    if let (Expr::Ident(param_name, _), Expr::String(value, _)) = (&parts[0], &parts[1]) {
                        match param_name.as_str() {
                            "path" => path = Some(value.clone()),
                            "label" => label = Some(value.clone()),
                            "icon" => icon = Some(value.clone()),
                            "tooltip" => tooltip = Some(value.clone()),
                            _ => {}
                        }
                    }
                }
            }
        }
        
        let path = path.ok_or_else(|| self.parser_error("@menu_entry requires 'path' parameter", self.current_span()))?;
        let label = label.ok_or_else(|| self.parser_error("@menu_entry requires 'label' parameter", self.current_span()))?;
        
        // Parse the method
        if let Item::Function(method) = self.parse_function_with_attrs(Visibility::Public, vec![])? {
            Ok(MenuEntryDef {
                path,
                label,
                method,
                icon,
                tooltip,
                attributes,
                span: start.merge(self.current_span()),
            })
        } else {
            Err(self.parser_error("Expected function after @menu_entry", self.current_span()))
        }
    }
    
    /// Parse @toolbar_button method
    fn parse_toolbar_button(&mut self, attributes: Vec<Attribute>) -> KainResult<ToolbarButtonDef> {
        let start = self.current_span();
        
        // Extract parameters from @toolbar_button attribute
        let toolbar_attr = attributes.iter()
            .find(|a| a.name == "toolbar_button")
            .ok_or_else(|| self.parser_error("Expected @toolbar_button attribute", self.current_span()))?;
        
        let mut section = None;
        let mut label = None;
        let mut icon = None;
        let mut tooltip = None;
        
        // Parse named arguments from attribute
        for arg in &toolbar_attr.args {
            if let Expr::Tuple(parts, _) = arg {
                if parts.len() == 2 {
                    if let (Expr::Ident(param_name, _), Expr::String(value, _)) = (&parts[0], &parts[1]) {
                        match param_name.as_str() {
                            "section" => section = Some(value.clone()),
                            "label" => label = Some(value.clone()),
                            "icon" => icon = Some(value.clone()),
                            "tooltip" => tooltip = Some(value.clone()),
                            _ => {}
                        }
                    }
                }
            }
        }
        
        let section = section.ok_or_else(|| self.parser_error("@toolbar_button requires 'section' parameter", self.current_span()))?;
        let icon = icon.ok_or_else(|| self.parser_error("@toolbar_button requires 'icon' parameter", self.current_span()))?;
        
        // Parse the method
        if let Item::Function(method) = self.parse_function_with_attrs(Visibility::Public, vec![])? {
            Ok(ToolbarButtonDef {
                section,
                label,
                icon,
                method,
                tooltip,
                attributes,
                span: start.merge(self.current_span()),
            })
        } else {
            Err(self.parser_error("Expected function after @toolbar_button", self.current_span()))
        }
    }
    
    /// Parse @toolbar_widget
    fn parse_toolbar_widget(&mut self, attributes: Vec<Attribute>) -> KainResult<ToolbarWidgetDef> {
        let start = self.current_span();
        
        // Extract parameters from @toolbar_widget attribute
        let widget_attr = attributes.iter()
            .find(|a| a.name == "toolbar_widget")
            .ok_or_else(|| self.parser_error("Expected @toolbar_widget attribute", self.current_span()))?;
        
        let mut section = None;
        let mut position = None;
        let mut widget_type = None;
        
        // Parse named arguments from attribute
        for arg in &widget_attr.args {
            if let Expr::Tuple(parts, _) = arg {
                if parts.len() == 2 {
                    if let Expr::Ident(param_name, _) = &parts[0] {
                        match param_name.as_str() {
                            "section" => {
                                if let Expr::String(value, _) = &parts[1] {
                                    section = Some(value.clone());
                                }
                            }
                            "position" => {
                                if let Expr::Ident(pos, _) = &parts[1] {
                                    position = Some(match pos.as_str() {
                                        "Before" => ToolbarPosition::Before,
                                        "After" => ToolbarPosition::After,
                                        "Start" => ToolbarPosition::Start,
                                        "End" => ToolbarPosition::End,
                                        _ => ToolbarPosition::After,
                                    });
                                }
                            }
                            "widget_type" => {
                                if let Expr::String(value, _) = &parts[1] {
                                    widget_type = Some(value.clone());
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
        }
        
        let section = section.ok_or_else(|| self.parser_error("@toolbar_widget requires 'section' parameter", self.current_span()))?;
        let position = position.unwrap_or(ToolbarPosition::After);
        let widget_type = widget_type.ok_or_else(|| self.parser_error("@toolbar_widget requires 'widget_type' parameter", self.current_span()))?;
        
        Ok(ToolbarWidgetDef {
            section,
            position,
            widget_type,
            attributes,
            span: start.merge(self.current_span()),
        })
    }
}

