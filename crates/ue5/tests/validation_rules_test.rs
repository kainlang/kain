// Tests for data-driven validation rules system

use ue5::validation_rules::{
    RuleCategory, RuleCondition, Severity, ValidationRule, ValidationRules,
};

#[test]
fn test_disabled_rule_filtering() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "enabled_rule".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Test message".to_string(),
                suggestion: Some("Test suggestion".to_string()),
                disabled: false,
            },
            ValidationRule {
                id: "disabled_rule".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test2".to_string()],
                },
                message: "Test2 message".to_string(),
                suggestion: None,
                disabled: true,
            },
        ],
    };

    let enabled = rules.enabled_rules();
    assert_eq!(enabled.len(), 1);
    assert_eq!(enabled[0].id, "enabled_rule");
    assert_eq!(enabled[0].message, "Test message");
    assert_eq!(enabled[0].suggestion, Some("Test suggestion".to_string()));
}

#[test]
fn test_custom_message_and_suggestion() {
    let rule = ValidationRule {
        id: "test_rule".to_string(),
        category: RuleCategory::Naming,
        severity: Severity::Error,
        condition: RuleCondition::TypeCollision {
            type_names: vec!["Test".to_string()],
        },
        message: "Custom error message".to_string(),
        suggestion: Some("Custom suggestion for fixing".to_string()),
        disabled: false,
    };

    assert_eq!(rule.message, "Custom error message");
    assert_eq!(
        rule.suggestion,
        Some("Custom suggestion for fixing".to_string())
    );
}

#[test]
fn test_rule_severity_levels() {
    let error_rule = ValidationRule {
        id: "error_rule".to_string(),
        category: RuleCategory::Naming,
        severity: Severity::Error,
        condition: RuleCondition::TypeCollision {
            type_names: vec!["Test".to_string()],
        },
        message: "Error".to_string(),
        suggestion: None,
        disabled: false,
    };

    let warning_rule = ValidationRule {
        id: "warning_rule".to_string(),
        category: RuleCategory::Naming,
        severity: Severity::Warning,
        condition: RuleCondition::TypeCollision {
            type_names: vec!["Test".to_string()],
        },
        message: "Warning".to_string(),
        suggestion: None,
        disabled: false,
    };

    let info_rule = ValidationRule {
        id: "info_rule".to_string(),
        category: RuleCategory::Naming,
        severity: Severity::Info,
        condition: RuleCondition::TypeCollision {
            type_names: vec!["Test".to_string()],
        },
        message: "Info".to_string(),
        suggestion: None,
        disabled: false,
    };

    assert_eq!(error_rule.severity, Severity::Error);
    assert_eq!(warning_rule.severity, Severity::Warning);
    assert_eq!(info_rule.severity, Severity::Info);
}

#[test]
fn test_rules_by_category() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "naming_rule".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Naming".to_string(),
                suggestion: None,
                disabled: false,
            },
            ValidationRule {
                id: "replication_rule".to_string(),
                category: RuleCategory::Replication,
                severity: Severity::Error,
                condition: RuleCondition::InvalidRpcNaming {
                    pattern: "^Server_".to_string(),
                },
                message: "Replication".to_string(),
                suggestion: None,
                disabled: false,
            },
        ],
    };

    let naming_rules = rules.rules_by_category(RuleCategory::Naming);
    assert_eq!(naming_rules.len(), 1);
    assert_eq!(naming_rules[0].id, "naming_rule");

    let replication_rules = rules.rules_by_category(RuleCategory::Replication);
    assert_eq!(replication_rules.len(), 1);
    assert_eq!(replication_rules[0].id, "replication_rule");
}

#[test]
fn test_conflict_detection_type_collision() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "rule1".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string(), "Foo".to_string()],
                },
                message: "Rule 1".to_string(),
                suggestion: None,
                disabled: false,
            },
            ValidationRule {
                id: "rule2".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Warning, // Different severity
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string(), "Bar".to_string()], // Overlapping "Test"
                },
                message: "Rule 2".to_string(),
                suggestion: None,
                disabled: false,
            },
        ],
    };

    let conflicts = rules.detect_conflicts();
    assert_eq!(conflicts.len(), 1);
    assert_eq!(conflicts[0].0, "rule1");
    assert_eq!(conflicts[0].1, "rule2");
    assert!(conflicts[0].2.contains("Test"));
}

#[test]
fn test_no_conflict_same_severity() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "rule1".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Rule 1".to_string(),
                suggestion: None,
                disabled: false,
            },
            ValidationRule {
                id: "rule2".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error, // Same severity
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Rule 2".to_string(),
                suggestion: None,
                disabled: false,
            },
        ],
    };

    let conflicts = rules.detect_conflicts();
    assert_eq!(conflicts.len(), 0); // No conflict because same severity
}

#[test]
fn test_conflict_detection_disabled_rules() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "rule1".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Error,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Rule 1".to_string(),
                suggestion: None,
                disabled: true, // Disabled
            },
            ValidationRule {
                id: "rule2".to_string(),
                category: RuleCategory::Naming,
                severity: Severity::Warning,
                condition: RuleCondition::TypeCollision {
                    type_names: vec!["Test".to_string()],
                },
                message: "Rule 2".to_string(),
                suggestion: None,
                disabled: false,
            },
        ],
    };

    let conflicts = rules.detect_conflicts();
    assert_eq!(conflicts.len(), 0); // No conflict because rule1 is disabled
}

#[test]
fn test_conflict_detection_incompatible_attributes() {
    let rules = ValidationRules {
        version: "1.0.0".to_string(),
        rules: vec![
            ValidationRule {
                id: "rule1".to_string(),
                category: RuleCategory::AttributeCombination,
                severity: Severity::Error,
                condition: RuleCondition::IncompatibleAttributes {
                    attributes: vec![("replicated".to_string(), "transient".to_string())],
                },
                message: "Rule 1".to_string(),
                suggestion: None,
                disabled: false,
            },
            ValidationRule {
                id: "rule2".to_string(),
                category: RuleCategory::AttributeCombination,
                severity: Severity::Warning, // Different severity
                condition: RuleCondition::IncompatibleAttributes {
                    attributes: vec![
                        ("replicated".to_string(), "transient".to_string()), // Same pair
                    ],
                },
                message: "Rule 2".to_string(),
                suggestion: None,
                disabled: false,
            },
        ],
    };

    let conflicts = rules.detect_conflicts();
    assert_eq!(conflicts.len(), 1);
    assert_eq!(conflicts[0].0, "rule1");
    assert_eq!(conflicts[0].1, "rule2");
}
