//! Layout Optimizer - Reactive Property Analysis
//!
//! Inspired by the InterpolatorPacker from usf.rs, this module analyzes
//! Slate widget properties to determine optimal reactivity:
//! - SLATE_ATTRIBUTE: Reactive properties that change frequently (needs Tick)
//! - SLATE_ARGUMENT: Static properties set once at construction
//!
//! This optimization significantly reduces Slate Tick overhead for complex UIs.

use kain_core::ast::{Field, Struct, Expr, Type};
use std::collections::{HashMap, HashSet};

/// Property reactivity classification
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PropertyReactivity {
    /// Static property - set once, never changes
    Static,
    /// Reactive property - may change during widget lifetime
    Reactive,
    /// Event handler - always a delegate
    Event,
}

/// Analysis result for a widget property
#[derive(Debug, Clone)]
pub struct PropertyAnalysis {
    pub name: String,
    pub reactivity: PropertyReactivity,
    pub reason: String,
}

/// Layout optimizer that analyzes widget properties
pub struct LayoutOptimizer {
    /// Known reactive property patterns
    reactive_patterns: HashSet<String>,
    /// Known static property patterns
    static_patterns: HashSet<String>,
    /// Property usage frequency (from analysis)
    usage_frequency: HashMap<String, usize>,
}

impl LayoutOptimizer {
    pub fn new() -> Self {
        let mut reactive_patterns = HashSet::new();
        let mut static_patterns = HashSet::new();
        
        // Common reactive properties (change frequently)
        reactive_patterns.insert("Text".to_string());
        reactive_patterns.insert("Value".to_string());
        reactive_patterns.insert("Progress".to_string());
        reactive_patterns.insert("IsEnabled".to_string());
        reactive_patterns.insert("Visibility".to_string());
        reactive_patterns.insert("IsChecked".to_string());
        reactive_patterns.insert("SelectedItem".to_string());
        reactive_patterns.insert("CurrentValue".to_string());
        reactive_patterns.insert("DisplayText".to_string());
        
        // Common static properties (set once)
        static_patterns.insert("ToolTipText".to_string());
        static_patterns.insert("HintText".to_string());
        static_patterns.insert("Label".to_string());
        static_patterns.insert("Icon".to_string());
        static_patterns.insert("Style".to_string());
        static_patterns.insert("Font".to_string());
        static_patterns.insert("ColorAndOpacity".to_string());
        static_patterns.insert("Padding".to_string());
        static_patterns.insert("Margin".to_string());
        static_patterns.insert("MinDesiredWidth".to_string());
        static_patterns.insert("MaxDesiredWidth".to_string());
        static_patterns.insert("MinDesiredHeight".to_string());
        static_patterns.insert("MaxDesiredHeight".to_string());
        
        Self {
            reactive_patterns,
            static_patterns,
            usage_frequency: HashMap::new(),
        }
    }
    
    /// Analyze a widget struct and classify all properties
    pub fn analyze_widget(&mut self, st: &Struct) -> Vec<PropertyAnalysis> {
        let mut results = Vec::new();
        
        for field in &st.fields {
            let analysis = self.analyze_property(field, st);
            results.push(analysis);
        }
        
        results
    }
    
    /// Analyze a single property
    fn analyze_property(&self, field: &Field, st: &Struct) -> PropertyAnalysis {
        let name = &field.name;
        
        // Check if it's an event handler (starts with "On")
        if name.starts_with("On") {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Event,
                reason: "Event handler (naming convention)".to_string(),
            };
        }
        
        // Check explicit @argument attribute (forces static)
        if field.attributes.iter().any(|a| a.name == "argument") {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Static,
                reason: "Explicit @argument attribute".to_string(),
            };
        }
        
        // Check explicit @reactive attribute (forces reactive)
        if field.attributes.iter().any(|a| a.name == "reactive") {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Reactive,
                reason: "Explicit @reactive attribute".to_string(),
            };
        }
        
        // Check against known patterns
        if self.reactive_patterns.contains(name) {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Reactive,
                reason: "Known reactive pattern".to_string(),
            };
        }
        
        if self.static_patterns.contains(name) {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Static,
                reason: "Known static pattern".to_string(),
            };
        }
        
        // Analyze type - complex types are usually static
        let is_complex_type = match &field.ty {
            Type::Array(_, _, _) => true,
            Type::Named { name, .. } if name.contains("Brush") || name.contains("Style") => true,
            _ => false,
        };
        
        if is_complex_type {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Static,
                reason: "Complex type (usually static)".to_string(),
            };
        }
        
        // Check if property is used in Compose() method
        let used_in_compose = self.is_used_in_compose(name, st);
        if used_in_compose {
            return PropertyAnalysis {
                name: name.clone(),
                reactivity: PropertyReactivity::Reactive,
                reason: "Used in Compose() method (likely reactive)".to_string(),
            };
        }
        
        // Default: assume static (safer, less overhead)
        PropertyAnalysis {
            name: name.clone(),
            reactivity: PropertyReactivity::Static,
            reason: "Default (no clear reactive usage)".to_string(),
        }
    }
    
    /// Check if a property is referenced in the Compose() method
    fn is_used_in_compose(&self, prop_name: &str, st: &Struct) -> bool {
        if let Some(compose_fn) = st.methods.iter().find(|m| m.name == "Compose") {
            self.expr_references_property(&compose_fn.body.stmts.iter()
                .filter_map(|stmt| match stmt {
                    kain_core::ast::Stmt::Expr(e) => Some(e),
                    _ => None,
                })
                .next()
                .unwrap_or(&Expr::None(kain_core::span::Span::default())), 
                prop_name)
        } else {
            false
        }
    }
    
    /// Recursively check if an expression references a property
    fn expr_references_property(&self, expr: &Expr, prop_name: &str) -> bool {
        match expr {
            Expr::Ident(name, _) => name == prop_name,
            Expr::Field { object, field, .. } => {
                field == prop_name || self.expr_references_property(object, prop_name)
            }
            Expr::Call { args, .. } => {
                args.iter().any(|arg| self.expr_references_property(&arg.value, prop_name))
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.expr_references_property(receiver, prop_name) ||
                args.iter().any(|arg| self.expr_references_property(&arg.value, prop_name))
            }
            _ => false,
        }
    }
    
    /// Generate optimization report
    pub fn generate_report(&self, analyses: &[PropertyAnalysis]) -> String {
        let mut report = String::new();
        
        let static_count = analyses.iter().filter(|a| a.reactivity == PropertyReactivity::Static).count();
        let reactive_count = analyses.iter().filter(|a| a.reactivity == PropertyReactivity::Reactive).count();
        let event_count = analyses.iter().filter(|a| a.reactivity == PropertyReactivity::Event).count();
        
        report.push_str(&format!("// Layout Optimization Report\n"));
        report.push_str(&format!("// Static Properties: {} (no Tick overhead)\n", static_count));
        report.push_str(&format!("// Reactive Properties: {} (requires Tick)\n", reactive_count));
        report.push_str(&format!("// Event Handlers: {}\n", event_count));
        report.push_str(&format!("// Estimated Tick Reduction: {:.1}%\n", 
            (static_count as f32 / analyses.len() as f32) * 100.0));
        report.push_str("//\n");
        
        for analysis in analyses {
            let macro_type = match analysis.reactivity {
                PropertyReactivity::Static => "SLATE_ARGUMENT",
                PropertyReactivity::Reactive => "SLATE_ATTRIBUTE",
                PropertyReactivity::Event => "SLATE_EVENT",
            };
            report.push_str(&format!("// {} {} - {}\n", 
                macro_type, analysis.name, analysis.reason));
        }
        
        report
    }
}

impl Default for LayoutOptimizer {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Field, Struct, Type, Visibility};
    use kain_core::span::Span;
    
    fn s() -> Span { Span::default() }
    
    fn make_field(name: &str, ty: Type) -> Field {
        Field {
            name: name.to_string(),
            ty,
            attributes: vec![],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: s(),
        }
    }
    
    fn make_struct(name: &str, fields: Vec<Field>) -> Struct {
        Struct {
            name: name.to_string(),
            generics: vec![],
            fields,
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: s(),
        }
    }
    
    #[test]
    fn test_event_detection() {
        let optimizer = LayoutOptimizer::new();
        let field = make_field("OnClicked", Type::Unit(s()));
        let st = make_struct("TestWidget", vec![field.clone()]);
        
        let analysis = optimizer.analyze_property(&field, &st);
        assert_eq!(analysis.reactivity, PropertyReactivity::Event);
    }
    
    #[test]
    fn test_reactive_pattern_detection() {
        let optimizer = LayoutOptimizer::new();
        let field = make_field("Text", Type::Named { 
            name: "String".to_string(), 
            generics: vec![],
            span: s() 
        });
        let st = make_struct("TestWidget", vec![field.clone()]);
        
        let analysis = optimizer.analyze_property(&field, &st);
        assert_eq!(analysis.reactivity, PropertyReactivity::Reactive);
    }
    
    #[test]
    fn test_static_pattern_detection() {
        let optimizer = LayoutOptimizer::new();
        let field = make_field("ToolTipText", Type::Named { 
            name: "String".to_string(), 
            generics: vec![],
            span: s() 
        });
        let st = make_struct("TestWidget", vec![field.clone()]);
        
        let analysis = optimizer.analyze_property(&field, &st);
        assert_eq!(analysis.reactivity, PropertyReactivity::Static);
    }
}

