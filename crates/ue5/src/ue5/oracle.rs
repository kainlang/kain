// Copyright 2026 Zentako. All Rights Reserved.
// Unreal Semantic Validator - "The Oracle"
// 
// This module validates KAIN code against Unreal Engine's semantic rules
// BEFORE generating C++. Catches UHT errors in 10ms instead of 2 minutes.
//
// Based on Epic's UHT source:
// - EpicGames.UHT/Specifiers/UhtFunctionSpecifiers.cs
// - EpicGames.UHT/Specifiers/UhtPropertyMemberSpecifiers.cs

use kain_core::types::{TypedProgram, TypedItem, TypedFunction, TypedStruct, TypedActor, TypedComponent};
use kain_core::error::{KainError, KainResult};
use kain_core::ast::{Type, Attribute};
use kain_core::ast::Visibility;
use super::engine_knowledge::EngineKnowledge;
use super::uht_rules::UhtRules;

/// Validation context for tracking state during validation
pub struct ValidationContext {
    pub errors: Vec<String>,
    pub warnings: Vec<String>,
}

impl ValidationContext {
    pub fn new() -> Self {
        Self {
            errors: Vec::new(),
            warnings: Vec::new(),
        }
    }

    pub fn error(&mut self, msg: String) {
        self.errors.push(msg);
    }

    pub fn warning(&mut self, msg: String) {
        self.warnings.push(msg);
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
pub fn validate_program(program: &TypedProgram) -> KainResult<()> {
    let kb = EngineKnowledge::new();
    validate_program_with_knowledge(program, &kb)
}

/// Validation with explicit EngineKnowledge (used when context already has one)
pub fn validate_program_with_knowledge(program: &TypedProgram, kb: &EngineKnowledge) -> KainResult<()> {
    let uht = UhtRules::new();
    validate_program_full(program, kb, &uht)
}

/// Full validation with EngineKnowledge + UHT rules (used when Ue5Context is available)
pub fn validate_program_full(program: &TypedProgram, kb: &EngineKnowledge, uht: &UhtRules) -> KainResult<()> {
    let mut ctx = ValidationContext::new();
    
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
    
    // If we have errors, return them
    if ctx.has_errors() {
        return Err(KainError::runtime(ctx.report()));
    }
    
    // Print warnings if any
    if !ctx.warnings.is_empty() {
        eprintln!("{}", ctx.report());
    }
    
    Ok(())
}

/// Validate function specifiers (UFUNCTION rules)
fn validate_function(ctx: &mut ValidationContext, func: &TypedFunction) {
    let func_name = &func.ast.name;
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
        ctx.error(format!(
            "Function '{}': A Private function cannot be a BlueprintImplementableEvent or BlueprintNativeEvent.",
            func_name
        ));
    }

    // RULE: BlueprintEvent cannot be a BlueprintGetter
    if flags.blueprint_event && flags.blueprint_getter {
        ctx.error(format!(
            "Function '{}': Function cannot be a blueprint event and a blueprint getter.",
            func_name
        ));
    }
    
    // RULE 1: BlueprintImplementableEvent cannot be replicated
    if flags.blueprint_implementable && flags.net {
        ctx.error(format!(
            "Function '{}': BlueprintImplementableEvent functions cannot be replicated (Server/Client/Multicast)",
            func_name
        ));
    }
    
    // RULE 2: BlueprintNativeEvent cannot be replicated
    if flags.blueprint_native && flags.net {
        ctx.error(format!(
            "Function '{}': BlueprintNativeEvent functions cannot be replicated (Server/Client/Multicast)",
            func_name
        ));
    }
    
    // RULE 3: Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent
    if flags.blueprint_implementable && flags.blueprint_native {
        ctx.error(format!(
            "Function '{}': Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent",
            func_name
        ));
    }
    
    // RULE 4: Exec functions cannot be replicated
    if flags.exec && flags.net {
        ctx.error(format!(
            "Function '{}': Exec functions cannot be replicated",
            func_name
        ));
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
        validate_property(ctx, &field.name, &struct_name, true, &field.ty, &field.attributes, kb);
    }

    // RULE: Struct Naming Prefix (F)
    let engine_name = crate::ue5::naming::to_struct_name(struct_name);
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
fn validate_enum(ctx: &mut ValidationContext, enum_def: &kain_core::types::TypedEnum, kb: &EngineKnowledge) {
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
    let has_max = enum_def.ast.variants.iter().any(|v| v.name.to_uppercase().ends_with("MAX"));
    if !has_max {
        ctx.warning(format!(
            "Enum '{}': Missing a '_MAX' variant. Unreal Engine metadata systems (and Blueprints) often require a MAX entry for stability.",
            enum_name
        ));
    }

    // RULE: Enum Naming Prefix (E)
    let engine_name = crate::ue5::naming::to_enum_name(enum_name);
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
    let engine_name = crate::ue5::naming::to_component_name(comp_name);
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
fn validate_property(ctx: &mut ValidationContext, prop_name: &str, owner_name: &str, is_struct_member: bool, ty: &Type, attributes: &[Attribute], _kb: &EngineKnowledge) {
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
        if matches!(attr.name.as_str(), "edit_anywhere" | "visible_anywhere" | "replicated") {
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
    
    #[test]
    fn test_validation_context() {
        let mut ctx = ValidationContext::new();
        assert!(!ctx.has_errors());
        
        ctx.error("Test error".to_string());
        assert!(ctx.has_errors());
        
        let report = ctx.report();
        assert!(report.contains("Test error"));
    }
}


// ═══════════════════════════════════════════════════════════════════
// DATA-DRIVEN UHT VALIDATION (Phase 2)
// Uses rules extracted from Epic's EpicGames.UHT C# source
// ═══════════════════════════════════════════════════════════════════

/// Validate actor state fields against UHT property type rules
fn validate_actor_uht(ctx: &mut ValidationContext, actor: &TypedActor, uht: &UhtRules) {
    for state in &actor.ast.state {
        validate_property_type_uht(ctx, &state.name, &actor.ast.name, &state.ty, &state.attributes, false, uht);
    }
}

/// Validate struct fields against UHT property type rules
fn validate_struct_uht(ctx: &mut ValidationContext, struct_def: &TypedStruct, uht: &UhtRules) {
    for field in &struct_def.ast.fields {
        validate_property_type_uht(ctx, &field.name, &struct_def.ast.name, &field.ty, &field.attributes, true, uht);
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
    if let Type::Generic { name, params, .. } = ty {
        let type_name = map_kain_container(name);
        if uht.is_container_type(&type_name) {
            for param in params {
                if let Type::Generic { name: inner_name, .. } = param {
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
    let is_blueprint_exposed = attributes.iter().any(|a| 
        a.name == "blueprint_read_write" || a.name == "blueprint_read_only"
    );
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
fn check_engine_name_collision(ctx: &mut ValidationContext, type_name: &str, type_kind: &str, kb: &EngineKnowledge) {
    // Check against the full EngineKnowledge database
    // This covers ALL known engine types, not just a hardcoded subset
    let collides = kb.is_known_type(type_name)
        || kb.is_known_type(&format!("A{}", type_name))
        || kb.is_known_type(&format!("U{}", type_name))
        || kb.is_known_type(&format!("F{}", type_name))
        || kb.is_known_type(&format!("E{}", type_name))
        // Also check type aliases (e.g. "Vec3" → "FVector")
        || kb.resolve_type_alias(type_name).is_some();

    if collides {
        ctx.error(format!(
            "{} '{}': This name collides with a UE5 engine type. UHT will reject it with 'shares engine name' error. \
            Please rename to something more specific (e.g., 'My{}', 'Custom{}', 'Game{}', etc.).",
            type_kind, type_name, type_name, type_name, type_name
        ));
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
