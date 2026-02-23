//! UE5 Type Mapping
//! 
//! The Source of Truth for how KAIN types map to C++ types.
//! Handles complex logic for TArray<T>, TMap<K,V>, Smart Pointers, etc.
//! 
//! Now data-driven via EngineKnowledge instead of hardcoded type lists!

use kain_core::ast::Type;
use super::naming::*;
use super::engine_knowledge::EngineKnowledge;
use std::collections::HashSet;

/// Configuration for type mapping behavior
#[derive(Debug, Clone)]
pub struct TypeMapConfig {
    /// Use float precision (FVector3f) vs double (FVector)
    pub use_float_precision: bool,
    /// Known enum names for proper prefixing
    pub enum_names: HashSet<String>,
    /// Known struct names for proper prefixing
    pub struct_names: HashSet<String>,
    /// Known component names for pointer generation
    pub component_names: HashSet<String>,
    /// Known delegate names for proper prefixing
    pub delegate_names: HashSet<String>,
}

impl Default for TypeMapConfig {
    fn default() -> Self {
        Self {
            use_float_precision: false, // Default to UE5 LWC-friendly FVector/FVector2D/FVector4
            enum_names: HashSet::new(),
            struct_names: HashSet::new(),
            component_names: HashSet::new(),
            delegate_names: HashSet::new(),
        }
    }
}

/// Map KAIN type to UE5 C++ type
/// Now queries EngineKnowledge for engine types instead of hardcoded lists!
pub fn map_type(ty: &Type, config: &TypeMapConfig) -> String {
    map_type_with_knowledge(ty, config, None)
}

/// Map KAIN type to UE5 C++ type with optional EngineKnowledge
/// This is the new data-driven version that eliminates hardcoded type lists
pub fn map_type_with_knowledge(ty: &Type, config: &TypeMapConfig, kb: Option<&EngineKnowledge>) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            // First, check primitives and built-in types (these are always the same)
            let ue_name = match name.as_str() {
                // Primitives
                "Int" => "int64",
                "Float" => "float",
                "Bool" => "bool",
                "String" => "FString",
                "Name" => "FName",
                "Text" => "FText",
                "Unit" | "()" => "void",
                
                // Vectors - configurable precision
                "Vec2" => if config.use_float_precision { "FVector2f" } else { "FVector2D" },
                "Vec3" => if config.use_float_precision { "FVector3f" } else { "FVector" },
                "Vec4" => if config.use_float_precision { "FVector4f" } else { "FVector4" },
                
                // Explicit precision vectors
                "DVec2" => "FVector2D",
                "DVec3" => "FVector",
                "DVec4" => "FVector4",
                
                // Rotations
                "Quat" => "FQuat",
                "Rotation" => "FRotator",
                
                // Containers
                "Array" => "TArray",
                "Map" => "TMap",
                "Set" => "TSet",
                "Option" => "TOptional",
                
                // Smart pointers
                "SharedPtr" => "TSharedPtr",
                "WeakPtr" => "TWeakPtr",
                "UniquePtr" => "TUniquePtr",
                "SoftObjectPtr" => "TSoftObjectPtr",
                "SubclassOf" => "TSubclassOf",
                
                // Generic object references (fallback)
                "Actor" => "AActor*",
                "Object" => "UObject*",
                "Component" => "UActorComponent*",
                "Class" => "TSubclassOf<UObject>",
                
                // Everything else - query EngineKnowledge or user-defined types
                _ => {
                    // Try EngineKnowledge first (data-driven!)
                    if let Some(knowledge) = kb {
                        // Check if it's a type alias (Vec3 -> FVector, Transform -> FTransform, etc.)
                        if let Some(alias) = knowledge.resolve_type_alias(name) {
                            return alias.to_string();
                        }
                        
                        // Check if it's a known engine type with automatic C++ mapping
                        if let Some(cpp_type) = knowledge.get_cpp_type(name) {
                            return cpp_type;
                        }
                    }
                    
                    // Fallback to user-defined types
                    // Check if it's a known delegate
                    if config.delegate_names.contains(name) {
                        return format!("F{}", name);
                    }
                    // Check if it's a known component - return as pointer
                    if config.component_names.contains(name) {
                        return format!("U{}*", name);
                    }
                    // Check if it's a known enum
                    if config.enum_names.contains(name) {
                        return to_enum_name(name);
                    }
                    // Check if it's a known struct
                    if config.struct_names.contains(name) {
                        return to_struct_name(name);
                    }
                    // Unknown type - return as-is
                    name
                }
            };

            if generics.is_empty() {
                ue_name.to_string()
            } else {
                let gen_strs: Vec<String> = generics.iter()
                    .map(|g| map_type_with_knowledge(g, config, kb))
                    .collect();
                format!("{}<{}>", ue_name, gen_strs.join(", "))
            }
        }
        Type::Tuple(types, _) => {
            let type_strs: Vec<String> = types.iter()
                .map(|t| map_type_with_knowledge(t, config, kb))
                .collect();
            format!("TTuple<{}>", type_strs.join(", "))
        }
        Type::Array(inner, size, _) => {
            format!("TStaticArray<{}, {}>", map_type_with_knowledge(inner, config, kb), size)
        }
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("{}&", map_type_with_knowledge(inner, config, kb))
            } else {
                format!("const {}&", map_type_with_knowledge(inner, config, kb))
            }
        }
        Type::Function { params, return_type, .. } => {
            let param_strs: Vec<String> = params.iter()
                .map(|p| map_type_with_knowledge(p, config, kb))
                .collect();
            format!("TFunction<{}({})>", 
                map_type_with_knowledge(return_type, config, kb), 
                param_strs.join(", "))
        }
        Type::Option(inner, _) => {
            format!("TOptional<{}>", map_type_with_knowledge(inner, config, kb))
        }
        Type::Infer(_) => "auto".to_string(),
        Type::Never(_) => "void".to_string(),
        Type::Unit(_) => "void".to_string(),
        _ => "auto".to_string(),
    }
}

/// Get default value for a type (used for shader uniforms, etc.)
pub fn get_default_value(ty: &Type, config: &TypeMapConfig) -> String {
    match ty {
        Type::Named { name, .. } => {
            match name.as_str() {
                "Int" => "0".to_string(),
                "Float" => "0.0f".to_string(),
                "Bool" => "false".to_string(),
                "Vec2" => if config.use_float_precision { 
                    "FVector2f(0.0f, 0.0f)".to_string() 
                } else { 
                    "FVector2D(0.0, 0.0)".to_string() 
                },
                "Vec3" => if config.use_float_precision { 
                    "FVector3f(0.0f, 0.0f, 0.0f)".to_string() 
                } else { 
                    "FVector(0.0, 0.0, 0.0)".to_string() 
                },
                "Vec4" => if config.use_float_precision { 
                    "FVector4f(0.0f, 0.0f, 0.0f, 0.0f)".to_string() 
                } else { 
                    "FVector4(0.0, 0.0, 0.0, 0.0)".to_string() 
                },
                _ => "{}".to_string(),
            }
        }
        _ => "{}".to_string(),
    }
}

/// Result of type mapping with additional metadata
#[derive(Debug, Clone)]
pub struct MappedType {
    /// The C++ type string
    pub cpp_type: String,
    /// Whether this type should be a pointer
    pub is_pointer: bool,
    /// Whether this type needs a forward declaration
    pub needs_forward_decl: bool,
    /// Include path for this type (if known)
    pub include_path: Option<String>,
    /// UE5 prefix (A, F, E, U, S) if applicable
    pub prefix: Option<String>,
}

/// Type registry for tracking user-defined types
#[derive(Debug, Clone, Default)]
pub struct TypeRegistry {
    enums: HashSet<String>,
    structs: HashSet<String>,
    actors: HashSet<String>,
    components: HashSet<String>,
    subsystems: HashSet<String>,
    delegates: HashSet<String>,
}

impl TypeRegistry {
    pub fn new() -> Self {
        Self::default()
    }
    
    pub fn register_enum(&mut self, name: String) {
        self.enums.insert(name);
    }
    
    pub fn register_struct(&mut self, name: String) {
        self.structs.insert(name);
    }
    
    pub fn register_actor(&mut self, name: String) {
        self.actors.insert(name);
    }
    
    pub fn register_component(&mut self, name: String) {
        self.components.insert(name);
    }
    
    pub fn register_subsystem(&mut self, name: String) {
        self.subsystems.insert(name);
    }
    
    pub fn register_delegate(&mut self, name: String) {
        self.delegates.insert(name);
    }
    
    pub fn is_enum(&self, name: &str) -> bool {
        self.enums.contains(name)
    }
    
    pub fn is_struct(&self, name: &str) -> bool {
        self.structs.contains(name)
    }
    
    pub fn is_actor(&self, name: &str) -> bool {
        self.actors.contains(name)
    }
    
    pub fn is_component(&self, name: &str) -> bool {
        self.components.contains(name)
    }
    
    pub fn is_subsystem(&self, name: &str) -> bool {
        self.subsystems.contains(name)
    }
    
    pub fn is_delegate(&self, name: &str) -> bool {
        self.delegates.contains(name)
    }
}

/// Centralized type mapper - single source of truth for type mapping
pub struct TypeMapper {
    config: TypeMapConfig,
    knowledge: Option<EngineKnowledge>,
    registry: TypeRegistry,
}

impl TypeMapper {
    /// Create a new TypeMapper with default configuration
    pub fn new() -> Self {
        Self {
            config: TypeMapConfig::default(),
            knowledge: None,
            registry: TypeRegistry::new(),
        }
    }
    
    /// Create a new TypeMapper with EngineKnowledge
    pub fn with_knowledge(knowledge: EngineKnowledge) -> Self {
        Self {
            config: TypeMapConfig::default(),
            knowledge: Some(knowledge),
            registry: TypeRegistry::new(),
        }
    }
    
    /// Create a new TypeMapper with custom configuration
    pub fn with_config(config: TypeMapConfig) -> Self {
        Self {
            config,
            knowledge: None,
            registry: TypeRegistry::new(),
        }
    }
    
    /// Register an enum type
    pub fn register_enum(&mut self, name: String) {
        self.registry.register_enum(name.clone());
        self.config.enum_names.insert(name);
    }
    
    /// Register a struct type
    pub fn register_struct(&mut self, name: String) {
        self.registry.register_struct(name.clone());
        self.config.struct_names.insert(name);
    }
    
    /// Register an actor type
    pub fn register_actor(&mut self, name: String) {
        self.registry.register_actor(name);
    }
    
    /// Register a component type
    pub fn register_component(&mut self, name: String) {
        self.registry.register_component(name.clone());
        self.config.component_names.insert(name);
    }
    
    /// Register a subsystem type
    pub fn register_subsystem(&mut self, name: String) {
        self.registry.register_subsystem(name.clone());
        // Subsystems are UObject-derived classes (U*Subsystem) and should map as pointers.
        self.config.component_names.insert(name);
    }
    
    /// Register a delegate type
    pub fn register_delegate(&mut self, name: String) {
        self.registry.register_delegate(name.clone());
        self.config.delegate_names.insert(name);
    }
    
    /// Map a KAIN type to UE5 C++ with full metadata
    /// This is the centralized type mapping method that prevents double-prefixing
    pub fn map_type(&self, ty: &Type) -> MappedType {
        let cpp_type = self.map_type_string(ty);
        let is_pointer = self.is_pointer_type(ty);
        let needs_forward_decl = self.needs_forward_decl(ty);
        let include_path = self.get_include_path(ty);
        let prefix = self.get_prefix(ty);
        
        MappedType {
            cpp_type,
            is_pointer,
            needs_forward_decl,
            include_path,
            prefix,
        }
    }
    
    /// Map a KAIN type to UE5 C++ type string with prefix detection
    /// This method implements the core type mapping logic with double-prefix prevention
    pub fn map_type_string(&self, ty: &Type) -> String {
        // Debug output at the very start
        use std::io::Write;
        if let Ok(mut file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("C:/temp/kain_type_debug.txt") {
            let _ = writeln!(file, "[DEBUG map_type_string] Called with type: {:?}", ty);
        }
        
        match ty {
            Type::Named { name, generics, .. } => {
                // First, check primitives and built-in types (these are always the same)
                let ue_name = match name.as_str() {
                    // Primitives
                    "Int" => return "int64".to_string(),
                    "Float" => return "float".to_string(),
                    "Bool" => return "bool".to_string(),
                    "String" => return "FString".to_string(),
                    "Name" => return "FName".to_string(),
                    "Text" => return "FText".to_string(),
                    "Unit" | "()" => return "void".to_string(),
                    
                    // Vectors - configurable precision
                    "Vec2" => return if self.config.use_float_precision { "FVector2f".to_string() } else { "FVector2D".to_string() },
                    "Vec3" => return if self.config.use_float_precision { "FVector3f".to_string() } else { "FVector".to_string() },
                    "Vec4" => return if self.config.use_float_precision { "FVector4f".to_string() } else { "FVector4".to_string() },
                    
                    // Explicit precision vectors
                    "DVec2" => return "FVector2D".to_string(),
                    "DVec3" => return "FVector".to_string(),
                    "DVec4" => return "FVector4".to_string(),
                    
                    // Rotations
                    "Quat" => return "FQuat".to_string(),
                    "Rotation" => return "FRotator".to_string(),
                    
                    // Containers
                    "Array" => "TArray",
                    "Map" => "TMap",
                    "Set" => "TSet",
                    "Option" => "TOptional",
                    
                    // Smart pointers
                    "SharedPtr" => "TSharedPtr",
                    "WeakPtr" => "TWeakPtr",
                    "UniquePtr" => "TUniquePtr",
                    "SoftObjectPtr" => "TSoftObjectPtr",
                    "SubclassOf" => "TSubclassOf",
                    
                    // Generic object references (fallback)
                    "Actor" => return "AActor*".to_string(),
                    "Object" => return "UObject*".to_string(),
                    "Component" => return "UActorComponent*".to_string(),
                    "Class" => return "TSubclassOf<UObject>".to_string(),
                    
                    // Hardcoded fixes for common UObject types that are missing pointers
                    "AnimSequence" => return "UAnimSequence*".to_string(),
                    "AnimMontage" => return "UAnimMontage*".to_string(),
                    "SkeletalMesh" => return "USkeletalMesh*".to_string(),
                    "StaticMesh" => return "UStaticMesh*".to_string(),
                    
                    // Everything else - query EngineKnowledge or user-defined types
                    _ => {
                        // Debug to file
                        use std::io::Write;
                        if let Ok(mut file) = std::fs::OpenOptions::new()
                            .create(true)
                            .append(true)
                            .open("C:/temp/kain_debug.txt") {
                            let _ = writeln!(file, "[DEBUG TypeMapper] Looking up type: {}", name);
                        
                            // Try EngineKnowledge first (data-driven!)
                            if let Some(knowledge) = &self.knowledge {
                                let _ = writeln!(file, "[DEBUG TypeMapper] EngineKnowledge available for: {}", name);
                                // Check if it's a known engine type with automatic C++ mapping
                                // This handles both type aliases and direct lookups with pointer detection
                                if let Some(cpp_type) = knowledge.get_cpp_type(name) {
                                    let _ = writeln!(file, "[DEBUG TypeMapper] Got cpp_type: {} -> {}", name, cpp_type);
                                    return cpp_type;
                                }
                                let _ = writeln!(file, "[DEBUG TypeMapper] No cpp_type from knowledge for: {}", name);
                            } else {
                                let _ = writeln!(file, "[DEBUG TypeMapper] EngineKnowledge NOT available!");
                            }
                        }
                        
                        // Fallback to user-defined types with prefix detection
                        // If debug file fails, continue with normal logic
                        if let Some(knowledge) = &self.knowledge {
                            if let Some(cpp_type) = knowledge.get_cpp_type(name) {
                                return cpp_type;
                            }
                        }
                        return self.apply_prefix_with_detection(name);
                    }
                };

                // Handle generics
                if generics.is_empty() {
                    ue_name.to_string()
                } else {
                    let gen_strs: Vec<String> = generics.iter()
                        .map(|g| self.map_type_string(g))
                        .collect();
                    format!("{}<{}>", ue_name, gen_strs.join(", "))
                }
            }
            Type::Tuple(types, _) => {
                let type_strs: Vec<String> = types.iter()
                    .map(|t| self.map_type_string(t))
                    .collect();
                format!("TTuple<{}>", type_strs.join(", "))
            }
            Type::Array(inner, size, _) => {
                format!("TStaticArray<{}, {}>", self.map_type_string(inner), size)
            }
            Type::Ref { mutable, inner, .. } => {
                if *mutable {
                    format!("{}&", self.map_type_string(inner))
                } else {
                    format!("const {}&", self.map_type_string(inner))
                }
            }
            Type::Function { params, return_type, .. } => {
                let param_strs: Vec<String> = params.iter()
                    .map(|p| self.map_type_string(p))
                    .collect();
                format!("TFunction<{}({})>", 
                    self.map_type_string(return_type), 
                    param_strs.join(", "))
            }
            Type::Option(inner, _) => {
                format!("TOptional<{}>", self.map_type_string(inner))
            }
            Type::Infer(_) => "auto".to_string(),
            Type::Never(_) => "void".to_string(),
            Type::Unit(_) => "void".to_string(),
            _ => "auto".to_string(),
        }
    }
    
    /// Apply UE5 prefix to a type name with detection to prevent double-prefixing
    /// This is the key method that solves the EEHealthStatus bug
    fn apply_prefix_with_detection(&self, name: &str) -> String {
        // Check if name already has a UE5 prefix
        if name.len() >= 2 {
            let first_char = name.chars().next().unwrap();
            let second_char = name.chars().nth(1).unwrap();
            
            // If it starts with A/F/E/U/S followed by uppercase, it's already prefixed
            if matches!(first_char, 'A' | 'F' | 'E' | 'U' | 'S') && second_char.is_uppercase() {
                // Already prefixed - return as-is
                return name.to_string();
            }
        }
        
        // Not prefixed - apply appropriate prefix based on type registry
        if self.registry.is_delegate(name) {
            format!("F{}", name)
        } else if self.registry.is_subsystem(name) {
            // Subsystems are UObject-derived classes and should keep explicit subsystem suffix.
            format!("{}*", to_subsystem_name(name))
        } else if self.registry.is_component(name) {
            // Components get U prefix only (no automatic Component suffix)
            // User-defined components keep their original names
            format!("U{}*", name)
        } else if self.registry.is_enum(name) {
            to_enum_name(name)
        } else if self.registry.is_struct(name) {
            to_struct_name(name)
        } else if self.registry.is_actor(name) {
            format!("{}*", to_actor_name(name))
        } else if name.ends_with("Graph") {
            // Runtime graph defs are generated as UObject instance classes.
            // Allow user-authored fields like `DialogueGraph` to bind to
            // `UDialogueGraphInstance*` without requiring source-level renames.
            format!("U{}Instance*", name)
        } else if self.is_pointer_type_by_name(name) {
            // Check if it's a known UObject-derived type that needs a pointer
            // This handles engine types like AnimSequence that aren't in the registry
            format!("U{}*", name)
        } else {
            // Unknown type - return as-is
            name.to_string()
        }
    }
    
    /// Check if a type should be a pointer
    /// This method queries EngineKnowledge for UObject-derived types
    pub fn is_pointer_type(&self, ty: &Type) -> bool {
        match ty {
            Type::Named { name, .. } => {
                self.is_pointer_type_by_name(name)
            }
            _ => false,
        }
    }
    
    /// Check if a type name should be a pointer
    /// Centralized logic for pointer type detection
    pub fn is_pointer_type_by_name(&self, name: &str) -> bool {
        // Check if it's a known UObject-derived type from EngineKnowledge
        if let Some(knowledge) = &self.knowledge {
            if knowledge.is_uobject_derived(name) {
                return true;
            }
        }
        
        // Check user-defined types
        if self.registry.is_actor(name) || self.registry.is_component(name) || self.registry.is_subsystem(name) {
            return true;
        }
        
        // Check for common UObject-derived types (fallback if EngineKnowledge is incomplete)
        // This list should be minimal - most types should come from EngineKnowledge
        matches!(name, 
            "AActor" | "APawn" | "ACharacter" | "APlayerController" | "AGameMode" |
            "UObject" | "UActorComponent" | "USceneComponent" | "UStaticMeshComponent" |
            "USkeletalMeshComponent" | "UPrimitiveComponent" | "UMeshComponent" |
            "UWorld" | "ULevel" | "UGameInstance" | "ULocalPlayer" |
            "UTexture" | "UTexture2D" | "UMaterial" | "UMaterialInstance" |
            "UStaticMesh" | "USkeletalMesh" | "UAnimSequence" | "UAnimMontage" |
            "USoundBase" | "USoundWave" | "USoundCue" |
            "UParticleSystem" | "UParticleSystemComponent" |
            "UWidget" | "UUserWidget" | "UWidgetComponent" |
            // Add AnimSequence without U prefix as fallback
            "AnimSequence"
        )
    }
    
    /// Check if a type needs a forward declaration
    pub fn needs_forward_decl(&self, ty: &Type) -> bool {
        match ty {
            Type::Named { name, .. } => {
                // User-defined types need forward declarations
                self.registry.is_actor(name) 
                    || self.registry.is_component(name)
                    || self.registry.is_subsystem(name)
                    || self.registry.is_struct(name)
            }
            _ => false,
        }
    }
    
    /// Get the include path for a type
    pub fn get_include_path(&self, ty: &Type) -> Option<String> {
        match ty {
            Type::Named { name, .. } => {
                // Query EngineKnowledge for include path
                if let Some(knowledge) = &self.knowledge {
                    return knowledge.get_include(name).map(|s| s.to_string());
                }
                None
            }
            _ => None,
        }
    }
    
    /// Get the UE5 prefix for a type (A, F, E, U, S)
    fn get_prefix(&self, ty: &Type) -> Option<String> {
        match ty {
            Type::Named { name, .. } => {
                if self.registry.is_actor(name) {
                    Some("A".to_string())
                } else if self.registry.is_struct(name) {
                    Some("F".to_string())
                } else if self.registry.is_enum(name) {
                    Some("E".to_string())
                } else if self.registry.is_component(name) {
                    Some("U".to_string())
                } else if self.registry.is_subsystem(name) {
                    Some("U".to_string())
                } else if self.registry.is_delegate(name) {
                    Some("F".to_string())
                } else {
                    None
                }
            }
            _ => None,
        }
    }
}

impl Default for TypeMapper {
    fn default() -> Self {
        Self::new()
    }
}
