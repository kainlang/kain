//! KAIN Parser - Python-style indentation with Rust semantics

use crate::ast::*;
use crate::diagnostic_registry::DiagnosticCode;
use crate::diagnostics::SpanMapper;
use crate::effects::Effect;
use crate::error::{DiagnosticReport, ErrorKind, KainError, KainResult};
use crate::language_features::{default_language_capabilities, LanguageCapabilities};
use crate::lexer::{Lexer, Token, TokenKind};
use crate::span::Span;
use kain_ownership::{COLLAPSE_KEYWORD, OBSERVE_KEYWORD, SHARE_KEYWORD};

/// Maximum number of errors to accumulate before bailing out.
/// Prevents runaway error accumulation from freezing the compiler.
const MAX_ERRORS: usize = 50;

/// Reserved keywords that cannot be used as identifiers.
/// This includes KAIN keywords, HLSL keywords, C++ keywords, and UE5 macros.
/// Note: Contextual keywords like "state", "compute", "weak" are NOT included here
/// as they are only keywords in specific contexts and can be used as identifiers elsewhere.
pub const RESERVED_KEYWORDS: &[&str] = &[
    // KAIN core keywords (always reserved)
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
    "actor",
    "spawn",
    "send",
    "receive",
    "emit",
    "comptime",
    "macro",
    "vertex",
    "fragment",
    "collapse",
    "observe",
    "decay",
    "share",
    "fanout",
    "axiom",
    "pulse",
    "shatter",
    "teleport",
    "test",
    "Pure",
    "IO",
    "async",
    "Async",
    "GPU",
    "Reactive",
    "Unsafe",
    // HLSL keywords (from ue5-shaders/src/codegen_usf.rs)
    // Note: HLSL type names like RWBuffer, Texture2D, etc. are NOT reserved keywords
    // because they are only valid as type annotations, not as variable names.
    // The type system will handle validation of type names separately.
    // Note: Shader stage abbreviations (vs, ps, gs, hs, ds, cs) are NOT reserved
    // because they are only meaningful in HLSL shader profile strings, not as variable names.
    "line",
    "compile",
    "pass",
    "technique",
    "register",
    "packoffset",
    "typedef",
    "sampler",
    "row_major",
    "column_major",
    "out",
    "inout",
    "inline",
    "cbuffer",
    "tbuffer",
    "uniform",
    "precise",
    "volatile",
    "extern",
    "shared",
    "groupshared",
    "half",
    "min16float",
    "min10float",
    "min16int",
    "min12int",
    "min16uint",
    "interface",
    "namespace",
    "static",
    "void",
    "bool",
    "int",
    "uint",
    "float",
    "double",
    "float2",
    "float3",
    "float4",
    "int2",
    "int3",
    "int4",
    "uint2",
    "uint3",
    "uint4",
    "float2x2",
    "float3x3",
    "float4x4",
    "matrix",
    "numthreads",
    "SV_Position",
    "SV_Target",
    "SV_DispatchThreadID",
    "SV_GroupID",
    "SV_GroupThreadID",
    // C++ keywords
    "class",
    "virtual",
    "override",
    "final",
    "explicit",
    "operator",
    "template",
    "typename",
    "private",
    "protected",
    "public",
    "friend",
    "this",
    "new",
    "delete",
    "nullptr",
    "try",
    "catch",
    "throw",
    "noexcept",
    "constexpr",
    "decltype",
    "auto",
    "signed",
    "unsigned",
    "short",
    "long",
    "char",
    "wchar_t",
    "char16_t",
    "char32_t",
    "sizeof",
    "alignof",
    "alignas",
    "typeid",
    "dynamic_cast",
    "static_cast",
    "reinterpret_cast",
    "const_cast",
    "goto",
    "switch",
    "case",
    "default",
    "do",
    "volatile",
    "mutable",
    "register",
    "union",
    "asm",
    "export",
    "thread_local",
    "static_assert",
    // UE5 macros and types
    // Note: UE5 type names like FVector, TArray, etc. are NOT reserved keywords
    // because they are only valid as type annotations, not as variable names.
    // The type system will handle validation of type names separately.
    "UCLASS",
    "USTRUCT",
    "UENUM",
    "UFUNCTION",
    "UPROPERTY",
    "UPARAM",
    "UMETA",
    "GENERATED_BODY",
    "GENERATED_USTRUCT_BODY",
    "GENERATED_UCLASS_BODY",
    "UINTERFACE",
    "RIGVM_METHOD",
    "FORCEINLINE",
    "FORCENOINLINE",
    "TEXT",
    "LOCTEXT",
    "NSLOCTEXT",
    "TEXTVIEW",
];

fn parse_orchestrate_stage_runtime(name: &str) -> Option<OrchestrateStageRuntime> {
    match name {
        "kain" => Some(OrchestrateStageRuntime::Kain),
        "rust" => Some(OrchestrateStageRuntime::Rust),
        "python" => Some(OrchestrateStageRuntime::Python),
        "node" => Some(OrchestrateStageRuntime::Node),
        _ => None,
    }
}

pub struct Parser<'a> {
    tokens: &'a [Token],
    pos: usize,
    injected_tokens: Vec<Token>, // Buffer for synthetic tokens (e.g., splitting >> into > >)
    span_mapper: &'a SpanMapper,
    filename: &'a str,
    capabilities: LanguageCapabilities,
    errors: Vec<KainError>, // Accumulated parse errors for multi-error recovery
    synthetic_counter: usize, // Fresh names for parser-desugared temporaries
}

impl<'a> Parser<'a> {
    pub fn new(tokens: &'a [Token], span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        Self::with_capabilities(
            tokens,
            span_mapper,
            filename,
            default_language_capabilities(),
        )
    }

    pub fn with_capabilities(
        tokens: &'a [Token],
        span_mapper: &'a SpanMapper,
        filename: &'a str,
        capabilities: LanguageCapabilities,
    ) -> Self {
        Self {
            tokens,
            pos: 0,
            injected_tokens: Vec::new(),
            span_mapper,
            filename,
            capabilities,
            errors: Vec::new(),
            synthetic_counter: 0,
        }
    }

    /// Create a parser error with file:line:col format
    fn parser_error(&self, message: impl Into<String>, span: Span) -> KainError {
        self.rich_parser_error(message, span)
    }

    fn synthetic_filename(filename: &str) -> bool {
        filename.starts_with('<') && filename.ends_with('>')
    }

    fn rich_parser_error(&self, message: impl Into<String>, span: Span) -> KainError {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let mut report =
            DiagnosticReport::new(ErrorKind::Parse, DiagnosticCode::ParseGeneric, message)
                .primary_label(span, "parser stopped here");
        if Self::synthetic_filename(self.filename) {
            report = report.origin(self.filename);
        } else {
            report = report.file(loc.file).location(loc.line, loc.col);
        }
        KainError::rich(report)
    }

    fn rich_parser_report(&self, report: DiagnosticReport) -> KainError {
        KainError::rich(report)
    }

    fn parser_report_at(
        &self,
        message: impl Into<String>,
        span: Span,
        label: impl Into<String>,
    ) -> DiagnosticReport {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let mut report =
            DiagnosticReport::new(ErrorKind::Parse, DiagnosticCode::ParseGeneric, message)
                .primary_label(span, label);
        if Self::synthetic_filename(self.filename) {
            report = report.origin(self.filename);
        } else {
            report = report.file(loc.file).location(loc.line, loc.col);
        }
        report
    }

    fn previous_significant_span(&self) -> Option<Span> {
        self.tokens[..self.pos.min(self.tokens.len())]
            .iter()
            .rev()
            .find(|token| {
                !matches!(
                    token.kind,
                    TokenKind::Newline(_)
                        | TokenKind::Indent
                        | TokenKind::Dedent
                        | TokenKind::Comment
                        | TokenKind::HashComment
                )
            })
            .map(|token| token.span)
    }

    /// Convert a token to a user-friendly string for error messages
    fn token_to_user_string(&self, kind: &TokenKind) -> String {
        match kind {
            TokenKind::Ident(s) => format!("identifier '{}'", s),
            TokenKind::Int(n) => format!("integer {}", n),
            TokenKind::Float(f) => format!("float {}", f),
            TokenKind::String(s) => format!("string \"{}\"", s),
            TokenKind::Fn => "keyword 'fn'".to_string(),
            TokenKind::Let => "keyword 'let'".to_string(),
            TokenKind::Struct => "keyword 'struct'".to_string(),
            TokenKind::Enum => "keyword 'enum'".to_string(),
            TokenKind::Actor => "keyword 'actor'".to_string(),
            TokenKind::Component => "keyword 'component'".to_string(),
            TokenKind::Shader => "keyword 'shader'".to_string(),
            TokenKind::If => "keyword 'if'".to_string(),
            TokenKind::Else => "keyword 'else'".to_string(),
            TokenKind::Match => "keyword 'match'".to_string(),
            TokenKind::Return => "keyword 'return'".to_string(),
            TokenKind::Colon => "':'".to_string(),
            TokenKind::Arrow => "'->'".to_string(),
            TokenKind::Eq => "'='".to_string(),
            TokenKind::LParen => "'('".to_string(),
            TokenKind::RParen => "')'".to_string(),
            TokenKind::LBrace => "'{'".to_string(),
            TokenKind::RBrace => "'}'".to_string(),
            TokenKind::LBracket => "'['".to_string(),
            TokenKind::RBracket => "']'".to_string(),
            TokenKind::Comma => "','".to_string(),
            TokenKind::Dot => "'.'".to_string(),
            TokenKind::At => "'@'".to_string(),
            TokenKind::Indent => "indentation".to_string(),
            TokenKind::Dedent => "dedentation".to_string(),
            TokenKind::Newline(_) => "newline".to_string(),
            TokenKind::Eof => "end of file".to_string(),
            _ => format!("{:?}", kind),
        }
    }

    /// Generate a list of expected tokens for error messages
    #[allow(dead_code)]
    fn expected_tokens_list(&self, expected: &[&str]) -> String {
        match expected.len() {
            0 => "something else".to_string(),
            1 => expected[0].to_string(),
            2 => format!("{} or {}", expected[0], expected[1]),
            _ => {
                let last = expected.last().unwrap();
                let rest = &expected[..expected.len() - 1];
                format!("{}, or {}", rest.join(", "), last)
            }
        }
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
            // Bail early if we've accumulated too many errors (prevents freeze)
            if self.errors.len() >= MAX_ERRORS {
                break;
            }
            self.skip_formatting();
            if self.at_end() {
                break;
            }

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
                    let before_pos = self.pos;
                    match self.parse_item() {
                        Ok(item) => items.push(item),
                        Err(e) => {
                            self.errors.push(e);
                            self.synchronize();
                            // Guard against no-progress recovery loops.
                            if self.pos == before_pos && !self.at_end() {
                                self.advance();
                            }
                        }
                    }
                }

                TokenKind::Ident(ref name)
                    if Self::is_contextual_item_start_name(name.as_str()) =>
                {
                    let before_pos = self.pos;
                    match self.parse_item() {
                        Ok(item) => items.push(item),
                        Err(e) => {
                            self.errors.push(e);
                            self.synchronize();
                            // Guard against no-progress recovery loops.
                            if self.pos == before_pos && !self.at_end() {
                                self.advance();
                            }
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
                    let before_pos = self.pos;
                    match self.parse_stmt() {
                        Ok(stmt) => top_level_stmts.push(stmt),
                        Err(e) => {
                            self.errors.push(e);
                            self.synchronize();
                            if self.pos == before_pos && !self.at_end() {
                                self.advance();
                            }
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
                body: Block {
                    stmts: top_level_stmts,
                    span: start.merge(self.current_span()),
                },
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
        Ok(Program {
            items,
            span: start.merge(end),
        })
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
                TokenKind::Indent => {
                    depth += 1;
                    self.advance();
                }
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
        match self.peek_kind() {
            TokenKind::At
            | TokenKind::Fn
            | TokenKind::Struct
            | TokenKind::Enum
            | TokenKind::Actor
            | TokenKind::Component
            | TokenKind::Shader
            | TokenKind::Pub
            | TokenKind::Const
            | TokenKind::Mod
            | TokenKind::Use
            | TokenKind::Impl
            | TokenKind::Macro
            | TokenKind::Test
            | TokenKind::AsyncKw
            | TokenKind::TypeKw
            | TokenKind::Trait
            | TokenKind::Comptime => true,
            TokenKind::Ident(name) => Self::is_contextual_item_start_name(name.as_str()),
            _ => false,
        }
    }

    fn is_contextual_item_start_name(name: &str) -> bool {
        matches!(
            name,
            "patch"
                | "law"
                | "axiom"
                | "converge"
                | "world"
                | "entangle"
                | "orchestrate"
                | "pulse"
                | "shatter"
        )
    }

    fn parse_path_name(&mut self) -> KainResult<String> {
        let mut name = self.parse_ident()?;
        while self.check(TokenKind::ColonColon) {
            self.advance();
            let part = self.parse_ident()?;
            name.push_str("::");
            name.push_str(&part);
        }
        Ok(name)
    }

    fn parse_use_path_segment(&mut self) -> KainResult<String> {
        let span = self.current_span();
        let segment = match self.peek_kind() {
            TokenKind::Ident(s) => Some(s),
            TokenKind::SelfLower => Some("self".to_string()),
            TokenKind::SelfUpper => Some("Self".to_string()),
            TokenKind::Fn => Some("fn".to_string()),
            TokenKind::Let => Some("let".to_string()),
            TokenKind::Mut => Some("mut".to_string()),
            TokenKind::Var => Some("var".to_string()),
            TokenKind::Const => Some("const".to_string()),
            TokenKind::If => Some("if".to_string()),
            TokenKind::Else => Some("else".to_string()),
            TokenKind::Elif => Some("elif".to_string()),
            TokenKind::Match => Some("match".to_string()),
            TokenKind::For => Some("for".to_string()),
            TokenKind::While => Some("while".to_string()),
            TokenKind::Loop => Some("loop".to_string()),
            TokenKind::Break => Some("break".to_string()),
            TokenKind::Continue => Some("continue".to_string()),
            TokenKind::Return => Some("return".to_string()),
            TokenKind::Await => Some("await".to_string()),
            TokenKind::In => Some("in".to_string()),
            TokenKind::With => Some("with".to_string()),
            TokenKind::As => Some("as".to_string()),
            TokenKind::TypeKw => Some("type".to_string()),
            TokenKind::Struct => Some("struct".to_string()),
            TokenKind::Enum => Some("enum".to_string()),
            TokenKind::Trait => Some("trait".to_string()),
            TokenKind::Impl => Some("impl".to_string()),
            TokenKind::Pub => Some("pub".to_string()),
            TokenKind::Mod => Some("mod".to_string()),
            TokenKind::Use => Some("use".to_string()),
            TokenKind::True => Some("true".to_string()),
            TokenKind::False => Some("false".to_string()),
            TokenKind::None => Some("none".to_string()),
            TokenKind::Component => Some("component".to_string()),
            TokenKind::Shader => Some("shader".to_string()),
            TokenKind::Actor => Some("actor".to_string()),
            TokenKind::State => Some("state".to_string()),
            TokenKind::Spawn => Some("spawn".to_string()),
            TokenKind::Send => Some("send".to_string()),
            TokenKind::Receive => Some("receive".to_string()),
            TokenKind::Emit => Some("emit".to_string()),
            TokenKind::Comptime => Some("comptime".to_string()),
            TokenKind::Macro => Some("macro".to_string()),
            TokenKind::Vertex => Some("vertex".to_string()),
            TokenKind::Fragment => Some("fragment".to_string()),
            TokenKind::Collapse => Some("collapse".to_string()),
            TokenKind::Observe => Some("observe".to_string()),
            TokenKind::Decay => Some("decay".to_string()),
            TokenKind::Share => Some("share".to_string()),
            TokenKind::Fanout => Some("fanout".to_string()),
            TokenKind::Test => Some("test".to_string()),
            TokenKind::Pure => Some("Pure".to_string()),
            TokenKind::Io => Some("IO".to_string()),
            TokenKind::AsyncKw => Some("async".to_string()),
            TokenKind::Async => Some("Async".to_string()),
            TokenKind::Gpu => Some("GPU".to_string()),
            TokenKind::Reactive => Some("Reactive".to_string()),
            TokenKind::Unsafe => Some("Unsafe".to_string()),
            _ => None,
        };

        if let Some(segment) = segment {
            self.advance();
            return Ok(segment);
        }

        Err(self.parser_error(
            format!(
                "Expected import path segment, got {}",
                crate::error::token_kind_to_user_string(&self.peek_kind())
            ),
            span,
        ))
    }

    fn parse_path_segments_after(&mut self, first: String) -> KainResult<Vec<String>> {
        let mut segments = vec![first];
        while self.check(TokenKind::ColonColon) {
            self.advance();
            segments.push(self.parse_ident()?);
        }
        Ok(segments)
    }

    fn join_path_segments(segments: &[String]) -> String {
        segments.join("::")
    }

    fn path_segment_looks_like_type_name(segment: &str) -> bool {
        segment.contains("__")
            || segment
                .chars()
                .next()
                .is_some_and(|ch| ch.is_ascii_uppercase())
    }

    fn path_looks_like_enum_variant(segments: &[String]) -> bool {
        if segments.len() < 2 {
            return false;
        }
        let variant = &segments[segments.len() - 1];
        let enum_head = &segments[segments.len() - 2];
        Self::path_segment_looks_like_type_name(variant)
            && Self::path_segment_looks_like_type_name(enum_head)
    }

    fn parse_variant_pattern_fields(&mut self) -> KainResult<VariantPatternFields> {
        if self.check(TokenKind::LParen) {
            self.advance();
            let mut patterns = Vec::new();
            while !self.check(TokenKind::RParen) {
                patterns.push(self.parse_pattern()?);
                if !self.check(TokenKind::RParen) {
                    self.expect(TokenKind::Comma)?;
                }
            }
            self.expect(TokenKind::RParen)?;
            Ok(VariantPatternFields::Tuple(patterns))
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
            Ok(VariantPatternFields::Struct(fields))
        } else {
            Ok(VariantPatternFields::Unit)
        }
    }

    fn parse_enum_variant_fields(&mut self) -> KainResult<EnumVariantFields> {
        if self.check(TokenKind::LParen) {
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
                Ok(EnumVariantFields::Unit)
            } else {
                Ok(EnumVariantFields::Tuple(items))
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
            Ok(EnumVariantFields::Struct(fields))
        } else {
            Ok(EnumVariantFields::Unit)
        }
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

        // Check for @gameplay_tags attribute
        if attributes.iter().any(|a| a.name == "gameplay_tags") {
            return self.parse_gameplay_tags(attributes);
        }

        // Check for @ability attribute
        if attributes.iter().any(|a| a.name == "ability") {
            return self.parse_gameplay_ability(attributes);
        }

        // Check for @gameplay_effect attribute
        if attributes.iter().any(|a| a.name == "gameplay_effect") {
            return self.parse_gameplay_effect(attributes);
        }

        // Check for @gameplay_cue attribute
        if attributes.iter().any(|a| a.name == "gameplay_cue") {
            return self.parse_gameplay_cue(attributes);
        }

        // Check for @ability_task attribute
        if attributes.iter().any(|a| a.name == "ability_task") {
            return self.parse_ability_task(attributes);
        }

        // Check for @target_actor attribute
        if attributes.iter().any(|a| a.name == "target_actor") {
            return self.parse_target_actor(attributes);
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
            TokenKind::Const => self.parse_const(vis, attributes),
            TokenKind::Comptime => self.parse_comptime_block(),
            TokenKind::Macro => self.parse_macro(),
            TokenKind::Test => self.parse_test(),
            TokenKind::Mod => self.parse_mod(vis),
            TokenKind::Use => self.parse_use(),
            TokenKind::Trait => self.parse_trait(vis),
            TokenKind::Impl => self.parse_impl(),
            TokenKind::TypeKw => self.parse_type_alias(vis),
            TokenKind::Ident(ref name) if name == "patch" => self.parse_patch(vis, attributes),
            TokenKind::Ident(ref name) if name == "law" => self.parse_law(vis, attributes),
            TokenKind::Ident(ref name) if name == "axiom" => self.parse_axiom(vis, attributes),
            TokenKind::Ident(ref name) if name == "converge" => {
                self.parse_converge(vis, attributes)
            }
            TokenKind::Ident(ref name) if name == "world" => self.parse_world(vis, attributes),
            TokenKind::Ident(ref name) if name == "entangle" => {
                self.parse_entangle(vis, attributes)
            }
            TokenKind::Ident(ref name) if name == "orchestrate" => {
                self.parse_orchestrate(vis, attributes)
            }
            TokenKind::Ident(ref name) if name == "pulse" => self.parse_pulse(vis, attributes),
            TokenKind::Ident(ref name) if name == "shatter" => {
                self.parse_shatter_struct(vis, attributes)
            }
            _ => Err(self.parser_error(
                format!(
                    "Expected item (fn, patch, law, axiom, converge, world, entangle, orchestrate, pulse, shatter struct, struct, enum, actor, component, shader, material, trait, impl, mod, use, const, test), found {}",
                    self.token_to_user_string(&self.peek_kind())
                ),
                self.current_span()
            )),
        }
    }

    fn parse_mod(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Mod)?;
        let name = self.parse_ident()?;

        // `mod name` (declaration-only) or `mod name:` with inline items.
        let inline = if self.check(TokenKind::Colon) {
            self.advance();
            self.skip_newlines();

            // Check if there's an INDENT token - if not, treat as empty module
            if !self.check(TokenKind::Indent) {
                // Empty module body (e.g., `mod name:` followed by newline or another item)
                Some(vec![])
            } else {
                self.expect(TokenKind::Indent)?;

                let mut items = Vec::new();
                while !self.check(TokenKind::Dedent) && !self.at_end() {
                    self.skip_newlines();
                    if self.check(TokenKind::Dedent) {
                        break;
                    }
                    items.push(self.parse_item()?);
                    self.skip_newlines();
                }
                self.expect(TokenKind::Dedent)?;
                Some(items)
            }
        } else {
            None
        };

        Ok(Item::Mod(Mod {
            name,
            inline,
            visibility: vis,
            span: start.merge(self.current_span()),
        }))
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

            attrs.push(Attribute {
                name,
                args,
                span: start.merge(self.current_span()),
            });
            self.skip_newlines();
        }
        Ok(attrs)
    }

    fn parse_attribute_name(&mut self) -> KainResult<String> {
        match self.peek_kind() {
            TokenKind::Ident(s) => {
                self.advance();
                Ok(s)
            }
            TokenKind::Component => {
                self.advance();
                Ok("component".to_string())
            }
            TokenKind::Shader => {
                self.advance();
                Ok("shader".to_string())
            }
            TokenKind::Actor => {
                self.advance();
                Ok("actor".to_string())
            }
            TokenKind::State => {
                self.advance();
                Ok("state".to_string())
            }
            TokenKind::AsyncKw => {
                self.advance();
                Ok("async".to_string())
            }
            TokenKind::Async => {
                self.advance();
                Ok("Async".to_string())
            }
            TokenKind::Gpu => {
                self.advance();
                Ok("GPU".to_string())
            }
            TokenKind::Reactive => {
                self.advance();
                Ok("Reactive".to_string())
            }
            k => Err(self.parser_error(
                format!(
                    "Expected attribute name, got {}",
                    crate::error::token_kind_to_user_string(&k)
                ),
                self.current_span(),
            )),
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
            supertraits: Vec::new(),
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

        let first_type = self.parse_type()?;
        let (trait_name, trait_generics, target_type) = if self.check(TokenKind::For) {
            self.advance();
            let (trait_name, trait_generics) = match first_type {
                Type::Named { name, generics, .. } => (name, generics),
                other => {
                    return Err(self.parser_error(
                        format!(
                            "Expected trait path before 'for' in impl block, found {:?}",
                            other
                        ),
                        other.span(),
                    ));
                }
            };
            let target_type = self.parse_type()?;
            (Some(trait_name), trait_generics, target_type)
        } else {
            (None, Vec::new(), first_type)
        };

        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        let mut methods = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            let vis = self.parse_visibility();
            if self.check(TokenKind::Fn) {
                if let Item::Function(f) = self.parse_function(vis)? {
                    methods.push(f);
                }
            } else {
                return Err(self.parser_error(
                    format!(
                        "Expected 'fn' in impl block (impl blocks can only contain function definitions), found {}",
                        self.token_to_user_string(&self.peek_kind())
                    ),
                    self.current_span()
                ));
            }
            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::Impl(Impl {
            generics,
            trait_name,
            trait_generics,
            target_type,
            methods,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_use(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Use)?;

        let mut path = Vec::new();
        path.push(self.parse_use_path_segment()?);

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
                    span: start.merge(self.current_span()),
                }));
            }

            path.push(self.parse_use_path_segment()?);
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
            span: start.merge(self.current_span()),
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
        Ok(Item::Test(TestDef {
            name,
            body,
            span: start.merge(self.current_span()),
        }))
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
            params.push(MacroParam {
                name: p_name,
                kind,
                span: self.current_span(),
            });

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
        } else {
            None
        };

        let effects = self.parse_effects()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;

        Ok(Item::Function(Function {
            name,
            generics,
            params,
            return_type,
            effects,
            body,
            visibility: vis,
            attributes: vec![],
            span: start.merge(body_span),
        }))
    }

    // Wrapper to parse function with pre-collected attributes
    fn parse_function_with_attrs(
        &mut self,
        vis: Visibility,
        attrs: Vec<Attribute>,
    ) -> KainResult<Item> {
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
        } else {
            None
        };
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
            name,
            generics,
            params,
            return_type,
            effects,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(body_span),
        }))
    }

    fn parse_async_function(&mut self, vis: Visibility) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::AsyncKw)?; // consume 'async'
        self.expect(TokenKind::Fn)?; // consume 'fn'
        let name = self.parse_ident()?;

        // Parse generics: <T, U: Bound>
        let generics = self.parse_generics()?;

        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;

        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };

        // Parse other effects, then add Async
        let mut effects = self.parse_effects()?;
        effects.push(crate::effects::Effect::Async);

        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;

        Ok(Item::Function(Function {
            name,
            generics,
            params,
            return_type,
            effects,
            body,
            visibility: vis,
            attributes: vec![],
            span: start.merge(body_span),
        }))
    }

    fn parse_patch(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("patch")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;
        Ok(Item::Patch(PatchDef {
            name,
            params,
            return_type,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(body_span),
        }))
    }

    fn parse_law(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("law")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        self.expect(TokenKind::Arrow)?;
        let return_type = self.parse_type()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Item::Law(LawDef {
            name,
            params,
            return_type,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_axiom(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("axiom")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        let mut predicates = Vec::new();
        let mut guarantees = Vec::new();
        let mut fallback = None;

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            match self.peek_kind() {
                TokenKind::Ident(ref keyword) if keyword == "when" => {
                    self.advance();
                    predicates.push(self.parse_axiom_predicate()?);
                }
                TokenKind::Ident(ref keyword) if keyword == "guarantee" => {
                    self.advance();
                    guarantees.push(self.parse_string_like_argument("axiom guarantee")?);
                }
                TokenKind::Ident(ref keyword) if keyword == "fallback" => {
                    self.advance();
                    if fallback.is_some() {
                        return Err(self.parser_error(
                            "axiom blocks may only declare one fallback",
                            self.current_span(),
                        ));
                    }
                    fallback = Some(self.parse_string_like_argument("axiom fallback")?);
                }
                _ => {
                    return Err(self.parser_error(
                        "axiom blocks expect 'when', 'guarantee', or 'fallback'",
                        self.current_span(),
                    ))
                }
            }
            self.skip_newlines();
        }

        self.expect(TokenKind::Dedent)?;
        Ok(Item::Axiom(AxiomDef {
            name,
            predicates,
            guarantees,
            fallback,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_axiom_predicate(&mut self) -> KainResult<AxiomPredicate> {
        let predicate_name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let value = self.parse_string_like_argument("axiom predicate")?;
        self.expect(TokenKind::RParen)?;
        match predicate_name.as_str() {
            "target" => Ok(AxiomPredicate::Target(value)),
            "arch" => Ok(AxiomPredicate::Arch(value)),
            "capability" => Ok(AxiomPredicate::Capability(value)),
            _ => Err(self.parser_error(
                format!(
                    "Unknown axiom predicate '{}'; expected target(...), arch(...), or capability(...)",
                    predicate_name
                ),
                self.current_span(),
            )),
        }
    }

    fn parse_pulse(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("pulse")?;
        let name = self.parse_ident()?;
        self.expect_contextual_ident("every")?;
        let interval = self.parse_pulse_duration()?;
        let jitter = if self.peek_contextual_ident("jitter") {
            self.advance();
            Some(self.parse_pulse_duration()?)
        } else {
            None
        };
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;
        Ok(Item::Pulse(PulseDef {
            name,
            interval,
            jitter,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(body_span),
        }))
    }

    fn parse_pulse_duration(&mut self) -> KainResult<PulseDuration> {
        let start = self.current_span();
        let value = match self.peek_kind() {
            TokenKind::Int(value) => {
                self.advance();
                value
            }
            _ => {
                return Err(self.parser_error(
                    "pulse duration expects an integer like 16ms, 250us, or 1s",
                    self.current_span(),
                ))
            }
        };
        let unit = self.parse_ident()?;
        Ok(PulseDuration {
            value,
            unit,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_orchestrate(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("orchestrate")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        let body_span = body.span;
        Ok(Item::Orchestrate(OrchestrateDef {
            name,
            params,
            return_type,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(body_span),
        }))
    }

    fn parse_converge(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("converge")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::LParen)?;
        let params = self.parse_params()?;
        self.expect(TokenKind::RParen)?;
        let return_type = if self.check(TokenKind::Arrow) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        let mut spec_lane = None;
        let mut fast_lanes = Vec::new();
        let mut verify_random_count = None;

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            match self.peek_kind() {
                TokenKind::Ident(ref name) if name == "spec" => {
                    if spec_lane.is_some() {
                        return Err(self.parser_error(
                            "converge blocks may only declare one spec lane",
                            self.current_span(),
                        ));
                    }
                    spec_lane = Some(self.parse_converge_lane(ConvergeLaneKind::Spec)?);
                }
                TokenKind::Ident(ref name) if name == "fast" => {
                    fast_lanes.push(self.parse_converge_lane(ConvergeLaneKind::Fast)?);
                }
                TokenKind::Ident(ref name) if name == "verify" => {
                    verify_random_count = Some(self.parse_converge_verify_random_count()?);
                }
                _ => {
                    return Err(self.parser_error(
                        "Expected 'spec', 'fast', or 'verify' inside converge block",
                        self.current_span(),
                    ))
                }
            }
            self.skip_newlines();
        }

        self.expect(TokenKind::Dedent)?;

        let spec_lane = spec_lane.ok_or_else(|| {
            self.parser_error(
                "converge blocks require exactly one spec lane",
                start.merge(self.current_span()),
            )
        })?;
        if fast_lanes.is_empty() {
            return Err(self.parser_error(
                "converge blocks require at least one fast lane",
                start.merge(self.current_span()),
            ));
        }

        Ok(Item::Converge(ConvergeDef {
            name,
            params,
            return_type,
            spec_lane,
            fast_lanes,
            verify_random_count,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_world(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("world")?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        let mut states = Vec::new();
        let mut surfaces = Vec::new();
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            match self.peek_kind() {
                TokenKind::State => {
                    states.push(self.parse_world_state_slot()?);
                }
                TokenKind::Ident(ref entry) if entry == "surface" => {
                    surfaces.push(self.parse_world_surface_projection()?);
                }
                _ => {
                    return Err(self.parser_error(
                        "Expected 'state' or 'surface' inside world block",
                        self.current_span(),
                    ))
                }
            }
            self.skip_newlines();
        }

        self.expect(TokenKind::Dedent)?;
        Ok(Item::World(WorldDef {
            name,
            states,
            surfaces,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_converge_lane(&mut self, kind: ConvergeLaneKind) -> KainResult<ConvergeLane> {
        let start = self.current_span();
        match kind {
            ConvergeLaneKind::Spec => self.expect_contextual_ident("spec")?,
            ConvergeLaneKind::Fast => self.expect_contextual_ident("fast")?,
        }
        let lane_name = self.parse_ident()?;
        let selector = if self.peek_contextual_ident("when") {
            self.advance();
            Some(self.parse_converge_selector()?)
        } else {
            None
        };
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(ConvergeLane {
            kind,
            lane_name,
            selector,
            span: start.merge(body.span),
            body,
        })
    }

    fn parse_converge_selector(&mut self) -> KainResult<ConvergeSelector> {
        if self.peek_contextual_ident("target") {
            self.advance();
            self.expect(TokenKind::LParen)?;
            let value = self.parse_string_like_argument("target selector")?;
            self.expect(TokenKind::RParen)?;
            Ok(ConvergeSelector::Target(value))
        } else if self.peek_contextual_ident("capability") {
            self.advance();
            self.expect(TokenKind::LParen)?;
            let value = self.parse_string_like_argument("capability selector")?;
            self.expect(TokenKind::RParen)?;
            Ok(ConvergeSelector::Capability(value))
        } else {
            Err(self.parser_error(
                "Expected converge selector 'target(\"...\")' or 'capability(\"...\")'",
                self.current_span(),
            ))
        }
    }

    fn parse_converge_verify_random_count(&mut self) -> KainResult<u32> {
        self.expect_contextual_ident("verify")?;
        self.expect_contextual_ident("random")?;
        self.expect(TokenKind::LParen)?;
        let count = match self.peek_kind() {
            TokenKind::Int(value) if value >= 0 => {
                self.advance();
                value as u32
            }
            _ => {
                return Err(self.parser_error(
                    "verify random(...) expects a non-negative integer sample count",
                    self.current_span(),
                ))
            }
        };
        self.expect(TokenKind::RParen)?;
        Ok(count)
    }

    fn parse_world_state_slot(&mut self) -> KainResult<WorldStateSlot> {
        let start = self.current_span();
        self.expect(TokenKind::State)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        let ty = self.parse_type()?;
        self.expect(TokenKind::Eq)?;
        let initial = self.parse_expr()?;
        Ok(WorldStateSlot {
            name,
            ty,
            initial,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_world_surface_projection(&mut self) -> KainResult<WorldSurfaceProjection> {
        let start = self.current_span();
        self.expect_contextual_ident("surface")?;
        let kind = match self.peek_kind() {
            TokenKind::Ident(ref value) if value == "native_ui" => {
                self.advance();
                WorldSurfaceKind::NativeUi
            }
            TokenKind::Ident(ref value) if value == "viewport3d" => {
                self.advance();
                WorldSurfaceKind::Viewport3d
            }
            TokenKind::Ident(ref value) if value == "web" => {
                self.advance();
                WorldSurfaceKind::Web
            }
            TokenKind::Ident(ref value) if value == "ue5" => {
                self.advance();
                WorldSurfaceKind::Ue5
            }
            _ => {
                return Err(self.parser_error(
                    "Expected world surface kind native_ui, viewport3d, web, or ue5",
                    self.current_span(),
                ))
            }
        };
        self.expect(TokenKind::FatArrow)?;
        let expr = self.parse_expr()?;
        Ok(WorldSurfaceProjection {
            kind,
            expr,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_entangle(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("entangle")?;
        let left = self.parse_entangle_endpoint()?;
        self.expect(TokenKind::Lt)?;
        self.expect(TokenKind::Arrow)?;
        let right = self.parse_entangle_endpoint()?;
        self.expect(TokenKind::With)?;
        let policy = match self.peek_kind() {
            TokenKind::Ident(ref value) if value == kain_entangle::SINGLE_WRITER_POLICY => {
                self.advance();
                EntanglePolicy::SingleWriter
            }
            _ => {
                return Err(self.parser_error(
                    "Expected entangle policy 'single_writer'",
                    self.current_span(),
                ))
            }
        };
        Ok(Item::Entangle(EntangleDef {
            left,
            right,
            policy,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_entangle_endpoint(&mut self) -> KainResult<EntangleEndpoint> {
        let start = self.current_span();
        let mut segments = vec![self.parse_ident()?];
        while self.check(TokenKind::Dot) {
            self.advance();
            segments.push(self.parse_ident()?);
        }
        if segments.len() < 2 {
            return Err(self.parser_error(
                "Entangle endpoint must be a stable dotted lvalue path like World.state",
                start,
            ));
        }
        Ok(EntangleEndpoint {
            segments,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_string_like_argument(&mut self, label: &str) -> KainResult<String> {
        match self.peek_kind() {
            TokenKind::String(ref value) => {
                let value = value.clone();
                self.advance();
                Ok(value)
            }
            TokenKind::Ident(ref value) => {
                let value = value.clone();
                self.advance();
                Ok(value)
            }
            _ => Err(self.parser_error(
                format!("{label} expects a string or identifier"),
                self.current_span(),
            )),
        }
    }
    #[allow(dead_code)]
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
            if self.check(TokenKind::Dedent) {
                break;
            }

            if self.check(TokenKind::Fn) {
                // Parse method
                if let Item::Function(f) = self.parse_function(Visibility::Private)? {
                    methods.push(f);
                }
            } else if self.check(TokenKind::State) {
                self.advance();
                let name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Eq)?;
                let initial = self.parse_expr()?;
                state.push(StateDecl {
                    name,
                    ty,
                    initial,
                    weak: false,
                    attributes: vec![],
                    span: self.current_span(),
                });
            } else if let TokenKind::Ident(ref s) = self.peek_kind() {
                if s == "state" {
                    self.advance();
                    let name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let ty = self.parse_type()?;
                    self.expect(TokenKind::Eq)?;
                    let initial = self.parse_expr()?;
                    state.push(StateDecl {
                        name,
                        ty,
                        initial,
                        weak: false,
                        attributes: vec![],
                        span: self.current_span(),
                    });
                } else if s == "weak" {
                    self.advance();
                    if self.check(TokenKind::State)
                        || self.check(TokenKind::Ident("state".to_string()))
                    {
                        // Check specifically for state
                        // "weak state name: Type = ..."
                        self.advance();
                        let name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;
                        let ty = self.parse_type()?;
                        self.expect(TokenKind::Eq)?;
                        let initial = self.parse_expr()?;
                        state.push(StateDecl {
                            name,
                            ty,
                            initial,
                            weak: true,
                            attributes: vec![],
                            span: self.current_span(),
                        });
                    } else {
                        return Err(self.parser_error(
                             format!(
                                 "Expected 'state' keyword after 'weak' in component (use 'weak state name: Type = value'), found {}",
                                 self.token_to_user_string(&self.peek_kind())
                             ),
                             self.current_span()
                         ));
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
                    return Err(self.parser_error(
                        format!(
                            "Unexpected identifier '{}' in component. Valid keywords: 'state', 'weak', 'render', or 'fn' for methods",
                            s
                        ),
                        self.current_span()
                    ));
                }
            } else if self.check(TokenKind::Lt) {
                // Direct JSX element (implicit render)
                body = Some(self.parse_jsx_element()?);
            } else {
                return Err(self.parser_error(
                    format!(
                        "Unexpected token in component: {}. Expected 'state', 'weak', 'render', 'fn', or JSX element",
                        crate::error::token_kind_to_user_string(&self.peek_kind())
                    ),
                    self.current_span()
                ));
            }
            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        let body = body.ok_or_else(|| {
            self.parser_error(
                "Component must have a render body (JSX element)",
                self.current_span(),
            )
        })?;

        Ok(Item::Component(Component {
            name,
            props,
            state,
            methods,
            effects,
            body,
            visibility: vis,
            attributes: vec![],
            span: start.merge(self.current_span()),
        }))
    }

    // Wrapper to parse component with pre-collected attributes
    fn parse_component_with_attrs(
        &mut self,
        vis: Visibility,
        attrs: Vec<Attribute>,
    ) -> KainResult<Item> {
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    state.push(StateDecl {
                        name,
                        ty,
                        initial,
                        weak: false,
                        attributes: vec![],
                        span: self.current_span(),
                    });
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
                    return Err(self.parser_error(
                        format!("Unexpected identifier in component: {}", s),
                        self.current_span(),
                    ));
                }
            } else if self.check(TokenKind::State) {
                self.advance();
                let name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Eq)?;
                let initial = self.parse_expr()?;
                state.push(StateDecl {
                    name,
                    ty,
                    initial,
                    weak: false,
                    attributes: vec![],
                    span: self.current_span(),
                });
            } else if self.check(TokenKind::Lt) {
                body = Some(self.parse_jsx_element()?);
            } else {
                return Err(self.parser_error(
                    format!(
                        "Unexpected token in component: {}",
                        crate::error::token_kind_to_user_string(&self.peek_kind())
                    ),
                    self.current_span(),
                ));
            }
            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        let body = body.ok_or_else(|| {
            self.parser_error("Component must have a render body", self.current_span())
        })?;

        Ok(Item::Component(Component {
            name,
            props,
            state,
            methods,
            effects,
            body,
            visibility: vis,
            attributes: attrs,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_shader(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Shader)?;

        let stage = if self.check(TokenKind::Vertex) {
            self.advance();
            ShaderStage::Vertex
        } else if self.check(TokenKind::Fragment) {
            self.advance();
            ShaderStage::Fragment
        } else if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s == "compute" {
                self.advance();
                ShaderStage::Compute
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    return Err(self.parser_error(
                        format!(
                            "Expected integer binding after '@' (e.g., '@0', '@1', '@2'), found {}",
                            self.token_to_user_string(&self.peek_kind())
                        ),
                        self.current_span(),
                    ));
                };

                uniforms.push(Uniform {
                    name: u_name,
                    ty: u_ty,
                    binding,
                    span: self.current_span(),
                });
            } else {
                stmts.push(self.parse_stmt()?);
            }
            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        let body = Block {
            stmts,
            span: block_start.merge(self.current_span()),
        };
        let body_span = body.span;

        let shader = Shader {
            name,
            stage,
            inputs,
            outputs,
            uniforms,
            body,
            span: start.merge(body_span),
        };

        if matches!(shader.stage, ShaderStage::Compute) {
            if let Err(err) = shader.explicit_compute_metadata() {
                return Err(self.parser_error(
                    format!(
                        "Invalid explicit compute metadata in shader '{}': {}",
                        shader.name, err
                    ),
                    shader.span,
                ));
            }
        }

        Ok(Item::Shader(shader))
    }

    fn parse_shatter_struct(
        &mut self,
        vis: Visibility,
        mut attrs: Vec<Attribute>,
    ) -> KainResult<Item> {
        let start = self.current_span();
        self.expect_contextual_ident("shatter")?;
        attrs.push(Attribute {
            name: SHATTER_ATTRIBUTE_NAME.to_string(),
            args: Vec::new(),
            span: start,
        });
        self.parse_struct_with_attrs(vis, attrs)
    }

    fn parse_struct_with_attrs(
        &mut self,
        vis: Visibility,
        attrs: Vec<Attribute>,
    ) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Struct)?;
        let name = self.parse_ident()?;

        let generics = self.parse_generics()?;

        self.expect(TokenKind::Colon)?;
        self.skip_newlines();
        let mut fields = Vec::new();
        let mut methods = Vec::new();

        // Allow opaque/forward struct declarations:
        //   struct FILE:
        // with no indented body.
        if !self.check(TokenKind::Indent) {
            return Ok(Item::Struct(Struct {
                name,
                generics,
                fields,
                methods,
                attributes: attrs,
                visibility: vis,
                span: start.merge(self.current_span()),
            }));
        }
        self.expect(TokenKind::Indent)?;

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                } else {
                    false
                }
            } else {
                false
            };

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
                span: self.current_span(),
            });
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::Struct(Struct {
            name,
            generics,
            fields,
            methods,
            attributes: attrs,
            visibility: vis,
            span: start.merge(self.current_span()),
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
            if self.check(TokenKind::Dedent) {
                break;
            }
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
                let indented = if self.check(TokenKind::Indent) {
                    self.advance();
                    true
                } else {
                    false
                };

                let mut fields = Vec::new();
                while !self.check(TokenKind::RBrace) && !self.at_end() {
                    if indented && self.check(TokenKind::Dedent) {
                        break;
                    }
                    if !indented && self.check(TokenKind::RBrace) {
                        break;
                    }

                    self.skip_newlines();
                    if self.check(TokenKind::RBrace) || (indented && self.check(TokenKind::Dedent))
                    {
                        break;
                    }

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
                        span: self.current_span(),
                    });

                    if !self.check(TokenKind::RBrace) {
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

            variants.push(Variant {
                name: vname,
                fields,
                span,
            });
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::Enum(Enum {
            name,
            generics,
            variants,
            visibility: vis,
            span: start.merge(self.current_span()),
        }))
    }

    #[allow(dead_code)]
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                state.push(StateDecl {
                    name,
                    ty,
                    initial,
                    weak: false,
                    attributes: method_attributes,
                    span: self.current_span(),
                });
            } else if self.check(TokenKind::Var) {
                // Support 'var' as alias for 'state' in actors
                self.advance();
                let name = self.parse_ident()?;
                self.expect(TokenKind::Colon)?;
                let ty = self.parse_type()?;
                self.expect(TokenKind::Eq)?;
                let initial = self.parse_expr()?;
                state.push(StateDecl {
                    name,
                    ty,
                    initial,
                    weak: false,
                    attributes: method_attributes,
                    span: self.current_span(),
                });
            } else if self.check(TokenKind::Fn) {
                // Parse method function with pre-parsed attributes
                if let Item::Function(func) =
                    self.parse_function_with_attrs(Visibility::Public, method_attributes)?
                {
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
                    state.push(StateDecl {
                        name,
                        ty,
                        initial,
                        weak: true,
                        attributes: method_attributes,
                        span: self.current_span(),
                    });
                } else if s == "on" {
                    self.advance();
                    let message_type = self.parse_ident()?;
                    self.expect(TokenKind::LParen)?;
                    let params = self.parse_params()?;
                    self.expect(TokenKind::RParen)?;
                    self.expect(TokenKind::Colon)?;
                    let body = self.parse_block()?;
                    handlers.push(MessageHandler {
                        message_type,
                        params,
                        body,
                        span: self.current_span(),
                    });
                } else {
                    return Err(self.parser_error(
                         format!(
                             "Unexpected identifier '{}' in actor. Valid keywords: 'state', 'var', 'fn', or 'on' for message handlers",
                             s
                         ),
                         self.current_span()
                     ));
                }
            } else {
                return Err(self.parser_error(
                    format!(
                        "Expected 'state', 'var', 'fn', or 'on' in actor definition, found {}",
                        self.token_to_user_string(&self.peek_kind())
                    ),
                    self.current_span(),
                ));
            }

            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        let span = start.merge(self.current_span());
        Ok(Item::Actor(Actor {
            name,
            state,
            handlers,
            methods,
            attributes,
            span,
        }))
    }

    fn parse_const(&mut self, vis: Visibility, attrs: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Const)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::Colon)?;
        let ty = self.parse_type()?;
        self.expect(TokenKind::Eq)?;
        let value = self.parse_expr()?;
        Ok(Item::Const(Const {
            name,
            ty,
            value,
            attributes: attrs,
            visibility: vis,
            span: start.merge(self.current_span()),
        }))
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
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_comptime_block(&mut self) -> KainResult<Item> {
        let start = self.current_span();
        self.expect(TokenKind::Comptime)?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Item::Comptime(ComptimeBlock {
            body,
            span: start.merge(self.current_span()),
        }))
    }

    fn parse_material_graph(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'material' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "material" {
                return Err(self.parser_error(
                    format!(
                        "Expected 'material' keyword after @material_graph attribute, found identifier '{}'",
                        s
                    ),
                    self.current_span()
                ));
            }
            self.advance(); // consume 'material'
        } else {
            return Err(self.parser_error(
                format!(
                    "Expected 'material' keyword after @material_graph attribute, found {}",
                    self.token_to_user_string(&self.peek_kind())
                ),
                self.current_span(),
            ));
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                            format!(
                                "Unexpected identifier '{}' in material graph body. Valid keywords: 'input' (for parameters), 'let' (for intermediate values), or 'output' (for material properties like base_color, roughness)",
                                s
                            ),
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
                    format!(
                        "Expected 'input', 'let', or 'output' in material graph body, found {}",
                        self.token_to_user_string(&self.peek_kind())
                    ),
                    self.current_span(),
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
            return Err(self.parser_error(
                "Expected 'fn' keyword after @material_function",
                self.current_span(),
            ));
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    self.current_span(),
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
            self.parser_error(
                "Material function must have a 'return' statement",
                self.current_span(),
            )
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
                return Err(self.parser_error(
                    format!(
                        "Expected 'graph' keyword after @graph_editor attribute, found identifier '{}'. Usage: @graph_editor\ngraph MyGraph:",
                        s
                    ),
                    self.current_span()
                ));
            }
            self.advance(); // consume 'graph'
        } else {
            return Err(self.parser_error(
                format!(
                    "Expected 'graph' keyword after @graph_editor attribute, found {}",
                    self.token_to_user_string(&self.peek_kind())
                ),
                self.current_span(),
            ));
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
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for @node_type or @schema
            let node_attrs = self.parse_attributes()?;

            if node_attrs.iter().any(|a| a.name == "node_type") {
                node_types.push(self.parse_node_type(node_attrs)?);
            } else if node_attrs.iter().any(|a| a.name == "schema") {
                schema = Some(self.parse_graph_schema(node_attrs)?);
            } else {
                return Err(self.parser_error(
                    format!(
                        "Expected @node_type or @schema attribute in graph editor body, found {}. Graph editors must define node types with @node_type and optionally a @schema",
                        self.token_to_user_string(&self.peek_kind())
                    ),
                    self.current_span()
                ));
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
                return Err(self.parser_error(
                    "Expected 'node' keyword after @node_type",
                    self.current_span(),
                ));
            }
            self.advance();
        } else {
            return Err(self.parser_error(
                "Expected 'node' keyword after @node_type",
                self.current_span(),
            ));
        }

        // Parse name
        let name = self.parse_ident()?;

        // Extract category from attributes
        let category = attributes
            .iter()
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                        return Err(self.parser_error(
                            "Expected 'inputs', 'outputs', or 'properties'",
                            self.current_span(),
                        ));
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                return Err(self.parser_error(
                    "Expected 'schema' keyword after @schema",
                    self.current_span(),
                ));
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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

            params.push(Param {
                name,
                ty,
                mutable,
                default,
                span: self.current_span(),
            });

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
                    bounds.push(TypeBound {
                        trait_name: bound_name,
                        span: self.current_span(),
                    });
                    if !self.check(TokenKind::Plus) {
                        break;
                    }
                    self.advance(); // consume +
                }
            }

            generics.push(Generic {
                name,
                bounds,
                span: start.merge(self.current_span()),
            });

            if !self.check(TokenKind::Gt) {
                self.expect(TokenKind::Comma)?;
            }
        }

        self.expect(TokenKind::Gt)?; // consume >
        Ok(generics)
    }

    #[allow(dead_code)]
    fn parse_generics_as_types(&mut self) -> KainResult<Vec<Type>> {
        let mut generics = Vec::new();
        if !self.check(TokenKind::Lt) {
            return Ok(generics);
        }
        self.advance();
        while !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) && !self.at_end() {
            generics.push(self.parse_type()?);
            if !self.check(TokenKind::Gt) && !self.check(TokenKind::Shr) {
                self.expect(TokenKind::Comma)?;
            }
        }
        if self.check(TokenKind::Shr) {
            let shr_span = self.current_span();
            self.advance();
            self.inject_token(Token::new(TokenKind::Gt, shr_span));
        } else {
            self.expect(TokenKind::Gt)?;
        }
        Ok(generics)
    }

    fn parse_effects(&mut self) -> KainResult<Vec<Effect>> {
        let mut effects = Vec::new();
        if self.check(TokenKind::With) {
            self.advance();
            loop {
                // Effects are keywords, not identifiers
                let effect = match self.peek_kind() {
                    TokenKind::Pure => {
                        self.advance();
                        Some(Effect::Pure)
                    }
                    TokenKind::Io => {
                        self.advance();
                        Some(Effect::IO)
                    }
                    TokenKind::Async => {
                        self.advance();
                        Some(Effect::Async)
                    }
                    TokenKind::Gpu => {
                        self.advance();
                        Some(Effect::GPU)
                    }
                    TokenKind::Reactive => {
                        self.advance();
                        Some(Effect::Reactive)
                    }
                    TokenKind::Unsafe => {
                        self.advance();
                        Some(Effect::Unsafe)
                    }
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
                if !self.check(TokenKind::Comma) {
                    break;
                }
                self.advance();
            }
        }
        Ok(effects)
    }

    fn parse_type(&mut self) -> KainResult<Type> {
        let span = self.current_span();

        if self.check(TokenKind::Amp) {
            self.advance();
            let mutable = if self.check(TokenKind::Mut) {
                self.advance();
                true
            } else {
                false
            };
            let lifetime = self.parse_optional_ref_lifetime();
            let inner = self.parse_type()?;
            return Ok(Type::Ref {
                mutable,
                inner: Box::new(inner),
                lifetime,
                span: span.merge(self.current_span()),
            });
        }

        if self.check(TokenKind::LBracket) {
            self.advance();
            let inner = self.parse_type()?;
            if self.check(TokenKind::Semi) {
                self.advance();
                let TokenKind::Int(size) = self.peek_kind() else {
                    return Err(
                        self.parser_error("Expected array size integer", self.current_span())
                    );
                };
                self.advance();
                self.expect(TokenKind::RBracket)?;
                return Ok(Type::Array(
                    Box::new(inner),
                    size as usize,
                    span.merge(self.current_span()),
                ));
            }

            self.expect(TokenKind::RBracket)?;
            return Ok(Type::Slice(
                Box::new(inner),
                span.merge(self.current_span()),
            ));
        }

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
                if self.check(TokenKind::RParen) {
                    break;
                } // trailing comma
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
        let name = self.parse_path_name()?;

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

        if (name == "ptr" || name == "ptr_mut") && type_args.len() == 1 {
            return Ok(Type::Ptr {
                mutable: name == "ptr_mut",
                inner: Box::new(type_args.into_iter().next().unwrap()),
                provenance: crate::ast::PointerProvenance::Raw,
                span,
            });
        }

        Ok(Type::Named {
            name,
            generics: type_args,
            span,
        })
    }

    fn parse_block(&mut self) -> KainResult<Block> {
        self.skip_newlines();
        let start = self.current_span();
        self.expect(TokenKind::Indent)?;

        let mut stmts = Vec::new();
        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent)
                || self.check(TokenKind::Elif)
                || self.check(TokenKind::Else)
            {
                break;
            }
            stmts.push(self.parse_stmt()?);
            self.skip_newlines();
        }
        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Block {
            stmts,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_stmt(&mut self) -> KainResult<Stmt> {
        if self.is_item_start() {
            return Ok(Stmt::Item(Box::new(self.parse_item()?)));
        }

        match self.peek_kind() {
            TokenKind::Let => self.parse_let(),
            TokenKind::Var => self.parse_var(),
            TokenKind::Return => self.parse_return(),
            TokenKind::For => self.parse_for(),
            TokenKind::Fanout => self.parse_fanout(),
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
        let ty = if self.check(TokenKind::Colon) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect(TokenKind::Eq)?;
        let value = Some(self.parse_expr()?);
        Ok(Stmt::Let {
            pattern,
            ty,
            value,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_var(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Var)?;
        let name = self.parse_ident()?;
        let ty = if self.check(TokenKind::Colon) {
            self.advance();
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect(TokenKind::Eq)?;
        let value = Some(self.parse_expr()?);
        // var x = val is effectively let mut x = val
        let pattern = Pattern::Binding {
            name,
            mutable: true,
            span: start,
        };
        Ok(Stmt::Let {
            pattern,
            ty,
            value,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_return(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Return)?;
        let value = if !self.check_line_end() {
            Some(self.parse_expr()?)
        } else {
            None
        };
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
        Ok(Stmt::For {
            binding: Pattern::Binding {
                name,
                mutable: false,
                span: start,
            },
            iter,
            body,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_fanout(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Fanout)?;
        let name = self.parse_ident()?;
        self.expect(TokenKind::In)?;
        let iter = self.parse_expr()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::Fanout {
            binding: Pattern::Binding {
                name,
                mutable: false,
                span: start,
            },
            iter,
            body,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_while(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::While)?;
        let condition = self.parse_expr()?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::While {
            condition,
            body,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_loop(&mut self) -> KainResult<Stmt> {
        let start = self.current_span();
        self.expect(TokenKind::Loop)?;
        self.expect(TokenKind::Colon)?;
        let body = self.parse_block()?;
        Ok(Stmt::Loop {
            body,
            span: start.merge(self.current_span()),
        })
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
    fn parse_expr(&mut self) -> KainResult<Expr> {
        self.parse_assignment()
    }

    fn parse_assignment(&mut self) -> KainResult<Expr> {
        let target = self.parse_conditional()?;

        if let Some(assign_binop) = self.get_assignment_binop() {
            self.advance();
            let rhs = self.parse_assignment()?;
            let span = target.span().merge(rhs.span());

            let value = if let Some(op) = assign_binop {
                Expr::Binary {
                    left: Box::new(target.clone()),
                    op,
                    right: Box::new(rhs),
                    span,
                }
            } else {
                rhs
            };

            Ok(Expr::Assign {
                target: Box::new(target),
                value: Box::new(value),
                span,
            })
        } else {
            Ok(target)
        }
    }

    fn parse_conditional(&mut self) -> KainResult<Expr> {
        let condition = self.parse_range_expr()?;
        if !self.check(TokenKind::Question) {
            return Ok(condition);
        }

        self.advance(); // '?'
        let then_expr = self.parse_assignment()?;
        self.expect(TokenKind::Colon)?;
        let else_expr = self.parse_assignment()?;

        let then_span = then_expr.span();
        let else_span = else_expr.span();
        let span = condition.span().merge(else_span);
        Ok(Expr::Match {
            scrutinee: Box::new(condition),
            arms: vec![
                MatchArm {
                    pattern: Pattern::Literal(Expr::Bool(true, then_span)),
                    guard: None,
                    body: then_expr,
                    span: then_span,
                },
                MatchArm {
                    pattern: Pattern::Literal(Expr::Bool(false, else_span)),
                    guard: None,
                    body: else_expr,
                    span: else_span,
                },
            ],
            span,
        })
    }

    fn parse_range_expr(&mut self) -> KainResult<Expr> {
        if self.check(TokenKind::DotDot) {
            let start_span = self.current_span();
            self.advance();
            let inclusive = if self.check(TokenKind::Eq) {
                self.advance();
                true
            } else {
                false
            };
            let end = if self.range_expr_end_is_omitted() {
                None
            } else {
                Some(Box::new(self.parse_coalesce()?))
            };
            let span = end
                .as_ref()
                .map(|value| start_span.merge(value.span()))
                .unwrap_or(start_span);
            return Ok(Expr::Range {
                start: None,
                end,
                inclusive,
                span,
            });
        }

        let start = self.parse_coalesce()?;
        if !self.check(TokenKind::DotDot) {
            return Ok(start);
        }

        let start_span = start.span();
        self.advance();
        let inclusive = if self.check(TokenKind::Eq) {
            self.advance();
            true
        } else {
            false
        };
        let end = if self.range_expr_end_is_omitted() {
            None
        } else {
            Some(Box::new(self.parse_coalesce()?))
        };
        let span = end
            .as_ref()
            .map(|value| start_span.merge(value.span()))
            .unwrap_or(start_span.merge(self.current_span()));
        Ok(Expr::Range {
            start: Some(Box::new(start)),
            end,
            inclusive,
            span,
        })
    }

    fn parse_coalesce(&mut self) -> KainResult<Expr> {
        let left = self.parse_binary(0)?;
        if self.check(TokenKind::QuestionQuestion) {
            self.advance();
            let right = self.parse_coalesce()?;
            let span = left.span().merge(right.span());
            self.make_null_coalesce_expr(left, right, span)
        } else {
            Ok(left)
        }
    }

    fn parse_binary(&mut self, min_prec: u8) -> KainResult<Expr> {
        let mut left = self.parse_unary()?;

        while let Some((op, prec)) = self.get_binary_op() {
            if prec < min_prec {
                break;
            }
            self.advance();
            let right = self.parse_binary(prec + 1)?;
            let span = left.span().merge(right.span());
            left = Expr::Binary {
                left: Box::new(left),
                op,
                right: Box::new(right),
                span,
            };
        }
        Ok(left)
    }

    fn parse_unary(&mut self) -> KainResult<Expr> {
        match self.peek_kind() {
            TokenKind::Observe => self.parse_scoped_ownership_expr(OBSERVE_KEYWORD),
            TokenKind::Collapse => self.parse_scoped_ownership_expr(COLLAPSE_KEYWORD),
            TokenKind::Decay => self.parse_decay_expr(),
            TokenKind::Share => self.parse_scoped_ownership_expr(SHARE_KEYWORD),
            TokenKind::Ident(ref name) if name == "teleport" => self.parse_teleport_expr(),
            TokenKind::Amp => {
                let s = self.current_span();
                self.advance();
                let mutable = if self.check(TokenKind::Mut) {
                    self.advance();
                    true
                } else {
                    false
                };
                Ok(Expr::Ref {
                    mutable,
                    value: Box::new(self.parse_unary()?),
                    span: s,
                })
            }
            TokenKind::PlusPlus => {
                let start = self.current_span();
                self.advance();
                let target = self.parse_unary()?;
                let span = start.merge(target.span());
                self.make_incdec_expr(target, true, true, span)
            }
            TokenKind::MinusMinus => {
                let start = self.current_span();
                self.advance();
                let target = self.parse_unary()?;
                let span = start.merge(target.span());
                self.make_incdec_expr(target, false, true, span)
            }
            TokenKind::Minus => {
                let s = self.current_span();
                self.advance();
                Ok(Expr::Unary {
                    op: UnaryOp::Neg,
                    operand: Box::new(self.parse_unary()?),
                    span: s,
                })
            }
            TokenKind::Not => {
                let s = self.current_span();
                self.advance();
                Ok(Expr::Unary {
                    op: UnaryOp::Not,
                    operand: Box::new(self.parse_unary()?),
                    span: s,
                })
            }
            TokenKind::Tilde => {
                let s = self.current_span();
                self.advance();
                Ok(Expr::Unary {
                    op: UnaryOp::BitNot,
                    operand: Box::new(self.parse_unary()?),
                    span: s,
                })
            }
            TokenKind::Star => {
                let s = self.current_span();
                self.advance();
                Ok(Expr::Deref(Box::new(self.parse_unary()?), s))
            }
            TokenKind::Await => {
                let start = self.current_span();
                self.advance();
                let expr = self.parse_unary()?; // Right-associative: await await x
                Ok(Expr::Await(
                    Box::new(expr),
                    start.merge(self.current_span()),
                ))
            }
            TokenKind::AsyncKw => {
                let start = self.current_span();
                self.advance();
                let body = if self.check(TokenKind::Colon) {
                    self.advance();
                    Expr::Block(self.parse_block()?, start)
                } else {
                    self.parse_unary()?
                };
                Ok(Expr::AsyncBlock(
                    Box::new(body),
                    start.merge(self.current_span()),
                ))
            }
            TokenKind::Send => {
                let start = self.current_span();
                self.advance();
                let expr = self.parse_postfix()?;

                match expr {
                    Expr::MethodCall {
                        receiver,
                        method,
                        args,
                        span,
                    } => {
                        let mut data = Vec::new();
                        for arg in args {
                            if let Some(name) = arg.name {
                                data.push((name, arg.value));
                            } else {
                                return Err(
                                    self.parser_error("Send requires named arguments", arg.span)
                                );
                            }
                        }
                        Ok(Expr::SendMsg {
                            target: receiver,
                            message: method,
                            data,
                            span: start.merge(span),
                        })
                    }
                    Expr::Call { callee, args, span } => {
                        if let Expr::Field {
                            object,
                            field,
                            span: _,
                        } = *callee
                        {
                            let mut data = Vec::new();
                            for arg in args {
                                if let Some(name) = arg.name {
                                    data.push((name, arg.value));
                                } else {
                                    return Err(self
                                        .parser_error("Send requires named arguments", arg.span));
                                }
                            }
                            Ok(Expr::SendMsg {
                                target: object,
                                message: field,
                                data,
                                span: start.merge(span),
                            })
                        } else {
                            Err(self.parser_error(
                                "Expected method call after send (e.g., actor.message())",
                                span,
                            ))
                        }
                    }
                    _ => Err(self.parser_error("Expected message call after send", expr.span())),
                }
            }
            _ => self.parse_postfix(),
        }
    }

    fn parse_scoped_ownership_expr(&mut self, keyword: &str) -> KainResult<Expr> {
        let start = self.current_span();
        self.advance();
        let target = self.parse_expr()?;
        self.expect(TokenKind::Colon)?;
        let body = Expr::Block(self.parse_block()?, start);
        let span = start.merge(body.span());
        match keyword {
            OBSERVE_KEYWORD => Ok(Expr::Observe {
                target: Box::new(target),
                body: Box::new(body),
                span,
            }),
            COLLAPSE_KEYWORD => Ok(Expr::Collapse {
                target: Box::new(target),
                body: Box::new(body),
                span,
            }),
            SHARE_KEYWORD => Ok(Expr::Share {
                target: Box::new(target),
                body: Box::new(body),
                span,
            }),
            _ => Err(self.parser_error("Unknown scoped ownership keyword", start)),
        }
    }

    fn parse_decay_expr(&mut self) -> KainResult<Expr> {
        let start = self.current_span();
        self.advance();
        let target = self.parse_unary()?;
        Ok(Expr::Decay {
            span: start.merge(target.span()),
            target: Box::new(target),
        })
    }

    fn parse_teleport_expr(&mut self) -> KainResult<Expr> {
        let start = self.current_span();
        self.expect_contextual_ident("teleport")?;
        let value = self.parse_unary()?;
        self.expect_contextual_ident("from")?;
        let source_world = self.parse_string_like_argument("teleport source world")?;
        self.expect_contextual_ident("to")?;
        let target_world = self.parse_string_like_argument("teleport target world")?;
        let channel = if self.peek_contextual_ident("via") {
            self.advance();
            Some(self.parse_string_like_argument("teleport channel")?)
        } else {
            None
        };
        let span = start.merge(value.span()).merge(self.current_span());
        Ok(Expr::Teleport {
            value: Box::new(value),
            source_world,
            target_world,
            channel,
            span,
        })
    }

    fn parse_postfix(&mut self) -> KainResult<Expr> {
        let mut expr = self.parse_primary()?;
        loop {
            match self.peek_kind() {
                TokenKind::LParen if self.previous_token_allows_postfix_call() => {
                    self.advance();
                    let args = self.parse_call_args()?;
                    self.expect(TokenKind::RParen)?;
                    let s = expr.span().merge(self.current_span());

                    // Check if this looks like struct initialization with named arguments
                    // Pattern: TypeName(field = val, ...) where TypeName starts with uppercase
                    if let Expr::Ident(name, _ident_span) = &expr {
                        // Check if identifier starts with uppercase (likely a type name)
                        let starts_with_uppercase = name
                            .chars()
                            .next()
                            .map(|c| c.is_uppercase())
                            .unwrap_or(false);

                        // Check if all arguments are named (using = syntax in KAIN)
                        let all_named =
                            !args.is_empty() && args.iter().all(|arg| arg.name.is_some());

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

                    if let Expr::Field {
                        object,
                        field,
                        span: _,
                    } = expr
                    {
                        expr = Expr::MethodCall {
                            receiver: object,
                            method: field,
                            args,
                            span: s,
                        };
                    } else {
                        expr = self.normalize_special_call(expr, args, s);
                    }
                }
                TokenKind::Dot => {
                    self.advance();
                    let field = self.parse_ident()?;
                    let s = expr.span().merge(self.current_span());
                    expr = Expr::Field {
                        object: Box::new(expr),
                        field,
                        span: s,
                    };
                }
                TokenKind::QuestionDot => {
                    self.advance();
                    let field = self.parse_ident()?;
                    let s = expr.span().merge(self.current_span());
                    expr = self.make_safe_nav_field_expr(expr, field, s)?;
                }
                TokenKind::As => {
                    self.advance();
                    let target = self.parse_type()?;
                    let s = expr.span().merge(self.current_span());
                    expr = Expr::Cast {
                        value: Box::new(expr),
                        target,
                        span: s,
                    };
                }
                TokenKind::LBracket => {
                    self.advance();
                    let idx = if self.check(TokenKind::DotDot) {
                        self.advance();
                        let inclusive = if self.check(TokenKind::Eq) {
                            self.advance();
                            true
                        } else {
                            false
                        };
                        let end = if self.check(TokenKind::RBracket) {
                            None
                        } else {
                            Some(Box::new(self.parse_expr()?))
                        };
                        Expr::Range {
                            start: None,
                            end,
                            inclusive,
                            span: self.current_span(),
                        }
                    } else {
                        let first = self.parse_expr()?;
                        if self.check(TokenKind::DotDot) {
                            let start_span = first.span();
                            self.advance();
                            let inclusive = if self.check(TokenKind::Eq) {
                                self.advance();
                                true
                            } else {
                                false
                            };
                            let end = if self.check(TokenKind::RBracket) {
                                None
                            } else {
                                Some(Box::new(self.parse_expr()?))
                            };
                            Expr::Range {
                                start: Some(Box::new(first)),
                                end,
                                inclusive,
                                span: start_span.merge(self.current_span()),
                            }
                        } else {
                            first
                        }
                    };
                    self.expect(TokenKind::RBracket)?;
                    let s = expr.span().merge(self.current_span());
                    expr = Expr::Index {
                        object: Box::new(expr),
                        index: Box::new(idx),
                        span: s,
                    };
                }
                TokenKind::PlusPlus => {
                    let start = expr.span();
                    self.advance();
                    let span = start.merge(self.current_span());
                    expr = self.make_incdec_expr(expr, true, false, span)?;
                }
                TokenKind::MinusMinus => {
                    let start = expr.span();
                    self.advance();
                    let span = start.merge(self.current_span());
                    expr = self.make_incdec_expr(expr, false, false, span)?;
                }
                TokenKind::Question => {
                    if self.question_starts_ternary() {
                        break;
                    }
                    self.advance();
                    let s = expr.span().merge(self.current_span());
                    expr = Expr::Try(Box::new(expr), s);
                }
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
                                if self.check(TokenKind::RParen) {
                                    break;
                                }
                                args.push(self.parse_expr()?);
                            }
                            args
                        } else {
                            Vec::new()
                        };
                        self.expect(TokenKind::RParen)?;
                        let s = expr.span().merge(self.current_span());
                        expr = Expr::MacroCall {
                            name: name.clone(),
                            args,
                            span: s,
                        };
                    } else {
                        // Maybe unary not? But we are in postfix. Unary not is handled in parse_unary.
                        // Postfix ! usually means macro or maybe future features (like factorial?).
                        // For now, only support macros on identifiers.
                        return Err(self.parser_error(
                            "Macro invocation only allowed on identifiers",
                            self.current_span(),
                        ));
                    }
                }
                _ => break,
            }
        }
        Ok(expr)
    }

    fn previous_token_allows_postfix_call(&self) -> bool {
        if self.pos == 0 {
            return true;
        }
        !matches!(
            self.tokens.get(self.pos - 1).map(|token| &token.kind),
            Some(TokenKind::Newline(_)) | Some(TokenKind::Dedent)
        )
    }

    fn parse_primary(&mut self) -> KainResult<Expr> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Int(n) => {
                self.advance();
                Ok(Expr::Int(n, span))
            }
            TokenKind::Float(n) => {
                self.advance();
                Ok(Expr::Float(n, span))
            }
            TokenKind::String(ref s) => {
                let s = s.clone();
                self.advance();
                Ok(Expr::String(s, span))
            }
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
                            if c2 == '{' {
                                depth += 1;
                            } else if c2 == '}' {
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
            TokenKind::True => {
                self.advance();
                Ok(Expr::Bool(true, span))
            }
            TokenKind::False => {
                self.advance();
                Ok(Expr::Bool(false, span))
            }
            TokenKind::None => {
                self.advance();
                Ok(Expr::None(span))
            }
            TokenKind::Ident(ref s) => {
                let name = s.clone();
                self.advance();

                if let Some(runtime) = parse_orchestrate_stage_runtime(name.as_str()) {
                    let saved_pos = self.pos;
                    if matches!(
                        self.peek_kind(),
                        TokenKind::Ident(_) | TokenKind::SelfLower | TokenKind::SelfUpper
                    ) {
                        let function = self.parse_path_name()?;
                        if self.check(TokenKind::LParen) {
                            self.advance();
                            let args = self.parse_call_args()?;
                            self.expect(TokenKind::RParen)?;
                            return Ok(Expr::StageCall {
                                runtime,
                                function,
                                args,
                                span: span.merge(self.current_span()),
                            });
                        }
                    }
                    self.pos = saved_pos;
                }

                let path_segments = self.parse_path_segments_after(name.clone())?;
                let path_name = Self::join_path_segments(&path_segments);
                let is_variant_path = Self::path_looks_like_enum_variant(&path_segments);

                // Check if this is a struct literal: Name { field: value, ... }
                // Skip formatting (newlines and indents) to handle multi-line struct literals.
                let saved_pos_for_check = self.pos;
                self.skip_formatting();
                let has_brace = self.check(TokenKind::LBrace);

                if is_variant_path && (self.check(TokenKind::LParen) || has_brace) {
                    let variant = path_segments.last().cloned().unwrap_or_default();
                    let enum_name =
                        Self::join_path_segments(&path_segments[..path_segments.len() - 1]);
                    let fields = self.parse_enum_variant_fields()?;
                    return Ok(Expr::EnumVariant {
                        enum_name,
                        variant,
                        fields,
                        span: span.merge(self.current_span()),
                    });
                }

                if has_brace {
                    if !self.capabilities.supports_parser_struct_literals() {
                        // Restore position before emitting error
                        self.pos = saved_pos_for_check;

                        return Err(self.parser_error(
                        format!(
                            "Struct literal syntax is not supported in KAIN. Found '{} {{ ... }}'.\n\
                             Use field-by-field assignment instead:\n\
                             \n\
                             Example:\n\
                               let obj = {}()\n\
                               obj.field1 = value1\n\
                               obj.field2 = value2",
                            path_name, path_name
                            ),
                            span,
                        ));
                    }
                    return self.parse_struct_literal_expr(path_name, span);
                }

                if is_variant_path {
                    self.pos = saved_pos_for_check;
                    let variant = path_segments.last().cloned().unwrap_or_default();
                    let enum_name =
                        Self::join_path_segments(&path_segments[..path_segments.len() - 1]);
                    return Ok(Expr::EnumVariant {
                        enum_name,
                        variant,
                        fields: EnumVariantFields::Unit,
                        span: span.merge(self.current_span()),
                    });
                }

                // Restore position if not a struct literal
                self.pos = saved_pos_for_check;

                // Just an identifier
                Ok(Expr::Ident(path_name, span))
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
                self.skip_newlines();
                let indented = if self.check(TokenKind::Indent) {
                    self.advance();
                    true
                } else {
                    false
                };
                if self.check(TokenKind::RParen) {
                    self.advance();
                    Ok(Expr::Tuple(vec![], span.merge(self.current_span())))
                } else {
                    let first = self.parse_expr()?;
                    if self.check(TokenKind::Comma) {
                        self.advance();
                        self.skip_newlines();
                        let mut items = vec![first];
                        while !self.check(TokenKind::RParen)
                            && !(indented && self.check(TokenKind::Dedent))
                        {
                            items.push(self.parse_expr()?);
                            if !self.check(TokenKind::RParen)
                                && !(indented && self.check(TokenKind::Dedent))
                            {
                                self.expect(TokenKind::Comma)?;
                                self.skip_newlines();
                            }
                        }
                        if indented && self.check(TokenKind::Dedent) {
                            self.advance();
                        }
                        self.expect(TokenKind::RParen)?;
                        Ok(Expr::Tuple(items, span.merge(self.current_span())))
                    } else {
                        if indented && self.check(TokenKind::Dedent) {
                            self.advance();
                        }
                        self.expect(TokenKind::RParen)?;
                        Ok(Expr::Paren(
                            Box::new(first),
                            span.merge(self.current_span()),
                        ))
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
                    if !self.check(TokenKind::Pipe) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::Pipe)?;
                let body = self.parse_expr()?;
                Ok(Expr::Lambda {
                    params,
                    return_type: None,
                    body: Box::new(body),
                    span: span.merge(self.current_span()),
                })
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
                Ok(Expr::Spawn {
                    actor,
                    init,
                    span: span.merge(self.current_span()),
                })
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
                    span: span.merge(self.current_span()),
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
                let value = if !self.check_line_end()
                    && !self.check(TokenKind::Dedent)
                    && !self.check(TokenKind::Comma)
                    && !self.check(TokenKind::RParen)
                {
                    Some(Box::new(self.parse_expr()?))
                } else {
                    None
                };
                Ok(Expr::Break(value, span))
            }
            // Contextual keywords - allowed as identifiers in expression context
            TokenKind::Component => {
                self.advance();
                Ok(Expr::Ident("component".to_string(), span))
            }
            TokenKind::Shader => {
                self.advance();
                Ok(Expr::Ident("shader".to_string(), span))
            }
            TokenKind::Actor => {
                self.advance();
                Ok(Expr::Ident("actor".to_string(), span))
            }
            TokenKind::State => {
                self.advance();
                Ok(Expr::Ident("state".to_string(), span))
            }
            _ => Err(self.parser_error(
                format!(
                    "Unexpected token: {}",
                    crate::error::token_kind_to_user_string(&self.peek_kind())
                ),
                span,
            )),
        }
    }

    fn parse_struct_literal_expr(&mut self, name: String, start_span: Span) -> KainResult<Expr> {
        self.expect(TokenKind::LBrace)?;
        self.skip_newlines();

        let indented = if self.check(TokenKind::Indent) {
            self.advance();
            true
        } else {
            false
        };

        let mut fields = Vec::new();
        let mut rest = None;
        while !self.check(TokenKind::RBrace) && !self.at_end() {
            if indented && self.check(TokenKind::Dedent) {
                break;
            }

            self.skip_newlines();
            if self.check(TokenKind::RBrace) || (indented && self.check(TokenKind::Dedent)) {
                break;
            }

            if self.check(TokenKind::DotDot) {
                self.advance();
                if rest.is_some() {
                    return Err(self.parser_error(
                        "Struct update syntax only supports one '..base' expression",
                        self.current_span(),
                    ));
                }
                if self.check(TokenKind::Comma)
                    || self.check(TokenKind::RBrace)
                    || (indented && self.check(TokenKind::Dedent))
                {
                    return Err(self.parser_error(
                        "Struct update syntax requires an expression after '..'",
                        self.current_span(),
                    ));
                }
                rest = Some(Box::new(self.parse_expr()?));
                self.skip_newlines();
                if self.check(TokenKind::Comma) {
                    self.advance();
                    self.skip_newlines();
                }
                break;
            }

            let field_name = self.parse_ident()?;
            self.expect(TokenKind::Colon)?;
            let field_value = self.parse_expr()?;
            fields.push((field_name, field_value));

            self.skip_newlines();
            if self.check(TokenKind::Comma) {
                self.advance();
                self.skip_newlines();
            }
        }

        if indented {
            self.expect(TokenKind::Dedent)?;
        }
        self.expect(TokenKind::RBrace)?;

        Ok(Expr::Struct {
            name,
            fields,
            rest,
            span: start_span.merge(self.current_span()),
        })
    }

    fn parse_optional_ref_lifetime(&mut self) -> Option<String> {
        match self.peek_kind() {
            TokenKind::Ident(name)
                if self
                    .peek_kind_at(1)
                    .as_ref()
                    .is_some_and(Self::token_can_start_type) =>
            {
                self.advance();
                Some(name)
            }
            _ => None,
        }
    }

    fn token_can_start_type(kind: &TokenKind) -> bool {
        matches!(
            kind,
            TokenKind::Ident(_)
                | TokenKind::SelfLower
                | TokenKind::SelfUpper
                | TokenKind::Component
                | TokenKind::Shader
                | TokenKind::Actor
                | TokenKind::State
                | TokenKind::LBracket
                | TokenKind::LParen
                | TokenKind::Amp
                | TokenKind::Fn
                | TokenKind::Impl
        )
    }

    fn normalize_special_call(&self, callee: Expr, args: Vec<CallArg>, span: Span) -> Expr {
        if let Expr::Ident(name, _) = &callee {
            if name == "asm" {
                if let Some(inline_asm) = self.parse_inline_asm_call(&args, span) {
                    return inline_asm;
                }
            }
            if args.iter().all(|arg| arg.name.is_none()) {
                match (name.as_str(), args.len()) {
                    ("addr_of", 1 | 2) => {
                        let mut values = args.into_iter();
                        let value = values.next().expect("addr_of must have first arg").value;
                        let pointee_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::AddrOf {
                            value: Box::new(value),
                            pointee_ty,
                            span,
                        };
                    }
                    ("ptr_offset", 2 | 3) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("ptr_offset must have first arg").value;
                        let offset = values
                            .next()
                            .expect("ptr_offset must have second arg")
                            .value;
                        let element_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::PtrOffset {
                            pointer: Box::new(pointer),
                            offset: Box::new(offset),
                            element_ty,
                            span,
                        };
                    }
                    ("bitcast", 2) => {
                        let value = args[0].value.clone();
                        if let Some(target) = self.parse_type_hint_arg(&args[1].value, span) {
                            return Expr::Bitcast {
                                value: Box::new(value),
                                target,
                                span,
                            };
                        }
                    }
                    ("ptr_to_int", 1) => {
                        let value = args[0].value.clone();
                        return Expr::Cast {
                            value: Box::new(value),
                            target: Type::Named {
                                name: "Int".to_string(),
                                generics: Vec::new(),
                                span,
                            },
                            span,
                        };
                    }
                    ("int_to_ptr", 2) => {
                        let value = args[0].value.clone();
                        if let Some(target) = self.parse_type_hint_arg(&args[1].value, span) {
                            let target = match target {
                                Type::Ptr { .. } => target,
                                other => Type::Ptr {
                                    mutable: true,
                                    inner: Box::new(other),
                                    provenance: PointerProvenance::Raw,
                                    span,
                                },
                            };
                            return Expr::Cast {
                                value: Box::new(value),
                                target,
                                span,
                            };
                        }
                    }
                    ("mem_load", 1 | 2) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("mem_load must have arg").value;
                        let load_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::MemLoad {
                            pointer: Box::new(pointer),
                            load_ty,
                            span,
                        };
                    }
                    ("mem_store", 2 | 3) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("mem_store must have first arg").value;
                        let value = values.next().expect("mem_store must have second arg").value;
                        let store_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::MemStore {
                            pointer: Box::new(pointer),
                            value: Box::new(value),
                            store_ty,
                            span,
                        };
                    }
                    ("volatile_load", 1 | 2) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("volatile_load must have arg").value;
                        let load_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::VolatileLoad {
                            pointer: Box::new(pointer),
                            load_ty,
                            span,
                        };
                    }
                    ("volatile_store", 2 | 3) => {
                        let mut values = args.into_iter();
                        let pointer = values
                            .next()
                            .expect("volatile_store must have first arg")
                            .value;
                        let value = values
                            .next()
                            .expect("volatile_store must have second arg")
                            .value;
                        let store_ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::VolatileStore {
                            pointer: Box::new(pointer),
                            value: Box::new(value),
                            store_ty,
                            span,
                        };
                    }
                    ("atomic_load", 1..=3) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("atomic_load must have arg").value;
                        let rest: Vec<Expr> = values.map(|arg| arg.value).collect();
                        let (load_ty, ordering) = self.parse_atomic_type_and_ordering_args(
                            &rest,
                            span,
                            AtomicOrdering::SeqCst,
                        );
                        return Expr::AtomicLoad {
                            pointer: Box::new(pointer),
                            load_ty,
                            ordering,
                            span,
                        };
                    }
                    ("atomic_store", 2..=4) => {
                        let mut values = args.into_iter();
                        let pointer = values
                            .next()
                            .expect("atomic_store must have first arg")
                            .value;
                        let value = values
                            .next()
                            .expect("atomic_store must have second arg")
                            .value;
                        let rest: Vec<Expr> = values.map(|arg| arg.value).collect();
                        let (store_ty, ordering) = self.parse_atomic_type_and_ordering_args(
                            &rest,
                            span,
                            AtomicOrdering::SeqCst,
                        );
                        return Expr::AtomicStore {
                            pointer: Box::new(pointer),
                            value: Box::new(value),
                            store_ty,
                            ordering,
                            span,
                        };
                    }
                    (
                        "atomic_add" | "atomic_sub" | "atomic_and" | "atomic_or" | "atomic_xor"
                        | "atomic_exchange",
                        2..=4,
                    ) => {
                        let mut values = args.into_iter();
                        let pointer = values.next().expect("atomic op must have first arg").value;
                        let value = values.next().expect("atomic op must have second arg").value;
                        let rest: Vec<Expr> = values.map(|arg| arg.value).collect();
                        let (op_ty, ordering) = self.parse_atomic_type_and_ordering_args(
                            &rest,
                            span,
                            AtomicOrdering::SeqCst,
                        );
                        return match name.as_str() {
                            "atomic_add" => Expr::AtomicAdd {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                            "atomic_sub" => Expr::AtomicSub {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                            "atomic_and" => Expr::AtomicAnd {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                            "atomic_or" => Expr::AtomicOr {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                            "atomic_xor" => Expr::AtomicXor {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                            _ => Expr::AtomicExchange {
                                pointer: Box::new(pointer),
                                value: Box::new(value),
                                op_ty,
                                ordering,
                                span,
                            },
                        };
                    }
                    ("atomic_compare_exchange", 3..=6) => {
                        let mut values = args.into_iter();
                        let pointer = values
                            .next()
                            .expect("atomic_compare_exchange must have first arg")
                            .value;
                        let expected = values
                            .next()
                            .expect("atomic_compare_exchange must have second arg")
                            .value;
                        let desired = values
                            .next()
                            .expect("atomic_compare_exchange must have third arg")
                            .value;
                        let rest: Vec<Expr> = values.map(|arg| arg.value).collect();
                        let (op_ty, success_ordering, failure_ordering) =
                            self.parse_atomic_compare_exchange_tail(&rest, span);
                        return Expr::AtomicCompareExchange {
                            pointer: Box::new(pointer),
                            expected: Box::new(expected),
                            desired: Box::new(desired),
                            op_ty,
                            success_ordering,
                            failure_ordering,
                            span,
                        };
                    }
                    ("atomic_fence", 0 | 1) => {
                        let ordering = args
                            .first()
                            .and_then(|arg| self.parse_atomic_ordering_arg(&arg.value))
                            .unwrap_or(AtomicOrdering::SeqCst);
                        return Expr::AtomicFence { ordering, span };
                    }
                    ("lfence", 0) => {
                        return Expr::CpuFence {
                            kind: CpuFenceKind::Load,
                            span,
                        };
                    }
                    ("sfence", 0) => {
                        return Expr::CpuFence {
                            kind: CpuFenceKind::Store,
                            span,
                        };
                    }
                    ("mfence", 0) => {
                        return Expr::CpuFence {
                            kind: CpuFenceKind::Full,
                            span,
                        };
                    }
                    ("clflush", 1) => {
                        return Expr::CpuCacheFlush {
                            pointer: Box::new(args[0].value.clone()),
                            span,
                        };
                    }
                    ("sizeof_type", 1) => {
                        let mut values = args.into_iter();
                        let target = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span))
                            .expect("sizeof_type must have parseable type arg");
                        return Expr::SizeOfType { target, span };
                    }
                    ("alignof_type", 1) => {
                        let mut values = args.into_iter();
                        let target = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span))
                            .expect("alignof_type must have parseable type arg");
                        return Expr::AlignOfType { target, span };
                    }
                    ("alloca", 1) => {
                        let mut values = args.into_iter();
                        let ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span))
                            .expect("alloca must have parseable type arg");
                        return Expr::Alloca { ty, span };
                    }
                    ("uninit", 1) => {
                        let mut values = args.into_iter();
                        let ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span))
                            .expect("uninit must have parseable type arg");
                        return Expr::Uninit { ty, span };
                    }
                    ("alloc" | "alloc_zeroed", 1 | 2) => {
                        let zeroed = name == "alloc_zeroed";
                        let mut values = args.into_iter();
                        let size = values.next().expect("alloc must have size arg").value;
                        let ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        return Expr::Alloc {
                            size: Box::new(size),
                            ty,
                            zeroed,
                            span,
                        };
                    }
                    ("realloc_mem", 2 | 3 | 4) => {
                        let mut values = args.into_iter();
                        let pointer = values
                            .next()
                            .expect("realloc_mem must have pointer arg")
                            .value;
                        let size = values.next().expect("realloc_mem must have size arg").value;
                        let ty = values
                            .next()
                            .and_then(|arg| self.parse_type_hint_arg(&arg.value, span));
                        let zeroed_new = values
                            .next()
                            .map(|arg| matches!(arg.value, Expr::Bool(true, _)))
                            .unwrap_or(false);
                        return Expr::Realloc {
                            pointer: Box::new(pointer),
                            size: Box::new(size),
                            ty,
                            zeroed_new,
                            span,
                        };
                    }
                    _ => {}
                }
            }
        }

        if let Expr::Ident(name, _) = &callee {
            if name == "aggregate_init" && !args.is_empty() {
                let mut values = args.into_iter();
                let ty = values
                    .next()
                    .and_then(|arg| self.parse_type_hint_arg(&arg.value, span))
                    .expect("aggregate_init must have parseable type arg");
                let mut zero_fill_rest = true;
                let mut fields = Vec::new();
                for arg in values {
                    match (&arg.name, &arg.value) {
                        (None, Expr::Bool(value, _)) => zero_fill_rest = *value,
                        (Some(name), value) => fields.push((name.clone(), value.clone())),
                        _ => {}
                    }
                }
                return Expr::AggregateInit {
                    ty,
                    fields,
                    zero_fill_rest,
                    span,
                };
            }
        }

        Expr::Call {
            callee: Box::new(callee),
            args,
            span,
        }
    }

    fn parse_type_hint_arg(&self, expr: &Expr, span: Span) -> Option<Type> {
        match expr {
            Expr::String(value, _) => self.parse_type_hint_string(value, span),
            Expr::Ident(value, _) => self.parse_type_hint_string(value, span),
            _ => None,
        }
    }

    fn parse_atomic_ordering_arg(&self, expr: &Expr) -> Option<AtomicOrdering> {
        let value = match expr {
            Expr::String(value, _) | Expr::Ident(value, _) => value.as_str(),
            _ => return None,
        };
        match value.trim().to_ascii_lowercase().replace('-', "_").as_str() {
            "relaxed" => Some(AtomicOrdering::Relaxed),
            "acquire" => Some(AtomicOrdering::Acquire),
            "release" => Some(AtomicOrdering::Release),
            "acq_rel" | "acqrel" | "acquire_release" => Some(AtomicOrdering::AcqRel),
            "seq_cst" | "seqcst" | "sequentially_consistent" => Some(AtomicOrdering::SeqCst),
            _ => None,
        }
    }

    fn parse_inline_asm_call(&self, args: &[CallArg], span: Span) -> Option<Expr> {
        let template = self.parse_inline_asm_template_arg(&args.first()?.value)?;
        let mut operands = Vec::new();
        let mut options = InlineAsmOptions::default();
        for arg in &args[1..] {
            if let Some(name) = arg.name.as_deref() {
                let value = self.parse_inline_asm_bool_option_arg(&arg.value)?;
                match name {
                    "volatile" => options.volatile = value,
                    "memory" => options.memory = value,
                    "intel" => options.intel = value,
                    _ => return None,
                }
            } else {
                operands.push(arg.value.clone());
            }
        }
        Some(Expr::InlineAsm {
            template,
            operands,
            options,
            span,
        })
    }

    fn parse_inline_asm_template_arg(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::String(value, _) | Expr::Ident(value, _) => Some(value.clone()),
            Expr::Paren(inner, _) => self.parse_inline_asm_template_arg(inner),
            _ => None,
        }
    }

    fn parse_inline_asm_bool_option_arg(&self, expr: &Expr) -> Option<bool> {
        match expr {
            Expr::Bool(value, _) => Some(*value),
            Expr::Paren(inner, _) => self.parse_inline_asm_bool_option_arg(inner),
            _ => None,
        }
    }

    fn parse_atomic_type_and_ordering_args(
        &self,
        exprs: &[Expr],
        span: Span,
        default_ordering: AtomicOrdering,
    ) -> (Option<Type>, AtomicOrdering) {
        let mut ty = None;
        let mut ordering = default_ordering;
        for expr in exprs {
            if let Some(parsed_ordering) = self.parse_atomic_ordering_arg(expr) {
                ordering = parsed_ordering;
            } else if ty.is_none() {
                ty = self.parse_type_hint_arg(expr, span);
            }
        }
        (ty, ordering)
    }

    fn parse_atomic_compare_exchange_tail(
        &self,
        exprs: &[Expr],
        span: Span,
    ) -> (Option<Type>, AtomicOrdering, AtomicOrdering) {
        let mut ty = None;
        let mut orderings = Vec::new();
        for expr in exprs {
            if let Some(ordering) = self.parse_atomic_ordering_arg(expr) {
                orderings.push(ordering);
            } else if ty.is_none() {
                ty = self.parse_type_hint_arg(expr, span);
            }
        }
        let success_ordering = orderings.first().copied().unwrap_or(AtomicOrdering::SeqCst);
        let failure_ordering = orderings
            .get(1)
            .copied()
            .unwrap_or_else(|| Self::default_compare_exchange_failure_ordering(success_ordering));
        (ty, success_ordering, failure_ordering)
    }

    fn default_compare_exchange_failure_ordering(ordering: AtomicOrdering) -> AtomicOrdering {
        match ordering {
            AtomicOrdering::Release => AtomicOrdering::Relaxed,
            AtomicOrdering::AcqRel => AtomicOrdering::Acquire,
            other => other,
        }
    }

    fn parse_type_hint_string(&self, value: &str, span: Span) -> Option<Type> {
        let tokens = Lexer::new(value).tokenize().ok()?;
        let span_mapper = SpanMapper::new(value);
        let mut parser = Parser::new(&tokens, &span_mapper, "<memory-type-hint>");
        let ty = parser.parse_type().ok()?;
        if !parser.check(TokenKind::Eof) {
            return None;
        }
        Some(Self::re_span_type(ty, span))
    }

    fn re_span_type(ty: Type, span: Span) -> Type {
        match ty {
            Type::Named { name, generics, .. } => Type::Named {
                name,
                generics: generics
                    .into_iter()
                    .map(|g| Self::re_span_type(g, span))
                    .collect(),
                span,
            },
            Type::Array(inner, size, _) => {
                Type::Array(Box::new(Self::re_span_type(*inner, span)), size, span)
            }
            Type::Slice(inner, _) => Type::Slice(Box::new(Self::re_span_type(*inner, span)), span),
            Type::Tuple(types, _) => Type::Tuple(
                types
                    .into_iter()
                    .map(|t| Self::re_span_type(t, span))
                    .collect(),
                span,
            ),
            Type::Ref {
                mutable,
                inner,
                lifetime,
                ..
            } => Type::Ref {
                mutable,
                inner: Box::new(Self::re_span_type(*inner, span)),
                lifetime,
                span,
            },
            Type::Ptr {
                mutable,
                inner,
                provenance,
                ..
            } => Type::Ptr {
                mutable,
                inner: Box::new(Self::re_span_type(*inner, span)),
                provenance,
                span,
            },
            Type::Function {
                params,
                return_type,
                effects,
                ..
            } => Type::Function {
                params: params
                    .into_iter()
                    .map(|p| Self::re_span_type(p, span))
                    .collect(),
                return_type: Box::new(Self::re_span_type(*return_type, span)),
                effects,
                span,
            },
            Type::Option(inner, _) => {
                Type::Option(Box::new(Self::re_span_type(*inner, span)), span)
            }
            Type::Result(ok, err, _) => Type::Result(
                Box::new(Self::re_span_type(*ok, span)),
                Box::new(Self::re_span_type(*err, span)),
                span,
            ),
            Type::Infer(_) => Type::Infer(span),
            Type::Never(_) => Type::Never(span),
            Type::Unit(_) => Type::Unit(span),
            Type::Impl {
                trait_name,
                generics,
                ..
            } => Type::Impl {
                trait_name,
                generics: generics
                    .into_iter()
                    .map(|g| Self::re_span_type(g, span))
                    .collect(),
                span,
            },
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
            if self.check(TokenKind::Dedent) {
                break;
            }
            let arm_start = self.current_span();
            let pattern = self.parse_pattern()?;
            let guard = if self.check(TokenKind::If) {
                self.advance();
                Some(self.parse_expr()?)
            } else {
                None
            };
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
                        if self.check(TokenKind::Dedent) {
                            break;
                        }
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
                            let block = Block {
                                stmts,
                                span: arm_start.merge(self.current_span()),
                            };
                            Expr::Block(block, arm_start.merge(self.current_span()))
                        }
                    } else {
                        let block = Block {
                            stmts,
                            span: arm_start.merge(self.current_span()),
                        };
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

            arms.push(MatchArm {
                pattern,
                guard,
                body,
                span: self.current_span(),
            });
        }
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        Ok(Expr::Match {
            scrutinee,
            arms,
            span: start.merge(self.current_span()),
        })
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
            Block {
                stmts: vec![stmt],
                span: start.merge(self.current_span()),
            }
        };

        let else_branch = self.parse_if_tail(start)?;
        Ok(Expr::If {
            condition,
            then_branch,
            else_branch,
            span: start.merge(self.current_span()),
        })
    }

    fn parse_if_tail(&mut self, start: Span) -> KainResult<Option<Box<ElseBranch>>> {
        self.skip_newlines();
        if self.check(TokenKind::Elif) {
            self.advance();
            let condition = Box::new(self.parse_expr()?);
            self.expect(TokenKind::Colon)?;

            let is_block = matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent);
            let then_branch = if is_block {
                self.parse_block()?
            } else {
                let stmt = self.parse_stmt()?;
                Block {
                    stmts: vec![stmt],
                    span: start.merge(self.current_span()),
                }
            };

            let nested_else = self.parse_if_tail(start)?;
            return Ok(Some(Box::new(ElseBranch::ElseIf(
                condition,
                then_branch,
                nested_else,
            ))));
        }

        if self.check(TokenKind::Else) {
            self.advance();

            // Check for 'else if' (elif pattern) - no colon between else and if
            if self.check(TokenKind::If) {
                // Parse the 'if' expression
                let elif_expr = self.parse_if()?;

                // Extract the condition, then_branch, and else_branch from the If expression
                if let Expr::If {
                    condition,
                    then_branch,
                    else_branch: nested_else,
                    ..
                } = elif_expr
                {
                    return Ok(Some(Box::new(ElseBranch::ElseIf(
                        condition,
                        then_branch,
                        nested_else,
                    ))));
                }

                // Shouldn't happen, but fallback
                return Err(
                    self.parser_error("Expected if expression after else", self.current_span())
                );
            }

            self.expect(TokenKind::Colon)?;
            let is_block = matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Indent);
            if is_block {
                return Ok(Some(Box::new(ElseBranch::Else(self.parse_block()?))));
            }

            let stmt = self.parse_stmt()?;
            return Ok(Some(Box::new(ElseBranch::Else(Block {
                stmts: vec![stmt],
                span: start.merge(self.current_span()),
            }))));
        }

        Ok(None)
    }

    fn parse_pattern(&mut self) -> KainResult<Pattern> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(ref s) if s == "_" => {
                self.advance();
                Ok(Pattern::Wildcard(span))
            }
            TokenKind::Ident(ref s) => {
                let name = s.clone();
                self.advance();

                // Validate identifier if it's used as a binding (not an enum name)
                // We'll validate after determining if it's a binding or enum reference

                let path_segments = self.parse_path_segments_after(name.clone())?;
                if path_segments.len() > 1 {
                    // This is an enum name, not a binding, so no validation needed here.
                    let variant = path_segments.last().cloned().unwrap_or_default();
                    let enum_name =
                        Self::join_path_segments(&path_segments[..path_segments.len() - 1]);
                    let fields = self.parse_variant_pattern_fields()?;

                    Ok(Pattern::Variant {
                        enum_name: Some(enum_name),
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
                    Ok(Pattern::Binding {
                        name,
                        mutable: false,
                        span,
                    })
                }
            }
            TokenKind::Mut => {
                self.advance();
                let name = self.parse_ident()?;
                Ok(Pattern::Binding {
                    name,
                    mutable: true,
                    span: span.merge(self.current_span()),
                })
            }
            TokenKind::Int(n) => {
                self.advance();
                Ok(Pattern::Literal(Expr::Int(n, span)))
            }
            TokenKind::String(ref s) => {
                let string_val = s.clone();
                self.advance();
                Ok(Pattern::Literal(Expr::String(string_val, span)))
            }
            TokenKind::True => {
                self.advance();
                Ok(Pattern::Literal(Expr::Bool(true, span)))
            }
            TokenKind::False => {
                self.advance();
                Ok(Pattern::Literal(Expr::Bool(false, span)))
            }
            TokenKind::LParen => {
                self.advance();
                let mut patterns = Vec::new();
                while !self.check(TokenKind::RParen) {
                    patterns.push(self.parse_pattern()?);
                    if !self.check(TokenKind::RParen) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::RParen)?;
                Ok(Pattern::Tuple(patterns, span.merge(self.current_span())))
            }
            TokenKind::LBracket => {
                self.advance();
                let mut patterns = Vec::new();
                while !self.check(TokenKind::RBracket) {
                    patterns.push(self.parse_pattern()?);
                    if !self.check(TokenKind::RBracket) {
                        self.expect(TokenKind::Comma)?;
                    }
                }
                self.expect(TokenKind::RBracket)?;
                Ok(Pattern::Slice {
                    patterns,
                    rest: None,
                    span: span.merge(self.current_span()),
                })
            }
            _ => Err(self.parser_error(
                format!(
                    "Expected pattern (identifier, integer, string, tuple, or array), found {}",
                    self.token_to_user_string(&self.peek_kind())
                ),
                span,
            )),
        }
    }

    #[allow(dead_code)]
    fn parse_jsx(&mut self) -> KainResult<JSXNode> {
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;
        self.skip_newlines();
        let result = self.parse_jsx_element()?;
        self.skip_newlines();
        if self.check(TokenKind::Dedent) {
            self.advance();
        }
        Ok(result)
    }

    fn parse_jsx_element(&mut self) -> KainResult<JSXNode> {
        let start = self.current_span();
        self.expect(TokenKind::Lt)?;
        let tag = self.parse_jsx_tag_name()?;
        let mut attrs = Vec::new();
        while !self.check(TokenKind::Gt) && !self.check(TokenKind::Slash) {
            let name = self.parse_jsx_attribute_name()?;
            self.expect(TokenKind::Eq)?;
            let value = if self.check(TokenKind::LBrace) {
                self.advance();
                let e = if self.check(TokenKind::Lt) {
                    let jsx = self.parse_jsx_element()?;
                    Expr::JSX(jsx, self.current_span())
                } else {
                    self.parse_expr()?
                };
                self.expect(TokenKind::RBrace)?;
                JSXAttrValue::Expr(e)
            } else if let TokenKind::String(s) = self.peek_kind() {
                self.advance();
                JSXAttrValue::String(s)
            } else {
                return Err(self.parser_error("Expected attribute value", self.current_span()));
            };
            attrs.push(JSXAttribute {
                name,
                value,
                span: self.current_span(),
            });
        }

        if self.check(TokenKind::Slash) {
            self.advance();
            self.expect(TokenKind::Gt)?;
            return Ok(self.finish_jsx_node(tag, attrs, vec![], start.merge(self.current_span())));
        }

        self.expect(TokenKind::Gt)?;

        let mut children = Vec::new();
        // Track the end of the previous token to detect gaps (whitespace)
        let mut last_end = self
            .tokens
            .get(self.pos - 1)
            .map(|t| t.span.end)
            .unwrap_or(0);
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
                    children.push(JSXNode::Text(
                        text_buffer.clone(),
                        text_start.merge(Span::new(last_end, last_end)),
                    ));
                    text_buffer.clear();
                }

                children.push(self.parse_jsx_braced_child()?);

                last_end = self
                    .tokens
                    .get(self.pos - 1)
                    .map(|t| t.span.end)
                    .unwrap_or(0);
                text_start = self.current_span(); // Reset text start for next text run
            } else if self.check(TokenKind::Lt) {
                if !text_buffer.is_empty() {
                    children.push(JSXNode::Text(
                        text_buffer.clone(),
                        text_start.merge(Span::new(last_end, last_end)),
                    ));
                    text_buffer.clear();
                }

                children.push(self.parse_jsx_element()?);

                last_end = self
                    .tokens
                    .get(self.pos - 1)
                    .map(|t| t.span.end)
                    .unwrap_or(0);
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
                    TokenKind::QuestionQuestion => consumed_text = Some("??".to_string()),
                    TokenKind::QuestionDot => consumed_text = Some("?.".to_string()),
                    TokenKind::Not => consumed_text = Some("!".to_string()),
                    TokenKind::Minus => consumed_text = Some("-".to_string()),
                    TokenKind::PlusPlus => consumed_text = Some("++".to_string()),
                    TokenKind::MinusMinus => consumed_text = Some("--".to_string()),
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
                        return Err(self.parser_error(
                            format!(
                                "Unexpected token in JSX child: {}. Use strings or {{}} for text.",
                                crate::error::token_kind_to_user_string(&self.peek_kind())
                            ),
                            self.current_span(),
                        ));
                    }
                }

                if let Some(t) = consumed_text {
                    if text_buffer.is_empty() {
                        text_start = self.current_span();
                    }
                    text_buffer.push_str(&t);
                    self.advance();
                }

                last_end = self
                    .tokens
                    .get(self.pos - 1)
                    .map(|t| t.span.end)
                    .unwrap_or(0);
            }
        }

        if !text_buffer.is_empty() {
            children.push(JSXNode::Text(
                text_buffer,
                text_start.merge(Span::new(last_end, last_end)),
            ));
        }

        self.expect(TokenKind::LtSlash)?;
        let closing_tag = self.parse_jsx_tag_name()?;
        if closing_tag != tag {
            return Err(self.parser_error(
                format!("Expected closing tag </{}>, found </{}>", tag, closing_tag),
                self.current_span(),
            ));
        }
        self.expect(TokenKind::Gt)?;

        Ok(self.finish_jsx_node(tag, attrs, children, start.merge(self.current_span())))
    }

    fn parse_jsx_tag_name(&mut self) -> KainResult<String> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(s) => {
                self.advance();
                Ok(s)
            }
            TokenKind::Fragment => {
                self.advance();
                Ok("Fragment".to_string())
            }
            TokenKind::Component => {
                self.advance();
                Ok("component".to_string())
            }
            other => Err(self.parser_error(
                format!(
                    "Expected JSX tag name, found {}",
                    crate::error::token_kind_to_user_string(&other)
                ),
                span,
            )),
        }
    }

    fn parse_jsx_attribute_name(&mut self) -> KainResult<String> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(s) => {
                self.advance();
                Ok(s)
            }
            other => {
                let source = self.span_mapper.source();
                let text = if span.end <= source.len() && span.start <= span.end {
                    &source[span.start..span.end]
                } else {
                    ""
                };

                if !text.is_empty()
                    && text.chars().enumerate().all(|(i, c)| {
                        if i == 0 {
                            c.is_ascii_alphabetic() || c == '_'
                        } else {
                            c.is_ascii_alphanumeric() || c == '_'
                        }
                    })
                {
                    self.advance();
                    Ok(text.to_string())
                } else {
                    Err(self.parser_error(
                        format!(
                            "Expected JSX attribute name, found {}",
                            crate::error::token_kind_to_user_string(&other)
                        ),
                        span,
                    ))
                }
            }
        }
    }

    fn finish_jsx_node(
        &self,
        tag: String,
        attributes: Vec<JSXAttribute>,
        children: Vec<JSXNode>,
        span: Span,
    ) -> JSXNode {
        if tag == "Fragment" {
            return JSXNode::Fragment(children, span);
        }

        if tag.chars().next().is_some_and(|c| c.is_ascii_uppercase()) {
            JSXNode::ComponentCall {
                name: tag,
                props: attributes,
                children,
                span,
            }
        } else {
            JSXNode::Element {
                tag,
                attributes,
                children,
                span,
            }
        }
    }

    fn parse_jsx_braced_child(&mut self) -> KainResult<JSXNode> {
        let start = self.current_span();
        self.expect(TokenKind::LBrace)?;

        let node = if self.check(TokenKind::If) {
            self.advance();
            let condition = self.parse_expr()?;
            self.expect(TokenKind::Colon)?;
            let then_branch = self.parse_jsx_inline_node()?;
            let else_branch = if self.check(TokenKind::Else) {
                self.advance();
                self.expect(TokenKind::Colon)?;
                Some(Box::new(self.parse_jsx_inline_node()?))
            } else {
                None
            };

            JSXNode::If {
                condition: Box::new(condition),
                then_branch: Box::new(then_branch),
                else_branch,
                span: start.merge(self.current_span()),
            }
        } else if self.check(TokenKind::For) {
            self.advance();
            let binding = self.parse_ident()?;
            self.expect(TokenKind::In)?;
            let iter = self.parse_expr()?;
            self.expect(TokenKind::Colon)?;
            let body = self.parse_jsx_inline_node()?;
            JSXNode::For {
                binding,
                iter: Box::new(iter),
                body: Box::new(body),
                span: start.merge(self.current_span()),
            }
        } else {
            JSXNode::Expression(Box::new(self.parse_expr()?))
        };

        self.expect(TokenKind::RBrace)?;
        Ok(node)
    }

    fn parse_jsx_inline_node(&mut self) -> KainResult<JSXNode> {
        if self.check(TokenKind::Lt) {
            self.parse_jsx_element()
        } else if self.check(TokenKind::LBrace) {
            self.parse_jsx_braced_child()
        } else {
            Ok(JSXNode::Expression(Box::new(self.parse_expr()?)))
        }
    }

    fn parse_call_args(&mut self) -> KainResult<Vec<CallArg>> {
        let mut args = Vec::new();
        self.skip_formatting();
        while !self.check(TokenKind::RParen) && !self.at_end() {
            let mut name = None;
            // Check for named argument: ident = expr
            if let TokenKind::Ident(s) = self.peek_kind() {
                // Look ahead for '='
                if self
                    .tokens
                    .get(self.pos + 1)
                    .map(|t| t.kind == TokenKind::Eq)
                    .unwrap_or(false)
                {
                    name = Some(s);
                    self.advance(); // eat ident
                    self.advance(); // eat =
                }
            }

            let value = self.parse_expr()?;
            args.push(CallArg {
                name,
                value,
                span: self.current_span(),
            });

            self.skip_formatting();
            if !self.check(TokenKind::RParen) {
                self.expect(TokenKind::Comma)?;
                self.skip_formatting();
            }
        }
        Ok(args)
    }

    fn parse_visibility(&mut self) -> Visibility {
        if self.check(TokenKind::Pub) {
            self.advance();
            Visibility::Public
        } else {
            Visibility::Private
        }
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
                 TokenKind::Collapse | TokenKind::Observe | TokenKind::Decay | TokenKind::Share | TokenKind::Fanout |
                 TokenKind::Test | TokenKind::Pure | TokenKind::Io | TokenKind::AsyncKw | TokenKind::Async |
                 TokenKind::Gpu | TokenKind::Reactive | TokenKind::Unsafe) => {
                Err(self.parser_error(
                    format!("{} is a reserved keyword and cannot be used as an identifier. Please choose a different name.", crate::error::token_kind_to_user_string(&k)),
                    span
                ))
            }
            k => Err(self.parser_error(format!("Expected identifier, got {}", crate::error::token_kind_to_user_string(&k)), span)),
        }
    }

    fn get_binary_op(&self) -> Option<(BinaryOp, u8)> {
        let candidate = match self.peek_kind() {
            TokenKind::Or => Some((BinaryOp::Or, 1)),
            TokenKind::And => Some((BinaryOp::And, 2)),
            TokenKind::Pipe => Some((BinaryOp::BitOr, 3)),
            TokenKind::Caret => Some((BinaryOp::BitXor, 4)),
            TokenKind::Amp => Some((BinaryOp::BitAnd, 5)),
            TokenKind::EqEq => Some((BinaryOp::Eq, 6)),
            TokenKind::NotEq => Some((BinaryOp::Ne, 6)),
            TokenKind::Lt => Some((BinaryOp::Lt, 7)),
            TokenKind::Gt => Some((BinaryOp::Gt, 7)),
            TokenKind::LtEq => Some((BinaryOp::Le, 7)),
            TokenKind::GtEq => Some((BinaryOp::Ge, 7)),
            TokenKind::Shl => Some((BinaryOp::Shl, 8)),
            TokenKind::Shr => Some((BinaryOp::Shr, 8)),
            TokenKind::Plus => Some((BinaryOp::Add, 9)),
            TokenKind::Minus => Some((BinaryOp::Sub, 9)),
            TokenKind::Star => Some((BinaryOp::Mul, 10)),
            TokenKind::Slash => Some((BinaryOp::Div, 10)),
            TokenKind::Percent => Some((BinaryOp::Mod, 10)),
            TokenKind::Power => Some((BinaryOp::Pow, 11)),
            _ => None,
        };

        candidate.filter(|(op, _)| self.capabilities.supports_parser_binary_op(*op))
    }

    fn get_assignment_binop(&self) -> Option<Option<BinaryOp>> {
        match self.peek_kind() {
            TokenKind::Eq => Some(None),
            TokenKind::PlusEq => Some(Some(BinaryOp::Add)),
            TokenKind::MinusEq => Some(Some(BinaryOp::Sub)),
            TokenKind::StarEq => Some(Some(BinaryOp::Mul)),
            TokenKind::SlashEq => Some(Some(BinaryOp::Div)),
            TokenKind::PercentEq => Some(Some(BinaryOp::Mod)),
            TokenKind::AmpEq => Some(Some(BinaryOp::BitAnd)),
            TokenKind::PipeEq => Some(Some(BinaryOp::BitOr)),
            TokenKind::CaretEq => Some(Some(BinaryOp::BitXor)),
            TokenKind::ShlEq => Some(Some(BinaryOp::Shl)),
            TokenKind::ShrEq => Some(Some(BinaryOp::Shr)),
            _ => None,
        }
    }

    fn make_incdec_expr(
        &mut self,
        target: Expr,
        increment: bool,
        prefix: bool,
        span: Span,
    ) -> KainResult<Expr> {
        if !matches!(
            target,
            Expr::Ident(_, _) | Expr::Field { .. } | Expr::Index { .. } | Expr::Deref(_, _)
        ) {
            return Err(self.parser_error(
                "Increment/decrement target must be assignable (identifier, field, index, or dereference)",
                span,
            ));
        }

        let binding = self.fresh_temp("__kain_incdec");
        let bound_ident = Expr::Ident(binding.clone(), span);
        let op = if increment {
            BinaryOp::Add
        } else {
            BinaryOp::Sub
        };

        let updated = Expr::Binary {
            left: Box::new(bound_ident.clone()),
            op,
            right: Box::new(Expr::Int(1, span)),
            span,
        };

        // Sequence assignment then return either new (prefix) or old (postfix) value.
        let assign_expr = Expr::Assign {
            target: Box::new(target.clone()),
            value: Box::new(updated.clone()),
            span,
        };
        let result_expr = if prefix { updated } else { bound_ident };
        let sequenced = Expr::Index {
            object: Box::new(Expr::Array(vec![assign_expr, result_expr], span)),
            index: Box::new(Expr::Int(1, span)),
            span,
        };

        Ok(Expr::Match {
            scrutinee: Box::new(target),
            arms: vec![MatchArm {
                pattern: Pattern::Binding {
                    name: binding,
                    mutable: false,
                    span,
                },
                guard: None,
                body: sequenced,
                span,
            }],
            span,
        })
    }

    fn make_null_coalesce_expr(&mut self, left: Expr, right: Expr, span: Span) -> KainResult<Expr> {
        let binding = self.fresh_temp("__kain_coalesce");
        Ok(Expr::Match {
            scrutinee: Box::new(left),
            arms: vec![
                MatchArm {
                    pattern: Pattern::Literal(Expr::None(span)),
                    guard: None,
                    body: right,
                    span,
                },
                MatchArm {
                    pattern: Pattern::Binding {
                        name: binding.clone(),
                        mutable: false,
                        span,
                    },
                    guard: None,
                    body: Expr::Ident(binding, span),
                    span,
                },
            ],
            span,
        })
    }

    fn make_safe_nav_field_expr(
        &mut self,
        object: Expr,
        field: String,
        span: Span,
    ) -> KainResult<Expr> {
        let binding = self.fresh_temp("__kain_safe");
        Ok(Expr::Match {
            scrutinee: Box::new(object),
            arms: vec![
                MatchArm {
                    pattern: Pattern::Literal(Expr::None(span)),
                    guard: None,
                    body: Expr::None(span),
                    span,
                },
                MatchArm {
                    pattern: Pattern::Binding {
                        name: binding.clone(),
                        mutable: false,
                        span,
                    },
                    guard: None,
                    body: Expr::Field {
                        object: Box::new(Expr::Ident(binding, span)),
                        field,
                        span,
                    },
                    span,
                },
            ],
            span,
        })
    }

    fn question_starts_ternary(&self) -> bool {
        let mut offset = 1usize;
        let mut paren_depth = 0usize;
        let mut bracket_depth = 0usize;
        let mut brace_depth = 0usize;

        while let Some(kind) = self.peek_kind_at(offset) {
            let at_top_level = paren_depth == 0 && bracket_depth == 0 && brace_depth == 0;
            match kind {
                TokenKind::LParen => paren_depth += 1,
                TokenKind::RParen => {
                    if paren_depth == 0 {
                        return false;
                    }
                    paren_depth -= 1;
                }
                TokenKind::LBracket => bracket_depth += 1,
                TokenKind::RBracket => {
                    if bracket_depth == 0 {
                        return false;
                    }
                    bracket_depth -= 1;
                }
                TokenKind::LBrace => brace_depth += 1,
                TokenKind::RBrace => {
                    if brace_depth == 0 {
                        return false;
                    }
                    brace_depth -= 1;
                }
                TokenKind::Colon if at_top_level => return true,
                TokenKind::Comma
                | TokenKind::Semi
                | TokenKind::Eof
                | TokenKind::Dedent
                | TokenKind::Newline(_)
                    if at_top_level =>
                {
                    return false
                }
                _ => {}
            }
            offset += 1;
        }

        false
    }

    // Helper methods
    fn fresh_temp(&mut self, prefix: &str) -> String {
        let name = format!("{}{}", prefix, self.synthetic_counter);
        self.synthetic_counter += 1;
        name
    }

    fn peek_kind_at(&self, offset: usize) -> Option<TokenKind> {
        if offset < self.injected_tokens.len() {
            return Some(self.injected_tokens[offset].kind.clone());
        }
        let base_offset = offset.saturating_sub(self.injected_tokens.len());
        self.tokens
            .get(self.pos + base_offset)
            .map(|t| t.kind.clone())
    }

    fn peek_kind(&self) -> TokenKind {
        // Check injected tokens first
        if !self.injected_tokens.is_empty() {
            return self.injected_tokens[0].kind.clone();
        }
        self.tokens
            .get(self.pos)
            .map(|t| t.kind.clone())
            .unwrap_or(TokenKind::Eof)
    }

    fn current_span(&self) -> Span {
        // Check injected tokens first
        if !self.injected_tokens.is_empty() {
            return self.injected_tokens[0].span;
        }
        self.tokens
            .get(self.pos)
            .map(|t| t.span)
            .unwrap_or(Span::new(0, 0))
    }

    fn at_end(&self) -> bool {
        matches!(self.peek_kind(), TokenKind::Eof)
    }
    fn check(&self, k: TokenKind) -> bool {
        std::mem::discriminant(&self.peek_kind()) == std::mem::discriminant(&k)
    }
    fn check_line_end(&self) -> bool {
        matches!(
            self.peek_kind(),
            TokenKind::Newline(_) | TokenKind::Dedent | TokenKind::Eof
        )
    }

    fn range_expr_end_is_omitted(&self) -> bool {
        matches!(
            self.peek_kind(),
            TokenKind::Newline(_)
                | TokenKind::Dedent
                | TokenKind::Eof
                | TokenKind::Comma
                | TokenKind::Colon
                | TokenKind::Semi
                | TokenKind::RParen
                | TokenKind::RBracket
                | TokenKind::RBrace
        )
    }

    fn advance(&mut self) {
        // Consume injected tokens first
        if !self.injected_tokens.is_empty() {
            self.injected_tokens.remove(0);
            return;
        }
        if !self.at_end() {
            self.pos += 1;
        }
    }

    fn skip_newlines(&mut self) {
        while let TokenKind::Newline(_) = self.peek_kind() {
            self.advance();
        }
    }
    fn check_newline(&self) -> bool {
        matches!(self.peek_kind(), TokenKind::Newline(_))
    }

    fn peek_contextual_ident(&self, expected: &str) -> bool {
        matches!(self.peek_kind(), TokenKind::Ident(ref value) if value == expected)
    }

    fn skip_formatting(&mut self) {
        while matches!(
            self.peek_kind(),
            TokenKind::Newline(_) | TokenKind::Indent | TokenKind::Dedent
        ) {
            self.advance();
        }
    }

    fn inject_token(&mut self, token: Token) {
        self.injected_tokens.push(token);
    }

    fn expect(&mut self, k: TokenKind) -> KainResult<()> {
        if self.check(k.clone()) {
            self.advance();
            Ok(())
        } else {
            let expected = crate::error::token_kind_to_user_string(&k);
            let actual = crate::error::token_kind_to_user_string(&self.peek_kind());
            if matches!(k, TokenKind::Colon)
                && matches!(self.peek_kind(), TokenKind::Newline(_) | TokenKind::Dedent)
            {
                let boundary_span = self.current_span();
                let header_span = self.previous_significant_span().unwrap_or(boundary_span);
                let report = self
                    .parser_report_at(
                        "Missing ':' before line break",
                        header_span,
                        "this header or declaration ended without ':'",
                    )
                    .label(boundary_span, "the next line started while ':' was still expected")
                    .note(
                        "Expected ':' before newline: Kain block headers and declarations must end with ':'.",
                    )
                    .note(
                        "If this was meant to be a continued expression, wrap it in parentheses or keep it on one logical line.",
                    )
                    .help("Look immediately before the highlighted line break; the following line may only be where recovery noticed the damage.")
                    .fixit(
                        Span::new(header_span.end, header_span.end),
                        ":",
                        "insert ':' at the end of the header",
                    );
                return Err(self.rich_parser_report(report));
            }

            let report = self
                .parser_report_at(
                    format!("Expected {}, got {}", expected, actual),
                    self.current_span(),
                    format!("expected {} here", expected),
                )
                .note(format!(
                    "Parser was in a grammar state that accepts {} but saw {} instead.",
                    expected, actual
                ))
                .help("Check the token immediately before this point; most parse errors are caused by the previous unfinished construct.");
            Err(self.rich_parser_report(report))
        }
    }

    fn expect_contextual_ident(&mut self, expected: &str) -> KainResult<()> {
        if self.peek_contextual_ident(expected) {
            self.advance();
            Ok(())
        } else {
            let actual = crate::error::token_kind_to_user_string(&self.peek_kind());
            let report = self
                .parser_report_at(
                    format!("Expected contextual keyword '{}', got {}", expected, actual),
                    self.current_span(),
                    format!("expected contextual keyword '{}' here", expected),
                )
                .note("Kain keeps several advanced forms as contextual keywords so they can coexist with normal identifiers outside that grammar slot.");
            Err(self.rich_parser_report(report))
        }
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    format!(
                        "Expected @graph_data, @node_data, @instance, or @pin_config attribute in graph runtime body, found {}. Graph runtimes define the execution model for custom graph editors",
                        self.token_to_user_string(&self.peek_kind())
                    ),
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    format!(
                        "Expected @input_pin, @output_pin, @property, or 'fn' in node data body, found {}. Node data defines the structure and behavior of graph nodes",
                        self.token_to_user_string(&self.peek_kind())
                    ),
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                    self.current_span(),
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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
            let loop_pos = self.pos;
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for @state attribute
            let state_attrs = self.parse_attributes()?;

            if state_attrs.iter().any(|a| a.name == "state") {
                states.push(self.parse_state(state_attrs)?);
            } else {
                return Err(self.parser_error(
                    "Expected @state in state machine definition",
                    self.current_span(),
                ));
            }

            self.skip_newlines();
            // Guard against no-progress loops
            if self.pos == loop_pos && !self.at_end() {
                self.advance();
            }
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
                            if let (Expr::Ident(name, _), Expr::Bool(true, _)) =
                                (&parts[0], &parts[1])
                            {
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

        // State name: bare `idle:` syntax (no `struct` keyword needed, but tolerate it)
        if self.check(TokenKind::Struct) {
            self.advance(); // consume optional `struct`
        }

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
            let loop_pos = self.pos;
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                        self.current_span(),
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
                            self.current_span(),
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
            // Guard against no-progress loops
            if self.pos == loop_pos && !self.at_end() {
                self.advance();
            }
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
        let to_state = attributes
            .iter()
            .find(|a| a.name == "transition")
            .and_then(|attr| {
                // Look for 'to' parameter
                attr.args.iter().find_map(|arg| {
                    if let Expr::Tuple(parts, _) = arg {
                        if parts.len() == 2 {
                            if let (Expr::Ident(param_name, _), Expr::String(state_name, _)) =
                                (&parts[0], &parts[1])
                            {
                                if param_name == "to" {
                                    return Some(state_name.clone());
                                }
                            }
                        }
                    }
                    None
                })
            })
            .ok_or_else(|| {
                self.parser_error(
                    "Expected 'to' parameter in @transition attribute",
                    self.current_span(),
                )
            })?;

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
            if self.check(TokenKind::Dedent) {
                break;
            }

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
                if let Item::Function(func) =
                    self.parse_function_with_attrs(Visibility::Public, method_attrs)?
                {
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
        let menu_attr = attributes
            .iter()
            .find(|a| a.name == "menu_entry")
            .ok_or_else(|| {
                self.parser_error("Expected @menu_entry attribute", self.current_span())
            })?;

        let mut path = None;
        let mut label = None;
        let mut icon = None;
        let mut tooltip = None;

        // Parse named arguments from attribute
        for arg in &menu_attr.args {
            if let Expr::Tuple(parts, _) = arg {
                if parts.len() == 2 {
                    if let (Expr::Ident(param_name, _), Expr::String(value, _)) =
                        (&parts[0], &parts[1])
                    {
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

        let path = path.ok_or_else(|| {
            self.parser_error("@menu_entry requires 'path' parameter", self.current_span())
        })?;
        let label = label.ok_or_else(|| {
            self.parser_error(
                "@menu_entry requires 'label' parameter",
                self.current_span(),
            )
        })?;

        // Parse the method
        if let Item::Function(method) =
            self.parse_function_with_attrs(Visibility::Public, vec![])?
        {
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
        let toolbar_attr = attributes
            .iter()
            .find(|a| a.name == "toolbar_button")
            .ok_or_else(|| {
                self.parser_error("Expected @toolbar_button attribute", self.current_span())
            })?;

        let mut section = None;
        let mut label = None;
        let mut icon = None;
        let mut tooltip = None;

        // Parse named arguments from attribute
        for arg in &toolbar_attr.args {
            if let Expr::Tuple(parts, _) = arg {
                if parts.len() == 2 {
                    if let (Expr::Ident(param_name, _), Expr::String(value, _)) =
                        (&parts[0], &parts[1])
                    {
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

        let section = section.ok_or_else(|| {
            self.parser_error(
                "@toolbar_button requires 'section' parameter",
                self.current_span(),
            )
        })?;
        let icon = icon.ok_or_else(|| {
            self.parser_error(
                "@toolbar_button requires 'icon' parameter",
                self.current_span(),
            )
        })?;

        // Parse the method
        if let Item::Function(method) =
            self.parse_function_with_attrs(Visibility::Public, vec![])?
        {
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
            Err(self.parser_error(
                "Expected function after @toolbar_button",
                self.current_span(),
            ))
        }
    }

    /// Parse @toolbar_widget
    fn parse_toolbar_widget(&mut self, attributes: Vec<Attribute>) -> KainResult<ToolbarWidgetDef> {
        let start = self.current_span();

        // Extract parameters from @toolbar_widget attribute
        let widget_attr = attributes
            .iter()
            .find(|a| a.name == "toolbar_widget")
            .ok_or_else(|| {
                self.parser_error("Expected @toolbar_widget attribute", self.current_span())
            })?;

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

        let section = section.ok_or_else(|| {
            self.parser_error(
                "@toolbar_widget requires 'section' parameter",
                self.current_span(),
            )
        })?;
        let position = position.unwrap_or(ToolbarPosition::After);
        let widget_type = widget_type.ok_or_else(|| {
            self.parser_error(
                "@toolbar_widget requires 'widget_type' parameter",
                self.current_span(),
            )
        })?;

        Ok(ToolbarWidgetDef {
            section,
            position,
            widget_type,
            attributes,
            span: start.merge(self.current_span()),
        })
    }

    /// Parse @gameplay_tags namespace definition
    ///
    /// Syntax:
    /// ```kain
    /// @gameplay_tags
    /// namespace Ability:
    ///     Attack:
    ///         Melee:
    ///             Sword
    ///             Axe
    ///         Ranged:
    ///             Bow
    /// ```
    fn parse_gameplay_tags(&mut self, _attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();
        self.skip_formatting();

        // Expect 'namespace' keyword
        if let TokenKind::Ident(ref s) = self.peek_kind() {
            if s != "namespace" {
                return Err(self.parser_error(
                    "Expected 'namespace' keyword after @gameplay_tags",
                    self.current_span(),
                ));
            }
            self.advance(); // consume 'namespace'
        } else {
            return Err(self.parser_error(
                "Expected 'namespace' keyword after @gameplay_tags",
                self.current_span(),
            ));
        }

        // Parse namespace name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse tag hierarchy (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        let children = self.parse_tag_hierarchy(&name)?;

        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::GameplayTags(GameplayTagsNamespace {
            name,
            children,
            span: start.merge(self.current_span()),
        }))
    }

    /// Parse tag hierarchy recursively
    /// Returns a list of tag nodes at the current indentation level
    fn parse_tag_hierarchy(&mut self, parent_path: &str) -> KainResult<Vec<GameplayTagNode>> {
        let mut nodes = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            let start = self.current_span();

            // Parse tag name
            let tag_name = self.parse_tag_name()?;

            // Build full path
            let full_path = if parent_path.is_empty() {
                tag_name.clone()
            } else {
                format!("{}.{}", parent_path, tag_name)
            };

            // Check for optional comment (after tag name, before colon or newline)
            let comment = None; // TODO: Support inline comments if needed

            // Check if this tag has children (colon + indent)
            let children = if self.check(TokenKind::Colon) {
                self.advance(); // consume ':'
                self.skip_newlines();

                if self.check(TokenKind::Indent) {
                    self.advance(); // consume indent
                    let child_nodes = self.parse_tag_hierarchy(&full_path)?;

                    if self.check(TokenKind::Dedent) {
                        self.advance();
                    }

                    child_nodes
                } else {
                    // Colon without indent - empty children
                    Vec::new()
                }
            } else {
                // No colon - leaf tag
                Vec::new()
            };

            nodes.push(GameplayTagNode {
                name: tag_name,
                full_path,
                comment,
                children,
                span: start.merge(self.current_span()),
            });

            self.skip_newlines();
        }

        Ok(nodes)
    }

    fn parse_tag_name(&mut self) -> KainResult<String> {
        let span = self.current_span();
        match self.peek_kind() {
            TokenKind::Ident(s) => {
                self.advance();
                Ok(s)
            }
            TokenKind::Pure => {
                self.advance();
                Ok("Pure".to_string())
            }
            TokenKind::Io => {
                self.advance();
                Ok("IO".to_string())
            }
            TokenKind::Async => {
                self.advance();
                Ok("Async".to_string())
            }
            TokenKind::AsyncKw => {
                self.advance();
                Ok("async".to_string())
            }
            TokenKind::Reactive => {
                self.advance();
                Ok("Reactive".to_string())
            }
            TokenKind::Unsafe => {
                self.advance();
                Ok("Unsafe".to_string())
            }
            TokenKind::Gpu => {
                self.advance();
                Ok("GPU".to_string())
            }
            TokenKind::True => {
                self.advance();
                Ok("true".to_string())
            }
            TokenKind::False => {
                self.advance();
                Ok("false".to_string())
            }
            TokenKind::None => {
                self.advance();
                Ok("none".to_string())
            }
            k => Err(self.parser_error(
                format!(
                    "Expected gameplay tag name, got {}",
                    crate::error::token_kind_to_user_string(&k)
                ),
                span,
            )),
        }
    }

    /// Parse gameplay ability definition
    ///
    /// Syntax:
    /// ```kain
    /// @ability
    /// struct JumpAbility:
    ///     @instancing(policy: "InstancedPerExecution")
    ///     @replication(policy: "ReplicateYes")
    ///     @net_execution(policy: "LocalPredicted")
    ///     
    ///     @ability_tags
    ///     tags: ["Ability.Jump"]
    ///     
    ///     @activation_required_tags
    ///     required: ["Status.Grounded"]
    ///     
    ///     @activation_blocked_tags
    ///     blocked: ["Status.Stunned"]
    ///     
    ///     @cost
    ///     effect: StaminaCostEffect
    ///     
    ///     fn activate_ability(handle, actor_info, activation_info, trigger_event_data):
    ///         # implementation
    /// ```
    fn parse_gameplay_ability(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;

        // Parse ability name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse ability body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        // Initialize fields
        let mut instancing_policy: Option<String> = None;
        let mut replication_policy: Option<String> = None;
        let mut net_execution_policy: Option<String> = None;
        let mut ability_tags: Vec<String> = Vec::new();
        let mut activation_required_tags: Vec<String> = Vec::new();
        let mut activation_blocked_tags: Vec<String> = Vec::new();
        let mut activation_owned_tags: Vec<String> = Vec::new();
        let mut cancel_abilities_with_tag: Vec<String> = Vec::new();
        let mut block_abilities_with_tag: Vec<String> = Vec::new();
        let mut cost_effect: Option<String> = None;
        let mut cooldown_effect: Option<String> = None;
        let mut methods: Vec<Function> = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for attributes or methods
            if self.check(TokenKind::At) {
                // Parse attribute
                let attr_start = self.current_span();
                self.advance(); // consume '@'
                let attr_name = self.parse_attribute_name()?;

                match attr_name.as_str() {
                    "instancing" => {
                        // @instancing(policy: "InstancedPerExecution")
                        self.expect(TokenKind::LParen)?;

                        // Expect "policy:"
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "policy" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse policy value (string)
                                if let TokenKind::String(policy_str) = self.peek_kind() {
                                    instancing_policy = Some(policy_str);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected string value for instancing policy",
                                        self.current_span(),
                                    ));
                                }
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'policy' parameter in @instancing",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'policy' parameter in @instancing",
                                self.current_span(),
                            ));
                        }

                        self.expect(TokenKind::RParen)?;
                    }
                    "replication" => {
                        // @replication(policy: "ReplicateYes")
                        self.expect(TokenKind::LParen)?;

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "policy" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                if let TokenKind::String(policy_str) = self.peek_kind() {
                                    replication_policy = Some(policy_str);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected string value for replication policy",
                                        self.current_span(),
                                    ));
                                }
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'policy' parameter in @replication",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'policy' parameter in @replication",
                                self.current_span(),
                            ));
                        }

                        self.expect(TokenKind::RParen)?;
                    }
                    "net_execution" => {
                        // @net_execution(policy: "LocalPredicted")
                        self.expect(TokenKind::LParen)?;

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "policy" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                if let TokenKind::String(policy_str) = self.peek_kind() {
                                    net_execution_policy = Some(policy_str);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected string value for net_execution policy",
                                        self.current_span(),
                                    ));
                                }
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'policy' parameter in @net_execution",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'policy' parameter in @net_execution",
                                self.current_span(),
                            ));
                        }

                        self.expect(TokenKind::RParen)?;
                    }
                    "ability_tags" => {
                        // @ability_tags
                        // tags: ["Ability.Jump"]
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "tags" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                ability_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'tags' field after @ability_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'tags' field after @ability_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "activation_required_tags" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "required" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                activation_required_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'required' field after @activation_required_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'required' field after @activation_required_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "activation_blocked_tags" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "blocked" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                activation_blocked_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'blocked' field after @activation_blocked_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'blocked' field after @activation_blocked_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "activation_owned_tags" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "owned" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                activation_owned_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'owned' field after @activation_owned_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'owned' field after @activation_owned_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "cancel_abilities_with_tag" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "cancel" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                cancel_abilities_with_tag = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'cancel' field after @cancel_abilities_with_tag",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'cancel' field after @cancel_abilities_with_tag",
                                self.current_span(),
                            ));
                        }
                    }
                    "block_abilities_with_tag" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "block" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                block_abilities_with_tag = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'block' field after @block_abilities_with_tag",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'block' field after @block_abilities_with_tag",
                                self.current_span(),
                            ));
                        }
                    }
                    "cost" => {
                        // @cost
                        // effect: StaminaCostEffect
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "effect" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse effect name (identifier)
                                cost_effect = Some(self.parse_ident()?);
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'effect' field after @cost",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'effect' field after @cost",
                                self.current_span(),
                            ));
                        }
                    }
                    "cooldown" => {
                        // @cooldown
                        // effect: JumpCooldownEffect
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "effect" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse effect name (identifier)
                                cooldown_effect = Some(self.parse_ident()?);
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'effect' field after @cooldown",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'effect' field after @cooldown",
                                self.current_span(),
                            ));
                        }
                    }
                    "net_security" => {
                        // @net_security(policy: "ClientOrServer") - parse but ignore for now
                        if self.check(TokenKind::LParen) {
                            self.advance();
                            // Skip parameters
                            while !self.check(TokenKind::RParen) && !self.at_end() {
                                self.advance();
                            }
                            self.expect(TokenKind::RParen)?;
                        }
                    }
                    _ => {
                        return Err(self.parser_error(
                            format!("Unknown attribute in @ability: @{}", attr_name),
                            attr_start,
                        ));
                    }
                }
            } else if self.check(TokenKind::Fn) {
                // Parse lifecycle hook method
                if let Item::Function(func) = self.parse_function(Visibility::Public)? {
                    methods.push(func);
                }
            } else {
                return Err(self.parser_error(
                    "Expected attribute (@instancing, @ability_tags, etc.) or method (fn) in ability body",
                    self.current_span()
                ));
            }

            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::GameplayAbility(GameplayAbilityDef {
            name,
            instancing_policy,
            replication_policy,
            net_execution_policy,
            ability_tags,
            activation_required_tags,
            activation_blocked_tags,
            activation_owned_tags,
            cancel_abilities_with_tag,
            block_abilities_with_tag,
            cost_effect,
            cooldown_effect,
            methods,
            attributes,
            span: start.merge(self.current_span()),
        }))
    }

    /// Parse string array: ["tag1", "tag2", "tag3"]
    fn parse_string_array(&mut self) -> KainResult<Vec<String>> {
        self.expect(TokenKind::LBracket)?;

        let mut strings = Vec::new();

        while !self.check(TokenKind::RBracket) && !self.at_end() {
            if let TokenKind::String(s) = self.peek_kind() {
                strings.push(s);
                self.advance();
            } else {
                return Err(self.parser_error("Expected string in array", self.current_span()));
            }

            if !self.check(TokenKind::RBracket) {
                self.expect(TokenKind::Comma)?;
            }
        }

        self.expect(TokenKind::RBracket)?;

        Ok(strings)
    }

    /// Parse gameplay effect definition
    /// Syntax:
    /// ```kain
    /// @gameplay_effect
    /// struct BurnEffect:
    ///     @duration(type: "HasDuration")
    ///     duration: 5.0
    ///     
    ///     @period
    ///     period: 1.0
    ///     execute_on_application: true
    ///     
    ///     @modifier(attribute: "Health", operation: "Add")
    ///     damage_per_tick: -10.0
    ///     
    ///     @stacking
    ///     type: "AggregateBySource"
    ///     limit: 5
    ///     
    ///     @owned_tags
    ///     tags: ["Effect.Burn"]
    ///     
    ///     @granted_tags
    ///     tags: ["Status.Burning"]
    ///     
    ///     @application_tag_requirements
    ///     require: ["Weakness.Fire"]
    ///     ignore: ["Immunity.Fire"]
    /// ```
    fn parse_gameplay_effect(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;

        // Parse effect name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse effect body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        // Initialize fields
        let mut duration_policy: Option<String> = None;
        let mut duration_magnitude: Option<f32> = None;
        let mut period: Option<f32> = None;
        let mut execute_on_application = false;
        let mut modifiers: Vec<GameplayEffectModifier> = Vec::new();
        let mut stacking_type: Option<String> = None;
        let mut stacking_limit: Option<i32> = None;
        let mut owned_tags: Vec<String> = Vec::new();
        let mut granted_tags: Vec<String> = Vec::new();
        let mut application_required_tags: Vec<String> = Vec::new();
        let mut application_ignored_tags: Vec<String> = Vec::new();
        let mut ongoing_required_tags: Vec<String> = Vec::new();
        let mut ongoing_ignored_tags: Vec<String> = Vec::new();
        let mut removal_required_tags: Vec<String> = Vec::new();
        let mut removal_ignored_tags: Vec<String> = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for attributes or fields
            if self.check(TokenKind::At) {
                // Parse attribute
                let attr_start = self.current_span();
                self.advance(); // consume '@'
                let attr_name = self.parse_attribute_name()?;

                match attr_name.as_str() {
                    "duration" => {
                        // @duration(type: "HasDuration")
                        self.expect(TokenKind::LParen)?;

                        // Expect "type:"
                        if matches!(self.peek_kind(), TokenKind::TypeKw)
                            || matches!(self.peek_kind(), TokenKind::Ident(ref s) if s == "type")
                        {
                            self.advance();
                            self.expect(TokenKind::Colon)?;

                            // Parse type value (string)
                            if let TokenKind::String(policy_str) = self.peek_kind() {
                                duration_policy = Some(policy_str);
                                self.advance();
                            } else {
                                return Err(self.parser_error(
                                    "Expected string value for duration type",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'type' parameter in @duration",
                                self.current_span(),
                            ));
                        }

                        self.expect(TokenKind::RParen)?;

                        // Next line should be duration: 5.0
                        self.skip_newlines();
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "duration" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse float value
                                if let TokenKind::Float(val) = self.peek_kind() {
                                    duration_magnitude = Some(val as f32);
                                    self.advance();
                                } else if let TokenKind::Int(val) = self.peek_kind() {
                                    duration_magnitude = Some(val as f32);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected numeric value for duration",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }
                    }
                    "period" => {
                        // @period
                        // period: 1.0
                        // execute_on_application: true
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "period" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse float value
                                if let TokenKind::Float(val) = self.peek_kind() {
                                    period = Some(val as f32);
                                    self.advance();
                                } else if let TokenKind::Int(val) = self.peek_kind() {
                                    period = Some(val as f32);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected numeric value for period",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }

                        // Check for execute_on_application
                        self.skip_newlines();
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "execute_on_application" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                // Parse bool value
                                if let TokenKind::True = self.peek_kind() {
                                    execute_on_application = true;
                                    self.advance();
                                } else if let TokenKind::False = self.peek_kind() {
                                    execute_on_application = false;
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected boolean value for execute_on_application",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }
                    }
                    "modifier" => {
                        // @modifier(attribute: "Health", operation: "Add")
                        // damage_per_tick: -10.0
                        self.expect(TokenKind::LParen)?;

                        let mut modifier_attribute: Option<String> = None;
                        let mut modifier_operation: Option<String> = None;

                        // Parse attribute parameter
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "attribute" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                if let TokenKind::String(attr_str) = self.peek_kind() {
                                    modifier_attribute = Some(attr_str);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected string value for attribute",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }

                        self.expect(TokenKind::Comma)?;

                        // Parse operation parameter
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "operation" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                if let TokenKind::String(op_str) = self.peek_kind() {
                                    modifier_operation = Some(op_str);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected string value for operation",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }

                        self.expect(TokenKind::RParen)?;

                        // Next line should be magnitude field
                        self.skip_newlines();
                        let _field_name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;

                        // Parse magnitude value
                        let magnitude = if let TokenKind::Float(val) = self.peek_kind() {
                            self.advance();
                            val as f32
                        } else if let TokenKind::Int(val) = self.peek_kind() {
                            self.advance();
                            val as f32
                        } else if let TokenKind::Minus = self.peek_kind() {
                            self.advance();
                            if let TokenKind::Float(val) = self.peek_kind() {
                                self.advance();
                                -(val as f32)
                            } else if let TokenKind::Int(val) = self.peek_kind() {
                                self.advance();
                                -(val as f32)
                            } else {
                                return Err(self.parser_error(
                                    "Expected numeric value after minus",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected numeric value for magnitude",
                                self.current_span(),
                            ));
                        };

                        if let (Some(attr), Some(op)) = (modifier_attribute, modifier_operation) {
                            modifiers.push(GameplayEffectModifier {
                                attribute: attr,
                                operation: op,
                                magnitude,
                                span: attr_start,
                            });
                        }
                    }
                    "stacking" => {
                        // @stacking
                        // type: "AggregateBySource"
                        // limit: 5
                        self.skip_newlines();

                        if matches!(self.peek_kind(), TokenKind::TypeKw)
                            || matches!(self.peek_kind(), TokenKind::Ident(ref s) if s == "type")
                        {
                            self.advance();
                            self.expect(TokenKind::Colon)?;

                            if let TokenKind::String(type_str) = self.peek_kind() {
                                stacking_type = Some(type_str);
                                self.advance();
                            } else {
                                return Err(self.parser_error(
                                    "Expected string value for stacking type",
                                    self.current_span(),
                                ));
                            }
                        }

                        self.skip_newlines();
                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "limit" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                if let TokenKind::Int(val) = self.peek_kind() {
                                    stacking_limit = Some(val as i32);
                                    self.advance();
                                } else {
                                    return Err(self.parser_error(
                                        "Expected integer value for stacking limit",
                                        self.current_span(),
                                    ));
                                }
                            }
                        }
                    }
                    "owned_tags" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "tags" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                owned_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'tags' field after @owned_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'tags' field after @owned_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "granted_tags" => {
                        self.skip_newlines();

                        if let TokenKind::Ident(ref s) = self.peek_kind() {
                            if s == "tags" {
                                self.advance();
                                self.expect(TokenKind::Colon)?;
                                granted_tags = self.parse_string_array()?;
                            } else {
                                return Err(self.parser_error(
                                    "Expected 'tags' field after @granted_tags",
                                    self.current_span(),
                                ));
                            }
                        } else {
                            return Err(self.parser_error(
                                "Expected 'tags' field after @granted_tags",
                                self.current_span(),
                            ));
                        }
                    }
                    "application_tag_requirements" => {
                        self.skip_newlines();

                        // Parse require and ignore arrays
                        while !self.check(TokenKind::At)
                            && !self.check(TokenKind::Dedent)
                            && !self.at_end()
                        {
                            if let TokenKind::Ident(ref s) = self.peek_kind() {
                                if s == "require" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    application_required_tags = self.parse_string_array()?;
                                } else if s == "ignore" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    application_ignored_tags = self.parse_string_array()?;
                                } else {
                                    break;
                                }
                            } else {
                                break;
                            }
                            self.skip_newlines();
                        }
                    }
                    "ongoing_tag_requirements" => {
                        self.skip_newlines();

                        while !self.check(TokenKind::At)
                            && !self.check(TokenKind::Dedent)
                            && !self.at_end()
                        {
                            if let TokenKind::Ident(ref s) = self.peek_kind() {
                                if s == "require" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    ongoing_required_tags = self.parse_string_array()?;
                                } else if s == "ignore" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    ongoing_ignored_tags = self.parse_string_array()?;
                                } else {
                                    break;
                                }
                            } else {
                                break;
                            }
                            self.skip_newlines();
                        }
                    }
                    "removal_tag_requirements" => {
                        self.skip_newlines();

                        while !self.check(TokenKind::At)
                            && !self.check(TokenKind::Dedent)
                            && !self.at_end()
                        {
                            if let TokenKind::Ident(ref s) = self.peek_kind() {
                                if s == "require" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    removal_required_tags = self.parse_string_array()?;
                                } else if s == "ignore" {
                                    self.advance();
                                    self.expect(TokenKind::Colon)?;
                                    removal_ignored_tags = self.parse_string_array()?;
                                } else {
                                    break;
                                }
                            } else {
                                break;
                            }
                            self.skip_newlines();
                        }
                    }
                    _ => {
                        // Unknown attribute - skip for now (could be @gameplay_cues, @immunity, etc.)
                        // These will be handled in future phases
                        if self.check(TokenKind::LParen) {
                            self.advance();
                            while !self.check(TokenKind::RParen) && !self.at_end() {
                                self.advance();
                            }
                            if self.check(TokenKind::RParen) {
                                self.advance();
                            }
                        }
                    }
                }
            } else {
                // Skip unknown fields for now
                self.advance();
            }

            self.skip_newlines();
        }

        if self.check(TokenKind::Dedent) {
            self.advance();
        }

        Ok(Item::GameplayEffect(GameplayEffectDef {
            name,
            duration_policy,
            duration_magnitude,
            period,
            execute_on_application,
            modifiers,
            stacking_type,
            stacking_limit,
            owned_tags,
            granted_tags,
            application_required_tags,
            application_ignored_tags,
            ongoing_required_tags,
            ongoing_ignored_tags,
            removal_required_tags,
            removal_ignored_tags,
            attributes,
            span: start.merge(self.current_span()),
        }))
    }

    /// Parse gameplay cue definition
    /// Syntax:
    /// ```kain
    /// @gameplay_cue
    /// struct BurnCue:
    ///     tag: "GameplayCue.Effect.Burn"
    ///     type: "Static"  # or "Actor"
    ///     auto_destroy: true
    ///     
    ///     state particle_system: ParticleSystemComponent
    ///     
    ///     on_execute:
    ///         spawn_particle("P_Burn", location)
    ///     
    ///     on_add:
    ///         spawn_particle_attached("P_Burn_Loop", target)
    ///     
    ///     on_remove:
    ///         spawn_particle("P_Burn_End", location)
    ///     
    ///     while_active(delta_time):
    ///         update_particle_color(delta_time)
    /// ```
    fn parse_gameplay_cue(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;

        // Parse cue name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse cue body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        // Initialize fields
        let mut tag: Option<String> = None;
        let mut cue_type = CueType::default();
        let mut auto_destroy = false;
        let mut state_fields: Vec<Field> = Vec::new();
        let mut on_execute: Option<Function> = None;
        let mut on_add: Option<Function> = None;
        let mut on_remove: Option<Function> = None;
        let mut while_active: Option<Function> = None;

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for field or lifecycle method
            let field_name = match self.peek_kind() {
                TokenKind::Ident(name) => {
                    self.advance();
                    name
                }
                TokenKind::TypeKw => {
                    self.advance();
                    "type".to_string()
                }
                _ => {
                    return Err(self.parser_error("Expected field name", self.current_span()));
                }
            };
            self.expect(TokenKind::Colon)?;

            match field_name.as_str() {
                "tag" => {
                    // Parse tag string
                    if let TokenKind::String(tag_str) = self.peek_kind() {
                        tag = Some(tag_str);
                        self.advance();
                    } else {
                        return Err(
                            self.parser_error("Expected string for tag", self.current_span())
                        );
                    }
                }
                "type" => {
                    // Parse cue type
                    if let TokenKind::String(type_str) = self.peek_kind() {
                        cue_type = match type_str.as_str() {
                            "Static" => CueType::Static,
                            "Actor" => CueType::Actor,
                            _ => {
                                return Err(self.parser_error(
                                    format!(
                                        "Invalid cue type '{}'. Valid: Static, Actor",
                                        type_str
                                    ),
                                    self.current_span(),
                                ))
                            }
                        };
                        self.advance();
                    } else {
                        return Err(
                            self.parser_error("Expected string for type", self.current_span())
                        );
                    }
                }
                "auto_destroy" => {
                    // Parse boolean
                    if let TokenKind::True = self.peek_kind() {
                        auto_destroy = true;
                        self.advance();
                    } else if let TokenKind::False = self.peek_kind() {
                        auto_destroy = false;
                        self.advance();
                    } else {
                        return Err(
                            self.parser_error("Expected true or false", self.current_span())
                        );
                    }
                }
                "state" => {
                    // Parse state field: state field_name: Type
                    let state_field_name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let field_type = self.parse_type()?;

                    state_fields.push(Field {
                        name: state_field_name,
                        ty: field_type,
                        attributes: vec![],
                        visibility: Visibility::Private,
                        default: None,
                        weak: false,
                        span: self.current_span(),
                    });
                }
                "on_execute" | "on_add" | "on_remove" | "while_active" => {
                    // Parse lifecycle method
                    self.skip_newlines();
                    self.expect(TokenKind::Indent)?;

                    // Parse function body (statements)
                    let mut body_stmts = Vec::new();
                    while !self.check(TokenKind::Dedent) && !self.at_end() {
                        self.skip_newlines();
                        if self.check(TokenKind::Dedent) {
                            break;
                        }
                        body_stmts.push(self.parse_stmt()?);
                    }

                    self.expect(TokenKind::Dedent)?;

                    // Create function def
                    let params = if field_name == "while_active" {
                        vec![Param {
                            name: "delta_time".to_string(),
                            ty: Type::Named {
                                name: "Float".to_string(),
                                generics: vec![],
                                span: self.current_span(),
                            },
                            mutable: false,
                            default: None,
                            span: self.current_span(),
                        }]
                    } else {
                        vec![]
                    };

                    let func_def = Function {
                        name: field_name.clone(),
                        generics: vec![],
                        params,
                        return_type: None,
                        effects: vec![],
                        body: Block {
                            stmts: body_stmts,
                            span: self.current_span(),
                        },
                        visibility: Visibility::Private,
                        attributes: vec![],
                        span: self.current_span(),
                    };

                    match field_name.as_str() {
                        "on_execute" => on_execute = Some(func_def),
                        "on_add" => on_add = Some(func_def),
                        "on_remove" => on_remove = Some(func_def),
                        "while_active" => while_active = Some(func_def),
                        _ => {}
                    }
                }
                _ => {
                    return Err(self.parser_error(
                        format!("Unknown gameplay cue field: {}", field_name),
                        self.current_span(),
                    ));
                }
            }

            self.skip_newlines();
        }

        self.expect(TokenKind::Dedent)?;

        // Validate required fields
        let tag =
            tag.ok_or_else(|| self.parser_error("Gameplay cue must have 'tag' field", start))?;

        Ok(Item::GameplayCue(GameplayCueDef {
            name,
            attributes,
            tag,
            cue_type,
            auto_destroy,
            state_fields,
            on_execute,
            on_add,
            on_remove,
            while_active,
            span: start.merge(self.current_span()),
        }))
    }

    /// Parse ability task definition
    /// Syntax:
    /// ```kain
    /// @ability_task
    /// struct WaitTargetData:
    ///     @delegate
    ///     on_data_ready: TargetDataDelegate
    ///     
    ///     @delegate
    ///     on_cancelled: TaskCancelledDelegate
    ///     
    ///     state confirmation_type: String
    ///     state max_range: Float
    ///     
    ///     fn activate():
    ///         # Task activation logic
    ///         register_callbacks()
    ///     
    ///     fn on_destroy():
    ///         # Cleanup logic
    ///         unregister_callbacks()
    /// ```
    fn parse_ability_task(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;

        // Parse task name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse task body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        // Initialize fields
        let mut delegates: Vec<TaskDelegateDef> = Vec::new();
        let mut state_fields: Vec<Field> = Vec::new();
        let mut activate_method: Option<Function> = None;
        let mut on_destroy_method: Option<Function> = None;
        let mut custom_methods: Vec<Function> = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for @delegate attribute
            if self.check(TokenKind::At) {
                let attr_start = self.current_span();
                self.advance();
                if let TokenKind::Ident(attr_name) = self.peek_kind() {
                    if attr_name == "delegate" {
                        self.advance();
                        self.skip_newlines();

                        // Parse delegate: name: Type
                        let delegate_name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;
                        let delegate_type = self.parse_ident()?;

                        delegates.push(TaskDelegateDef {
                            name: delegate_name,
                            delegate_type,
                            span: attr_start.merge(self.current_span()),
                        });

                        self.skip_newlines();
                        continue;
                    }
                }
            }

            // Check for 'state' keyword
            if let TokenKind::Ident(keyword) = self.peek_kind() {
                if keyword == "state" {
                    self.advance();

                    // Parse state field: state field_name: Type = default
                    let field_name = self.parse_ident()?;
                    self.expect(TokenKind::Colon)?;
                    let field_type = self.parse_type()?;

                    let default = if self.check(TokenKind::Eq) {
                        self.advance();
                        Some(self.parse_expr()?)
                    } else {
                        None
                    };

                    state_fields.push(Field {
                        name: field_name,
                        ty: field_type,
                        attributes: vec![],
                        visibility: Visibility::Private,
                        default,
                        weak: false,
                        span: self.current_span(),
                    });

                    self.skip_newlines();
                    continue;
                }
            }

            // Check for 'fn' keyword (methods)
            if self.check(TokenKind::Fn) {
                self.advance(); // consume 'fn'

                // Parse method name
                let method_name = self.parse_ident()?;

                // Parse parameters (optional)
                let params = if self.check(TokenKind::LParen) {
                    self.advance();
                    let mut params = Vec::new();
                    while !self.check(TokenKind::RParen) && !self.at_end() {
                        let param_name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;
                        let param_type = self.parse_type()?;
                        params.push(Param {
                            name: param_name,
                            ty: param_type,
                            mutable: false,
                            default: None,
                            span: self.current_span(),
                        });
                        if self.check(TokenKind::Comma) {
                            self.advance();
                        }
                    }
                    self.expect(TokenKind::RParen)?;
                    params
                } else {
                    vec![]
                };

                // Expect ':'
                self.expect(TokenKind::Colon)?;

                // Parse method body (indented block)
                self.skip_newlines();
                self.expect(TokenKind::Indent)?;

                let mut body_stmts = Vec::new();
                while !self.check(TokenKind::Dedent) && !self.at_end() {
                    self.skip_newlines();
                    if self.check(TokenKind::Dedent) {
                        break;
                    }
                    body_stmts.push(self.parse_stmt()?);
                }

                self.expect(TokenKind::Dedent)?;

                // Create function def
                let func_def = Function {
                    name: method_name.clone(),
                    generics: vec![],
                    params,
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: body_stmts,
                        span: self.current_span(),
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span: self.current_span(),
                };

                match method_name.as_str() {
                    "activate" => activate_method = Some(func_def),
                    "on_destroy" => on_destroy_method = Some(func_def),
                    _ => custom_methods.push(func_def),
                }

                self.skip_newlines();
                continue;
            }

            // Unknown token
            return Err(self.parser_error(
                format!(
                    "Unexpected token in ability task: {}",
                    crate::error::token_kind_to_user_string(&self.peek_kind())
                ),
                self.current_span(),
            ));
        }

        self.expect(TokenKind::Dedent)?;

        Ok(Item::AbilityTask(AbilityTaskDef {
            name,
            attributes,
            delegates,
            state_fields,
            activate_method,
            on_destroy_method,
            custom_methods,
            span: start.merge(self.current_span()),
        }))
    }

    /// Parse target actor definition
    /// Syntax:
    /// ```kain
    /// @target_actor
    /// struct LineTraceTarget:
    ///     trace_type: "Line"
    ///     max_range: 1000.0
    ///     trace_channel: "Visibility"
    ///     
    ///     filter:
    ///         self_filter: "Exclude"
    ///         required_actor_class: "ACharacter"
    ///         require_tags: ["Status.Alive"]
    ///         ignore_tags: ["Status.Dead"]
    ///     
    ///     reticle_class: "BP_LineTraceReticle"
    /// ```
    fn parse_target_actor(&mut self, attributes: Vec<Attribute>) -> KainResult<Item> {
        let start = self.current_span();

        // Expect 'struct' keyword
        self.expect(TokenKind::Struct)?;

        // Parse target actor name
        let name = self.parse_ident()?;

        // Expect ':'
        self.expect(TokenKind::Colon)?;

        // Parse target actor body (indented block)
        self.skip_newlines();
        self.expect(TokenKind::Indent)?;

        // Initialize fields
        let mut trace_type = TraceType::default();
        let mut max_range: Option<f64> = None;
        let mut trace_channel: Option<String> = None;
        let mut filter: Option<TargetFilter> = None;
        let mut reticle_class: Option<String> = None;
        let mut custom_methods: Vec<Function> = Vec::new();

        while !self.check(TokenKind::Dedent) && !self.at_end() {
            self.skip_newlines();
            if self.check(TokenKind::Dedent) {
                break;
            }

            // Check for field name
            if let TokenKind::Ident(field_name) = self.peek_kind() {
                self.advance();
                self.expect(TokenKind::Colon)?;

                match field_name.as_str() {
                    "trace_type" => {
                        // Parse trace type string
                        if let TokenKind::String(type_str) = self.peek_kind() {
                            trace_type = match type_str.as_str() {
                                "Line" => TraceType::Line,
                                "Sphere" => TraceType::Sphere,
                                "Cone" => TraceType::Cone,
                                "Box" => TraceType::Box,
                                "Cylinder" => TraceType::Cylinder,
                                _ => return Err(self.parser_error(
                                    format!("Invalid trace type '{}'. Valid: Line, Sphere, Cone, Box, Cylinder", type_str),
                                    self.current_span()
                                )),
                            };
                            self.advance();
                        } else {
                            return Err(self.parser_error(
                                "Expected string for trace_type",
                                self.current_span(),
                            ));
                        }
                    }
                    "max_range" => {
                        // Parse float
                        if let TokenKind::Float(val) = self.peek_kind() {
                            max_range = Some(val);
                            self.advance();
                        } else if let TokenKind::Int(val) = self.peek_kind() {
                            max_range = Some(val as f64);
                            self.advance();
                        } else {
                            return Err(self.parser_error(
                                "Expected number for max_range",
                                self.current_span(),
                            ));
                        }
                    }
                    "trace_channel" => {
                        // Parse string
                        if let TokenKind::String(channel) = self.peek_kind() {
                            trace_channel = Some(channel);
                            self.advance();
                        } else {
                            return Err(self.parser_error(
                                "Expected string for trace_channel",
                                self.current_span(),
                            ));
                        }
                    }
                    "reticle_class" => {
                        // Parse string
                        if let TokenKind::String(class) = self.peek_kind() {
                            reticle_class = Some(class);
                            self.advance();
                        } else {
                            return Err(self.parser_error(
                                "Expected string for reticle_class",
                                self.current_span(),
                            ));
                        }
                    }
                    "filter" => {
                        // Parse filter block (indented)
                        self.skip_newlines();
                        self.expect(TokenKind::Indent)?;

                        let mut self_filter: Option<String> = None;
                        let mut required_actor_class: Option<String> = None;
                        let mut require_tags: Vec<String> = Vec::new();
                        let mut ignore_tags: Vec<String> = Vec::new();
                        let mut custom_filter_method: Option<Function> = None;

                        while !self.check(TokenKind::Dedent) && !self.at_end() {
                            self.skip_newlines();
                            if self.check(TokenKind::Dedent) {
                                break;
                            }

                            if let TokenKind::Ident(filter_field) = self.peek_kind() {
                                self.advance();
                                self.expect(TokenKind::Colon)?;

                                match filter_field.as_str() {
                                    "self_filter" => {
                                        if let TokenKind::String(val) = self.peek_kind() {
                                            self_filter = Some(val);
                                            self.advance();
                                        }
                                    }
                                    "required_actor_class" => {
                                        if let TokenKind::String(val) = self.peek_kind() {
                                            required_actor_class = Some(val);
                                            self.advance();
                                        }
                                    }
                                    "require_tags" => {
                                        // Parse array of strings
                                        require_tags = self.parse_string_array()?;
                                    }
                                    "ignore_tags" => {
                                        // Parse array of strings
                                        ignore_tags = self.parse_string_array()?;
                                    }
                                    _ => {
                                        return Err(self.parser_error(
                                            format!("Unknown filter field: {}", filter_field),
                                            self.current_span(),
                                        ));
                                    }
                                }
                            } else if self.check(TokenKind::Fn) {
                                // Custom filter method
                                self.advance();
                                let method_name = self.parse_ident()?;

                                // Parse parameters
                                let params = if self.check(TokenKind::LParen) {
                                    self.advance();
                                    let mut params = Vec::new();
                                    while !self.check(TokenKind::RParen) && !self.at_end() {
                                        let param_name = self.parse_ident()?;
                                        self.expect(TokenKind::Colon)?;
                                        let param_type = self.parse_type()?;
                                        params.push(Param {
                                            name: param_name,
                                            ty: param_type,
                                            mutable: false,
                                            default: None,
                                            span: self.current_span(),
                                        });
                                        if self.check(TokenKind::Comma) {
                                            self.advance();
                                        }
                                    }
                                    self.expect(TokenKind::RParen)?;
                                    params
                                } else {
                                    vec![]
                                };

                                // Expect ':'
                                self.expect(TokenKind::Colon)?;

                                // Parse method body
                                self.skip_newlines();
                                self.expect(TokenKind::Indent)?;

                                let mut body_stmts = Vec::new();
                                while !self.check(TokenKind::Dedent) && !self.at_end() {
                                    self.skip_newlines();
                                    if self.check(TokenKind::Dedent) {
                                        break;
                                    }
                                    body_stmts.push(self.parse_stmt()?);
                                }

                                self.expect(TokenKind::Dedent)?;

                                custom_filter_method = Some(Function {
                                    name: method_name,
                                    generics: vec![],
                                    params,
                                    return_type: None,
                                    effects: vec![],
                                    body: Block {
                                        stmts: body_stmts,
                                        span: self.current_span(),
                                    },
                                    visibility: Visibility::Private,
                                    attributes: vec![],
                                    span: self.current_span(),
                                });
                            }

                            self.skip_newlines();
                        }

                        self.expect(TokenKind::Dedent)?;

                        filter = Some(TargetFilter {
                            self_filter,
                            required_actor_class,
                            require_tags,
                            ignore_tags,
                            custom_filter_method,
                            span: self.current_span(),
                        });
                    }
                    _ => {
                        return Err(self.parser_error(
                            format!("Unknown target actor field: {}", field_name),
                            self.current_span(),
                        ));
                    }
                }
            } else if self.check(TokenKind::Fn) {
                // Custom method
                self.advance();
                let method_name = self.parse_ident()?;

                // Parse parameters
                let params = if self.check(TokenKind::LParen) {
                    self.advance();
                    let mut params = Vec::new();
                    while !self.check(TokenKind::RParen) && !self.at_end() {
                        let param_name = self.parse_ident()?;
                        self.expect(TokenKind::Colon)?;
                        let param_type = self.parse_type()?;
                        params.push(Param {
                            name: param_name,
                            ty: param_type,
                            mutable: false,
                            default: None,
                            span: self.current_span(),
                        });
                        if self.check(TokenKind::Comma) {
                            self.advance();
                        }
                    }
                    self.expect(TokenKind::RParen)?;
                    params
                } else {
                    vec![]
                };

                // Expect ':'
                self.expect(TokenKind::Colon)?;

                // Parse method body
                self.skip_newlines();
                self.expect(TokenKind::Indent)?;

                let mut body_stmts = Vec::new();
                while !self.check(TokenKind::Dedent) && !self.at_end() {
                    self.skip_newlines();
                    if self.check(TokenKind::Dedent) {
                        break;
                    }
                    body_stmts.push(self.parse_stmt()?);
                }

                self.expect(TokenKind::Dedent)?;

                custom_methods.push(Function {
                    name: method_name,
                    generics: vec![],
                    params,
                    return_type: None,
                    effects: vec![],
                    body: Block {
                        stmts: body_stmts,
                        span: self.current_span(),
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span: self.current_span(),
                });
            } else {
                return Err(self.parser_error("Expected field name or fn", self.current_span()));
            }

            self.skip_newlines();
        }

        self.expect(TokenKind::Dedent)?;

        Ok(Item::TargetActor(TargetActorDef {
            name,
            attributes,
            trace_type,
            max_range,
            trace_channel,
            filter,
            reticle_class,
            custom_methods,
            span: start.merge(self.current_span()),
        }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::diagnostics::SpanMapper;
    use crate::lexer::Lexer;

    fn parse_program(source: &str) -> KainResult<Program> {
        let tokens = Lexer::new(source).tokenize()?;
        let span_mapper = SpanMapper::new(source);
        Parser::new(&tokens, &span_mapper, "<test>").parse()
    }

    #[test]
    fn parses_component_call_and_fragment_in_component_render() {
        let program = parse_program(
            "component App():\n    render <Fragment><Panel title={title} /></Fragment>\n",
        )
        .expect("program should parse");

        let Item::Component(component) = &program.items[0] else {
            panic!("expected component");
        };

        match &component.body {
            JSXNode::Fragment(children, _) => {
                assert_eq!(children.len(), 1);
                match &children[0] {
                    JSXNode::ComponentCall { name, props, .. } => {
                        assert_eq!(name, "Panel");
                        assert_eq!(props.len(), 1);
                    }
                    other => panic!("expected component call, got {other:?}"),
                }
            }
            other => panic!("expected fragment body, got {other:?}"),
        }
    }

    #[test]
    fn parses_jsx_if_and_for_children() {
        let program = parse_program(
            "component App(items: List<Int>, ready: Bool):\n    render <div>{if ready: <Spinner /> else: <Empty />}{for item in items: <Row value={item} />}</div>\n",
        )
        .expect("program should parse");

        let Item::Component(component) = &program.items[0] else {
            panic!("expected component");
        };

        let JSXNode::Element { children, .. } = &component.body else {
            panic!("expected element body");
        };

        assert_eq!(children.len(), 2);
        match &children[0] {
            JSXNode::If {
                then_branch,
                else_branch,
                ..
            } => {
                assert!(
                    matches!(then_branch.as_ref(), JSXNode::ComponentCall { name, .. } if name == "Spinner")
                );
                assert!(
                    matches!(else_branch.as_deref(), Some(JSXNode::ComponentCall { name, .. }) if name == "Empty")
                );
            }
            other => panic!("expected if child, got {other:?}"),
        }

        match &children[1] {
            JSXNode::For { binding, body, .. } => {
                assert_eq!(binding, "item");
                assert!(
                    matches!(body.as_ref(), JSXNode::ComponentCall { name, .. } if name == "Row")
                );
            }
            other => panic!("expected for child, got {other:?}"),
        }
    }

    #[test]
    fn parses_jsx_attributes_named_like_keywords() {
        let program = parse_program(
            "component App():\n    render <button type=\"button\" className={none}>\"ok\"</button>\n",
        )
        .expect("program should parse");

        let Item::Component(component) = &program.items[0] else {
            panic!("expected component");
        };

        let JSXNode::Element { attributes, .. } = &component.body else {
            panic!("expected element body");
        };

        assert_eq!(attributes.len(), 2);
        assert_eq!(attributes[0].name, "type");
        assert_eq!(attributes[1].name, "className");
    }

    #[test]
    fn parses_component_valued_jsx_attributes() {
        let program = parse_program(
            "component App():\n    render <Shell topBar={<TopBar title={title} />} bottom={<Footer />} />\n",
        )
        .expect("program should parse");

        let Item::Component(component) = &program.items[0] else {
            panic!("expected component");
        };

        let JSXNode::ComponentCall { props, .. } = &component.body else {
            panic!("expected component call body");
        };

        assert_eq!(props.len(), 2);

        match &props[0].value {
            JSXAttrValue::Expr(Expr::JSX(JSXNode::ComponentCall { name, .. }, _)) => {
                assert_eq!(name, "TopBar");
            }
            other => panic!("expected nested JSX component prop, got {other:?}"),
        }

        match &props[1].value {
            JSXAttrValue::Expr(Expr::JSX(JSXNode::ComponentCall { name, .. }, _)) => {
                assert_eq!(name, "Footer");
            }
            other => panic!("expected nested JSX component prop, got {other:?}"),
        }
    }

    #[test]
    fn parses_match_arm_guards() {
        let program = parse_program(
            "fn inspect(value: Int):\n    match value:\n        n if n > 0 => true\n        _ => false\n",
        )
        .expect("program should parse guarded match arms");

        let Item::Function(function) = &program.items[0] else {
            panic!("expected function");
        };
        let Stmt::Expr(Expr::Match { arms, .. }) = &function.body.stmts[0] else {
            panic!("expected match expression");
        };
        assert_eq!(arms.len(), 2);
        assert!(arms[0].guard.is_some());
        assert!(arms[1].guard.is_none());
    }

    #[test]
    fn parses_multiline_match_arm_trailing_unit_as_separate_statement() {
        let program = parse_program(
            "enum Item:\n    Function(Function)\n\nstruct Function:\n    name: String\n\nstruct Filter:\n    custom_filter_method: Option<Function>\n\nfn collect_type_names_from_item(item: &Item, out_: &mut Set<String>):\n    ()\n\nfn demo(filter: Option<Filter>, out_: &mut Set<String>):\n    match &filter:\n        Some(filter) =>\n            match &filter.custom_filter_method:\n                Some(custom_filter) =>\n                    collect_type_names_from_item(&Item::Function(custom_filter.clone()), out_)\n                    ()\n                _ => ()\n            ()\n        _ => ()\n",
        )
        .expect("program should parse nested multiline match arms");

        let Item::Function(function) = &program.items[4] else {
            panic!("expected demo function");
        };
        let Stmt::Expr(Expr::Match { arms, .. }) = &function.body.stmts[0] else {
            panic!("expected outer match");
        };
        let Expr::Block(outer_some_body, _) = &arms[0].body else {
            panic!("expected outer Some arm to stay block-shaped");
        };
        let Stmt::Expr(Expr::Match {
            arms: inner_arms, ..
        }) = &outer_some_body.stmts[0]
        else {
            panic!("expected nested match statement");
        };
        let Expr::Block(inner_some_body, _) = &inner_arms[0].body else {
            panic!("expected inner Some arm to stay block-shaped");
        };
        assert!(matches!(
            inner_some_body.stmts.get(0),
            Some(Stmt::Expr(Expr::Call { .. }))
        ));
        assert!(matches!(
            inner_some_body.stmts.get(1),
            Some(Stmt::Expr(Expr::Tuple(items, _))) if items.is_empty()
        ));
        assert!(matches!(
            outer_some_body.stmts.get(1),
            Some(Stmt::Expr(Expr::Tuple(items, _))) if items.is_empty()
        ));
    }

    #[test]
    fn parses_general_range_expressions_in_for_and_fanout_iterators() {
        let program = parse_program(
            "fn demo():\n    for lane in 0..4:\n        ()\n    fanout worker in 0..=7:\n        ()\n",
        )
        .expect("program should parse general range iterators");

        let Item::Function(function) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::For { iter: for_iter, .. } = &function.body.stmts[0] else {
            panic!("expected for statement");
        };
        let Expr::Range {
            start: Some(for_start),
            end: Some(for_end),
            inclusive: false,
            ..
        } = for_iter
        else {
            panic!("expected exclusive range iterator");
        };
        assert!(matches!(for_start.as_ref(), Expr::Int(0, _)));
        assert!(matches!(for_end.as_ref(), Expr::Int(4, _)));

        let Stmt::Fanout {
            iter: fanout_iter, ..
        } = &function.body.stmts[1]
        else {
            panic!("expected fanout statement");
        };
        let Expr::Range {
            start: Some(fanout_start),
            end: Some(fanout_end),
            inclusive: true,
            ..
        } = fanout_iter
        else {
            panic!("expected inclusive range iterator");
        };
        assert!(matches!(fanout_start.as_ref(), Expr::Int(0, _)));
        assert!(matches!(fanout_end.as_ref(), Expr::Int(7, _)));
    }

    #[test]
    fn compare_exchange_preserves_explicit_failure_ordering_for_later_validation() {
        let program = parse_program(
            "fn demo(slot: ptr<Int>) -> Bool with Unsafe:\n    return atomic_compare_exchange(slot, 0, 1, \"Int\", \"release\", \"release\")\n",
        )
        .expect("program should parse compare_exchange ordering literals");

        let Item::Function(function) = &program.items[0] else {
            panic!("expected function");
        };
        let Stmt::Return(
            Some(Expr::AtomicCompareExchange {
                success_ordering,
                failure_ordering,
                ..
            }),
            _,
        ) = &function.body.stmts[0]
        else {
            panic!("expected compare_exchange return");
        };

        assert_eq!(*success_ordering, AtomicOrdering::Release);
        assert_eq!(*failure_ordering, AtomicOrdering::Release);
    }

    #[test]
    fn compare_exchange_default_failure_ordering_still_tracks_success_ordering() {
        let program = parse_program(
            "fn demo(slot: ptr<Int>) -> Bool with Unsafe:\n    return atomic_compare_exchange(slot, 0, 1, \"Int\", \"release\")\n",
        )
        .expect("program should parse compare_exchange default failure ordering");

        let Item::Function(function) = &program.items[0] else {
            panic!("expected function");
        };
        let Stmt::Return(
            Some(Expr::AtomicCompareExchange {
                success_ordering,
                failure_ordering,
                ..
            }),
            _,
        ) = &function.body.stmts[0]
        else {
            panic!("expected compare_exchange return");
        };

        assert_eq!(*success_ordering, AtomicOrdering::Release);
        assert_eq!(*failure_ordering, AtomicOrdering::Relaxed);
    }

    #[test]
    fn concatenated_stdlib_files_keep_following_top_level_items_visible() {
        let source = format!(
            "{}\n\n{}",
            include_str!("../../../stdlib/path.kn"),
            include_str!("../../../stdlib/ascii.kn")
        );
        let program = parse_program(&source).expect("concatenated stdlib files should parse");

        let top_level_names: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function) => Some(format!("fn:{}", function.name)),
                Item::Const(constant) => Some(format!("const:{}", constant.name)),
                _ => None,
            })
            .collect();

        assert!(
            top_level_names.iter().any(|name| name == "fn:ascii_is_byte"),
            "ascii follow-on function missing from top level: {top_level_names:?}"
        );
        assert!(
            top_level_names.iter().any(|name| name == "const:ASCII_NUL"),
            "ascii follow-on const missing from top level: {top_level_names:?}"
        );
    }

    #[test]
    fn ambient_frontend_bundle_prefix_keeps_ascii_items_visible() {
        let source = format!(
            "{}\n\n{}\n\n{}\n\n{}\n\n{}\n\n{}",
            include_str!("../../../stdlib/runtime.kn"),
            include_str!("../../../stdlib/actor.kn"),
            include_str!("../../../stdlib/platform.kn"),
            include_str!("../../../stdlib/target.kn"),
            include_str!("../../../stdlib/path.kn"),
            include_str!("../../../stdlib/ascii.kn")
        );
        let program = parse_program(&source).expect("ambient stdlib prefix should parse");

        let top_level_names: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function) => Some(format!("fn:{}", function.name)),
                Item::Const(constant) => Some(format!("const:{}", constant.name)),
                _ => None,
            })
            .collect();

        assert!(
            top_level_names.iter().any(|name| name == "fn:ascii_is_byte"),
            "ascii function disappeared after ambient prefix: {top_level_names:?}"
        );
        assert!(
            top_level_names.iter().any(|name| name == "const:ASCII_NUL"),
            "ascii const disappeared after ambient prefix: {top_level_names:?}"
        );
    }

    #[test]
    fn target_stdlib_file_leaves_following_function_at_top_level_boundary() {
        let source = include_str!("../../../stdlib/target.kn");
        let tokens = Lexer::new(source).tokenize().expect("target tokens");
        let span_mapper = SpanMapper::new(source);
        let mut parser = Parser::new(&tokens, &span_mapper, "<target-stdlib>");

        let mut parsed_names = Vec::new();
        while !parser.at_end() {
            parser.skip_formatting();
            if parser.at_end() {
                break;
            }
            let item = parser.parse_item().expect("target item should parse");
            match &item {
                Item::Use(import) => parsed_names.push(format!("use:{}", import.path.join("::"))),
                Item::Enum(def) => parsed_names.push(format!("enum:{}", def.name)),
                Item::Struct(def) => parsed_names.push(format!("struct:{}", def.name)),
                Item::Function(def) => parsed_names.push(format!("fn:{}", def.name)),
                other => parsed_names.push(format!("{other:?}")),
            }
            parser.skip_formatting();
        }

        assert_eq!(
            parsed_names,
            vec![
                "use:std::runtime",
                "enum:Arch",
                "enum:OS",
                "enum:Env",
                "struct:Target",
                "fn:target_current",
                "fn:target_has_feature",
            ],
            "target stdlib parse lost the trailing function: {parsed_names:?}"
        );
    }
}
