//! Intermediate Representation for UE5 Configuration Systems
//!
//! This module defines the IR types that represent KAIN @config structs
//! and their @setting fields after parsing but before code generation.

use kain_core::ast::{Expr, Field, Struct, Type};
use kain_core::span::Span;

/// Configuration category determines which .ini file and UCLASS Config specifier to use
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConfigCategory {
    /// Config=Game, DefaultGame.ini
    Game,
    /// Config=Engine, DefaultEngine.ini
    Engine,
    /// Config=Editor, DefaultEditor.ini
    Editor,
    /// Config=EditorPerProjectUserSettings, DefaultEditorPerProjectUserSettings.ini
    EditorPerProjectUserSettings,
}

impl ConfigCategory {
    /// Get the UCLASS Config specifier (e.g., "Game", "Engine")
    pub fn uclass_specifier(&self) -> &'static str {
        match self {
            ConfigCategory::Game => "Game",
            ConfigCategory::Engine => "Engine",
            ConfigCategory::Editor => "Editor",
            ConfigCategory::EditorPerProjectUserSettings => "EditorPerProjectUserSettings",
        }
    }

    /// Get the default .ini file name
    pub fn default_ini_file(&self) -> &'static str {
        match self {
            ConfigCategory::Game => "DefaultGame.ini",
            ConfigCategory::Engine => "DefaultEngine.ini",
            ConfigCategory::Editor => "DefaultEditor.ini",
            ConfigCategory::EditorPerProjectUserSettings => {
                "DefaultEditorPerProjectUserSettings.ini"
            }
        }
    }

    /// Parse from string (case-insensitive)
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "game" => Some(ConfigCategory::Game),
            "engine" => Some(ConfigCategory::Engine),
            "editor" => Some(ConfigCategory::Editor),
            "editorperprojectusersettings" => Some(ConfigCategory::EditorPerProjectUserSettings),
            _ => None,
        }
    }
}

/// Represents a @config struct that will generate a UDeveloperSettings subclass
#[derive(Debug, Clone, PartialEq)]
pub struct ConfigStruct {
    /// Original struct name (e.g., "VoxelSettings")
    pub name: String,

    /// Configuration category (Game, Engine, Editor, EditorPerProjectUserSettings)
    pub category: ConfigCategory,

    /// Optional custom .ini file name (overrides category default)
    pub ini_file: Option<String>,

    /// Optional custom .ini section name (defaults to plugin name)
    pub ini_section: Option<String>,

    /// Optional display name for Project Settings UI
    pub display_name: Option<String>,

    /// Fields with @setting attributes
    pub fields: Vec<ConfigField>,

    /// Original struct for reference
    pub original_struct: Struct,

    /// Span for error reporting
    pub span: Span,
}

impl ConfigStruct {
    /// Get the UE5 class name (adds U prefix if not present)
    pub fn ue5_class_name(&self) -> String {
        if self.name.starts_with('U') {
            self.name.clone()
        } else {
            format!("U{}", self.name)
        }
    }

    /// Get the display name for Project Settings (defaults to struct name with spaces)
    pub fn get_display_name(&self) -> String {
        self.display_name.clone().unwrap_or_else(|| {
            // Convert PascalCase to "Pascal Case"
            let mut result = String::new();
            for (i, ch) in self.name.chars().enumerate() {
                if i > 0 && ch.is_uppercase() {
                    result.push(' ');
                }
                result.push(ch);
            }
            result
        })
    }

    /// Get the .ini file name (uses category default if not specified)
    pub fn get_ini_file(&self) -> String {
        self.ini_file
            .clone()
            .unwrap_or_else(|| self.category.default_ini_file().to_string())
    }

    /// Get the .ini section name (defaults to plugin name)
    pub fn get_ini_section(&self, plugin_name: &str) -> String {
        self.ini_section
            .clone()
            .unwrap_or_else(|| format!("/Script/{}.{}", plugin_name, self.ue5_class_name()))
    }
}

/// Represents a field with @setting attribute
#[derive(Debug, Clone, PartialEq)]
pub struct ConfigField {
    /// Field name (e.g., "chunk_size")
    pub name: String,

    /// Field type
    pub ty: Type,

    /// Default value expression
    pub default: Option<Expr>,

    /// Display name for UI (optional)
    pub display_name: Option<String>,

    /// Tooltip text (optional)
    pub tooltip: Option<String>,

    /// Console variable name (e.g., "voxel.ChunkSize")
    pub cvar: Option<String>,

    /// Generate Blueprint accessor functions
    pub blueprint: bool,

    /// Minimum value (for numeric types)
    pub min: Option<f64>,

    /// Maximum value (for numeric types)
    pub max: Option<f64>,

    /// Generate setter functions (default: false, read-only)
    pub writable: bool,

    /// Original field for reference
    pub original_field: Field,

    /// Span for error reporting
    pub span: Span,
}

impl ConfigField {
    /// Get the UE5 property name (converts snake_case to PascalCase)
    pub fn ue5_property_name(&self) -> String {
        use heck::ToPascalCase;
        self.name.to_pascal_case()
    }

    /// Get the display name for UI (defaults to field name with spaces)
    pub fn get_display_name(&self) -> String {
        self.display_name.clone().unwrap_or_else(|| {
            // Convert snake_case to "Snake Case"
            self.name
                .replace('_', " ")
                .split_whitespace()
                .map(|word| {
                    let mut chars = word.chars();
                    match chars.next() {
                        None => String::new(),
                        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
                    }
                })
                .collect::<Vec<_>>()
                .join(" ")
        })
    }

    /// Get the console variable name (defaults to plugin.FieldName)
    pub fn get_cvar_name(&self, plugin_name: &str) -> Option<String> {
        match &self.cvar {
            Some(cvar) if !cvar.is_empty() => Some(cvar.clone()),
            Some(_) => Some(format!("{}.{}", plugin_name, self.ue5_property_name())),
            None => None,
        }
    }

    /// Check if this field should generate a console variable
    pub fn has_cvar(&self) -> bool {
        self.cvar.is_some()
    }
}

/// Console variable metadata
#[derive(Debug, Clone, PartialEq)]
pub struct CVar {
    /// Console variable name (e.g., "voxel.ChunkSize")
    pub name: String,

    /// Variable type (Float, Int, Bool, String)
    pub ty: Type,

    /// Default value
    pub default: Option<Expr>,

    /// Help text
    pub help_text: String,

    /// Flags (e.g., ECVF_Default)
    pub flags: String,

    /// Associated config field
    pub field: ConfigField,
}

impl CVar {
    /// Get the C++ type for TAutoConsoleVariable<T>
    pub fn cpp_type(&self) -> &'static str {
        // This will be implemented in the codegen phase
        // For now, return a placeholder
        match self.ty {
            Type::Named { ref name, .. } => match name.as_str() {
                "Float" => "float",
                "Int" => "int32",
                "Bool" => "bool",
                "String" => "FString",
                _ => "float", // Default fallback
            },
            _ => "float",
        }
    }

    /// Get the callback method name (e.g., "OnChunkSizeChanged")
    pub fn callback_method_name(&self) -> String {
        use heck::ToPascalCase;
        format!("On{}Changed", self.field.name.to_pascal_case())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_config_category_uclass_specifier() {
        assert_eq!(ConfigCategory::Game.uclass_specifier(), "Game");
        assert_eq!(ConfigCategory::Engine.uclass_specifier(), "Engine");
        assert_eq!(ConfigCategory::Editor.uclass_specifier(), "Editor");
        assert_eq!(
            ConfigCategory::EditorPerProjectUserSettings.uclass_specifier(),
            "EditorPerProjectUserSettings"
        );
    }

    #[test]
    fn test_config_category_default_ini_file() {
        assert_eq!(ConfigCategory::Game.default_ini_file(), "DefaultGame.ini");
        assert_eq!(
            ConfigCategory::Engine.default_ini_file(),
            "DefaultEngine.ini"
        );
        assert_eq!(
            ConfigCategory::Editor.default_ini_file(),
            "DefaultEditor.ini"
        );
        assert_eq!(
            ConfigCategory::EditorPerProjectUserSettings.default_ini_file(),
            "DefaultEditorPerProjectUserSettings.ini"
        );
    }

    #[test]
    fn test_config_category_from_str() {
        assert_eq!(ConfigCategory::from_str("game"), Some(ConfigCategory::Game));
        assert_eq!(ConfigCategory::from_str("Game"), Some(ConfigCategory::Game));
        assert_eq!(ConfigCategory::from_str("GAME"), Some(ConfigCategory::Game));
        assert_eq!(
            ConfigCategory::from_str("engine"),
            Some(ConfigCategory::Engine)
        );
        assert_eq!(
            ConfigCategory::from_str("editor"),
            Some(ConfigCategory::Editor)
        );
        assert_eq!(
            ConfigCategory::from_str("editorperprojectusersettings"),
            Some(ConfigCategory::EditorPerProjectUserSettings)
        );
        assert_eq!(ConfigCategory::from_str("invalid"), None);
    }

    #[test]
    fn test_config_struct_ue5_class_name() {
        let config = ConfigStruct {
            name: "VoxelSettings".to_string(),
            category: ConfigCategory::Game,
            ini_file: None,
            ini_section: None,
            display_name: None,
            fields: vec![],
            original_struct: Struct {
                name: "VoxelSettings".to_string(),
                generics: vec![],
                fields: vec![],
                methods: vec![],
                attributes: vec![],
                visibility: kain_core::ast::Visibility::Public,
                span: Span::default(),
            },
            span: Span::default(),
        };

        assert_eq!(config.ue5_class_name(), "UVoxelSettings");

        let config_with_u = ConfigStruct {
            name: "UVoxelSettings".to_string(),
            ..config
        };
        assert_eq!(config_with_u.ue5_class_name(), "UVoxelSettings");
    }

    #[test]
    fn test_config_field_ue5_property_name() {
        use kain_core::ast::{Field, Visibility};

        let field = ConfigField {
            name: "chunk_size".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            default: None,
            display_name: None,
            tooltip: None,
            cvar: None,
            blueprint: false,
            min: None,
            max: None,
            writable: false,
            original_field: Field {
                name: "chunk_size".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: Span::default(),
            },
            span: Span::default(),
        };

        assert_eq!(field.ue5_property_name(), "ChunkSize");
    }

    #[test]
    fn test_config_field_get_display_name() {
        use kain_core::ast::{Field, Visibility};

        let field = ConfigField {
            name: "chunk_size".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            default: None,
            display_name: None,
            tooltip: None,
            cvar: None,
            blueprint: false,
            min: None,
            max: None,
            writable: false,
            original_field: Field {
                name: "chunk_size".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: Span::default(),
            },
            span: Span::default(),
        };

        assert_eq!(field.get_display_name(), "Chunk Size");

        let field_with_display = ConfigField {
            display_name: Some("Custom Display Name".to_string()),
            ..field
        };
        assert_eq!(field_with_display.get_display_name(), "Custom Display Name");
    }
}
