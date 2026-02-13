//! Detail Customization Generation
//!
//! Generates IDetailCustomization subclasses for custom property panels.
//! Supports:
//! - @category for property grouping
//! - @slider(min, max) for SpinBox ranges
//! - @color_picker for color pickers
//! - @asset_picker(allowed_classes=["..."]) for asset pickers
//! - @visible_if("expr") for conditional visibility
//! - @button("Label") for action buttons

use kain_core::ast::{Expr, Field, Struct, Type};
use kain_core::types::TypedStruct;

/// Information about a category group
#[derive(Debug, Clone)]
struct CategoryGroup {
    name: String,
    fields: Vec<DetailField>,
}

/// A field with its detail customization metadata
#[derive(Debug, Clone)]
struct DetailField {
    name: String,
    cpp_type: String,
    widget_override: Option<WidgetOverride>,
    visibility_condition: Option<String>,
}

/// Widget override for custom property display
#[derive(Debug, Clone)]
enum WidgetOverride {
    Slider { min: f64, max: f64 },
    ColorPicker,
    AssetPicker { allowed_classes: Vec<String> },
    Button { label: String },
}

pub struct DetailsGenerator {
    lines: Vec<String>,
    indent: usize,
}

impl DetailsGenerator {
    pub fn new() -> Self {
        Self {
            lines: Vec::new(),
            indent: 0,
        }
    }
    
    /// Generate IDetailCustomization header and source from a @details struct
    pub fn generate_customization(&mut self, st: &TypedStruct) -> (String, String) {
        let class_name = format!("F{}Customization", st.ast.name);
        let target_class = format!("U{}", st.ast.name.replace("Details", ""));
        
        let header = self.generate_header(&st.ast, &class_name);
        let source = self.generate_source(&st.ast, &class_name, &target_class);
        
        (header, source)
    }
    
    /// Generate registration code for module startup
    pub fn generate_registration(&self, st: &TypedStruct) -> String {
        let class_name = format!("F{}Customization", st.ast.name);
        let target_class = format!("U{}", st.ast.name.replace("Details", ""));
        
        format!(
            concat!(
                "\t{{\n",
                "\t\tFPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(\"PropertyEditor\");\n",
                "\t\tPropertyModule.RegisterCustomClassLayout(\n",
                "\t\t\t{}::StaticClass()->GetFName(),\n",
                "\t\t\tFOnGetDetailCustomizationInstance::CreateStatic(&{}::MakeInstance)\n",
                "\t\t);\n",
                "\t}}"
            ),
            target_class, class_name
        )
    }
    
    fn generate_header(&mut self, st: &Struct, class_name: &str) -> String {
        self.lines.clear();
        self.indent = 0;
        
        self.push_line(&format!("class {} : public IDetailCustomization", class_name));
        self.push_line("{");
        self.push_line("public:");
        self.indent += 1;
        
        // Factory method
        self.push_line(&format!(
            "static TSharedRef<IDetailCustomization> MakeInstance() {{ return MakeShareable(new {}()); }}",
            class_name
        ));
        self.push_line("");
        
        // CustomizeDetails override
        self.push_line("virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;");
        self.push_line("");
        
        // Generate button handler declarations
        for field in &st.fields {
            if let Some(button_attr) = field.attributes.iter().find(|a| a.name == "button") {
                let handler_name = format!("OnButton_{}", field.name);
                self.push_line(&format!("FReply {}();", handler_name));
            }
        }
        for method in &st.methods {
            if method.attributes.iter().any(|a| a.name == "button") {
                self.push_line(&format!("FReply On_{}();", method.name));
            }
        }
        
        self.indent -= 1;
        self.push_line("private:");
        self.indent += 1;
        
        // Cached object pointer
        self.push_line("TWeakObjectPtr<UObject> CachedObject;");
        
        self.indent -= 1;
        self.push_line("};");
        
        self.lines.join("\n")
    }
    
    fn generate_source(&mut self, st: &Struct, class_name: &str, target_class: &str) -> String {
        self.lines.clear();
        self.indent = 0;
        
        // Build category groups
        let categories = self.build_categories(st);
        
        // CustomizeDetails implementation
        self.push_line(&format!("void {}::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)", class_name));
        self.push_line("{");
        self.indent += 1;
        
        // Cache the object being customized
        self.push_line("TArray<TWeakObjectPtr<UObject>> Objects;");
        self.push_line("DetailBuilder.GetObjectsBeingCustomized(Objects);");
        self.push_line("if (Objects.Num() > 0)");
        self.push_line("{");
        self.indent += 1;
        self.push_line("CachedObject = Objects[0];");
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // Generate category layouts
        for category in &categories {
            self.push_line(&format!(
                "IDetailCategoryBuilder& {}Cat = DetailBuilder.EditCategory(TEXT(\"{}\"));",
                category.name.replace(" ", ""), category.name
            ));
            self.push_line("");
            
            for field in &category.fields {
                self.generate_field_customization(field, &category.name, class_name);
            }
        }
        
        // Generate method-level button actions in an "Actions" category
        let has_method_buttons = st.methods.iter().any(|m| m.attributes.iter().any(|a| a.name == "button"));
        if has_method_buttons {
            self.push_line("IDetailCategoryBuilder& ActionsCat = DetailBuilder.EditCategory(TEXT(\"Actions\"));");
            for method in &st.methods {
                if let Some(button_attr) = method.attributes.iter().find(|a| a.name == "button") {
                    let label = self.extract_string_attr_arg(button_attr)
                        .unwrap_or_else(|| method.name.clone());
                    self.push_line(&format!(
                        "ActionsCat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                        label
                    ));
                    self.push_line(".WholeRowContent()");
                    self.push_line("[");
                    self.indent += 1;
                    self.push_line(&format!(
                        "SNew(SButton).Text(FText::FromString(TEXT(\"{}\"))).OnClicked(FOnClicked::CreateSP(this, &{}::On_{}))",
                        label, class_name, method.name
                    ));
                    self.indent -= 1;
                    self.push_line("];");
                }
            }
            self.push_line("");
        }
        
        self.indent -= 1;
        self.push_line("}");
        self.push_line("");
        
        // Generate button handler implementations
        for field in &st.fields {
            if field.attributes.iter().any(|a| a.name == "button") {
                let handler_name = format!("OnButton_{}", field.name);
                self.push_line(&format!("FReply {}::{}()", class_name, handler_name));
                self.push_line("{");
                self.indent += 1;
                self.push_line("// TODO: Implement button action");
                self.push_line("return FReply::Handled();");
                self.indent -= 1;
                self.push_line("}");
                self.push_line("");
            }
        }
        
        for method in &st.methods {
            if method.attributes.iter().any(|a| a.name == "button") {
                self.push_line(&format!("FReply {}::On_{}()", class_name, method.name));
                self.push_line("{");
                self.indent += 1;
                self.push_line("// TODO: Implement button action");
                self.push_line("return FReply::Handled();");
                self.indent -= 1;
                self.push_line("}");
                self.push_line("");
            }
        }
        
        self.lines.join("\n")
    }
    
    fn build_categories(&self, st: &Struct) -> Vec<CategoryGroup> {
        let mut categories: Vec<CategoryGroup> = Vec::new();
        let mut current_category = "Default".to_string();
        
        for field in &st.fields {
            // Check for @category attribute
            if let Some(cat_attr) = field.attributes.iter().find(|a| a.name == "category") {
                if let Some(cat_name) = self.extract_string_attr_arg(cat_attr) {
                    current_category = cat_name;
                }
            }
            
            let widget_override = self.detect_widget_override(field);
            let visibility_condition = self.detect_visibility_condition(field);
            
            let detail_field = DetailField {
                name: field.name.clone(),
                cpp_type: self.map_type(&field.ty),
                widget_override,
                visibility_condition,
            };
            
            // Find or create category
            if let Some(cat) = categories.iter_mut().find(|c| c.name == current_category) {
                cat.fields.push(detail_field);
            } else {
                categories.push(CategoryGroup {
                    name: current_category.clone(),
                    fields: vec![detail_field],
                });
            }
        }
        
        categories
    }
    
    fn detect_widget_override(&self, field: &Field) -> Option<WidgetOverride> {
        for attr in &field.attributes {
            match attr.name.as_str() {
                "slider" => {
                    // @slider(min, max) — positional args: first is min, second is max
                    let min = self.extract_float_arg_at(&attr.args, 0).unwrap_or(0.0);
                    let max = self.extract_float_arg_at(&attr.args, 1).unwrap_or(100.0);
                    return Some(WidgetOverride::Slider { min, max });
                }
                "color_picker" => {
                    return Some(WidgetOverride::ColorPicker);
                }
                "asset_picker" => {
                    let classes = self.extract_string_list_arg(&attr.args, "allowed_classes");
                    return Some(WidgetOverride::AssetPicker { allowed_classes: classes });
                }
                "button" => {
                    let label = self.extract_string_attr_arg(attr).unwrap_or_else(|| field.name.clone());
                    return Some(WidgetOverride::Button { label });
                }
                _ => {}
            }
        }
        None
    }
    
    fn detect_visibility_condition(&self, field: &Field) -> Option<String> {
        field.attributes.iter()
            .find(|a| a.name == "visible_if")
            .and_then(|a| self.extract_string_attr_arg(a))
    }
    
    fn generate_field_customization(&mut self, field: &DetailField, category_name: &str, class_name: &str) {
        let cat_var = category_name.replace(" ", "");
        
        match &field.widget_override {
            Some(WidgetOverride::Slider { min, max }) => {
                self.push_line(&format!("// Custom slider for {}", field.name));
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, field.name
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    field.name
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                // Format floats to always have decimal point
                let min_str = if min.fract() == 0.0 { format!("{:.1}", min) } else { format!("{}", min) };
                let max_str = if max.fract() == 0.0 { format!("{:.1}", max) } else { format!("{}", max) };
                self.push_line(&format!(
                    "SNew(SSpinBox<float>)\n\t\t.MinValue({}f)\n\t\t.MaxValue({}f)",
                    min_str, max_str
                ));
                self.indent -= 1;
                self.push_line("];");
                self.push_line("");
            }
            Some(WidgetOverride::ColorPicker) => {
                self.push_line(&format!("// Color picker for {}", field.name));
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, field.name
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    field.name
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line("SNew(SColorBlock)");
                self.indent -= 1;
                self.push_line("];");
                self.push_line("");
            }
            Some(WidgetOverride::AssetPicker { allowed_classes }) => {
                self.push_line(&format!("// Asset picker for {}", field.name));
                let classes_str = allowed_classes.join(", ");
                self.push_line(&format!(
                    "// Allowed classes: {}",
                    classes_str
                ));
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, field.name
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    field.name
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line("SNew(SObjectPropertyEntryBox)");
                if let Some(first_class) = allowed_classes.first() {
                    self.push_line(&format!(".AllowedClass({}::StaticClass())", first_class));
                }
                self.indent -= 1;
                self.push_line("];");
                self.push_line("");
            }
            Some(WidgetOverride::Button { label }) => {
                let handler_name = format!("OnButton_{}", field.name);
                self.push_line(&format!("// Button: {}", label));
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, label
                ));
                self.push_line(".WholeRowContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(SButton)\n\t\t.Text(FText::FromString(TEXT(\"{}\")))\n\t\t.OnClicked(FOnClicked::CreateSP(this, &{}::{}))",
                    label, class_name, handler_name
                ));
                self.indent -= 1;
                self.push_line("];");
                self.push_line("");
            }
            None => {
                // Standard property — just let UE5 handle it via DetailBuilder
                if let Some(condition) = &field.visibility_condition {
                    self.push_line(&format!("// Conditional visibility: {}", condition));
                    self.push_line(&format!(
                        "TSharedRef<IPropertyHandle> {}Handle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED({}, {}));",
                        field.name, "TargetClass", field.name
                    ));
                    self.push_line(&format!(
                        "{}Cat.AddProperty({}Handle);",
                        cat_var, field.name
                    ));
                }
                // Otherwise default property display is automatic
            }
        }
    }
    
    fn extract_string_attr_arg(&self, attr: &kain_core::ast::Attribute) -> Option<String> {
        attr.args.first().and_then(|arg| {
            if let Expr::String(s, _) = arg {
                Some(s.clone())
            } else {
                None
            }
        })
    }
    
    /// Extract a float argument by positional index.
    /// For @slider(0.0, 1.0): index 0 = 0.0 (min), index 1 = 1.0 (max)
    fn extract_float_arg_at(&self, args: &[Expr], index: usize) -> Option<f64> {
        if let Some(arg) = args.get(index) {
            match arg {
                Expr::Float(f, _) => return Some(*f),
                Expr::Int(n, _) => return Some(*n as f64),
                Expr::Unary { op: kain_core::ast::UnaryOp::Neg, operand, .. } => {
                    match operand.as_ref() {
                        Expr::Float(f, _) => return Some(-f),
                        Expr::Int(n, _) => return Some(-(*n as f64)),
                        _ => {}
                    }
                }
                _ => {}
            }
        }
        None
    }
    
    fn extract_string_list_arg(&self, args: &[Expr], _name: &str) -> Vec<String> {
        let mut result = Vec::new();
        for arg in args {
            if let Expr::String(s, _) = arg {
                result.push(s.clone());
            }
        }
        result
    }
    
    fn map_type(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, .. } => match name.as_str() {
                "Int" | "int" => "int32".to_string(),
                "Float" | "float" => "float".to_string(),
                "Bool" | "bool" => "bool".to_string(),
                "String" | "str" => "FString".to_string(),
                "Text" => "FText".to_string(),
                "Color" => "FLinearColor".to_string(),
                "Vec2" => "FVector2D".to_string(),
                "Vec3" => "FVector".to_string(),
                "Brush" => "const FSlateBrush*".to_string(),
                "Margin" => "FMargin".to_string(),
                "Texture2D" => "UTexture2D*".to_string(),
                "StaticMesh" => "UStaticMesh*".to_string(),
                "Material" => "UMaterialInterface*".to_string(),
                _ => name.clone(),
            },
            Type::Array(element, _, _) => {
                format!("TArray<{}>", self.map_type(element))
            }
            Type::Unit(_) => "void".to_string(),
            _ => "auto".to_string(),
        }
    }
    
    fn push_line(&mut self, line: &str) {
        let indent_str = "\t".repeat(self.indent);
        self.lines.push(format!("{}{}", indent_str, line));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Attribute, Struct, Field, Type, Visibility};
    use kain_core::span::Span;
    use kain_core::types::{TypedStruct, ResolvedType};
    use std::collections::HashMap;
    
    fn s() -> Span { Span::default() }
    
    fn float_type() -> Type {
        Type::Named { name: "Float".to_string(), generics: vec![], span: s() }
    }
    
    fn make_typed_struct(st: Struct) -> TypedStruct {
        let field_types: HashMap<String, ResolvedType> = st.fields.iter()
            .map(|f| (f.name.clone(), ResolvedType::Unknown))
            .collect();
        TypedStruct {
            ast: st,
            field_types,
        }
    }
    
    #[test]
    fn test_category_grouping() {
        let st = Struct {
            name: "WeaponDetails".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "damage".to_string(),
                    ty: float_type(),
                    attributes: vec![
                        Attribute { name: "category".to_string(), args: vec![Expr::String("Weapon Stats".to_string(), s())], span: s() },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
                Field {
                    name: "fire_rate".to_string(),
                    ty: float_type(),
                    attributes: vec![],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
            ],
            methods: vec![],
            attributes: vec![Attribute { name: "details".to_string(), args: vec![], span: s() }],
            visibility: Visibility::Public,
            span: s(),
        };
        
        let gen = DetailsGenerator::new();
        let categories = gen.build_categories(&st);
        
        assert_eq!(categories.len(), 1);
        assert_eq!(categories[0].name, "Weapon Stats");
        assert_eq!(categories[0].fields.len(), 2);
    }
    
    #[test]
    fn test_slider_generation() {
        let st = Struct {
            name: "TestDetails".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "value".to_string(),
                    ty: float_type(),
                    attributes: vec![
                        Attribute { name: "category".to_string(), args: vec![Expr::String("Test".to_string(), s())], span: s() },
                        Attribute { name: "slider".to_string(), args: vec![Expr::Float(0.0, s()), Expr::Float(100.0, s())], span: s() },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
            ],
            methods: vec![],
            attributes: vec![Attribute { name: "details".to_string(), args: vec![], span: s() }],
            visibility: Visibility::Public,
            span: s(),
        };
        
        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (header, source) = gen.generate_customization(&typed_st);
        
        assert!(header.contains("FTestDetailsCustomization"));
        assert!(header.contains("IDetailCustomization"));
        assert!(source.contains("SSpinBox<float>"));
        assert!(source.contains("MinValue"));
        assert!(source.contains("MaxValue"));
    }
    
    #[test]
    fn test_button_generation() {
        let st = Struct {
            name: "TestDetails".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "reset_action".to_string(),
                    ty: Type::Unit(s()),
                    attributes: vec![
                        Attribute { name: "category".to_string(), args: vec![Expr::String("Actions".to_string(), s())], span: s() },
                        Attribute { name: "button".to_string(), args: vec![Expr::String("Reset to Defaults".to_string(), s())], span: s() },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
            ],
            methods: vec![],
            attributes: vec![Attribute { name: "details".to_string(), args: vec![], span: s() }],
            visibility: Visibility::Public,
            span: s(),
        };
        
        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (header, source) = gen.generate_customization(&typed_st);
        
        assert!(header.contains("OnButton_reset_action"));
        assert!(source.contains("SNew(SButton)"));
        assert!(source.contains("Reset to Defaults"));
    }
}

