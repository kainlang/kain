//! UE5 Project File Generation
//! 
//! Handles .uplugin, .uproject, and .Build.cs generation with automatic dependency management.

use std::collections::HashSet;

/// Plugin descriptor (.uplugin file)
#[derive(Debug, Clone)]
pub struct PluginDescriptor {
    pub name: String,
    pub version: String,
    pub description: String,
    pub category: String,
    pub created_by: String,
    pub loading_phase: LoadingPhase,
    pub modules: Vec<ModuleDescriptor>,
}

#[derive(Debug, Clone)]
pub enum LoadingPhase {
    Default,
    PostConfigInit,
    PreDefault,
    PostDefault,
    PostEngineInit,
}

impl LoadingPhase {
    pub fn as_str(&self) -> &str {
        match self {
            LoadingPhase::Default => "Default",
            LoadingPhase::PostConfigInit => "PostConfigInit",
            LoadingPhase::PreDefault => "PreDefault",
            LoadingPhase::PostDefault => "PostDefault",
            LoadingPhase::PostEngineInit => "PostEngineInit",
        }
    }
}

#[derive(Debug, Clone)]
pub struct ModuleDescriptor {
    pub name: String,
    pub module_type: ModuleType,
    pub loading_phase: LoadingPhase,
}

#[derive(Debug, Clone)]
pub enum ModuleType {
    Runtime,
    RuntimeNoCommandlet,
    Developer,
    Editor,
    EditorNoCommandlet,
    Program,
}

impl ModuleType {
    pub fn as_str(&self) -> &str {
        match self {
            ModuleType::Runtime => "Runtime",
            ModuleType::RuntimeNoCommandlet => "RuntimeNoCommandlet",
            ModuleType::Developer => "Developer",
            ModuleType::Editor => "Editor",
            ModuleType::EditorNoCommandlet => "EditorNoCommandlet",
            ModuleType::Program => "Program",
        }
    }
}

impl PluginDescriptor {
    pub fn to_json(&self) -> String {
        format!(
            r#"{{
	"FileVersion": 3,
	"Version": 1,
	"VersionName": "{}",
	"FriendlyName": "{}",
	"Description": "{}",
	"Category": "{}",
	"CreatedBy": "{}",
	"CreatedByURL": "",
	"DocsURL": "",
	"MarketplaceURL": "",
	"SupportURL": "",
	"CanContainContent": true,
	"IsBetaVersion": false,
	"IsExperimentalVersion": false,
	"Installed": false,
	"Modules": [
		{{
			"Name": "{}",
			"Type": "{}",
			"LoadingPhase": "{}"
		}}
	]
}}"#,
            self.version,
            self.name,
            self.description,
            self.category,
            self.created_by,
            self.modules[0].name,
            self.modules[0].module_type.as_str(),
            self.modules[0].loading_phase.as_str()
        )
    }
}

/// Build.cs file generator with automatic dependency detection
#[derive(Debug, Clone)]
pub struct BuildFile {
    pub module_name: String,
    pub public_dependencies: HashSet<String>,
    pub private_dependencies: HashSet<String>,
    /// Track which features have been used (for debugging/reporting)
    pub features_used: HashSet<String>,
}

impl BuildFile {
    pub fn new(module_name: impl Into<String>) -> Self {
        let mut public_deps = HashSet::new();
        // Always include core dependencies
        public_deps.insert("Core".to_string());
        public_deps.insert("CoreUObject".to_string());
        public_deps.insert("Engine".to_string());
        
        Self {
            module_name: module_name.into(),
            public_dependencies: public_deps,
            private_dependencies: HashSet::new(),
            features_used: HashSet::new(),
        }
    }

    /// Add dependency based on feature usage
    /// Returns true if dependencies were added
    pub fn add_dependency_for_feature(&mut self, feature: &str) -> bool {
        // Track that this feature was used
        self.features_used.insert(feature.to_string());
        
        match feature {
            "Slate" | "SlateWidget" => {
                self.public_dependencies.insert("Slate".to_string());
                self.public_dependencies.insert("SlateCore".to_string());
                true
            }
            "UMG" | "Widget" => {
                self.public_dependencies.insert("UMG".to_string());
                true
            }
            "EnhancedInput" => {
                self.public_dependencies.insert("EnhancedInput".to_string());
                true
            }
            "Networking" | "Replication" => {
                self.public_dependencies.insert("OnlineSubsystem".to_string());
                true
            }
            "Shader" | "RenderCore" => {
                self.public_dependencies.insert("RenderCore".to_string());
                self.public_dependencies.insert("Renderer".to_string());
                self.public_dependencies.insert("RHI".to_string());
                true
            }
            "Projects" => {
                self.public_dependencies.insert("Projects".to_string());
                true
            }
            _ => false
        }
    }

    pub fn to_csharp(&self) -> String {
        let public_deps: Vec<_> = self.public_dependencies.iter().collect();
        let private_deps: Vec<_> = self.private_dependencies.iter().collect();
        
        format!(
            r#"using UnrealBuildTool;

public class {} : ModuleRules
{{
	public {}(ReadOnlyTargetRules Target) : base(Target)
	{{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {{
			{}
		}});

		PrivateDependencyModuleNames.AddRange(new string[] {{
			{}
		}});
	}}
}}"#,
            self.module_name,
            self.module_name,
            public_deps.iter().map(|s| format!("\"{}\"", s)).collect::<Vec<_>>().join(", "),
            if private_deps.is_empty() {
                "".to_string()
            } else {
                private_deps.iter().map(|s| format!("\"{}\"", s)).collect::<Vec<_>>().join(", ")
            }
        )
    }
}
