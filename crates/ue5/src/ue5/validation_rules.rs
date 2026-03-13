// Data-driven validation rules system
// Allows loading validation rules from JSON configuration

use kain_core::error::{ErrorContext, KainError, KainResult};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;

/// Category of validation rule
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum RuleCategory {
    Naming,
    TypeCompatibility,
    AttributeCombination,
    Replication,
    Blueprint,
    Shader,
    Editor,
}

/// Severity level of rule violation
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum Severity {
    Error,
    Warning,
    Info,
}

/// Condition that triggers a validation rule
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum RuleCondition {
    /// Type name collides with reserved names
    TypeCollision { type_names: Vec<String> },
    /// Incompatible attribute combinations
    IncompatibleAttributes { attributes: Vec<(String, String)> },
    /// Invalid RPC naming pattern
    InvalidRpcNaming { pattern: String },
    /// Nested container types
    NestedContainer {
        outer: Vec<String>,
        inner: Vec<String>,
    },
    /// Invalid naming pattern
    InvalidNaming {
        pattern: String,
        applies_to: Vec<String>,
    },
    /// Missing required attribute
    MissingAttribute {
        required_attribute: String,
        when_attribute: String,
    },
    /// Forbidden type in specific context
    ForbiddenType {
        forbidden_types: Vec<String>,
        context: String,
    },
}

/// A single validation rule
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValidationRule {
    pub id: String,
    pub category: RuleCategory,
    pub severity: Severity,
    pub condition: RuleCondition,
    pub message: String,
    pub suggestion: Option<String>,
    #[serde(default)]
    pub disabled: bool,
}

/// Container for all validation rules
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValidationRules {
    pub version: String,
    pub rules: Vec<ValidationRule>,
}

impl ValidationRules {
    /// Load validation rules from JSON file
    pub fn load<P: AsRef<Path>>(path: P) -> KainResult<Self> {
        let path = path.as_ref();

        // If file doesn't exist, return empty rules (will use built-in defaults)
        if !path.exists() {
            return Ok(Self {
                version: "1.0.0".to_string(),
                rules: Vec::new(),
            });
        }

        let content = fs::read_to_string(path)
            .map_err(|e| KainError::io_error(format!("Failed to read validation rules: {}", e)))
            .with_file(path.to_path_buf())
            .with_context("Reading validation_rules.json")?;

        let rules: ValidationRules = serde_json::from_str(&content)
            .map_err(|e| {
                KainError::config_error(format!("Failed to parse validation rules: {}", e))
            })
            .with_file(path.to_path_buf())
            .with_context("Parsing validation_rules.json")
            .with_suggestion("Check JSON syntax and ensure it matches the schema")?;

        // Validate schema (basic checks)
        rules.validate_schema()?;

        Ok(rules)
    }

    /// Validate the rules schema
    fn validate_schema(&self) -> KainResult<()> {
        // Check version format
        if !self.version.contains('.') {
            return Err(KainError::config_error(
                "Invalid version format. Expected format: X.Y.Z",
            ));
        }

        // Check for duplicate rule IDs
        let mut seen_ids = HashMap::new();
        for rule in &self.rules {
            if let Some(first_occurrence) = seen_ids.insert(&rule.id, rule) {
                return Err(KainError::config_error(format!(
                    "Duplicate rule ID '{}' found in validation rules",
                    rule.id
                )));
            }
        }

        // Validate each rule
        for rule in &self.rules {
            rule.validate()?;
        }

        Ok(())
    }

    /// Get all enabled rules
    pub fn enabled_rules(&self) -> Vec<&ValidationRule> {
        self.rules.iter().filter(|r| !r.disabled).collect()
    }

    /// Get rules by category
    pub fn rules_by_category(&self, category: RuleCategory) -> Vec<&ValidationRule> {
        self.rules
            .iter()
            .filter(|r| !r.disabled && r.category == category)
            .collect()
    }

    /// Get rule by ID
    pub fn get_rule(&self, id: &str) -> Option<&ValidationRule> {
        self.rules.iter().find(|r| r.id == id)
    }

    /// Check for conflicting rules
    pub fn detect_conflicts(&self) -> Vec<(String, String, String)> {
        let mut conflicts = Vec::new();

        // Check for rules that might conflict
        for i in 0..self.rules.len() {
            for j in (i + 1)..self.rules.len() {
                let rule1 = &self.rules[i];
                let rule2 = &self.rules[j];

                if rule1.disabled || rule2.disabled {
                    continue;
                }

                // Check if rules conflict
                if let Some(reason) = Self::check_conflict(rule1, rule2) {
                    conflicts.push((rule1.id.clone(), rule2.id.clone(), reason));
                }
            }
        }

        conflicts
    }

    /// Check if two rules conflict
    fn check_conflict(rule1: &ValidationRule, rule2: &ValidationRule) -> Option<String> {
        // Check for conflicting type collision rules
        if let (
            RuleCondition::TypeCollision { type_names: names1 },
            RuleCondition::TypeCollision { type_names: names2 },
        ) = (&rule1.condition, &rule2.condition)
        {
            // If they have overlapping type names but different severities, that's a conflict
            let overlap: Vec<_> = names1.iter().filter(|n| names2.contains(n)).collect();
            if !overlap.is_empty() && rule1.severity != rule2.severity {
                return Some(format!(
                    "Both rules check type collision for {:?} but have different severities",
                    overlap
                ));
            }
        }

        // Check for conflicting incompatible attribute rules
        if let (
            RuleCondition::IncompatibleAttributes { attributes: attrs1 },
            RuleCondition::IncompatibleAttributes { attributes: attrs2 },
        ) = (&rule1.condition, &rule2.condition)
        {
            // If they have overlapping attribute pairs but different severities
            let overlap: Vec<_> = attrs1.iter().filter(|a| attrs2.contains(a)).collect();
            if !overlap.is_empty() && rule1.severity != rule2.severity {
                return Some(format!(
                    "Both rules check incompatible attributes {:?} but have different severities",
                    overlap
                ));
            }
        }

        None
    }
}

impl ValidationRule {
    /// Validate the rule structure
    fn validate(&self) -> KainResult<()> {
        // Check ID format
        if !self
            .id
            .chars()
            .all(|c| c.is_ascii_lowercase() || c == '_' || c.is_ascii_digit())
        {
            return Err(KainError::config_error(format!(
                "Invalid rule ID '{}'. Must be lowercase with underscores only",
                self.id
            )));
        }

        // Validate condition-specific requirements
        match &self.condition {
            RuleCondition::TypeCollision { type_names } => {
                if type_names.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': TypeCollision must have at least one type name",
                        self.id
                    )));
                }
            }
            RuleCondition::IncompatibleAttributes { attributes } => {
                if attributes.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': IncompatibleAttributes must have at least one attribute pair",
                        self.id
                    )));
                }
            }
            RuleCondition::InvalidRpcNaming { pattern } => {
                // Try to compile the regex to validate it
                if regex::Regex::new(pattern).is_err() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': Invalid regex pattern '{}'",
                        self.id, pattern
                    )));
                }
            }
            RuleCondition::NestedContainer { outer, inner } => {
                if outer.is_empty() || inner.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': NestedContainer must have both outer and inner types",
                        self.id
                    )));
                }
            }
            RuleCondition::InvalidNaming {
                pattern,
                applies_to,
            } => {
                if regex::Regex::new(pattern).is_err() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': Invalid regex pattern '{}'",
                        self.id, pattern
                    )));
                }
                if applies_to.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': InvalidNaming must specify what it applies to",
                        self.id
                    )));
                }
            }
            RuleCondition::MissingAttribute {
                required_attribute,
                when_attribute,
            } => {
                if required_attribute.is_empty() || when_attribute.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': MissingAttribute must specify both attributes",
                        self.id
                    )));
                }
            }
            RuleCondition::ForbiddenType {
                forbidden_types,
                context,
            } => {
                if forbidden_types.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': ForbiddenType must have at least one forbidden type",
                        self.id
                    )));
                }
                if context.is_empty() {
                    return Err(KainError::config_error(format!(
                        "Rule '{}': ForbiddenType must specify a context",
                        self.id
                    )));
                }
            }
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_load_validation_rules() {
        // Test loading from non-existent file (should return empty rules)
        let rules = ValidationRules::load("nonexistent.json").unwrap();
        assert_eq!(rules.rules.len(), 0);
    }

    #[test]
    fn test_rule_validation() {
        let rule = ValidationRule {
            id: "test_rule".to_string(),
            category: RuleCategory::Naming,
            severity: Severity::Error,
            condition: RuleCondition::TypeCollision {
                type_names: vec!["Test".to_string()],
            },
            message: "Test message".to_string(),
            suggestion: None,
            disabled: false,
        };

        assert!(rule.validate().is_ok());
    }

    #[test]
    fn test_invalid_rule_id() {
        let rule = ValidationRule {
            id: "TestRule".to_string(), // Invalid: contains uppercase
            category: RuleCategory::Naming,
            severity: Severity::Error,
            condition: RuleCondition::TypeCollision {
                type_names: vec!["Test".to_string()],
            },
            message: "Test message".to_string(),
            suggestion: None,
            disabled: false,
        };

        assert!(rule.validate().is_err());
    }

    #[test]
    fn test_enabled_rules_filter() {
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
                    message: "Test".to_string(),
                    suggestion: None,
                    disabled: false,
                },
                ValidationRule {
                    id: "disabled_rule".to_string(),
                    category: RuleCategory::Naming,
                    severity: Severity::Error,
                    condition: RuleCondition::TypeCollision {
                        type_names: vec!["Test2".to_string()],
                    },
                    message: "Test2".to_string(),
                    suggestion: None,
                    disabled: true,
                },
            ],
        };

        let enabled = rules.enabled_rules();
        assert_eq!(enabled.len(), 1);
        assert_eq!(enabled[0].id, "enabled_rule");
    }
}
