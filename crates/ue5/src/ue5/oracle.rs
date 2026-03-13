// Copyright 2026 Zentako. All Rights Reserved.
// Unreal Semantic Validator - "The Oracle"
//
// This module validates KAIN code against Unreal Engine's semantic rules
// BEFORE generating C++. Catches UHT errors in 10ms instead of 2 minutes.
//
// Based on Epic's UHT source:
// - EpicGames.UHT/Specifiers/UhtFunctionSpecifiers.cs
// - EpicGames.UHT/Specifiers/UhtPropertyMemberSpecifiers.cs

use super::engine_knowledge::EngineKnowledge;
use super::naming::{
    to_actor_name, to_component_name, to_enum_name, to_struct_name, to_uobject_name,
};
use super::uht_rules::UhtRules;
use super::validation_rules::{RuleCondition, Severity, ValidationRule, ValidationRules};
use kain_core::ast::Visibility;
use kain_core::ast::{Attribute, Type};
use kain_core::diagnostics::SpanMapper;
use kain_core::error::{KainError, KainResult};
use kain_core::span::Span;
use kain_core::types::{
    TypedActor, TypedComponent, TypedFunction, TypedItem, TypedProgram, TypedStruct,
};
use std::collections::{HashMap, HashSet};

/// Validation context for tracking state during validation
pub struct ValidationContext<'a> {
    pub errors: Vec<String>,
    pub warnings: Vec<String>,
    span_mapper: &'a SpanMapper,
    filename: &'a str,
}

impl<'a> ValidationContext<'a> {
    pub fn new(span_mapper: &'a SpanMapper, filename: &'a str) -> Self {
        Self {
            errors: Vec::new(),
            warnings: Vec::new(),
            span_mapper,
            filename,
        }
    }

    /// Report an error with file:line:col format
    pub fn error(&mut self, msg: String) {
        self.errors.push(msg);
    }

    /// Report an error with span information in file:line:col format
    pub fn error_with_span(&mut self, msg: String, span: Span) {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted = format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, msg);
        self.errors.push(formatted);
    }

    /// Report a warning with file:line:col format
    pub fn warning(&mut self, msg: String) {
        self.warnings.push(msg);
    }

    /// Report a warning with span information in file:line:col format
    pub fn warning_with_span(&mut self, msg: String, span: Span) {
        let loc = self.span_mapper.span_to_location(span, self.filename);
        let formatted = format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, msg);
        self.warnings.push(formatted);
    }

    pub fn has_errors(&self) -> bool {
        !self.errors.is_empty()
    }

    pub fn report(&self) -> String {
        let mut report = String::new();

        if !self.errors.is_empty() {
            report.push_str("❌ Unreal Semantic Validation Errors:\n");
            for (i, error) in self.errors.iter().enumerate() {
                report.push_str(&format!("   {}. {}\n", i + 1, error));
            }
        }

        if !self.warnings.is_empty() {
            report.push_str("\n⚠️  Warnings:\n");
            for (i, warning) in self.warnings.iter().enumerate() {
                report.push_str(&format!("   {}. {}\n", i + 1, warning));
            }
        }

        report
    }
}

/// Main validation entry point - runs BEFORE C++ codegen
pub fn validate_program(
    program: &TypedProgram,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<()> {
    let kb = EngineKnowledge::new();
    validate_program_with_knowledge(program, &kb, span_mapper, filename)
}

/// Validation with explicit EngineKnowledge (used when context already has one)
pub fn validate_program_with_knowledge(
    program: &TypedProgram,
    kb: &EngineKnowledge,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<()> {
    let uht = UhtRules::new();
    validate_program_full(program, kb, &uht, span_mapper, filename)
}

/// Full validation with EngineKnowledge + UHT rules (used when Ue5Context is available)
pub fn validate_program_full(
    program: &TypedProgram,
    kb: &EngineKnowledge,
    uht: &UhtRules,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<()> {
    // Load custom validation rules (if available)
    let custom_rules = ValidationRules::load("unreal/metadata/validation_rules.json")
        .unwrap_or_else(|_| ValidationRules {
            version: "1.0.0".to_string(),
            rules: Vec::new(),
        });

    validate_program_with_custom_rules(program, kb, uht, &custom_rules, span_mapper, filename)
}

/// Full validation with custom rules support
pub fn validate_program_with_custom_rules(
    program: &TypedProgram,
    kb: &EngineKnowledge,
    uht: &UhtRules,
    custom_rules: &ValidationRules,
    span_mapper: &SpanMapper,
    filename: &str,
) -> KainResult<()> {
    let mut ctx = ValidationContext::new(span_mapper, filename);

    // Check for rule conflicts before validation
    let conflicts = custom_rules.detect_conflicts();
    if !conflicts.is_empty() {
        for (rule1, rule2, reason) in conflicts {
            ctx.error(format!(
                "Conflicting validation rules '{}' and '{}': {}",
                rule1, rule2, reason
            ));
        }
        return Err(KainError::runtime(ctx.report()));
    }

    // Phase 1: Per-item validation (existing hardcoded rules)
    for item in &program.items {
        match item {
            TypedItem::Function(func) => validate_function(&mut ctx, func),
            TypedItem::Actor(actor) => validate_actor(&mut ctx, actor, kb),
            TypedItem::Struct(struct_def) => validate_struct(&mut ctx, struct_def, kb),
            TypedItem::Component(comp) => validate_component(&mut ctx, comp, kb),
            TypedItem::Enum(en) => validate_enum(&mut ctx, en, kb),
            _ => {}
        }
    }

    // Phase 2: Data-driven UHT validation (from uht_rules.json)
    if uht.is_loaded() {
        for item in &program.items {
            match item {
                TypedItem::Actor(actor) => validate_actor_uht(&mut ctx, actor, uht),
                TypedItem::Struct(struct_def) => validate_struct_uht(&mut ctx, struct_def, uht),
                TypedItem::Component(comp) => validate_component_uht(&mut ctx, comp, uht),
                _ => {}
            }
        }
    }

    // Phase 3: Enhanced validation (Phase 3 of robustness spec)
    validate_replication(&mut ctx, program);
    validate_rpcs(&mut ctx, program);
    validate_datatables(&mut ctx, program, kb);
    validate_components_enhanced(&mut ctx, program);
    validate_name_collisions(&mut ctx, program, kb);
    validate_circular_dependencies(&mut ctx, program);

    // Phase 4: Custom rules validation (data-driven from validation_rules.json)
    enforce_custom_rules(&mut ctx, program, custom_rules, kb);

    // If we have errors, return them
    if ctx.has_errors() {
        return Err(KainError::runtime(ctx.report()));
    }

    // Silently collect warnings without printing

    Ok(())
}

/// Validate function specifiers (UFUNCTION rules)
fn validate_function(ctx: &mut ValidationContext, func: &TypedFunction) {
    let func_name = &func.ast.name;
    let func_span = func.ast.span;
    let mut flags = FunctionFlags::new();

    // Parse function attributes/specifiers from AST
    // Check for RPC naming convention
    if func_name.starts_with("Server_") {
        flags.net = true;
        flags.net_server = true;
    } else if func_name.starts_with("Client_") {
        flags.net = true;
        flags.net_client = true;
    } else if func_name.starts_with("Multicast_") {
        flags.net = true;
        flags.net_multicast = true;
    }

    // Check for @blueprint attribute
    for attr in &func.ast.attributes {
        match attr.name.as_str() {
            "blueprint" => {
                flags.blueprint_callable = true;
            }
            "blueprint_pure" => {
                flags.blueprint_callable = true;
                flags.blueprint_pure = true;
            }
            "blueprint_event" => {
                flags.blueprint_event = true;
            }
            "blueprint_implementable_event" => {
                flags.blueprint_event = true;
                flags.blueprint_implementable = true;
            }
            "blueprint_native_event" => {
                flags.blueprint_event = true;
                flags.blueprint_native = true;
            }
            "blueprint_getter" => {
                flags.blueprint_getter = true;
            }
            "blueprint_setter" => {
                flags.blueprint_setter = true;
            }
            _ => {}
        }
    }

    // RULE: Private functions cannot be BlueprintImplementableEvent or BlueprintNativeEvent
    let is_private = func.ast.visibility == Visibility::Private;
    if is_private && flags.blueprint_event {
        ctx.error_with_span(format!(
            "Function '{}': A Private function cannot be a BlueprintImplementableEvent or BlueprintNativeEvent.",
            func_name
        ), func_span);
    }

    // RULE: BlueprintEvent cannot be a BlueprintGetter
    if flags.blueprint_event && flags.blueprint_getter {
        ctx.error_with_span(
            format!(
                "Function '{}': Function cannot be a blueprint event and a blueprint getter.",
                func_name
            ),
            func_span,
        );
    }

    // RULE 1: BlueprintImplementableEvent cannot be replicated
    if flags.blueprint_implementable && flags.net {
        ctx.error_with_span(format!(
            "Function '{}': BlueprintImplementableEvent functions cannot be replicated (Server/Client/Multicast)",
            func_name
        ), func_span);
    }

    // RULE 2: BlueprintNativeEvent cannot be replicated
    if flags.blueprint_native && flags.net {
        ctx.error_with_span(format!(
            "Function '{}': BlueprintNativeEvent functions cannot be replicated (Server/Client/Multicast)",
            func_name
        ), func_span);
    }

    // RULE 3: Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent
    if flags.blueprint_implementable && flags.blueprint_native {
        ctx.error_with_span(
            format!(
            "Function '{}': Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent",
            func_name
        ),
            func_span,
        );
    }

    // RULE 4: Exec functions cannot be replicated
    if flags.exec && flags.net {
        ctx.error_with_span(
            format!(
                "Function '{}': Exec functions cannot be replicated",
                func_name
            ),
            func_span,
        );
    }

    // RULE: RigVM methods cannot have parameters (UE 5.2+ Rule)
    let is_rigvm = func.ast.attributes.iter().any(|a| a.name == "rigvm_method");
    if is_rigvm && !func.ast.params.is_empty() {
        ctx.error(format!(
            "Function '{}': RIGVM_METHOD functions cannot have parameters in UE 5.2+. Use the struct state instead.",
            func_name
        ));
    }

    // RULE: Replicated functions cannot have delegate parameters (UhtDelegateProperty.cs:158)
    if flags.net {
        for param in &func.ast.params {
            if is_delegate_type(&param.ty) {
                ctx.error(format!(
                    "Function '{}', parameter '{}': Replicated functions (Server/Client/Multicast) cannot have delegate parameters. This is a security/stability restriction.",
                    func_name, param.name
                ));
            }
        }
    }
}

/// Validate actor-specific rules
fn validate_actor(ctx: &mut ValidationContext, actor: &TypedActor, kb: &EngineKnowledge) {
    let actor_name = &actor.ast.name;

    // Validate actor-level attributes
    let mut has_blueprint_implementable = false;
    let mut has_blueprint_native = false;

    for attr in &actor.ast.attributes {
        match attr.name.as_str() {
            "blueprint_implementable_event" => has_blueprint_implementable = true,
            "blueprint_native_event" => has_blueprint_native = true,
            _ => {}
        }
    }

    // RULE: Actor Naming Prefix (A)
    let engine_name = crate::ue5::naming::to_actor_name(actor_name);
    if engine_name.len() <= 1 {
        ctx.error(format!(
            "Actor '{}': Resulting engine name '{}' is invalid. Ensure you haven't used an empty or numeric name.",
            actor_name, engine_name
        ));
    }

    // RULE: Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent
    if has_blueprint_implementable && has_blueprint_native {
        ctx.error(format!(
            "Actor '{}': Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent",
            actor_name
        ));
    }

    // Validate actor state fields
    for state in &actor.ast.state {
        validate_property(ctx, &state.name, &actor_name, false, &state.ty, &[], kb);
    }

    // Validate message handlers (RPCs)
    for handler in &actor.ast.handlers {
        let handler_name = &handler.message_type;

        // Check if this is an RPC (Server_, Client_, Multicast_)
        let is_server_rpc = handler_name.starts_with("Server_");
        let is_client_rpc = handler_name.starts_with("Client_");
        let is_multicast_rpc = handler_name.starts_with("Multicast_");
        let is_any_rpc = is_server_rpc || is_client_rpc || is_multicast_rpc;

        // RULE: BlueprintImplementableEvent cannot be replicated
        if has_blueprint_implementable && is_any_rpc {
            ctx.error(format!(
                "Actor '{}', handler '{}': BlueprintImplementableEvent functions cannot be replicated (Server/Client/Multicast)",
                actor_name, handler_name
            ));
        }

        // RULE: BlueprintNativeEvent cannot be replicated
        if has_blueprint_native && is_any_rpc {
            ctx.error(format!(
                "Actor '{}', handler '{}': BlueprintNativeEvent functions cannot be replicated (Server/Client/Multicast)",
                actor_name, handler_name
            ));
        }
    }

    // Validate actor methods
    for method in &actor.ast.methods {
        // Check method-level attributes
        let mut method_has_blueprint_implementable = false;
        let mut method_has_blueprint_native = false;
        let mut method_has_exec = false;

        for attr in &method.attributes {
            match attr.name.as_str() {
                "blueprint_implementable_event" => method_has_blueprint_implementable = true,
                "blueprint_native_event" => method_has_blueprint_native = true,
                "exec" => method_has_exec = true,
                _ => {}
            }
        }

        // Check if method name indicates RPC
        let is_server_rpc = method.name.starts_with("Server_");
        let is_client_rpc = method.name.starts_with("Client_");
        let is_multicast_rpc = method.name.starts_with("Multicast_");
        let is_any_rpc = is_server_rpc || is_client_rpc || is_multicast_rpc;

        // RULE: BlueprintImplementableEvent cannot be replicated
        if method_has_blueprint_implementable && is_any_rpc {
            ctx.error(format!(
                "Actor '{}', method '{}': BlueprintImplementableEvent functions cannot be replicated (Server/Client/Multicast)",
                actor_name, method.name
            ));
        }

        // RULE: BlueprintNativeEvent cannot be replicated
        if method_has_blueprint_native && is_any_rpc {
            ctx.error(format!(
                "Actor '{}', method '{}': BlueprintNativeEvent functions cannot be replicated (Server/Client/Multicast)",
                actor_name, method.name
            ));
        }

        // RULE: Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent
        if method_has_blueprint_implementable && method_has_blueprint_native {
            ctx.error(format!(
                "Actor '{}', method '{}': Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent",
                actor_name, method.name
            ));
        }

        // RULE: Exec functions cannot be replicated
        if method_has_exec && is_any_rpc {
            ctx.error(format!(
                "Actor '{}', method '{}': Exec functions cannot be replicated",
                actor_name, method.name
            ));
        }
    }
}

/// Validate struct-specific rules
fn validate_struct(ctx: &mut ValidationContext, struct_def: &TypedStruct, kb: &EngineKnowledge) {
    let struct_name = &struct_def.ast.name;

    // Validate struct fields
    for field in &struct_def.ast.fields {
        validate_property(
            ctx,
            &field.name,
            &struct_name,
            true,
            &field.ty,
            &field.attributes,
            kb,
        );
    }

    // RULE: Struct Naming Prefix (F)
    let engine_name = to_struct_name(struct_name);
    if engine_name.len() <= 1 {
        ctx.error(format!(
            "Struct '{}': Resulting engine name '{}' is too short or empty. Use a more descriptive name.",
            struct_name, engine_name
        ));
    }

    // RULE: Check for name collisions with known UE5 engine types (powered by EngineKnowledge)
    check_engine_name_collision(ctx, struct_name, "Struct", kb);
}

/// Validate enum-specific rules
fn validate_enum(
    ctx: &mut ValidationContext,
    enum_def: &kain_core::types::TypedEnum,
    kb: &EngineKnowledge,
) {
    let enum_name = &enum_def.ast.name;

    // RULE: Enum variants cannot be named 'true' or 'false' (case-insensitive)
    for variant in &enum_def.ast.variants {
        let variant_lower = variant.name.to_lowercase();
        if variant_lower == "true" || variant_lower == "false" {
            ctx.error(format!(
                "Enum '{}', variant '{}': Enumerations cannot have variants named 'true' or 'false' (case-insensitive). This is a UE5 restriction.",
                enum_name, variant.name
            ));
        }
    }

    // RULE (UhtEnum.cs): Every UE5 Enum should have a _MAX entry for metadata
    let has_max = enum_def
        .ast
        .variants
        .iter()
        .any(|v| v.name.to_uppercase().ends_with("MAX"));
    if !has_max {
        ctx.warning(format!(
            "Enum '{}': Missing a '_MAX' variant. Unreal Engine metadata systems (and Blueprints) often require a MAX entry for stability.",
            enum_name
        ));
    }

    // RULE: Enum Naming Prefix (E)
    let engine_name = to_enum_name(enum_name);
    if engine_name.len() <= 1 {
        ctx.error(format!(
            "Enum '{}': Resulting engine name '{}' is invalid.",
            enum_name, engine_name
        ));
    }

    // RULE: Check for name collisions with known UE5 engine types (powered by EngineKnowledge)
    check_engine_name_collision(ctx, enum_name, "Enum", kb);
}

fn validate_component(ctx: &mut ValidationContext, comp: &TypedComponent, kb: &EngineKnowledge) {
    let comp_name = &comp.ast.name;

    // RULE: Component Naming Prefix (U)
    let engine_name = to_component_name(comp_name);
    if engine_name.len() <= 1 {
        ctx.error(format!(
            "Component '{}': Resulting engine name '{}' is invalid.",
            comp_name, engine_name
        ));
    }

    // Validate component state fields
    for state in &comp.ast.state {
        validate_property(ctx, &state.name, &comp_name, false, &state.ty, &[], kb);
    }
}

/// Validate property specifiers (UPROPERTY rules)
fn validate_property(
    ctx: &mut ValidationContext,
    prop_name: &str,
    owner_name: &str,
    is_struct_member: bool,
    ty: &Type,
    attributes: &[Attribute],
    _kb: &EngineKnowledge,
) {
    let mut is_replicated = false;
    let mut is_blueprint_read_only = false;
    let mut is_blueprint_setter = false;
    let mut has_uproperty = false;

    for attr in attributes {
        match attr.name.as_str() {
            "replicated" => is_replicated = true,
            "blueprint_read_only" => is_blueprint_read_only = true,
            "blueprint_setter" => is_blueprint_setter = true,
            "property" => has_uproperty = true,
            _ => {}
        }
        // Common UE5 attributes often imply UPROPERTY intent
        if matches!(
            attr.name.as_str(),
            "edit_anywhere" | "visible_anywhere" | "replicated"
        ) {
            has_uproperty = true;
        }
    }

    // RULE: Struct members cannot be replicated
    if is_struct_member && is_replicated {
        ctx.error(format!(
            "Property '{}' in Struct '{}': Struct members cannot be replicated.",
            prop_name, owner_name
        ));
    }

    // RULE: Cannot be both BlueprintReadOnly and have a BlueprintSetter
    if is_blueprint_read_only && is_blueprint_setter {
        ctx.error(format!(
            "Property '{}' in '{}': Cannot specify a property as being both BlueprintReadOnly and having a BlueprintSetter.",
            prop_name, owner_name
        ));
    }

    // 🚨 GC WATCHDOG: Raw pointers to Actors/Components MUST be UPROPERTY
    // Note: We can't check for ActorRef/ComponentRef as those are typed program types
    // This validation would need to be enhanced with type information
    // For now, we skip this check as it requires typed program context
}

/// Function flags tracker (mirrors UE5's EFunctionFlags)
struct FunctionFlags {
    net: bool,
    net_server: bool,
    net_client: bool,
    net_multicast: bool,
    blueprint_callable: bool,
    blueprint_pure: bool,
    blueprint_event: bool,
    blueprint_implementable: bool,
    blueprint_native: bool,
    blueprint_getter: bool,
    blueprint_setter: bool,
    exec: bool,
}

impl FunctionFlags {
    fn new() -> Self {
        Self {
            net: false,
            net_server: false,
            net_client: false,
            net_multicast: false,
            blueprint_callable: false,
            blueprint_pure: false,
            blueprint_event: false,
            blueprint_implementable: false,
            blueprint_native: false,
            blueprint_getter: false,
            blueprint_setter: false,
            exec: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::diagnostics::SpanMapper;
    use kain_core::span::Span;

    #[test]
    fn test_validation_context() {
        let source = "test source";
        let span_mapper = SpanMapper::new(source);
        let mut ctx = ValidationContext::new(&span_mapper, "test.kn");
        assert!(!ctx.has_errors());

        ctx.error("Test error".to_string());
        assert!(ctx.has_errors());

        let report = ctx.report();
        assert!(report.contains("Test error"));
    }

    #[test]
    fn test_validation_context_with_span() {
        let source = "line1\nline2\nline3";
        let span_mapper = SpanMapper::new(source);
        let mut ctx = ValidationContext::new(&span_mapper, "test.kn");

        // Error on line 2 (starts at byte 6)
        let span = Span::new(6, 11);
        ctx.error_with_span("Test error on line 2".to_string(), span);

        assert!(ctx.has_errors());
        let report = ctx.report();

        // Should contain file:line:col format
        assert!(
            report.contains("test.kn:2:1:"),
            "Report should contain 'test.kn:2:1:' but got: {}",
            report
        );
        assert!(report.contains("Test error on line 2"));
    }
}

// ═══════════════════════════════════════════════════════════════════
// DATA-DRIVEN UHT VALIDATION (Phase 2)
// Uses rules extracted from Epic's EpicGames.UHT C# source
// ═══════════════════════════════════════════════════════════════════

/// Validate actor state fields against UHT property type rules
fn validate_actor_uht(ctx: &mut ValidationContext, actor: &TypedActor, uht: &UhtRules) {
    for state in &actor.ast.state {
        validate_property_type_uht(
            ctx,
            &state.name,
            &actor.ast.name,
            &state.ty,
            &[],
            false,
            uht,
        );
    }
}

/// Validate struct fields against UHT property type rules
fn validate_struct_uht(ctx: &mut ValidationContext, struct_def: &TypedStruct, uht: &UhtRules) {
    for field in &struct_def.ast.fields {
        validate_property_type_uht(
            ctx,
            &field.name,
            &struct_def.ast.name,
            &field.ty,
            &field.attributes,
            true,
            uht,
        );
    }
}

/// Validate component state fields against UHT property type rules
fn validate_component_uht(ctx: &mut ValidationContext, comp: &TypedComponent, uht: &UhtRules) {
    for state in &comp.ast.state {
        validate_property_type_uht(ctx, &state.name, &comp.ast.name, &state.ty, &[], false, uht);
    }
}

/// Data-driven property type validation using UHT rules
fn validate_property_type_uht(
    ctx: &mut ValidationContext,
    prop_name: &str,
    owner_name: &str,
    ty: &Type,
    attributes: &[Attribute],
    is_struct_member: bool,
    uht: &UhtRules,
) {
    // Check for nested container types (UHT rejects these)
    // e.g., TMap<FString, TArray<int>> is invalid
    if let Type::Named { name, generics, .. } = ty {
        let type_name = map_kain_container(name);
        if uht.is_container_type(&type_name) {
            for param in generics {
                if let Type::Named {
                    name: inner_name, ..
                } = param
                {
                    let inner_type = map_kain_container(inner_name);
                    if uht.is_container_type(&inner_type) {
                        ctx.error(format!(
                            "Property '{}' in '{}': Nested containers are not supported by UHT. \
                            '{}<..., {}<...>>' will fail UE5 compilation. Use a wrapper struct instead.",
                            prop_name, owner_name, type_name, inner_type
                        ));
                    }
                }
            }
        }
    }

    // Check specifier compatibility using UHT incompatible combos
    let mut seen_specifiers: Vec<String> = Vec::new();
    for attr in attributes {
        let spec_name = map_kain_attr_to_specifier(&attr.name);
        if !spec_name.is_empty() {
            // Check against all previously seen specifiers
            for prev in &seen_specifiers {
                if let Some(msg) = uht.are_incompatible(&spec_name, prev) {
                    ctx.error(format!(
                        "Property '{}' in '{}': {} (UHT rule)",
                        prop_name, owner_name, msg
                    ));
                }
            }
            seen_specifiers.push(spec_name);
        }
    }

    // UHT rule: BlueprintSetter/BlueprintGetter cannot be used on struct members
    if is_struct_member {
        for attr in attributes {
            if attr.name == "blueprint_setter" {
                ctx.error(format!(
                    "Property '{}' in struct '{}': Cannot specify BlueprintSetter for a struct member. \
                    This is only valid on class (actor/component) members. (UHT rule)",
                    prop_name, owner_name
                ));
            }
            if attr.name == "blueprint_getter" {
                ctx.error(format!(
                    "Property '{}' in struct '{}': Cannot specify BlueprintGetter for a struct member. \
                    This is only valid on class (actor/component) members. (UHT rule)",
                    prop_name, owner_name
                ));
            }
        }
    }

    // UHT rule: editor_only properties in Blueprint-exposed structs are invalid
    let is_blueprint_exposed = attributes
        .iter()
        .any(|a| a.name == "blueprint_read_write" || a.name == "blueprint_read_only");
    let is_editor_only = attributes.iter().any(|a| a.name == "editor_only");
    if is_struct_member && is_blueprint_exposed && is_editor_only {
        ctx.error(format!(
            "Property '{}' in struct '{}': Blueprint exposed struct members cannot be editor only. (UHT rule)",
            prop_name, owner_name
        ));
    }
}

/// Map KAIN container type names to UHT type names
fn map_kain_container(name: &str) -> String {
    match name {
        "Array" | "TArray" => "Array".to_string(),
        "Map" | "TMap" => "Map".to_string(),
        "Set" | "TSet" => "Set".to_string(),
        "Optional" | "TOptional" => "Optional".to_string(),
        _ => name.to_string(),
    }
}

/// Map KAIN attribute names to UHT specifier names for compatibility checking
fn map_kain_attr_to_specifier(attr_name: &str) -> String {
    match attr_name {
        "blueprint_read_write" => "BlueprintReadWrite".to_string(),
        "blueprint_read_only" => "BlueprintReadOnly".to_string(),
        "blueprint_setter" => "BlueprintSetter".to_string(),
        "blueprint_getter" => "BlueprintGetter".to_string(),
        "edit_anywhere" => "EditAnywhere".to_string(),
        "edit_defaults_only" => "EditDefaultsOnly".to_string(),
        "edit_instance_only" => "EditInstanceOnly".to_string(),
        "visible_anywhere" => "VisibleAnywhere".to_string(),
        "visible_defaults_only" => "VisibleDefaultsOnly".to_string(),
        "visible_instance_only" => "VisibleInstanceOnly".to_string(),
        "replicated" => "Replicated".to_string(),
        "transient" => "Transient".to_string(),
        "savegame" => "SaveGame".to_string(),
        "config" => "Config".to_string(),
        "interp" => "Interp".to_string(),
        _ => String::new(),
    }
}

/// Helper: Check if a type is a delegate
fn is_delegate_type(ty: &Type) -> bool {
    // Check if type name contains "Delegate" or matches delegate pattern
    match ty {
        Type::Named { name, .. } => name.to_lowercase().contains("delegate"),
        _ => false,
    }
}

/// Helper: Check for name collisions with known UE5 engine types
/// Uses EngineKnowledge for comprehensive collision detection instead of a hardcoded list.
/// This prevents UHT errors like "shares engine name with class/struct in Engine"
fn check_engine_name_collision(
    ctx: &mut ValidationContext,
    type_name: &str,
    type_kind: &str,
    kb: &EngineKnowledge,
) {
    let emitted_name = match type_kind {
        "Actor" => to_actor_name(type_name),
        "Component" => to_component_name(type_name),
        "Struct" => to_struct_name(type_name),
        "Enum" => to_enum_name(type_name),
        "UObject" => to_uobject_name(type_name),
        _ => type_name.to_string(),
    };

    let emitted_engine_name = strip_ue_prefix(&emitted_name);

    // If the emitted name is already safe, validation should not reject the source name.
    let emitted_collides = kb.is_known_type(&emitted_name)
        || kb.is_known_type(emitted_engine_name)
        || kb.resolve_type_alias(emitted_engine_name).is_some();

    if emitted_collides {
        ctx.warning(format!(
            "{} '{}': This name may collide with a UE5 engine type. The UE5 naming layer should remap hard collisions, \
            but this name deserves review if generated code still trips UHT. Consider a more specific name (e.g., 'My{}', 'Custom{}', 'Game{}').",
            type_kind, type_name, type_name, type_name, type_name
        ));
    }
}

fn strip_ue_prefix(name: &str) -> &str {
    if let Some(first) = name.chars().next() {
        if matches!(first, 'A' | 'U' | 'F' | 'E' | 'I')
            && name.chars().nth(1).map_or(false, |c| c.is_uppercase())
        {
            return &name[1..];
        }
    }
    name
}

// ═══════════════════════════════════════════════════════════════════
// PHASE 3: ENHANCED ORACLE VALIDATION
// Implements Requirements 3.1-3.12 from kain-pipeline-robustness spec
// ═══════════════════════════════════════════════════════════════════

/// Task 4.1: Validate replication setup
/// Requirement 3.1: Verify GetLifetimeReplicatedProps will be generated
/// Requirement 3.2: Verify RPC naming conventions and parameter serialization
fn validate_replication(ctx: &mut ValidationContext, program: &TypedProgram) {
    // Collect all enum names from the program for serialization checking
    let enum_names: std::collections::HashSet<String> = program
        .items
        .iter()
        .filter_map(|item| {
            if let TypedItem::Enum(e) = item {
                Some(e.ast.name.clone())
            } else {
                None
            }
        })
        .collect();

    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            let actor_name = &actor.ast.name;
            let mut has_replicated_props = false;

            // Check actor state for @replicated attributes
            for state in &actor.ast.state {
                let is_replicated = state.attributes.iter().any(|a| a.name == "replicated");
                if is_replicated {
                    has_replicated_props = true;

                    // Validate that the type is serializable
                    if !is_serializable_type(&state.ty, &enum_names) {
                        ctx.error(format!(
                            "Actor '{}', property '{}': Replicated properties must be UE5-serializable. \
                            Type '{:?}' cannot be replicated. Use primitives, structs, enums, or UObject pointers.",
                            actor_name, state.name, state.ty
                        ));
                    }
                }
            }

            // Note: GetLifetimeReplicatedProps is auto-generated by codegen_ue5.rs
            // We just validate that IF there are replicated props, the generation will work
            if has_replicated_props {
                // Validation passed - codegen will generate GetLifetimeReplicatedProps
                // No error needed here as the generation is automatic
            }
        }

        if let TypedItem::Component(comp) = item {
            let comp_name = &comp.ast.name;

            // Check component state for @replicated attributes
            for state in &comp.ast.state {
                let is_replicated = state.attributes.iter().any(|a| a.name == "replicated");
                if is_replicated {
                    // Validate that the type is serializable
                    if !is_serializable_type(&state.ty, &enum_names) {
                        ctx.error(format!(
                            "Component '{}', property '{}': Replicated properties must be UE5-serializable. \
                            Type '{:?}' cannot be replicated.",
                            comp_name, state.name, state.ty
                        ));
                    }
                }
            }
        }
    }
}

/// Task 4.2: Validate RPC naming and parameters
/// Requirement 3.2: Verify Server_*, Client_*, Multicast_* naming
/// Requirement 3.2: Validate RPC parameter types are serializable
fn validate_rpcs(ctx: &mut ValidationContext, program: &TypedProgram) {
    // Collect all enum and struct names from the program for serialization checking
    // User-defined enums and structs (UENUM/USTRUCT) are always serializable in UE5
    let enum_names: std::collections::HashSet<String> = program
        .items
        .iter()
        .filter_map(|item| match item {
            TypedItem::Enum(e) => Some(e.ast.name.clone()),
            TypedItem::Struct(s) => Some(s.ast.name.clone()),
            _ => None,
        })
        .collect();

    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            let actor_name = &actor.ast.name;

            // Check message handlers (RPCs)
            for handler in &actor.ast.handlers {
                let handler_name = &handler.message_type;

                // Validate RPC naming convention
                let is_server = handler_name.starts_with("Server_");
                let is_client = handler_name.starts_with("Client_");
                let is_multicast = handler_name.starts_with("Multicast_");

                if is_server || is_client || is_multicast {
                    // Validate parameters are serializable
                    for param in &handler.params {
                        if !is_serializable_type(&param.ty, &enum_names) {
                            ctx.error(format!(
                                "Actor '{}', RPC '{}', parameter '{}': RPC parameters must be serializable. \
                                Type '{:?}' cannot be used in RPCs.",
                                actor_name, handler_name, param.name, param.ty
                            ));
                        }

                        // Check for delegate parameters (already checked in validate_function but double-check)
                        if is_delegate_type(&param.ty) {
                            ctx.error(format!(
                                "Actor '{}', RPC '{}', parameter '{}': RPC parameters cannot be delegates. \
                                This is a UE5 security/stability restriction.",
                                actor_name, handler_name, param.name
                            ));
                        }
                    }
                }
            }

            // Check methods for RPC naming
            for method in &actor.ast.methods {
                let method_name = &method.name;

                let is_server = method_name.starts_with("Server_");
                let is_client = method_name.starts_with("Client_");
                let is_multicast = method_name.starts_with("Multicast_");

                if is_server || is_client || is_multicast {
                    // Validate parameters are serializable
                    for param in &method.params {
                        if !is_serializable_type(&param.ty, &enum_names) {
                            ctx.error(format!(
                                "Actor '{}', RPC '{}', parameter '{}': RPC parameters must be serializable. \
                                Type '{:?}' cannot be used in RPCs.",
                                actor_name, method_name, param.name, param.ty
                            ));
                        }

                        if is_delegate_type(&param.ty) {
                            ctx.error(format!(
                                "Actor '{}', RPC '{}', parameter '{}': RPC parameters cannot be delegates.",
                                actor_name, method_name, param.name
                            ));
                        }
                    }
                }
            }
        }
    }
}

/// Task 4.3: Validate datatable structs
/// Requirement 3.3: Verify all fields are UE5-serializable, no pointers
fn validate_datatables(ctx: &mut ValidationContext, program: &TypedProgram, _kb: &EngineKnowledge) {
    // Collect all user-defined enum and struct names from the program.
    // UE5 DataTables fully support UENUM and USTRUCT field types — the type-checker
    // has already verified these names resolve to real program items.
    let user_defined_types: std::collections::HashSet<String> = program
        .items
        .iter()
        .filter_map(|item| match item {
            TypedItem::Enum(e) => Some(e.ast.name.clone()),
            TypedItem::Struct(s) => Some(s.ast.name.clone()),
            _ => None,
        })
        .collect();

    // Collect enum names for serialization checking
    let enum_names: std::collections::HashSet<String> = program
        .items
        .iter()
        .filter_map(|item| {
            if let TypedItem::Enum(e) = item {
                Some(e.ast.name.clone())
            } else {
                None
            }
        })
        .collect();

    for item in &program.items {
        if let TypedItem::Struct(struct_def) = item {
            let is_datatable = struct_def
                .ast
                .attributes
                .iter()
                .any(|a| a.name == "datatable");

            if is_datatable {
                let struct_name = &struct_def.ast.name;

                for field in &struct_def.ast.fields {
                    // Allow user-defined enum/struct types in addition to UE5 primitives —
                    // they map to UENUM/USTRUCT which are always DataTable-compatible.
                    let is_user_type = if let Type::Named { name, .. } = &field.ty {
                        user_defined_types.contains(name.as_str())
                    } else {
                        false
                    };

                    if !is_user_type && !is_serializable_type(&field.ty, &enum_names) {
                        ctx.error(format!(
                            "DataTable struct '{}', field '{}': DataTable fields must be UE5-serializable. \
                            Type '{:?}' cannot be used in DataTables.",
                            struct_name, field.name, field.ty
                        ));
                    }

                    // Check for pointer types (not allowed in DataTables)
                    if is_pointer_type(&field.ty) {
                        ctx.error(format!(
                            "DataTable struct '{}', field '{}': DataTable fields cannot be pointers. \
                            Use value types or soft object references instead.",
                            struct_name, field.name
                        ));
                    }
                }
            }
        }
    }
}

/// Task 4.4: Validate component-specific rules
/// Requirement 3.4: Verify no actor-only features in components
fn validate_components_enhanced(ctx: &mut ValidationContext, program: &TypedProgram) {
    for item in &program.items {
        if let TypedItem::Component(comp) = item {
            let comp_name = &comp.ast.name;

            // Check for actor-only features
            for method in &comp.ast.methods {
                // Components shouldn't have Tick by default (they can, but it's opt-in)
                if method.name == "Tick" || method.name == "tick" {
                    ctx.warning(format!(
                        "Component '{}': Tick functions in components must be explicitly enabled with PrimaryComponentTick.bCanEverTick = true. \
                        Consider using timers or events instead for better performance.",
                        comp_name
                    ));
                }

                // Check for RPC methods (components can have RPCs but it's unusual)
                if method.name.starts_with("Server_")
                    || method.name.starts_with("Client_")
                    || method.name.starts_with("Multicast_")
                {
                    ctx.warning(format!(
                        "Component '{}', method '{}': RPCs in components are unusual. \
                        Consider moving networking logic to the owning actor.",
                        comp_name, method.name
                    ));
                }
            }
        }
    }
}

/// Task 4.5: Validate name collisions with engine types
/// Requirement 3.10: Check against EngineKnowledge for engine types
/// Requirement 3.11: Check for C++ keywords and UE5 macro names
fn validate_name_collisions(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    kb: &EngineKnowledge,
) {
    // C++ keywords that cannot be used as identifiers
    const CPP_KEYWORDS: &[&str] = &[
        "alignas",
        "alignof",
        "and",
        "and_eq",
        "asm",
        "auto",
        "bitand",
        "bitor",
        "bool",
        "break",
        "case",
        "catch",
        "char",
        "char8_t",
        "char16_t",
        "char32_t",
        "class",
        "compl",
        "concept",
        "const",
        "consteval",
        "constexpr",
        "constinit",
        "const_cast",
        "continue",
        "co_await",
        "co_return",
        "co_yield",
        "decltype",
        "default",
        "delete",
        "do",
        "double",
        "dynamic_cast",
        "else",
        "enum",
        "explicit",
        "export",
        "extern",
        "false",
        "float",
        "for",
        "friend",
        "goto",
        "if",
        "inline",
        "int",
        "long",
        "mutable",
        "namespace",
        "new",
        "noexcept",
        "not",
        "not_eq",
        "nullptr",
        "operator",
        "or",
        "or_eq",
        "private",
        "protected",
        "public",
        "register",
        "reinterpret_cast",
        "requires",
        "return",
        "short",
        "signed",
        "sizeof",
        "static",
        "static_assert",
        "static_cast",
        "struct",
        "switch",
        "template",
        "this",
        "thread_local",
        "throw",
        "true",
        "try",
        "typedef",
        "typeid",
        "typename",
        "union",
        "unsigned",
        "using",
        "virtual",
        "void",
        "volatile",
        "wchar_t",
        "while",
        "xor",
        "xor_eq",
    ];

    // UE5 macro names that should not be used as identifiers
    const UE5_MACROS: &[&str] = &[
        "UCLASS",
        "USTRUCT",
        "UENUM",
        "UFUNCTION",
        "UPROPERTY",
        "UMETA",
        "GENERATED_BODY",
        "GENERATED_UCLASS_BODY",
        "GENERATED_USTRUCT_BODY",
        "TEXT",
        "LOCTEXT",
        "NSLOCTEXT",
        "TEXTVIEW",
        "UPARAM",
        "UDELEGATE",
        "DECLARE_DYNAMIC_MULTICAST_DELEGATE",
    ];

    for item in &program.items {
        let (type_name, type_kind) = match item {
            TypedItem::Enum(en) => (&en.ast.name, "Enum"),
            TypedItem::Struct(st) => (&st.ast.name, "Struct"),
            TypedItem::Actor(actor) => (&actor.ast.name, "Actor"),
            TypedItem::Component(comp) => (&comp.ast.name, "Component"),
            _ => continue,
        };

        // Check C++ keywords
        let lower_name = type_name.to_lowercase();
        if CPP_KEYWORDS.contains(&lower_name.as_str()) {
            ctx.error(format!(
                "{} '{}': This name is a C++ keyword and cannot be used as an identifier.",
                type_kind, type_name
            ));
        }

        // Check UE5 macro names
        if UE5_MACROS.contains(&type_name.as_str()) {
            ctx.error(format!(
                "{} '{}': This name is a UE5 macro and cannot be used as a type name.",
                type_kind, type_name
            ));
        }

        // Check engine type collisions (already implemented in check_engine_name_collision)
        // This is called per-type in validate_enum, validate_struct, etc.
    }
}

/// Task 4.6: Validate circular dependencies
/// Requirement 3.12: Detect cycles and suggest forward declarations
fn validate_circular_dependencies(ctx: &mut ValidationContext, program: &TypedProgram) {
    use std::collections::{HashMap, HashSet};

    // Build dependency graph
    let mut dependencies: HashMap<String, HashSet<String>> = HashMap::new();

    for item in &program.items {
        match item {
            TypedItem::Struct(struct_def) => {
                let struct_name = struct_def.ast.name.clone();
                let mut deps = HashSet::new();

                for field in &struct_def.ast.fields {
                    if let Some(dep_name) = extract_type_name(&field.ty) {
                        deps.insert(dep_name);
                    }
                }

                dependencies.insert(struct_name, deps);
            }
            TypedItem::Actor(actor) => {
                let actor_name = actor.ast.name.clone();
                let mut deps = HashSet::new();

                for state in &actor.ast.state {
                    if let Some(dep_name) = extract_type_name(&state.ty) {
                        deps.insert(dep_name);
                    }
                }

                dependencies.insert(actor_name, deps);
            }
            TypedItem::Component(comp) => {
                let comp_name = comp.ast.name.clone();
                let mut deps = HashSet::new();

                for state in &comp.ast.state {
                    if let Some(dep_name) = extract_type_name(&state.ty) {
                        deps.insert(dep_name);
                    }
                }

                dependencies.insert(comp_name, deps);
            }
            _ => {}
        }
    }

    // Detect cycles using DFS
    let mut visited = HashSet::new();
    let mut rec_stack = HashSet::new();

    for type_name in dependencies.keys() {
        if !visited.contains(type_name) {
            if detect_cycle(
                type_name,
                &dependencies,
                &mut visited,
                &mut rec_stack,
                &mut Vec::new(),
                ctx,
            ) {
                // Cycle detected and reported
            }
        }
    }
}

/// Helper: Detect cycles in dependency graph using DFS
fn detect_cycle(
    node: &str,
    graph: &HashMap<String, HashSet<String>>,
    visited: &mut HashSet<String>,
    rec_stack: &mut HashSet<String>,
    path: &mut Vec<String>,
    ctx: &mut ValidationContext,
) -> bool {
    visited.insert(node.to_string());
    rec_stack.insert(node.to_string());
    path.push(node.to_string());

    if let Some(neighbors) = graph.get(node) {
        for neighbor in neighbors {
            if !visited.contains(neighbor) {
                if detect_cycle(neighbor, graph, visited, rec_stack, path, ctx) {
                    return true;
                }
            } else if rec_stack.contains(neighbor) {
                // Cycle detected
                let cycle_start = path.iter().position(|n| n == neighbor).unwrap();
                let cycle: Vec<_> = path[cycle_start..].iter().cloned().collect();

                ctx.error(format!(
                    "Circular dependency detected: {} → {}. \
                    Use forward declarations or pointers to break the cycle.",
                    cycle.join(" → "),
                    neighbor
                ));
                return true;
            }
        }
    }

    rec_stack.remove(node);
    path.pop();
    false
}

/// Helper: Extract type name from Type AST node
fn extract_type_name(ty: &Type) -> Option<String> {
    match ty {
        Type::Named { name, .. } => Some(name.clone()),
        _ => None,
    }
}

/// Helper: Check if a type is serializable for replication/RPCs
fn is_serializable_type(ty: &Type, enum_names: &std::collections::HashSet<String>) -> bool {
    match ty {
        Type::Named { name, .. } => {
            // Check if it's a user-defined enum
            if enum_names.contains(name) {
                return true;
            }

            // Primitives are serializable
            matches!(name.as_str(),
                "Int" | "Float" | "Bool" | "String" |
                "i8" | "i16" | "i32" | "i64" |
                "u8" | "u16" | "u32" | "u64" |
                "f32" | "f64" | "bool" |
                // KAIN vector types (map to UE5 types)
                "Vec2" | "Vec3" | "Vec4" |
                // UE5 types
                "FVector" | "FRotator" | "FTransform" | "FLinearColor" | "FColor" |
                "FVector2D" | "FVector4" | "FQuat" | "FName" | "FString" | "FText" |
                // Containers (if inner types are serializable, which we check recursively)
                "Array" | "TArray" | "Map" | "TMap" | "Set" | "TSet"
            ) || name.starts_with("E") // Enums are serializable (UE5 prefixed)
              || name.starts_with("F") // Structs are serializable
              || name.starts_with("U") // UObject pointers are serializable
              || name.starts_with("A") // Actor pointers are serializable
        }
        _ => false,
    }
}

/// Helper: Check if a type is a pointer type
fn is_pointer_type(ty: &Type) -> bool {
    match ty {
        Type::Named { name, .. } => {
            // UObject-derived types are pointers
            name.starts_with("U") || name.starts_with("A") || name.contains("*")
        }
        _ => false,
    }
}

/*
 * =========================================================================================
 *  KNOWN UE5 COMPILATION ERRORS & ORACLE COVERAGE
 * =========================================================================================
 *
 * The following errors have been encountered and resolved. While strictly codegen bugs,
 * keeping them here helps future oracle development.
 *
 * 1. Double API Macro Suffix (error C2079: uses undefined class 'XXX_API_API')
 *    - Cause: UE5 codegen appending _API to a module name that already had _API.
 *    - Fix: Ensure `ue5.rs` uses `{} {}` instead of `{}_API {}` for class declarations.
 *
 * 2. Default Argument Redefinition (error C2572)
 *    - Cause: Emitting `= Value` in both .h and .cpp for helper functions.
 *    - Fix: Only emit default values in the header file.
 *
 * 3. Undeclared Identifier 'PrimaryActorTick', 'particle_count'
 *    - Cause: Class declaration failure (due to #1) caused the compiler to miss the
 *      class definition entirely, making member variables inaccessible.
 *    - Fix: Fixing the class declaration macro (#1) resolves this.
 *
 * 3. Vector Type Mismatch (error C2440: 'initializing': cannot convert from 'FVector3f' to 'FVector')
 *    - Cause: KAIN used float-based vectors (FVector3f) while UE5 LWC standard is double (FVector).
 *    - Fix: Forced double-based types for all Actor state and Blueprints.
 *
 * 4. Missing Includes for RDG (error C2079: 'GraphBuilder' uses undefined class 'FRDGBuilder')
 *    - Cause: Actor source file missing headers for RenderGraphBuilder, RenderGraphUtils, and RenderTarget2D.
 *    - Fix: Added automatic includes for RDG and shader-specific headers in `ue5.rs` preamble.
 *
 * 5. FSceneRenderTargetItem Conversion Error (error C2440)
 *    - Cause: UE5.4 changed FSceneRenderTargetItem constructor to be more strict with pointer types.
 *    - Fix: Pass raw pointer using `(FRHITexture*)TextureRHI.GetReference()` in `ue5.rs`.
 *
 * 6. FRenderTargetPool::CreateUntrackedElement Argument Mismatch (error C2660)
 *    - Cause: UE5.4 signature requires 3 arguments (Desc, PooledRT, Item).
 *    - Fix: Ensure the `ue5.rs` template provides all 3 arguments in the render target creation lambda.
 */

/// Enforce custom validation rules from validation_rules.json
fn enforce_custom_rules(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rules: &ValidationRules,
    kb: &EngineKnowledge,
) {
    for rule in rules.enabled_rules() {
        match &rule.condition {
            RuleCondition::TypeCollision { type_names } => {
                enforce_type_collision_rule(ctx, program, rule, type_names);
            }
            RuleCondition::IncompatibleAttributes { attributes } => {
                enforce_incompatible_attributes_rule(ctx, program, rule, attributes);
            }
            RuleCondition::InvalidRpcNaming { pattern } => {
                enforce_rpc_naming_rule(ctx, program, rule, pattern);
            }
            RuleCondition::NestedContainer { outer, inner } => {
                enforce_nested_container_rule(ctx, program, rule, outer, inner);
            }
            RuleCondition::InvalidNaming {
                pattern,
                applies_to,
            } => {
                enforce_invalid_naming_rule(ctx, program, rule, pattern, applies_to);
            }
            RuleCondition::MissingAttribute {
                required_attribute,
                when_attribute,
            } => {
                enforce_missing_attribute_rule(
                    ctx,
                    program,
                    rule,
                    required_attribute,
                    when_attribute,
                );
            }
            RuleCondition::ForbiddenType {
                forbidden_types,
                context: type_context,
            } => {
                enforce_forbidden_type_rule(ctx, program, rule, forbidden_types, type_context);
            }
        }
    }
}

/// Enforce type collision rule
fn enforce_type_collision_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    type_names: &[String],
) {
    for item in &program.items {
        let item_name = match item {
            TypedItem::Actor(a) => Some(&a.ast.name),
            TypedItem::Struct(s) => Some(&s.ast.name),
            TypedItem::Enum(e) => Some(&e.ast.name),
            TypedItem::Component(c) => Some(&c.ast.name),
            _ => None,
        };

        if let Some(name) = item_name {
            if type_names.contains(name) {
                let msg = format!("{}: {}", rule.message, name);
                match rule.severity {
                    Severity::Error => {
                        ctx.error(msg);
                        if let Some(suggestion) = &rule.suggestion {
                            ctx.error(format!("  Suggestion: {}", suggestion));
                        }
                    }
                    Severity::Warning => {
                        ctx.warning(msg);
                        if let Some(suggestion) = &rule.suggestion {
                            ctx.warning(format!("  Suggestion: {}", suggestion));
                        }
                    }
                    Severity::Info => {
                        ctx.warning(format!("Info: {}", msg));
                    }
                }
            }
        }
    }
}

/// Enforce incompatible attributes rule
fn enforce_incompatible_attributes_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    attribute_pairs: &[(String, String)],
) {
    for item in &program.items {
        match item {
            TypedItem::Actor(actor) => {
                for field in &actor.ast.state {
                    check_incompatible_attrs(
                        ctx,
                        rule,
                        &field.attributes,
                        &field.name,
                        attribute_pairs,
                    );
                }
            }
            TypedItem::Struct(struct_def) => {
                for field in &struct_def.ast.fields {
                    check_incompatible_attrs(
                        ctx,
                        rule,
                        &field.attributes,
                        &field.name,
                        attribute_pairs,
                    );
                }
            }
            TypedItem::Component(comp) => {
                for field in &comp.ast.state {
                    check_incompatible_attrs(
                        ctx,
                        rule,
                        &field.attributes,
                        &field.name,
                        attribute_pairs,
                    );
                }
            }
            _ => {}
        }
    }
}

fn check_incompatible_attrs(
    ctx: &mut ValidationContext,
    rule: &ValidationRule,
    attributes: &[Attribute],
    field_name: &str,
    attribute_pairs: &[(String, String)],
) {
    let attr_names: Vec<String> = attributes.iter().map(|a| a.name.clone()).collect();

    for (attr1, attr2) in attribute_pairs {
        if attr_names.contains(attr1) && attr_names.contains(attr2) {
            let msg = format!("{} (field: {})", rule.message, field_name);
            match rule.severity {
                Severity::Error => {
                    ctx.error(msg);
                    if let Some(suggestion) = &rule.suggestion {
                        ctx.error(format!("  Suggestion: {}", suggestion));
                    }
                }
                Severity::Warning => {
                    ctx.warning(msg);
                }
                Severity::Info => {
                    ctx.warning(format!("Info: {}", msg));
                }
            }
        }
    }
}

/// Enforce RPC naming rule
fn enforce_rpc_naming_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    pattern: &str,
) {
    let re = match regex::Regex::new(pattern) {
        Ok(r) => r,
        Err(_) => return, // Invalid regex, skip
    };

    for item in &program.items {
        if let TypedItem::Actor(actor) = item {
            for method in &actor.ast.methods {
                // Check if method has RPC attributes
                let has_rpc_attr = method
                    .attributes
                    .iter()
                    .any(|a| matches!(a.name.as_str(), "server" | "client" | "multicast"));

                if has_rpc_attr && !re.is_match(&method.name) {
                    let msg = format!("{}: {}", rule.message, method.name);
                    match rule.severity {
                        Severity::Error => {
                            ctx.error(msg);
                            if let Some(suggestion) = &rule.suggestion {
                                ctx.error(format!("  Suggestion: {}", suggestion));
                            }
                        }
                        Severity::Warning => {
                            ctx.warning(msg);
                        }
                        Severity::Info => {
                            ctx.warning(format!("Info: {}", msg));
                        }
                    }
                }
            }
        }
    }
}

/// Enforce nested container rule
fn enforce_nested_container_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    outer: &[String],
    inner: &[String],
) {
    for item in &program.items {
        match item {
            TypedItem::Actor(actor) => {
                for field in &actor.ast.state {
                    check_nested_container(ctx, rule, &field.ty, &field.name, outer, inner);
                }
            }
            TypedItem::Struct(struct_def) => {
                for field in &struct_def.ast.fields {
                    check_nested_container(ctx, rule, &field.ty, &field.name, outer, inner);
                }
            }
            TypedItem::Component(comp) => {
                for field in &comp.ast.state {
                    check_nested_container(ctx, rule, &field.ty, &field.name, outer, inner);
                }
            }
            _ => {}
        }
    }
}

fn check_nested_container(
    ctx: &mut ValidationContext,
    rule: &ValidationRule,
    ty: &Type,
    field_name: &str,
    outer: &[String],
    inner: &[String],
) {
    if let Type::Named {
        name: outer_name,
        generics: inner_types,
        ..
    } = ty
    {
        if outer.contains(outer_name) {
            for inner_ty in inner_types {
                if let Type::Named {
                    name: inner_name, ..
                } = inner_ty
                {
                    if inner.contains(inner_name) {
                        let msg = format!(
                            "{} (field: {}, type: {})",
                            rule.message, field_name, outer_name
                        );
                        match rule.severity {
                            Severity::Error => {
                                ctx.error(msg);
                                if let Some(suggestion) = &rule.suggestion {
                                    ctx.error(format!("  Suggestion: {}", suggestion));
                                }
                            }
                            Severity::Warning => {
                                ctx.warning(msg);
                            }
                            Severity::Info => {
                                ctx.warning(format!("Info: {}", msg));
                            }
                        }
                    }
                }
            }
        }
    }
}

/// Enforce invalid naming rule
fn enforce_invalid_naming_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    pattern: &str,
    applies_to: &[String],
) {
    let re = match regex::Regex::new(pattern) {
        Ok(r) => r,
        Err(_) => return, // Invalid regex, skip
    };

    for item in &program.items {
        match item {
            TypedItem::Actor(actor) if applies_to.contains(&"actor".to_string()) => {
                if re.is_match(&actor.ast.name) {
                    report_naming_violation(ctx, rule, &actor.ast.name, "actor");
                }
            }
            TypedItem::Struct(struct_def) if applies_to.contains(&"struct".to_string()) => {
                if re.is_match(&struct_def.ast.name) {
                    report_naming_violation(ctx, rule, &struct_def.ast.name, "struct");
                }
            }
            TypedItem::Enum(enum_def) if applies_to.contains(&"enum".to_string()) => {
                if re.is_match(&enum_def.ast.name) {
                    report_naming_violation(ctx, rule, &enum_def.ast.name, "enum");
                }
            }
            TypedItem::Component(comp) if applies_to.contains(&"component".to_string()) => {
                if re.is_match(&comp.ast.name) {
                    report_naming_violation(ctx, rule, &comp.ast.name, "component");
                }
            }
            TypedItem::Function(func) if applies_to.contains(&"function".to_string()) => {
                if re.is_match(&func.ast.name) {
                    report_naming_violation(ctx, rule, &func.ast.name, "function");
                }
            }
            _ => {}
        }
    }
}

fn report_naming_violation(
    ctx: &mut ValidationContext,
    rule: &ValidationRule,
    name: &str,
    kind: &str,
) {
    let msg = format!("{} ({} '{}')", rule.message, kind, name);
    match rule.severity {
        Severity::Error => {
            ctx.error(msg);
            if let Some(suggestion) = &rule.suggestion {
                ctx.error(format!("  Suggestion: {}", suggestion));
            }
        }
        Severity::Warning => {
            ctx.warning(msg);
        }
        Severity::Info => {
            ctx.warning(format!("Info: {}", msg));
        }
    }
}

/// Enforce missing attribute rule
fn enforce_missing_attribute_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    _required_attribute: &str,
    _when_attribute: &str,
) {
    // This rule is more complex and context-dependent
    // For now, we'll implement a basic version
    // Full implementation would check editor attributes, etc.

    // TODO: Implement full missing attribute checking
    // This would require access to editor attribute definitions
}

/// Enforce forbidden type rule
fn enforce_forbidden_type_rule(
    ctx: &mut ValidationContext,
    program: &TypedProgram,
    rule: &ValidationRule,
    forbidden_types: &[String],
    type_context: &str,
) {
    match type_context {
        "datatable" => {
            for item in &program.items {
                if let TypedItem::Struct(struct_def) = item {
                    // Check if this is a datatable struct
                    let is_datatable = struct_def
                        .ast
                        .attributes
                        .iter()
                        .any(|a| a.name == "datatable");
                    if is_datatable {
                        for field in &struct_def.ast.fields {
                            if contains_forbidden_type(&field.ty, forbidden_types) {
                                let msg = format!(
                                    "{} (struct: {}, field: {})",
                                    rule.message, struct_def.ast.name, field.name
                                );
                                match rule.severity {
                                    Severity::Error => {
                                        ctx.error(msg);
                                        if let Some(suggestion) = &rule.suggestion {
                                            ctx.error(format!("  Suggestion: {}", suggestion));
                                        }
                                    }
                                    Severity::Warning => {
                                        ctx.warning(msg);
                                    }
                                    Severity::Info => {
                                        ctx.warning(format!("Info: {}", msg));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        "rpc_parameter" => {
            for item in &program.items {
                if let TypedItem::Actor(actor) = item {
                    for method in &actor.ast.methods {
                        let has_rpc_attr = method
                            .attributes
                            .iter()
                            .any(|a| matches!(a.name.as_str(), "server" | "client" | "multicast"));
                        if has_rpc_attr {
                            for param in &method.params {
                                if contains_forbidden_type(&param.ty, forbidden_types) {
                                    let msg = format!(
                                        "{} (RPC: {}, parameter: {})",
                                        rule.message, method.name, param.name
                                    );
                                    match rule.severity {
                                        Severity::Error => {
                                            ctx.error(msg);
                                            if let Some(suggestion) = &rule.suggestion {
                                                ctx.error(format!("  Suggestion: {}", suggestion));
                                            }
                                        }
                                        Severity::Warning => {
                                            ctx.warning(msg);
                                        }
                                        Severity::Info => {
                                            ctx.warning(format!("Info: {}", msg));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        _ => {
            // Unknown context, skip
        }
    }
}

fn contains_forbidden_type(ty: &Type, forbidden_types: &[String]) -> bool {
    match ty {
        Type::Named { name, generics, .. } => {
            if forbidden_types.iter().any(|ft| name.contains(ft)) {
                return true;
            }
            generics
                .iter()
                .any(|t| contains_forbidden_type(t, forbidden_types))
        }
        Type::Tuple(types, _) => types
            .iter()
            .any(|t| contains_forbidden_type(t, forbidden_types)),
        Type::Array(inner, _, _) | Type::Slice(inner, _) | Type::Option(inner, _) => {
            contains_forbidden_type(inner, forbidden_types)
        }
        Type::Result(ok, err, _) => {
            contains_forbidden_type(ok, forbidden_types)
                || contains_forbidden_type(err, forbidden_types)
        }
        Type::Ref { inner, .. } => contains_forbidden_type(inner, forbidden_types),
        Type::Function {
            params,
            return_type,
            ..
        } => {
            params
                .iter()
                .any(|t| contains_forbidden_type(t, forbidden_types))
                || contains_forbidden_type(return_type, forbidden_types)
        }
        _ => false,
    }
}
