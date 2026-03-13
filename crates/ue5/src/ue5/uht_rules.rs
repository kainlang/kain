//! UHT Validation Rules Database
//!
//! Data-driven validation rules extracted from Epic's Unreal Header Tool (EpicGames.UHT)
//! source code. Replaces hardcoded oracle rules with 337 validation rules, 154 specifier
//! definitions, 41 property type constraints, and 25 incompatible combinations extracted
//! directly from UE5's own validation logic.
//!
//! Loaded from `unreal/metadata/uht_rules.json` at compile time via `Ue5Context`.

use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};

// ═══════════════════════════════════════════════════════════════════
// Schema Types — mirrors uht_rules.json structure
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhtRulesData {
    #[serde(default)]
    pub _meta: UhtMeta,
    #[serde(default)]
    pub validation_rules: Vec<UhtValidationRule>,
    #[serde(default)]
    pub specifiers: Vec<UhtSpecifier>,
    #[serde(default)]
    pub property_types: Vec<UhtPropertyType>,
    #[serde(default)]
    pub incompatible_combos: Vec<UhtIncompatibleCombo>,
    #[serde(default)]
    pub kain_rules: HashMap<String, Vec<UhtValidationRule>>,
    #[serde(default)]
    pub replication_rules: Option<ReplicationRules>,
    #[serde(default)]
    pub attribute_compatibility_matrix: Option<AttributeCompatibilityMatrix>,
    #[serde(default)]
    pub kain_specific_rules: Option<KainSpecificRules>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct UhtMeta {
    #[serde(default)]
    pub generator: String,
    #[serde(default)]
    pub source: String,
    #[serde(default)]
    pub total_rules: usize,
    #[serde(default)]
    pub total_specifiers: usize,
    #[serde(default)]
    pub total_property_types: usize,
    #[serde(default)]
    pub total_incompatible_combos: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhtValidationRule {
    #[serde(default)]
    pub severity: String,
    #[serde(default)]
    pub message: String,
    #[serde(default)]
    pub message_raw: String,
    #[serde(default)]
    pub source_file: String,
    #[serde(default)]
    pub line: usize,
    #[serde(default)]
    pub context: UhtRuleContext,
    #[serde(default)]
    pub category: String,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct UhtRuleContext {
    #[serde(default)]
    pub method: String,
    #[serde(default)]
    pub class: String,
    #[serde(default)]
    pub specifier_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhtSpecifier {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub applies_to: String,
    #[serde(default)]
    pub value_type: String,
    #[serde(default)]
    pub source_file: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhtPropertyType {
    #[serde(default)]
    pub uht_class: String,
    #[serde(default)]
    pub type_name: String,
    #[serde(default)]
    pub engine_class_name: String,
    #[serde(default)]
    pub is_container: bool,
    #[serde(default)]
    pub constraints: Vec<String>,
    #[serde(default)]
    pub source_file: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UhtIncompatibleCombo {
    #[serde(default)]
    pub specifier_a: String,
    #[serde(default)]
    pub specifier_b: Option<String>,
    #[serde(default)]
    pub message: String,
    #[serde(default)]
    pub category: String,
    #[serde(default)]
    pub severity: String,
    #[serde(default)]
    pub constraint: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReplicationRules {
    #[serde(default)]
    pub property_replication: PropertyReplication,
    #[serde(default)]
    pub rpc_validation: RpcValidation,
    #[serde(default)]
    pub lifetime_replication: LifetimeReplication,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PropertyReplication {
    #[serde(default)]
    pub allowed_types: Vec<String>,
    #[serde(default)]
    pub disallowed_types: Vec<String>,
    #[serde(default)]
    pub constraints: Vec<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct RpcValidation {
    #[serde(default)]
    pub naming_conventions: HashMap<String, String>,
    #[serde(default)]
    pub required_specifiers: Vec<String>,
    #[serde(default)]
    pub constraints: Vec<String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct LifetimeReplication {
    #[serde(default)]
    pub required_includes: Vec<String>,
    #[serde(default)]
    pub required_macros: Vec<String>,
    #[serde(default)]
    pub function_signature: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AttributeCompatibilityMatrix {
    #[serde(default)]
    pub property_attributes: HashMap<String, AttributeCompatibility>,
    #[serde(default)]
    pub function_attributes: HashMap<String, AttributeCompatibility>,
    #[serde(default)]
    pub class_attributes: HashMap<String, AttributeCompatibility>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AttributeCompatibility {
    #[serde(default)]
    pub compatible_with: Vec<String>,
    #[serde(default)]
    pub incompatible_with: Vec<String>,
    #[serde(default)]
    pub requires_one_of: Option<Vec<String>>,
    #[serde(default)]
    pub implies: Option<Vec<String>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KainSpecificRules {
    #[serde(default)]
    pub actor_rules: Vec<String>,
    #[serde(default)]
    pub struct_rules: Vec<String>,
    #[serde(default)]
    pub enum_rules: Vec<String>,
    #[serde(default)]
    pub component_rules: Vec<String>,
    #[serde(default)]
    pub delegate_rules: Vec<String>,
}

// ═══════════════════════════════════════════════════════════════════
// UhtRules — Query API
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Default, Clone)]
pub struct UhtRules {
    /// All validation rules indexed by category
    rules_by_category: HashMap<String, Vec<UhtValidationRule>>,

    /// Valid specifier names grouped by what they apply to
    /// e.g., "class" -> {"NoExport", "Intrinsic", "Abstract", ...}
    specifiers_by_type: HashMap<String, HashSet<String>>,

    /// All valid specifier names (flat set for quick lookup)
    all_specifier_names: HashSet<String>,

    /// Property type constraints
    /// e.g., "Map" -> ["Nested containers not supported", ...]
    property_constraints: HashMap<String, Vec<String>>,

    /// Container property types (Array, Map, Set, Optional)
    container_types: HashSet<String>,

    /// Incompatible specifier combinations
    incompatible: Vec<UhtIncompatibleCombo>,

    /// KAIN-relevant rules grouped by KAIN construct
    kain_rules: HashMap<String, Vec<UhtValidationRule>>,

    /// Replication rules (property types, RPC validation, lifetime replication)
    replication_rules: Option<ReplicationRules>,

    /// Attribute compatibility matrix (property, function, class attributes)
    attribute_compatibility: Option<AttributeCompatibilityMatrix>,

    /// KAIN-specific rules (actor, struct, enum, component, delegate)
    kain_specific_rules: Option<KainSpecificRules>,

    /// Total counts for diagnostics
    total_rules: usize,
    total_specifiers: usize,
}

impl UhtRules {
    pub fn new() -> Self {
        Self::default()
    }

    /// Load from JSON string (uht_rules.json content)
    pub fn load(&mut self, json_data: &str) -> Result<(), String> {
        let data: UhtRulesData = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse uht_rules.json: {}", e))?;

        // Index validation rules by category
        for rule in &data.validation_rules {
            self.rules_by_category
                .entry(rule.category.clone())
                .or_default()
                .push(rule.clone());
        }

        // Index specifiers by type
        for spec in &data.specifiers {
            self.specifiers_by_type
                .entry(spec.applies_to.clone())
                .or_default()
                .insert(spec.name.clone());
            self.all_specifier_names.insert(spec.name.to_lowercase());
        }

        // Index property types
        for pt in &data.property_types {
            if !pt.constraints.is_empty() {
                self.property_constraints
                    .insert(pt.type_name.clone(), pt.constraints.clone());
            }
            if pt.is_container {
                self.container_types.insert(pt.type_name.clone());
            }
        }

        // Store incompatible combos
        self.incompatible = data.incompatible_combos;

        // Store KAIN-relevant rules
        self.kain_rules = data.kain_rules;

        // Store new sections
        self.replication_rules = data.replication_rules;
        self.attribute_compatibility = data.attribute_compatibility_matrix;
        self.kain_specific_rules = data.kain_specific_rules;

        self.total_rules = data.validation_rules.len();
        self.total_specifiers = data.specifiers.len();

        Ok(())
    }

    // ─── Query API ───────────────────────────────────────────────

    /// Check if a specifier name is valid for a given type (class, function, property, struct, enum)
    pub fn is_valid_specifier(&self, name: &str, applies_to: &str) -> bool {
        if let Some(specs) = self.specifiers_by_type.get(applies_to) {
            specs.contains(name)
        } else {
            false
        }
    }

    /// Check if a specifier name exists at all (any type)
    pub fn is_known_specifier(&self, name: &str) -> bool {
        self.all_specifier_names.contains(&name.to_lowercase())
    }

    /// Get all valid specifiers for a given type
    pub fn specifiers_for(&self, applies_to: &str) -> Vec<&str> {
        if let Some(specs) = self.specifiers_by_type.get(applies_to) {
            specs.iter().map(|s| s.as_str()).collect()
        } else {
            Vec::new()
        }
    }

    /// Check if two specifiers are incompatible
    pub fn are_incompatible(&self, spec_a: &str, spec_b: &str) -> Option<&str> {
        let a_lower = spec_a.to_lowercase();
        let b_lower = spec_b.to_lowercase();

        for combo in &self.incompatible {
            let ca = combo.specifier_a.to_lowercase();
            let cb = combo
                .specifier_b
                .as_ref()
                .map(|s| s.to_lowercase())
                .unwrap_or_default();

            if (ca == a_lower && cb == b_lower) || (ca == b_lower && cb == a_lower) {
                return Some(&combo.message);
            }
        }
        None
    }

    /// Get all incompatible combinations for a specifier
    pub fn incompatible_with(&self, specifier: &str) -> Vec<(&str, &str)> {
        let s_lower = specifier.to_lowercase();
        let mut results = Vec::new();

        for combo in &self.incompatible {
            let ca = combo.specifier_a.to_lowercase();
            let cb = combo
                .specifier_b
                .as_ref()
                .map(|s| s.to_lowercase())
                .unwrap_or_default();

            if ca == s_lower {
                results.push((
                    combo.specifier_b.as_deref().unwrap_or(""),
                    combo.message.as_str(),
                ));
            } else if cb == s_lower {
                results.push((combo.specifier_a.as_str(), combo.message.as_str()));
            }
        }
        results
    }

    /// Check if a property type is a container (Array, Map, Set, Optional)
    pub fn is_container_type(&self, type_name: &str) -> bool {
        self.container_types.contains(type_name)
    }

    /// Get constraints for a property type
    pub fn property_type_constraints(&self, type_name: &str) -> &[String] {
        self.property_constraints
            .get(type_name)
            .map(|v| v.as_slice())
            .unwrap_or(&[])
    }

    /// Get KAIN-relevant rules for a specific construct type
    pub fn rules_for_kain_construct(&self, construct: &str) -> &[UhtValidationRule] {
        self.kain_rules
            .get(construct)
            .map(|v| v.as_slice())
            .unwrap_or(&[])
    }

    /// Get rules that match a search term in their message
    pub fn search_rules(&self, query: &str) -> Vec<&UhtValidationRule> {
        let q_lower = query.to_lowercase();
        let mut results = Vec::new();

        for rules in self.rules_by_category.values() {
            for rule in rules {
                if rule.message.to_lowercase().contains(&q_lower) {
                    results.push(rule);
                }
            }
        }
        results
    }

    /// Get total loaded counts
    pub fn stats(&self) -> (usize, usize) {
        (self.total_rules, self.total_specifiers)
    }

    /// Check if any data has been loaded
    pub fn is_loaded(&self) -> bool {
        self.total_rules > 0
    }

    // ─── Replication Rules API ───────────────────────────────────────

    /// Check if a type is allowed for replication
    pub fn is_replicable_type(&self, type_name: &str) -> bool {
        if let Some(rules) = &self.replication_rules {
            rules
                .property_replication
                .allowed_types
                .iter()
                .any(|t| t.eq_ignore_ascii_case(type_name))
        } else {
            false
        }
    }

    /// Check if a type is explicitly disallowed for replication
    pub fn is_non_replicable_type(&self, type_name: &str) -> bool {
        if let Some(rules) = &self.replication_rules {
            rules
                .property_replication
                .disallowed_types
                .iter()
                .any(|t| t.eq_ignore_ascii_case(type_name))
        } else {
            false
        }
    }

    /// Get replication constraints for properties
    pub fn replication_constraints(&self) -> &[String] {
        if let Some(rules) = &self.replication_rules {
            &rules.property_replication.constraints
        } else {
            &[]
        }
    }

    /// Get RPC naming convention for a given RPC type (Server, Client, Multicast)
    pub fn rpc_naming_convention(&self, rpc_type: &str) -> Option<&str> {
        self.replication_rules
            .as_ref()
            .and_then(|r| r.rpc_validation.naming_conventions.get(rpc_type))
            .map(|s| s.as_str())
    }

    /// Get RPC validation constraints
    pub fn rpc_constraints(&self) -> &[String] {
        if let Some(rules) = &self.replication_rules {
            &rules.rpc_validation.constraints
        } else {
            &[]
        }
    }

    /// Get GetLifetimeReplicatedProps function signature
    pub fn lifetime_replication_signature(&self) -> Option<&str> {
        self.replication_rules
            .as_ref()
            .map(|r| r.lifetime_replication.function_signature.as_str())
    }

    /// Get required includes for replication
    pub fn replication_includes(&self) -> &[String] {
        if let Some(rules) = &self.replication_rules {
            &rules.lifetime_replication.required_includes
        } else {
            &[]
        }
    }

    // ─── Attribute Compatibility API ─────────────────────────────────

    /// Check if two property attributes are compatible
    pub fn are_property_attributes_compatible(&self, attr1: &str, attr2: &str) -> bool {
        if let Some(matrix) = &self.attribute_compatibility {
            if let Some(compat) = matrix.property_attributes.get(attr1) {
                !compat
                    .incompatible_with
                    .iter()
                    .any(|a| a.eq_ignore_ascii_case(attr2))
            } else {
                true // Unknown attributes are assumed compatible
            }
        } else {
            true
        }
    }

    /// Check if two function attributes are compatible
    pub fn are_function_attributes_compatible(&self, attr1: &str, attr2: &str) -> bool {
        if let Some(matrix) = &self.attribute_compatibility {
            if let Some(compat) = matrix.function_attributes.get(attr1) {
                !compat
                    .incompatible_with
                    .iter()
                    .any(|a| a.eq_ignore_ascii_case(attr2))
            } else {
                true
            }
        } else {
            true
        }
    }

    /// Get required attributes for a given attribute (e.g., Server RPC requires Reliable or Unreliable)
    pub fn required_attributes_for(&self, attr: &str, attr_type: &str) -> Option<&[String]> {
        let matrix = self.attribute_compatibility.as_ref()?;

        let compat = match attr_type {
            "property" => matrix.property_attributes.get(attr)?,
            "function" => matrix.function_attributes.get(attr)?,
            "class" => matrix.class_attributes.get(attr)?,
            _ => return None,
        };

        compat.requires_one_of.as_ref().map(|v| v.as_slice())
    }

    /// Get attributes implied by a given attribute (e.g., Abstract implies NotPlaceable)
    pub fn implied_attributes(&self, attr: &str, attr_type: &str) -> Option<&[String]> {
        let matrix = self.attribute_compatibility.as_ref()?;

        let compat = match attr_type {
            "property" => matrix.property_attributes.get(attr)?,
            "function" => matrix.function_attributes.get(attr)?,
            "class" => matrix.class_attributes.get(attr)?,
            _ => return None,
        };

        compat.implies.as_ref().map(|v| v.as_slice())
    }

    // ─── KAIN-Specific Rules API ─────────────────────────────────────

    /// Get KAIN-specific rules for actors
    pub fn kain_actor_rules(&self) -> &[String] {
        self.kain_specific_rules
            .as_ref()
            .map(|r| r.actor_rules.as_slice())
            .unwrap_or(&[])
    }

    /// Get KAIN-specific rules for structs
    pub fn kain_struct_rules(&self) -> &[String] {
        self.kain_specific_rules
            .as_ref()
            .map(|r| r.struct_rules.as_slice())
            .unwrap_or(&[])
    }

    /// Get KAIN-specific rules for enums
    pub fn kain_enum_rules(&self) -> &[String] {
        self.kain_specific_rules
            .as_ref()
            .map(|r| r.enum_rules.as_slice())
            .unwrap_or(&[])
    }

    /// Get KAIN-specific rules for components
    pub fn kain_component_rules(&self) -> &[String] {
        self.kain_specific_rules
            .as_ref()
            .map(|r| r.component_rules.as_slice())
            .unwrap_or(&[])
    }

    /// Get KAIN-specific rules for delegates
    pub fn kain_delegate_rules(&self) -> &[String] {
        self.kain_specific_rules
            .as_ref()
            .map(|r| r.delegate_rules.as_slice())
            .unwrap_or(&[])
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_json() -> String {
        r#"{
            "_meta": {
                "generator": "test",
                "total_rules": 3,
                "total_specifiers": 5,
                "total_property_types": 2,
                "total_incompatible_combos": 2
            },
            "validation_rules": [
                {
                    "severity": "error",
                    "message": "Struct members cannot be replicated",
                    "category": "property_specifier",
                    "context": {"specifier_type": "property"}
                },
                {
                    "severity": "error",
                    "message": "BlueprintImplementableEvent functions cannot be replicated!",
                    "category": "function_specifier",
                    "context": {"specifier_type": "function"}
                },
                {
                    "severity": "warning",
                    "message": "The dependsOn specifier is deprecated",
                    "category": "class_specifier",
                    "context": {"specifier_type": "class"}
                }
            ],
            "specifiers": [
                {"name": "BlueprintCallable", "applies_to": "function", "value_type": "Legacy"},
                {"name": "BlueprintPure", "applies_to": "function", "value_type": "OptionalString"},
                {"name": "Replicated", "applies_to": "property", "value_type": "Legacy"},
                {"name": "EditAnywhere", "applies_to": "property", "value_type": "Legacy"},
                {"name": "Abstract", "applies_to": "class", "value_type": "Legacy"}
            ],
            "property_types": [
                {"type_name": "Array", "is_container": true, "constraints": ["Nested containers are not supported"]},
                {"type_name": "Map", "is_container": true, "constraints": ["TMap key must be hashable", "Nested containers not supported"]}
            ],
            "incompatible_combos": [
                {
                    "specifier_a": "BlueprintReadOnly",
                    "specifier_b": "BlueprintReadWrite",
                    "message": "Cannot specify both BlueprintReadOnly and BlueprintReadWrite",
                    "severity": "error"
                },
                {
                    "specifier_a": "BlueprintImplementableEvent",
                    "specifier_b": "BlueprintNativeEvent",
                    "message": "Cannot be both BlueprintImplementableEvent and BlueprintNativeEvent",
                    "severity": "error"
                }
            ],
            "kain_rules": {
                "property": [
                    {
                        "severity": "error",
                        "message": "Struct members cannot be replicated",
                        "category": "property_specifier"
                    }
                ],
                "function": [
                    {
                        "severity": "error",
                        "message": "BlueprintImplementableEvent functions cannot be replicated!",
                        "category": "function_specifier"
                    }
                ]
            }
        }"#.to_string()
    }

    #[test]
    fn test_load_uht_rules() {
        let mut rules = UhtRules::new();
        assert!(!rules.is_loaded());

        rules.load(&make_test_json()).unwrap();
        assert!(rules.is_loaded());

        let (total_rules, total_specs) = rules.stats();
        assert_eq!(total_rules, 3);
        assert_eq!(total_specs, 5);
    }

    #[test]
    fn test_specifier_lookup() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        assert!(rules.is_valid_specifier("BlueprintCallable", "function"));
        assert!(rules.is_valid_specifier("BlueprintPure", "function"));
        assert!(!rules.is_valid_specifier("BlueprintCallable", "property"));
        assert!(rules.is_valid_specifier("Replicated", "property"));
        assert!(rules.is_valid_specifier("Abstract", "class"));
    }

    #[test]
    fn test_known_specifier() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        assert!(rules.is_known_specifier("BlueprintCallable"));
        assert!(rules.is_known_specifier("blueprintcallable"));
        assert!(!rules.is_known_specifier("TotallyFakeSpecifier"));
    }

    #[test]
    fn test_incompatible_combos() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        let result = rules.are_incompatible("BlueprintReadOnly", "BlueprintReadWrite");
        assert!(result.is_some());
        assert!(result.unwrap().contains("Cannot specify both"));

        let result2 = rules.are_incompatible("BlueprintCallable", "Replicated");
        assert!(result2.is_none());
    }

    #[test]
    fn test_container_types() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        assert!(rules.is_container_type("Array"));
        assert!(rules.is_container_type("Map"));
        assert!(!rules.is_container_type("Float"));
    }

    #[test]
    fn test_property_constraints() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        let constraints = rules.property_type_constraints("Map");
        assert_eq!(constraints.len(), 2);

        let no_constraints = rules.property_type_constraints("Float");
        assert_eq!(no_constraints.len(), 0);
    }

    #[test]
    fn test_kain_rules() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        let prop_rules = rules.rules_for_kain_construct("property");
        assert_eq!(prop_rules.len(), 1);
        assert!(prop_rules[0].message.contains("replicated"));

        let func_rules = rules.rules_for_kain_construct("function");
        assert_eq!(func_rules.len(), 1);
    }

    #[test]
    fn test_search_rules() {
        let mut rules = UhtRules::new();
        rules.load(&make_test_json()).unwrap();

        let results = rules.search_rules("replicated");
        assert!(results.len() >= 2);

        let results2 = rules.search_rules("deprecated");
        assert_eq!(results2.len(), 1);
    }
}
