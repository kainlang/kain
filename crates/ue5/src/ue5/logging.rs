//! UE5 Smart Logging
//! 
//! Infrastructure for generating efficient UE_LOG statements with proper format specifiers.
//! No more double-allocation with FString::Printf!

use kain_core::ast::Expr;

/// Get proper UE_LOG format specifier and argument for an expression
/// Returns (format_spec, argument_code)
pub fn get_ue_log_format_spec(expr: &Expr, expr_code: &str) -> (String, String) {
    match expr {
        Expr::Int(_, _) => ("%lld".to_string(), expr_code.to_string()),
        Expr::Float(_, _) => ("%f".to_string(), expr_code.to_string()),
        Expr::Bool(_, _) => ("%s".to_string(), format!("({} ? TEXT(\"true\") : TEXT(\"false\"))", expr_code)),
        Expr::String(_, _) => ("%s".to_string(), format!("*FString({})", expr_code)),
        Expr::Ident(name, _) => {
            // Infer type from name patterns for better format specifiers
            if name.contains("count") || name.contains("index") || name.contains("id") || name.ends_with("_i") {
                ("%lld".to_string(), expr_code.to_string())
            } else if name.contains("scale") || name.contains("speed") || name.contains("intensity") || name.contains("phase") {
                ("%f".to_string(), expr_code.to_string())
            } else if name.starts_with("is_") || name.starts_with("has_") || name.starts_with("can_") {
                ("%s".to_string(), format!("({} ? TEXT(\"true\") : TEXT(\"false\"))", expr_code))
            } else {
                // Default: use LexToString for safety
                ("%s".to_string(), format!("*LexToString({})", expr_code))
            }
        }
        _ => {
            // For complex expressions, use LexToString
            ("%s".to_string(), format!("*LexToString({})", expr_code))
        }
    }
}

/// Escape string for C++ TEXT() macro
pub fn escape_string(s: &str) -> String {
    let mut result = String::new();
    for c in s.chars() {
        match c {
            '\n' => result.push_str("\\n"),
            '\t' => result.push_str("\\t"),
            '\\' => result.push_str("\\\\"),
            '"' => result.push_str("\\\""),
            _ => result.push(c),
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::Expr;
    use kain_core::span::Span;

    #[test]
    fn test_int_format() {
        let expr = Expr::Int(42, Span::default());
        let (spec, arg) = get_ue_log_format_spec(&expr, "42");
        assert_eq!(spec, "%lld");
        assert_eq!(arg, "42");
    }

    #[test]
    fn test_float_format() {
        let expr = Expr::Float(3.14, Span::default());
        let (spec, arg) = get_ue_log_format_spec(&expr, "3.14f");
        assert_eq!(spec, "%f");
        assert_eq!(arg, "3.14f");
    }

    #[test]
    fn test_bool_format() {
        let expr = Expr::Bool(true, Span::default());
        let (spec, arg) = get_ue_log_format_spec(&expr, "true");
        assert_eq!(spec, "%s");
        assert!(arg.contains("TEXT(\"true\")"));
    }
}
