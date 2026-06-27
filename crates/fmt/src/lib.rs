//! Compiler-owned source formatter for Kain.
//!
//! The formatter intentionally canonicalizes authored source through the same
//! frontend AST the rest of the compiler uses. That gives the language a single
//! printer of record for humans, tooling, and LLM-oriented workflows.

use kain_core::ast::*;
use kain_core::diagnostics::SpanMapper;
use kain_core::effects::Effect;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_error::{KainError, KainResult};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FormatOptions {
    pub indent_width: usize,
    pub max_width: usize,
}

impl Default for FormatOptions {
    fn default() -> Self {
        Self {
            indent_width: 4,
            max_width: 100,
        }
    }
}

pub fn format_source(source: &str) -> KainResult<String> {
    format_source_with_options(source, FormatOptions::default())
}

pub fn format_program(program: &Program) -> KainResult<String> {
    format_program_with_options(program, FormatOptions::default())
}

pub fn format_program_with_options(
    program: &Program,
    options: FormatOptions,
) -> KainResult<String> {
    let formatter = SourceFormatter::new(options);
    formatter.format_program(program)
}

pub fn format_source_with_options(source: &str, options: FormatOptions) -> KainResult<String> {
    let prologue = SourcePrologue::extract(source);
    let body = prologue.body();
    if body.trim().is_empty() {
        return Ok(prologue.reassemble(""));
    }

    let tokens = Lexer::new(body).tokenize()?;
    let span_mapper = SpanMapper::new(body);
    let program = Parser::new(&tokens, &span_mapper, "<formatter>").parse()?;
    let formatted_body = format_program_with_options(&program, options)?;
    Ok(prologue.reassemble(&formatted_body))
}

struct SourcePrologue<'a> {
    has_bom: bool,
    shebang: Option<&'a str>,
    body: &'a str,
}

impl<'a> SourcePrologue<'a> {
    fn extract(source: &'a str) -> Self {
        let (has_bom, remainder) = if let Some(rest) = source.strip_prefix('\u{feff}') {
            (true, rest)
        } else {
            (false, source)
        };

        if let Some(rest) = remainder.strip_prefix("#!") {
            if let Some(newline_index) = rest.find('\n') {
                let shebang_len = 2 + newline_index;
                Self {
                    has_bom,
                    shebang: Some(&remainder[..shebang_len]),
                    body: &rest[(newline_index + 1)..],
                }
            } else {
                Self {
                    has_bom,
                    shebang: Some(remainder),
                    body: "",
                }
            }
        } else {
            Self {
                has_bom,
                shebang: None,
                body: remainder,
            }
        }
    }

    fn body(&self) -> &'a str {
        self.body
    }

    fn reassemble(&self, body: &str) -> String {
        let mut output = String::new();
        if self.has_bom {
            output.push('\u{feff}');
        }
        if let Some(shebang) = self.shebang {
            output.push_str(shebang);
            if !body.is_empty() {
                output.push('\n');
            }
        }
        output.push_str(body);
        if !body.is_empty() && !body.ends_with('\n') {
            output.push('\n');
        }
        output
    }
}

struct SourceFormatter {
    options: FormatOptions,
}

impl SourceFormatter {
    fn new(options: FormatOptions) -> Self {
        Self { options }
    }

    fn format_program(&self, program: &Program) -> KainResult<String> {
        let mut sections = Vec::new();
        let mut items = program.items.iter().collect::<Vec<_>>();

        let synthetic_main = if items
            .last()
            .is_some_and(|item| self.is_synthetic_script_main(program, item))
        {
            items.pop()
        } else {
            None
        };

        for item in items {
            sections.push(self.format_item(item)?);
        }

        if let Some(Item::Function(function)) = synthetic_main {
            if !function.body.stmts.is_empty() {
                sections.push(self.format_statement_sequence(&function.body.stmts)?);
            }
        }

        Ok(sections.join("\n\n"))
    }

    fn is_synthetic_script_main(&self, program: &Program, item: &Item) -> bool {
        let Item::Function(function) = item else {
            return false;
        };
        function.name == "main"
            && function.generics.is_empty()
            && function.params.is_empty()
            && function.return_type.is_none()
            && function.effects.is_empty()
            && function.attributes.is_empty()
            && function.visibility == Visibility::Public
            && function.span == program.span
            && function.body.span == program.span
    }

    fn format_item(&self, item: &Item) -> KainResult<String> {
        let mut output = String::new();
        match item {
            Item::Function(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_function(value)?);
            }
            Item::Patch(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_patch(value)?);
            }
            Item::Law(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_law(value)?);
            }
            Item::Axiom(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_axiom(value)?);
            }
            Item::Converge(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_converge(value)?);
            }
            Item::World(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_world(value)?);
            }
            Item::Entangle(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_entangle(value)?);
            }
            Item::Orchestrate(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_orchestrate(value)?);
            }
            Item::Pulse(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_pulse(value)?);
            }
            Item::Resonate(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_resonate(value)?);
            }
            Item::Component(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_component(value)?);
            }
            Item::Shader(value) => {
                self.push_text(&mut output, &self.format_shader(value)?);
            }
            Item::Actor(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_actor(value)?);
            }
            Item::Struct(value) => {
                if value.is_shattered() {
                    let attrs = value
                        .attributes
                        .iter()
                        .filter(|attribute| attribute.name != SHATTER_ATTRIBUTE_NAME)
                        .cloned()
                        .collect::<Vec<_>>();
                    self.push_attributes(&mut output, &attrs)?;
                } else {
                    self.push_attributes(&mut output, &value.attributes)?;
                }
                self.push_text(&mut output, &self.format_struct(value)?);
            }
            Item::Enum(value) => {
                self.push_text(&mut output, &self.format_enum(value)?);
            }
            Item::Trait(value) => {
                self.push_text(&mut output, &self.format_trait(value)?);
            }
            Item::Impl(value) => {
                self.push_text(&mut output, &self.format_impl(value)?);
            }
            Item::TypeAlias(value) => {
                self.push_text(&mut output, &self.format_type_alias(value));
            }
            Item::Use(value) => {
                self.push_text(&mut output, &self.format_use(value));
            }
            Item::Import(value) => {
                self.push_text(&mut output, &self.format_import(value));
            }
            Item::Mod(value) => {
                self.push_text(&mut output, &self.format_mod(value)?);
            }
            Item::Const(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_const(value)?);
            }
            Item::Comptime(value) => {
                self.push_text(&mut output, &self.format_comptime_block(value)?);
            }
            Item::Macro(value) => {
                self.push_text(&mut output, &self.format_macro(value)?);
            }
            Item::Test(value) => {
                self.push_text(&mut output, &self.format_test(value)?);
            }
            Item::MaterialGraph(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_material_graph(value)?);
            }
            Item::MaterialFunction(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_material_function(value)?);
            }
            Item::GraphEditor(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_graph_editor(value)?);
            }
            Item::GraphRuntime(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_graph_runtime(value)?);
            }
            Item::StateMachine(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_state_machine(value)?);
            }
            Item::AsyncTask(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_async_task(value)?);
            }
            Item::EditorModule(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_editor_module(value)?);
            }
            Item::GameplayTags(value) => {
                self.push_text(&mut output, &self.format_gameplay_tags(value)?);
            }
            Item::GameplayAbility(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_gameplay_ability(value)?);
            }
            Item::GameplayEffect(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_gameplay_effect(value)?);
            }
            Item::GameplayCue(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_gameplay_cue(value)?);
            }
            Item::AbilityTask(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_ability_task(value)?);
            }
            Item::TargetActor(value) => {
                self.push_attributes(&mut output, &value.attributes)?;
                self.push_text(&mut output, &self.format_target_actor(value)?);
            }
        }
        Ok(output)
    }

    fn push_attributes(&self, output: &mut String, attrs: &[Attribute]) -> KainResult<()> {
        if attrs.is_empty() {
            return Ok(());
        }
        self.push_text(
            output,
            &attrs
                .iter()
                .map(|attr| self.format_attribute(attr))
                .collect::<KainResult<Vec<_>>>()?
                .join("\n"),
        );
        output.push('\n');
        Ok(())
    }

    fn push_text(&self, output: &mut String, text: &str) {
        output.push_str(text);
    }

    fn format_function(&self, value: &Function) -> KainResult<String> {
        let signature = self.function_signature(
            "fn",
            value.visibility,
            &value.name,
            &value.generics,
            &value.params,
            value.return_type.as_ref(),
            value.where_clause.as_ref(),
            &value.effects,
        )?;
        if self.is_extern_function(value) {
            return Ok(signature);
        }
        self.format_header_with_block(&signature, &value.body)
    }

    fn format_patch(&self, value: &PatchDef) -> KainResult<String> {
        let signature = self.callable_signature(
            "patch",
            value.visibility,
            &value.name,
            &[],
            &value.params,
            value.return_type.as_ref(),
            &[],
        )?;
        self.format_header_with_block(&signature, &value.body)
    }

    fn format_law(&self, value: &LawDef) -> KainResult<String> {
        let signature = self.callable_signature(
            "law",
            value.visibility,
            &value.name,
            &[],
            &value.params,
            Some(&value.return_type),
            &[],
        )?;
        self.format_header_with_block(&signature, &value.body)
    }

    fn format_axiom(&self, value: &AxiomDef) -> KainResult<String> {
        let header = format!(
            "{}axiom {}",
            self.visibility_prefix(value.visibility),
            value.name
        );
        let mut entries = Vec::new();
        for predicate in &value.predicates {
            entries.push(format!(
                "when {}({})",
                predicate.kind(),
                self.format_string_like_selector(predicate.value())
            ));
        }
        for guarantee in &value.guarantees {
            entries.push(format!("guarantee {}", self.quote_string(guarantee)));
        }
        if let Some(fallback) = &value.fallback {
            entries.push(format!(
                "fallback {}",
                self.format_identifier_or_string(fallback)
            ));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_pulse(&self, value: &PulseDef) -> KainResult<String> {
        let mut header = format!(
            "{}pulse {} every {}",
            self.visibility_prefix(value.visibility),
            value.name,
            value.interval.as_authored()
        );
        if let Some(jitter) = &value.jitter {
            header.push_str(&format!(" jitter {}", jitter.as_authored()));
        }
        self.format_header_with_block(&header, &value.body)
    }

    fn format_resonate(&self, value: &ResonateDef) -> KainResult<String> {
        let mut header = format!(
            "{}resonate {}",
            self.visibility_prefix(value.visibility),
            value.target.authored_path()
        );
        if let Some(dampen) = &value.dampen {
            header.push_str(&format!(" dampen {}", dampen.as_authored()));
        }
        self.format_header_with_block(&header, &value.body)
    }

    fn format_orchestrate(&self, value: &OrchestrateDef) -> KainResult<String> {
        let signature = self.callable_signature(
            "orchestrate",
            value.visibility,
            &value.name,
            &[],
            &value.params,
            value.return_type.as_ref(),
            &[],
        )?;
        self.format_header_with_block(&signature, &value.body)
    }

    fn format_converge(&self, value: &ConvergeDef) -> KainResult<String> {
        let signature = self.callable_signature(
            "converge",
            value.visibility,
            &value.name,
            &[],
            &value.params,
            value.return_type.as_ref(),
            &[],
        )?;
        let mut body_sections = Vec::new();
        body_sections.push(self.format_converge_lane(&value.spec_lane)?);
        for lane in &value.fast_lanes {
            body_sections.push(self.format_converge_lane(lane)?);
        }
        if let Some(count) = value.verify_random_count {
            body_sections.push(format!("verify random({count})"));
        }
        self.format_header_with_body(&signature, &body_sections.join("\n"))
    }

    fn format_converge_lane(&self, lane: &ConvergeLane) -> KainResult<String> {
        let kind = match lane.kind {
            ConvergeLaneKind::Spec => "spec",
            ConvergeLaneKind::Fast => "fast",
        };
        let selector = match &lane.selector {
            Some(ConvergeSelector::Target(value)) => {
                format!(" when target({})", self.format_string_like_selector(value))
            }
            Some(ConvergeSelector::Capability(value)) => {
                format!(
                    " when capability({})",
                    self.format_string_like_selector(value)
                )
            }
            None => String::new(),
        };
        let header = format!("{kind} {}{selector}", lane.lane_name);
        self.format_header_with_block(&header, &lane.body)
    }

    fn format_string_like_selector(&self, value: &str) -> String {
        if value
            .chars()
            .all(|ch| ch.is_ascii_alphanumeric() || ch == '_' || ch == ':' || ch == '.')
        {
            self.quote_string(value)
        } else {
            self.quote_string(value)
        }
    }

    fn format_identifier_or_string(&self, value: &str) -> String {
        if value
            .chars()
            .next()
            .is_some_and(|ch| ch.is_ascii_alphabetic() || ch == '_')
            && value
                .chars()
                .all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
        {
            value.to_string()
        } else {
            self.quote_string(value)
        }
    }

    fn format_world(&self, value: &WorldDef) -> KainResult<String> {
        let header = format!(
            "{}world {}",
            self.visibility_prefix(value.visibility),
            value.name
        );
        let mut entries = Vec::new();
        for state in &value.states {
            entries.push(format!(
                "state {}: {} = {}",
                state.name,
                self.format_type(&state.ty),
                self.format_expr(&state.initial)?
            ));
        }
        for surface in &value.surfaces {
            entries.push(format!(
                "surface {} => {}",
                surface.kind.as_str(),
                self.format_expr(&surface.expr)?
            ));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_entangle(&self, value: &EntangleDef) -> KainResult<String> {
        Ok(format!(
            "{}entangle {} <-> {} with {}",
            self.visibility_prefix(value.visibility),
            value.left.authored_path(),
            value.right.authored_path(),
            value.policy.as_str()
        ))
    }

    fn format_component(&self, value: &Component) -> KainResult<String> {
        let header = self.callable_signature(
            "component",
            value.visibility,
            &value.name,
            &[],
            &value.props,
            None,
            &value.effects,
        )?;
        let mut sections = Vec::new();
        for state in &value.state {
            sections.push(self.format_state_decl(state)?);
        }
        for method in &value.methods {
            let nested = self.indent_text(&self.format_function(method)?, 1);
            sections.push(nested);
        }
        sections.push(format!(
            "render:\n{}",
            self.indent_text(&self.format_jsx_node(&value.body)?, 1)
        ));
        self.format_header_with_body(&header, &sections.join("\n"))
    }

    fn format_shader(&self, value: &Shader) -> KainResult<String> {
        let stage = match value.stage {
            ShaderStage::Vertex => "vertex ",
            ShaderStage::Fragment => "fragment ",
            ShaderStage::Compute => "compute ",
            ShaderStage::Surface => {
                return Err(KainError::runtime(
                    "Kain formatter cannot emit `surface` shader stage because the parser does not currently accept it",
                ))
            }
            ShaderStage::Mesh => "mesh ",
            ShaderStage::Task => "task ",
            ShaderStage::RayGen => "raygen ",
            ShaderStage::AnyHit => "anyhit ",
            ShaderStage::ClosestHit => "closesthit ",
            ShaderStage::Miss => "miss ",
            ShaderStage::Intersection => "intersection ",
            ShaderStage::Callable => "callable ",
        };
        let mut header = self.render_callable_head(
            &format!("shader {stage}{}(", value.name),
            &self.format_param_parts(&value.inputs)?,
            &format!(") -> {}", self.format_type(&value.outputs)),
        );
        if let Some([x, y, z]) = value.workgroup_size {
            header.push_str(&format!(" workgroup({x}, {y}, {z})"));
        }
        let mut body_lines = Vec::new();
        for uniform in &value.uniforms {
            body_lines.push(format!(
                "uniform {}: {} @{}",
                uniform.name,
                self.format_type(&uniform.ty),
                uniform.binding
            ));
        }
        if !value.body.stmts.is_empty() {
            body_lines.push(self.format_statement_sequence(&value.body.stmts)?);
        }
        self.format_header_with_body(&header, &body_lines.join("\n"))
    }

    fn format_actor(&self, value: &Actor) -> KainResult<String> {
        let header = format!("actor {}", value.name);
        let mut sections = Vec::new();
        for state in &value.state {
            sections.push(self.format_state_decl(state)?);
        }
        for handler in &value.handlers {
            let signature = self.render_callable_head(
                &format!("on {}(", handler.message_type),
                &self.format_param_parts(&handler.params)?,
                ")",
            );
            sections.push(self.format_header_with_block(&signature, &handler.body)?);
        }
        for method in &value.methods {
            sections.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &sections.join("\n"))
    }

    fn format_struct(&self, value: &Struct) -> KainResult<String> {
        let keyword = if value.is_shattered() {
            "shatter struct"
        } else {
            "struct"
        };
        let mut header = format!(
            "{}{} {}{}",
            self.visibility_prefix(value.visibility),
            keyword,
            value.name,
            self.format_generics(&value.generics)
        );
        self.push_where_clause(&mut header, value.where_clause.as_ref());
        if value.fields.is_empty() && value.methods.is_empty() {
            return Ok(format!("{header}:"));
        }

        let mut entries = Vec::new();
        for field in &value.fields {
            entries.push(self.format_field(field)?);
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_enum(&self, value: &Enum) -> KainResult<String> {
        if value.variants.is_empty() {
            return Err(KainError::runtime(
                "Kain formatter cannot emit empty enums because the parser requires at least one variant",
            ));
        }
        let mut header = format!(
            "{}enum {}{}",
            self.visibility_prefix(value.visibility),
            value.name,
            self.format_generics(&value.generics)
        );
        self.push_where_clause(&mut header, value.where_clause.as_ref());
        let body = value
            .variants
            .iter()
            .map(|variant| self.format_variant(variant))
            .collect::<KainResult<Vec<_>>>()?
            .join("\n");
        self.format_header_with_body(&header, &body)
    }

    fn format_variant(&self, variant: &Variant) -> KainResult<String> {
        match &variant.fields {
            VariantFields::Unit => Ok(variant.name.clone()),
            VariantFields::Tuple(types) => Ok(format!(
                "{}({})",
                variant.name,
                types
                    .iter()
                    .map(|ty| self.format_type(ty))
                    .collect::<Vec<_>>()
                    .join(", ")
            )),
            VariantFields::Struct(fields) => Ok(format!(
                "{} {{ {} }}",
                variant.name,
                fields
                    .iter()
                    .map(|field| self.format_inline_field(field))
                    .collect::<KainResult<Vec<_>>>()?
                    .join(", ")
            )),
        }
    }

    fn format_trait(&self, value: &Trait) -> KainResult<String> {
        if value.methods.is_empty() {
            return Err(KainError::runtime(
                "Kain formatter cannot emit empty traits because the parser requires at least one method",
            ));
        }
        let mut header = format!(
            "{}trait {}{}",
            self.visibility_prefix(value.visibility),
            value.name,
            self.format_generics(&value.generics)
        );
        self.push_where_clause(&mut header, value.where_clause.as_ref());
        let mut methods = Vec::new();
        for method in &value.methods {
            methods.push(self.format_trait_method(method)?);
        }
        self.format_header_with_body(&header, &methods.join("\n"))
    }

    fn format_trait_method(&self, value: &TraitMethod) -> KainResult<String> {
        let signature = self.render_callable_head(
            &format!("fn {}(", value.name),
            &self.format_param_parts(&value.params)?,
            &self.render_callable_suffix(value.return_type.as_ref(), None, &value.effects),
        );
        if let Some(block) = &value.default_impl {
            self.format_header_with_block(&signature, block)
        } else {
            Ok(signature)
        }
    }

    fn format_impl(&self, value: &Impl) -> KainResult<String> {
        let generics = self.format_generics(&value.generics);
        let mut header = if let Some(trait_name) = &value.trait_name {
            if value.trait_generics.is_empty() {
                format!(
                    "impl{generics} {trait_name} for {}",
                    self.format_type(&value.target_type)
                )
            } else {
                format!(
                    "impl{generics} {}<{}> for {}",
                    trait_name,
                    value
                        .trait_generics
                        .iter()
                        .map(|ty| self.format_type(ty))
                        .collect::<Vec<_>>()
                        .join(", "),
                    self.format_type(&value.target_type)
                )
            }
        } else {
            format!("impl{generics} {}", self.format_type(&value.target_type))
        };
        self.push_where_clause(&mut header, value.where_clause.as_ref());

        if value.methods.is_empty() {
            return Err(KainError::runtime(
                "Kain formatter cannot emit empty impl blocks because the parser requires at least one method",
            ));
        }

        let body = value
            .methods
            .iter()
            .map(|method| Ok(self.indent_text(&self.format_function(method)?, 1)))
            .collect::<KainResult<Vec<_>>>()?
            .join("\n");
        self.format_header_with_body(&header, &body)
    }

    fn format_type_alias(&self, value: &TypeAlias) -> String {
        let mut output = format!(
            "{}type {}{} = {}",
            self.visibility_prefix(value.visibility),
            value.name,
            self.format_generics(&value.generics),
            self.format_type(&value.target)
        );
        if let Some(where_clause) = value.where_clause.as_ref() {
            let needle = " = ";
            if let Some(index) = output.find(needle) {
                output.insert_str(
                    index,
                    &format!(" {}", self.format_where_clause(where_clause)),
                );
            }
        }
        output
    }

    fn format_use(&self, value: &Use) -> String {
        if value.origin == UseOrigin::CInclude {
            let include_segments = if value.path.first().is_some_and(|segment| segment == "c") {
                &value.path[1..]
            } else {
                &value.path[..]
            };
            let include_name = include_segments.join("/");
            let mut output = format!("include {}", include_name);
            if let Some(alias) = &value.alias {
                output.push_str(" as ");
                output.push_str(alias);
            }
            return output;
        }

        let mut output = format!("use {}", value.path.join("::"));
        if value.glob {
            output.push_str("::*");
        }
        if let Some(alias) = &value.alias {
            output.push_str(" as ");
            output.push_str(alias);
        }
        output
    }

    fn format_import(&self, value: &Import) -> String {
        if value.members.is_empty() {
            let mut output = format!("import {}", value.module_path.join("."));
            if let Some(alias) = &value.alias {
                output.push_str(" as ");
                output.push_str(alias);
            }
            return output;
        }

        let members = value
            .members
            .iter()
            .map(|member| match &member.alias {
                Some(alias) => format!("{} as {}", member.name, alias),
                None => member.name.clone(),
            })
            .collect::<Vec<_>>()
            .join(", ");
        format!("from {} import {}", value.module_path.join("."), members)
    }

    fn format_mod(&self, value: &Mod) -> KainResult<String> {
        let header = format!(
            "{}mod {}",
            self.visibility_prefix(value.visibility),
            value.name
        );
        match &value.inline {
            None => Ok(header),
            Some(children) if children.is_empty() => Ok(format!("{header}:")),
            Some(children) => {
                let body = children
                    .iter()
                    .map(|item| Ok(self.indent_text(&self.format_item(item)?, 1)))
                    .collect::<KainResult<Vec<_>>>()?
                    .join("\n");
                self.format_header_with_body(&header, &body)
            }
        }
    }

    fn format_const(&self, value: &Const) -> KainResult<String> {
        Ok(format!(
            "{}const {}: {} = {}",
            self.visibility_prefix(value.visibility),
            value.name,
            self.format_type(&value.ty),
            self.format_expr(&value.value)?
        ))
    }

    fn format_comptime_block(&self, value: &ComptimeBlock) -> KainResult<String> {
        self.format_header_with_block("comptime", &value.body)
    }

    fn format_macro(&self, value: &MacroDef) -> KainResult<String> {
        let params = value
            .params
            .iter()
            .map(|param| {
                format!(
                    "{}: {}",
                    param.name,
                    match &param.kind {
                        MacroParamKind::Expr => "expr".to_string(),
                        MacroParamKind::Type => "type".to_string(),
                        MacroParamKind::Ident => "ident".to_string(),
                        MacroParamKind::Block => "block".to_string(),
                        MacroParamKind::Token => "token".to_string(),
                        MacroParamKind::Repetition(inner) => {
                            format!("repeat<{}>", self.format_macro_param_kind(inner))
                        }
                    }
                )
            })
            .collect::<Vec<_>>()
            .join(", ");
        let header = format!("macro {}!({params})", value.name);
        match &value.body {
            MacroBody::Block(block) => self.format_header_with_block(&header, block),
            MacroBody::Tokens(tokens) => {
                let body = tokens
                    .iter()
                    .map(|token| token.content.clone())
                    .collect::<Vec<_>>()
                    .join(" ");
                self.format_header_with_body(&header, &body)
            }
        }
    }

    fn format_macro_param_kind(&self, kind: &MacroParamKind) -> String {
        match kind {
            MacroParamKind::Expr => "expr".to_string(),
            MacroParamKind::Type => "type".to_string(),
            MacroParamKind::Ident => "ident".to_string(),
            MacroParamKind::Block => "block".to_string(),
            MacroParamKind::Token => "token".to_string(),
            MacroParamKind::Repetition(inner) => {
                format!("repeat<{}>", self.format_macro_param_kind(inner))
            }
        }
    }

    fn format_test(&self, value: &TestDef) -> KainResult<String> {
        let header = format!("test {}", self.quote_string(&value.name));
        self.format_header_with_block(&header, &value.body)
    }

    fn format_material_graph(&self, value: &MaterialGraphDef) -> KainResult<String> {
        let header = format!("material {}", value.name);
        let mut entries = Vec::new();
        for input in &value.inputs {
            let mut line = format!("input {}: {}", input.name, self.format_type(&input.ty));
            if let Some(default) = &input.default {
                line.push_str(" = ");
                line.push_str(&self.format_expr(default)?);
            }
            entries.push(line);
        }
        for statement in &value.body {
            match statement {
                MaterialStatement::Let { name, value, .. } => {
                    entries.push(format!("let {name} = {}", self.format_expr(value)?));
                }
            }
        }
        for output in &value.outputs {
            entries.push(format!(
                "output {} = {}",
                output.name,
                self.format_expr(&output.value)?
            ));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_material_function(&self, value: &MaterialFunctionDef) -> KainResult<String> {
        let header = format!(
            "fn {}({})",
            value.name,
            value
                .inputs
                .iter()
                .map(|input| {
                    let mut rendered = format!("{}: {}", input.name, self.format_type(&input.ty));
                    if let Some(default) = &input.default {
                        rendered.push_str(" = ");
                        rendered.push_str(&self.format_expr(default)?);
                    }
                    Ok(rendered)
                })
                .collect::<KainResult<Vec<_>>>()?
                .join(", ")
        );
        let mut entries = Vec::new();
        for statement in &value.body {
            match statement {
                MaterialStatement::Let { name, value, .. } => {
                    entries.push(format!("let {name} = {}", self.format_expr(value)?));
                }
            }
        }
        entries.push(format!("return {}", self.format_expr(&value.output)?));
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_graph_editor(&self, value: &GraphEditorDef) -> KainResult<String> {
        let header = format!("graph {}", value.name);
        let mut entries = Vec::new();
        for node_type in &value.node_types {
            let mut node = String::new();
            self.push_attributes(&mut node, &node_type.attributes)?;
            let node_header = format!("node {}", node_type.name);
            let mut sections = Vec::new();
            if !node_type.inputs.is_empty() {
                sections.push(self.format_pin_section("inputs", &node_type.inputs)?);
            }
            if !node_type.outputs.is_empty() {
                sections.push(self.format_pin_section("outputs", &node_type.outputs)?);
            }
            if !node_type.properties.is_empty() {
                sections.push(self.format_property_section("properties", &node_type.properties)?);
            }
            node.push_str(&self.format_header_with_body(&node_header, &sections.join("\n"))?);
            entries.push(self.indent_text(&node, 1));
        }
        if let Some(schema) = &value.schema {
            let schema_body = schema
                .rules
                .iter()
                .map(|rule| {
                    Ok(format!(
                        "{}: {}",
                        rule.name,
                        self.format_expr(&rule.condition)?
                    ))
                })
                .collect::<KainResult<Vec<_>>>()?
                .join("\n");
            entries.push(self.indent_text(
                &format!(
                    "@schema\n{}",
                    self.format_header_with_body("schema", &schema_body)?
                ),
                0,
            ));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_graph_runtime(&self, value: &GraphRuntimeDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        if let Some(graph_data) = &value.graph_data {
            let mut section = String::new();
            self.push_attributes(&mut section, &graph_data.attributes)?;
            section.push_str(&self.format_graph_data(graph_data)?);
            entries.push(self.indent_text(&section, 1));
        }
        for node in &value.node_types {
            let mut section = String::new();
            self.push_attributes(&mut section, &node.attributes)?;
            section.push_str(&self.format_node_data(node)?);
            entries.push(self.indent_text(&section, 1));
        }
        if let Some(instance) = &value.instance {
            let mut section = String::new();
            self.push_attributes(&mut section, &instance.attributes)?;
            section.push_str(&self.format_graph_instance(instance)?);
            entries.push(self.indent_text(&section, 1));
        }
        if let Some(pin_config) = &value.pin_config {
            let mut section = String::new();
            self.push_attributes(&mut section, &pin_config.attributes)?;
            section.push_str(&self.format_pin_config(pin_config)?);
            entries.push(self.indent_text(&section, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_graph_data(&self, value: &GraphDataDef) -> KainResult<String> {
        let mut entries = Vec::new();
        for property in &value.properties {
            entries.push(self.format_field(property)?);
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body("struct GraphData", &entries.join("\n"))
    }

    fn format_node_data(&self, value: &NodeDataDef) -> KainResult<String> {
        let header = if let Some(base_class) = &value.base_class {
            format!("struct {}({base_class})", value.name)
        } else {
            format!("struct {}", value.name)
        };
        let mut entries = Vec::new();
        for pin in &value.input_pins {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &pin.attributes)?;
            rendered.push_str(&self.format_pin_line(pin)?);
            entries.push(self.indent_text(&rendered, 1));
        }
        for pin in &value.output_pins {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &pin.attributes)?;
            rendered.push_str(&self.format_pin_line(pin)?);
            entries.push(self.indent_text(&rendered, 1));
        }
        for property in &value.properties {
            entries.push(self.indent_text(&self.format_field(property)?, 1));
        }
        if let Some(execute_logic) = &value.execute_logic {
            entries.push(self.indent_text(
                &self.format_header_with_block("fn execute()", execute_logic)?,
                1,
            ));
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_graph_instance(&self, value: &GraphInstanceDef) -> KainResult<String> {
        let mut entries = Vec::new();
        for field in &value.state {
            entries.push(self.format_field(field)?);
        }
        for delegate in &value.delegates {
            entries.push(self.format_graph_delegate(delegate)?);
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body("struct Instance", &entries.join("\n"))
    }

    fn format_graph_delegate(&self, value: &DelegateDef) -> KainResult<String> {
        Ok(self.render_callable_head(
            &format!("delegate {}(", value.name),
            &self.format_param_parts(&value.params)?,
            ")",
        ))
    }

    fn format_pin_config(&self, value: &PinConfigDef) -> KainResult<String> {
        let mut entries = Vec::new();
        for property in &value.properties {
            entries.push(self.format_field(property)?);
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body("struct PinConfig", &entries.join("\n"))
    }

    fn format_pin_section(&self, name: &str, pins: &[PinDef]) -> KainResult<String> {
        let body = pins
            .iter()
            .map(|pin| {
                let mut rendered = String::new();
                self.push_attributes(&mut rendered, &pin.attributes)?;
                rendered.push_str(&self.format_pin_line(pin)?);
                Ok(self.indent_text(&rendered, 1))
            })
            .collect::<KainResult<Vec<_>>>()?
            .join("\n");
        self.format_header_with_body(name, &body)
    }

    fn format_property_section(
        &self,
        name: &str,
        properties: &[PropertyDef],
    ) -> KainResult<String> {
        let body = properties
            .iter()
            .map(|property| {
                let mut rendered = String::new();
                self.push_attributes(&mut rendered, &property.attributes)?;
                let mut line = format!("{}: {}", property.name, self.format_type(&property.ty));
                if let Some(default) = &property.default {
                    line.push_str(" = ");
                    line.push_str(&self.format_expr(default)?);
                }
                rendered.push_str(&line);
                Ok(self.indent_text(&rendered, 1))
            })
            .collect::<KainResult<Vec<_>>>()?
            .join("\n");
        self.format_header_with_body(name, &body)
    }

    fn format_pin_line(&self, pin: &PinDef) -> KainResult<String> {
        let mut line = format!("{}: {}", pin.name, self.format_type(&pin.ty));
        if let Some(default) = &pin.default {
            line.push_str(" = ");
            line.push_str(&self.format_expr(default)?);
        }
        Ok(line)
    }

    fn format_state_machine(&self, value: &StateMachineDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let body = value
            .states
            .iter()
            .map(|state| Ok(self.indent_text(&self.format_state_machine_state(state)?, 1)))
            .collect::<KainResult<Vec<_>>>()?
            .join("\n");
        self.format_header_with_body(&header, &body)
    }

    fn format_state_machine_state(&self, value: &StateDef) -> KainResult<String> {
        let mut output = String::new();
        self.push_attributes(&mut output, &value.attributes)?;
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        if let Some(animation) = &value.animation {
            entries.push(format!("animation: {}", self.quote_string(animation)));
        }
        for property in &value.properties {
            entries.push(self.format_field(property)?);
        }
        for transition in &value.transitions {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &transition.attributes)?;
            let transition_body = if let Some(condition) = &transition.condition {
                self.format_header_with_block("fn transition() -> Bool", condition)?
            } else {
                String::from("fn transition() -> Bool")
            };
            rendered.push_str(&transition_body);
            entries.push(self.indent_text(&rendered, 1));
        }
        if let Some(on_enter) = &value.on_enter {
            entries.push(self.indent_text(
                &self.format_header_with_block("fn on_enter()", on_enter)?,
                1,
            ));
        }
        if let Some(on_exit) = &value.on_exit {
            entries.push(
                self.indent_text(&self.format_header_with_block("fn on_exit()", on_exit)?, 1),
            );
        }
        output.push_str(&self.format_header_with_body(&header, &entries.join("\n"))?);
        Ok(output)
    }

    fn format_async_task(&self, value: &AsyncTaskDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        for field in &value.input_fields {
            entries.push(self.format_field(field)?);
        }
        for field in &value.output_fields {
            entries.push(self.format_field(field)?);
        }
        if let Some(callback) = &value.callback {
            entries.push(self.format_async_task_callback(callback)?);
        }
        if let Some(do_work) = &value.do_work {
            entries.push(self.format_header_with_block("fn do_work()", do_work)?);
        }
        if let Some(priority) = value.priority {
            entries.push(format!("priority: {priority}"));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_async_task_callback(&self, value: &AsyncTaskCallback) -> KainResult<String> {
        let mut output = String::new();
        self.push_attributes(&mut output, &value.attributes)?;
        let thread = match value.thread {
            AsyncTaskThread::Main => "Main",
            AsyncTaskThread::Worker => "Worker",
        };
        output.push_str(&format!(
            "@callback(thread: {thread})\n{}",
            self.format_header_with_block(
                &self.render_callable_head(
                    &format!("fn {}(", value.name),
                    &self.format_param_parts(&value.params)?,
                    ")",
                ),
                &value.body
            )?
        ));
        Ok(output)
    }

    fn format_editor_module(&self, value: &EditorModuleDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        for entry in &value.menu_entries {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &entry.attributes)?;
            rendered.push_str(&self.format_menu_entry(entry)?);
            entries.push(self.indent_text(&rendered, 1));
        }
        for button in &value.toolbar_buttons {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &button.attributes)?;
            rendered.push_str(&self.format_toolbar_button(button)?);
            entries.push(self.indent_text(&rendered, 1));
        }
        for widget in &value.toolbar_widgets {
            let mut rendered = String::new();
            self.push_attributes(&mut rendered, &widget.attributes)?;
            rendered.push_str(&self.format_toolbar_widget(widget));
            entries.push(self.indent_text(&rendered, 1));
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_menu_entry(&self, value: &MenuEntryDef) -> KainResult<String> {
        let mut rendered_attr = vec![
            format!("path: {}", self.quote_string(&value.path)),
            format!("label: {}", self.quote_string(&value.label)),
        ];
        if let Some(icon) = &value.icon {
            rendered_attr.push(format!("icon: {}", self.quote_string(icon)));
        }
        if let Some(tooltip) = &value.tooltip {
            rendered_attr.push(format!("tooltip: {}", self.quote_string(tooltip)));
        }
        Ok(format!(
            "@menu_entry({})\n{}",
            rendered_attr.join(", "),
            self.format_function(&value.method)?
        ))
    }

    fn format_toolbar_button(&self, value: &ToolbarButtonDef) -> KainResult<String> {
        let mut rendered_attr = vec![
            format!("section: {}", self.quote_string(&value.section)),
            format!("icon: {}", self.quote_string(&value.icon)),
        ];
        if let Some(label) = &value.label {
            rendered_attr.push(format!("label: {}", self.quote_string(label)));
        }
        if let Some(tooltip) = &value.tooltip {
            rendered_attr.push(format!("tooltip: {}", self.quote_string(tooltip)));
        }
        Ok(format!(
            "@toolbar_button({})\n{}",
            rendered_attr.join(", "),
            self.format_function(&value.method)?
        ))
    }

    fn format_toolbar_widget(&self, value: &ToolbarWidgetDef) -> String {
        format!(
            "@toolbar_widget(section: {}, position: {}, widget_type: {})",
            self.quote_string(&value.section),
            match value.position {
                ToolbarPosition::Before => "Before",
                ToolbarPosition::After => "After",
                ToolbarPosition::Start => "Start",
                ToolbarPosition::End => "End",
            },
            self.quote_string(&value.widget_type)
        )
    }

    fn format_gameplay_tags(&self, value: &GameplayTagsNamespace) -> KainResult<String> {
        let body = self.format_tag_hierarchy(&value.children)?;
        Ok(format!(
            "@gameplay_tags\n{}",
            self.format_header_with_body(&format!("namespace {}", value.name), &body)?
        ))
    }

    fn format_tag_hierarchy(&self, nodes: &[GameplayTagNode]) -> KainResult<String> {
        let mut lines = Vec::new();
        for node in nodes {
            if node.children.is_empty() {
                lines.push(node.name.clone());
            } else {
                lines.push(format!(
                    "{}:\n{}",
                    node.name,
                    self.indent_text(&self.format_tag_hierarchy(&node.children)?, 1)
                ));
            }
        }
        Ok(lines.join("\n"))
    }

    fn format_gameplay_ability(&self, value: &GameplayAbilityDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        if let Some(policy) = &value.instancing_policy {
            entries.push(format!(
                "@instancing(policy: {})",
                self.quote_string(policy)
            ));
        }
        if let Some(policy) = &value.replication_policy {
            entries.push(format!(
                "@replication(policy: {})",
                self.quote_string(policy)
            ));
        }
        if let Some(policy) = &value.net_execution_policy {
            entries.push(format!(
                "@net_execution(policy: {})",
                self.quote_string(policy)
            ));
        }
        self.push_tag_section(&mut entries, "@ability_tags", "tags", &value.ability_tags);
        self.push_tag_section(
            &mut entries,
            "@activation_required_tags",
            "required",
            &value.activation_required_tags,
        );
        self.push_tag_section(
            &mut entries,
            "@activation_blocked_tags",
            "blocked",
            &value.activation_blocked_tags,
        );
        self.push_tag_section(
            &mut entries,
            "@activation_owned_tags",
            "owned",
            &value.activation_owned_tags,
        );
        self.push_tag_section(
            &mut entries,
            "@cancel_abilities_with_tag",
            "cancel",
            &value.cancel_abilities_with_tag,
        );
        self.push_tag_section(
            &mut entries,
            "@block_abilities_with_tag",
            "block",
            &value.block_abilities_with_tag,
        );
        if let Some(effect) = &value.cost_effect {
            entries.push(format!("@cost\neffect: {effect}"));
        }
        if let Some(effect) = &value.cooldown_effect {
            entries.push(format!("@cooldown\neffect: {effect}"));
        }
        for method in &value.methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn push_tag_section(
        &self,
        entries: &mut Vec<String>,
        attribute_name: &str,
        field_name: &str,
        tags: &[String],
    ) {
        if !tags.is_empty() {
            entries.push(format!(
                "{attribute_name}\n{field_name}: {}",
                self.format_string_array(tags)
            ));
        }
    }

    fn format_gameplay_effect(&self, value: &GameplayEffectDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        if let Some(policy) = &value.duration_policy {
            let mut section = format!("@duration(type: {})", self.quote_string(policy));
            if let Some(magnitude) = value.duration_magnitude {
                section.push('\n');
                section.push_str(&format!("duration: {}", self.format_f32(magnitude)));
            }
            entries.push(section);
        }
        if value.period.is_some() || value.execute_on_application {
            let mut section = String::from("@period");
            if let Some(period) = value.period {
                section.push('\n');
                section.push_str(&format!("period: {}", self.format_f32(period)));
            }
            section.push('\n');
            section.push_str(&format!(
                "execute_on_application: {}",
                if value.execute_on_application {
                    "true"
                } else {
                    "false"
                }
            ));
            entries.push(section);
        }
        for modifier in &value.modifiers {
            entries.push(format!(
                "@modifier(attribute: {}, operation: {})\nmagnitude: {}",
                self.quote_string(&modifier.attribute),
                self.quote_string(&modifier.operation),
                self.format_f32(modifier.magnitude)
            ));
        }
        if value.stacking_type.is_some() || value.stacking_limit.is_some() {
            let mut section = String::from("@stacking");
            if let Some(stacking_type) = &value.stacking_type {
                section.push('\n');
                section.push_str(&format!("type: {}", self.quote_string(stacking_type)));
            }
            if let Some(limit) = value.stacking_limit {
                section.push('\n');
                section.push_str(&format!("limit: {limit}"));
            }
            entries.push(section);
        }
        self.push_tag_section(&mut entries, "@owned_tags", "tags", &value.owned_tags);
        self.push_tag_section(&mut entries, "@granted_tags", "tags", &value.granted_tags);
        self.push_require_ignore_section(
            &mut entries,
            "@application_tag_requirements",
            &value.application_required_tags,
            &value.application_ignored_tags,
        );
        self.push_require_ignore_section(
            &mut entries,
            "@ongoing_tag_requirements",
            &value.ongoing_required_tags,
            &value.ongoing_ignored_tags,
        );
        self.push_require_ignore_section(
            &mut entries,
            "@removal_tag_requirements",
            &value.removal_required_tags,
            &value.removal_ignored_tags,
        );
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn push_require_ignore_section(
        &self,
        entries: &mut Vec<String>,
        attribute_name: &str,
        required: &[String],
        ignored: &[String],
    ) {
        if required.is_empty() && ignored.is_empty() {
            return;
        }
        let mut lines = vec![attribute_name.to_string()];
        if !required.is_empty() {
            lines.push(format!("require: {}", self.format_string_array(required)));
        }
        if !ignored.is_empty() {
            lines.push(format!("ignore: {}", self.format_string_array(ignored)));
        }
        entries.push(lines.join("\n"));
    }

    fn format_gameplay_cue(&self, value: &GameplayCueDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = vec![
            format!("tag: {}", self.quote_string(&value.tag)),
            format!(
                "type: {}",
                self.quote_string(match value.cue_type {
                    CueType::Static => "Static",
                    CueType::Actor => "Actor",
                })
            ),
            format!(
                "auto_destroy: {}",
                if value.auto_destroy { "true" } else { "false" }
            ),
        ];
        for field in &value.state_fields {
            entries.push(format!(
                "state {}: {}",
                field.name,
                self.format_type(&field.ty)
            ));
        }
        if let Some(on_execute) = &value.on_execute {
            entries.push(self.format_named_body_field("on_execute", &on_execute.body)?);
        }
        if let Some(on_add) = &value.on_add {
            entries.push(self.format_named_body_field("on_add", &on_add.body)?);
        }
        if let Some(on_remove) = &value.on_remove {
            entries.push(self.format_named_body_field("on_remove", &on_remove.body)?);
        }
        if let Some(while_active) = &value.while_active {
            entries.push(self.format_named_body_field("while_active", &while_active.body)?);
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_named_body_field(&self, name: &str, body: &Block) -> KainResult<String> {
        self.format_header_with_block(name, body)
    }

    fn format_ability_task(&self, value: &AbilityTaskDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = Vec::new();
        for delegate in &value.delegates {
            entries.push(format!(
                "@delegate\n{}: {}",
                delegate.name, delegate.delegate_type
            ));
        }
        for field in &value.state_fields {
            let mut line = format!("state {}: {}", field.name, self.format_type(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(" = ");
                line.push_str(&self.format_expr(default)?);
            }
            entries.push(line);
        }
        if let Some(activate_method) = &value.activate_method {
            entries.push(self.indent_text(&self.format_function(activate_method)?, 1));
        }
        if let Some(on_destroy_method) = &value.on_destroy_method {
            entries.push(self.indent_text(&self.format_function(on_destroy_method)?, 1));
        }
        for method in &value.custom_methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_target_actor(&self, value: &TargetActorDef) -> KainResult<String> {
        let header = format!("struct {}", value.name);
        let mut entries = vec![format!(
            "trace_type: {}",
            self.quote_string(match value.trace_type {
                TraceType::Line => "Line",
                TraceType::Sphere => "Sphere",
                TraceType::Cone => "Cone",
                TraceType::Box => "Box",
                TraceType::Cylinder => "Cylinder",
            })
        )];
        if let Some(max_range) = value.max_range {
            entries.push(format!("max_range: {}", self.format_f64(max_range)));
        }
        if let Some(trace_channel) = &value.trace_channel {
            entries.push(format!(
                "trace_channel: {}",
                self.quote_string(trace_channel)
            ));
        }
        if let Some(filter) = &value.filter {
            entries.push(self.format_target_filter(filter)?);
        }
        if let Some(reticle_class) = &value.reticle_class {
            entries.push(format!(
                "reticle_class: {}",
                self.quote_string(reticle_class)
            ));
        }
        for method in &value.custom_methods {
            entries.push(self.indent_text(&self.format_function(method)?, 1));
        }
        self.format_header_with_body(&header, &entries.join("\n"))
    }

    fn format_target_filter(&self, value: &TargetFilter) -> KainResult<String> {
        let mut entries = Vec::new();
        if let Some(self_filter) = &value.self_filter {
            entries.push(format!("self_filter: {}", self.quote_string(self_filter)));
        }
        if let Some(required_actor_class) = &value.required_actor_class {
            entries.push(format!(
                "required_actor_class: {}",
                self.quote_string(required_actor_class)
            ));
        }
        if !value.require_tags.is_empty() {
            entries.push(format!(
                "require_tags: {}",
                self.format_string_array(&value.require_tags)
            ));
        }
        if !value.ignore_tags.is_empty() {
            entries.push(format!(
                "ignore_tags: {}",
                self.format_string_array(&value.ignore_tags)
            ));
        }
        if let Some(custom_filter_method) = &value.custom_filter_method {
            entries.push(self.indent_text(&self.format_function(custom_filter_method)?, 1));
        }
        self.format_header_with_body("filter", &entries.join("\n"))
    }

    fn format_statement_sequence(&self, stmts: &[Stmt]) -> KainResult<String> {
        if stmts.is_empty() {
            return Err(KainError::runtime(
                "Kain formatter cannot emit empty executable blocks in v1",
            ));
        }
        stmts
            .iter()
            .map(|stmt| self.format_stmt(stmt))
            .collect::<KainResult<Vec<_>>>()
            .map(|parts| parts.join("\n"))
    }

    fn format_stmt(&self, stmt: &Stmt) -> KainResult<String> {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                let mut line = format!("let {}", self.format_pattern(pattern)?);
                if let Some(ty) = ty {
                    line.push_str(": ");
                    line.push_str(&self.format_type(ty));
                }
                if let Some(value) = value {
                    line.push_str(" = ");
                    line.push_str(&self.format_expr(value)?);
                }
                Ok(line)
            }
            Stmt::Expr(Expr::Block(block, _)) => self.format_statement_sequence(&block.stmts),
            Stmt::Expr(expr) => self.format_expr(expr),
            Stmt::Defer { expr, .. } => Ok(format!("defer {}", self.format_expr(expr)?)),
            Stmt::Dispatch {
                compute_key,
                dispatch_size,
                ..
            } => {
                let dims = match dispatch_size {
                    DispatchSize::Fixed([x, y, z]) => [x, y, z],
                    DispatchSize::Indirect(expr) => {
                        return Ok(format!(
                            "dispatch {} from {}",
                            self.quote_string(compute_key),
                            self.format_expr(expr)?
                        ))
                    }
                };
                Ok(format!(
                    "dispatch {} [{}, {}, {}]",
                    self.quote_string(compute_key),
                    self.format_expr(&dims[0])?,
                    self.format_expr(&dims[1])?,
                    self.format_expr(&dims[2])?
                ))
            },
            Stmt::Return(Some(expr), _) => Ok(format!("return {}", self.format_expr(expr)?)),
            Stmt::Return(None, _) => Ok(String::from("return")),
            Stmt::Break(Some(expr), _) => Ok(format!("break {}", self.format_expr(expr)?)),
            Stmt::Break(None, _) => Ok(String::from("break")),
            Stmt::Continue(_) => Ok(String::from("continue")),
            Stmt::For {
                binding,
                iter,
                body,
                ..
            } => self.format_header_with_body(
                &format!(
                    "for {} in {}",
                    self.format_pattern(binding)?,
                    self.format_expr(iter)?
                ),
                &self.format_statement_sequence(&body.stmts)?,
            ),
            Stmt::Fanout {
                binding,
                iter,
                body,
                ..
            } => self.format_header_with_body(
                &format!(
                    "fanout {} in {}",
                    self.format_pattern(binding)?,
                    self.format_expr(iter)?
                ),
                &self.format_statement_sequence(&body.stmts)?,
            ),
            Stmt::While {
                condition, body, ..
            } => self.format_header_with_body(
                &format!("while {}", self.format_expr(condition)?),
                &self.format_statement_sequence(&body.stmts)?,
            ),
            Stmt::Loop { body, .. } => {
                self.format_header_with_body("loop", &self.format_statement_sequence(&body.stmts)?)
            }
            Stmt::Item(item) => self.format_item(item),
            Stmt::Subgroup { .. } => Ok(String::new()),
        }
    }

    fn format_expr(&self, expr: &Expr) -> KainResult<String> {
        self.format_expr_with_prec(expr, 0)
    }

    fn format_expr_with_prec(&self, expr: &Expr, parent_prec: u8) -> KainResult<String> {
        let current_prec = self.expr_precedence(expr);
        let mut rendered = match expr {
            Expr::Int(value, _) => value.to_string(),
            Expr::Float(value, _) => self.format_f64(*value),
            Expr::String(value, _) => self.quote_string(value),
            Expr::FString(parts, _) => self.format_f_string(parts)?,
            Expr::Bool(value, _) => value.to_string(),
            Expr::None(_) => String::from("none"),
            Expr::Ident(name, _) => name.clone(),
            Expr::MacroCall { name, args, .. } => format!(
                "{}!({})",
                name,
                args.iter()
                    .map(|arg| self.format_expr(arg))
                    .collect::<KainResult<Vec<_>>>()?
                    .join(", ")
            ),
            Expr::Binary {
                left, op, right, ..
            } => {
                let prec = self.binary_precedence(*op);
                let left_rendered = self.format_expr_with_prec(left, prec)?;
                let right_rendered = self.format_expr_with_prec(right, prec + 1)?;
                format!(
                    "{} {} {}",
                    left_rendered,
                    self.binary_op_to_string(*op),
                    right_rendered
                )
            }
            Expr::Unary { op, operand, .. } => format!(
                "{}{}",
                self.unary_op_to_string(*op),
                self.format_expr_with_prec(operand, 13)?
            ),
            Expr::Call { callee, args, .. } => {
                let callee_text = self.format_expr_with_prec(callee, 14)?;
                let rendered_args = args
                    .iter()
                    .map(|arg| self.format_call_arg(arg))
                    .collect::<KainResult<Vec<_>>>()?;
                self.render_call_like(&callee_text, &rendered_args)
            }
            Expr::StageCall {
                runtime,
                function,
                args,
                ..
            } => {
                let callee_text = format!("{} {}", runtime.as_str(), function);
                let rendered_args = args
                    .iter()
                    .map(|arg| self.format_call_arg(arg))
                    .collect::<KainResult<Vec<_>>>()?;
                self.render_call_like(&callee_text, &rendered_args)
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let callee_text =
                    format!("{}.{}", self.format_expr_with_prec(receiver, 14)?, method);
                let rendered_args = args
                    .iter()
                    .map(|arg| self.format_call_arg(arg))
                    .collect::<KainResult<Vec<_>>>()?;
                self.render_call_like(&callee_text, &rendered_args)
            }
            Expr::Field { object, field, .. } => {
                format!("{}.{}", self.format_expr_with_prec(object, 14)?, field)
            }
            Expr::Index { object, index, .. } => format!(
                "{}[{}]",
                self.format_expr_with_prec(object, 14)?,
                self.format_expr(index)?
            ),
            Expr::Assign { target, value, .. } => format!(
                "{} = {}",
                self.format_expr(target)?,
                self.format_expr(value)?
            ),
            Expr::Struct {
                name, fields, rest, ..
            } => {
                let mut entries = fields
                    .iter()
                    .map(|(field, value)| Ok(format!("{field}: {}", self.format_expr(value)?)))
                    .collect::<KainResult<Vec<_>>>()?;
                if let Some(rest) = rest {
                    entries.push(format!("..{}", self.format_expr(rest)?));
                }
                self.render_braced_sequence(name, &entries)
            }
            Expr::AggregateInit {
                ty,
                fields,
                zero_fill_rest,
                ..
            } => {
                let mut args = vec![self.quote_string(&self.format_type(ty))];
                for (field, value) in fields {
                    args.push(format!("{field} = {}", self.format_expr(value)?));
                }
                if !zero_fill_rest {
                    args.push(String::from("zero_fill_rest = false"));
                }
                self.render_call_like("aggregate_init", &args)
            }
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                ..
            } => {
                let head = format!("{enum_name}::{variant}");
                self.format_enum_variant_fields(&head, fields)?
            }
            Expr::Array(items, _) => self.render_wrapped_sequence(
                "[",
                &items
                    .iter()
                    .map(|item| self.format_expr(item))
                    .collect::<KainResult<Vec<_>>>()?,
                "]",
            ),
            Expr::Tuple(items, _) => self.render_tuple_sequence(
                &items
                    .iter()
                    .map(|item| self.format_expr(item))
                    .collect::<KainResult<Vec<_>>>()?,
            ),
            Expr::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                let start_text = start
                    .as_ref()
                    .map(|value| self.format_expr(value))
                    .transpose()?
                    .unwrap_or_default();
                let end_text = end
                    .as_ref()
                    .map(|value| self.format_expr(value))
                    .transpose()?
                    .unwrap_or_default();
                let marker = if *inclusive { "..=" } else { ".." };
                format!("{start_text}{marker}{end_text}")
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => self.format_if_expr(condition, then_branch, else_branch.as_deref())?,
            Expr::Match {
                scrutinee, arms, ..
            } => self.format_match_expr(scrutinee, arms)?,
            Expr::Lambda {
                params,
                return_type,
                body,
                ..
            } => self.format_lambda(params, return_type.as_ref(), body)?,
            Expr::Ref { mutable, value, .. } => {
                if *mutable {
                    format!("&mut {}", self.format_expr_with_prec(value, 13)?)
                } else {
                    format!("&{}", self.format_expr_with_prec(value, 13)?)
                }
            }
            Expr::AddrOf {
                value, pointee_ty, ..
            } => match pointee_ty {
                Some(ty) => format!(
                    "addr_of({}, {})",
                    self.format_expr(value)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!("addr_of({})", self.format_expr(value)?),
            },
            Expr::Deref(value, _) => format!("*{}", self.format_expr_with_prec(value, 13)?),
            Expr::PtrOffset {
                pointer,
                offset,
                element_ty,
                ..
            } => match element_ty {
                Some(ty) => format!(
                    "ptr_offset({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(offset)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!(
                    "ptr_offset({}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(offset)?
                ),
            },
            Expr::MemLoad {
                pointer, load_ty, ..
            } => match load_ty {
                Some(ty) => format!(
                    "mem_load({}, {})",
                    self.format_expr(pointer)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!("mem_load({})", self.format_expr(pointer)?),
            },
            Expr::MemStore {
                pointer,
                value,
                store_ty,
                ..
            } => match store_ty {
                Some(ty) => format!(
                    "mem_store({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!(
                    "mem_store({}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?
                ),
            },
            Expr::VolatileLoad {
                pointer, load_ty, ..
            } => match load_ty {
                Some(ty) => format!(
                    "volatile_load({}, {})",
                    self.format_expr(pointer)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!("volatile_load({})", self.format_expr(pointer)?),
            },
            Expr::VolatileStore {
                pointer,
                value,
                store_ty,
                ..
            } => match store_ty {
                Some(ty) => format!(
                    "volatile_store({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!(
                    "volatile_store({}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?
                ),
            },
            Expr::AtomicLoad {
                pointer,
                load_ty,
                ordering,
                ..
            } => match load_ty {
                Some(ty) => format!(
                    "atomic_load({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.quote_string(&self.format_type(ty)),
                    self.quote_string(ordering.as_str())
                ),
                None => format!(
                    "atomic_load({}, {})",
                    self.format_expr(pointer)?,
                    self.quote_string(ordering.as_str())
                ),
            },
            Expr::AtomicStore {
                pointer,
                value,
                store_ty,
                ordering,
                ..
            } => match store_ty {
                Some(ty) => format!(
                    "atomic_store({}, {}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?,
                    self.quote_string(&self.format_type(ty)),
                    self.quote_string(ordering.as_str())
                ),
                None => format!(
                    "atomic_store({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(value)?,
                    self.quote_string(ordering.as_str())
                ),
            },
            Expr::AtomicAdd {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_add",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicSub {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_sub",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicAnd {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_and",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicOr {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_or",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicXor {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_xor",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicExchange {
                pointer,
                value,
                op_ty,
                ordering,
                ..
            } => self.format_ordered_atomic_binary_call(
                "atomic_exchange",
                pointer,
                value,
                op_ty.as_ref(),
                *ordering,
            )?,
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                op_ty,
                success_ordering,
                failure_ordering,
                ..
            } => match op_ty {
                Some(ty) => format!(
                    "atomic_compare_exchange({}, {}, {}, {}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(expected)?,
                    self.format_expr(desired)?,
                    self.quote_string(&self.format_type(ty)),
                    self.quote_string(success_ordering.as_str()),
                    self.quote_string(failure_ordering.as_str())
                ),
                None => format!(
                    "atomic_compare_exchange({}, {}, {}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(expected)?,
                    self.format_expr(desired)?,
                    self.quote_string(success_ordering.as_str()),
                    self.quote_string(failure_ordering.as_str())
                ),
            },
            Expr::AtomicFence { ordering, .. } => {
                format!("atomic_fence({})", self.quote_string(ordering.as_str()))
            }
            Expr::CpuFence { kind, .. } => format!("{}()", kind.intrinsic_name()),
            Expr::CpuCacheFlush { pointer, .. } => {
                format!("clflush({})", self.format_expr(pointer)?)
            }
            Expr::InlineAsm {
                template,
                operands,
                options,
                ..
            } => {
                let mut parts = vec![self.quote_string(template)];
                for operand in operands {
                    parts.push(self.format_expr(operand)?);
                }
                if !options.volatile {
                    parts.push("volatile = false".to_string());
                }
                if options.memory {
                    parts.push("memory = true".to_string());
                }
                if options.intel {
                    parts.push("intel = true".to_string());
                }
                if !options.constraints.is_empty() {
                    parts.push(format!(
                        "constraints = {}",
                        self.quote_string(&options.constraints.join(","))
                    ));
                }
                if !options.clobbers.is_empty() {
                    parts.push(format!(
                        "clobbers = {}",
                        self.quote_string(&options.clobbers.join(","))
                    ));
                }
                format!("asm({})", parts.join(", "))
            }
            Expr::SizeOfType { target, .. } => {
                format!(
                    "sizeof_type({})",
                    self.quote_string(&self.format_type(target))
                )
            }
            Expr::AlignOfType { target, .. } => {
                format!(
                    "alignof_type({})",
                    self.quote_string(&self.format_type(target))
                )
            }
            Expr::Alloca { ty, .. } => {
                format!("alloca({})", self.quote_string(&self.format_type(ty)))
            }
            Expr::Uninit { ty, .. } => {
                format!("uninit({})", self.quote_string(&self.format_type(ty)))
            }
            Expr::Alloc {
                size, ty, zeroed, ..
            } => match ty {
                Some(ty) => format!(
                    "{}({}, {})",
                    if *zeroed { "alloc_zeroed" } else { "alloc" },
                    self.format_expr(size)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!(
                    "{}({})",
                    if *zeroed { "alloc_zeroed" } else { "alloc" },
                    self.format_expr(size)?
                ),
            },
            Expr::Realloc {
                pointer, size, ty, ..
            } => match ty {
                Some(ty) => format!(
                    "realloc_mem({}, {}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(size)?,
                    self.quote_string(&self.format_type(ty))
                ),
                None => format!(
                    "realloc_mem({}, {})",
                    self.format_expr(pointer)?,
                    self.format_expr(size)?
                ),
            },
            Expr::Observe { target, body, .. } => {
                if let Expr::Block(block, _) = body.as_ref() {
                    let head = format!("observe {}", self.format_expr(target)?);
                    self.format_header_with_block(&head, block)?
                } else {
                    return Err(KainError::runtime(
                        "Kain formatter cannot emit non-block observe expressions in v1",
                    ));
                }
            }
            Expr::Collapse { target, body, .. } => {
                if let Expr::Block(block, _) = body.as_ref() {
                    let head = format!("collapse {}", self.format_expr(target)?);
                    self.format_header_with_block(&head, block)?
                } else {
                    return Err(KainError::runtime(
                        "Kain formatter cannot emit non-block collapse expressions in v1",
                    ));
                }
            }
            Expr::Decay { target, .. } => {
                format!("decay {}", self.format_expr_with_prec(target, 13)?)
            }
            Expr::Share { target, body, .. } => {
                if let Expr::Block(block, _) = body.as_ref() {
                    let head = format!("share {}", self.format_expr(target)?);
                    self.format_header_with_block(&head, block)?
                } else {
                    return Err(KainError::runtime(
                        "Kain formatter cannot emit non-block share expressions in v1",
                    ));
                }
            }
            Expr::Teleport {
                value,
                source_world,
                target_world,
                channel,
                ..
            } => {
                let mut rendered = format!(
                    "teleport {} from {} to {}",
                    self.format_expr_with_prec(value, 13)?,
                    self.format_identifier_or_string(source_world),
                    self.format_identifier_or_string(target_world)
                );
                if let Some(channel) = channel {
                    rendered.push_str(&format!(
                        " via {}",
                        self.format_identifier_or_string(channel)
                    ));
                }
                rendered
            }
            Expr::Cast { value, target, .. } => {
                format!(
                    "{} as {}",
                    self.format_expr_with_prec(value, 12)?,
                    self.format_type(target)
                )
            }
            Expr::Bitcast { value, target, .. } => format!(
                "bitcast({}, {})",
                self.format_expr(value)?,
                self.quote_string(&self.format_type(target))
            ),
            Expr::Try(value, _) => format!("{}?", self.format_expr_with_prec(value, 14)?),
            Expr::Await(value, _) => format!("await {}", self.format_expr_with_prec(value, 13)?),
            Expr::AsyncBlock(value, _) => {
                if let Expr::Block(block, _) = value.as_ref() {
                    self.format_header_with_block("async", block)?
                } else {
                    format!("async {}", self.format_expr_with_prec(value, 13)?)
                }
            }
            Expr::Spawn { actor, init, .. } => {
                let args = init
                    .iter()
                    .map(|(name, value)| Ok(format!("{name} = {}", self.format_expr(value)?)))
                    .collect::<KainResult<Vec<_>>>()?;
                format!("spawn {}({})", actor, args.join(", "))
            }
            Expr::SendMsg {
                target,
                message,
                data,
                ..
            } => {
                let args = data
                    .iter()
                    .map(|(name, value)| Ok(format!("{name} = {}", self.format_expr(value)?)))
                    .collect::<KainResult<Vec<_>>>()?;
                format!(
                    "send {}.{}({})",
                    self.format_expr(target)?,
                    message,
                    args.join(", ")
                )
            }
            Expr::Emit {
                event,
                data,
                ..
            } => {
                let args = data
                    .iter()
                    .map(|(name, value)| Ok(format!("{name} = {}", self.format_expr(value)?)))
                    .collect::<KainResult<Vec<_>>>()?;
                format!(
                    "emit {}({})",
                    event,
                    args.join(", ")
                )
            }
            Expr::Comptime(value, _) => {
                if let Expr::Block(block, _) = value.as_ref() {
                    self.format_header_with_block("comptime", block)?
                } else {
                    return Err(KainError::runtime(
                        "Kain formatter cannot emit non-block comptime expressions in v1",
                    ));
                }
            }
            Expr::Block(block, _) => {
                if let Some(expr) = self.single_expr_from_block(block) {
                    self.format_expr(expr)?
                } else {
                    return Err(KainError::runtime(
                        "Kain formatter cannot emit standalone multi-statement block expressions in v1",
                    ));
                }
            }
            Expr::JSX(node, _) => self.format_jsx_node(node)?,
            Expr::Paren(value, _) => format!("({})", self.format_expr(value)?),
            Expr::Return(Some(value), _) => format!("return {}", self.format_expr(value)?),
            Expr::Return(None, _) => String::from("return"),
            Expr::Break(Some(value), _) => format!("break {}", self.format_expr(value)?),
            Expr::Break(None, _) => String::from("break"),
            Expr::Continue(_) => String::from("continue"),
        };

        if current_prec != 0 && current_prec < parent_prec {
            rendered = format!("({rendered})");
        }
        Ok(rendered)
    }

    fn single_expr_from_block<'a>(&self, block: &'a Block) -> Option<&'a Expr> {
        if block.stmts.len() != 1 {
            return None;
        }
        match block.stmts.first() {
            Some(Stmt::Expr(expr)) => Some(expr),
            Some(Stmt::Return(Some(expr), _)) => Some(expr),
            _ => None,
        }
    }

    fn expr_precedence(&self, expr: &Expr) -> u8 {
        match expr {
            Expr::Assign { .. } => 1,
            Expr::Binary { op, .. } => self.binary_precedence(*op),
            Expr::Cast { .. } | Expr::Bitcast { .. } => 12,
            Expr::Unary { .. }
            | Expr::Ref { .. }
            | Expr::Deref(..)
            | Expr::Await(..)
            | Expr::Try(..)
            | Expr::Teleport { .. } => 13,
            Expr::Call { .. }
            | Expr::StageCall { .. }
            | Expr::MethodCall { .. }
            | Expr::Field { .. }
            | Expr::Index { .. } => 14,
            _ => 0,
        }
    }

    fn binary_precedence(&self, op: BinaryOp) -> u8 {
        match op {
            BinaryOp::Or => 1,
            BinaryOp::And => 2,
            BinaryOp::BitOr => 3,
            BinaryOp::BitXor => 4,
            BinaryOp::BitAnd => 5,
            BinaryOp::Eq | BinaryOp::Ne => 6,
            BinaryOp::Lt | BinaryOp::Gt | BinaryOp::Le | BinaryOp::Ge => 7,
            BinaryOp::Shl | BinaryOp::Shr => 8,
            BinaryOp::Add | BinaryOp::Sub => 9,
            BinaryOp::Mul | BinaryOp::Div | BinaryOp::Mod => 10,
            BinaryOp::Pow => 11,
            BinaryOp::Assign
            | BinaryOp::AddAssign
            | BinaryOp::SubAssign
            | BinaryOp::MulAssign
            | BinaryOp::DivAssign
            | BinaryOp::Range
            | BinaryOp::RangeInclusive => 1,
        }
    }

    fn format_if_expr(
        &self,
        condition: &Expr,
        then_branch: &Block,
        else_branch: Option<&ElseBranch>,
    ) -> KainResult<String> {
        let mut output = format!(
            "if {}:\n{}",
            self.format_expr(condition)?,
            self.indent_text(&self.format_statement_sequence(&then_branch.stmts)?, 1)
        );
        if let Some(else_branch) = else_branch {
            output.push('\n');
            output.push_str(&self.format_else_branch(else_branch)?);
        }
        Ok(output)
    }

    fn format_else_branch(&self, branch: &ElseBranch) -> KainResult<String> {
        match branch {
            ElseBranch::Else(block) => Ok(format!(
                "else:\n{}",
                self.indent_text(&self.format_statement_sequence(&block.stmts)?, 1)
            )),
            ElseBranch::ElseIf(condition, block, next) => {
                let mut output = format!(
                    "else if {}:\n{}",
                    self.format_expr(condition)?,
                    self.indent_text(&self.format_statement_sequence(&block.stmts)?, 1)
                );
                if let Some(next) = next.as_deref() {
                    output.push('\n');
                    output.push_str(&self.format_else_branch(next)?);
                }
                Ok(output)
            }
        }
    }

    fn format_match_expr(&self, scrutinee: &Expr, arms: &[MatchArm]) -> KainResult<String> {
        let mut lines = Vec::new();
        lines.push(format!("match {}:", self.format_expr(scrutinee)?));
        for arm in arms {
            lines.push(self.indent_text(&self.format_match_arm(arm)?, 1));
        }
        Ok(lines.join("\n"))
    }

    fn format_match_arm(&self, arm: &MatchArm) -> KainResult<String> {
        let mut head = self.format_pattern(&arm.pattern)?;
        if let Some(guard) = &arm.guard {
            head.push_str(" if ");
            head.push_str(&self.format_expr(guard)?);
        }
        match &arm.body {
            Expr::Block(block, _) => Ok(format!(
                "{} =>\n{}",
                head,
                self.indent_text(&self.format_statement_sequence(&block.stmts)?, 1)
            )),
            Expr::If { .. } | Expr::Match { .. } => Ok(format!(
                "{} =>\n{}",
                head,
                self.indent_text(&self.format_expr(&arm.body)?, 1)
            )),
            _ => Ok(format!("{} => {}", head, self.format_expr(&arm.body)?)),
        }
    }

    fn format_lambda(
        &self,
        params: &[Param],
        return_type: Option<&Type>,
        body: &Expr,
    ) -> KainResult<String> {
        let can_use_pipe = return_type.is_none()
            && params.iter().all(|param| {
                matches!(param.ty, Type::Infer(_)) && !param.mutable && param.default.is_none()
            });
        if can_use_pipe {
            return Ok(format!(
                "|{}| {}",
                params
                    .iter()
                    .map(|param| param.name.clone())
                    .collect::<Vec<_>>()
                    .join(", "),
                self.format_expr(body)?
            ));
        }

        let head = self.render_callable_head(
            "fn(",
            &self.format_param_parts(params)?,
            &self.render_callable_suffix(return_type, None, &[]),
        );
        match body {
            Expr::Block(block, _) => self.format_header_with_block(&head, block),
            _ => Ok(format!("{head}: {}", self.format_expr(body)?)),
        }
    }

    fn format_call_arg(&self, arg: &CallArg) -> KainResult<String> {
        match &arg.name {
            Some(name) => Ok(format!("{name} = {}", self.format_expr(&arg.value)?)),
            None => self.format_expr(&arg.value),
        }
    }

    fn inline_width(&self, text: &str) -> Option<usize> {
        if text.contains('\n') {
            None
        } else {
            Some(text.chars().count())
        }
    }

    fn can_inline_sequence(&self, prefix: &str, items: &[String], suffix: &str) -> bool {
        let Some(prefix_width) = self.inline_width(prefix) else {
            return false;
        };
        let Some(suffix_width) = self.inline_width(suffix) else {
            return false;
        };
        let Some(items_width) = items
            .iter()
            .try_fold(0usize, |acc, item| self.inline_width(item).map(|width| acc + width))
        else {
            return false;
        };
        let separators = 2 * items.len().saturating_sub(1);
        prefix_width + items_width + separators + suffix_width <= self.options.max_width
    }

    fn render_wrapped_sequence(&self, prefix: &str, items: &[String], suffix: &str) -> String {
        if items.is_empty() {
            return format!("{prefix}{suffix}");
        }
        if self.can_inline_sequence(prefix, items, suffix) {
            return format!("{prefix}{}{suffix}", items.join(", "));
        }

        let body = items
            .iter()
            .map(|item| self.indent_text(item, 1))
            .collect::<Vec<_>>()
            .join(",\n");
        format!("{prefix}\n{body}\n{suffix}")
    }

    fn render_braced_sequence(&self, head: &str, items: &[String]) -> String {
        if items.is_empty() {
            return format!("{head} {{}}");
        }
        if self.can_inline_sequence(&format!("{head} {{ "), items, " }") {
            return format!("{head} {{ {} }}", items.join(", "));
        }

        let body = items
            .iter()
            .map(|item| self.indent_text(item, 1))
            .collect::<Vec<_>>()
            .join(",\n");
        format!("{head} {{\n{body}\n}}")
    }

    fn render_tuple_sequence(&self, items: &[String]) -> String {
        if items.is_empty() {
            return String::from("()");
        }
        if items.len() == 1 {
            if self.can_inline_sequence("(", items, ",)") {
                return format!("({},)", items[0]);
            }
            return format!("(\n{},\n)", self.indent_text(&items[0], 1));
        }
        self.render_wrapped_sequence("(", items, ")")
    }

    fn render_callable_suffix(
        &self,
        return_type: Option<&Type>,
        where_clause: Option<&WhereClause>,
        effects: &[Effect],
    ) -> String {
        let mut suffix = String::from(")");
        if let Some(return_type) = return_type {
            suffix.push_str(" -> ");
            suffix.push_str(&self.format_type(return_type));
        }
        if let Some(where_clause) = where_clause {
            suffix.push(' ');
            suffix.push_str(&self.format_where_clause(where_clause));
        }
        if !effects.is_empty() {
            suffix.push_str(" with ");
            suffix.push_str(&self.format_effects(effects));
        }
        suffix
    }

    fn render_callable_head(&self, prefix: &str, params: &[String], suffix: &str) -> String {
        self.render_wrapped_sequence(prefix, params, suffix)
    }

    fn render_call_like(&self, callee: &str, args: &[String]) -> String {
        self.render_wrapped_sequence(&format!("{callee}("), args, ")")
    }

    fn format_enum_variant_fields(
        &self,
        head: &str,
        fields: &EnumVariantFields,
    ) -> KainResult<String> {
        match fields {
            EnumVariantFields::Unit => Ok(head.to_string()),
            EnumVariantFields::Tuple(values) => Ok(self.render_call_like(
                head,
                &values
                    .iter()
                    .map(|value| self.format_expr(value))
                    .collect::<KainResult<Vec<_>>>()?,
            )),
            EnumVariantFields::Struct(values) => Ok(self.render_braced_sequence(
                head,
                &values
                    .iter()
                    .map(|(name, value)| Ok(format!("{name}: {}", self.format_expr(value)?)))
                    .collect::<KainResult<Vec<_>>>()?,
            )),
        }
    }

    fn format_jsx_node(&self, node: &JSXNode) -> KainResult<String> {
        match node {
            JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                let attrs = self.format_jsx_attrs(attributes)?;
                if children.is_empty() {
                    Ok(format!("<{tag}{attrs} />"))
                } else {
                    Ok(format!(
                        "<{tag}{attrs}>{}</{tag}>",
                        children
                            .iter()
                            .map(|child| self.format_jsx_node(child))
                            .collect::<KainResult<Vec<_>>>()?
                            .join("")
                    ))
                }
            }
            JSXNode::Text(text, _) => Ok(text.clone()),
            JSXNode::Expression(expr) => Ok(format!("{{{}}}", self.format_expr(expr)?)),
            JSXNode::ComponentCall {
                name,
                props,
                children,
                ..
            } => {
                let attrs = self.format_jsx_attrs(props)?;
                if children.is_empty() {
                    Ok(format!("<{name}{attrs} />"))
                } else {
                    Ok(format!(
                        "<{name}{attrs}>{}</{name}>",
                        children
                            .iter()
                            .map(|child| self.format_jsx_node(child))
                            .collect::<KainResult<Vec<_>>>()?
                            .join("")
                    ))
                }
            }
            JSXNode::For {
                binding,
                iter,
                body,
                ..
            } => Ok(format!(
                "{{for {binding} in {}: {}}}",
                self.format_expr(iter)?,
                self.format_jsx_node(body)?
            )),
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                let mut rendered = format!(
                    "{{if {}: {}",
                    self.format_expr(condition)?,
                    self.format_jsx_node(then_branch)?
                );
                if let Some(else_branch) = else_branch {
                    rendered.push_str(" else: ");
                    rendered.push_str(&self.format_jsx_node(else_branch)?);
                }
                rendered.push('}');
                Ok(rendered)
            }
            JSXNode::Fragment(children, _) => Ok(format!(
                "<Fragment>{}</Fragment>",
                children
                    .iter()
                    .map(|child| self.format_jsx_node(child))
                    .collect::<KainResult<Vec<_>>>()?
                    .join("")
            )),
        }
    }

    fn format_jsx_attrs(&self, attrs: &[JSXAttribute]) -> KainResult<String> {
        if attrs.is_empty() {
            return Ok(String::new());
        }
        let rendered = attrs
            .iter()
            .map(|attr| match &attr.value {
                JSXAttrValue::String(value) => {
                    Ok(format!(r#"{}={}"#, attr.name, self.quote_string(value)))
                }
                JSXAttrValue::Expr(expr) => {
                    Ok(format!("{}={{{}}}", attr.name, self.format_expr(expr)?))
                }
                JSXAttrValue::Bool(value) => Ok(format!("{}={{{}}}", attr.name, value)),
                JSXAttrValue::Callback(event_kind, handler_expr) => {
                    Ok(format!(
                        "{}_{}={{{}}}",
                        event_kind,
                        attr.name,
                        self.format_expr(handler_expr.as_ref())?
                    ))
                }
            })
            .collect::<KainResult<Vec<_>>>()?
            .join(" ");
        Ok(format!(" {rendered}"))
    }

    fn format_state_decl(&self, value: &StateDecl) -> KainResult<String> {
        let mut output = String::new();
        self.push_attributes(&mut output, &value.attributes)?;
        let weak_prefix = if value.weak { "weak " } else { "" };
        output.push_str(&format!(
            "{weak_prefix}state {}: {} = {}",
            value.name,
            self.format_type(&value.ty),
            self.format_expr(&value.initial)?
        ));
        Ok(output)
    }

    fn format_field(&self, value: &Field) -> KainResult<String> {
        let mut output = String::new();
        self.push_attributes(&mut output, &value.attributes)?;
        output.push_str(&self.format_inline_field(value)?);
        Ok(output)
    }

    fn format_inline_field(&self, value: &Field) -> KainResult<String> {
        let mut line = String::new();
        if value.weak {
            line.push_str("weak ");
        }
        line.push_str(&value.name);
        line.push_str(": ");
        line.push_str(&self.format_type(&value.ty));
        if let Some(default) = &value.default {
            line.push_str(" = ");
            line.push_str(&self.format_expr(default)?);
        }
        Ok(line)
    }

    fn format_attribute(&self, value: &Attribute) -> KainResult<String> {
        if value.args.is_empty() {
            return Ok(format!("@{}", value.name));
        }
        Ok(format!(
            "@{}({})",
            value.name,
            value
                .args
                .iter()
                .map(|arg| self.format_attribute_arg(arg))
                .collect::<KainResult<Vec<_>>>()?
                .join(", ")
        ))
    }

    fn format_attribute_arg(&self, expr: &Expr) -> KainResult<String> {
        if let Expr::Tuple(parts, _) = expr {
            if parts.len() == 2 {
                if let Expr::Ident(name, _) = &parts[0] {
                    return Ok(format!("{name}: {}", self.format_expr(&parts[1])?));
                }
            }
        }
        self.format_expr(expr)
    }

    fn function_signature(
        &self,
        keyword: &str,
        visibility: Visibility,
        name: &str,
        generics: &[Generic],
        params: &[Param],
        return_type: Option<&Type>,
        where_clause: Option<&WhereClause>,
        effects: &[Effect],
    ) -> KainResult<String> {
        self.render_function_signature(
            keyword,
            visibility,
            name,
            generics,
            params,
            return_type,
            where_clause,
            effects,
        )
    }

    fn callable_signature(
        &self,
        keyword: &str,
        visibility: Visibility,
        name: &str,
        generics: &[Generic],
        params: &[Param],
        return_type: Option<&Type>,
        effects: &[Effect],
    ) -> KainResult<String> {
        self.render_function_signature(
            keyword,
            visibility,
            name,
            generics,
            params,
            return_type,
            None,
            effects,
        )
    }

    fn render_function_signature(
        &self,
        keyword: &str,
        visibility: Visibility,
        name: &str,
        generics: &[Generic],
        params: &[Param],
        return_type: Option<&Type>,
        where_clause: Option<&WhereClause>,
        effects: &[Effect],
    ) -> KainResult<String> {
        let prefix = format!(
            "{}{} {}{}(",
            self.visibility_prefix(visibility),
            keyword,
            name,
            self.format_generics(generics)
        );
        Ok(self.render_callable_head(
            &prefix,
            &self.format_param_parts(params)?,
            &self.render_callable_suffix(return_type, where_clause, effects),
        ))
    }

    fn format_param_parts(&self, params: &[Param]) -> KainResult<Vec<String>> {
        params.iter().map(|param| self.format_param(param)).collect()
    }

    fn format_param(&self, value: &Param) -> KainResult<String> {
        let mut output = String::new();
        if value.mutable {
            output.push_str("mut ");
        }
        output.push_str(&value.name);
        if !matches!(value.ty, Type::Infer(_)) {
            output.push_str(": ");
            output.push_str(&self.format_type(&value.ty));
        }
        if let Some(default) = &value.default {
            output.push_str(" = ");
            output.push_str(&self.format_expr(default)?);
        }
        Ok(output)
    }

    fn format_generics(&self, generics: &[Generic]) -> String {
        if generics.is_empty() {
            return String::new();
        }
        format!(
            "<{}>",
            generics
                .iter()
                .map(|generic| {
                    if generic.bounds.is_empty() {
                        generic.name.clone()
                    } else {
                        format!(
                            "{}: {}",
                            generic.name,
                            generic
                                .bounds
                                .iter()
                                .map(|bound| bound.trait_name.clone())
                                .collect::<Vec<_>>()
                                .join(" + ")
                        )
                    }
                })
                .collect::<Vec<_>>()
                .join(", ")
        )
    }

    fn push_where_clause(&self, output: &mut String, where_clause: Option<&WhereClause>) {
        if let Some(where_clause) = where_clause {
            output.push(' ');
            output.push_str(&self.format_where_clause(where_clause));
        }
    }

    fn format_where_clause(&self, where_clause: &WhereClause) -> String {
        format!(
            "where {}",
            where_clause
                .bounds
                .iter()
                .map(|bound| {
                    format!(
                        "{}: {}",
                        bound.generic_name,
                        bound
                            .bounds
                            .iter()
                            .map(|trait_bound| trait_bound.trait_name.clone())
                            .collect::<Vec<_>>()
                            .join(" + ")
                    )
                })
                .collect::<Vec<_>>()
                .join(", ")
        )
    }

    fn format_effects(&self, effects: &[Effect]) -> String {
        effects
            .iter()
            .map(|effect| match effect {
                Effect::Pure => "Pure",
                Effect::IO => "IO",
                Effect::Async => "Async",
                Effect::GPU => "GPU",
                Effect::Reactive => "Reactive",
                Effect::Unsafe => "Unsafe",
                Effect::Alloc => "Alloc",
                Effect::Panic => "Panic",
            })
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn format_pattern(&self, pattern: &Pattern) -> KainResult<String> {
        match pattern {
            Pattern::Wildcard(_) => Ok(String::from("_")),
            Pattern::Literal(expr) => self.format_expr(expr),
            Pattern::Binding { name, mutable, .. } => {
                if *mutable {
                    Ok(format!("mut {name}"))
                } else {
                    Ok(name.clone())
                }
            }
            Pattern::Struct {
                name, fields, rest, ..
            } => {
                let mut entries = fields
                    .iter()
                    .map(|(field, pattern)| {
                        Ok(format!("{field}: {}", self.format_pattern(pattern)?))
                    })
                    .collect::<KainResult<Vec<_>>>()?;
                if *rest {
                    entries.push(String::from(".."));
                }
                Ok(format!("{name} {{ {} }}", entries.join(", ")))
            }
            Pattern::Tuple(patterns, _) => Ok(format!(
                "({})",
                patterns
                    .iter()
                    .map(|pattern| self.format_pattern(pattern))
                    .collect::<KainResult<Vec<_>>>()?
                    .join(", ")
            )),
            Pattern::Variant {
                enum_name,
                variant,
                fields,
                ..
            } => {
                let head = if let Some(enum_name) = enum_name {
                    format!("{enum_name}::{variant}")
                } else {
                    variant.clone()
                };
                match fields {
                    VariantPatternFields::Unit => Ok(head),
                    VariantPatternFields::Tuple(patterns) => Ok(format!(
                        "{}({})",
                        head,
                        patterns
                            .iter()
                            .map(|pattern| self.format_pattern(pattern))
                            .collect::<KainResult<Vec<_>>>()?
                            .join(", ")
                    )),
                    VariantPatternFields::Struct(patterns) => Ok(format!(
                        "{} {{ {} }}",
                        head,
                        patterns
                            .iter()
                            .map(|(field, pattern)| {
                                Ok(format!("{field}: {}", self.format_pattern(pattern)?))
                            })
                            .collect::<KainResult<Vec<_>>>()?
                            .join(", ")
                    )),
                }
            }
            Pattern::Slice { patterns, rest, .. } => {
                let mut entries = patterns
                    .iter()
                    .map(|pattern| self.format_pattern(pattern))
                    .collect::<KainResult<Vec<_>>>()?;
                if let Some(rest) = rest {
                    entries.push(format!("{rest} @ .."));
                }
                Ok(format!("[{}]", entries.join(", ")))
            }
            Pattern::Or(patterns, _) => Ok(patterns
                .iter()
                .map(|pattern| self.format_pattern(pattern))
                .collect::<KainResult<Vec<_>>>()?
                .join(" | ")),
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                let start_text = start
                    .as_ref()
                    .map(|value| self.format_expr(value))
                    .transpose()?
                    .unwrap_or_default();
                let end_text = end
                    .as_ref()
                    .map(|value| self.format_expr(value))
                    .transpose()?
                    .unwrap_or_default();
                let marker = if *inclusive { "..=" } else { ".." };
                Ok(format!("{start_text}{marker}{end_text}"))
            }
        }
    }

    fn format_type(&self, ty: &Type) -> String {
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
                            .map(|generic| self.format_type(generic))
                            .collect::<Vec<_>>()
                            .join(", ")
                    )
                }
            }
            Type::Tuple(types, _) => format!(
                "({})",
                types
                    .iter()
                    .map(|ty| self.format_type(ty))
                    .collect::<Vec<_>>()
                    .join(", ")
            ),
            Type::Array(inner, size, _) => format!("[{}; {}]", self.format_type(inner), size),
            Type::Slice(inner, _) => format!("[{}]", self.format_type(inner)),
            Type::Ref {
                mutable,
                inner,
                lifetime,
                ..
            } => match (mutable, lifetime) {
                (true, Some(lifetime)) => format!("&mut {} {}", lifetime, self.format_type(inner)),
                (false, Some(lifetime)) => format!("&{} {}", lifetime, self.format_type(inner)),
                (true, None) => format!("&mut {}", self.format_type(inner)),
                (false, None) => format!("&{}", self.format_type(inner)),
            },
            Type::Ptr { mutable, inner, .. } => {
                if *mutable {
                    format!("ptr_mut<{}>", self.format_type(inner))
                } else {
                    format!("ptr<{}>", self.format_type(inner))
                }
            }
            Type::Function {
                params,
                return_type,
                effects,
                ..
            } => {
                let mut rendered = format!(
                    "fn({})",
                    params
                        .iter()
                        .map(|param| self.format_type(param))
                        .collect::<Vec<_>>()
                        .join(", ")
                );
                rendered.push_str(" -> ");
                rendered.push_str(&self.format_type(return_type));
                if !effects.is_empty() {
                    rendered.push_str(" with ");
                    rendered.push_str(&self.format_effects(effects));
                }
                rendered
            }
            Type::Option(inner, _) => format!("Option<{}>", self.format_type(inner)),
            Type::Result(ok, err, _) => {
                format!(
                    "Result<{}, {}>",
                    self.format_type(ok),
                    self.format_type(err)
                )
            }
            Type::Infer(_) => String::from("_"),
            Type::Never(_) => String::from("!"),
            Type::Unit(_) => String::from("()"),
            Type::Impl {
                trait_name,
                generics,
                ..
            } => {
                if generics.is_empty() {
                    format!("impl {trait_name}")
                } else {
                    format!(
                        "impl {}<{}>",
                        trait_name,
                        generics
                            .iter()
                            .map(|ty| self.format_type(ty))
                            .collect::<Vec<_>>()
                            .join(", ")
                    )
                }
            }
        }
    }

    fn format_string_array(&self, values: &[String]) -> String {
        format!(
            "[{}]",
            values
                .iter()
                .map(|value| self.quote_string(value))
                .collect::<Vec<_>>()
                .join(", ")
        )
    }

    fn format_f_string(&self, parts: &[Expr]) -> KainResult<String> {
        let mut output = String::from("f\"");
        for part in parts {
            match part {
                Expr::String(value, _) => output.push_str(&self.escape_f_string_text(value)),
                other => {
                    output.push('{');
                    output.push_str(&self.format_expr(other)?);
                    output.push('}');
                }
            }
        }
        output.push('"');
        Ok(output)
    }

    fn escape_f_string_text(&self, value: &str) -> String {
        let mut output = String::new();
        for ch in value.chars() {
            match ch {
                '\\' => output.push_str("\\\\"),
                '"' => output.push_str("\\\""),
                '\n' => output.push_str("\\n"),
                '\r' => output.push_str("\\r"),
                '\t' => output.push_str("\\t"),
                '{' => output.push_str("{{"),
                '}' => output.push_str("}}"),
                other => output.push(other),
            }
        }
        output
    }

    fn quote_string(&self, value: &str) -> String {
        format!("{value:?}")
    }

    fn format_f64(&self, value: f64) -> String {
        format!("{value:?}")
    }

    fn format_f32(&self, value: f32) -> String {
        format!("{value:?}")
    }

    fn binary_op_to_string(&self, op: BinaryOp) -> &'static str {
        match op {
            BinaryOp::Add => "+",
            BinaryOp::Sub => "-",
            BinaryOp::Mul => "*",
            BinaryOp::Div => "/",
            BinaryOp::Mod => "%",
            BinaryOp::Pow => "**",
            BinaryOp::Eq => "==",
            BinaryOp::Ne => "!=",
            BinaryOp::Lt => "<",
            BinaryOp::Gt => ">",
            BinaryOp::Le => "<=",
            BinaryOp::Ge => ">=",
            BinaryOp::And => "and",
            BinaryOp::Or => "or",
            BinaryOp::BitAnd => "&",
            BinaryOp::BitOr => "|",
            BinaryOp::BitXor => "^",
            BinaryOp::Shl => "<<",
            BinaryOp::Shr => ">>",
            BinaryOp::Assign => "=",
            BinaryOp::AddAssign => "+=",
            BinaryOp::SubAssign => "-=",
            BinaryOp::MulAssign => "*=",
            BinaryOp::DivAssign => "/=",
            BinaryOp::Range => "..",
            BinaryOp::RangeInclusive => "..=",
        }
    }

    fn unary_op_to_string(&self, op: UnaryOp) -> &'static str {
        match op {
            UnaryOp::Neg => "-",
            UnaryOp::Not => "!",
            UnaryOp::BitNot => "~",
            UnaryOp::Ref => "&",
            UnaryOp::RefMut => "&mut ",
            UnaryOp::Deref => "*",
        }
    }

    fn format_ordered_atomic_binary_call(
        &self,
        name: &str,
        pointer: &Expr,
        value: &Expr,
        op_ty: Option<&Type>,
        ordering: AtomicOrdering,
    ) -> KainResult<String> {
        Ok(match op_ty {
            Some(ty) => format!(
                "{name}({}, {}, {}, {})",
                self.format_expr(pointer)?,
                self.format_expr(value)?,
                self.quote_string(&self.format_type(ty)),
                self.quote_string(ordering.as_str())
            ),
            None => format!(
                "{name}({}, {}, {})",
                self.format_expr(pointer)?,
                self.format_expr(value)?,
                self.quote_string(ordering.as_str())
            ),
        })
    }

    fn format_header_with_block(&self, header: &str, block: &Block) -> KainResult<String> {
        self.format_header_with_body(header, &self.format_statement_sequence(&block.stmts)?)
    }

    fn format_header_with_body(&self, header: &str, body: &str) -> KainResult<String> {
        if body.trim().is_empty() {
            return Err(KainError::runtime(
                "Kain formatter cannot emit empty blocks in v1",
            ));
        }
        Ok(format!("{header}:\n{}", self.indent_text(body, 1)))
    }

    fn indent_text(&self, text: &str, levels: usize) -> String {
        let prefix = self.indent(levels);
        text.lines()
            .map(|line| {
                if line.is_empty() {
                    String::new()
                } else {
                    format!("{prefix}{line}")
                }
            })
            .collect::<Vec<_>>()
            .join("\n")
    }

    fn indent(&self, levels: usize) -> String {
        " ".repeat(self.options.indent_width * levels)
    }

    fn visibility_prefix(&self, visibility: Visibility) -> &'static str {
        match visibility {
            Visibility::Private => "",
            Visibility::Public | Visibility::Crate | Visibility::Super => "pub ",
        }
    }

    fn is_extern_function(&self, value: &Function) -> bool {
        value.attributes.iter().any(|attr| attr.name == "extern") && value.body.stmts.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::diagnostics::SpanMapper;
    use std::{fs, path::PathBuf};

    fn parse(source: &str) -> KainResult<Program> {
        let tokens = Lexer::new(source).tokenize()?;
        let mapper = SpanMapper::new(source);
        Parser::new(&tokens, &mapper, "<test>").parse()
    }

    fn format_with_width(source: &str, max_width: usize) -> KainResult<String> {
        format_source_with_options(
            source,
            FormatOptions {
                indent_width: 4,
                max_width,
            },
        )
    }

    #[test]
    fn formats_basic_functions_and_structs() {
        let source = r#"
pub fn add(left:Int,right:Int)->Int with Pure,IO:
    let sum=left+right
    return sum

struct Pair:
    left:Int
    right:Int=2
"#;

        let formatted = format_source(source).expect("format");
        let expected = r#"pub fn add(left: Int, right: Int) -> Int with Pure, IO:
    let sum = left + right
    return sum

struct Pair:
    left: Int
    right: Int = 2
"#;
        assert_eq!(formatted, expected);
        parse(&formatted).expect("formatted output should parse");
    }

    #[test]
    fn formats_c_include_with_alias_provenance() {
        let source = "include native/nuklear.h as nk\n";
        let formatted = format_source(source).expect("format");
        assert_eq!(formatted, "include nuklear as nk\n");
        parse(&formatted).expect("formatted include should parse");
    }

    #[test]
    fn keeps_script_mode_as_top_level_statements() {
        let source = r#"
let count=1
count=count+1
"#;

        let formatted = format_source(source).expect("format");
        assert_eq!(formatted, "let count = 1\ncount = count + 1\n");
        assert!(!formatted.contains("fn main"));
        parse(&formatted).expect("formatted script should parse");
    }

    #[test]
    fn formats_components_and_jsx() {
        let source = r#"
component App(title:String):
    state count:Int=0
    render<div title={title}>{if count>0: <Badge value={count} /> else: <Empty />}</div>
"#;

        let formatted = format_source(source).expect("format");
        let expected = r#"component App(title: String):
    state count: Int = 0
    render:
        <div title={title}>{if count > 0: <Badge value={count} /> else: <Empty />}</div>
"#;
        assert_eq!(formatted, expected);
        parse(&formatted).expect("formatted component should parse");
    }

    #[test]
    fn formats_gameplay_tags_namespace() {
        let source = r#"
@gameplay_tags
namespace Ability:
    Attack:
        Melee
"#;

        let formatted = format_source(source).expect("format");
        let expected = "@gameplay_tags\nnamespace Ability:\n    Attack:\n        Melee\n";
        assert_eq!(formatted, expected);
        parse(&formatted).expect("formatted gameplay tags should parse");
    }

    #[test]
    fn preserves_shebang() {
        let source = "#!/usr/bin/env kn\nlet value=1\n";
        let formatted = format_source(source).expect("format");
        assert_eq!(formatted, "#!/usr/bin/env kn\nlet value = 1\n");
    }

    #[test]
    fn wraps_long_signatures_and_calls_with_max_width() {
        let source = r#"
pub fn render_entity(world:WorldState,entity:VeryLongEntityHandle,shader:GpuPipeline)->RenderResult with Pure,GPU:
    return draw_entity(world,entity,shader,"01234567890123456789")
"#;

        let formatted = format_with_width(source, 48).expect("format");
        let expected = r#"pub fn render_entity(
    world: WorldState,
    entity: VeryLongEntityHandle,
    shader: GpuPipeline
) -> RenderResult with Pure, GPU:
    return draw_entity(
        world,
        entity,
        shader,
        "01234567890123456789"
    )
"#;
        assert_eq!(formatted, expected);
        parse(&formatted).expect("wrapped callable output should parse");
    }

    #[test]
    fn wraps_structs_arrays_and_tuples_with_max_width() {
        let source = r#"
fn demo():
    let packet=RenderPacket{label:"01234567890123456789",shader:resolve_shader(shader_bank,entity_name),pair:(left_signal,right_signal),items:[first_signal,second_signal,third_signal],single:(solo_signal,)}
"#;

        let formatted = format_with_width(source, 32).expect("format");
        let expected = r#"fn demo():
    let packet = RenderPacket {
        label: "01234567890123456789",
        shader: resolve_shader(
            shader_bank,
            entity_name
        ),
        pair: (left_signal, right_signal),
        items: [
            first_signal,
            second_signal,
            third_signal
        ],
        single: (solo_signal,)
    }
"#;
        assert_eq!(formatted, expected);
        parse(&formatted).expect("wrapped literal output should parse");
    }

    #[test]
    fn wraps_tuple_literals_when_they_exceed_max_width() {
        let source = r#"
fn demo():
    let pair=(left_signal,right_signal)
"#;

        let formatted = format_with_width(source, 24).expect("format");
        let expected = r#"fn demo():
    let pair = (
        left_signal,
        right_signal
    )
"#;
        assert_eq!(formatted, expected);
        parse(&formatted).expect("wrapped tuple output should parse");
    }

    #[test]
    fn formatter_is_idempotent_for_real_repo_sources() {
        let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .expect("crate parent")
            .parent()
            .expect("repo root")
            .to_path_buf();

        for relative in ["stdlib/fmt.kn", "stdlib/intent.kn", "stdlib/os.kn"] {
            let source = fs::read_to_string(repo_root.join(relative))
                .unwrap_or_else(|err| panic!("failed to read {relative}: {err}"));
            let once = format_source(&source)
                .unwrap_or_else(|err| panic!("failed to format {relative}: {err}"));
            parse(&once)
                .unwrap_or_else(|err| panic!("formatted {relative} should parse: {err}"));
            let twice = format_source(&once)
                .unwrap_or_else(|err| panic!("failed to reformat {relative}: {err}"));
            assert_eq!(twice, once, "formatter should be idempotent for {relative}");
        }
    }
}
