//! Slate Widget Generation with Smart Slot Awareness
//!
//! This module generates production-ready Slate UI code from KAIN structs.
//! Key features:
//! - Parent stack tracking for correct slot types
//! - Automatic list view generation (SListView, STreeView)
//! - Full declarative syntax support
//! - Slot configuration (padding, alignment, fill)
//! - Event handler generation

#![allow(dead_code, unused_variables)]

use kain_core::ast::{Attribute, Block, ElseBranch, Expr, Function, Pattern, Stmt, Struct, Type};
use kain_core::types::TypedStruct;
use std::collections::HashMap;

use crate::editor::reactive::{LayoutOptimizer, PropertyReactivity};
use ue5::ue5::naming;

/// Widget type information for slot generation
#[derive(Debug, Clone, PartialEq)]
pub enum WidgetType {
    // Layout containers
    VerticalBox,
    HorizontalBox,
    GridPanel,
    UniformGridPanel,
    ScrollBox,
    Border,
    Overlay,
    Splitter,
    WrapBox,
    Canvas,
    // Interactive widgets
    Button,
    CheckBox,
    ComboBox,
    EditableTextBox,
    Slider,
    SpinBox,
    ColorBlock,
    // Display widgets
    TextBlock,
    Image,
    ProgressBar,
    Separator,
    ToolTip,
    // List widgets
    ListView,
    TreeView,
    TileView,
    TableRow,
    // Menu widgets
    MenuAnchor,
    // Unknown fallback
    Unknown(String),
}

impl WidgetType {
    pub fn from_name(name: &str) -> Self {
        match name {
            "VerticalBox" | "SVerticalBox" => WidgetType::VerticalBox,
            "HorizontalBox" | "SHorizontalBox" => WidgetType::HorizontalBox,
            "GridPanel" | "SGridPanel" => WidgetType::GridPanel,
            "UniformGridPanel" | "SUniformGridPanel" => WidgetType::UniformGridPanel,
            "ScrollBox" | "SScrollBox" => WidgetType::ScrollBox,
            "Border" | "SBorder" => WidgetType::Border,
            "Overlay" | "SOverlay" => WidgetType::Overlay,
            "Splitter" | "SSplitter" => WidgetType::Splitter,
            "WrapBox" | "SWrapBox" => WidgetType::WrapBox,
            "Canvas" | "SCanvas" => WidgetType::Canvas,
            "Button" | "SButton" => WidgetType::Button,
            "CheckBox" | "SCheckBox" => WidgetType::CheckBox,
            "ComboBox" | "SComboBox" => WidgetType::ComboBox,
            "EditableTextBox" | "SEditableTextBox" => WidgetType::EditableTextBox,
            "Slider" | "SSlider" => WidgetType::Slider,
            "SpinBox" | "SSpinBox" => WidgetType::SpinBox,
            "ColorBlock" | "SColorBlock" | "ColorPicker" | "SColorPicker" => WidgetType::ColorBlock,
            "TextBlock" | "STextBlock" => WidgetType::TextBlock,
            "Image" | "SImage" => WidgetType::Image,
            "ProgressBar" | "SProgressBar" => WidgetType::ProgressBar,
            "Separator" | "SSeparator" => WidgetType::Separator,
            "ToolTip" | "SToolTip" => WidgetType::ToolTip,
            "ListView" | "SListView" => WidgetType::ListView,
            "TreeView" | "STreeView" => WidgetType::TreeView,
            "TileView" | "STileView" => WidgetType::TileView,
            "TableRow" | "STableRow" => WidgetType::TableRow,
            "MenuAnchor" | "SMenuAnchor" => WidgetType::MenuAnchor,
            _ => WidgetType::Unknown(name.to_string()),
        }
    }

    pub fn to_slate_class(&self) -> String {
        match self {
            WidgetType::VerticalBox => "SVerticalBox".to_string(),
            WidgetType::HorizontalBox => "SHorizontalBox".to_string(),
            WidgetType::GridPanel => "SGridPanel".to_string(),
            WidgetType::UniformGridPanel => "SUniformGridPanel".to_string(),
            WidgetType::ScrollBox => "SScrollBox".to_string(),
            WidgetType::Border => "SBorder".to_string(),
            WidgetType::Overlay => "SOverlay".to_string(),
            WidgetType::Splitter => "SSplitter".to_string(),
            WidgetType::WrapBox => "SWrapBox".to_string(),
            WidgetType::Canvas => "SCanvas".to_string(),
            WidgetType::Button => "SButton".to_string(),
            WidgetType::CheckBox => "SCheckBox".to_string(),
            WidgetType::ComboBox => "SComboBox<TSharedPtr<FString>>".to_string(),
            WidgetType::EditableTextBox => "SEditableTextBox".to_string(),
            WidgetType::Slider => "SSlider".to_string(),
            WidgetType::SpinBox => "SSpinBox<float>".to_string(),
            WidgetType::ColorBlock => "SColorBlock".to_string(),
            WidgetType::TextBlock => "STextBlock".to_string(),
            WidgetType::Image => "SImage".to_string(),
            WidgetType::ProgressBar => "SProgressBar".to_string(),
            WidgetType::Separator => "SSeparator".to_string(),
            WidgetType::ToolTip => "SToolTip".to_string(),
            WidgetType::ListView => "SListView".to_string(),
            WidgetType::TreeView => "STreeView".to_string(),
            WidgetType::TileView => "STileView".to_string(),
            WidgetType::TableRow => "STableRow".to_string(),
            WidgetType::MenuAnchor => "SMenuAnchor".to_string(),
            WidgetType::Unknown(name) => format!("S{}", name),
        }
    }

    pub fn has_slots(&self) -> bool {
        matches!(
            self,
            WidgetType::VerticalBox
                | WidgetType::HorizontalBox
                | WidgetType::GridPanel
                | WidgetType::UniformGridPanel
                | WidgetType::ScrollBox
                | WidgetType::Overlay
                | WidgetType::Splitter
                | WidgetType::WrapBox
                | WidgetType::Canvas
        )
    }

    /// Whether this widget is a list-type that needs type parameters
    pub fn is_list_widget(&self) -> bool {
        matches!(
            self,
            WidgetType::ListView | WidgetType::TreeView | WidgetType::TileView
        )
    }

    /// Whether this widget has content slot (single child)
    pub fn has_content_slot(&self) -> bool {
        matches!(
            self,
            WidgetType::Border | WidgetType::Button | WidgetType::ToolTip | WidgetType::MenuAnchor
        )
    }
}

/// Slot configuration for layout widgets
#[derive(Debug, Clone, Default)]
pub struct SlotConfig {
    pub padding: Option<String>,
    pub h_align: Option<String>,
    pub v_align: Option<String>,
    pub fill_width: Option<String>,
    pub fill_height: Option<String>,
    pub auto_width: bool,
    pub auto_height: bool,
    pub max_width: Option<String>,
    pub max_height: Option<String>,
    // Grid support
    pub column: Option<i32>,
    pub row: Option<i32>,
    pub column_span: Option<i32>,
    pub row_span: Option<i32>,
    // Canvas support
    pub position: Option<String>,
    pub size: Option<String>,
}

/// Shader brush tracked for generation
struct ShaderBrush {
    id: usize,
    material_expr: Expr,
    size_expr: Expr,
}

/// Widget information tracked in symbol table
#[derive(Debug, Clone)]
struct WidgetInfo {
    /// The initial widget construction expression (e.g., VerticalBox())
    construction: Expr,
    /// All method calls made on this widget variable
    method_calls: Vec<MethodCallInfo>,
}

/// Method call information for widget tree reconstruction
#[derive(Debug, Clone)]
struct MethodCallInfo {
    method: String,
    args: Vec<kain_core::ast::CallArg>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum SlateConstructKind {
    Compound,
    TableRow,
    ToolTip,
}

#[derive(Debug, Clone)]
struct SlateWidgetModel {
    base_class: String,
    construct_kind: SlateConstructKind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum SpecialSlateMethodKind {
    GenerateWidgetForColumn,
    OnOpening,
}

impl SlateWidgetModel {
    fn compound() -> Self {
        Self {
            base_class: "SCompoundWidget".to_string(),
            construct_kind: SlateConstructKind::Compound,
        }
    }
}

/// Slate widget generator with hierarchy tracking
pub struct SlateGenerator {
    /// Stack of parent widget types for slot generation
    parent_stack: Vec<WidgetType>,
    /// Current indentation level
    indent: usize,
    /// Generated code lines
    lines: Vec<String>,
    /// Slot configurations by widget path
    #[allow(dead_code)]
    slot_configs: HashMap<String, SlotConfig>,
    /// Counter for shader brushes to generate unique member names
    shader_brush_counter: usize,
    /// UE5 context for type mapping (optional, for proper type resolution)
    context: Option<ue5::ue5::Ue5Context>,
    /// Struct field names for the current widget being generated
    /// Used during Construct impl to resolve field references to InArgs._fieldname
    struct_field_names: std::collections::HashSet<String>,
    /// Map of field names to their resolved C++ types (populated during generate_slate_args)
    /// Used to check delegate type compatibility (e.g. FOnClicked vs FOnTestRun)
    field_type_map: HashMap<String, String>,
    /// Map of delegate field names to their parameter C++ types
    /// e.g. "on_category_changed" -> vec!["EEToolCategory"]
    /// Used by emit_delegate_bridge_or_passthrough to generate correct Broadcast() args
    delegate_param_map: HashMap<String, Vec<String>>,
    /// Bug-2 fix: maps array field name -> element C++ type for SListView<T> template args.
    /// Populated in generate_list_view_support() before generate_compose_body() runs.
    list_item_types: HashMap<String, String>,
    /// Current generated widget class name (e.g. SInventoryPanel).
    /// Used for CreateSP bindings instead of emitting invalid `Self::` references.
    widget_class_name: Option<String>,
}

impl SlateGenerator {
    pub fn new() -> Self {
        Self {
            parent_stack: Vec::new(),
            indent: 0,
            lines: Vec::new(),
            slot_configs: HashMap::new(),
            shader_brush_counter: 0,
            context: None,
            struct_field_names: std::collections::HashSet::new(),
            field_type_map: HashMap::new(),
            delegate_param_map: HashMap::new(),
            list_item_types: HashMap::new(),
            widget_class_name: None,
        }
    }

    pub fn with_context(mut self, context: ue5::ue5::Ue5Context) -> Self {
        self.context = Some(context);
        self
    }

    fn resolve_widget_model(&self, st: &Struct) -> SlateWidgetModel {
        let mut model = SlateWidgetModel::compound();

        if let Some(base_attr) = st
            .attributes
            .iter()
            .find(|a| a.name == "base" || a.name == "slate_base")
        {
            if let Some(base_name) = Self::attr_first_arg_text(base_attr) {
                model.base_class = base_name;
            }
        }

        if let Some(row_attr) = st
            .attributes
            .iter()
            .find(|a| a.name == "table_row" || a.name == "multi_column_row")
        {
            let item_type = row_attr
                .args
                .first()
                .and_then(|arg| match arg {
                    Expr::String(s, _) => Some(s.clone()),
                    Expr::Ident(name, _) => {
                        if name.starts_with('F') || name.starts_with('T') || name.contains("::") {
                            Some(name.clone())
                        } else {
                            Some(naming::to_struct_name(name))
                        }
                    }
                    _ => None,
                })
                .unwrap_or_else(|| "FString".to_string());

            model.base_class = format!("SMultiColumnTableRow<TSharedPtr<{}>>", item_type);
            model.construct_kind = SlateConstructKind::TableRow;
            return model;
        }

        if st
            .attributes
            .iter()
            .any(|a| a.name == "tooltip_widget" || a.name == "slate_tooltip")
        {
            model.base_class = "SToolTip".to_string();
            model.construct_kind = SlateConstructKind::ToolTip;
            return model;
        }

        if model.base_class.contains("SMultiColumnTableRow") {
            model.construct_kind = SlateConstructKind::TableRow;
        } else if model.base_class.ends_with("SToolTip") || model.base_class == "SToolTip" {
            model.construct_kind = SlateConstructKind::ToolTip;
        }

        model
    }

    fn attr_first_arg_text(attr: &Attribute) -> Option<String> {
        let first = attr.args.first()?;
        match first {
            Expr::String(s, _) => Some(s.clone()),
            Expr::Ident(name, _) => Some(name.clone()),
            _ => None,
        }
    }

    /// Register delegate parameter types from the program's type aliases.
    /// This allows the delegate bridge to generate correct Broadcast() calls
    /// with default-constructed parameter values when bridging from parameterless
    /// native delegates (e.g. FOnClicked) to parameterized custom delegates.
    pub fn register_delegate_params(&mut self, field_name: &str, param_cpp_types: Vec<String>) {
        self.delegate_param_map
            .insert(field_name.to_string(), param_cpp_types);
    }

    /// Scan for shader_image() calls in Compose to generate brush members
    fn scan_for_shader_brushes(&self, st: &TypedStruct) -> Vec<ShaderBrush> {
        let mut brushes = Vec::new();

        if let Some(compose_fn) = st.ast.methods.iter().find(|m| m.name == "Compose") {
            if let Some(last_stmt) = compose_fn.body.stmts.last() {
                let expr_opt = match last_stmt {
                    Stmt::Expr(expr) => Some(expr),
                    Stmt::Return(Some(expr), _) => Some(expr),
                    _ => None,
                };

                if let Some(expr) = expr_opt {
                    self.visit_expr_for_brushes(expr, &mut brushes);
                }
            }
        }

        brushes
    }

    fn visit_expr_for_brushes(&self, expr: &Expr, brushes: &mut Vec<ShaderBrush>) {
        match expr {
            Expr::Call { callee, args, .. } => {
                // Check if it's shader_image(...)
                if let Expr::Ident(name, _) = &**callee {
                    if name == "shader_image" {
                        if args.len() >= 2 {
                            brushes.push(ShaderBrush {
                                id: brushes.len(),
                                material_expr: args[0].value.clone(),
                                size_expr: args[1].value.clone(),
                            });
                            return;
                        }
                    }
                }

                // Recurse children
                for arg in args {
                    self.visit_expr_for_brushes(&arg.value, brushes);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.visit_expr_for_brushes(receiver, brushes);
                for arg in args {
                    self.visit_expr_for_brushes(&arg.value, brushes);
                }
            }
            _ => {}
        }
    }

    pub fn generate_widget(&mut self, st: &TypedStruct) -> String {
        self.lines.clear();

        let widget_name = format!("S{}", st.ast.name);
        self.widget_class_name = Some(widget_name.clone());
        let widget_model = self.resolve_widget_model(&st.ast);

        // Generate class declaration
        self.push_line(&format!(
            "class {} : public {}",
            widget_name, widget_model.base_class
        ));
        self.push_line("{");
        self.push_line("public:");
        self.indent += 1;

        if widget_model.construct_kind == SlateConstructKind::TableRow {
            self.push_line(&format!(
                "using FSuperRowType = {};",
                widget_model.base_class
            ));
            self.push_line("");
        }

        // Generate SLATE_BEGIN_ARGS
        self.generate_slate_args(&st.ast, &widget_name);

        // Generate Construct declaration
        match widget_model.construct_kind {
            SlateConstructKind::TableRow => {
                self.push_line("void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);");
            }
            _ => self.push_line("void Construct(const FArguments& InArgs);"),
        }

        // Generate event handlers if any
        self.generate_event_handlers(&st.ast);

        // Generate list view support if needed
        if self.has_list_data(&st.ast) {
            self.generate_list_view_support(&st.ast);
        }

        // Generate shader brushes members
        let brushes = self.scan_for_shader_brushes(st);
        if !brushes.is_empty() {
            self.push_line("");
            self.push_line("// Shader Brushes");
            for brush in brushes {
                self.push_line(&format!(
                    "TSharedPtr<FSlateImageBrush> ShaderBrush_{};",
                    brush.id
                ));
            }
        }

        self.indent -= 1;
        self.push_line("};");

        self.lines.join("\n")
    }

    pub fn generate_construct_impl(&mut self, st: &TypedStruct, widget_name: &str) -> String {
        self.lines.clear();
        self.widget_class_name = Some(widget_name.to_string());
        let widget_model = self.resolve_widget_model(&st.ast);

        // Populate struct field names so format_expr can resolve field references
        // to InArgs._fieldname during Construct body generation
        self.struct_field_names.clear();
        for field in &st.ast.fields {
            self.struct_field_names.insert(field.name.clone());
        }

        self.push_line("BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION");
        match widget_model.construct_kind {
            SlateConstructKind::TableRow => {
                self.push_line(&format!(
                    "void {}::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)",
                    widget_name
                ));
            }
            _ => self.push_line(&format!(
                "void {}::Construct(const FArguments& InArgs)",
                widget_name
            )),
        }
        self.push_line("{");
        self.indent += 1;

        // Initialize shader brushes
        let brushes = self.scan_for_shader_brushes(st);
        for brush in &brushes {
            let mat_code = self.format_expr_in_construct(&brush.material_expr);
            let size_code = self.format_expr_in_construct(&brush.size_expr);

            self.push_line(&format!(
                "ShaderBrush_{} = MakeShareable(new FSlateImageBrush({}, {}));",
                brush.id, mat_code, size_code
            ));
        }

        if widget_model.construct_kind == SlateConstructKind::TableRow {
            self.push_line(
                "FSuperRowType::Construct(FSuperRowType::FArguments(), InOwnerTableView);",
            );
        }

        // Find Compose method and build widget tree
        if let Some(compose_fn) = st.ast.methods.iter().find(|m| m.name == "Compose") {
            // Build symbol table from all statements in Compose()
            let symbol_table = self.build_symbol_table(&compose_fn.body);

            // Find the return expression
            if let Some(last_stmt) = compose_fn.body.stmts.last() {
                let expr_opt = match last_stmt {
                    Stmt::Expr(expr) => Some(expr),
                    Stmt::Return(Some(expr), _) => Some(expr),
                    _ => None,
                };

                if let Some(expr) = expr_opt {
                    match widget_model.construct_kind {
                        SlateConstructKind::Compound => {
                            self.push_line("ChildSlot");
                            self.push_line("[");
                            self.indent += 1;
                            // Generate widget tree with symbol table for identifier resolution
                            self.generate_widget_tree_with_context(expr, st, &symbol_table);
                            self.indent -= 1;
                            self.push_line("];");
                        }
                        SlateConstructKind::ToolTip => {
                            self.push_line(
                                "SToolTip::FArguments ToolTipArgs = SToolTip::FArguments();",
                            );
                            self.push_line("ToolTipArgs.Content()");
                            self.push_line("[");
                            self.indent += 1;
                            self.generate_widget_tree_with_context(expr, st, &symbol_table);
                            self.indent -= 1;
                            self.push_line("];");
                            self.push_line("SToolTip::Construct(ToolTipArgs);");
                        }
                        SlateConstructKind::TableRow => {
                            self.push_line("// Compose() ignored for SMultiColumnTableRow; override GenerateWidgetForColumn instead.");
                        }
                    }
                }
            }
        } else {
            if widget_model.construct_kind == SlateConstructKind::ToolTip {
                self.push_line("SToolTip::Construct(SToolTip::FArguments());");
            } else {
                self.push_line("// No Compose() method found");
            }
        }

        self.indent -= 1;
        self.push_line("}");
        self.push_line("END_SLATE_FUNCTION_BUILD_OPTIMIZATION");

        self.generate_special_method_impls(st, widget_name);

        self.lines.join("\n")
    }

    fn generate_special_method_impls(&mut self, st: &TypedStruct, widget_name: &str) {
        let widget_model = self.resolve_widget_model(&st.ast);
        let has_generate_widget_for_column = widget_model.construct_kind
            == SlateConstructKind::TableRow
            && st
                .ast
                .methods
                .iter()
                .any(|m| m.name == "GenerateWidgetForColumn");
        let has_on_opening = widget_model.construct_kind == SlateConstructKind::ToolTip
            && st.ast.methods.iter().any(|m| m.name == "OnOpening");

        if has_generate_widget_for_column {
            if let Some(method) = st
                .ast
                .methods
                .iter()
                .find(|m| m.name == "GenerateWidgetForColumn")
            {
                self.emit_special_method_impl(
                    st,
                    widget_name,
                    method,
                    SpecialSlateMethodKind::GenerateWidgetForColumn,
                );
            }
        }

        if has_on_opening {
            if let Some(method) = st.ast.methods.iter().find(|m| m.name == "OnOpening") {
                self.emit_special_method_impl(
                    st,
                    widget_name,
                    method,
                    SpecialSlateMethodKind::OnOpening,
                );
            }
        }
    }

    fn emit_special_method_impl(
        &mut self,
        st: &TypedStruct,
        widget_name: &str,
        method: &Function,
        kind: SpecialSlateMethodKind,
    ) {
        let symbol_table = self.build_symbol_table(&method.body);

        self.push_line("");
        match kind {
            SpecialSlateMethodKind::GenerateWidgetForColumn => {
                self.push_line(&format!(
                    "TSharedRef<SWidget> {}::GenerateWidgetForColumn(const FName& ColumnName)",
                    widget_name
                ));
            }
            SpecialSlateMethodKind::OnOpening => {
                self.push_line(&format!("void {}::OnOpening()", widget_name));
            }
        }
        self.push_line("{");
        self.indent += 1;

        if kind == SpecialSlateMethodKind::GenerateWidgetForColumn {
            self.push_line("(void)ColumnName;");
        }

        if kind == SpecialSlateMethodKind::OnOpening {
            self.push_line("SToolTip::OnOpening();");
        }

        for stmt in &method.body.stmts {
            self.emit_special_stmt(stmt, st, &symbol_table, kind);
        }

        if kind == SpecialSlateMethodKind::GenerateWidgetForColumn {
            self.push_line("return SNullWidget::NullWidget;");
        }

        self.indent -= 1;
        self.push_line("}");
    }

    fn emit_special_stmt(
        &mut self,
        stmt: &Stmt,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                if let Pattern::Binding { name, .. } = pattern {
                    let cpp_ty = ty
                        .as_ref()
                        .map(|t| self.map_type(t))
                        .unwrap_or_else(|| "auto".to_string());
                    if let Some(expr) = value {
                        if self.is_widget_expr(expr) {
                            self.push_line(&format!("{} {} =", cpp_ty, name));
                            self.indent += 1;
                            self.generate_widget_tree_with_context(expr, st, symbol_table);
                            self.indent -= 1;
                            self.push_line(";");
                        } else {
                            self.push_line(&format!(
                                "{} {} = {};",
                                cpp_ty,
                                name,
                                self.format_method_expr(expr)
                            ));
                        }
                    } else {
                        self.push_line(&format!("{} {};", cpp_ty, name));
                    }
                }
            }
            Stmt::Expr(expr) => self.emit_special_expr_stmt(expr, st, symbol_table, kind),
            Stmt::Return(Some(expr), _) => {
                self.emit_special_return_expr(expr, st, symbol_table, kind)
            }
            Stmt::Return(None, _) => self.push_line("return;"),
            Stmt::Break(Some(expr), _) => {
                self.push_line(&format!("break /* {} */;", self.format_method_expr(expr)))
            }
            Stmt::Break(None, _) => self.push_line("break;"),
            Stmt::Continue(_) => self.push_line("continue;"),
            Stmt::For {
                binding,
                iter,
                body,
                ..
            } => {
                if let Pattern::Binding { name, .. } = binding {
                    self.push_line(&format!(
                        "for (auto {} : {})",
                        name,
                        self.format_method_expr(iter)
                    ));
                } else {
                    self.push_line(&format!("for (auto _ : {})", self.format_method_expr(iter)));
                }
                self.push_line("{");
                self.indent += 1;
                for s in &body.stmts {
                    self.emit_special_stmt(s, st, symbol_table, kind);
                }
                self.indent -= 1;
                self.push_line("}");
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.push_line(&format!("while ({})", self.format_method_expr(condition)));
                self.push_line("{");
                self.indent += 1;
                for s in &body.stmts {
                    self.emit_special_stmt(s, st, symbol_table, kind);
                }
                self.indent -= 1;
                self.push_line("}");
            }
            Stmt::Loop { body, .. } => {
                self.push_line("while (true)");
                self.push_line("{");
                self.indent += 1;
                for s in &body.stmts {
                    self.emit_special_stmt(s, st, symbol_table, kind);
                }
                self.indent -= 1;
                self.push_line("}");
            }
            Stmt::Item(_) => {}
        }
    }

    fn emit_special_expr_stmt(
        &mut self,
        expr: &Expr,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        match expr {
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.emit_special_if_stmt(
                    condition,
                    then_branch,
                    else_branch.as_deref(),
                    st,
                    symbol_table,
                    kind,
                );
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.emit_special_match_stmt(scrutinee, arms, false, st, symbol_table, kind);
            }
            Expr::Call { callee, args, .. } => {
                if args.len() == 1 && self.is_widget_expr(&args[0].value) {
                    self.push_line(&format!("{}(", self.format_method_expr(callee)));
                    self.indent += 1;
                    self.generate_widget_tree_with_context(&args[0].value, st, symbol_table);
                    self.indent -= 1;
                    self.push_line(");");
                } else {
                    self.push_line(&format!("{};", self.format_method_expr(expr)));
                }
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                if args.len() == 1 && self.is_widget_expr(&args[0].value) {
                    self.push_line(&format!(
                        "{}.{}(",
                        self.format_method_expr(receiver),
                        method
                    ));
                    self.indent += 1;
                    self.generate_widget_tree_with_context(&args[0].value, st, symbol_table);
                    self.indent -= 1;
                    self.push_line(");");
                } else {
                    self.push_line(&format!("{};", self.format_method_expr(expr)));
                }
            }
            _ => self.push_line(&format!("{};", self.format_method_expr(expr))),
        }
    }

    fn emit_special_return_expr(
        &mut self,
        expr: &Expr,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        match expr {
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.emit_special_if_stmt(
                    condition,
                    then_branch,
                    else_branch.as_deref(),
                    st,
                    symbol_table,
                    kind,
                );
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.emit_special_match_stmt(scrutinee, arms, true, st, symbol_table, kind);
            }
            _ if kind == SpecialSlateMethodKind::GenerateWidgetForColumn
                && self.is_widget_expr(expr) =>
            {
                self.push_line("return");
                self.indent += 1;
                self.generate_widget_tree_with_context(expr, st, symbol_table);
                self.indent -= 1;
                self.push_line(";");
            }
            _ => self.push_line(&format!("return {};", self.format_method_expr(expr))),
        }
    }

    fn emit_special_if_stmt(
        &mut self,
        condition: &Expr,
        then_branch: &Block,
        else_branch: Option<&ElseBranch>,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        self.push_line(&format!("if ({})", self.format_method_expr(condition)));
        self.push_line("{");
        self.indent += 1;
        for s in &then_branch.stmts {
            self.emit_special_stmt(s, st, symbol_table, kind);
        }
        self.indent -= 1;
        self.push_line("}");

        if let Some(else_branch) = else_branch {
            self.emit_special_else_branch(else_branch, st, symbol_table, kind);
        }
    }

    fn emit_special_else_branch(
        &mut self,
        else_branch: &ElseBranch,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        match else_branch {
            ElseBranch::Else(block) => {
                self.push_line("else");
                self.push_line("{");
                self.indent += 1;
                for s in &block.stmts {
                    self.emit_special_stmt(s, st, symbol_table, kind);
                }
                self.indent -= 1;
                self.push_line("}");
            }
            ElseBranch::ElseIf(cond, block, tail) => {
                self.push_line(&format!("else if ({})", self.format_method_expr(cond)));
                self.push_line("{");
                self.indent += 1;
                for s in &block.stmts {
                    self.emit_special_stmt(s, st, symbol_table, kind);
                }
                self.indent -= 1;
                self.push_line("}");
                if let Some(next) = tail.as_deref() {
                    self.emit_special_else_branch(next, st, symbol_table, kind);
                }
            }
        }
    }

    fn emit_special_match_stmt(
        &mut self,
        scrutinee: &Expr,
        arms: &[kain_core::ast::MatchArm],
        as_return: bool,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
        kind: SpecialSlateMethodKind,
    ) {
        let scrutinee_str = self.format_method_expr(scrutinee);
        let mut is_first = true;

        for arm in arms {
            let mut cond = self.match_pattern_condition(&scrutinee_str, &arm.pattern);
            if let Some(guard) = &arm.guard {
                let guard_str = self.format_method_expr(guard);
                cond = match cond {
                    Some(c) => Some(format!("({}) && ({})", c, guard_str)),
                    None => Some(guard_str),
                };
            }

            match cond {
                Some(cond_str) => {
                    if is_first {
                        self.push_line(&format!("if ({})", cond_str));
                    } else {
                        self.push_line(&format!("else if ({})", cond_str));
                    }
                }
                None => {
                    if is_first {
                        self.push_line("if (true)");
                    } else {
                        self.push_line("else");
                    }
                }
            }

            self.push_line("{");
            self.indent += 1;
            if as_return {
                self.emit_special_return_expr(&arm.body, st, symbol_table, kind);
            } else {
                self.emit_special_expr_stmt(&arm.body, st, symbol_table, kind);
            }
            self.indent -= 1;
            self.push_line("}");
            is_first = false;
        }
    }

    fn match_pattern_condition(&self, scrutinee: &str, pattern: &Pattern) -> Option<String> {
        match pattern {
            Pattern::Wildcard(_) => None,
            Pattern::Literal(expr) => Some(format!(
                "{} == {}",
                scrutinee,
                self.format_method_expr(expr)
            )),
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                let mut parts = Vec::new();
                if let Some(lo) = start {
                    parts.push(format!("{} >= {}", scrutinee, self.format_method_expr(lo)));
                }
                if let Some(hi) = end {
                    let op = if *inclusive { "<=" } else { "<" };
                    parts.push(format!(
                        "{} {} {}",
                        scrutinee,
                        op,
                        self.format_method_expr(hi)
                    ));
                }
                if parts.is_empty() {
                    None
                } else {
                    Some(parts.join(" && "))
                }
            }
            Pattern::Or(patterns, _) => {
                let mut branches = Vec::new();
                for p in patterns {
                    match self.match_pattern_condition(scrutinee, p) {
                        None => return None,
                        Some(cond) => branches.push(format!("({})", cond)),
                    }
                }
                if branches.is_empty() {
                    None
                } else {
                    Some(branches.join(" || "))
                }
            }
            Pattern::Variant {
                enum_name,
                variant,
                fields,
                ..
            } => {
                if matches!(fields, kain_core::ast::VariantPatternFields::Unit) {
                    let variant_ref = if let Some(en) = enum_name {
                        format!("{}::{}", naming::to_enum_name(en), variant)
                    } else {
                        variant.clone()
                    };
                    Some(format!("{} == {}", scrutinee, variant_ref))
                } else {
                    Some("false".to_string())
                }
            }
            Pattern::Binding { .. } => None,
            _ => Some("false".to_string()),
        }
    }

    fn is_widget_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::Call { callee, .. } => {
                if let Expr::Ident(name, _) = &**callee {
                    !matches!(WidgetType::from_name(name), WidgetType::Unknown(_))
                } else {
                    false
                }
            }
            Expr::MethodCall { receiver, .. } => self.is_widget_expr(receiver),
            _ => false,
        }
    }

    /// Build a symbol table from the Compose() method body
    /// Maps variable names to their widget construction and method calls
    fn build_symbol_table(&self, block: &Block) -> HashMap<String, WidgetInfo> {
        let mut table = HashMap::new();

        for stmt in &block.stmts {
            match stmt {
                Stmt::Let { pattern, value, .. } => {
                    // Extract variable name from pattern
                    if let Pattern::Binding { name, .. } = pattern {
                        // Track the initial widget construction
                        if let Some(value_expr) = value {
                            let widget_info = WidgetInfo {
                                construction: value_expr.clone(),
                                method_calls: Vec::new(),
                            };
                            table.insert(name.clone(), widget_info);
                        }
                    }
                }
                Stmt::Expr(expr) => {
                    // Track method calls on variables
                    self.track_method_calls(expr, &mut table);
                }
                _ => {}
            }
        }

        table
    }

    /// Track method calls on variables to build complete widget trees
    fn track_method_calls(&self, expr: &Expr, table: &mut HashMap<String, WidgetInfo>) {
        match expr {
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                // Check if receiver is a variable we're tracking
                if let Expr::Ident(var_name, _) = &**receiver {
                    if let Some(widget_info) = table.get_mut(var_name) {
                        widget_info.method_calls.push(MethodCallInfo {
                            method: method.clone(),
                            args: args.clone(),
                        });
                    }
                }
                // Recursively track nested method calls
                self.track_method_calls(receiver, table);
                for arg in args {
                    self.track_method_calls(&arg.value, table);
                }
            }
            Expr::Call { args, .. } => {
                for arg in args {
                    self.track_method_calls(&arg.value, table);
                }
            }
            _ => {}
        }
    }

    /// Generate widget tree with symbol table context for identifier resolution
    fn generate_widget_tree_with_context(
        &mut self,
        expr: &Expr,
        st: &TypedStruct,
        symbol_table: &HashMap<String, WidgetInfo>,
    ) {
        match expr {
            Expr::Ident(name, _) => {
                // Resolve identifier from symbol table
                if let Some(widget_info) = symbol_table.get(name) {
                    // Extract widget type from construction
                    let widget_type = if let Expr::Call { callee, .. } = &widget_info.construction {
                        self.extract_widget_type(callee)
                    } else {
                        WidgetType::Unknown("Unknown".to_string())
                    };

                    // Generate the widget construction (SNew(...))
                    let slate_class = widget_type.to_slate_class();
                    if widget_type.is_list_widget() {
                        let inferred_item_type =
                            self.infer_list_item_type_from_method_calls(&widget_info.method_calls);
                        self.push_line(
                            &self
                                .list_widget_stype_for(&slate_class, inferred_item_type.as_deref()),
                        );
                    } else {
                        self.push_line(&format!("SNew({})", slate_class));
                    }

                    // Push widget type so generate_widget_property can check it
                    // (e.g. SSlider::MinValue takes float, SSpinBox takes TOptional<float>)
                    self.parent_stack.push(widget_type.clone());

                    // Apply all method calls that were made on this variable
                    for method_call in &widget_info.method_calls {
                        match method_call.method.as_str() {
                            "Add" => {
                                // For Add() calls, we need to generate slots with the child widgets
                                if widget_type.has_slots() {
                                    let slot_type = widget_type.to_slate_class();
                                    self.push_line("");
                                    self.push_line(&format!("+{}::Slot()", slot_type));
                                    self.push_line("[");
                                    self.indent += 1;

                                    // Resolve the argument (which might be another variable)
                                    if let Some(first_arg) = method_call.args.first() {
                                        self.generate_widget_tree_with_context(
                                            &first_arg.value,
                                            st,
                                            symbol_table,
                                        );
                                    }

                                    self.indent -= 1;
                                    self.push_line("]");
                                } else if widget_type.has_content_slot() {
                                    self.push_line("[");
                                    self.indent += 1;
                                    if let Some(first_arg) = method_call.args.first() {
                                        self.generate_widget_tree_with_context(
                                            &first_arg.value,
                                            st,
                                            symbol_table,
                                        );
                                    }
                                    self.indent -= 1;
                                    self.push_line("]");
                                }
                            }
                            "Content" => {
                                // Content slot
                                self.push_line("[");
                                self.indent += 1;
                                if let Some(first_arg) = method_call.args.first() {
                                    self.generate_widget_tree_with_context(
                                        &first_arg.value,
                                        st,
                                        symbol_table,
                                    );
                                }
                                self.indent -= 1;
                                self.push_line("]");
                            }
                            _ => {
                                // Regular property setter
                                self.generate_widget_property(
                                    &method_call.method,
                                    &method_call.args,
                                );
                            }
                        }
                    }

                    // Pop the widget type now that all its properties are processed
                    self.parent_stack.pop();
                } else {
                    // Check if it's a struct field (custom widget)
                    if let Some(field) = st.ast.fields.iter().find(|f| &f.name == name) {
                        let type_name = match &field.ty {
                            Type::Named { name, .. } => name.clone(),
                            _ => "Unknown".to_string(),
                        };
                        self.push_line(&format!("SNew(S{})", type_name));
                    } else {
                        // Fallback - treat as widget type
                        self.push_line(&format!("SNew(S{})", name));
                    }
                }
            }
            Expr::Call { callee, args: _args, .. } => {
                // Check for shader_image
                if let Expr::Ident(name, _) = &**callee {
                    if name == "shader_image" {
                        let id = self.shader_brush_counter;
                        self.shader_brush_counter += 1;

                        self.push_line("SNew(SImage)");
                        self.push_line(&format!(".Image(ShaderBrush_{}.Get())", id));
                        self.parent_stack.push(WidgetType::Image);
                        self.parent_stack.pop();
                        return;
                    }
                }

                // Extract widget type from callee
                let widget_type = self.extract_widget_type(callee);
                let slate_class = widget_type.to_slate_class();

                if widget_type.is_list_widget() {
                    self.push_line(&self.list_widget_stype_for(&slate_class, None));
                } else {
                    self.push_line(&format!("SNew({})", slate_class));
                }

                self.parent_stack.push(widget_type.clone());
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => match method.as_str() {
                "Padding" | "HAlign" | "VAlign" | "FillWidth" | "FillHeight" | "AutoWidth"
                | "AutoHeight" | "MaxWidth" | "MaxHeight" | "Column" | "Row" | "ColumnSpan"
                | "RowSpan" => {
                    self.generate_widget_tree_with_context(receiver, st, symbol_table);
                    if self.is_slot_context_expr(receiver) {
                        self.generate_slot_property(method, args);
                    } else {
                        self.generate_widget_property(method, args);
                    }
                }
                "Content" => {
                    self.generate_widget_tree_with_context(receiver, st, symbol_table);
                    if let Some(first_arg) = args.first() {
                        self.push_line("[");
                        self.indent += 1;
                        self.generate_widget_tree_with_context(&first_arg.value, st, symbol_table);
                        self.indent -= 1;
                        self.push_line("]");
                    }
                }
                _ => {
                    self.generate_widget_tree_with_context(receiver, st, symbol_table);
                    self.generate_widget_property(method, args);
                }
            },
            _ => {
                self.push_line("/* Unsupported widget expression */");
            }
        }
    }

    fn format_expr_in_construct(&self, expr: &Expr) -> String {
        match expr {
            Expr::Ident(name, _) => {
                if self.struct_field_names.contains(name) {
                    format!("InArgs._{}", name)
                } else {
                    name.clone()
                }
            }
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(callee_name, _) = &**callee {
                    if let Some(resolved) = self.resolve_constructor_call(callee_name, args) {
                        return resolved;
                    }
                }
                self.format_expr(expr)
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let recv = self.format_expr_in_construct(receiver);
                let formatted_args: Vec<String> = args
                    .iter()
                    .map(|a| self.format_expr_in_construct(&a.value))
                    .collect();
                if formatted_args.is_empty() {
                    format!("{}.{}()", recv, method)
                } else {
                    format!("{}.{}({})", recv, method, formatted_args.join(", "))
                }
            }
            _ => self.format_expr(expr),
        }
    }

    /// Resolve a KAIN constructor call to its UE5 C++ equivalent.
    /// Handles: color("sunset"), vec3(x,y,z), vec2(x,y), rotator(p,y,r),
    ///          margin(uniform), margin(h,v), margin(l,t,r,b), quat(x,y,z,w),
    ///          transform(), linear_color(r,g,b), linear_color(r,g,b,a)
    fn resolve_constructor_call(
        &self,
        callee_name: &str,
        args: &[kain_core::ast::CallArg],
    ) -> Option<String> {
        // Map KAIN constructor names to UE5 type names
        let ue5_type = match callee_name {
            "vec2" | "Vec2" | "vector2d" => "FVector2D",
            "vec3" | "Vec3" | "vector" | "Vector" => "FVector",
            "vec4" | "Vec4" => "FVector4",
            "rotator" | "Rotator" | "rot" | "Rot" => "FRotator",
            "quat" | "Quat" => "FQuat",
            "transform" | "Transform" => "FTransform",
            "linear_color" | "LinearColor" | "Color" => "FLinearColor",
            "margin" | "Margin" | "padding" | "Padding" => "FMargin",
            "color" => {
                // Special case: color("name") resolves named colors
                // color(r, g, b) and color(r, g, b, a) resolve to FLinearColor constructor
                if args.len() == 1 {
                    if let Expr::String(color_name, _) = &args[0].value {
                        if let Some(ctx) = &self.context {
                            if let Some(resolved) = ctx.knowledge.resolve_named_color(color_name) {
                                return Some(resolved);
                            }
                        }
                        // Fallback: try as a static color constant
                        let upper = color_name.to_uppercase();
                        return match upper.as_str() {
                            "WHITE" => Some("FLinearColor::White".to_string()),
                            "BLACK" => Some("FLinearColor::Black".to_string()),
                            "RED" => Some("FLinearColor::Red".to_string()),
                            "GREEN" => Some("FLinearColor::Green".to_string()),
                            "BLUE" => Some("FLinearColor::Blue".to_string()),
                            "YELLOW" => Some("FLinearColor::Yellow".to_string()),
                            "TRANSPARENT" => Some("FLinearColor::Transparent".to_string()),
                            "GRAY" | "GREY" => Some("FLinearColor::Gray".to_string()),
                            _ => Some(format!(
                                "FLinearColor::White /* unknown color: {} */",
                                color_name
                            )),
                        };
                    }
                }
                "FLinearColor"
            }
            _ => return None,
        };

        // Format the arguments
        let formatted_args: Vec<String> = args.iter().map(|a| self.format_expr(&a.value)).collect();

        // Try EngineKnowledge constructor resolution first
        if let Some(ctx) = &self.context {
            if let Some(resolved) = ctx.knowledge.resolve_constructor(ue5_type, &formatted_args) {
                return Some(resolved);
            }
        }

        // Direct fallback: construct with formatted args
        if formatted_args.is_empty() {
            Some(format!("{}()", ue5_type))
        } else {
            Some(format!("{}({})", ue5_type, formatted_args.join(", ")))
        }
    }

    #[allow(dead_code)]
    fn generate_widget_tree(&mut self, expr: &Expr, st: &TypedStruct) {
        match expr {
            Expr::Call { callee, args: _args, .. } => {
                // Check for shader_image
                if let Expr::Ident(name, _) = &**callee {
                    if name == "shader_image" {
                        // Find ID (reuse scan logic logic - simplistic but robust enough for strictly ordered AST)
                        // A better way would be dragging a counter, but let's assume strict traversal order matches
                        // We need a stable ID.
                        // Let's rely on a global counter passed in or state?
                        // For now, let's increment a counter in self that resets on generate_construct_impl?
                        // Actually, I can't easily match the scan ID without exact traversal.
                        // Hack: use a local counter in this method?
                        // I'll add `shader_brush_counter` to SlateGenerator.
                        let id = self.shader_brush_counter;
                        self.shader_brush_counter += 1;

                        self.push_line("SNew(SImage)");
                        self.push_line(&format!(".Image(ShaderBrush_{}.Get())", id));

                        // Push dummy parent so children don't attach weirdly (though SImage has no children)
                        self.parent_stack.push(WidgetType::Image);
                        self.parent_stack.pop();
                        return; // SImage has no children from shader_image args
                    }
                }

                // Extract widget type from callee
                let widget_type = self.extract_widget_type(callee);
                let slate_class = widget_type.to_slate_class();

                // List widgets need the item-pointer type argument.
                if widget_type.is_list_widget() {
                    self.push_line(&self.list_widget_stype_for(&slate_class, None));
                } else {
                    self.push_line(&format!("SNew({})", slate_class));
                }

                // Push to parent stack for children
                self.parent_stack.push(widget_type.clone());
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                match method.as_str() {
                    "Add" | "Slot" => {
                        // Generate receiver first, then slot
                        self.generate_widget_tree(receiver, st);
                        self.generate_slot(args, st);
                    }
                    // Slot-level properties (applied to the slot, not the widget)
                    "Padding" | "HAlign" | "VAlign" | "FillWidth" | "FillHeight" | "AutoWidth"
                    | "AutoHeight" | "MaxWidth" | "MaxHeight" | "Column" | "Row" | "ColumnSpan"
                    | "RowSpan" => {
                        self.generate_widget_tree(receiver, st);
                        if self.is_slot_context_expr(receiver) {
                            self.generate_slot_property(method, args);
                        } else {
                            self.generate_widget_property(method, args);
                        }
                    }
                    // Content slot (Border, Button, ToolTip)
                    "Content" => {
                        self.generate_widget_tree(receiver, st);
                        if let Some(first_arg) = args.first() {
                            self.push_line("[");
                            self.indent += 1;
                            self.generate_widget_tree(&first_arg.value, st);
                            self.indent -= 1;
                            self.push_line("]");
                        }
                    }
                    // All widget properties - comprehensive coverage
                    _ => {
                        self.generate_widget_tree(receiver, st);
                        self.generate_widget_property(method, args);
                    }
                }
            }
            Expr::Ident(name, _) => {
                // Check if Ident matches a field name in the struct
                if let Some(field) = st.ast.fields.iter().find(|f| &f.name == name) {
                    // Map KAIN type name to Slate class name
                    let type_name = match &field.ty {
                        Type::Named { name, .. } => name.clone(),
                        _ => "Unknown".to_string(),
                    };

                    // Simple heuristic: if it looks like a custom widget
                    self.push_line(&format!("SNew(S{})", type_name));
                } else {
                    // Fallback to old behavior
                    self.push_line(&format!("SNew(S{})", name));
                }
            }
            _ => {
                self.push_line("/* Unsupported widget expression */");
            }
        }
    }

    #[allow(dead_code)]
    fn generate_slot(&mut self, args: &[kain_core::ast::CallArg], st: &TypedStruct) {
        if let Some(parent) = self.parent_stack.last() {
            if parent.has_slots() {
                let slot_type = parent.to_slate_class();
                self.push_line("");
                self.push_line(&format!("+{}::Slot()", slot_type));

                self.push_line("[");
                self.indent += 1;

                // Generate child widget
                if let Some(first_arg) = args.first() {
                    self.generate_widget_tree(&first_arg.value, st);
                }

                self.indent -= 1;
                self.push_line("]");
            } else if parent.has_content_slot() {
                // Single content slot (Border, Button, etc.)
                self.push_line("[");
                self.indent += 1;
                if let Some(first_arg) = args.first() {
                    self.generate_widget_tree(&first_arg.value, st);
                }
                self.indent -= 1;
                self.push_line("]");
            }
        }
    }

    fn generate_slot_property(&mut self, method: &str, args: &[kain_core::ast::CallArg]) {
        let formatted_args = self.format_args(args);
        match method {
            "Column" => self.push_line(&format!(".Column({})", formatted_args)),
            "Row" => self.push_line(&format!(".Row({})", formatted_args)),
            "ColumnSpan" => self.push_line(&format!(".ColumnSpan({})", formatted_args)),
            "RowSpan" => self.push_line(&format!(".RowSpan({})", formatted_args)),
            _ => self.push_line(&format!(".{}({})", method, formatted_args)),
        }
    }

    fn generate_widget_property(&mut self, method: &str, args: &[kain_core::ast::CallArg]) {
        let formatted_args = self.format_args(args);

        match method {
            // === Text properties ===
            "Text" => {
                if let Some(arg) = args.first() {
                    if let Expr::String(s, _) = &arg.value {
                        self.push_line(&format!(".Text(FText::FromString(TEXT(\"{}\")))", s));
                        return;
                    }
                }
                // TAttribute<FText> binding
                self.push_line(&format!(".Text({})", formatted_args));
            }
            "HintText" => {
                if let Some(arg) = args.first() {
                    if let Expr::String(s, _) = &arg.value {
                        self.push_line(&format!(".HintText(FText::FromString(TEXT(\"{}\")))", s));
                        return;
                    }
                }
                self.push_line(&format!(".HintText({})", formatted_args));
            }
            "ToolTipText" => {
                if let Some(arg) = args.first() {
                    if let Expr::String(s, _) = &arg.value {
                        self.push_line(&format!(
                            ".ToolTipText(FText::FromString(TEXT(\"{}\")))",
                            s
                        ));
                        return;
                    }
                }
                self.push_line(&format!(".ToolTipText({})", formatted_args));
            }
            "ToolTip" => {
                self.push_line(&format!(".ToolTip({})", formatted_args));
            }

            // === Delegate properties (click, value change, text, etc.) ===
            // Uses the systematic delegate bridge: if the InArgs field's delegate type
            // doesn't match the native Slate delegate, wrap in a lambda bridge.
            // Otherwise pass through directly. For non-InArgs (local handlers), use CreateSP.
            "OnClicked"
            | "OnPressed"
            | "OnReleased"
            | "OnHovered"
            | "OnUnhovered"
            | "OnValueChanged"
            | "OnTextCommitted"
            | "OnTextChanged"
            | "OnCheckStateChanged"
            | "OnSelectionChanged"
            | "OnColorChanged" => {
                // SColorBlock is display-only and has no OnColorChanged delegate.
                if method == "OnColorChanged"
                    && self.parent_stack.last() == Some(&WidgetType::ColorBlock)
                {
                    return;
                }
                if self.is_inargs_reference(&formatted_args) {
                    self.emit_delegate_bridge_or_passthrough(method, &formatted_args);
                } else {
                    let native = self
                        .native_delegate_for_property(method)
                        .unwrap_or("FSimpleDelegate");
                    self.push_line(&format!(
                        ".{}({}::CreateSP(this, &{}::Handle{}))",
                        method,
                        native,
                        self.current_widget_class(),
                        self.handler_name_from_args(args)
                    ));
                }
            }

            // === List view properties ===
            "ListItemsSource" => {
                self.push_line(&format!(".ListItemsSource({})", formatted_args));
            }
            "OnGenerateRow" => {
                self.push_line(&format!(".OnGenerateRow({})", formatted_args));
            }
            "OnGetChildren" => {
                self.push_line(&format!(".OnGetChildren({})", formatted_args));
            }
            "SelectionMode" => {
                self.push_line(&format!(".SelectionMode({})", formatted_args));
            }
            "ItemHeight" => {
                self.push_line(&format!(".ItemHeight({})", formatted_args));
            }
            "HeaderRow" => {
                self.push_line(&format!(".HeaderRow({})", formatted_args));
            }

            // === Visual properties ===
            "ColorAndOpacity" => {
                // Special case: SColorBlock uses .Color() not .ColorAndOpacity()
                if self.parent_stack.last() == Some(&WidgetType::ColorBlock) {
                    self.push_line(&format!(".Color({})", formatted_args));
                } else {
                    self.push_line(&format!(".ColorAndOpacity({})", formatted_args));
                }
            }
            "Color" => {
                // SColorBlock::Color takes FLinearColor, not FVector.
                // If the argument is a vec3() call, convert to FLinearColor(r, g, b, 1.0f)
                if let Some(arg) = args.first() {
                    let expr_str = self.format_expr(&arg.value);
                    if expr_str.starts_with("FVector(") {
                        // Extract the inner args and wrap in FLinearColor with alpha=1.0
                        let inner = &expr_str["FVector(".len()..expr_str.len() - 1];
                        self.push_line(&format!(".Color(FLinearColor({}, 1.0f))", inner));
                        return;
                    }
                }
                // Also handle InArgs field refs that are mapped as FVector (e.g. color_a/color_b)
                // and convert explicitly to FLinearColor.
                if formatted_args.starts_with("InArgs._") {
                    let field_name = formatted_args.trim_start_matches("InArgs._");
                    if let Some(field_ty) = self.field_type_map.get(field_name) {
                        if field_ty == "FVector" {
                            self.push_line(&format!(
                                ".Color(FLinearColor({0}.X, {0}.Y, {0}.Z, 1.0f))",
                                formatted_args
                            ));
                            return;
                        }
                    }
                }
                self.push_line(&format!(".Color({})", formatted_args));
            }
            "BackgroundColor" => {
                self.push_line(&format!(".BackgroundColor({})", formatted_args));
            }
            "ForegroundColor" => {
                self.push_line(&format!(".ForegroundColor({})", formatted_args));
            }
            "Image" | "Brush" => {
                self.push_line(&format!(".Image({})", formatted_args));
            }
            "BorderImage" => {
                self.push_line(&format!(".BorderImage({})", formatted_args));
            }
            "Style" => {
                self.push_line(&format!(".Style({})", formatted_args));
            }
            "Font" => {
                self.push_line(&format!(".Font({})", formatted_args));
            }
            "FontSize" => {
                // STextBlock::FArguments has no .FontSize() shorthand.
                // Keep generation compile-safe; users can set .Font(...) explicitly.
            }

            // === State binding properties (TAttribute) ===
            "IsEnabled" => {
                self.push_line(&format!(".IsEnabled({})", formatted_args));
            }
            "Visibility" => {
                self.push_line(&format!(".Visibility({})", formatted_args));
            }
            "IsChecked" => {
                self.push_line(&format!(".IsChecked({})", formatted_args));
            }
            "Value" => {
                self.push_line(&format!(".Value({})", formatted_args));
            }
            "Percent" => {
                self.push_line(&format!(".Percent({})", formatted_args));
            }

            // === Numeric properties ===
            // SSpinBox MinValue/MaxValue expect TOptional<NumericType>
            // SSlider MinValue/MaxValue expect raw float
            "MinValue" | "MinSliderValue" => {
                if self.parent_stack.last() == Some(&WidgetType::Slider) {
                    self.push_line(&format!(".MinValue({})", formatted_args));
                } else {
                    self.push_line(&format!(".MinValue(TOptional<float>({}))", formatted_args));
                }
            }
            "MaxValue" | "MaxSliderValue" => {
                if self.parent_stack.last() == Some(&WidgetType::Slider) {
                    self.push_line(&format!(".MaxValue({})", formatted_args));
                } else {
                    self.push_line(&format!(".MaxValue(TOptional<float>({}))", formatted_args));
                }
            }
            "MinDesiredWidth" => {
                self.push_line(&format!(".MinDesiredWidth({})", formatted_args));
            }
            "MaxDesiredWidth" => {
                self.push_line(&format!(".MaxDesiredWidth({})", formatted_args));
            }
            "MinDesiredHeight" => {
                self.push_line(&format!(".MinDesiredHeight({})", formatted_args));
            }
            "MaxDesiredHeight" => {
                self.push_line(&format!(".MaxDesiredHeight({})", formatted_args));
            }

            // === Layout properties ===
            "Orientation" => {
                self.push_line(&format!(".Orientation({})", formatted_args));
            }
            "Justification" => {
                self.push_line(&format!(".Justification({})", formatted_args));
            }
            "AutoWrapText" => {
                self.push_line(&format!(".AutoWrapText({})", formatted_args));
            }
            "WrapTextAt" => {
                self.push_line(&format!(".WrapTextAt({})", formatted_args));
            }
            "RenderTransform" => {
                self.push_line(&format!(".RenderTransform({})", formatted_args));
            }
            "RenderTransformPivot" => {
                self.push_line(&format!(".RenderTransformPivot({})", formatted_args));
            }

            // === ComboBox specific ===
            "OptionsSource" => {
                self.push_line(&format!(".OptionsSource({})", formatted_args));
            }
            "OnGenerateWidget" => {
                self.push_line(&format!(".OnGenerateWidget({})", formatted_args));
            }
            "AddOption" => {
                // SComboBox does not expose AddOption in declarative args.
                // Options should be provided through OptionsSource data.
                // Skip silently to keep generated code compiling.
            }

            // === Splitter specific ===
            "ResizeMode" => {
                self.push_line(&format!(".ResizeMode({})", formatted_args));
            }
            "PhysicalSplitterHandleSize" => {
                self.push_line(&format!(".PhysicalSplitterHandleSize({})", formatted_args));
            }

            // === Fallback for any unrecognized property ===
            _ => {
                // If the argument is a string literal, wrap in FText::FromString(TEXT(...))
                // since custom SLATE_ARGUMENT(FText, ...) properties expect FText, not raw strings.
                if let Some(arg) = args.first() {
                    if let Expr::String(s, _) = &arg.value {
                        self.push_line(&format!(".{}(FText::FromString(TEXT(\"{}\")))", method, s));
                        return;
                    }
                }
                self.push_line(&format!(".{}({})", method, formatted_args));
            }
        }
    }

    fn extract_widget_type(&self, expr: &Expr) -> WidgetType {
        match expr {
            Expr::Ident(name, _) => WidgetType::from_name(name),
            _ => WidgetType::Unknown("Unknown".to_string()),
        }
    }

    fn format_args(&self, args: &[kain_core::ast::CallArg]) -> String {
        args.iter()
            .map(|arg| self.format_expr(&arg.value))
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn format_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::String(s, _) => format!("\"{}\"", s),
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => {
                // Ensure float literals always have decimal point for valid C++ syntax
                if f.fract() == 0.0 {
                    format!("{:.1}f", f)
                } else {
                    format!("{}f", f)
                }
            }
            Expr::Bool(b, _) => b.to_string(),
            Expr::Ident(name, _) => {
                // During Construct body generation, resolve struct field references
                // to InArgs._fieldname (e.g., 'title' -> 'InArgs._title')
                if self.struct_field_names.contains(name) {
                    format!("InArgs._{}", name)
                } else {
                    name.clone()
                }
            }
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(callee_name, _) = &**callee {
                    // Try constructor resolution (color, vec3, margin, etc.)
                    if let Some(resolved) = self.resolve_constructor_call(callee_name, args) {
                        return resolved;
                    }
                    // Generic function call
                    let formatted_args: Vec<String> =
                        args.iter().map(|a| self.format_expr(&a.value)).collect();
                    return format!("{}({})", callee_name, formatted_args.join(", "));
                }
                let callee_str = self.format_expr(callee);
                let formatted_args: Vec<String> =
                    args.iter().map(|a| self.format_expr(&a.value)).collect();
                format!("{}({})", callee_str, formatted_args.join(", "))
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let recv = self.format_expr(receiver);
                let formatted_args: Vec<String> =
                    args.iter().map(|a| self.format_expr(&a.value)).collect();
                if formatted_args.is_empty() {
                    format!("{}.{}()", recv, method)
                } else {
                    format!("{}.{}({})", recv, method, formatted_args.join(", "))
                }
            }
            Expr::Field { object, field, .. } => {
                let obj = self.format_expr(object);
                format!("{}.{}", obj, field)
            }
            Expr::Unary { op, operand, .. } => {
                let operand_str = self.format_expr(operand);
                format!(
                    "{}{}",
                    match op {
                        kain_core::ast::UnaryOp::Neg => "-",
                        kain_core::ast::UnaryOp::Not => "!",
                        kain_core::ast::UnaryOp::BitNot => "~",
                        kain_core::ast::UnaryOp::Ref => "&",
                        kain_core::ast::UnaryOp::RefMut => "&",
                        kain_core::ast::UnaryOp::Deref => "*",
                    },
                    operand_str
                )
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let l = self.format_expr(left);
                let r = self.format_expr(right);
                let op_str = match op {
                    kain_core::ast::BinaryOp::Add => "+",
                    kain_core::ast::BinaryOp::Sub => "-",
                    kain_core::ast::BinaryOp::Mul => "*",
                    kain_core::ast::BinaryOp::Div => "/",
                    kain_core::ast::BinaryOp::Mod => "%",
                    kain_core::ast::BinaryOp::Eq => "==",
                    kain_core::ast::BinaryOp::Ne => "!=",
                    kain_core::ast::BinaryOp::Lt => "<",
                    kain_core::ast::BinaryOp::Le => "<=",
                    kain_core::ast::BinaryOp::Gt => ">",
                    kain_core::ast::BinaryOp::Ge => ">=",
                    kain_core::ast::BinaryOp::And => "&&",
                    kain_core::ast::BinaryOp::Or => "||",
                    kain_core::ast::BinaryOp::BitAnd => "&",
                    kain_core::ast::BinaryOp::BitOr => "|",
                    kain_core::ast::BinaryOp::BitXor => "^",
                    kain_core::ast::BinaryOp::Shl => "<<",
                    kain_core::ast::BinaryOp::Shr => ">>",
                    kain_core::ast::BinaryOp::Pow => "/* pow */",
                    kain_core::ast::BinaryOp::Assign => "=",
                    kain_core::ast::BinaryOp::AddAssign => "+=",
                    kain_core::ast::BinaryOp::SubAssign => "-=",
                    kain_core::ast::BinaryOp::MulAssign => "*=",
                    kain_core::ast::BinaryOp::DivAssign => "/=",
                    kain_core::ast::BinaryOp::Range => "/* range */",
                    kain_core::ast::BinaryOp::RangeInclusive => "/* range_inclusive */",
                };
                format!("({} {} {})", l, op_str, r)
            }
            Expr::EnumVariant {
                enum_name, variant, ..
            } => {
                // Map KAIN enum name to UE5 C++ enum name (E-prefix)
                let cpp_enum = if let Some(ref ctx) = self.context {
                    if ctx.enum_names.contains(enum_name) {
                        naming::to_enum_name(enum_name)
                    } else {
                        enum_name.clone()
                    }
                } else {
                    naming::to_enum_name(enum_name)
                };
                format!("{}::{}", cpp_enum, variant)
            }
            _ => "/* <unsupported_expression> */".to_string(),
        }
    }

    fn format_method_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::String(s, _) => format!("TEXT(\"{}\")", s),
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => {
                if f.fract() == 0.0 {
                    format!("{:.1}f", f)
                } else {
                    format!("{}f", f)
                }
            }
            Expr::Bool(b, _) => b.to_string(),
            Expr::Ident(name, _) => name.clone(),
            Expr::Call { callee, args, .. } => {
                let callee_str = self.format_method_expr(callee);
                let formatted_args: Vec<String> = args
                    .iter()
                    .map(|a| self.format_method_expr(&a.value))
                    .collect();
                format!("{}({})", callee_str, formatted_args.join(", "))
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                let recv = self.format_method_expr(receiver);
                let formatted_args: Vec<String> = args
                    .iter()
                    .map(|a| self.format_method_expr(&a.value))
                    .collect();
                if formatted_args.is_empty() {
                    format!("{}.{}()", recv, method)
                } else {
                    format!("{}.{}({})", recv, method, formatted_args.join(", "))
                }
            }
            Expr::Field { object, field, .. } => {
                if let Expr::Ident(obj_name, _) = &**object {
                    let looks_type_like = obj_name.contains("::")
                        || obj_name.starts_with('S')
                        || obj_name.starts_with('F')
                        || obj_name.starts_with('E')
                        || obj_name.starts_with('U')
                        || obj_name.starts_with('A')
                        || obj_name.starts_with('T');
                    if looks_type_like {
                        return format!("{}::{}", obj_name, field);
                    }
                }
                format!("{}.{}", self.format_method_expr(object), field)
            }
            Expr::Unary { op, operand, .. } => {
                let operand_str = self.format_method_expr(operand);
                let op_str = match op {
                    kain_core::ast::UnaryOp::Neg => "-",
                    kain_core::ast::UnaryOp::Not => "!",
                    kain_core::ast::UnaryOp::BitNot => "~",
                    kain_core::ast::UnaryOp::Ref => "&",
                    kain_core::ast::UnaryOp::RefMut => "&",
                    kain_core::ast::UnaryOp::Deref => "*",
                };
                format!("{}{}", op_str, operand_str)
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let l = self.format_method_expr(left);
                let r = self.format_method_expr(right);
                let op_str = match op {
                    kain_core::ast::BinaryOp::Add => "+",
                    kain_core::ast::BinaryOp::Sub => "-",
                    kain_core::ast::BinaryOp::Mul => "*",
                    kain_core::ast::BinaryOp::Div => "/",
                    kain_core::ast::BinaryOp::Mod => "%",
                    kain_core::ast::BinaryOp::Eq => "==",
                    kain_core::ast::BinaryOp::Ne => "!=",
                    kain_core::ast::BinaryOp::Lt => "<",
                    kain_core::ast::BinaryOp::Le => "<=",
                    kain_core::ast::BinaryOp::Gt => ">",
                    kain_core::ast::BinaryOp::Ge => ">=",
                    kain_core::ast::BinaryOp::And => "&&",
                    kain_core::ast::BinaryOp::Or => "||",
                    kain_core::ast::BinaryOp::BitAnd => "&",
                    kain_core::ast::BinaryOp::BitOr => "|",
                    kain_core::ast::BinaryOp::BitXor => "^",
                    kain_core::ast::BinaryOp::Shl => "<<",
                    kain_core::ast::BinaryOp::Shr => ">>",
                    kain_core::ast::BinaryOp::Pow => "/* pow */",
                    kain_core::ast::BinaryOp::Assign => "=",
                    kain_core::ast::BinaryOp::AddAssign => "+=",
                    kain_core::ast::BinaryOp::SubAssign => "-=",
                    kain_core::ast::BinaryOp::MulAssign => "*=",
                    kain_core::ast::BinaryOp::DivAssign => "/=",
                    kain_core::ast::BinaryOp::Range => "/* range */",
                    kain_core::ast::BinaryOp::RangeInclusive => "/* range_inclusive */",
                };
                format!("({} {} {})", l, op_str, r)
            }
            Expr::EnumVariant {
                enum_name, variant, ..
            } => {
                let cpp_enum = if let Some(ref ctx) = self.context {
                    if ctx.enum_names.contains(enum_name) {
                        naming::to_enum_name(enum_name)
                    } else {
                        enum_name.clone()
                    }
                } else {
                    naming::to_enum_name(enum_name)
                };
                format!("{}::{}", cpp_enum, variant)
            }
            Expr::None(_) => "nullptr".to_string(),
            _ => self.format_expr(expr),
        }
    }

    /// Check if a formatted argument string references an InArgs field (delegate pass-through)
    /// When true, the delegate value should be passed directly (it's already bound).
    /// When false, we need to create a binding to a local handler method via CreateSP.
    fn is_inargs_reference(&self, formatted: &str) -> bool {
        formatted.contains("InArgs._")
    }

    /// Extract a handler name from delegate arguments for CreateSP binding
    fn handler_name_from_args(&self, args: &[kain_core::ast::CallArg]) -> String {
        if let Some(arg) = args.first() {
            if let Expr::Ident(name, _) = &arg.value {
                return name.clone();
            }
        }
        "UnknownHandler".to_string()
    }

    /// Get the native UE5 delegate type expected by a Slate property.
    /// Queries the widget registry first (data-driven from 2,346 extracted widgets),
    /// then falls back to hardcoded values for core widgets.
    fn native_delegate_for_property(&self, property_name: &str) -> Option<&str> {
        // Query widget registry first — check current widget context
        if let Some(ctx) = &self.context {
            // Try to find the current widget from parent_stack
            let current_widget = self.parent_stack.last().map(|w| w.to_slate_class());
            if let Some(widget_name) = current_widget {
                if let Some(delegate) = ctx
                    .widget_registry
                    .get_event_delegate(&widget_name, property_name)
                {
                    return Some(delegate);
                }
            }
            // Fall back to global event name lookup across all widgets
            if let Some(delegate) = ctx.widget_registry.get_event_delegate_any(property_name) {
                return Some(delegate);
            }
        }

        // Hardcoded fallback for core widgets (safety net)
        match property_name {
            "OnClicked" => Some("FOnClicked"),
            "OnPressed" | "OnReleased" | "OnHovered" | "OnUnhovered" => Some("FSimpleDelegate"),
            "OnValueChanged" => Some("FOnFloatValueChanged"),
            "OnTextCommitted" => Some("FOnTextCommitted"),
            "OnTextChanged" => Some("FOnTextChanged"),
            "OnCheckStateChanged" => Some("FOnCheckStateChanged"),
            "OnSelectionChanged" => Some("FOnSelectionChanged"),
            "OnColorChanged" => Some("FOnLinearColorValueChanged"),
            "OnMouseButtonDown" | "OnMouseButtonUp" => Some("FPointerEventHandler"),
            "OnKeyDown" | "OnKeyUp" => Some("FKeyEventHandler"),
            _ => None,
        }
    }

    /// Check if an InArgs field's delegate type matches the native Slate delegate.
    /// If not, returns the lambda bridge code to wrap the custom delegate.
    fn emit_delegate_bridge_or_passthrough(&mut self, property_name: &str, formatted_args: &str) {
        let field_name = formatted_args.trim_start_matches("InArgs._");
        let native_type = self.native_delegate_for_property(property_name);
        let field_type = self.field_type_map.get(field_name).cloned();

        // If we know the native type AND the field type, and they differ, bridge it
        let needs_bridge = match (&native_type, &field_type) {
            (Some(native), Some(field)) => native != field,
            _ => false, // Can't determine — pass through directly
        };

        if !needs_bridge {
            self.push_line(&format!(".{}({})", property_name, formatted_args));
            return;
        }

        let native = native_type.unwrap();

        // Generate the appropriate lambda bridge based on the native delegate signature
        match native {
            // FOnClicked: () -> FReply
            "FOnClicked" => {
                // Check if the custom delegate has parameters that FOnClicked doesn't provide
                let broadcast_args = self.get_default_broadcast_args(field_name);
                self.push_line(&format!(
                    ".OnClicked(FOnClicked::CreateLambda([=]() -> FReply {{ auto D = {}; D.Broadcast({}); return FReply::Handled(); }}))",
                    formatted_args, broadcast_args
                ));
            }
            // FOnFloatValueChanged: (float) -> void
            "FOnFloatValueChanged" => {
                self.push_line(&format!(
                    ".OnValueChanged(FOnFloatValueChanged::CreateLambda([=](float Val) {{ auto D = {}; D.Broadcast(Val); }}))",
                    formatted_args
                ));
            }
            // FSimpleDelegate: () -> void
            "FSimpleDelegate" => {
                // Check if the custom delegate has parameters that FSimpleDelegate doesn't provide
                let broadcast_args = self.get_default_broadcast_args(field_name);
                self.push_line(&format!(
                    ".{}(FSimpleDelegate::CreateLambda([=]() {{ auto D = {}; D.Broadcast({}); }}))",
                    property_name, formatted_args, broadcast_args
                ));
            }
            // FOnTextCommitted: (const FText&, ETextCommit::Type) -> void
            "FOnTextCommitted" => {
                self.push_line(&format!(
                    ".OnTextCommitted(FOnTextCommitted::CreateLambda([=](const FText& Text, ETextCommit::Type Type) {{ auto D = {}; D.Broadcast(Text, Type); }}))",
                    formatted_args
                ));
            }
            // FOnTextChanged: (const FText&) -> void
            "FOnTextChanged" => {
                self.push_line(&format!(
                    ".OnTextChanged(FOnTextChanged::CreateLambda([=](const FText& Text) {{ auto D = {}; D.Broadcast(Text); }}))",
                    formatted_args
                ));
            }
            // FOnCheckStateChanged: (ECheckBoxState) -> void
            "FOnCheckStateChanged" => {
                self.push_line(&format!(
                    ".OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) {{ auto D = {}; D.Broadcast(State); }}))",
                    formatted_args
                ));
            }
            // FOnLinearColorValueChanged: (FLinearColor) -> void
            "FOnLinearColorValueChanged" => {
                self.push_line(&format!(
                    ".OnColorChanged(FOnLinearColorValueChanged::CreateLambda([=](FLinearColor Color) {{ auto D = {}; D.Broadcast(Color); }}))",
                    formatted_args
                ));
            }
            // SComboBox::OnSelectionChanged commonly expects (TSharedPtr<FString>, ESelectInfo::Type)
            // and the generated InArgs delegate is often simpler. Bridge via _Lambda.
            "FOnSelectionChanged" => {
                let broadcast_args = self.get_default_broadcast_args(field_name);
                self.push_line(&format!(
                    ".OnSelectionChanged_Lambda([=](TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo) {{ auto D = {}; D.Broadcast({}); }})",
                    formatted_args, broadcast_args
                ));
            }
            // Default fallback: pass through directly
            _ => {
                self.push_line(&format!(".{}({})", property_name, formatted_args));
            }
        }
    }

    /// Get default-constructed Broadcast() arguments for a delegate field.
    /// When bridging from a parameterless native delegate (FOnClicked) to a
    /// parameterized custom delegate (FOnToolExecuted(EToolCategory)), we need
    /// to call Broadcast with default values for each parameter.
    fn get_default_broadcast_args(&self, field_name: &str) -> String {
        if let Some(param_types) = self.delegate_param_map.get(field_name) {
            if param_types.is_empty() {
                return String::new();
            }
            param_types
                .iter()
                .map(|t| {
                    // Generate default-constructed value for each C++ type
                    match t.as_str() {
                        "int32" | "int" => "0".to_string(),
                        "float" => "0.0f".to_string(),
                        "double" => "0.0".to_string(),
                        "bool" => "false".to_string(),
                        "FString" => "FString()".to_string(),
                        "FText" => "FText::GetEmpty()".to_string(),
                        "FName" => "FName()".to_string(),
                        "FVector" => "FVector::ZeroVector".to_string(),
                        "FVector2D" => "FVector2D::ZeroVector".to_string(),
                        "FLinearColor" => "FLinearColor::White".to_string(),
                        _ => {
                            // For enum types (E-prefixed) use static_cast from 0
                            if t.starts_with("E") {
                                format!("{}(0)", t)
                            } else {
                                // Default-construct any other type
                                format!("{}()", t)
                            }
                        }
                    }
                })
                .collect::<Vec<_>>()
                .join(", ")
        } else {
            String::new()
        }
    }

    /// Get the current widget class name for CreateSP bindings
    fn current_widget_class(&self) -> String {
        self.widget_class_name
            .clone()
            .unwrap_or_else(|| "Self".to_string())
    }

    /// Return true when a property method call is being applied to a slot expression
    /// (e.g. `VerticalBox().Add(...).Padding(...)`) instead of to a widget expression.
    fn is_slot_context_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::MethodCall {
                method, receiver, ..
            } => method == "Add" || method == "Slot" || self.is_slot_context_expr(receiver),
            _ => false,
        }
    }

    fn generate_slate_args(&mut self, st: &Struct, widget_name: &str) {
        // Use LayoutOptimizer for smart ARGUMENT vs ATTRIBUTE classification
        let mut optimizer = LayoutOptimizer::new();
        let analyses = optimizer.analyze_widget(st);

        // Clear and populate field_type_map for delegate type checking during Construct
        self.field_type_map.clear();

        // Pre-pass: collect all delegate types that will be used, and emit
        // DECLARE_DELEGATE for any that aren't known engine or registered custom delegates.
        let mut declared_delegates: std::collections::HashSet<String> =
            std::collections::HashSet::new();
        for (field, analysis) in st.fields.iter().zip(analyses.iter()) {
            let is_event_field = matches!(analysis.reactivity, PropertyReactivity::Event)
                || field.name.starts_with("on_")
                || field.name.starts_with("On");
            if is_event_field {
                let cpp_type = self.map_type(&field.ty);
                let delegate_type = self.map_event_delegate_type(&field.name, &cpp_type);
                // Check if this delegate type needs a forward declaration
                if delegate_type.starts_with("F")
                    && delegate_type.len() > 1
                    && !declared_delegates.contains(&delegate_type)
                {
                    let is_known = self.is_known_delegate_type(&delegate_type);
                    if !is_known {
                        if delegate_type == "FOnSelectionChanged" {
                            self.push_line("DECLARE_DELEGATE_TwoParams(FOnSelectionChanged, TSharedPtr<FString>, ESelectInfo::Type);");
                        } else {
                            self.push_line(&format!("DECLARE_DELEGATE({});", delegate_type));
                        }
                        declared_delegates.insert(delegate_type);
                    }
                }
            }
        }
        if !declared_delegates.is_empty() {
            self.push_line("");
        }

        // Emit optimization report as comment
        let report = optimizer.generate_report(&analyses);
        for line in report.lines() {
            self.push_line(line);
        }

        self.push_line(&format!("SLATE_BEGIN_ARGS({})", widget_name));
        self.indent += 1;

        // Constructor initializer list
        self.push_line(": _Content()");

        for field in &st.fields {
            // Check if this field is a delegate/event type
            let is_delegate = field.attributes.iter().any(|a| a.name == "event")
                || field.name.starts_with("on_")
                || field.name.starts_with("On");

            if is_delegate {
                // Delegates need explicit () for value-initialization in the initializer list
                // Map the field type to the proper UE5 delegate type for construction
                let cpp_type = self.map_type(&field.ty);
                let delegate_type = self.map_event_delegate_type(&field.name, &cpp_type);
                // Record the resolved delegate type for later use in Construct impl
                self.field_type_map
                    .insert(field.name.clone(), delegate_type.clone());
                self.push_line(&format!(", _{}({}())", field.name, delegate_type));
            } else {
                // For non-delegates, provide explicit default value
                let cpp_type = self.map_type(&field.ty);
                self.field_type_map.insert(field.name.clone(), cpp_type);
                let default_val = self.get_default_value(&field.ty);
                self.push_line(&format!(", _{}({})", field.name, default_val));
            }
        }

        self.push_line("{}");
        self.push_line("");

        // Default slot
        self.push_line("SLATE_DEFAULT_SLOT(FArguments, Content)");

        // Use optimizer analysis to pick correct macro for each field
        for (field, analysis) in st.fields.iter().zip(analyses.iter()) {
            let cpp_type = self.map_type(&field.ty);

            match analysis.reactivity {
                PropertyReactivity::Event => {
                    let delegate_type = self.map_event_delegate_type(&field.name, &cpp_type);
                    self.push_line(&format!("SLATE_EVENT({}, {})", delegate_type, field.name));
                }
                PropertyReactivity::Static => {
                    // For event-like fields (on_*, On*), map to proper delegate type
                    if field.name.starts_with("on_") || field.name.starts_with("On") {
                        let delegate_type = self.map_event_delegate_type(&field.name, &cpp_type);
                        self.push_line(&format!(
                            "SLATE_ARGUMENT({}, {})",
                            delegate_type, field.name
                        ));
                    } else {
                        self.push_line(&format!("SLATE_ARGUMENT({}, {})", cpp_type, field.name));
                    }
                }
                PropertyReactivity::Reactive => {
                    self.push_line(&format!("SLATE_ATTRIBUTE({}, {})", cpp_type, field.name));
                }
            }
        }

        self.indent -= 1;
        self.push_line("SLATE_END_ARGS()");
        self.push_line("");
    }

    /// Map event field names to proper UE5 delegate types.
    /// Queries the widget registry first (data-driven), then falls back to hardcoded mappings.
    fn map_event_delegate_type(&self, name: &str, cpp_type: &str) -> String {
        // First check if cpp_type is already a known delegate type.
        // Do NOT blindly trust any F* token because unresolved invented types
        // (e.g. FOnNodeSelected without a declaration) break SLATE_ARGUMENT expansion.
        if cpp_type.starts_with("F") && cpp_type.len() > 1 {
            let is_known_engine_delegate = matches!(
                cpp_type,
                "FOnClicked"
                    | "FSimpleDelegate"
                    | "FOnFloatValueChanged"
                    | "FOnTextCommitted"
                    | "FOnTextChanged"
                    | "FOnCheckStateChanged"
                    | "FOnLinearColorValueChanged"
                    | "FPointerEventHandler"
                    | "FKeyEventHandler"
                    | "FOnGenerateRow"
                    | "FOnGetChildren"
            );

            let is_known_custom_delegate = if let Some(ref ctx) = self.context {
                let stripped = cpp_type.strip_prefix('F').unwrap_or(cpp_type);
                ctx.delegate_names.contains(stripped)
            } else {
                false
            };

            if is_known_engine_delegate || is_known_custom_delegate {
                return cpp_type.to_string();
            }
        }

        // Query widget registry for the event name (data-driven from 2,346 widgets)
        if let Some(ref ctx) = self.context {
            // Normalize name: convert snake_case on_ prefix to PascalCase On prefix
            let pascal_name = if name.starts_with("on_") {
                let base = name.strip_prefix("on_").unwrap_or(name);
                format!("On{}", self.to_pascal_case(base))
            } else {
                name.to_string()
            };

            if let Some(delegate) = ctx.widget_registry.get_event_delegate_any(&pascal_name) {
                return delegate.to_string();
            }
        }

        // Hardcoded fallback for core events
        match name {
            "OnClicked" | "on_clicked" | "on_start_clicked" | "on_stop_clicked"
            | "on_pause_clicked" => "FOnClicked".to_string(),
            "OnPressed" | "OnReleased" | "OnHovered" | "OnUnhovered" => {
                "FSimpleDelegate".to_string()
            }
            "OnTextCommitted" | "on_text_committed" => "FOnTextCommitted".to_string(),
            "OnTextChanged" | "on_text_changed" => "FOnTextChanged".to_string(),
            "OnValueChanged" | "on_value_changed" => "FOnFloatValueChanged".to_string(),
            "OnCheckStateChanged" | "on_check_state_changed" => "FOnCheckStateChanged".to_string(),
            "OnSelectionChanged" | "on_selection_changed" => "FOnSelectionChanged".to_string(),
            "OnGenerateRow" | "on_generate_row" => "FOnGenerateRow".to_string(),
            "OnGetChildren" | "on_get_children" => "FOnGetChildren".to_string(),
            "OnMouseButtonDown" | "OnMouseButtonUp" => "FPointerEventHandler".to_string(),
            "OnKeyDown" | "OnKeyUp" => "FKeyEventHandler".to_string(),
            "OnColorChanged" | "on_color_changed" => "FOnLinearColorValueChanged".to_string(),
            _ => {
                // Check if it's a custom delegate from the context
                if let Some(ref ctx) = self.context {
                    let base_name = name.strip_prefix("on_").unwrap_or(name);
                    let pascal_name = self.to_pascal_case(base_name);

                    if ctx.delegate_names.contains(&pascal_name) {
                        return format!("F{}", pascal_name);
                    }
                }

                // Default fallback
                if cpp_type == "void" {
                    "FSimpleDelegate".to_string()
                } else {
                    cpp_type.to_string()
                }
            }
        }
    }

    /// Check if a delegate type is a known engine or registered custom delegate.
    fn is_known_delegate_type(&self, delegate_type: &str) -> bool {
        // Force explicit declaration for this Slate delegate in generated widget headers.
        // It is frequently needed even when registry/context suggests it exists elsewhere.
        if delegate_type == "FOnSelectionChanged" {
            return false;
        }

        let is_engine = matches!(
            delegate_type,
            "FOnClicked"
                | "FSimpleDelegate"
                | "FOnFloatValueChanged"
                | "FOnTextCommitted"
                | "FOnTextChanged"
                | "FOnCheckStateChanged"
                | "FOnLinearColorValueChanged"
                | "FPointerEventHandler"
                | "FKeyEventHandler"
                | "FOnGenerateRow"
                | "FOnGetChildren"
        );
        if is_engine {
            return true;
        }
        if let Some(ref ctx) = self.context {
            let stripped = delegate_type.strip_prefix('F').unwrap_or(delegate_type);
            if ctx.delegate_names.contains(stripped) {
                return true;
            }
        }
        false
    }

    /// Convert snake_case to PascalCase
    fn to_pascal_case(&self, s: &str) -> String {
        s.split('_')
            .map(|word| {
                let mut chars = word.chars();
                match chars.next() {
                    None => String::new(),
                    Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
                }
            })
            .collect()
    }

    fn generate_event_handlers(&mut self, st: &Struct) {
        let widget_model = self.resolve_widget_model(st);

        // Generate properly-typed handler declarations
        for field in &st.fields {
            if field.name.starts_with("On") || field.name.starts_with("on_") {
                let cpp_type = self.map_type(&field.ty);
                let delegate_type = self.map_event_delegate_type(&field.name, &cpp_type);
                self.push_line("");
                self.push_line(&self.handler_decl_for_delegate(&delegate_type, &field.name));
            }
        }

        // Generate handler methods for explicit @event functions
        for method in &st.methods {
            if method.name != "Compose" && method.name != "Construct" {
                if method.name == "GenerateWidgetForColumn"
                    && widget_model.construct_kind == SlateConstructKind::TableRow
                {
                    self.push_line("");
                    self.push_line("virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;");
                    continue;
                }
                if method.name == "OnOpening"
                    && widget_model.construct_kind == SlateConstructKind::ToolTip
                {
                    self.push_line("");
                    self.push_line("virtual void OnOpening() override;");
                    continue;
                }

                let params = method
                    .params
                    .iter()
                    .map(|p| format!("{} {}", self.map_type(&p.ty), p.name))
                    .collect::<Vec<_>>()
                    .join(", ");
                let ret_type = if let Some(ret_ty) = &method.return_type {
                    self.map_type(ret_ty)
                } else if method.name == "OnOpening" {
                    "void".to_string()
                } else if method.name.starts_with("On") || method.name.starts_with("Handle") {
                    "FReply".to_string()
                } else {
                    "void".to_string()
                };
                self.push_line("");
                self.push_line(&format!("{} {}({});", ret_type, method.name, params));
            }
        }
    }

    /// Build a handler declaration from a resolved delegate type.
    fn handler_decl_for_delegate(&self, delegate_type: &str, field_name: &str) -> String {
        match delegate_type {
            "FOnClicked" => format!("FReply Handle{}();", field_name),
            "FSimpleDelegate" => format!("void Handle{}();", field_name),
            "FOnFloatValueChanged" => format!("void Handle{}(float NewValue);", field_name),
            "FOnTextCommitted" => {
                format!(
                    "void Handle{}(const FText& InText, ETextCommit::Type CommitType);",
                    field_name
                )
            }
            "FOnTextChanged" => format!("void Handle{}(const FText& InText);", field_name),
            "FOnCheckStateChanged" => {
                format!("void Handle{}(ECheckBoxState NewState);", field_name)
            }
            "FOnSelectionChanged" => {
                format!(
                    "void Handle{}(TSharedPtr<FString> InItem, ESelectInfo::Type SelectInfo);",
                    field_name
                )
            }
            "FOnLinearColorValueChanged" => {
                format!("void Handle{}(FLinearColor NewColor);", field_name)
            }
            "FPointerEventHandler" => {
                format!(
                    "FReply Handle{}(const FGeometry& Geometry, const FPointerEvent& MouseEvent);",
                    field_name
                )
            }
            "FKeyEventHandler" => {
                format!(
                    "FReply Handle{}(const FGeometry& Geometry, const FKeyEvent& KeyEvent);",
                    field_name
                )
            }
            "FOnGenerateRow" => {
                format!(
                    "TSharedRef<ITableRow> Handle{}(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);",
                    field_name
                )
            }
            "FOnGetChildren" => {
                format!(
                    "void Handle{}(TSharedPtr<FString> InItem, TArray<TSharedPtr<FString>>& OutChildren);",
                    field_name
                )
            }
            _ => format!("void Handle{}();", field_name),
        }
    }

    fn has_list_data(&self, st: &Struct) -> bool {
        st.fields.iter().any(|f| {
            matches!(&f.ty, Type::Array(_, _, _))
                || matches!(&f.ty, Type::Named { name, generics, .. } if name.eq_ignore_ascii_case("Array") && !generics.is_empty())
        })
    }

    fn generate_list_view_support(&mut self, st: &Struct) {
        self.push_line("");
        self.push_line("// === List View Support ===");

        for field in &st.fields {
            let element_opt = match &field.ty {
                Type::Array(element, _, _) => Some(element.as_ref()),
                Type::Named { name, generics, .. }
                    if name.eq_ignore_ascii_case("Array") && !generics.is_empty() =>
                {
                    Some(&generics[0])
                }
                _ => None,
            };

            if let Some(element) = element_opt {
                let element_type = self.map_type(element);
                let ptr_type = format!("TSharedPtr<{}>", element_type);

                // Bug-2 fix: record element type so SNew(SListView<T>) can use it.
                self.list_item_types
                    .insert(field.name.clone(), element_type.clone());

                // Member variable for the list source
                self.push_line(&format!("TArray<{}> {};", ptr_type, field.name));
                self.push_line("");

                // Selection variable
                self.push_line(&format!("{} Selected{}Item;", ptr_type, field.name));
                self.push_line("");

                // OnGenerateRow delegate with proper signature
                self.push_line(&format!(
                    "TSharedRef<ITableRow> OnGenerateRow_{}({} InItem, const TSharedRef<STableViewBase>& OwnerTable);",
                    field.name, ptr_type
                ));

                // OnSelectionChanged delegate
                self.push_line(&format!(
                    "void OnSelectionChanged_{}({} InItem, ESelectInfo::Type SelectInfo);",
                    field.name, ptr_type
                ));
                self.push_line("");

                // ListView widget reference — correctly typed
                self.push_line(&format!(
                    "TSharedPtr<SListView<{}>> {}ListView;",
                    ptr_type, field.name
                ));
            }
        }
    }

    fn infer_list_item_type_from_method_calls(
        &self,
        method_calls: &[MethodCallInfo],
    ) -> Option<String> {
        for call in method_calls {
            if call.method == "ListItemsSource" {
                if let Some(arg) = call.args.first() {
                    if let Some(item_ty) = self.infer_list_item_type_from_expr(&arg.value) {
                        return Some(item_ty);
                    }
                }
            }
        }
        None
    }

    fn infer_list_item_type_from_expr(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::Ident(name, _) => self.list_item_types.get(name).cloned(),
            Expr::Field { object, field, .. } => {
                if let Expr::Ident(base, _) = &**object {
                    if base == "InArgs" {
                        return self.list_item_types.get(field).cloned();
                    }
                }
                None
            }
            _ => None,
        }
    }

    /// Return the SNew type string for a list widget, including the `<ItemPtr>` template arg
    /// when an item type is known from a previous `generate_list_view_support()` call.
    #[allow(dead_code)]
    fn list_widget_stype(&self, slate_class: &str) -> String {
        self.list_widget_stype_for(slate_class, None)
    }

    fn list_widget_stype_for(
        &self,
        slate_class: &str,
        preferred_item_type: Option<&str>,
    ) -> String {
        if let Some(elem) = preferred_item_type {
            return format!("SNew({}<TSharedPtr<{}>>)", slate_class, elem);
        }

        // Deterministic fallback: only use implicit list type when there is exactly one list source.
        if self.list_item_types.len() == 1 {
            if let Some(elem) = self.list_item_types.values().next() {
                return format!("SNew({}<TSharedPtr<{}>>)", slate_class, elem);
            }
        }

        // Final fallback to a valid list item type accepted by Slate traits.
        format!("SNew({}<TSharedPtr<FString>>)", slate_class)
    }

    fn map_type(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                if name.eq_ignore_ascii_case("Array") && !generics.is_empty() {
                    return format!("TArray<{}>", self.map_type(&generics[0]));
                }

                match name.as_str() {
                    "Int" | "int" => "int32".to_string(),
                    "Float" | "float" => "float".to_string(),
                    "Bool" | "bool" => "bool".to_string(),
                    "String" | "str" => "FString".to_string(),
                    "Text" => "FText".to_string(),
                    "Color" => "FLinearColor".to_string(),
                    "Vec2" => "FVector2D".to_string(),
                    "Vec3" => "FVector".to_string(),
                    "Vec4" => "FVector4".to_string(),
                    "Brush" => "const FSlateBrush*".to_string(),
                    "Margin" => "FMargin".to_string(),
                    // Preserve known engine Slate delegate types as-is.
                    "FOnClicked"
                    | "FSimpleDelegate"
                    | "FOnFloatValueChanged"
                    | "FOnTextCommitted"
                    | "FOnTextChanged"
                    | "FOnCheckStateChanged"
                    | "FOnSelectionChanged"
                    | "FOnLinearColorValueChanged"
                    | "FPointerEventHandler"
                    | "FKeyEventHandler"
                    | "FOnGenerateRow"
                    | "FOnGetChildren" => name.clone(),
                    _ => {
                        // Use context to map custom types (enums, structs, actors, delegates)
                        if let Some(ref ctx) = self.context {
                            // Check if it's an enum — handle both canonical and explicit E-prefixed references.
                            let enum_base = name.strip_prefix('E').unwrap_or(name);
                            let mapped_enum_name = naming::to_enum_name(enum_base);
                            if ctx.enum_names.contains(name)
                                || ctx.enum_names.contains(enum_base)
                                || ctx.enum_names.contains(&mapped_enum_name)
                                || ctx
                                    .enum_names
                                    .iter()
                                    .any(|e| naming::to_enum_name(e) == mapped_enum_name)
                            {
                                return mapped_enum_name;
                            }
                            // Check if it's a struct
                            if ctx.struct_names.contains(name) {
                                return naming::to_struct_name(name);
                            }
                            // Check if it's an actor
                            if ctx.actor_names.contains(name) {
                                return format!("{}*", naming::to_actor_name(name));
                            }
                            // Check if it's a component
                            if ctx.component_names.contains(name) {
                                return format!("{}*", naming::to_uobject_name(name));
                            }
                            // Check if it's a delegate
                            if ctx.delegate_names.contains(name) {
                                return naming::to_struct_name(name);
                            }
                        }
                        // Fallback: assume it's a custom type with F prefix
                        naming::to_struct_name(name)
                    }
                }
            }
            Type::Array(element, _, _) => {
                format!("TArray<{}>", self.map_type(element))
            }
            Type::Unit(_) => "void".to_string(),
            _ => "auto".to_string(),
        }
    }

    fn get_default_value(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                if name.eq_ignore_ascii_case("Array") && !generics.is_empty() {
                    return format!("TArray<{}>()", self.map_type(&generics[0]));
                }

                match name.as_str() {
                    "Int" | "int" => "0".to_string(),
                    "Float" | "float" => "0.0f".to_string(),
                    "Bool" | "bool" => "false".to_string(),
                    "String" | "str" => "FString()".to_string(),
                    "Text" => "FText::GetEmpty()".to_string(),
                    "Color" => "FLinearColor::White".to_string(),
                    "Vec2" => "FVector2D::ZeroVector".to_string(),
                    "Vec3" => "FVector::ZeroVector".to_string(),
                    "Vec4" => "FVector4(0, 0, 0, 0)".to_string(),
                    "Margin" => "FMargin(0)".to_string(),
                    "Brush" => "nullptr".to_string(),
                    _ => {
                        // Check if it's a delegate type (starts with On or on_)
                        if name.starts_with("On") || name.starts_with("on_") {
                            let delegate_type = self.map_event_delegate_type(name, "");
                            format!("{}()", delegate_type)
                        } else {
                            // Use map_type() to resolve custom types properly
                            // This ensures enums get E prefix, structs get F prefix, etc.
                            let mapped = self.map_type(ty);
                            if mapped.ends_with('*') {
                                // Pointer types default to nullptr
                                "nullptr".to_string()
                            } else {
                                format!("{}()", mapped)
                            }
                        }
                    }
                }
            }
            Type::Array(element, _, _) => {
                let element_type = self.map_type(element);
                format!("TArray<{}>()", element_type)
            }
            Type::Unit(_) => "FSimpleDelegate()".to_string(),
            _ => "{}".to_string(),
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
    use kain_core::ast::{
        Attribute, BinaryOp, Block, CallArg, Expr, Function, MatchArm, Pattern, Struct, Type,
        Visibility,
    };
    use kain_core::effects::Effect;
    use kain_core::span::Span;
    use kain_core::types::{ResolvedType, TypedStruct};
    use std::collections::HashMap;

    fn s() -> Span {
        Span::default()
    }

    fn call_arg(value: Expr) -> CallArg {
        CallArg {
            name: None,
            value,
            span: s(),
        }
    }

    fn widget_call(name: &str) -> Expr {
        Expr::Call {
            callee: Box::new(Expr::Ident(name.to_string(), s())),
            args: vec![],
            span: s(),
        }
    }

    fn method(name: &str, body: Block) -> Function {
        Function {
            name: name.to_string(),
            generics: vec![],
            params: vec![],
            return_type: None,
            effects: vec![Effect::Pure],
            body,
            visibility: Visibility::Public,
            attributes: vec![],
            span: s(),
        }
    }

    fn typed_struct(st: Struct) -> TypedStruct {
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
    fn test_widget_type_detection() {
        assert!(matches!(
            WidgetType::from_name("VerticalBox"),
            WidgetType::VerticalBox
        ));
        assert!(matches!(
            WidgetType::from_name("SHorizontalBox"),
            WidgetType::HorizontalBox
        ));
    }

    #[test]
    fn test_slot_awareness() {
        let vbox = WidgetType::VerticalBox;
        assert!(vbox.has_slots());
        assert_eq!(vbox.to_slate_class(), "SVerticalBox");
    }

    #[test]
    fn golden_generate_widget_for_column_emits_branching_guards_and_widget_returns() {
        let match_expr = Expr::Match {
            scrutinee: Box::new(Expr::Ident("ColumnName".to_string(), s())),
            arms: vec![
                MatchArm {
                    pattern: Pattern::Literal(Expr::String("Name".to_string(), s())),
                    guard: Some(Expr::Binary {
                        left: Box::new(Expr::Ident("bShowName".to_string(), s())),
                        op: BinaryOp::Eq,
                        right: Box::new(Expr::Bool(true, s())),
                        span: s(),
                    }),
                    body: widget_call("TextBlock"),
                    span: s(),
                },
                MatchArm {
                    pattern: Pattern::Or(
                        vec![
                            Pattern::Literal(Expr::String("Value".to_string(), s())),
                            Pattern::Literal(Expr::String("Amount".to_string(), s())),
                        ],
                        s(),
                    ),
                    guard: None,
                    body: widget_call("Button"),
                    span: s(),
                },
                MatchArm {
                    pattern: Pattern::Range {
                        start: Some(Box::new(Expr::Int(1, s()))),
                        end: Some(Box::new(Expr::Int(5, s()))),
                        inclusive: true,
                        span: s(),
                    },
                    guard: None,
                    body: widget_call("Text"),
                    span: s(),
                },
                MatchArm {
                    pattern: Pattern::Wildcard(s()),
                    guard: None,
                    body: widget_call("Image"),
                    span: s(),
                },
            ],
            span: s(),
        };

        let st = Struct {
            name: "InventoryRow".to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![method(
                "GenerateWidgetForColumn",
                Block {
                    stmts: vec![Stmt::Return(Some(match_expr), s())],
                    span: s(),
                },
            )],
            attributes: vec![Attribute {
                name: "table_row".to_string(),
                args: vec![Expr::String("FString".to_string(), s())],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed = typed_struct(st);
        let mut gen = SlateGenerator::new();
        let cpp = gen.generate_construct_impl(&typed, "SInventoryRow");

        assert!(cpp.contains(
            "TSharedRef<SWidget> SInventoryRow::GenerateWidgetForColumn(const FName& ColumnName)"
        ));
        assert!(cpp.contains("ColumnName == TEXT(\"Name\")"));
        assert!(
            cpp.contains("&&"),
            "Guard expressions should be combined with pattern conditions.\n{}",
            cpp
        );
        assert!(
            cpp.contains("||"),
            "Or-patterns should lower into || chains.\n{}",
            cpp
        );
        assert!(
            cpp.contains("ColumnName >= 1"),
            "Range-pattern lower bound should be emitted.\n{}",
            cpp
        );
        assert!(
            cpp.contains("ColumnName <= 5"),
            "Range-pattern upper bound should be emitted.\n{}",
            cpp
        );
        assert!(cpp.contains("SNew(STextBlock)"));
        assert!(cpp.contains("SNew(SButton)"));
        assert!(cpp.contains("SNew(SImage)"));
    }

    #[test]
    fn golden_on_opening_emits_branching_and_widget_content_calls() {
        let if_expr = Expr::If {
            condition: Box::new(Expr::Ident("bShowTooltip".to_string(), s())),
            then_branch: Block {
                stmts: vec![Stmt::Expr(Expr::Call {
                    callee: Box::new(Expr::Ident("SetContentWidget".to_string(), s())),
                    args: vec![call_arg(widget_call("TextBlock"))],
                    span: s(),
                })],
                span: s(),
            },
            else_branch: Some(Box::new(ElseBranch::Else(Block {
                stmts: vec![Stmt::Expr(Expr::Call {
                    callee: Box::new(Expr::Ident("SetContentWidget".to_string(), s())),
                    args: vec![call_arg(widget_call("Button"))],
                    span: s(),
                })],
                span: s(),
            }))),
            span: s(),
        };

        let st = Struct {
            name: "StatusTooltip".to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![method(
                "OnOpening",
                Block {
                    stmts: vec![Stmt::Expr(if_expr)],
                    span: s(),
                },
            )],
            attributes: vec![Attribute {
                name: "tooltip_widget".to_string(),
                args: vec![],
                span: s(),
            }],
            visibility: Visibility::Public,
            span: s(),
        };

        let typed = typed_struct(st);
        let mut gen = SlateGenerator::new();
        let cpp = gen.generate_construct_impl(&typed, "SStatusTooltip");

        assert!(cpp.contains("void SStatusTooltip::OnOpening()"));
        assert!(cpp.contains("SToolTip::OnOpening();"));
        assert!(cpp.contains("if (bShowTooltip)"));
        assert!(cpp.contains("SetContentWidget("));
        assert!(cpp.contains("SNew(STextBlock)"));
        assert!(cpp.contains("SNew(SButton)"));
    }
}
