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
    display_name: Option<String>,
    tooltip: Option<String>,
}

/// Widget override for custom property display
#[derive(Debug, Clone)]
enum WidgetOverride {
    Slider { min: f64, max: f64 },
    ColorPicker,
    AssetPicker { allowed_classes: Vec<String> },
    TextBox { multiline: bool },
    CheckBox,
    Button { label: String },
}

#[derive(Debug, Clone)]
enum VisibilityConditionExpr {
    BoolField {
        field: String,
    },
    NumericCompare {
        field: String,
        op: &'static str,
        rhs: f64,
    },
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
        let target_type_name = st
            .ast
            .name
            .strip_suffix("Details")
            .unwrap_or(&st.ast.name)
            .to_string();

        let header = self.generate_header(&st.ast, &class_name);
        let source = self.generate_source(&st.ast, &class_name, &target_type_name);

        (header, source)
    }

    /// Generate registration code for module startup
    pub fn generate_registration(&self, st: &TypedStruct) -> String {
        let class_name = format!("F{}Customization", st.ast.name);
        let target_type_name = st
            .ast
            .name
            .strip_suffix("Details")
            .unwrap_or(&st.ast.name)
            .to_string();

        format!(
            concat!(
                "\t{{\n",
                "\t\tFPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(\"PropertyEditor\");\n",
                "\t\tPropertyModule.RegisterCustomClassLayout(\n",
                "\t\t\tFName(TEXT(\"{}\")),\n",
                "\t\t\tFOnGetDetailCustomizationInstance::CreateStatic(&{}::MakeInstance)\n",
                "\t\t);\n",
                "\t}}"
            ),
            target_type_name, class_name
        )
    }

    fn generate_header(&mut self, st: &Struct, class_name: &str) -> String {
        self.lines.clear();
        self.indent = 0;

        self.push_line(&format!(
            "class {} : public IDetailCustomization",
            class_name
        ));
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
        self.push_line(
            "virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;",
        );
        self.push_line("");

        // Generate button handler declarations
        for field in &st.fields {
            if field.attributes.iter().any(|a| a.name == "button") {
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

    fn generate_source(
        &mut self,
        st: &Struct,
        class_name: &str,
        _target_type_name: &str,
    ) -> String {
        self.lines.clear();
        self.indent = 0;

        // Build category groups
        let categories = self.build_categories(st);

        // CustomizeDetails implementation
        self.push_line(&format!(
            "void {}::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)",
            class_name
        ));
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
                Self::sanitize_identifier(&category.name),
                category.name
            ));
            self.push_line("");

            for field in &category.fields {
                self.generate_field_customization(field, &category.name, class_name);
            }
        }

        // Generate method-level button actions in an "Actions" category
        let has_method_buttons = st
            .methods
            .iter()
            .any(|m| m.attributes.iter().any(|a| a.name == "button"));
        if has_method_buttons {
            self.push_line("IDetailCategoryBuilder& ActionsCat = DetailBuilder.EditCategory(TEXT(\"Actions\"));");
            for method in &st.methods {
                if let Some(button_attr) = method.attributes.iter().find(|a| a.name == "button") {
                    let label = self
                        .extract_string_attr_arg(button_attr)
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

                // Implement button action with property change notification
                self.push_line("// Execute button action");
                self.push_line(&format!(
                    "UE_LOG(LogTemp, Log, TEXT(\"Details button '{}' clicked\"));",
                    field.name
                ));
                self.push_line("");
                self.push_line("// Notify property change if object is valid");
                self.push_line("if (CachedObject.IsValid())");
                self.push_line("{");
                self.indent += 1;
                self.push_line("CachedObject->Modify();");
                self.push_line("CachedObject->PostEditChange();");
                self.indent -= 1;
                self.push_line("}");
                self.push_line("");

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

                // Implement button action with property change notification
                self.push_line("// Execute button action");
                self.push_line(&format!(
                    "UE_LOG(LogTemp, Log, TEXT(\"Details action '{}' executed\"));",
                    method.name
                ));
                self.push_line("");
                self.push_line("// Notify property change if object is valid");
                self.push_line("if (CachedObject.IsValid())");
                self.push_line("{");
                self.indent += 1;
                self.push_line("CachedObject->Modify();");
                self.push_line("CachedObject->PostEditChange();");
                self.indent -= 1;
                self.push_line("}");
                self.push_line("");

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
                display_name: self.detect_display_name(field),
                tooltip: self.detect_tooltip(field),
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
                    return Some(WidgetOverride::AssetPicker {
                        allowed_classes: classes,
                    });
                }
                "text_box" => {
                    return Some(WidgetOverride::TextBox { multiline: false });
                }
                "multiline_text" => {
                    return Some(WidgetOverride::TextBox { multiline: true });
                }
                "checkbox" => {
                    return Some(WidgetOverride::CheckBox);
                }
                "button" => {
                    let label = self
                        .extract_string_attr_arg(attr)
                        .unwrap_or_else(|| field.name.clone());
                    return Some(WidgetOverride::Button { label });
                }
                _ => {}
            }
        }
        None
    }

    fn detect_visibility_condition(&self, field: &Field) -> Option<String> {
        field
            .attributes
            .iter()
            .find(|a| a.name == "visible_if")
            .and_then(|a| self.extract_string_attr_arg(a))
    }

    fn detect_display_name(&self, field: &Field) -> Option<String> {
        field
            .attributes
            .iter()
            .find(|a| a.name == "display_name")
            .and_then(|a| self.extract_string_attr_arg(a))
    }

    fn detect_tooltip(&self, field: &Field) -> Option<String> {
        field
            .attributes
            .iter()
            .find(|a| a.name == "tooltip")
            .and_then(|a| self.extract_string_attr_arg(a))
    }

    fn parse_visibility_condition(condition: &str) -> Option<VisibilityConditionExpr> {
        let cond = condition.trim();
        if cond.is_empty() {
            return None;
        }

        for op in [">=", "<=", "==", "!=", ">", "<"] {
            if let Some(idx) = cond.find(op) {
                let left = cond[..idx].trim();
                let right = cond[idx + op.len()..].trim();
                if Self::is_simple_identifier(left) {
                    if let Ok(rhs) = right.parse::<f64>() {
                        return Some(VisibilityConditionExpr::NumericCompare {
                            field: left.to_string(),
                            op,
                            rhs,
                        });
                    }
                }
            }
        }

        if Self::is_simple_identifier(cond) {
            return Some(VisibilityConditionExpr::BoolField {
                field: cond.to_string(),
            });
        }

        None
    }

    fn is_simple_identifier(text: &str) -> bool {
        let mut chars = text.chars();
        match chars.next() {
            Some(c) if c == '_' || c.is_ascii_alphabetic() => {}
            _ => return false,
        }
        chars.all(|c| c == '_' || c.is_ascii_alphanumeric())
    }

    fn emit_visibility_for_custom_row(&mut self, visibility_condition: Option<&str>) {
        if let Some(condition) = visibility_condition {
            if let Some(lambda) = Self::visibility_lambda_expr(condition) {
                self.push_line(&format!(".Visibility({})", lambda));
            }
        }
    }

    fn emit_property_tooltip(&mut self, handle_var: &str, tooltip: Option<&str>) {
        if let Some(tooltip_text) = tooltip {
            self.push_line(&format!(
                "{}->SetToolTipText(FText::FromString(TEXT(\"{}\")));",
                handle_var, tooltip_text
            ));
        }
    }

    fn emit_visibility_for_property_row(
        &mut self,
        row_var: &str,
        visibility_condition: Option<&str>,
    ) {
        if let Some(condition) = visibility_condition {
            if let Some(lambda) = Self::visibility_lambda_expr(condition) {
                self.push_line(&format!("{}.Visibility({});", row_var, lambda));
            }
        }
    }

    fn visibility_lambda_expr(condition: &str) -> Option<String> {
        match Self::parse_visibility_condition(condition) {
            Some(VisibilityConditionExpr::BoolField { field }) => Some(format!(
                "TAttribute<EVisibility>::CreateLambda([&DetailBuilder]() -> EVisibility {{ bool bVisible = false; TSharedRef<IPropertyHandle> VisibleIfHandle = DetailBuilder.GetProperty(TEXT(\"{}\")); if (VisibleIfHandle->GetValue(bVisible) == FPropertyAccess::Success && bVisible) {{ return EVisibility::Visible; }} return EVisibility::Collapsed; }})",
                field
            )),
            Some(VisibilityConditionExpr::NumericCompare { field, op, rhs }) => {
                let rhs_str = if rhs.fract() == 0.0 {
                    format!("{:.1}", rhs)
                } else {
                    rhs.to_string()
                };
                Some(format!(
                    "TAttribute<EVisibility>::CreateLambda([&DetailBuilder]() -> EVisibility {{ TSharedRef<IPropertyHandle> VisibleIfHandle = DetailBuilder.GetProperty(TEXT(\"{}\")); FString CondText; if (VisibleIfHandle->GetValueAsFormattedString(CondText) == FPropertyAccess::Success) {{ const double CondValue = FCString::Atod(*CondText); if (CondValue {} {}) {{ return EVisibility::Visible; }} }} return EVisibility::Collapsed; }})",
                    field,
                    op,
                    rhs_str
                ))
            }
            None => None,
        }
    }

    fn generate_field_customization(
        &mut self,
        field: &DetailField,
        category_name: &str,
        class_name: &str,
    ) {
        let cat_var = Self::sanitize_identifier(category_name);
        let handle_var = format!("{}Handle", field.name);
        let display_label = field.display_name.as_deref().unwrap_or(&field.name);
        match &field.widget_override {
            Some(WidgetOverride::Slider { min, max }) => {
                self.push_line(&format!(
                    "// Custom slider for {} (bound to property)",
                    field.name
                ));
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, display_label
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    display_label
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                if field.cpp_type == "int32" || field.cpp_type == "int64" {
                    let min_val = *min as i32;
                    let max_val = *max as i32;
                    self.push_line(&format!(
                        "SNew(SSpinBox<int32>)\n\t\t.MinValue({})\n\t\t.MaxValue({})\n\t\t.Value_Lambda([{}]() -> int32 {{\n\t\t\tint32 Val = 0;\n\t\t\t{}->GetValue(Val);\n\t\t\treturn Val;\n\t\t}})\n\t\t.OnValueChanged_Lambda([{}](int32 NewVal) {{\n\t\t\t{}->SetValue(NewVal);\n\t\t}})",
                        min_val,
                        max_val,
                        handle_var,
                        handle_var,
                        handle_var,
                        handle_var
                    ));
                } else {
                    let min_str = if min.fract() == 0.0 {
                        format!("{:.1}", min)
                    } else {
                        format!("{}", min)
                    };
                    let max_str = if max.fract() == 0.0 {
                        format!("{:.1}", max)
                    } else {
                        format!("{}", max)
                    };
                    self.push_line(&format!(
                        "SNew(SSpinBox<float>)\n\t\t.MinValue({}f)\n\t\t.MaxValue({}f)\n\t\t.Value_Lambda([{}]() -> float {{\n\t\t\tfloat Val = 0.0f;\n\t\t\t{}->GetValue(Val);\n\t\t\treturn Val;\n\t\t}})\n\t\t.OnValueChanged_Lambda([{}](float NewVal) {{\n\t\t\t{}->SetValue(NewVal);\n\t\t}})",
                        min_str,
                        max_str,
                        handle_var,
                        handle_var,
                        handle_var,
                        handle_var
                    ));
                }
                self.indent -= 1;
                self.push_line("]");
                self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                self.push_line(";");
                self.push_line("");
            }
            Some(WidgetOverride::ColorPicker) => {
                self.push_line(&format!(
                    "// Color picker for {} (bound to property)",
                    field.name
                ));
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                if field.cpp_type == "FLinearColor" || field.cpp_type == "FColor" {
                    self.push_line(&format!(
                        "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                        cat_var, display_label
                    ));
                    self.push_line(".NameContent()");
                    self.push_line("[");
                    self.indent += 1;
                    self.push_line(&format!(
                        "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                        display_label
                    ));
                    self.indent -= 1;
                    self.push_line("]");
                    self.push_line(".ValueContent()");
                    self.push_line("[");
                    self.indent += 1;
                    self.push_line(&format!(
                        "SNew(SColorBlock)\n\t\t.Color_Lambda([{}]() -> FLinearColor {{\n\t\t\tFLinearColor Val = FLinearColor::White;\n\t\t\t{}->GetValue(Val);\n\t\t\treturn Val;\n\t\t}})",
                        handle_var, handle_var
                    ));
                    self.indent -= 1;
                    self.push_line("]");
                    self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                    self.push_line(";");
                } else {
                    let row_var = format!("{}Row", field.name);
                    self.push_line(&format!(
                        "auto& {} = {}Cat.AddProperty({});",
                        row_var, cat_var, handle_var
                    ));
                    self.emit_visibility_for_property_row(
                        &row_var,
                        field.visibility_condition.as_deref(),
                    );
                }
                self.push_line("");
            }
            Some(WidgetOverride::AssetPicker { allowed_classes }) => {
                self.push_line(&format!(
                    "// Asset picker for {} (bound to property)",
                    field.name
                ));
                let classes_str = allowed_classes.join(", ");
                self.push_line(&format!("// Allowed classes: {}", classes_str));
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, display_label
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    display_label
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
                self.push_line(&format!(".PropertyHandle({})", handle_var));
                self.indent -= 1;
                self.push_line("]");
                self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                self.push_line(";");
                self.push_line("");
            }
            Some(WidgetOverride::TextBox { multiline }) => {
                self.push_line(&format!(
                    "// Text box for {} (bound to property)",
                    field.name
                ));
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, display_label
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    display_label
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                if *multiline {
                    self.push_line(&format!(
                        "SNew(SMultiLineEditableTextBox)\n\t\t.Text_Lambda([{}]() -> FText {{\n\t\t\tFString Val;\n\t\t\t{}->GetValue(Val);\n\t\t\treturn FText::FromString(Val);\n\t\t}})\n\t\t.OnTextCommitted_Lambda([{}](const FText& NewText, ETextCommit::Type) {{\n\t\t\t{}->SetValueFromFormattedString(NewText.ToString());\n\t\t}})",
                        handle_var,
                        handle_var,
                        handle_var,
                        handle_var
                    ));
                } else {
                    self.push_line(&format!(
                        "SNew(SEditableTextBox)\n\t\t.Text_Lambda([{}]() -> FText {{\n\t\t\tFString Val;\n\t\t\t{}->GetValue(Val);\n\t\t\treturn FText::FromString(Val);\n\t\t}})\n\t\t.OnTextCommitted_Lambda([{}](const FText& NewText, ETextCommit::Type) {{\n\t\t\t{}->SetValueFromFormattedString(NewText.ToString());\n\t\t}})",
                        handle_var,
                        handle_var,
                        handle_var,
                        handle_var
                    ));
                }
                self.indent -= 1;
                self.push_line("]");
                self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                self.push_line(";");
                self.push_line("");
            }
            Some(WidgetOverride::CheckBox) => {
                self.push_line(&format!(
                    "// Checkbox for {} (bound to property)",
                    field.name
                ));
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                self.push_line(&format!(
                    "{}Cat.AddCustomRow(FText::FromString(TEXT(\"{}\")))",
                    cat_var, display_label
                ));
                self.push_line(".NameContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(STextBlock).Text(FText::FromString(TEXT(\"{}\")))",
                    display_label
                ));
                self.indent -= 1;
                self.push_line("]");
                self.push_line(".ValueContent()");
                self.push_line("[");
                self.indent += 1;
                self.push_line(&format!(
                    "SNew(SCheckBox)\n\t\t.IsChecked_Lambda([{}]() -> ECheckBoxState {{\n\t\t\tbool bVal = false;\n\t\t\t{}->GetValue(bVal);\n\t\t\treturn bVal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;\n\t\t}})\n\t\t.OnCheckStateChanged_Lambda([{}](ECheckBoxState NewState) {{\n\t\t\t{}->SetValue(NewState == ECheckBoxState::Checked);\n\t\t}})",
                    handle_var,
                    handle_var,
                    handle_var,
                    handle_var
                ));
                self.indent -= 1;
                self.push_line("]");
                self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                self.push_line(";");
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
                self.push_line("]");
                self.emit_visibility_for_custom_row(field.visibility_condition.as_deref());
                self.push_line(";");
                self.push_line("");
            }
            None => {
                self.push_line(&format!(
                    "TSharedRef<IPropertyHandle> {} = DetailBuilder.GetProperty(TEXT(\"{}\"));",
                    handle_var, field.name
                ));
                self.emit_property_tooltip(&handle_var, field.tooltip.as_deref());
                let row_var = format!("{}Row", field.name);
                self.push_line(&format!(
                    "auto& {} = {}Cat.AddProperty({});",
                    row_var, cat_var, handle_var
                ));
                self.emit_visibility_for_property_row(
                    &row_var,
                    field.visibility_condition.as_deref(),
                );
                self.push_line("");
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
                Expr::Unary {
                    op: kain_core::ast::UnaryOp::Neg,
                    operand,
                    ..
                } => match operand.as_ref() {
                    Expr::Float(f, _) => return Some(-f),
                    Expr::Int(n, _) => return Some(-(*n as f64)),
                    _ => {}
                },
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

    /// Convert a category display name into a valid C++ identifier.
    /// Strips namespace prefix ("ToonShaderz|Colors" → "Colors"), then replaces
    /// spaces and any non-alphanumeric character with `_`.
    fn sanitize_identifier(s: &str) -> String {
        let base = s.rsplit('|').next().unwrap_or(s).trim();
        let ident: String = base
            .chars()
            .map(|c| {
                if c.is_alphanumeric() || c == '_' {
                    c
                } else {
                    '_'
                }
            })
            .collect();
        if ident.is_empty() {
            return "Category".to_string();
        }
        if ident.starts_with(|c: char| c.is_ascii_digit()) {
            format!("_{}", ident)
        } else {
            ident
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Attribute, Field, Struct, Type, Visibility};
    use kain_core::span::Span;
    use kain_core::types::{ResolvedType, TypedStruct};
    use std::collections::HashMap;

    fn s() -> Span {
        Span::default()
    }

    fn float_type() -> Type {
        Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: s(),
        }
    }

    fn make_typed_struct(st: Struct) -> TypedStruct {
        let field_types: HashMap<String, ResolvedType> = st
            .fields
            .iter()
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
                    attributes: vec![Attribute {
                        name: "category".to_string(),
                        args: vec![Expr::String("Weapon Stats".to_string(), s())],
                        span: s(),
                    }],
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
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
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
            fields: vec![Field {
                name: "value".to_string(),
                ty: float_type(),
                attributes: vec![
                    Attribute {
                        name: "category".to_string(),
                        args: vec![Expr::String("Test".to_string(), s())],
                        span: s(),
                    },
                    Attribute {
                        name: "slider".to_string(),
                        args: vec![Expr::Float(0.0, s()), Expr::Float(100.0, s())],
                        span: s(),
                    },
                ],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: s(),
            }],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
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
        assert!(
            source.contains("DetailBuilder.GetProperty(TEXT(\"value\"))"),
            "Slider should generate property handle lookup by name. Got:\n{}",
            source
        );
        assert!(
            source.contains("Value_Lambda"),
            "Slider should bind Value via lambda. Got:\n{}",
            source
        );
        assert!(
            source.contains("OnValueChanged_Lambda"),
            "Slider should bind OnValueChanged via lambda. Got:\n{}",
            source
        );
        assert!(
            source.contains("GetValue(Val)"),
            "Slider getter should call GetValue. Got:\n{}",
            source
        );
        assert!(
            source.contains("SetValue(NewVal)"),
            "Slider setter should call SetValue. Got:\n{}",
            source
        );
    }

    #[test]
    fn test_default_property_binding() {
        let st = Struct {
            name: "WeaponDetails".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "damage".to_string(),
                ty: float_type(),
                attributes: vec![Attribute {
                    name: "category".to_string(),
                    args: vec![Expr::String("Stats".to_string(), s())],
                    span: s(),
                }],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: s(),
            }],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (_, source) = gen.generate_customization(&typed_st);

        assert!(
            source.contains("DetailBuilder.GetProperty(TEXT(\"damage\"))"),
            "Default property should use property lookup by name. Got:\n{}",
            source
        );
        assert!(
            source.contains("AddProperty(damageHandle)"),
            "Default property should be added via handle. Got:\n{}",
            source
        );
    }

    #[test]
    fn test_color_picker_binding() {
        let st = Struct {
            name: "TestDetails".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "tint_color".to_string(),
                ty: Type::Named {
                    name: "Color".to_string(),
                    generics: vec![],
                    span: s(),
                },
                attributes: vec![
                    Attribute {
                        name: "category".to_string(),
                        args: vec![Expr::String("Visual".to_string(), s())],
                        span: s(),
                    },
                    Attribute {
                        name: "color_picker".to_string(),
                        args: vec![],
                        span: s(),
                    },
                ],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: s(),
            }],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (_, source) = gen.generate_customization(&typed_st);

        assert!(
            source.contains("DetailBuilder.GetProperty(TEXT(\"tint_color\"))"),
            "Color picker should generate property handle lookup by name. Got:\n{}",
            source
        );
        assert!(
            source.contains("Color_Lambda"),
            "Color picker should bind Color via lambda. Got:\n{}",
            source
        );
        assert!(
            source.contains("FLinearColor"),
            "Color picker should use FLinearColor. Got:\n{}",
            source
        );
    }

    #[test]
    fn test_button_generation() {
        let st = Struct {
            name: "TestDetails".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "reset_action".to_string(),
                ty: Type::Unit(s()),
                attributes: vec![
                    Attribute {
                        name: "category".to_string(),
                        args: vec![Expr::String("Actions".to_string(), s())],
                        span: s(),
                    },
                    Attribute {
                        name: "button".to_string(),
                        args: vec![Expr::String("Reset to Defaults".to_string(), s())],
                        span: s(),
                    },
                ],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: s(),
            }],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
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

    #[test]
    fn test_visible_if_generates_visibility_lambda() {
        let st = Struct {
            name: "VisualDetails".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "emissive_strength".to_string(),
                    ty: float_type(),
                    attributes: vec![Attribute {
                        name: "category".to_string(),
                        args: vec![Expr::String("Visual".to_string(), s())],
                        span: s(),
                    }],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
                Field {
                    name: "emissive_color".to_string(),
                    ty: Type::Named {
                        name: "Color".to_string(),
                        generics: vec![],
                        span: s(),
                    },
                    attributes: vec![
                        Attribute {
                            name: "category".to_string(),
                            args: vec![Expr::String("Visual".to_string(), s())],
                            span: s(),
                        },
                        Attribute {
                            name: "visible_if".to_string(),
                            args: vec![Expr::String("emissive_strength > 0.0".to_string(), s())],
                            span: s(),
                        },
                        Attribute {
                            name: "color_picker".to_string(),
                            args: vec![],
                            span: s(),
                        },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
            ],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (_, source) = gen.generate_customization(&typed_st);

        assert!(
            source.contains(".Visibility(TAttribute<EVisibility>::CreateLambda"),
            "visible_if should emit a real visibility lambda. Got:\n{}",
            source
        );
        assert!(
            source.contains("FCString::Atod"),
            "numeric visible_if should parse numeric property text for comparison. Got:\n{}",
            source
        );
        assert!(
            source.contains("CondValue > 0.0"),
            "numeric comparator should be preserved in generated visibility check. Got:\n{}",
            source
        );
    }

    #[test]
    fn test_textbox_and_checkbox_overrides_generate_custom_widgets() {
        let st = Struct {
            name: "FormDetails".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "title".to_string(),
                    ty: Type::Named {
                        name: "String".to_string(),
                        generics: vec![],
                        span: s(),
                    },
                    attributes: vec![
                        Attribute {
                            name: "category".to_string(),
                            args: vec![Expr::String("Form".to_string(), s())],
                            span: s(),
                        },
                        Attribute {
                            name: "text_box".to_string(),
                            args: vec![],
                            span: s(),
                        },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
                Field {
                    name: "notes".to_string(),
                    ty: Type::Named {
                        name: "String".to_string(),
                        generics: vec![],
                        span: s(),
                    },
                    attributes: vec![
                        Attribute {
                            name: "category".to_string(),
                            args: vec![Expr::String("Form".to_string(), s())],
                            span: s(),
                        },
                        Attribute {
                            name: "multiline_text".to_string(),
                            args: vec![],
                            span: s(),
                        },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
                Field {
                    name: "enabled".to_string(),
                    ty: Type::Named {
                        name: "Bool".to_string(),
                        generics: vec![],
                        span: s(),
                    },
                    attributes: vec![
                        Attribute {
                            name: "category".to_string(),
                            args: vec![Expr::String("Form".to_string(), s())],
                            span: s(),
                        },
                        Attribute {
                            name: "checkbox".to_string(),
                            args: vec![],
                            span: s(),
                        },
                    ],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: s(),
                },
            ],
            methods: vec![],
            attributes: vec![Attribute {
                name: "details".to_string(),
                args: vec![],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed_st = make_typed_struct(st);
        let mut gen = DetailsGenerator::new();
        let (_, source) = gen.generate_customization(&typed_st);

        assert!(
            source.contains("SEditableTextBox"),
            "text_box override should emit SEditableTextBox. Got:\n{}",
            source
        );
        assert!(
            source.contains("SMultiLineEditableTextBox"),
            "multiline_text override should emit SMultiLineEditableTextBox. Got:\n{}",
            source
        );
        assert!(
            source.contains("SCheckBox"),
            "checkbox override should emit SCheckBox. Got:\n{}",
            source
        );
        assert!(
            source.contains("OnCheckStateChanged_Lambda"),
            "checkbox should emit state-change binding. Got:\n{}",
            source
        );
    }
}
