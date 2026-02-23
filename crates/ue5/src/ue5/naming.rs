//! UE5 Naming Conventions
//! 
//! The Authority on Naming - centralizes all naming transformations.
//! If we change how a class is named, it changes everywhere automatically.
//! 
//! Also validates against UE5 reserved engine names loaded from
//! `unreal/metadata/reserved_engine_names.json` (data-driven!).

use kain_core::error::{KainError, KainResult};
use std::collections::HashSet;
use std::sync::OnceLock;

/// Data-driven reserved engine names — loaded once from JSON.
/// UHT rejects types whose "engine name" (name without A/U/F/E/I prefix)
/// matches a built-in engine class, interface, struct, or enum.
#[derive(Debug, Default, serde::Deserialize)]
struct ReservedEngineNames {
    #[serde(default)]
    reserved_component_names: Vec<String>,
    #[serde(default)]
    reserved_actor_names: Vec<String>,
    #[serde(default)]
    reserved_struct_names: Vec<String>,
    #[serde(default)]
    reserved_enum_names: Vec<String>,
    #[serde(default)]
    reserved_interface_names: Vec<String>,
}

/// Global cache — loaded exactly once from the JSON metadata file.
static RESERVED_NAMES: OnceLock<HashSet<String>> = OnceLock::new();

/// Load the reserved engine names set (all categories merged into one HashSet).
fn reserved_engine_names() -> &'static HashSet<String> {
    RESERVED_NAMES.get_or_init(|| {
        let mut set = HashSet::new();

        // Search for the JSON file using the same strategy as other metadata files
        let relative = std::path::Path::new("unreal")
            .join("metadata")
            .join("reserved_engine_names.json");

        // 1. KAIN_ROOT env var
        let from_env = std::env::var("KAIN_ROOT")
            .ok()
            .map(|root| std::path::PathBuf::from(root).join(&relative))
            .filter(|p| p.exists());

        // 2. Walk up from CWD
        let from_walk = {
            let mut found = None;
            if let Ok(mut dir) = std::env::current_dir() {
                for _ in 0..10 {
                    let candidate = dir.join(&relative);
                    if candidate.exists() {
                        found = Some(candidate);
                        break;
                    }
                    match dir.parent() {
                        Some(p) => dir = p.to_path_buf(),
                        None => break,
                    }
                }
            }
            found
        };

        let path = from_env.or(from_walk);

        if let Some(path) = path {
            if let Ok(data) = std::fs::read_to_string(&path) {
                if let Ok(names) = serde_json::from_str::<ReservedEngineNames>(&data) {
                    set.extend(names.reserved_component_names);
                    set.extend(names.reserved_actor_names);
                    set.extend(names.reserved_struct_names);
                    set.extend(names.reserved_enum_names);
                    set.extend(names.reserved_interface_names);
                }
            }
        }

        set
    })
}

/// Check if a name (engine name, i.e. without prefix) clashes with a UE5 built-in.
/// Returns Err with a helpful message if it does.
pub fn check_engine_name_collision(engine_name: &str, kain_name: &str) -> KainResult<()> {
    let reserved = reserved_engine_names();
    if reserved.contains(engine_name) {
        Err(KainError::validation_error(format!(
            "Name '{}' (engine name '{}') collides with a built-in UE5 type. \
             UHT will reject this. Consider renaming to 'Kain{}' or '{}_Custom'.",
            kain_name, engine_name, kain_name, kain_name
        )))
    } else {
        Ok(())
    }
}

/// C++ keywords that cannot be used as identifiers
const CPP_KEYWORDS: &[&str] = &[
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
    "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
    "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
    "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline",
    "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
    "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return", "short", "signed",
    "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
    "template", "this", "thread_local", "throw", "true", "try", "typedef",
    "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
    "volatile", "wchar_t", "while", "xor", "xor_eq",
];

/// UE5 macro names that cannot be used as identifiers
const UE5_MACROS: &[&str] = &[
    "UCLASS", "USTRUCT", "UENUM", "UFUNCTION", "UPROPERTY", "UMETA",
    "GENERATED_BODY", "GENERATED_USTRUCT_BODY", "GENERATED_UCLASS_BODY",
    "UPARAM", "UDELEGATE", "TEXT", "LOCTEXT", "NSLOCTEXT",
];

/// Validate that a name is a valid C++ identifier
/// Returns error if name starts with number or contains special characters
fn validate_identifier(name: &str) -> KainResult<()> {
    if name.is_empty() {
        return Err(KainError::validation_error("Identifier cannot be empty"));
    }
    
    // Check if starts with number
    if name.chars().next().unwrap().is_numeric() {
        return Err(KainError::validation_error(format!(
            "Invalid identifier '{}': cannot start with a number. Use a letter or underscore instead.",
            name
        )));
    }
    
    // Check for special characters (allow only alphanumeric and underscore)
    for (i, ch) in name.chars().enumerate() {
        if !ch.is_alphanumeric() && ch != '_' {
            return Err(KainError::validation_error(format!(
                "Invalid identifier '{}': contains special character '{}' at position {}. Only letters, numbers, and underscores are allowed.",
                name, ch, i
            )));
        }
    }
    
    // Check if it's a C++ keyword
    let lower = name.to_lowercase();
    if CPP_KEYWORDS.contains(&lower.as_str()) {
        return Err(KainError::validation_error(format!(
            "Invalid identifier '{}': this is a C++ keyword. Consider using a different name like 'My{}' or '{}Type'.",
            name, name, name
        )));
    }
    
    // Check if it's a UE5 macro name
    let upper = name.to_uppercase();
    if UE5_MACROS.contains(&upper.as_str()) {
        return Err(KainError::validation_error(format!(
            "Invalid identifier '{}': this is a UE5 macro name. Consider using a different name.",
            name
        )));
    }
    
    Ok(())
}

/// Actor names get 'A' prefix: Player -> APlayer
/// Returns error if name is invalid
pub fn to_actor_name_checked(name: &str) -> KainResult<String> {
    validate_identifier(name)?;
    check_engine_name_collision(name, name)?;
    
    if name.starts_with('A') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        Ok(name.to_string())
    } else {
        Ok(format!("A{}", name))
    }
}

/// Actor names get 'A' prefix: Player -> APlayer
/// Panics if name is invalid (use to_actor_name_checked for error handling)
pub fn to_actor_name(name: &str) -> String {
    to_actor_name_checked(name).unwrap_or_else(|e| {
        panic!("Invalid actor name '{}': {}", name, e);
    })
}

/// Struct names get 'F' prefix: Transform -> FTransform
/// Returns error if name is invalid
pub fn to_struct_name_checked(name: &str) -> KainResult<String> {
    validate_identifier(name)?;
    check_engine_name_collision(name, name)?;
    
    if name.starts_with('F') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        Ok(name.to_string())
    } else {
        Ok(format!("F{}", name))
    }
}

/// Struct names get 'F' prefix: Transform -> FTransform
/// Panics if name is invalid (use to_struct_name_checked for error handling)
pub fn to_struct_name(name: &str) -> String {
    to_struct_name_checked(name).unwrap_or_else(|e| {
        panic!("Invalid struct name '{}': {}", name, e);
    })
}

/// Enum names get 'E' prefix: Direction -> EDirection
/// Returns error if name is invalid
pub fn to_enum_name_checked(name: &str) -> KainResult<String> {
    validate_identifier(name)?;
    check_engine_name_collision(name, name)?;
    
    if name.starts_with('E') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        Ok(name.to_string())
    } else {
        Ok(format!("E{}", name))
    }
}

/// Enum names get 'E' prefix: Direction -> EDirection
/// Panics if name is invalid (use to_enum_name_checked for error handling)
pub fn to_enum_name(name: &str) -> String {
    to_enum_name_checked(name).unwrap_or_else(|e| {
        panic!("Invalid enum name '{}': {}", name, e);
    })
}

/// UObject names get 'U' prefix: Component -> UComponent
/// Returns error if name is invalid
pub fn to_uobject_name_checked(name: &str) -> KainResult<String> {
    validate_identifier(name)?;
    
    if name.starts_with('U') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        Ok(name.to_string())
    } else {
        Ok(format!("U{}", name))
    }
}

/// UObject names get 'U' prefix: Component -> UComponent
/// Panics if name is invalid (use to_uobject_name_checked for error handling)
pub fn to_uobject_name(name: &str) -> String {
    to_uobject_name_checked(name).unwrap_or_else(|e| {
        panic!("Invalid UObject name '{}': {}", name, e);
    })
}

/// Component names get 'U' prefix: Health -> UHealthComponent
/// Note: Components are UObjects, so they follow UObject naming
/// Returns error if name is invalid
pub fn to_component_name_checked(name: &str) -> KainResult<String> {
    validate_identifier(name)?;
    check_engine_name_collision(name, name)?;
    
    // If name already ends with "Component", just add U prefix
    if name.ends_with("Component") {
        return to_uobject_name_checked(name);
    }
    
    // Otherwise, add U prefix (without Component suffix for CreateDefaultSubobject)
    to_uobject_name_checked(name)
}

/// Component names get 'U' prefix: Health -> UHealthComponent
/// Note: Components are UObjects, so they follow UObject naming
/// Panics if name is invalid (use to_component_name_checked for error handling)
pub fn to_component_name(name: &str) -> String {
    to_component_name_checked(name).unwrap_or_else(|e| {
        panic!("Invalid component name '{}': {}", name, e);
    })
}

/// Subsystem names get 'U' prefix and 'Subsystem' suffix: TickOptimizer -> UTickOptimizerSubsystem
/// If name already ends with "Subsystem", just add U prefix.
pub fn to_subsystem_name(name: &str) -> String {
    if name.starts_with('U') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        if name.ends_with("Subsystem") {
            name.to_string()
        } else {
            format!("{}Subsystem", name)
        }
    } else if name.ends_with("Subsystem") {
        format!("U{}", name)
    } else {
        format!("U{}Subsystem", name)
    }
}

/// Generate module API macro from plugin name: "UltimateVFX" -> "ULTIMATEVFX_API"
pub fn to_module_api(plugin_name: &str) -> String {
    format!("{}_API", plugin_name.to_uppercase())
}

use heck::{ToPascalCase, ToSnakeCase};

/// Convert any case to PascalCase: "my_variable" -> "MyVariable", "GpuID" -> "GpuId"
/// Preserves numbers in correct position: "Player2" -> "Player2"
pub fn to_pascal_case(name: &str) -> String {
    name.to_pascal_case()
}

/// Convert any case to snake_case: "MyVariable" -> "my_variable"
/// Handles consecutive capitals correctly: "HTTPServer" -> "http_server"
/// Preserves numbers in correct position: "Player2" -> "player2"
pub fn to_snake_case(name: &str) -> String {
    // Use heck's implementation which handles consecutive capitals correctly
    name.to_snake_case()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_actor_naming() {
        assert_eq!(to_actor_name("Player"), "APlayer");
        assert_eq!(to_actor_name("APlayer"), "APlayer");
        assert_eq!(to_actor_name("GameMode"), "AGameMode");
    }

    #[test]
    fn test_struct_naming() {
        assert_eq!(to_struct_name("Vector"), "FVector");
        assert_eq!(to_struct_name("FVector"), "FVector");
    }

    #[test]
    fn test_enum_naming() {
        assert_eq!(to_enum_name("Direction"), "EDirection");
        assert_eq!(to_enum_name("EDirection"), "EDirection");
    }

    #[test]
    fn test_pascal_case() {
        assert_eq!(to_pascal_case("my_variable"), "MyVariable");
        assert_eq!(to_pascal_case("time_of_day"), "TimeOfDay");
        // Number preservation
        assert_eq!(to_pascal_case("player2"), "Player2");
        assert_eq!(to_pascal_case("item_3d"), "Item3d");
    }

    #[test]
    fn test_snake_case() {
        assert_eq!(to_snake_case("MyVariable"), "my_variable");
        assert_eq!(to_snake_case("TimeOfDay"), "time_of_day");
        // Consecutive capitals handling
        assert_eq!(to_snake_case("HTTPServer"), "http_server");
        assert_eq!(to_snake_case("XMLParser"), "xml_parser");
        assert_eq!(to_snake_case("IOStream"), "io_stream");
        // Number preservation
        assert_eq!(to_snake_case("Player2"), "player2");
        assert_eq!(to_snake_case("Item3D"), "item3_d");
    }

    #[test]
    fn test_invalid_identifier_starts_with_number() {
        let result = to_actor_name_checked("2Player");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("cannot start with a number"));
    }

    #[test]
    fn test_invalid_identifier_special_chars() {
        let result = to_struct_name_checked("My-Struct");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("special character"));
        
        let result2 = to_enum_name_checked("My@Enum");
        assert!(result2.is_err());
        assert!(result2.unwrap_err().to_string().contains("special character"));
    }

    #[test]
    fn test_invalid_identifier_cpp_keyword() {
        let result = to_actor_name_checked("class");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("C++ keyword"));
        
        let result2 = to_struct_name_checked("struct");
        assert!(result2.is_err());
        assert!(result2.unwrap_err().to_string().contains("C++ keyword"));
    }

    #[test]
    fn test_invalid_identifier_ue5_macro() {
        let result = to_actor_name_checked("UCLASS");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("UE5 macro"));
        
        let result2 = to_struct_name_checked("UPROPERTY");
        assert!(result2.is_err());
        assert!(result2.unwrap_err().to_string().contains("UE5 macro"));
    }

    #[test]
    fn test_valid_identifiers_with_numbers() {
        // Numbers in middle/end are valid
        assert_eq!(to_actor_name("Player2"), "APlayer2");
        assert_eq!(to_struct_name("Vec3"), "FVec3");
        assert_eq!(to_enum_name("Level10"), "ELevel10");
    }

    #[test]
    fn test_valid_identifiers_with_underscores() {
        assert_eq!(to_actor_name("health_component"), "Ahealth_component");
        assert_eq!(to_struct_name("item_data"), "Fitem_data");
    }
}
