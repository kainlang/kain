//! UE5 Compilation Context
//! 
//! Provides shared state and symbol tables for cross-module intelligence.
//! This allows the Editor codegen to know what the Runtime codegen created,
//! enabling automatic registration and type-safe references.

use std::collections::{HashSet, HashMap};
use std::cell::RefCell;
use super::project::BuildFile;
use super::engine_knowledge::EngineKnowledge;
use super::widget_registry::WidgetRegistry;
use super::editor_attributes::EditorAttributesRegistry;
use ue5_shaders::ShaderKnowledge;
use super::uht_rules::UhtRules;
use super::module_graph::ModuleGraph;
use super::virtual_obligations::VirtualObligations;

/// Shared compilation context for UE5 code generation
/// 
/// This context is passed between different codegen phases to enable
/// cross-module intelligence. For example:
/// - Runtime codegen registers actors/components
/// - Editor codegen can reference them for UI generation
/// - Shader codegen can auto-wire to actor state
#[derive(Debug, Clone)]
pub struct Ue5Context {
    /// All enum names defined in the program
    pub enum_names: HashSet<String>,
    
    /// All struct names defined in the program
    pub struct_names: HashSet<String>,
    
    /// All component names (structs with @component attribute)
    pub component_names: HashSet<String>,
    
    /// All subsystem names (structs with @subsystem attribute)
    pub subsystem_names: HashSet<String>,
    
    /// All delegate names (type aliases to function types)
    pub delegate_names: HashSet<String>,
    
    /// All actor names defined in the program
    pub actor_names: HashSet<String>,
    
    /// Maps KAIN identifiers to UE5 equivalents (e.g. delta_time -> DeltaTime)
    pub ident_remap: HashMap<String, String>,
    
    /// Forward declarations needed in header
    pub forward_decls: HashSet<String>,
    
    /// Module API macro (e.g. "GAME_API", "MYPLUGIN_API")
    pub module_api: String,
    
    /// Output name (used for class names, file names, etc.)
    pub output_name: String,
    
    /// Features enabled via Library of Babel (e.g., "Mograph", "Replication")
    pub enabled_features: HashSet<String>,
    
    /// Build file for automatic dependency management
    pub build_file: BuildFile,
    
    /// StdLib call resolver for UE5 mappings (Babel Layer)
    pub resolver: StdLibResolver,
    
    /// Copyright header for generated files
    pub copyright: String,

    /// Maps KAIN type names to their generated header files (e.g. MyActor -> MyActor.h)
    pub type_to_header: HashMap<String, String>,

    /// Headers actually used in the current generation (for #include resolution)
    pub needed_headers: RefCell<HashSet<String>>,

    /// Rich engine knowledge base (class hierarchy, includes, type validation)
    pub knowledge: EngineKnowledge,

    /// Module dependencies discovered during codegen (for .Build.cs)
    pub needed_modules: RefCell<HashSet<String>>,

    /// Slate widget registry (properties, events, delegate types)
    pub widget_registry: WidgetRegistry,

    /// Editor attributes registry (attribute definitions, naming conventions, boilerplate)
    pub editor_attributes: EditorAttributesRegistry,

    /// Shader knowledge base (intrinsics, includes, permutations, material properties)
    pub shader_knowledge: ShaderKnowledge,

    /// UHT validation rules (specifiers, property types, incompatible combos)
    pub uht_rules: UhtRules,

    /// Module dependency graph (type→module, header→module, API→module)
    pub module_graph: ModuleGraph,

    /// Virtual method obligations (pure virtual overrides required by base classes)
    pub virtual_obligations: VirtualObligations,

    /// Trait implementations (class_name -> [trait_names])
    pub trait_impls: HashMap<String, Vec<String>>,
    
    /// KAIN marker configuration (for round-trip compilation)
    pub marker_config: crate::ue5::kain_markers::MarkerConfig,
}

use super::resolver::StdLibResolver;

impl Ue5Context {
    /// Create a new context with the given output name
    pub fn new(output_name: &str, copyright: Option<&str>) -> Self {
        let module_api = format!("{}_API", output_name.to_uppercase());
        let mut resolver = StdLibResolver::new();
        let mut knowledge = EngineKnowledge::new();
        let mut widget_registry = WidgetRegistry::new();
        let mut editor_attributes = EditorAttributesRegistry::new();
        let mut shader_knowledge = ShaderKnowledge::new();
        let mut uht_rules = UhtRules::new();
        let mut module_graph = ModuleGraph::new();
        let mut virtual_obligations = VirtualObligations::new();
        
        // Load all metadata from unreal/metadata/*.json into both systems
        let metadata_dir = std::path::Path::new("unreal/metadata");
        if metadata_dir.exists() && metadata_dir.is_dir() {
            if let Ok(entries) = std::fs::read_dir(metadata_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().map_or(false, |e| e == "json") {
                        if let Ok(data) = std::fs::read_to_string(&path) {
                            let filename = path.file_name().and_then(|f| f.to_str()).unwrap_or("");
                            if filename == "widget_registry.json" {
                                let _ = widget_registry.load(&data);
                            } else if filename == "editor_attributes.json" {
                                let _ = editor_attributes.load(&data);
                            } else if filename == "shader_knowledge.json" {
                                let _ = shader_knowledge.load(&data);
                            } else if filename == "uht_rules.json" {
                                let _ = uht_rules.load(&data);
                            } else if filename == "module_graph.json" {
                                let _ = module_graph.load(&data);
                            } else if filename == "virtual_obligations.json" {
                                let _ = virtual_obligations.load(&data);
                            } else {
                                // Feed into EngineKnowledge system
                                let _ = knowledge.load_metadata(&data);
                                // Legacy: also feed into StdLibResolver
                                let _ = resolver.load_from_metadata(&data);
                            }
                        }
                    }
                }
            }
        }

        let copyright = copyright.map(|s| s.to_string()).unwrap_or_else(|| {
            format!("Copyright {} Zentako. All Rights Reserved.", 
                chrono::Local::now().format("%Y"))
        });
        
        Self {
            enum_names: HashSet::new(),
            struct_names: HashSet::new(),
            component_names: HashSet::new(),
            subsystem_names: HashSet::new(),
            delegate_names: HashSet::new(),
            actor_names: HashSet::new(),
            ident_remap: HashMap::new(),
            forward_decls: HashSet::new(),
            module_api: module_api.clone(),
            output_name: output_name.to_string(),
            enabled_features: HashSet::new(),
            build_file: BuildFile::new(output_name),
            resolver,
            copyright,
            type_to_header: HashMap::new(),
            needed_headers: RefCell::new(HashSet::new()),
            knowledge,
            needed_modules: RefCell::new(HashSet::new()),
            widget_registry,
            editor_attributes,
            shader_knowledge,
            uht_rules,
            module_graph,
            virtual_obligations,
            trait_impls: HashMap::new(),
            marker_config: crate::ue5::kain_markers::MarkerConfig::default(),
        }
    }

    /// Set the global type map for modular compilation
    pub fn set_type_to_header(&mut self, map: HashMap<String, String>) {
        self.type_to_header = map;
    }

    /// Mark a module dependency as needed (discovered during codegen via EngineKnowledge)
    pub fn need_module(&self, module: String) {
        self.needed_modules.borrow_mut().insert(module);
    }

    /// Flush discovered module dependencies into the build file
    pub fn flush_modules(&mut self) {
        let modules: Vec<String> = self.needed_modules.borrow().iter().cloned().collect();
        for module in modules {
            self.build_file.public_dependencies.insert(module);
        }
    }
    
    /// Enable KAIN source markers in generated C++ (for round-trip compilation)
    pub fn enable_markers(&mut self, style: crate::ue5::kain_markers::MarkerStyle) {
        self.marker_config = crate::ue5::kain_markers::MarkerConfig {
            style,
            include_attributes: true,
            include_types: true,
            include_expressions: true,  // Maximum verbosity for debugging
        };
    }

    /// Mark a header as needed for current generation
    pub fn need_header(&self, header: String) {
        if header != format!("{}.h", self.output_name) {
            self.needed_headers.borrow_mut().insert(header);
        }
    }

    /// Get all headers needed for the current translation unit
    pub fn get_needed_headers(&self) -> Vec<String> {
        let mut headers: Vec<_> = self.needed_headers.borrow().iter().cloned().collect();
        headers.sort();
        headers
    }
    
    /// Register an enum name and its header
    pub fn register_enum(&mut self, name: String, header: String) {
        self.enum_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Register a struct name and its header
    pub fn register_struct(&mut self, name: String, header: String) {
        self.struct_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Register a component name and its header
    pub fn register_component(&mut self, name: String, header: String) {
        self.component_names.insert(name.clone());
        self.struct_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Register a subsystem name and its header
    pub fn register_subsystem(&mut self, name: String, header: String) {
        self.subsystem_names.insert(name.clone());
        self.struct_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Register a delegate name and its header
    pub fn register_delegate(&mut self, name: String, header: String) {
        self.delegate_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Register an actor name and its header
    pub fn register_actor(&mut self, name: String, header: String) {
        self.actor_names.insert(name.clone());
        self.type_to_header.insert(name, header);
    }
    
    /// Check if a name is a known enum
    pub fn is_enum(&self, name: &str) -> bool {
        self.enum_names.contains(name)
    }
    
    /// Check if a name is a known struct
    pub fn is_struct(&self, name: &str) -> bool {
        self.struct_names.contains(name)
    }
    
    /// Check if a name is a known component
    pub fn is_component(&self, name: &str) -> bool {
        self.component_names.contains(name)
    }
    
    /// Check if a name is a known subsystem
    pub fn is_subsystem(&self, name: &str) -> bool {
        self.subsystem_names.contains(name)
    }
    
    /// Check if a name is a known delegate
    pub fn is_delegate(&self, name: &str) -> bool {
        self.delegate_names.contains(name)
    }
    
    /// Check if a name is a known actor
    pub fn is_actor(&self, name: &str) -> bool {
        self.actor_names.contains(name)
    }
    
    /// Add an identifier remapping (e.g. delta_time -> DeltaTime)
    pub fn add_ident_remap(&mut self, from: String, to: String) {
        self.ident_remap.insert(from, to);
    }
    
    /// Get remapped identifier, or return original if no mapping exists
    pub fn remap_ident(&self, name: &str) -> String {
        self.ident_remap.get(name).cloned().unwrap_or_else(|| name.to_string())
    }
    
    /// Clear identifier remappings (useful after generating a function body)
    pub fn clear_ident_remaps(&mut self) {
        self.ident_remap.clear();
    }
    
    /// Add a forward declaration
    pub fn add_forward_decl(&mut self, decl: String) {
        self.forward_decls.insert(decl);
    }
    
    /// Add a feature dependency (automatically updates build file)
    pub fn use_feature(&mut self, feature: &str) {
        self.build_file.add_dependency_for_feature(feature);
    }
    
    /// Get all forward declarations as a formatted string
    pub fn get_forward_decls(&self) -> String {
        if self.forward_decls.is_empty() {
            return String::new();
        }
        
        let mut decls: Vec<_> = self.forward_decls.iter().collect();
        decls.sort();
        
        let mut result = String::from("// Forward declarations\n");
        for decl in decls {
            result.push_str(decl);
            result.push_str(";\n");
        }
        result.push('\n');
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_context_creation() {
        let ctx = Ue5Context::new("MyPlugin", None);
        assert_eq!(ctx.module_api, "MYPLUGIN_API");
        assert_eq!(ctx.output_name, "MyPlugin");
    }
    
    #[test]
    fn test_type_registration() {
        let mut ctx = Ue5Context::new("Test", None);
        
        ctx.register_enum("Direction".to_string(), "Direction.h".to_string());
        ctx.register_struct("Point".to_string(), "Point.h".to_string());
        ctx.register_component("Health".to_string(), "Health.h".to_string());
        ctx.register_delegate("OnDamage".to_string(), "OnDamage.h".to_string());
        ctx.register_actor("Player".to_string(), "Player.h".to_string());
        
        assert!(ctx.is_enum("Direction"));
        assert!(ctx.is_struct("Point"));
        assert!(ctx.is_component("Health"));
        assert!(ctx.is_struct("Health")); // Components are also structs
        assert!(ctx.is_delegate("OnDamage"));
        assert!(ctx.is_actor("Player"));
    }
    
    #[test]
    fn test_ident_remapping() {
        let mut ctx = Ue5Context::new("Test", None);
        
        ctx.add_ident_remap("delta_time".to_string(), "DeltaTime".to_string());
        
        assert_eq!(ctx.remap_ident("delta_time"), "DeltaTime");
        assert_eq!(ctx.remap_ident("other"), "other");
        
        ctx.clear_ident_remaps();
        assert_eq!(ctx.remap_ident("delta_time"), "delta_time");
    }

    #[test]
    fn test_trait_tracking() {
        let mut ctx = Ue5Context::new("Test", None);
        
        ctx.register_trait_impl("Player", "Damageable");
        ctx.register_trait_impl("Player", "Simulatable");
        
        let interface_list = ctx.get_interface_list("Player");
        assert!(interface_list.contains("IDamageable"));
        assert!(interface_list.contains("ISimulatable"));
    }
}

impl Ue5Context {
    /// Register that a class implements a trait
    pub fn register_trait_impl(&mut self, class_name: &str, trait_name: &str) {
        self.trait_impls
            .entry(class_name.to_string())
            .or_default()
            .push(trait_name.to_string());
    }

    /// Get the interface inheritance list for a class
    /// Returns a string like ", public IDamageable, public ISimulatable"
    pub fn get_interface_list(&self, class_name: &str) -> String {
        match self.trait_impls.get(class_name) {
            Some(traits) => traits.iter()
                .map(|t| format!(", public I{}", t))
                .collect::<Vec<_>>()
                .join(""),
            None => String::new(),
        }
    }
}
