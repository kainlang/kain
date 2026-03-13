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
static NAMING_POLICY: OnceLock<NamingPolicy> = OnceLock::new();

/// Data-driven naming policy loaded from environment with safe defaults.
#[derive(Debug, Clone)]
struct NamingPolicy {
    collision_prefix: String,
}

impl Default for NamingPolicy {
    fn default() -> Self {
        let collision_prefix = std::env::var("KAIN_UE5_NAME_PREFIX")
            .ok()
            .map(|value| value.trim().to_string())
            .filter(|value| !value.is_empty())
            .unwrap_or_else(|| "Kain".to_string());
        Self { collision_prefix }
    }
}

fn naming_policy() -> &'static NamingPolicy {
    NAMING_POLICY.get_or_init(NamingPolicy::default)
}

/// Load the reserved engine names set (all categories merged into one HashSet).
fn reserved_engine_names() -> &'static HashSet<String> {
    RESERVED_NAMES.get_or_init(|| {
        let mut set = HashSet::new();

        let reserved_relative = std::path::Path::new("unreal")
            .join("metadata")
            .join("reserved_engine_names.json");
        if let Some(path) = find_metadata_file(&reserved_relative) {
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

        let knowledge_relative = std::path::Path::new("unreal")
            .join("metadata")
            .join("engine_knowledge.json");
        if let Some(path) = find_metadata_file(&knowledge_relative) {
            if let Ok(data) = std::fs::read_to_string(&path) {
                if let Ok(json) = serde_json::from_str::<serde_json::Value>(&data) {
                    extend_names_from_engine_knowledge(&mut set, &json);
                }
            }
        }

        set
    })
}

fn find_metadata_file(relative: &std::path::Path) -> Option<std::path::PathBuf> {
    let from_env = std::env::var("KAIN_ROOT")
        .ok()
        .map(|root| std::path::PathBuf::from(root).join(relative))
        .filter(|p| p.exists());

    let from_walk = {
        let mut found = None;
        if let Ok(mut dir) = std::env::current_dir() {
            for _ in 0..10 {
                let candidate = dir.join(relative);
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

    from_env.or(from_walk)
}

fn extend_names_from_engine_knowledge(set: &mut HashSet<String>, json: &serde_json::Value) {
    for key in ["classes", "structs", "enums"] {
        let Some(entries) = json.get(key).and_then(|value| value.as_array()) else {
            continue;
        };
        for entry in entries {
            if let Some(name) = entry.get("name").and_then(|value| value.as_str()) {
                set.insert(strip_any_ue_prefix(name).to_string());
            }
        }
    }

    if let Some(entries) = json.get("type_aliases").and_then(|value| value.as_array()) {
        for entry in entries {
            if let Some(name) = entry.get("ue5_name").and_then(|value| value.as_str()) {
                set.insert(strip_any_ue_prefix(name).to_string());
            }
        }
    }
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

fn strip_prefixed_engine_name<'a>(name: &'a str, prefix: char) -> &'a str {
    if name.starts_with(prefix) && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        &name[1..]
    } else {
        name
    }
}

fn strip_any_ue_prefix(name: &str) -> &str {
    if let Some(first) = name.chars().next() {
        if matches!(first, 'A' | 'U' | 'F' | 'E' | 'I')
            && name.chars().nth(1).map_or(false, |c| c.is_uppercase())
        {
            return &name[1..];
        }
    }
    name
}

fn remap_engine_name(engine_name: &str) -> String {
    let reserved = reserved_engine_names();
    if !reserved.contains(engine_name) {
        return engine_name.to_string();
    }

    let prefix = &naming_policy().collision_prefix;
    let mut candidate = format!("{}{}", prefix, engine_name);
    let mut counter = 2usize;
    while reserved.contains(&candidate) {
        candidate = format!("{}{}{}", prefix, engine_name, counter);
        counter += 1;
    }
    candidate
}

fn format_prefixed_name(name: &str, prefix: char, check_collision: bool) -> KainResult<String> {
    validate_identifier(name)?;
    let engine_name = strip_prefixed_engine_name(name, prefix);
    let resolved_engine_name = if check_collision {
        remap_engine_name(engine_name)
    } else {
        engine_name.to_string()
    };
    validate_identifier(&resolved_engine_name)?;
    Ok(format!("{}{}", prefix, resolved_engine_name))
}

/// C++ keywords that cannot be used as identifiers
const CPP_KEYWORDS: &[&str] = &[
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
];

/// UE5 macro names that cannot be used as identifiers
const UE5_MACROS: &[&str] = &[
    "UCLASS",
    "USTRUCT",
    "UENUM",
    "UFUNCTION",
    "UPROPERTY",
    "UMETA",
    "GENERATED_BODY",
    "GENERATED_USTRUCT_BODY",
    "GENERATED_UCLASS_BODY",
    "UPARAM",
    "UDELEGATE",
    "TEXT",
    "LOCTEXT",
    "NSLOCTEXT",
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
    format_prefixed_name(name, 'A', true)
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
    format_prefixed_name(name, 'F', true)
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
    format_prefixed_name(name, 'E', true)
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
    format_prefixed_name(name, 'U', true)
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
    let raw_name =
        if name.starts_with('U') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
            strip_prefixed_engine_name(name, 'U').to_string()
        } else {
            name.to_string()
        };
    let subsystem_base = if raw_name.ends_with("Subsystem") {
        raw_name
    } else {
        format!("{}Subsystem", raw_name)
    };
    let resolved = remap_engine_name(&subsystem_base);
    format!("U{}", resolved)
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
        assert_eq!(to_actor_name("SampleActor"), "ASampleActor");
        assert_eq!(to_actor_name("ASampleActor"), "ASampleActor");
        assert!(to_actor_name("GameMode").starts_with('A'));
    }

    #[test]
    fn test_struct_naming() {
        assert_eq!(to_struct_name("SampleStruct"), "FSampleStruct");
        assert_eq!(to_struct_name("FSampleStruct"), "FSampleStruct");
    }

    #[test]
    fn test_reserved_struct_collision_is_remapped() {
        let result = to_struct_name("Color");
        assert_ne!(result, "FColor");
        assert!(result.starts_with('F'));
        assert!(result.contains("Color"));
    }

    #[test]
    fn test_prefixed_reserved_struct_collision_is_remapped() {
        let result = to_struct_name("FColor");
        assert_ne!(result, "FColor");
        assert!(result.starts_with('F'));
        assert!(result.contains("Color"));
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
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("cannot start with a number"));
    }

    #[test]
    fn test_invalid_identifier_special_chars() {
        let result = to_struct_name_checked("My-Struct");
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("special character"));

        let result2 = to_enum_name_checked("My@Enum");
        assert!(result2.is_err());
        assert!(result2
            .unwrap_err()
            .to_string()
            .contains("special character"));
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
