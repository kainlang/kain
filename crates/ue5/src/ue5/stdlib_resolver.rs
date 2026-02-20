//! Standard Library Function Resolver
//!
//! Maps KAIN stdlib functions to UE5 equivalents (primarily FMath::).
//! Provides centralized, testable, and extensible function mapping.
//!
//! Example:
//! ```
//! use ue5::StdLibResolver;
//! let resolver = StdLibResolver::new();
//! let result = resolver.resolve("sqrt", &["16.0".to_string()]);
//! assert_eq!(result, Ok("FMath::Sqrt(16.0)".to_string()));
//! ```

use std::collections::HashMap;

/// Mapping from KAIN stdlib function to UE5 implementation
#[derive(Debug, Clone)]
pub struct StdLibMapping {
    /// KAIN function name (e.g., "sqrt")
    pub kain_name: String,
    /// UE5 template with $0, $1, ... placeholders (e.g., "FMath::Sqrt($0)")
    pub ue5_template: String,
    /// Expected parameter count
    pub param_count: usize,
    /// Required include file (e.g., "Math/UnrealMathUtility.h")
    pub requires_include: Option<String>,
    /// Optional description for documentation
    pub description: Option<String>,
}

/// Resolves KAIN stdlib function calls to UE5 equivalents
pub struct StdLibResolver {
    mappings: HashMap<String, StdLibMapping>,
}

impl StdLibResolver {
    /// Create a new resolver with all standard math functions
    pub fn new() -> Self {
        let mut resolver = Self {
            mappings: HashMap::new(),
        };

        // ===== Basic Math Functions =====
        resolver.add("abs", StdLibMapping {
            kain_name: "abs".into(),
            ue5_template: "FMath::Abs($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Absolute value".into()),
        });

        resolver.add("sqrt", StdLibMapping {
            kain_name: "sqrt".into(),
            ue5_template: "FMath::Sqrt($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Square root".into()),
        });

        resolver.add("pow", StdLibMapping {
            kain_name: "pow".into(),
            ue5_template: "FMath::Pow($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Power (base^exponent)".into()),
        });

        resolver.add("exp", StdLibMapping {
            kain_name: "exp".into(),
            ue5_template: "FMath::Exp($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Exponential (e^x)".into()),
        });

        resolver.add("log", StdLibMapping {
            kain_name: "log".into(),
            ue5_template: "FMath::Loge($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Natural logarithm".into()),
        });

        resolver.add("log2", StdLibMapping {
            kain_name: "log2".into(),
            ue5_template: "FMath::Log2($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Base-2 logarithm".into()),
        });

        // ===== Trigonometric Functions =====
        resolver.add("sin", StdLibMapping {
            kain_name: "sin".into(),
            ue5_template: "FMath::Sin($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Sine (radians)".into()),
        });

        resolver.add("cos", StdLibMapping {
            kain_name: "cos".into(),
            ue5_template: "FMath::Cos($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Cosine (radians)".into()),
        });

        resolver.add("tan", StdLibMapping {
            kain_name: "tan".into(),
            ue5_template: "FMath::Tan($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Tangent (radians)".into()),
        });

        resolver.add("asin", StdLibMapping {
            kain_name: "asin".into(),
            ue5_template: "FMath::Asin($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Arc sine (returns radians)".into()),
        });

        resolver.add("acos", StdLibMapping {
            kain_name: "acos".into(),
            ue5_template: "FMath::Acos($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Arc cosine (returns radians)".into()),
        });

        resolver.add("atan", StdLibMapping {
            kain_name: "atan".into(),
            ue5_template: "FMath::Atan($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Arc tangent (returns radians)".into()),
        });

        resolver.add("atan2", StdLibMapping {
            kain_name: "atan2".into(),
            ue5_template: "FMath::Atan2($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Two-argument arc tangent (y, x)".into()),
        });

        // ===== Rounding Functions =====
        resolver.add("floor", StdLibMapping {
            kain_name: "floor".into(),
            ue5_template: "FMath::FloorToFloat($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Round down to nearest integer".into()),
        });

        resolver.add("ceil", StdLibMapping {
            kain_name: "ceil".into(),
            ue5_template: "FMath::CeilToFloat($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Round up to nearest integer".into()),
        });

        resolver.add("round", StdLibMapping {
            kain_name: "round".into(),
            ue5_template: "FMath::RoundToFloat($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Round to nearest integer".into()),
        });

        resolver.add("fract", StdLibMapping {
            kain_name: "fract".into(),
            ue5_template: "FMath::Frac($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Fractional part (x - floor(x))".into()),
        });

        resolver.add("frac", StdLibMapping {
            kain_name: "frac".into(),
            ue5_template: "FMath::Frac($0)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Fractional part (alias for fract)".into()),
        });

        // ===== Min/Max/Clamp =====
        resolver.add("min", StdLibMapping {
            kain_name: "min".into(),
            ue5_template: "FMath::Min($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Minimum of two values".into()),
        });

        resolver.add("max", StdLibMapping {
            kain_name: "max".into(),
            ue5_template: "FMath::Max($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Maximum of two values".into()),
        });

        resolver.add("clamp", StdLibMapping {
            kain_name: "clamp".into(),
            ue5_template: "FMath::Clamp($0, $1, $2)".into(),
            param_count: 3,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Clamp value between min and max".into()),
        });

        // ===== Interpolation =====
        resolver.add("lerp", StdLibMapping {
            kain_name: "lerp".into(),
            ue5_template: "FMath::Lerp($0, $1, $2)".into(),
            param_count: 3,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Linear interpolation (a, b, t)".into()),
        });

        resolver.add("mix", StdLibMapping {
            kain_name: "mix".into(),
            ue5_template: "FMath::Lerp($0, $1, $2)".into(),
            param_count: 3,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Linear interpolation (GLSL alias for lerp)".into()),
        });

        resolver.add("smoothstep", StdLibMapping {
            kain_name: "smoothstep".into(),
            ue5_template: "FMath::SmoothStep($0, $1, $2)".into(),
            param_count: 3,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Smooth Hermite interpolation (edge0, edge1, x)".into()),
        });

        resolver.add("saturate", StdLibMapping {
            kain_name: "saturate".into(),
            ue5_template: "FMath::Clamp($0, 0.0f, 1.0f)".into(),
            param_count: 1,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Clamp to [0, 1] range".into()),
        });

        // ===== Random Functions =====
        resolver.add("random", StdLibMapping {
            kain_name: "random".into(),
            ue5_template: "FMath::FRand()".into(),
            param_count: 0,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Random float in [0, 1)".into()),
        });

        resolver.add("rand", StdLibMapping {
            kain_name: "rand".into(),
            ue5_template: "FMath::FRand()".into(),
            param_count: 0,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Random float in [0, 1) (alias)".into()),
        });

        resolver.add("random_range", StdLibMapping {
            kain_name: "random_range".into(),
            ue5_template: "FMath::FRandRange($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Random float in [min, max)".into()),
        });

        resolver.add("rand_range", StdLibMapping {
            kain_name: "rand_range".into(),
            ue5_template: "FMath::FRandRange($0, $1)".into(),
            param_count: 2,
            requires_include: Some("Math/UnrealMathUtility.h".into()),
            description: Some("Random float in [min, max) (alias)".into()),
        });

        // ===== Collection Functions (TArray) =====
        resolver.add("len", StdLibMapping {
            kain_name: "len".into(),
            ue5_template: "$0.Num()".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Get array length".into()),
        });

        resolver.add("push", StdLibMapping {
            kain_name: "push".into(),
            ue5_template: "$0.Add($1)".into(),
            param_count: 2,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Push element to array".into()),
        });

        resolver.add("pop", StdLibMapping {
            kain_name: "pop".into(),
            ue5_template: "$0.Pop()".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Pop last element from array".into()),
        });

        resolver.add("first", StdLibMapping {
            kain_name: "first".into(),
            ue5_template: "$0[0]".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Get first element of array".into()),
        });

        resolver.add("last", StdLibMapping {
            kain_name: "last".into(),
            ue5_template: "$0[$0.Num() - 1]".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Get last element of array".into()),
        });

        resolver.add("reverse", StdLibMapping {
            kain_name: "reverse".into(),
            ue5_template: "Algo::Reverse($0)".into(),
            param_count: 1,
            requires_include: Some("Algo/Reverse.h".into()),
            description: Some("Reverse array in-place".into()),
        });

        resolver.add("contains", StdLibMapping {
            kain_name: "contains".into(),
            ue5_template: "$0.Contains($1)".into(),
            param_count: 2,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Check if array contains value".into()),
        });

        resolver.add("index_of", StdLibMapping {
            kain_name: "index_of".into(),
            ue5_template: "$0.Find($1)".into(),
            param_count: 2,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Find index of value in array".into()),
        });

        resolver.add("remove", StdLibMapping {
            kain_name: "remove".into(),
            ue5_template: "$0.RemoveAt($1)".into(),
            param_count: 2,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Remove element at index".into()),
        });

        resolver.add("clear", StdLibMapping {
            kain_name: "clear".into(),
            ue5_template: "$0.Empty()".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Clear all elements from array".into()),
        });

        resolver.add("is_empty", StdLibMapping {
            kain_name: "is_empty".into(),
            ue5_template: "$0.IsEmpty()".into(),
            param_count: 1,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Check if array is empty".into()),
        });

        resolver.add("reserve", StdLibMapping {
            kain_name: "reserve".into(),
            ue5_template: "$0.Reserve($1)".into(),
            param_count: 2,
            requires_include: Some("Containers/Array.h".into()),
            description: Some("Reserve capacity for array".into()),
        });

        // ===== String Functions =====
        resolver.add("trim", StdLibMapping {
            kain_name: "trim".into(),
            ue5_template: "$0.TrimStartAndEnd()".into(),
            param_count: 1,
            requires_include: None,  // FString is always available
            description: Some("Trim whitespace from start and end of string".into()),
        });

        resolver.add("upper", StdLibMapping {
            kain_name: "upper".into(),
            ue5_template: "$0.ToUpper()".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Convert string to uppercase".into()),
        });

        resolver.add("lower", StdLibMapping {
            kain_name: "lower".into(),
            ue5_template: "$0.ToLower()".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Convert string to lowercase".into()),
        });

        resolver.add("str_contains", StdLibMapping {
            kain_name: "str_contains".into(),
            ue5_template: "$0.Contains($1)".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Check if string contains substring".into()),
        });

        resolver.add("starts_with", StdLibMapping {
            kain_name: "starts_with".into(),
            ue5_template: "$0.StartsWith($1)".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Check if string starts with prefix".into()),
        });

        resolver.add("ends_with", StdLibMapping {
            kain_name: "ends_with".into(),
            ue5_template: "$0.EndsWith($1)".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Check if string ends with suffix".into()),
        });

        resolver.add("replace", StdLibMapping {
            kain_name: "replace".into(),
            ue5_template: "$0.Replace(*$1, *$2)".into(),
            param_count: 3,
            requires_include: None,
            description: Some("Replace all occurrences of substring".into()),
        });

        resolver.add("substring", StdLibMapping {
            kain_name: "substring".into(),
            ue5_template: "$0.Mid($1, $2)".into(),
            param_count: 3,
            requires_include: None,
            description: Some("Extract substring (start, length)".into()),
        });

        resolver.add("char_at", StdLibMapping {
            kain_name: "char_at".into(),
            ue5_template: "FString(1, &$0[$1])".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Get character at index as string".into()),
        });

        resolver.add("to_int", StdLibMapping {
            kain_name: "to_int".into(),
            ue5_template: "FCString::Atoi(*$0)".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Convert string to integer".into()),
        });

        resolver.add("to_float", StdLibMapping {
            kain_name: "to_float".into(),
            ue5_template: "FCString::Atof(*$0)".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Convert string to float".into()),
        });

        resolver.add("str_is_empty", StdLibMapping {
            kain_name: "str_is_empty".into(),
            ue5_template: "$0.IsEmpty()".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Check if string is empty".into()),
        });

        resolver.add("join", StdLibMapping {
            kain_name: "join".into(),
            ue5_template: "FString::Join($0, *$1)".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Join array of strings with delimiter".into()),
        });

        resolver.add("str_len", StdLibMapping {
            kain_name: "str_len".into(),
            ue5_template: "$0.Len()".into(),
            param_count: 1,
            requires_include: None,
            description: Some("Get string length".into()),
        });

        resolver.add("split", StdLibMapping {
            kain_name: "split".into(),
            ue5_template: "[&](){ TArray<FString> Parts; $0.ParseIntoArray(Parts, *$1); return Parts; }()".into(),
            param_count: 2,
            requires_include: None,
            description: Some("Split string by delimiter into array".into()),
        });

        resolver
    }

    /// Add a custom mapping (for testing or extensions)
    pub fn add(&mut self, name: &str, mapping: StdLibMapping) {
        self.mappings.insert(name.to_string(), mapping);
    }

    /// Resolve a function call to UE5 code
    ///
    /// # Arguments
    /// * `fn_name` - KAIN function name (e.g., "sqrt")
    /// * `args` - Generated C++ argument strings (e.g., ["16.0"])
    ///
    /// # Returns
    /// * `Ok(String)` - UE5 code (e.g., "FMath::Sqrt(16.0)")
    /// * `Err(String)` - Error message if function not found or arg count mismatch
    pub fn resolve(&self, fn_name: &str, args: &[String]) -> Result<String, String> {
        let mapping = self
            .mappings
            .get(fn_name)
            .ok_or_else(|| format!("Unknown stdlib function: {}", fn_name))?;

        if args.len() != mapping.param_count {
            return Err(format!(
                "Function '{}' expects {} arguments, got {}",
                fn_name,
                mapping.param_count,
                args.len()
            ));
        }

        // Substitute $0, $1, ... with actual args
        let mut result = mapping.ue5_template.clone();
        for (i, arg) in args.iter().enumerate() {
            result = result.replace(&format!("${}", i), arg);
        }

        Ok(result)
    }

    /// Check if a function is a stdlib function
    pub fn is_stdlib_function(&self, fn_name: &str) -> bool {
        self.mappings.contains_key(fn_name)
    }

    /// Get required include for a function (if any)
    pub fn get_required_include(&self, fn_name: &str) -> Option<&str> {
        self.mappings
            .get(fn_name)
            .and_then(|m| m.requires_include.as_deref())
    }

    /// Get all stdlib function names (for documentation/debugging)
    pub fn get_all_functions(&self) -> Vec<&str> {
        self.mappings.keys().map(|s| s.as_str()).collect()
    }

    /// Get mapping details for a function (for documentation)
    pub fn get_mapping(&self, fn_name: &str) -> Option<&StdLibMapping> {
        self.mappings.get(fn_name)
    }
}

impl Default for StdLibResolver {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_math() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("abs", &["-5.0".to_string()]),
            Ok("FMath::Abs(-5.0)".to_string())
        );

        assert_eq!(
            resolver.resolve("sqrt", &["16.0".to_string()]),
            Ok("FMath::Sqrt(16.0)".to_string())
        );

        assert_eq!(
            resolver.resolve("pow", &["2.0".to_string(), "3.0".to_string()]),
            Ok("FMath::Pow(2.0, 3.0)".to_string())
        );
    }

    #[test]
    fn test_trig_functions() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("sin", &["3.14159".to_string()]),
            Ok("FMath::Sin(3.14159)".to_string())
        );

        assert_eq!(
            resolver.resolve("atan2", &["y".to_string(), "x".to_string()]),
            Ok("FMath::Atan2(y, x)".to_string())
        );
    }

    #[test]
    fn test_rounding() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("floor", &["3.7".to_string()]),
            Ok("FMath::FloorToFloat(3.7)".to_string())
        );

        assert_eq!(
            resolver.resolve("ceil", &["3.2".to_string()]),
            Ok("FMath::CeilToFloat(3.2)".to_string())
        );

        assert_eq!(
            resolver.resolve("round", &["3.5".to_string()]),
            Ok("FMath::RoundToFloat(3.5)".to_string())
        );
    }

    #[test]
    fn test_min_max_clamp() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("min", &["10.0".to_string(), "20.0".to_string()]),
            Ok("FMath::Min(10.0, 20.0)".to_string())
        );

        assert_eq!(
            resolver.resolve("max", &["10.0".to_string(), "20.0".to_string()]),
            Ok("FMath::Max(10.0, 20.0)".to_string())
        );

        assert_eq!(
            resolver.resolve("clamp", &["150.0".to_string(), "0.0".to_string(), "100.0".to_string()]),
            Ok("FMath::Clamp(150.0, 0.0, 100.0)".to_string())
        );
    }

    #[test]
    fn test_interpolation() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("lerp", &["0.0".to_string(), "100.0".to_string(), "0.5".to_string()]),
            Ok("FMath::Lerp(0.0, 100.0, 0.5)".to_string())
        );

        assert_eq!(
            resolver.resolve("smoothstep", &["0.0".to_string(), "1.0".to_string(), "0.5".to_string()]),
            Ok("FMath::SmoothStep(0.0, 1.0, 0.5)".to_string())
        );
    }

    #[test]
    fn test_random() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.resolve("random", &[]),
            Ok("FMath::FRand()".to_string())
        );

        assert_eq!(
            resolver.resolve("random_range", &["0.0".to_string(), "100.0".to_string()]),
            Ok("FMath::FRandRange(0.0, 100.0)".to_string())
        );
    }

    #[test]
    fn test_error_handling() {
        let resolver = StdLibResolver::new();

        // Unknown function
        assert!(resolver.resolve("unknown_func", &[]).is_err());

        // Wrong arg count
        assert!(resolver.resolve("sqrt", &[]).is_err());
        assert!(resolver.resolve("sqrt", &["1.0".to_string(), "2.0".to_string()]).is_err());
    }

    #[test]
    fn test_is_stdlib_function() {
        let resolver = StdLibResolver::new();

        assert!(resolver.is_stdlib_function("sqrt"));
        assert!(resolver.is_stdlib_function("sin"));
        assert!(resolver.is_stdlib_function("lerp"));
        assert!(!resolver.is_stdlib_function("custom_function"));
    }

    #[test]
    fn test_get_required_include() {
        let resolver = StdLibResolver::new();

        assert_eq!(
            resolver.get_required_include("sqrt"),
            Some("Math/UnrealMathUtility.h")
        );
    }

    #[test]
    fn test_all_20_functions() {
        let resolver = StdLibResolver::new();
        let functions = resolver.get_all_functions();

        // Verify we have at least 20 unique functions
        assert!(functions.len() >= 20, "Expected at least 20 stdlib functions, got {}", functions.len());

        // Verify key functions exist
        let key_functions = vec![
            "abs", "sqrt", "pow", "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
            "floor", "ceil", "round", "min", "max", "clamp", "lerp", "smoothstep",
            "random", "random_range"
        ];

        for func in key_functions {
            assert!(
                resolver.is_stdlib_function(func),
                "Missing stdlib function: {}",
                func
            );
        }
    }

    #[test]
    fn test_collection_functions() {
        let resolver = StdLibResolver::new();

        // len
        assert_eq!(
            resolver.resolve("len", &["arr".to_string()]),
            Ok("arr.Num()".to_string())
        );

        // push
        assert_eq!(
            resolver.resolve("push", &["arr".to_string(), "value".to_string()]),
            Ok("arr.Add(value)".to_string())
        );

        // pop
        assert_eq!(
            resolver.resolve("pop", &["arr".to_string()]),
            Ok("arr.Pop()".to_string())
        );

        // first
        assert_eq!(
            resolver.resolve("first", &["arr".to_string()]),
            Ok("arr[0]".to_string())
        );

        // last
        assert_eq!(
            resolver.resolve("last", &["arr".to_string()]),
            Ok("arr[arr.Num() - 1]".to_string())
        );

        // reverse
        assert_eq!(
            resolver.resolve("reverse", &["arr".to_string()]),
            Ok("Algo::Reverse(arr)".to_string())
        );

        // contains
        assert_eq!(
            resolver.resolve("contains", &["arr".to_string(), "10".to_string()]),
            Ok("arr.Contains(10)".to_string())
        );

        // index_of
        assert_eq!(
            resolver.resolve("index_of", &["arr".to_string(), "20".to_string()]),
            Ok("arr.Find(20)".to_string())
        );

        // remove
        assert_eq!(
            resolver.resolve("remove", &["arr".to_string(), "0".to_string()]),
            Ok("arr.RemoveAt(0)".to_string())
        );

        // clear
        assert_eq!(
            resolver.resolve("clear", &["arr".to_string()]),
            Ok("arr.Empty()".to_string())
        );

        // is_empty
        assert_eq!(
            resolver.resolve("is_empty", &["arr".to_string()]),
            Ok("arr.IsEmpty()".to_string())
        );

        // reserve
        assert_eq!(
            resolver.resolve("reserve", &["arr".to_string(), "100".to_string()]),
            Ok("arr.Reserve(100)".to_string())
        );
    }

    #[test]
    fn test_collection_includes() {
        let resolver = StdLibResolver::new();

        // Most collection functions use Containers/Array.h
        assert_eq!(
            resolver.get_required_include("len"),
            Some("Containers/Array.h")
        );
        assert_eq!(
            resolver.get_required_include("push"),
            Some("Containers/Array.h")
        );
        assert_eq!(
            resolver.get_required_include("contains"),
            Some("Containers/Array.h")
        );

        // reverse uses Algo/Reverse.h
        assert_eq!(
            resolver.get_required_include("reverse"),
            Some("Algo/Reverse.h")
        );
    }

    #[test]
    fn test_all_32_functions() {
        let resolver = StdLibResolver::new();
        let functions = resolver.get_all_functions();

        // Verify we have at least 32 unique functions (20 math + 12 collection)
        assert!(functions.len() >= 32, "Expected at least 32 stdlib functions, got {}", functions.len());

        // Verify all collection functions exist
        let collection_functions = vec![
            "len", "push", "pop", "first", "last", "reverse",
            "contains", "index_of", "remove", "clear", "is_empty", "reserve"
        ];

        for func in collection_functions {
            assert!(
                resolver.is_stdlib_function(func),
                "Missing collection function: {}",
                func
            );
        }
    }

    #[test]
    fn test_string_functions() {
        let resolver = StdLibResolver::new();

        // Test trim
        assert_eq!(
            resolver.resolve("trim", &["str".to_string()]),
            Ok("str.TrimStartAndEnd()".to_string())
        );

        // Test upper/lower
        assert_eq!(
            resolver.resolve("upper", &["str".to_string()]),
            Ok("str.ToUpper()".to_string())
        );

        assert_eq!(
            resolver.resolve("lower", &["str".to_string()]),
            Ok("str.ToLower()".to_string())
        );

        // Test str_contains
        assert_eq!(
            resolver.resolve("str_contains", &["str".to_string(), "TEXT(\"sub\")".to_string()]),
            Ok("str.Contains(TEXT(\"sub\"))".to_string())
        );

        // Test starts_with/ends_with
        assert_eq!(
            resolver.resolve("starts_with", &["str".to_string(), "TEXT(\"prefix\")".to_string()]),
            Ok("str.StartsWith(TEXT(\"prefix\"))".to_string())
        );

        assert_eq!(
            resolver.resolve("ends_with", &["str".to_string(), "TEXT(\"suffix\")".to_string()]),
            Ok("str.EndsWith(TEXT(\"suffix\"))".to_string())
        );

        // Test replace
        assert_eq!(
            resolver.resolve("replace", &["str".to_string(), "TEXT(\"old\")".to_string(), "TEXT(\"new\")".to_string()]),
            Ok("str.Replace(*TEXT(\"old\"), *TEXT(\"new\"))".to_string())
        );

        // Test substring
        assert_eq!(
            resolver.resolve("substring", &["str".to_string(), "0".to_string(), "5".to_string()]),
            Ok("str.Mid(0, 5)".to_string())
        );

        // Test char_at
        assert_eq!(
            resolver.resolve("char_at", &["str".to_string(), "0".to_string()]),
            Ok("FString(1, &str[0])".to_string())
        );

        // Test to_int/to_float
        assert_eq!(
            resolver.resolve("to_int", &["str".to_string()]),
            Ok("FCString::Atoi(*str)".to_string())
        );

        assert_eq!(
            resolver.resolve("to_float", &["str".to_string()]),
            Ok("FCString::Atof(*str)".to_string())
        );

        // Test str_is_empty
        assert_eq!(
            resolver.resolve("str_is_empty", &["str".to_string()]),
            Ok("str.IsEmpty()".to_string())
        );

        // Test join
        assert_eq!(
            resolver.resolve("join", &["arr".to_string(), "TEXT(\",\")".to_string()]),
            Ok("FString::Join(arr, *TEXT(\",\"))".to_string())
        );

        // Test str_len
        assert_eq!(
            resolver.resolve("str_len", &["str".to_string()]),
            Ok("str.Len()".to_string())
        );

        // Test split
        assert_eq!(
            resolver.resolve("split", &["str".to_string(), "TEXT(\",\")".to_string()]),
            Ok("[&](){ TArray<FString> Parts; str.ParseIntoArray(Parts, *TEXT(\",\")); return Parts; }()".to_string())
        );
    }

    #[test]
    fn test_string_function_count() {
        let resolver = StdLibResolver::new();
        
        // Verify all 15 string functions exist
        let string_functions = vec![
            "trim", "upper", "lower", "str_contains", "starts_with", "ends_with",
            "replace", "substring", "char_at", "to_int", "to_float",
            "str_is_empty", "join", "str_len", "split"
        ];

        for func in string_functions {
            assert!(
                resolver.is_stdlib_function(func),
                "Missing string function: {}",
                func
            );
        }
    }

    #[test]
    fn test_all_47_functions() {
        let resolver = StdLibResolver::new();
        let functions = resolver.get_all_functions();

        // Verify we have at least 47 unique functions (20 math + 12 collection + 15 string)
        assert!(functions.len() >= 47, "Expected at least 47 stdlib functions, got {}", functions.len());
    }
}
