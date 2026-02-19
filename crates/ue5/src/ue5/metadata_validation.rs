// Metadata validation module for KAIN UE5 pipeline
// Validates all metadata JSON files against schemas before use
// Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8, 13.9

use jsonschema::{Draft, JSONSchema};
use serde_json::Value;
use std::collections::HashMap;
use std::path::{Path, PathBuf};

/// Result type for metadata validation operations
pub type ValidationResult<T> = Result<T, ValidationError>;

/// Structured error for metadata validation failures
#[derive(Debug, Clone)]
pub struct ValidationError {
    pub file_path: PathBuf,
    pub json_path: Option<String>,
    pub message: String,
    pub suggestion: Option<String>,
}

/// Warning for incomplete metadata
#[derive(Debug, Clone)]
pub struct ValidationWarning {
    pub file_path: PathBuf,
    pub field_path: String,
    pub message: String,
}

/// Completeness check result
#[derive(Debug)]
pub struct CompletenessReport {
    pub errors: Vec<ValidationError>,
    pub warnings: Vec<ValidationWarning>,
    pub missing_files: Vec<PathBuf>,
}

impl CompletenessReport {
    pub fn new() -> Self {
        Self {
            errors: Vec::new(),
            warnings: Vec::new(),
            missing_files: Vec::new(),
        }
    }
    
    pub fn has_errors(&self) -> bool {
        !self.errors.is_empty() || !self.missing_files.is_empty()
    }
    
    pub fn add_error(&mut self, error: ValidationError) {
        self.errors.push(error);
    }
    
    pub fn add_warning(&mut self, warning: ValidationWarning) {
        self.warnings.push(warning);
    }
    
    pub fn add_missing_file(&mut self, path: PathBuf) {
        self.missing_files.push(path);
    }
}

impl std::fmt::Display for ValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Metadata validation error in {}: {}", 
            self.file_path.display(), self.message)?;
        if let Some(json_path) = &self.json_path {
            write!(f, " at {}", json_path)?;
        }
        if let Some(suggestion) = &self.suggestion {
            write!(f, "\n  Suggestion: {}", suggestion)?;
        }
        Ok(())
    }
}

impl std::error::Error for ValidationError {}

/// Metadata schema validator
pub struct MetadataValidator {
    schemas: HashMap<String, JSONSchema>,
}

impl MetadataValidator {
    /// Create a new validator with all metadata schemas loaded
    pub fn new() -> Self {
        let mut validator = Self {
            schemas: HashMap::new(),
        };
        
        // Load all schema definitions
        validator.load_engine_knowledge_schema();
        validator.load_module_graph_schema();
        validator.load_uht_rules_schema();
        validator.load_shader_knowledge_schema();
        validator.load_widget_registry_schema();
        validator.load_editor_attributes_schema();
        validator.load_virtual_obligations_schema();
        validator.load_codegen_rules_schema();
        validator.load_version_config_schema();
        
        validator
    }
    
    /// Check completeness of all metadata files in a directory
    /// Requirements: 13.10, 13.20
    pub fn check_completeness(&self, metadata_dir: &Path) -> CompletenessReport {
        let mut report = CompletenessReport::new();
        
        // Required metadata files
        let required_files = vec![
            "engine_knowledge.json",
            "module_graph.json",
            "uht_rules.json",
            "shader_knowledge.json",
            "widget_registry.json",
        ];
        
        // Optional metadata files (warn if missing)
        let optional_files = vec![
            "editor_attributes.json",
            "virtual_obligations.json",
            "codegen_rules.json",
            "engine_knowledge_expanded.json",
        ];
        
        // Check required files exist
        for filename in required_files {
            let file_path = metadata_dir.join(filename);
            if !file_path.exists() {
                report.add_error(ValidationError {
                    file_path: file_path.clone(),
                    json_path: None,
                    message: format!("Required metadata file missing: {}", filename),
                    suggestion: Some(format!("Run metadata extraction scripts to generate {}", filename)),
                });
                report.add_missing_file(file_path);
            } else {
                // Check file is not empty
                if let Ok(metadata) = std::fs::metadata(&file_path) {
                    if metadata.len() == 0 {
                        report.add_error(ValidationError {
                            file_path: file_path.clone(),
                            json_path: None,
                            message: format!("Required metadata file is empty: {}", filename),
                            suggestion: Some("Re-run metadata extraction scripts".to_string()),
                        });
                    }
                }
            }
        }
        
        // Check optional files (warnings only)
        for filename in optional_files {
            let file_path = metadata_dir.join(filename);
            if !file_path.exists() {
                report.add_warning(ValidationWarning {
                    file_path: file_path.clone(),
                    field_path: String::new(),
                    message: format!("Optional metadata file missing: {}", filename),
                });
            }
        }
        
        // Check completeness of engine_knowledge.json
        let engine_knowledge_path = metadata_dir.join("engine_knowledge.json");
        if engine_knowledge_path.exists() {
            if let Ok(content) = std::fs::read_to_string(&engine_knowledge_path) {
                if let Ok(json) = serde_json::from_str::<Value>(&content) {
                    self.check_engine_knowledge_completeness(&engine_knowledge_path, &json, &mut report);
                }
            }
        }
        
        // Check completeness of module_graph.json
        let module_graph_path = metadata_dir.join("module_graph.json");
        if module_graph_path.exists() {
            if let Ok(content) = std::fs::read_to_string(&module_graph_path) {
                if let Ok(json) = serde_json::from_str::<Value>(&content) {
                    self.check_module_graph_completeness(&module_graph_path, &json, &mut report);
                }
            }
        }
        
        report
    }
    
    /// Check completeness of engine_knowledge.json
    fn check_engine_knowledge_completeness(&self, file_path: &Path, json: &Value, report: &mut CompletenessReport) {
        // Check for required top-level fields
        if json.get("classes").is_none() {
            report.add_warning(ValidationWarning {
                file_path: file_path.to_path_buf(),
                field_path: "classes".to_string(),
                message: "Missing 'classes' array in engine_knowledge.json".to_string(),
            });
        }
        
        if json.get("structs").is_none() {
            report.add_warning(ValidationWarning {
                file_path: file_path.to_path_buf(),
                field_path: "structs".to_string(),
                message: "Missing 'structs' array in engine_knowledge.json".to_string(),
            });
        }
        
        if json.get("enums").is_none() {
            report.add_warning(ValidationWarning {
                file_path: file_path.to_path_buf(),
                field_path: "enums".to_string(),
                message: "Missing 'enums' array in engine_knowledge.json".to_string(),
            });
        }
        
        // Check for common UE5 types
        let common_types = vec!["AActor", "UObject", "UActorComponent", "FVector", "FRotator", "FTransform"];
        if let Some(classes) = json.get("classes").and_then(|c| c.as_array()) {
            for expected_type in common_types {
                let found = classes.iter().any(|c| {
                    c.get("name").and_then(|n| n.as_str()) == Some(expected_type)
                });
                
                if !found {
                    report.add_warning(ValidationWarning {
                        file_path: file_path.to_path_buf(),
                        field_path: format!("classes[{}]", expected_type),
                        message: format!("Common UE5 type '{}' not found in engine_knowledge.json", expected_type),
                    });
                }
            }
        }
    }
    
    /// Check completeness of module_graph.json
    fn check_module_graph_completeness(&self, file_path: &Path, json: &Value, report: &mut CompletenessReport) {
        // Check for required fields
        if json.get("modules").is_none() {
            report.add_warning(ValidationWarning {
                file_path: file_path.to_path_buf(),
                field_path: "modules".to_string(),
                message: "Missing 'modules' array in module_graph.json".to_string(),
            });
        }
        
        if json.get("include_to_module").is_none() {
            report.add_warning(ValidationWarning {
                file_path: file_path.to_path_buf(),
                field_path: "include_to_module".to_string(),
                message: "Missing 'include_to_module' map in module_graph.json".to_string(),
            });
        }
        
        // Check for common UE5 modules
        let common_modules = vec!["Core", "CoreUObject", "Engine", "Slate", "SlateCore"];
        if let Some(modules) = json.get("modules").and_then(|m| m.as_array()) {
            for expected_module in common_modules {
                let found = modules.iter().any(|m| {
                    m.get("name").and_then(|n| n.as_str()) == Some(expected_module)
                });
                
                if !found {
                    report.add_warning(ValidationWarning {
                        file_path: file_path.to_path_buf(),
                        field_path: format!("modules[{}]", expected_module),
                        message: format!("Common UE5 module '{}' not found in module_graph.json", expected_module),
                    });
                }
            }
        }
    }
    
    /// Validate a metadata file against its schema
    pub fn validate_file(&self, file_path: &Path, content: &str) -> ValidationResult<Value> {
        // Parse JSON
        let instance: Value = serde_json::from_str(content)
            .map_err(|e| ValidationError {
                file_path: file_path.to_path_buf(),
                json_path: None,
                message: format!("Failed to parse JSON: {}", e),
                suggestion: Some("Check JSON syntax for missing commas, brackets, or quotes".to_string()),
            })?;
        
        // Determine schema based on filename
        let filename = file_path.file_name()
            .and_then(|n| n.to_str())
            .ok_or_else(|| ValidationError {
                file_path: file_path.to_path_buf(),
                json_path: None,
                message: "Invalid file path".to_string(),
                suggestion: None,
            })?;
        
        let schema_name = self.get_schema_name(filename);
        
        // Get schema
        let schema = self.schemas.get(&schema_name)
            .ok_or_else(|| ValidationError {
                file_path: file_path.to_path_buf(),
                json_path: None,
                message: format!("No schema found for file type: {}", filename),
                suggestion: Some("This metadata file type is not yet supported for validation".to_string()),
            })?;
        
        // Validate against schema
        if let Err(errors) = schema.validate(&instance) {
            let error_messages: Vec<String> = errors
                .map(|e| format!("{} at {}", e, e.instance_path))
                .collect();
            
            return Err(ValidationError {
                file_path: file_path.to_path_buf(),
                json_path: Some(error_messages.join("; ")),
                message: "Schema validation failed".to_string(),
                suggestion: Some("Check the metadata file structure against the expected schema".to_string()),
            });
        }
        
        Ok(instance)
    }
    
    /// Determine schema name from filename
    fn get_schema_name(&self, filename: &str) -> String {
        if filename.starts_with("engine_") && filename.ends_with("_scanned.json") {
            // Scanned files use the same schema as engine_knowledge
            "engine_knowledge".to_string()
        } else if filename == "engine_knowledge.json" || filename == "engine_knowledge_expanded.json" {
            "engine_knowledge".to_string()
        } else if filename == "module_graph.json" {
            "module_graph".to_string()
        } else if filename == "uht_rules.json" {
            "uht_rules".to_string()
        } else if filename == "shader_knowledge.json" {
            "shader_knowledge".to_string()
        } else if filename == "widget_registry.json" {
            "widget_registry".to_string()
        } else if filename == "editor_attributes.json" {
            "editor_attributes".to_string()
        } else if filename == "virtual_obligations.json" {
            "virtual_obligations".to_string()
        } else if filename == "codegen_rules.json" {
            "codegen_rules".to_string()
        } else if filename.ends_with(".json") && filename.chars().next().map_or(false, |c| c.is_numeric()) {
            "version_config".to_string()
        } else {
            "unknown".to_string()
        }
    }
    
    /// Load engine_knowledge.json schema
    fn load_engine_knowledge_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "engine_version": { "type": "string" },
                "classes": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name", "parent", "header", "module", "prefix"],
                        "properties": {
                            "name": { "type": "string" },
                            "parent": { "type": "string" },
                            "header": { "type": "string" },
                            "module": { "type": "string" },
                            "prefix": { "type": "string", "enum": ["U", "A", "F", "E", "S"] },
                            "is_abstract": { "type": "boolean" },
                            "functions": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "required": ["name", "return_type", "params"],
                                    "properties": {
                                        "name": { "type": "string" },
                                        "return_type": { "type": "string" },
                                        "params": {
                                            "type": "array",
                                            "items": {
                                                "type": "object",
                                                "required": ["name", "type"],
                                                "properties": {
                                                    "name": { "type": "string" },
                                                    "type": { "type": "string" }
                                                }
                                            }
                                        },
                                        "is_const": { "type": "boolean" },
                                        "is_static": { "type": "boolean" },
                                        "specifiers": {
                                            "type": "array",
                                            "items": { "type": "string" }
                                        }
                                    }
                                }
                            },
                            "properties": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "required": ["name", "type"],
                                    "properties": {
                                        "name": { "type": "string" },
                                        "type": { "type": "string" },
                                        "specifiers": {
                                            "type": "array",
                                            "items": { "type": "string" }
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                "structs": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name", "header", "module"],
                        "properties": {
                            "name": { "type": "string" },
                            "header": { "type": "string" },
                            "module": { "type": "string" },
                            "fields": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "required": ["name", "type"],
                                    "properties": {
                                        "name": { "type": "string" },
                                        "type": { "type": "string" }
                                    }
                                }
                            }
                        }
                    }
                },
                "enums": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name", "header", "module"],
                        "properties": {
                            "name": { "type": "string" },
                            "header": { "type": "string" },
                            "module": { "type": "string" },
                            "values": {
                                "type": "array",
                                "items": { "type": "string" }
                            }
                        }
                    }
                },
                "type_aliases": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["kain_name", "ue5_name"],
                        "properties": {
                            "kain_name": { "type": "string" },
                            "ue5_name": { "type": "string" },
                            "header": { "type": "string" }
                        }
                    }
                },
                "include_map": {
                    "type": "object",
                    "additionalProperties": { "type": "string" }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("engine_knowledge".to_string(), schema);
        }
    }
    
    /// Load module_graph.json schema
    fn load_module_graph_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "modules": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name"],
                        "properties": {
                            "name": { "type": "string" },
                            "dependencies": {
                                "type": "array",
                                "items": { "type": "string" }
                            },
                            "includes": {
                                "type": "array",
                                "items": { "type": "string" }
                            }
                        }
                    }
                },
                "include_to_module": {
                    "type": "object",
                    "additionalProperties": { "type": "string" }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("module_graph".to_string(), schema);
        }
    }
    
    /// Load uht_rules.json schema
    fn load_uht_rules_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "rules": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["id", "category", "severity"],
                        "properties": {
                            "id": { "type": "string" },
                            "category": { "type": "string" },
                            "severity": { "type": "string", "enum": ["error", "warning", "info"] },
                            "message": { "type": "string" },
                            "suggestion": { "type": "string" }
                        }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("uht_rules".to_string(), schema);
        }
    }
    
    /// Load shader_knowledge.json schema
    fn load_shader_knowledge_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "hlsl_types": {
                    "type": "array",
                    "items": { "type": "string" }
                },
                "hlsl_keywords": {
                    "type": "array",
                    "items": { "type": "string" }
                },
                "binding_rules": {
                    "type": "object",
                    "properties": {
                        "max_texture_slots": { "type": "integer" },
                        "max_uniform_slots": { "type": "integer" }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("shader_knowledge".to_string(), schema);
        }
    }
    
    /// Load widget_registry.json schema
    fn load_widget_registry_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "widgets": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name", "header"],
                        "properties": {
                            "name": { "type": "string" },
                            "header": { "type": "string" },
                            "properties": {
                                "type": "array",
                                "items": {
                                    "type": "object",
                                    "required": ["name", "type"],
                                    "properties": {
                                        "name": { "type": "string" },
                                        "type": { "type": "string" }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("widget_registry".to_string(), schema);
        }
    }
    
    /// Load editor_attributes.json schema
    fn load_editor_attributes_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "attributes": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["name"],
                        "properties": {
                            "name": { "type": "string" },
                            "parameters": {
                                "type": "array",
                                "items": { "type": "string" }
                            },
                            "description": { "type": "string" }
                        }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("editor_attributes".to_string(), schema);
        }
    }
    
    /// Load virtual_obligations.json schema
    fn load_virtual_obligations_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "obligations": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["class_name", "virtual_functions"],
                        "properties": {
                            "class_name": { "type": "string" },
                            "virtual_functions": {
                                "type": "array",
                                "items": { "type": "string" }
                            }
                        }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("virtual_obligations".to_string(), schema);
        }
    }
    
    /// Load codegen_rules.json schema
    fn load_codegen_rules_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "rules": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "required": ["pattern", "replacement"],
                        "properties": {
                            "pattern": { "type": "string" },
                            "replacement": { "type": "string" },
                            "description": { "type": "string" }
                        }
                    }
                }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("codegen_rules".to_string(), schema);
        }
    }
    
    /// Load version config schema (5.4.json, 5.5.json, etc.)
    fn load_version_config_schema(&mut self) {
        let schema_json = serde_json::json!({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "version": { "type": "string" },
                "install_path": { "type": "string" },
                "engine_path": { "type": "string" }
            }
        });
        
        if let Ok(schema) = JSONSchema::options()
            .with_draft(Draft::Draft7)
            .compile(&schema_json)
        {
            self.schemas.insert("version_config".to_string(), schema);
        }
    }
    
}

impl Default for MetadataValidator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    
    #[test]
    fn test_validator_creation() {
        let validator = MetadataValidator::new();
        assert!(validator.schemas.len() > 0, "Validator should have loaded schemas");
    }
    
    #[test]
    fn test_schema_name_detection() {
        let validator = MetadataValidator::new();
        
        assert_eq!(validator.get_schema_name("engine_knowledge.json"), "engine_knowledge");
        assert_eq!(validator.get_schema_name("engine_5.4_scanned.json"), "engine_knowledge");
        assert_eq!(validator.get_schema_name("module_graph.json"), "module_graph");
        assert_eq!(validator.get_schema_name("5.4.json"), "version_config");
    }
    
    #[test]
    fn test_valid_engine_knowledge() {
        let validator = MetadataValidator::new();
        let valid_json = r#"{
            "engine_version": "5.4",
            "classes": [],
            "structs": [],
            "enums": [],
            "type_aliases": [],
            "include_map": {}
        }"#;
        
        let result = validator.validate_file(
            Path::new("engine_knowledge.json"),
            valid_json
        );
        
        assert!(result.is_ok(), "Valid engine_knowledge.json should pass validation");
    }
    
    #[test]
    fn test_invalid_json_syntax() {
        let validator = MetadataValidator::new();
        let invalid_json = r#"{ "engine_version": "5.4", "#; // Missing closing brace
        
        let result = validator.validate_file(
            Path::new("engine_knowledge.json"),
            invalid_json
        );
        
        assert!(result.is_err(), "Invalid JSON syntax should fail");
        if let Err(e) = result {
            assert!(e.message.contains("Failed to parse JSON"));
        }
    }
    
    #[test]
    fn test_completeness_check_missing_required_files() {
        let validator = MetadataValidator::new();
        
        // Create a temporary directory
        let temp_dir = std::env::temp_dir().join("kain_metadata_test");
        let _ = fs::create_dir_all(&temp_dir);
        
        // Check completeness (should report missing files)
        let report = validator.check_completeness(&temp_dir);
        
        assert!(report.has_errors(), "Should report errors for missing required files");
        assert!(report.missing_files.len() > 0, "Should list missing files");
        
        // Cleanup
        let _ = fs::remove_dir_all(&temp_dir);
    }
    
    #[test]
    fn test_completeness_check_empty_file() {
        let validator = MetadataValidator::new();
        
        // Create a temporary directory with an empty file
        let temp_dir = std::env::temp_dir().join("kain_metadata_test_empty");
        let _ = fs::create_dir_all(&temp_dir);
        
        let empty_file = temp_dir.join("engine_knowledge.json");
        let _ = fs::File::create(&empty_file);
        
        // Check completeness (should report empty file error)
        let report = validator.check_completeness(&temp_dir);
        
        assert!(report.has_errors(), "Should report error for empty file");
        
        // Cleanup
        let _ = fs::remove_dir_all(&temp_dir);
    }
    
    #[test]
    fn test_completeness_check_incomplete_engine_knowledge() {
        let validator = MetadataValidator::new();
        
        // Create a temporary directory with incomplete engine_knowledge.json
        let temp_dir = std::env::temp_dir().join("kain_metadata_test_incomplete");
        let _ = fs::create_dir_all(&temp_dir);
        
        let incomplete_json = r#"{
            "engine_version": "5.4",
            "classes": [],
            "structs": []
        }"#;
        
        let file_path = temp_dir.join("engine_knowledge.json");
        let mut file = fs::File::create(&file_path).unwrap();
        file.write_all(incomplete_json.as_bytes()).unwrap();
        
        // Also create other required files to avoid missing file errors
        for filename in &["module_graph.json", "uht_rules.json", "shader_knowledge.json", "widget_registry.json"] {
            let path = temp_dir.join(filename);
            let mut f = fs::File::create(&path).unwrap();
            f.write_all(b"{}").unwrap();
        }
        
        // Check completeness (should report warnings for missing fields)
        let report = validator.check_completeness(&temp_dir);
        
        assert!(report.warnings.len() > 0, "Should report warnings for incomplete metadata");
        
        // Cleanup
        let _ = fs::remove_dir_all(&temp_dir);
    }
}
