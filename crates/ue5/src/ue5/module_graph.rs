//! UE5 Module Dependency Graph
//!
//! Data-driven module dependency information extracted from all .Build.cs files
//! in the Unreal Engine source tree. Provides query APIs to:
//!   - Look up which module a type belongs to
//!   - Look up which module a header belongs to
//!   - Look up which module an API symbol belongs to
//!   - Get public/private dependencies for any module
//!   - Compute the minimal set of module deps needed for a set of referenced types
//!
//! Loaded from `unreal/metadata/module_graph.json` at compile time via `Ue5Context`.

use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};

// ═══════════════════════════════════════════════════════════════════
// Schema Types — mirrors module_graph.json structure
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModuleGraphData {
    #[serde(default)]
    pub _meta: ModuleGraphMeta,
    #[serde(default)]
    pub modules: HashMap<String, ModuleInfo>,
    #[serde(default)]
    pub transitive_public_deps: HashMap<String, Vec<String>>,
    #[serde(default)]
    pub type_to_module: HashMap<String, String>,
    #[serde(default)]
    pub header_to_module: HashMap<String, String>,
    #[serde(default)]
    pub api_to_module: HashMap<String, String>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ModuleGraphMeta {
    #[serde(default)]
    pub generator: String,
    #[serde(default)]
    pub source: String,
    #[serde(default)]
    pub total_modules: usize,
    #[serde(default)]
    pub total_types_mapped: usize,
    #[serde(default)]
    pub total_headers_mapped: usize,
    #[serde(default)]
    pub total_api_symbols: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ModuleInfo {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub category: String,
    #[serde(default)]
    pub path: String,
    #[serde(default)]
    pub public_deps: Vec<String>,
    #[serde(default)]
    pub private_deps: Vec<String>,
    #[serde(default)]
    pub dynamic_deps: Vec<String>,
    #[serde(default)]
    pub private_include_path_modules: Vec<String>,
    #[serde(default)]
    pub public_include_path_modules: Vec<String>,
}

// ═══════════════════════════════════════════════════════════════════
// ModuleGraph — Query API
// ═══════════════════════════════════════════════════════════════════

#[derive(Debug, Default, Clone)]
pub struct ModuleGraph {
    /// Module name → module info (deps, category, path)
    modules: HashMap<String, ModuleInfo>,

    /// Type name (UClass/UStruct/UEnum) → module name
    type_to_module: HashMap<String, String>,

    /// Header filename or relative path → module name
    header_to_module: HashMap<String, String>,

    /// Known API symbol → module name
    api_to_module: HashMap<String, String>,

    /// Module → transitive public dependency closure
    transitive_deps: HashMap<String, Vec<String>>,

    /// Total counts for diagnostics
    total_modules: usize,
    total_types: usize,
    total_headers: usize,
}

impl ModuleGraph {
    pub fn new() -> Self {
        Self::default()
    }

    /// Load from JSON string (module_graph.json content)
    pub fn load(&mut self, json_data: &str) -> Result<(), String> {
        let data: ModuleGraphData = serde_json::from_str(json_data)
            .map_err(|e| format!("Failed to parse module_graph.json: {}", e))?;

        self.modules = data.modules;
        self.type_to_module = data.type_to_module;
        self.header_to_module = data.header_to_module;
        self.api_to_module = data.api_to_module;
        self.transitive_deps = data.transitive_public_deps;

        self.total_modules = self.modules.len();
        self.total_types = self.type_to_module.len();
        self.total_headers = self.header_to_module.len();

        Ok(())
    }

    /// Check if the graph has been loaded with data
    pub fn is_loaded(&self) -> bool {
        self.total_modules > 0
    }

    // ─── Type → Module Queries ──────────────────────────────────

    /// Look up which module a UE5 type (class/struct/enum) belongs to.
    /// Handles both prefixed ("USceneComponent") and unprefixed ("SceneComponent") names.
    pub fn module_for_type(&self, type_name: &str) -> Option<&str> {
        // Direct lookup
        if let Some(m) = self.type_to_module.get(type_name) {
            return Some(m.as_str());
        }
        // Try with common UE5 prefixes
        for prefix in &["U", "A", "F", "E", "I", "S"] {
            let prefixed = format!("{}{}", prefix, type_name);
            if let Some(m) = self.type_to_module.get(&prefixed) {
                return Some(m.as_str());
            }
        }
        // Try stripping prefix
        if type_name.len() > 1 {
            let first = &type_name[..1];
            if ["U", "A", "F", "E", "I", "S"].contains(&first) {
                let stripped = &type_name[1..];
                if let Some(m) = self.type_to_module.get(stripped) {
                    return Some(m.as_str());
                }
            }
        }
        None
    }

    // ─── Header → Module Queries ────────────────────────────────

    /// Look up which module a header file belongs to.
    /// Accepts either a filename ("ShaderCore.h") or relative path ("Shader/ShaderCore.h").
    pub fn module_for_header(&self, header: &str) -> Option<&str> {
        if let Some(m) = self.header_to_module.get(header) {
            return Some(m.as_str());
        }
        // Try just the filename if a path was given
        if let Some(fname) = header.rsplit('/').next() {
            if fname != header {
                if let Some(m) = self.header_to_module.get(fname) {
                    return Some(m.as_str());
                }
            }
        }
        None
    }

    // ─── API Symbol → Module Queries ────────────────────────────

    /// Look up which module provides a known API function/symbol.
    pub fn module_for_api(&self, symbol: &str) -> Option<&str> {
        self.api_to_module.get(symbol).map(|s| s.as_str())
    }

    // ─── Module Info Queries ────────────────────────────────────

    /// Get full module info by name
    pub fn get_module(&self, name: &str) -> Option<&ModuleInfo> {
        self.modules.get(name)
    }

    /// Get public dependencies for a module
    pub fn public_deps(&self, module: &str) -> &[String] {
        self.modules
            .get(module)
            .map(|m| m.public_deps.as_slice())
            .unwrap_or(&[])
    }

    /// Get private dependencies for a module
    pub fn private_deps(&self, module: &str) -> &[String] {
        self.modules
            .get(module)
            .map(|m| m.private_deps.as_slice())
            .unwrap_or(&[])
    }

    /// Get the transitive public dependency closure for a module
    pub fn transitive_public_deps(&self, module: &str) -> &[String] {
        self.transitive_deps
            .get(module)
            .map(|v| v.as_slice())
            .unwrap_or(&[])
    }

    /// Get the category (Runtime, Editor, Developer, etc.) for a module
    pub fn module_category(&self, module: &str) -> Option<&str> {
        self.modules.get(module).map(|m| m.category.as_str())
    }

    /// Check if a module exists in the graph
    pub fn has_module(&self, name: &str) -> bool {
        self.modules.contains_key(name)
    }

    // ─── Dependency Resolution (the key feature) ────────────────

    /// Given a set of referenced type names, compute the minimal set of
    /// module dependencies needed. This is the core query for auto-deriving
    /// Build.cs dependencies.
    ///
    /// Returns (public_deps, private_deps) where:
    /// - public_deps: modules that should be in PublicDependencyModuleNames
    /// - private_deps: modules only needed for implementation (headers in .cpp)
    ///
    /// The `base_modules` parameter specifies modules that are always included
    /// (e.g., "Core", "CoreUObject", "Engine") and should not be duplicated.
    pub fn resolve_deps_for_types(
        &self,
        referenced_types: &[&str],
        referenced_headers: &[&str],
        referenced_apis: &[&str],
        base_modules: &[&str],
    ) -> Vec<String> {
        let base_set: HashSet<&str> = base_modules.iter().copied().collect();
        let mut needed: HashSet<String> = HashSet::new();

        // Resolve types → modules
        for type_name in referenced_types {
            if let Some(module) = self.module_for_type(type_name) {
                if !base_set.contains(module) {
                    needed.insert(module.to_string());
                }
            }
        }

        // Resolve headers → modules
        for header in referenced_headers {
            if let Some(module) = self.module_for_header(header) {
                if !base_set.contains(module) {
                    needed.insert(module.to_string());
                }
            }
        }

        // Resolve API symbols → modules
        for api in referenced_apis {
            if let Some(module) = self.module_for_api(api) {
                if !base_set.contains(module) {
                    needed.insert(module.to_string());
                }
            }
        }

        // Remove modules that are transitively provided by other needed modules
        let needed_vec: Vec<String> = needed.iter().cloned().collect();
        let mut redundant: HashSet<String> = HashSet::new();
        for module in &needed_vec {
            let transitive = self.transitive_public_deps(module);
            for dep in transitive {
                if needed.contains(dep) && dep != module {
                    // `dep` is transitively provided by `module`, but only mark
                    // redundant if `module` is NOT transitively provided by `dep`
                    let dep_transitive = self.transitive_public_deps(dep);
                    if !dep_transitive.contains(module) {
                        redundant.insert(dep.clone());
                    }
                }
            }
        }

        let mut result: Vec<String> = needed
            .into_iter()
            .filter(|m| !redundant.contains(m))
            .collect();
        result.sort();
        result
    }

    // ─── Diagnostics ────────────────────────────────────────────

    /// Get summary statistics
    pub fn stats(&self) -> (usize, usize, usize) {
        (self.total_modules, self.total_types, self.total_headers)
    }

    /// Get all module names
    pub fn module_names(&self) -> Vec<&str> {
        self.modules.keys().map(|s| s.as_str()).collect()
    }

    /// Get all modules in a specific category
    pub fn modules_in_category(&self, category: &str) -> Vec<&str> {
        self.modules
            .values()
            .filter(|m| m.category == category)
            .map(|m| m.name.as_str())
            .collect()
    }
}

// ═══════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_graph() -> ModuleGraph {
        let json = r#"{
            "_meta": {
                "generator": "test",
                "total_modules": 5,
                "total_types_mapped": 4,
                "total_headers_mapped": 3,
                "total_api_symbols": 2
            },
            "modules": {
                "Core": {
                    "name": "Core",
                    "category": "Runtime",
                    "path": "Runtime/Core/Core.Build.cs",
                    "public_deps": [],
                    "private_deps": [],
                    "dynamic_deps": [],
                    "private_include_path_modules": [],
                    "public_include_path_modules": []
                },
                "CoreUObject": {
                    "name": "CoreUObject",
                    "category": "Runtime",
                    "path": "Runtime/CoreUObject/CoreUObject.Build.cs",
                    "public_deps": ["Core"],
                    "private_deps": [],
                    "dynamic_deps": [],
                    "private_include_path_modules": [],
                    "public_include_path_modules": []
                },
                "Engine": {
                    "name": "Engine",
                    "category": "Runtime",
                    "path": "Runtime/Engine/Engine.Build.cs",
                    "public_deps": ["Core", "CoreUObject"],
                    "private_deps": ["RenderCore"],
                    "dynamic_deps": [],
                    "private_include_path_modules": [],
                    "public_include_path_modules": []
                },
                "RenderCore": {
                    "name": "RenderCore",
                    "category": "Runtime",
                    "path": "Runtime/RenderCore/RenderCore.Build.cs",
                    "public_deps": ["RHI", "CoreUObject"],
                    "private_deps": ["Core"],
                    "dynamic_deps": [],
                    "private_include_path_modules": [],
                    "public_include_path_modules": []
                },
                "RHI": {
                    "name": "RHI",
                    "category": "Runtime",
                    "path": "Runtime/RHI/RHI.Build.cs",
                    "public_deps": [],
                    "private_deps": ["Core"],
                    "dynamic_deps": [],
                    "private_include_path_modules": [],
                    "public_include_path_modules": []
                }
            },
            "transitive_public_deps": {
                "Core": [],
                "CoreUObject": ["Core"],
                "Engine": ["Core", "CoreUObject"],
                "RenderCore": ["CoreUObject", "Core", "RHI"],
                "RHI": []
            },
            "type_to_module": {
                "USceneComponent": "Engine",
                "AActor": "Engine",
                "FShaderMapResource": "RenderCore",
                "UObject": "CoreUObject"
            },
            "header_to_module": {
                "ShaderCore.h": "RenderCore",
                "Actor.h": "Engine",
                "Object.h": "CoreUObject"
            },
            "api_to_module": {
                "AddShaderSourceDirectoryMapping": "RenderCore",
                "AllShaderSourceDirectoryMappings": "RenderCore"
            }
        }"#;

        let mut graph = ModuleGraph::new();
        graph.load(json).unwrap();
        graph
    }

    #[test]
    fn test_load_and_stats() {
        let g = make_test_graph();
        assert!(g.is_loaded());
        let (modules, types, headers) = g.stats();
        assert_eq!(modules, 5);
        assert_eq!(types, 4);
        assert_eq!(headers, 3);
    }

    #[test]
    fn test_module_for_type_direct() {
        let g = make_test_graph();
        assert_eq!(g.module_for_type("USceneComponent"), Some("Engine"));
        assert_eq!(g.module_for_type("AActor"), Some("Engine"));
        assert_eq!(g.module_for_type("FShaderMapResource"), Some("RenderCore"));
        assert_eq!(g.module_for_type("UObject"), Some("CoreUObject"));
    }

    #[test]
    fn test_module_for_type_prefix_stripping() {
        let g = make_test_graph();
        // Should find "USceneComponent" when given "SceneComponent"
        assert_eq!(g.module_for_type("SceneComponent"), Some("Engine"));
        assert_eq!(g.module_for_type("Actor"), Some("Engine"));
    }

    #[test]
    fn test_module_for_type_unknown() {
        let g = make_test_graph();
        assert_eq!(g.module_for_type("FMyCustomThing"), None);
    }

    #[test]
    fn test_module_for_header() {
        let g = make_test_graph();
        assert_eq!(g.module_for_header("ShaderCore.h"), Some("RenderCore"));
        assert_eq!(g.module_for_header("Actor.h"), Some("Engine"));
        assert_eq!(g.module_for_header("NonExistent.h"), None);
    }

    #[test]
    fn test_module_for_api() {
        let g = make_test_graph();
        assert_eq!(
            g.module_for_api("AddShaderSourceDirectoryMapping"),
            Some("RenderCore")
        );
        assert_eq!(
            g.module_for_api("AllShaderSourceDirectoryMappings"),
            Some("RenderCore")
        );
        assert_eq!(g.module_for_api("UnknownFunction"), None);
    }

    #[test]
    fn test_public_deps() {
        let g = make_test_graph();
        assert_eq!(g.public_deps("RenderCore"), &["RHI", "CoreUObject"]);
        assert_eq!(g.public_deps("Core"), &[] as &[String]);
    }

    #[test]
    fn test_transitive_deps() {
        let g = make_test_graph();
        let trans = g.transitive_public_deps("RenderCore");
        assert!(trans.contains(&"RHI".to_string()));
        assert!(trans.contains(&"CoreUObject".to_string()));
        assert!(trans.contains(&"Core".to_string()));
    }

    #[test]
    fn test_module_category() {
        let g = make_test_graph();
        assert_eq!(g.module_category("RenderCore"), Some("Runtime"));
        assert_eq!(g.module_category("NonExistent"), None);
    }

    #[test]
    fn test_resolve_deps_for_types() {
        let g = make_test_graph();
        // If we reference AddShaderSourceDirectoryMapping, we need RenderCore
        // Base modules are Core, CoreUObject, Engine — should not be duplicated
        let deps = g.resolve_deps_for_types(
            &[],
            &["ShaderCore.h"],
            &["AddShaderSourceDirectoryMapping"],
            &["Core", "CoreUObject", "Engine"],
        );
        assert!(deps.contains(&"RenderCore".to_string()));
        // Core/CoreUObject/Engine should NOT be in the result (they're base)
        assert!(!deps.contains(&"Core".to_string()));
        assert!(!deps.contains(&"Engine".to_string()));
    }

    #[test]
    fn test_resolve_deps_deduplicates_transitive() {
        let g = make_test_graph();
        // RenderCore transitively provides RHI and CoreUObject
        // If we only reference types from RenderCore, we should get RenderCore
        // but NOT separately get RHI (it's transitive via RenderCore)
        let deps = g.resolve_deps_for_types(
            &["FShaderMapResource"],
            &[],
            &[],
            &["Core", "CoreUObject", "Engine"],
        );
        assert!(deps.contains(&"RenderCore".to_string()));
        // RHI is transitively provided by RenderCore, so it should be pruned
        assert!(!deps.contains(&"RHI".to_string()));
    }

    #[test]
    fn test_has_module() {
        let g = make_test_graph();
        assert!(g.has_module("RenderCore"));
        assert!(g.has_module("Engine"));
        assert!(!g.has_module("FakeModule"));
    }
}
