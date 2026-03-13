use kain_core::error::{KainError, KainResult};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeSet, HashMap, HashSet};
use std::path::PathBuf;

/// Module dependency information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModuleInfo {
    pub name: String,
    pub public_deps: Vec<String>,
    pub private_deps: Vec<String>,
}

/// Resolved dependencies for a plugin
#[derive(Debug, Clone)]
pub struct Dependencies {
    pub public_modules: BTreeSet<String>,
    pub private_modules: BTreeSet<String>,
    pub circular_deps: Vec<(String, String)>,
}

impl Dependencies {
    pub fn new() -> Self {
        Self {
            public_modules: BTreeSet::new(),
            private_modules: BTreeSet::new(),
            circular_deps: Vec::new(),
        }
    }
}

/// Resolves module dependencies from generated code
pub struct DependencyResolver {
    /// Module mappings loaded from metadata
    pub module_map: HashMap<String, Vec<String>>,
    /// Include-to-module mappings
    pub include_to_modules: HashMap<String, Vec<String>>,
}

impl DependencyResolver {
    /// Create a new DependencyResolver with default mappings
    pub fn new() -> Self {
        Self {
            module_map: Self::default_module_map(),
            include_to_modules: Self::default_include_map(),
        }
    }

    /// Load module mappings from engine_modules.json if present, otherwise use defaults
    pub fn load(metadata_path: Option<PathBuf>) -> KainResult<Self> {
        if let Some(path) = metadata_path {
            // Try to load from JSON
            if path.exists() {
                let content = std::fs::read_to_string(&path)
                    .map_err(|e| KainError::io_error(e.to_string()))?;

                let module_map: HashMap<String, Vec<String>> = serde_json::from_str(&content)
                    .map_err(|e| {
                        KainError::config_error(format!(
                            "Failed to parse engine_modules.json: {}",
                            e
                        ))
                    })?;

                Ok(Self {
                    module_map: module_map.clone(),
                    include_to_modules: Self::build_include_map(&module_map),
                })
            } else {
                Ok(Self::new())
            }
        } else {
            Ok(Self::new())
        }
    }

    /// Build include-to-module mapping from module map
    fn build_include_map(
        _module_map: &HashMap<String, Vec<String>>,
    ) -> HashMap<String, Vec<String>> {
        // This would be populated from metadata in a real implementation
        // For now, return the default mapping
        Self::default_include_map()
    }

    /// Default module mappings (hardcoded fallback)
    pub fn default_module_map() -> HashMap<String, Vec<String>> {
        let mut map = HashMap::new();

        // Core modules
        map.insert("Core".to_string(), vec![]);
        map.insert("CoreUObject".to_string(), vec!["Core".to_string()]);
        map.insert(
            "Engine".to_string(),
            vec!["Core".to_string(), "CoreUObject".to_string()],
        );

        // Rendering modules
        map.insert("RenderCore".to_string(), vec!["Core".to_string()]);
        map.insert("RHI".to_string(), vec!["Core".to_string()]);
        map.insert(
            "Renderer".to_string(),
            vec![
                "Core".to_string(),
                "RenderCore".to_string(),
                "RHI".to_string(),
            ],
        );

        // Slate modules
        map.insert(
            "SlateCore".to_string(),
            vec!["Core".to_string(), "CoreUObject".to_string()],
        );
        map.insert(
            "Slate".to_string(),
            vec!["Core".to_string(), "SlateCore".to_string()],
        );

        // Editor modules
        map.insert(
            "UnrealEd".to_string(),
            vec![
                "Core".to_string(),
                "CoreUObject".to_string(),
                "Engine".to_string(),
            ],
        );
        map.insert(
            "PropertyEditor".to_string(),
            vec![
                "Core".to_string(),
                "SlateCore".to_string(),
                "Slate".to_string(),
            ],
        );
        map.insert(
            "AssetTools".to_string(),
            vec!["Core".to_string(), "UnrealEd".to_string()],
        );
        map.insert(
            "EditorStyle".to_string(),
            vec!["Core".to_string(), "SlateCore".to_string()],
        );
        map.insert(
            "AdvancedPreviewScene".to_string(),
            vec!["Core".to_string(), "Engine".to_string()],
        );
        map.insert(
            "ToolMenus".to_string(),
            vec!["Core".to_string(), "Slate".to_string()],
        );

        // Networking modules
        map.insert("NetCore".to_string(), vec!["Core".to_string()]);

        // Other modules
        map.insert("InputCore".to_string(), vec!["Core".to_string()]);
        map.insert("Projects".to_string(), vec!["Core".to_string()]);
        map.insert(
            "DataTable".to_string(),
            vec![
                "Core".to_string(),
                "CoreUObject".to_string(),
                "Engine".to_string(),
            ],
        );

        map
    }

    /// Default include-to-module mappings
    fn default_include_map() -> HashMap<String, Vec<String>> {
        let mut map = HashMap::new();

        // Core includes
        map.insert("CoreMinimal.h".to_string(), vec!["Core".to_string()]);
        map.insert(
            "UObject/Object.h".to_string(),
            vec!["CoreUObject".to_string()],
        );
        map.insert(
            "GameFramework/Actor.h".to_string(),
            vec!["Engine".to_string()],
        );
        map.insert(
            "Components/ActorComponent.h".to_string(),
            vec!["Engine".to_string()],
        );

        // Rendering includes
        map.insert(
            "RenderResource.h".to_string(),
            vec!["RenderCore".to_string()],
        );
        map.insert("RHI.h".to_string(), vec!["RHI".to_string()]);
        map.insert("GlobalShader.h".to_string(), vec!["RenderCore".to_string()]);
        map.insert(
            "ShaderParameters.h".to_string(),
            vec!["RenderCore".to_string()],
        );
        map.insert(
            "ShaderParameterStruct.h".to_string(),
            vec!["RenderCore".to_string()],
        );

        // Slate includes
        map.insert(
            "Widgets/SCompoundWidget.h".to_string(),
            vec!["SlateCore".to_string()],
        );
        map.insert(
            "Widgets/SWidget.h".to_string(),
            vec!["SlateCore".to_string()],
        );
        map.insert(
            "Widgets/DeclarativeSyntaxSupport.h".to_string(),
            vec!["SlateCore".to_string()],
        );
        map.insert(
            "Widgets/Input/SButton.h".to_string(),
            vec!["Slate".to_string()],
        );
        map.insert(
            "Widgets/Text/STextBlock.h".to_string(),
            vec!["Slate".to_string()],
        );
        map.insert(
            "Widgets/Layout/SBox.h".to_string(),
            vec!["Slate".to_string()],
        );

        // Editor includes
        map.insert("Editor.h".to_string(), vec!["UnrealEd".to_string()]);
        map.insert(
            "IDetailCustomization.h".to_string(),
            vec!["PropertyEditor".to_string()],
        );
        map.insert(
            "DetailLayoutBuilder.h".to_string(),
            vec!["PropertyEditor".to_string()],
        );
        map.insert(
            "DetailCategoryBuilder.h".to_string(),
            vec!["PropertyEditor".to_string()],
        );
        map.insert(
            "DetailWidgetRow.h".to_string(),
            vec!["PropertyEditor".to_string()],
        );
        map.insert(
            "SEditorViewport.h".to_string(),
            vec!["UnrealEd".to_string(), "AdvancedPreviewScene".to_string()],
        );
        map.insert(
            "EditorViewportClient.h".to_string(),
            vec!["UnrealEd".to_string()],
        );
        map.insert(
            "AssetEditorToolkit.h".to_string(),
            vec!["UnrealEd".to_string()],
        );
        map.insert("ToolMenus.h".to_string(), vec!["ToolMenus".to_string()]);
        map.insert(
            "Framework/Commands/Commands.h".to_string(),
            vec!["Slate".to_string()],
        );

        // Networking includes
        map.insert(
            "Net/UnrealNetwork.h".to_string(),
            vec!["Engine".to_string()],
        );
        map.insert(
            "Engine/NetSerialization.h".to_string(),
            vec!["Engine".to_string()],
        );

        // DataTable includes
        map.insert(
            "Engine/DataTable.h".to_string(),
            vec!["Engine".to_string(), "DataTable".to_string()],
        );

        map
    }

    /// Analyze generated files and detect module dependencies
    /// Requirement 6.1: Parse #include statements from generated files
    pub fn analyze(&self, generated_files: &[(PathBuf, String)]) -> KainResult<Dependencies> {
        let mut deps = Dependencies::new();

        // Parse includes from all generated files
        for (file_path, content) in generated_files {
            self.analyze_file(&mut deps, file_path, content)?;
        }

        // Validate dependencies (Requirement 6.10: Detect circular dependencies)
        self.validate_dependencies(&mut deps)?;

        Ok(deps)
    }

    /// Analyze a single file for dependencies
    /// Requirement 6.1: Map includes to UE5 modules using module_map
    fn analyze_file(
        &self,
        deps: &mut Dependencies,
        file_path: &PathBuf,
        content: &str,
    ) -> KainResult<()> {
        let is_header = file_path.extension().and_then(|s| s.to_str()) == Some("h");

        // Parse #include statements
        for line in content.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with("#include") {
                if let Some(include) = self.extract_include(trimmed) {
                    // Map include to modules
                    if let Some(modules) = self.include_to_modules.get(&include) {
                        for module in modules {
                            // Headers typically need public dependencies
                            // Implementation files use private dependencies
                            if is_header {
                                deps.public_modules.insert(module.clone());
                            } else {
                                deps.private_modules.insert(module.clone());
                            }
                        }
                    } else {
                        // Try to infer module from include path
                        if let Some(module) = self.infer_module_from_include(&include) {
                            if is_header {
                                deps.public_modules.insert(module);
                            } else {
                                deps.private_modules.insert(module);
                            }
                        }
                    }
                }
            }
        }

        Ok(())
    }

    /// Infer module name from include path
    fn infer_module_from_include(&self, include: &str) -> Option<String> {
        // Common patterns:
        // - "Engine/..." -> Engine
        // - "Slate/..." -> Slate
        // - "SlateCore/..." -> SlateCore
        // - "RenderCore/..." -> RenderCore

        if let Some(first_part) = include.split('/').next() {
            // Check if it matches a known module
            if self.module_map.contains_key(first_part) {
                return Some(first_part.to_string());
            }
        }

        None
    }

    /// Extract include path from #include statement
    pub fn extract_include(&self, line: &str) -> Option<String> {
        // Handle both #include "..." and #include <...>
        if let Some(start) = line.find('"') {
            if let Some(end) = line[start + 1..].find('"') {
                return Some(line[start + 1..start + 1 + end].to_string());
            }
        }
        if let Some(start) = line.find('<') {
            if let Some(end) = line[start + 1..].find('>') {
                return Some(line[start + 1..start + 1 + end].to_string());
            }
        }
        None
    }

    /// Validate dependencies for circular references
    /// Requirement 6.10: Check for circular module dependencies, verify all modules exist, warn about missing optional modules
    pub fn validate_dependencies(&self, deps: &mut Dependencies) -> KainResult<()> {
        // Check for circular dependencies using the module map
        let mut visited = HashSet::new();
        let mut rec_stack = HashSet::new();
        let mut path = Vec::new();

        for module in deps
            .public_modules
            .iter()
            .chain(deps.private_modules.iter())
        {
            // Verify module exists in our known modules
            if !self.module_map.contains_key(module) {
                // Check if it's a commonly optional module
                if self.is_optional_module(module) {
                    eprintln!(
                        "Warning: Optional module '{}' not found in module map",
                        module
                    );
                } else {
                    eprintln!(
                        "Warning: Module '{}' not found in module map - it may not exist in UE5",
                        module
                    );
                }
            }

            // Check for circular dependencies
            if let Some(cycle) = self.find_cycle(module, &mut visited, &mut rec_stack, &mut path) {
                // Record the circular dependency
                if cycle.len() >= 2 {
                    deps.circular_deps
                        .push((cycle[0].clone(), cycle[1].clone()));
                }

                return Err(KainError::validation_error(format!(
                    "Circular dependency detected: {}",
                    cycle.join(" -> ")
                )));
            }
        }

        Ok(())
    }

    /// Check if a module is commonly optional
    fn is_optional_module(&self, module: &str) -> bool {
        matches!(
            module,
            "AdvancedPreviewScene"
                | "ToolMenus"
                | "DataTable"
                | "NetCore"
                | "AssetTools"
                | "PropertyEditor"
        )
    }

    /// Find cycles in module dependencies using DFS, returning the cycle path if found
    fn find_cycle(
        &self,
        module: &str,
        visited: &mut HashSet<String>,
        rec_stack: &mut HashSet<String>,
        path: &mut Vec<String>,
    ) -> Option<Vec<String>> {
        if rec_stack.contains(module) {
            // Found a cycle - extract the cycle from the path
            if let Some(cycle_start) = path.iter().position(|m| m == module) {
                let mut cycle = path[cycle_start..].to_vec();
                cycle.push(module.to_string());
                return Some(cycle);
            }
            return Some(vec![module.to_string()]);
        }

        if visited.contains(module) {
            return None;
        }

        visited.insert(module.to_string());
        rec_stack.insert(module.to_string());
        path.push(module.to_string());

        if let Some(deps) = self.module_map.get(module) {
            for dep in deps {
                if let Some(cycle) = self.find_cycle(dep, visited, rec_stack, path) {
                    return Some(cycle);
                }
            }
        }

        path.pop();
        rec_stack.remove(module);
        None
    }

    /// Add automatic module dependencies based on feature detection
    pub fn add_automatic_modules(
        &self,
        deps: &mut Dependencies,
        has_shaders: bool,
        has_slate: bool,
        has_details: bool,
        has_viewport: bool,
        has_asset_editor: bool,
        has_toolbar: bool,
        has_networking: bool,
    ) {
        // Requirement 6.2: Add RenderCore, RHI for shaders
        if has_shaders {
            deps.public_modules.insert("RenderCore".to_string());
            deps.public_modules.insert("RHI".to_string());
        }

        // Requirement 6.3: Add Slate, SlateCore for Slate widgets
        if has_slate {
            deps.private_modules.insert("Slate".to_string());
            deps.private_modules.insert("SlateCore".to_string());
        }

        // Requirement 6.4: Add PropertyEditor for Details panels
        if has_details {
            deps.private_modules.insert("PropertyEditor".to_string());
        }

        // Requirement 6.5: Add UnrealEd, AssetTools for asset editors
        if has_asset_editor {
            deps.private_modules.insert("UnrealEd".to_string());
            deps.private_modules.insert("AssetTools".to_string());
        }

        // Add AdvancedPreviewScene for viewports
        if has_viewport {
            deps.private_modules
                .insert("AdvancedPreviewScene".to_string());
        }

        // Add ToolMenus for toolbars
        if has_toolbar {
            deps.private_modules.insert("ToolMenus".to_string());
        }

        // Requirement 6.6: Add Engine, NetCore for networking
        if has_networking {
            deps.public_modules.insert("Engine".to_string());
            deps.public_modules.insert("NetCore".to_string());
        }
    }
}

impl Default for DependencyResolver {
    fn default() -> Self {
        Self::new()
    }
}
