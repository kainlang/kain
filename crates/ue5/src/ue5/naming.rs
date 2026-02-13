//! UE5 Naming Conventions
//! 
//! The Authority on Naming - centralizes all naming transformations.
//! If we change how a class is named, it changes everywhere automatically.

/// Actor names get 'A' prefix: Player -> APlayer
pub fn to_actor_name(name: &str) -> String {
    if name.starts_with('A') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        name.to_string()
    } else {
        format!("A{}", name)
    }
}

/// Struct names get 'F' prefix: Transform -> FTransform
pub fn to_struct_name(name: &str) -> String {
    if name.starts_with('F') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        name.to_string()
    } else {
        format!("F{}", name)
    }
}

/// Enum names get 'E' prefix: Direction -> EDirection
pub fn to_enum_name(name: &str) -> String {
    if name.starts_with('E') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        name.to_string()
    } else {
        format!("E{}", name)
    }
}

/// UObject names get 'U' prefix: Component -> UComponent
pub fn to_uobject_name(name: &str) -> String {
    if name.starts_with('U') && name.chars().nth(1).map_or(false, |c| c.is_uppercase()) {
        name.to_string()
    } else {
        format!("U{}", name)
    }
}

/// Component names get 'U' prefix: Health -> UHealthComponent
/// Note: Components are UObjects, so they follow UObject naming
pub fn to_component_name(name: &str) -> String {
    // If name already ends with "Component", just add U prefix
    if name.ends_with("Component") {
        return to_uobject_name(name);
    }
    
    // Otherwise, add U prefix (without Component suffix for CreateDefaultSubobject)
    to_uobject_name(name)
}

/// Generate module API macro from plugin name: "UltimateVFX" -> "ULTIMATEVFX_API"
pub fn to_module_api(plugin_name: &str) -> String {
    format!("{}_API", plugin_name.to_uppercase())
}

use heck::{ToPascalCase, ToSnakeCase};

/// Convert any case to PascalCase: "my_variable" -> "MyVariable", "GpuID" -> "GpuId"
pub fn to_pascal_case(name: &str) -> String {
    name.to_pascal_case()
}

/// Convert any case to snake_case: "MyVariable" -> "my_variable"
pub fn to_snake_case(name: &str) -> String {
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
    }

    #[test]
    fn test_snake_case() {
        assert_eq!(to_snake_case("MyVariable"), "my_variable");
        assert_eq!(to_snake_case("TimeOfDay"), "time_of_day");
    }
}
