//! UE5 Virtual Method Obligations
//!
//! Data-driven pure virtual method tracking extracted from all C++ headers
//! in the Unreal Engine source tree. Provides query APIs to:
//!   - Look up what pure virtual methods a class requires subclasses to implement
//!   - Get default stub implementations for each obligation
//!   - Check if a base class has any unresolved obligations
//!
//! This prevents the entire class of linker errors caused by missing
//! pure virtual overrides (e.g., OnClose() in FAssetEditorToolkit).
//!
//! Loaded from `unreal/metadata/virtual_obligations.json` at compile time via `Ue5Context`.

use std::collections::HashMap;
use serde::{Deserialize, Serialize};

// ═══════════════════════════════════════════════════════════════════
// Schema Types — mirrors virtual_obligations.json structure
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VirtualObligationsData {
    #[serde(default)]
    pub _meta: ObligationsMeta,
    #[serde(default)]
    pub kain_focus: HashMap<String, ClassObligations>,
    #[serde(default)]
    pub obligations: HashMap<String, ClassObligations>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ObligationsMeta {
    #[serde(default)]
    pub generator: String,
    #[serde(default)]
    pub total_classes_scanned: usize,
    #[serde(default)]
    pub total_pure_virtual_declarations: usize,
    #[serde(default)]
    pub total_classes_with_obligations: usize,
    #[serde(default)]
    pub compact_classes_included: usize,
    #[serde(default)]
    pub kain_focus_classes: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClassObligations {
    #[serde(default, rename = "class")]
    pub class_name: String,
    #[serde(default)]
    pub parents: Vec<String>,
    #[serde(default)]
    pub header: String,
    #[serde(default, rename = "module")]
    pub module_name: String,
    #[serde(default)]
    pub category: String,
    #[serde(default)]
    pub obligation_count: usize,
    #[serde(default)]
    pub obligations: Vec<MethodObligation>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MethodObligation {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub return_type: String,
    #[serde(default)]
    pub params: Vec<ObligationParam>,
    #[serde(default)]
    pub is_const: bool,
    #[serde(default)]
    pub declared_in: String,
    #[serde(default)]
    pub default_body: String,
    #[serde(default)]
    pub raw_signature: String,
    #[serde(default)]
    pub override_declaration: String,
    #[serde(default)]
    pub override_definition: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ObligationParam {
    #[serde(default)]
    pub name: String,
    #[serde(default, rename = "type")]
    pub param_type: String,
    #[serde(default)]
    pub default_value: Option<String>,
}

// ═══════════════════════════════════════════════════════════════════
// VirtualObligations — Query API
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Default, Clone)]
pub struct VirtualObligations {
    /// Class name → obligations (KAIN-focus classes with full detail)
    kain_focus: HashMap<String, ClassObligations>,

    /// Class name → obligations (compact, all classes with ≤10 obligations)
    all_obligations: HashMap<String, ClassObligations>,

    /// Total stats
    total_classes: usize,
    total_obligations: usize,
}

impl VirtualObligations {
    pub fn new() -> Self {
        Self::default()
    }

    /// Load from JSON string (virtual_obligations.json content)
    pub fn load(&mut self, json_data: &str) -> Result<(), String> {
        let data: VirtualObligationsData = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse virtual_obligations.json: {}", e))?;

        self.kain_focus = data.kain_focus;
        self.all_obligations = data.obligations;

        self.total_classes = self.kain_focus.len() + self.all_obligations.len();
        self.total_obligations = self.kain_focus.values()
            .chain(self.all_obligations.values())
            .map(|c| c.obligation_count)
            .sum();

        Ok(())
    }

    /// Check if the database has been loaded
    pub fn is_loaded(&self) -> bool {
        self.total_classes > 0
    }

    // ─── Core Query API ─────────────────────────────────────────

    /// Get the obligations for a class by name.
    /// Checks kain_focus first (richer data), then falls back to all_obligations.
    pub fn get_obligations(&self, class_name: &str) -> Option<&ClassObligations> {
        self.kain_focus.get(class_name)
            .or_else(|| self.all_obligations.get(class_name))
    }

    /// Check if a class has any unresolved pure virtual obligations.
    pub fn has_obligations(&self, class_name: &str) -> bool {
        self.get_obligations(class_name)
            .map(|o| o.obligation_count > 0)
            .unwrap_or(false)
    }

    /// Get the number of obligations for a class.
    pub fn obligation_count(&self, class_name: &str) -> usize {
        self.get_obligations(class_name)
            .map(|o| o.obligation_count)
            .unwrap_or(0)
    }

    /// Get just the method names that must be overridden.
    pub fn required_method_names(&self, class_name: &str) -> Vec<&str> {
        self.get_obligations(class_name)
            .map(|o| o.obligations.iter().map(|m| m.name.as_str()).collect())
            .unwrap_or_default()
    }

    /// Check if a specific method is a required override for a class.
    pub fn is_required_override(&self, class_name: &str, method_name: &str) -> bool {
        self.get_obligations(class_name)
            .map(|o| o.obligations.iter().any(|m| m.name == method_name))
            .unwrap_or(false)
    }

    // ─── Code Generation Helpers ────────────────────────────────

    /// Generate the C++ header declaration for a required override.
    /// Returns e.g. "virtual FName GetToolkitFName() const override;"
    pub fn generate_override_declaration(&self, class_name: &str, method_name: &str) -> Option<String> {
        let obs = self.get_obligations(class_name)?;
        let method = obs.obligations.iter().find(|m| m.name == method_name)?;

        // If we have a pre-built declaration (kain_focus), use it
        if !method.override_declaration.is_empty() {
            return Some(format!("{};", method.override_declaration));
        }

        // Build from parts
        let params = method.params.iter()
            .map(|p| {
                if p.name.is_empty() {
                    p.param_type.clone()
                } else {
                    format!("{} {}", p.param_type, p.name)
                }
            })
            .collect::<Vec<_>>()
            .join(", ");
        let const_str = if method.is_const { " const" } else { "" };
        Some(format!("virtual {} {}({}){} override;",
            method.return_type, method.name, params, const_str))
    }

    /// Generate the C++ source definition for a required override.
    /// Returns e.g. "FName FMyClass::GetToolkitFName() const\n{ return FName(); }"
    pub fn generate_override_definition(
        &self,
        class_name: &str,
        concrete_class: &str,
        method_name: &str,
    ) -> Option<String> {
        let obs = self.get_obligations(class_name)?;
        let method = obs.obligations.iter().find(|m| m.name == method_name)?;

        // If we have a pre-built definition (kain_focus), use it with class substitution
        if !method.override_definition.is_empty() {
            return Some(method.override_definition.replace("{CLASS}", concrete_class));
        }

        // Build from parts
        let params = method.params.iter()
            .map(|p| {
                if p.name.is_empty() {
                    p.param_type.clone()
                } else {
                    format!("{} {}", p.param_type, p.name)
                }
            })
            .collect::<Vec<_>>()
            .join(", ");
        let const_str = if method.is_const { " const" } else { "" };
        let body = if method.default_body.is_empty() {
            Self::generate_default_body(&method.return_type)
        } else {
            method.default_body.clone()
        };

        Some(format!("{} {}::{}({}){}\n{}",
            method.return_type, concrete_class, method.name, params, const_str, body))
    }

    /// Generate all override declarations for a class.
    /// Returns a Vec of declaration strings ready for insertion into a header.
    pub fn generate_all_declarations(&self, class_name: &str) -> Vec<String> {
        self.get_obligations(class_name)
            .map(|obs| {
                obs.obligations.iter()
                    .filter_map(|m| self.generate_override_declaration(class_name, &m.name))
                    .collect()
            })
            .unwrap_or_default()
    }

    /// Generate all override definitions for a class.
    /// Returns a Vec of definition strings ready for insertion into a source file.
    pub fn generate_all_definitions(&self, class_name: &str, concrete_class: &str) -> Vec<String> {
        self.get_obligations(class_name)
            .map(|obs| {
                obs.obligations.iter()
                    .filter_map(|m| self.generate_override_definition(class_name, concrete_class, &m.name))
                    .collect()
            })
            .unwrap_or_default()
    }

    // ─── Internal Helpers ───────────────────────────────────────

    /// Generate a sensible default body for a return type.
    fn generate_default_body(return_type: &str) -> String {
        match return_type {
            "void" => "{ }".to_string(),
            "bool" => "{ return false; }".to_string(),
            "int" | "int32" | "int64" | "uint32" | "uint64" | "float" | "double" =>
                "{ return 0; }".to_string(),
            "FName" => "{ return FName(); }".to_string(),
            "FString" => "{ return FString(); }".to_string(),
            "FText" => "{ return FText::GetEmpty(); }".to_string(),
            "FLinearColor" => "{ return FLinearColor::White; }".to_string(),
            t if t.starts_with("TSharedRef") => "{ return SNullWidget::NullWidget; }".to_string(),
            t if t.starts_with("TSharedPtr") => "{ return nullptr; }".to_string(),
            t if t.ends_with('*') => "{ return nullptr; }".to_string(),
            t if t.ends_with('&') => {
                let base = t.trim_end_matches('&').trim();
                format!("{{ static {} Default; return Default; }}", base)
            }
            t => format!("{{ return {}(); }}", t),
        }
    }

    // ─── Diagnostics ────────────────────────────────────────────

    /// Get summary statistics
    pub fn stats(&self) -> (usize, usize, usize) {
        (self.kain_focus.len(), self.all_obligations.len(), self.total_obligations)
    }

    /// Get all KAIN-focus class names
    pub fn kain_focus_classes(&self) -> Vec<&str> {
        self.kain_focus.keys().map(|s| s.as_str()).collect()
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_obligations() -> VirtualObligations {
        let json = r#"{
            "_meta": {
                "generator": "test",
                "total_classes_scanned": 100,
                "total_pure_virtual_declarations": 50,
                "total_classes_with_obligations": 10,
                "compact_classes_included": 8,
                "kain_focus_classes": 2
            },
            "kain_focus": {
                "FAssetEditorToolkit": {
                    "class": "FAssetEditorToolkit",
                    "parents": ["IAssetEditorInstance", "FBaseToolkit"],
                    "header": "Toolkits/AssetEditorToolkit.h",
                    "module": "UnrealEd",
                    "category": "Editor",
                    "obligation_count": 3,
                    "obligations": [
                        {
                            "name": "GetToolkitFName",
                            "return_type": "FName",
                            "params": [],
                            "is_const": true,
                            "declared_in": "FAssetEditorToolkit",
                            "default_body": "{ return FName(); }",
                            "raw_signature": "virtual FName GetToolkitFName() const = 0;",
                            "override_declaration": "virtual FName GetToolkitFName() const override",
                            "override_definition": "FName {CLASS}::GetToolkitFName() const\n{ return FName(); }"
                        },
                        {
                            "name": "GetBaseToolkitName",
                            "return_type": "FText",
                            "params": [],
                            "is_const": true,
                            "declared_in": "FAssetEditorToolkit",
                            "default_body": "{ return FText::GetEmpty(); }",
                            "raw_signature": "virtual FText GetBaseToolkitName() const = 0;",
                            "override_declaration": "virtual FText GetBaseToolkitName() const override",
                            "override_definition": "FText {CLASS}::GetBaseToolkitName() const\n{ return FText::GetEmpty(); }"
                        },
                        {
                            "name": "GetWorldCentricTabPrefix",
                            "return_type": "FString",
                            "params": [],
                            "is_const": true,
                            "declared_in": "FAssetEditorToolkit",
                            "default_body": "{ return FString(); }",
                            "raw_signature": "virtual FString GetWorldCentricTabPrefix() const = 0;",
                            "override_declaration": "virtual FString GetWorldCentricTabPrefix() const override",
                            "override_definition": "FString {CLASS}::GetWorldCentricTabPrefix() const\n{ return FString(); }"
                        }
                    ]
                },
                "IDetailCustomization": {
                    "class": "IDetailCustomization",
                    "parents": [],
                    "header": "IDetailCustomization.h",
                    "module": "PropertyEditor",
                    "category": "Editor",
                    "obligation_count": 1,
                    "obligations": [
                        {
                            "name": "CustomizeDetails",
                            "return_type": "void",
                            "params": [{"name": "DetailBuilder", "type": "IDetailLayoutBuilder&"}],
                            "is_const": false,
                            "declared_in": "IDetailCustomization",
                            "default_body": "{ }",
                            "raw_signature": "virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) = 0;",
                            "override_declaration": "virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override",
                            "override_definition": "void {CLASS}::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)\n{ }"
                        }
                    ]
                }
            },
            "obligations": {
                "FGCObject": {
                    "parents": [],
                    "header": "GCObject.h",
                    "module": "CoreUObject",
                    "category": "Runtime",
                    "obligation_count": 2,
                    "obligations": [
                        {
                            "name": "AddReferencedObjects",
                            "return_type": "void",
                            "params": [{"name": "Collector", "type": "FReferenceCollector&"}],
                            "is_const": false,
                            "declared_in": "FGCObject",
                            "default_body": "{ }"
                        },
                        {
                            "name": "GetReferencerName",
                            "return_type": "FString",
                            "params": [],
                            "is_const": true,
                            "declared_in": "FGCObject",
                            "default_body": "{ return FString(); }"
                        }
                    ]
                }
            }
        }"#;

        let mut vo = VirtualObligations::new();
        vo.load(json).unwrap();
        vo
    }

    #[test]
    fn test_load_and_stats() {
        let vo = make_test_obligations();
        assert!(vo.is_loaded());
        let (focus, all, total) = vo.stats();
        assert_eq!(focus, 2);
        assert_eq!(all, 1);
        assert_eq!(total, 6); // 3 + 1 + 2
    }

    #[test]
    fn test_has_obligations() {
        let vo = make_test_obligations();
        assert!(vo.has_obligations("FAssetEditorToolkit"));
        assert!(vo.has_obligations("IDetailCustomization"));
        assert!(vo.has_obligations("FGCObject"));
        assert!(!vo.has_obligations("UObject")); // not in data
    }

    #[test]
    fn test_obligation_count() {
        let vo = make_test_obligations();
        assert_eq!(vo.obligation_count("FAssetEditorToolkit"), 3);
        assert_eq!(vo.obligation_count("IDetailCustomization"), 1);
        assert_eq!(vo.obligation_count("FGCObject"), 2);
        assert_eq!(vo.obligation_count("UnknownClass"), 0);
    }

    #[test]
    fn test_required_method_names() {
        let vo = make_test_obligations();
        let names = vo.required_method_names("FAssetEditorToolkit");
        assert!(names.contains(&"GetToolkitFName"));
        assert!(names.contains(&"GetBaseToolkitName"));
        assert!(names.contains(&"GetWorldCentricTabPrefix"));
    }

    #[test]
    fn test_is_required_override() {
        let vo = make_test_obligations();
        assert!(vo.is_required_override("FAssetEditorToolkit", "GetToolkitFName"));
        assert!(vo.is_required_override("IDetailCustomization", "CustomizeDetails"));
        assert!(!vo.is_required_override("FAssetEditorToolkit", "NonExistentMethod"));
    }

    #[test]
    fn test_generate_override_declaration() {
        let vo = make_test_obligations();
        let decl = vo.generate_override_declaration("FAssetEditorToolkit", "GetToolkitFName");
        assert_eq!(decl, Some("virtual FName GetToolkitFName() const override;".to_string()));

        let decl2 = vo.generate_override_declaration("IDetailCustomization", "CustomizeDetails");
        assert_eq!(decl2, Some("virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;".to_string()));
    }

    #[test]
    fn test_generate_override_definition() {
        let vo = make_test_obligations();
        let def = vo.generate_override_definition("FAssetEditorToolkit", "FMyEditor", "GetToolkitFName");
        assert!(def.is_some());
        let def = def.unwrap();
        assert!(def.contains("FMyEditor::GetToolkitFName"));
        assert!(def.contains("return FName()"));
    }

    #[test]
    fn test_generate_all_declarations() {
        let vo = make_test_obligations();
        let decls = vo.generate_all_declarations("FAssetEditorToolkit");
        assert_eq!(decls.len(), 3);
        assert!(decls[0].contains("GetToolkitFName"));
    }

    #[test]
    fn test_generate_all_definitions() {
        let vo = make_test_obligations();
        let defs = vo.generate_all_definitions("FAssetEditorToolkit", "FMyEditor");
        assert_eq!(defs.len(), 3);
        assert!(defs[0].contains("FMyEditor::"));
    }

    #[test]
    fn test_compact_obligations_fallback() {
        let vo = make_test_obligations();
        // FGCObject is in compact (all_obligations), not kain_focus
        let decl = vo.generate_override_declaration("FGCObject", "AddReferencedObjects");
        assert!(decl.is_some());
        assert!(decl.unwrap().contains("AddReferencedObjects"));

        let def = vo.generate_override_definition("FGCObject", "FMyObject", "GetReferencerName");
        assert!(def.is_some());
        assert!(def.unwrap().contains("FMyObject::GetReferencerName"));
    }

    #[test]
    fn test_default_body_generation() {
        assert_eq!(VirtualObligations::generate_default_body("void"), "{ }");
        assert_eq!(VirtualObligations::generate_default_body("bool"), "{ return false; }");
        assert_eq!(VirtualObligations::generate_default_body("FName"), "{ return FName(); }");
        assert_eq!(VirtualObligations::generate_default_body("FString"), "{ return FString(); }");
        assert!(VirtualObligations::generate_default_body("FLinearColor").contains("White"));
        assert!(VirtualObligations::generate_default_body("UObject*").contains("nullptr"));
        assert!(VirtualObligations::generate_default_body("TSharedPtr<SWidget>").contains("nullptr"));
    }
}
