//! UE5 Type Mapping
//! 
//! The Source of Truth for how KAIN types map to C++ types.
//! Handles complex logic for TArray<T>, TMap<K,V>, Smart Pointers, etc.

use kain_core::ast::Type;
use super::naming::*;
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
            use_float_precision: true, // Default to float for game data
            enum_names: HashSet::new(),
            struct_names: HashSet::new(),
            component_names: HashSet::new(),
            delegate_names: HashSet::new(),
        }
    }
}

/// Map KAIN type to UE5 C++ type
pub fn map_type(ty: &Type, config: &TypeMapConfig) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            let ue_name = match name.as_str() {
                // Primitives
                "Int" => "int64",
                "Float" => "float",
                "Bool" => "bool",
                "String" => "FString",
                "Name" => "FName",
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
                
                // Object references
                "Actor" => "AActor*",
                "Object" => "UObject*",
                "Component" => "UActorComponent*",
                "Class" => "TSubclassOf<UObject>",
                
                // Common UE5 types
                "Transform" => "FTransform",
                "AnimMontage" => "UAnimMontage*",
                "StaticMesh" => "UStaticMesh*",
                "SkeletalMesh" => "USkeletalMesh*",
                "Texture2D" => "UTexture2D*",
                "Material" => "UMaterial*",
                "MaterialInstance" => "UMaterialInstance*",
                "MaterialInstanceDynamic" => "UMaterialInstanceDynamic*",
                "StaticMeshComponent" => "UStaticMeshComponent*",
                "SplineComponent" => "USplineComponent*",
                "InstancedStaticMeshComponent" => "UInstancedStaticMeshComponent*",
                "SkeletalMeshComponent" => "USkeletalMeshComponent*",
                "AnimSequence" => "UAnimSequence*",
                "SoundBase" => "USoundBase*",
                "ParticleSystem" => "UParticleSystem*",
                "NiagaraSystem" => "UNiagaraSystem*",
                
                // User-defined types
                _ => {
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
                let gen_strs: Vec<String> = generics.iter().map(|g| map_type(g, config)).collect();
                format!("{}<{}>", ue_name, gen_strs.join(", "))
            }
        }
        Type::Tuple(types, _) => {
            let type_strs: Vec<String> = types.iter().map(|t| map_type(t, config)).collect();
            format!("TTuple<{}>", type_strs.join(", "))
        }
        Type::Array(inner, size, _) => {
            format!("TStaticArray<{}, {}>", map_type(inner, config), size)
        }
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("{}&", map_type(inner, config))
            } else {
                format!("const {}&", map_type(inner, config))
            }
        }
        Type::Function { params, return_type, .. } => {
            let param_strs: Vec<String> = params.iter().map(|p| map_type(p, config)).collect();
            format!("TFunction<{}({})>", map_type(return_type, config), param_strs.join(", "))
        }
        Type::Option(inner, _) => {
            format!("TOptional<{}>", map_type(inner, config))
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
