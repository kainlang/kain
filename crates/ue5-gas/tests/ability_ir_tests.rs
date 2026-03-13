// ============================================================================
// Ability IR Tests — Comprehensive test suite for GameplayAbilityIR
// ============================================================================

use kain_core::ast::{Attribute, Block, Function, GameplayAbilityDef, Param, Type};
use kain_core::span::Span;
use ue5_gas::*;

// ============================================================================
// Helper Functions
// ============================================================================

fn create_test_ability(name: &str) -> GameplayAbilityDef {
    GameplayAbilityDef {
        name: name.to_string(),
        instancing_policy: None,
        replication_policy: None,
        net_execution_policy: None,
        ability_tags: vec![],
        activation_required_tags: vec![],
        activation_blocked_tags: vec![],
        activation_owned_tags: vec![],
        cancel_abilities_with_tag: vec![],
        block_abilities_with_tag: vec![],
        cost_effect: None,
        cooldown_effect: None,
        methods: vec![],
        attributes: vec![Attribute {
            name: "ability".to_string(),
            args: vec![],
            span: Span::default(),
        }],
        span: Span::default(),
    }
}

fn create_test_function(name: &str) -> Function {
    Function {
        name: name.to_string(),
        generics: vec![],
        params: vec![],
        return_type: None,
        effects: vec![],
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        visibility: kain_core::ast::Visibility::Public,
        attributes: vec![],
        span: Span::default(),
    }
}

// ============================================================================
// Policy Tests
// ============================================================================

#[test]
fn test_ability_with_all_policies() {
    let mut ability = create_test_ability("TestAbility");
    ability.instancing_policy = Some("InstancedPerExecution".to_string());
    ability.replication_policy = Some("ReplicateYes".to_string());
    ability.net_execution_policy = Some("LocalPredicted".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();

    assert_eq!(ir.name, "TestAbility");
    assert_eq!(
        ir.instancing_policy,
        InstancingPolicy::InstancedPerExecution
    );
    assert_eq!(ir.replication_policy, ReplicationPolicy::ReplicateYes);
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::LocalPredicted);
}

#[test]
fn test_ability_with_default_policies() {
    let ability = create_test_ability("TestAbility");
    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();

    assert_eq!(
        ir.instancing_policy,
        InstancingPolicy::InstancedPerExecution
    );
    assert_eq!(ir.replication_policy, ReplicationPolicy::ReplicateYes);
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::LocalPredicted);
}

#[test]
fn test_instancing_policy_instanced_per_actor() {
    let mut ability = create_test_ability("TestAbility");
    ability.instancing_policy = Some("InstancedPerActor".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.instancing_policy, InstancingPolicy::InstancedPerActor);
}

#[test]
fn test_instancing_policy_non_instanced() {
    let mut ability = create_test_ability("TestAbility");
    ability.instancing_policy = Some("NonInstanced".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.instancing_policy, InstancingPolicy::NonInstanced);
}

#[test]
fn test_replication_policy_no() {
    let mut ability = create_test_ability("TestAbility");
    ability.replication_policy = Some("ReplicateNo".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.replication_policy, ReplicationPolicy::ReplicateNo);
}

#[test]
fn test_net_execution_policy_local_only() {
    let mut ability = create_test_ability("TestAbility");
    ability.net_execution_policy = Some("LocalOnly".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::LocalOnly);
}

#[test]
fn test_net_execution_policy_server_initiated() {
    let mut ability = create_test_ability("TestAbility");
    ability.net_execution_policy = Some("ServerInitiated".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::ServerInitiated);
}

#[test]
fn test_net_execution_policy_server_only() {
    let mut ability = create_test_ability("TestAbility");
    ability.net_execution_policy = Some("ServerOnly".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::ServerOnly);
}

#[test]
fn test_invalid_instancing_policy() {
    let mut ability = create_test_ability("TestAbility");
    ability.instancing_policy = Some("InvalidPolicy".to_string());

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_invalid_replication_policy() {
    let mut ability = create_test_ability("TestAbility");
    ability.replication_policy = Some("InvalidPolicy".to_string());

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_invalid_net_execution_policy() {
    let mut ability = create_test_ability("TestAbility");
    ability.net_execution_policy = Some("InvalidPolicy".to_string());

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

// ============================================================================
// Tag Array Tests
// ============================================================================

#[test]
fn test_ability_with_tags() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["Ability.Jump".to_string()];
    ability.activation_required_tags = vec!["Status.Grounded".to_string()];
    ability.activation_blocked_tags =
        vec!["Status.Stunned".to_string(), "Status.Rooted".to_string()];

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();

    assert_eq!(ir.ability_tags, vec!["Ability.Jump"]);
    assert_eq!(ir.activation_required_tags, vec!["Status.Grounded"]);
    assert_eq!(
        ir.activation_blocked_tags,
        vec!["Status.Stunned", "Status.Rooted"]
    );
}

#[test]
fn test_ability_with_all_tag_types() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["Ability.Attack".to_string()];
    ability.activation_required_tags = vec!["Status.Alive".to_string()];
    ability.activation_blocked_tags = vec!["Status.Stunned".to_string()];
    ability.activation_owned_tags = vec!["Status.Attacking".to_string()];
    ability.cancel_abilities_with_tag = vec!["Ability.Defend".to_string()];
    ability.block_abilities_with_tag = vec!["Ability.Move".to_string()];

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();

    assert_eq!(ir.ability_tags, vec!["Ability.Attack"]);
    assert_eq!(ir.activation_required_tags, vec!["Status.Alive"]);
    assert_eq!(ir.activation_blocked_tags, vec!["Status.Stunned"]);
    assert_eq!(ir.activation_owned_tags, vec!["Status.Attacking"]);
    assert_eq!(ir.cancel_abilities_with_tag, vec!["Ability.Defend"]);
    assert_eq!(ir.block_abilities_with_tag, vec!["Ability.Move"]);
}

#[test]
fn test_tag_validation_valid() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec![
        "Ability.Jump".to_string(),
        "Status.Grounded".to_string(),
        "Effect.Buff.Speed".to_string(),
    ];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_ok());
}

#[test]
fn test_tag_validation_empty_tag() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["".to_string()];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_tag_validation_empty_component() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["Ability..Jump".to_string()];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_tag_validation_invalid_start() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["1Ability.Jump".to_string()];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_tag_validation_invalid_char() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["Ability.Jump!".to_string()];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

#[test]
fn test_tag_validation_underscore_allowed() {
    let mut ability = create_test_ability("TestAbility");
    ability.ability_tags = vec!["Ability.Jump_High".to_string()];

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_ok());
}

// ============================================================================
// Cost and Cooldown Tests
// ============================================================================

#[test]
fn test_ability_with_cost() {
    let mut ability = create_test_ability("TestAbility");
    ability.cost_effect = Some("StaminaCost".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.cost_effect, Some("StaminaCost".to_string()));
}

#[test]
fn test_ability_with_cooldown() {
    let mut ability = create_test_ability("TestAbility");
    ability.cooldown_effect = Some("JumpCooldown".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.cooldown_effect, Some("JumpCooldown".to_string()));
}

#[test]
fn test_ability_with_cost_and_cooldown() {
    let mut ability = create_test_ability("TestAbility");
    ability.cost_effect = Some("ManaCost".to_string());
    ability.cooldown_effect = Some("SpellCooldown".to_string());

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert_eq!(ir.cost_effect, Some("ManaCost".to_string()));
    assert_eq!(ir.cooldown_effect, Some("SpellCooldown".to_string()));
}

// ============================================================================
// Lifecycle Hook Tests
// ============================================================================

#[test]
fn test_lifecycle_hook_can_activate() {
    let mut ability = create_test_ability("TestAbility");
    ability
        .methods
        .push(create_test_function("can_activate_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.can_activate_ability.is_some());
    assert_eq!(
        ir.lifecycle_hooks.can_activate_ability.unwrap().name,
        "can_activate_ability"
    );
}

#[test]
fn test_lifecycle_hook_activate() {
    let mut ability = create_test_ability("TestAbility");
    ability
        .methods
        .push(create_test_function("activate_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.activate_ability.is_some());
    assert_eq!(
        ir.lifecycle_hooks.activate_ability.unwrap().name,
        "activate_ability"
    );
}

#[test]
fn test_lifecycle_hook_end() {
    let mut ability = create_test_ability("TestAbility");
    ability.methods.push(create_test_function("end_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.end_ability.is_some());
    assert_eq!(ir.lifecycle_hooks.end_ability.unwrap().name, "end_ability");
}

#[test]
fn test_lifecycle_hook_cancel() {
    let mut ability = create_test_ability("TestAbility");
    ability.methods.push(create_test_function("cancel_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.cancel_ability.is_some());
}

#[test]
fn test_lifecycle_hook_commit() {
    let mut ability = create_test_ability("TestAbility");
    ability.methods.push(create_test_function("commit_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.commit_ability.is_some());
}

#[test]
fn test_lifecycle_hook_input_pressed() {
    let mut ability = create_test_ability("TestAbility");
    ability.methods.push(create_test_function("input_pressed"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.input_pressed.is_some());
}

#[test]
fn test_lifecycle_hook_input_released() {
    let mut ability = create_test_ability("TestAbility");
    ability.methods.push(create_test_function("input_released"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.input_released.is_some());
}

#[test]
fn test_multiple_lifecycle_hooks() {
    let mut ability = create_test_ability("TestAbility");
    ability
        .methods
        .push(create_test_function("can_activate_ability"));
    ability
        .methods
        .push(create_test_function("activate_ability"));
    ability.methods.push(create_test_function("end_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();
    assert!(ir.lifecycle_hooks.can_activate_ability.is_some());
    assert!(ir.lifecycle_hooks.activate_ability.is_some());
    assert!(ir.lifecycle_hooks.end_ability.is_some());
}

#[test]
fn test_unknown_method_ignored() {
    let mut ability = create_test_ability("TestAbility");
    ability
        .methods
        .push(create_test_function("helper_function"));

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_ok());
}

// ============================================================================
// Validation Tests
// ============================================================================

#[test]
fn test_missing_ability_attribute() {
    let mut ability = create_test_ability("TestAbility");
    ability.attributes.clear();

    let result = GameplayAbilityIR::from_ast(&ability);
    assert!(result.is_err());
}

// ============================================================================
// Integration Tests
// ============================================================================

#[test]
fn test_complete_ability() {
    let mut ability = create_test_ability("JumpAbility");
    ability.instancing_policy = Some("InstancedPerExecution".to_string());
    ability.replication_policy = Some("ReplicateYes".to_string());
    ability.net_execution_policy = Some("LocalPredicted".to_string());
    ability.ability_tags = vec!["Ability.Jump".to_string()];
    ability.activation_required_tags = vec!["Status.Grounded".to_string()];
    ability.activation_blocked_tags =
        vec!["Status.Stunned".to_string(), "Status.Rooted".to_string()];
    ability.activation_owned_tags = vec!["Status.Jumping".to_string()];
    ability.cost_effect = Some("StaminaCost".to_string());
    ability.cooldown_effect = Some("JumpCooldown".to_string());
    ability
        .methods
        .push(create_test_function("can_activate_ability"));
    ability
        .methods
        .push(create_test_function("activate_ability"));
    ability.methods.push(create_test_function("end_ability"));

    let ir = GameplayAbilityIR::from_ast(&ability).unwrap();

    assert_eq!(ir.name, "JumpAbility");
    assert_eq!(
        ir.instancing_policy,
        InstancingPolicy::InstancedPerExecution
    );
    assert_eq!(ir.replication_policy, ReplicationPolicy::ReplicateYes);
    assert_eq!(ir.net_execution_policy, NetExecutionPolicy::LocalPredicted);
    assert_eq!(ir.ability_tags, vec!["Ability.Jump"]);
    assert_eq!(ir.activation_required_tags, vec!["Status.Grounded"]);
    assert_eq!(
        ir.activation_blocked_tags,
        vec!["Status.Stunned", "Status.Rooted"]
    );
    assert_eq!(ir.activation_owned_tags, vec!["Status.Jumping"]);
    assert_eq!(ir.cost_effect, Some("StaminaCost".to_string()));
    assert_eq!(ir.cooldown_effect, Some("JumpCooldown".to_string()));
    assert!(ir.lifecycle_hooks.can_activate_ability.is_some());
    assert!(ir.lifecycle_hooks.activate_ability.is_some());
    assert!(ir.lifecycle_hooks.end_ability.is_some());
}
