//! KAIN Code Generation - UE5 Editor Tools
//! Generates Slate UI, Editor Modules, Asset Types, Details Customizations, and Viewports

use kain_core::types::{TypedProgram, TypedItem};
use kain_core::error::KainResult;
use kain_core::ast::Type;

// Import the UE5 support library
use ue5::ue5::Ue5Context;
use ue5::ue5::naming;

use crate::editor::slate::SlateGenerator;
use crate::editor::details::DetailsGenerator;
use crate::editor::viewport::ViewportGenerator;

/// Output from UE5 Editor codegen
pub struct Ue5EditorOutput {
    pub header: String,
    pub source: String,
}

/// Generate UE5 Editor C++ code
/// Takes an optional Ue5Context from the runtime pass to access actors/components
pub fn generate(program: &TypedProgram, plugin_name: &str, copyright: Option<&str>) -> KainResult<Ue5EditorOutput> {
    generate_with_context(program, plugin_name, None, copyright)
}

/// Generate UE5 Editor C++ code with a shared context from runtime codegen
pub fn generate_with_context(program: &TypedProgram, plugin_name: &str, runtime_context: Option<Ue5Context>, copyright: Option<&str>) -> KainResult<Ue5EditorOutput> {
    let mut gen = Ue5EditorGen::new(plugin_name, runtime_context, copyright);
    Ok(gen.gen_program(program))
}

/// A single editor item's generated output (for modular file output)
pub struct EditorItem {
    /// Output file name (e.g. "SInventoryPanel", "FWeaponDetailsCustomization")
    pub name: String,
    /// Kind of editor item (e.g. "Slate", "Details", "Viewport", "Toolbar", "AssetEditor", "EditorModule", "AssetType")
    pub kind: String,
    /// Generated header content
    pub header: String,
    /// Generated source content
    pub source: String,
}

/// Check if an attribute name is an editor attribute
/// Now queries the EditorAttributesRegistry from Ue5Context
pub fn is_editor_attribute(name: &str) -> bool {
    // Create a temporary context to access the registry
    // This is lightweight since the registry is loaded once at startup
    let ctx = Ue5Context::new("temp", None);
    ctx.editor_attributes.is_editor_attribute(name)
}

/// Get all known editor attribute names from the registry
pub fn get_editor_attributes() -> Vec<String> {
    let ctx = Ue5Context::new("temp", None);
    ctx.editor_attributes.attribute_names()
        .into_iter()
        .map(|s| s.to_string())
        .collect()
}

/// Generate per-item editor files for modular output.
/// Returns a Vec of EditorItem, one per @slate/@details/@viewport/etc struct.
pub fn generate_per_item(program: &TypedProgram, plugin_name: &str, copyright: Option<&str>) -> KainResult<Vec<EditorItem>> {
    let mut items = Vec::new();
    
    // Collect detail registrations across all items
    let mut detail_registrations = Vec::new();
    
    // Build a populated Ue5Context from the program's type information
    // This is CRITICAL for map_type() to correctly resolve enums (E prefix),
    // structs (F prefix), actors (A prefix + pointer), delegates (F prefix), etc.
    let mut shared_context = Ue5Context::new(plugin_name, copyright);
    for item in &program.items {
        match item {
            TypedItem::Enum(e) => {
                shared_context.register_enum(e.ast.name.clone(), format!("E{}.h", e.ast.name));
            }
            TypedItem::Struct(s) => {
                let is_component = s.ast.attributes.iter().any(|a| a.name == "component");
                if is_component {
                    shared_context.register_component(s.ast.name.clone(), format!("U{}.h", s.ast.name));
                } else {
                    shared_context.register_struct(s.ast.name.clone(), format!("F{}.h", s.ast.name));
                }
            }
            TypedItem::Actor(a) => {
                shared_context.register_actor(a.ast.name.clone(), format!("A{}.h", a.ast.name));
            }
            TypedItem::Component(c) => {
                shared_context.register_component(c.ast.name.clone(), format!("U{}.h", c.ast.name));
            }
            TypedItem::TypeAlias(alias) => {
                if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                    shared_context.register_delegate(alias.ast.name.clone(), format!("{}Delegates.h", plugin_name));
                }
            }
            _ => {}
        }
    }
    
    // Pre-collect all Slate widget names and their output header names
    // so we can resolve cross-widget includes in .cpp files
    let mut slate_widget_map: std::collections::HashMap<String, String> = std::collections::HashMap::new();
    for item in &program.items {
        if let TypedItem::Struct(st) = item {
            if st.ast.attributes.iter().any(|a| a.name == "slate") {
                let output_name = format!("S{}", st.ast.name);
                // Map both the Kain name and the S-prefixed name
                slate_widget_map.insert(st.ast.name.clone(), output_name.clone());
                slate_widget_map.insert(output_name.clone(), output_name.clone());
            }
        }
    }
    
    // Detect if program has any shaders (needed for module shader directory mapping)
    let program_has_shaders = program.items.iter().any(|item| matches!(item, TypedItem::Shader(_)));
    
    // Build delegate parameter type map from program type aliases
    // Used by SlateGenerator to generate correct Broadcast() calls in delegate bridges
    let mut delegate_param_types: std::collections::HashMap<String, Vec<String>> = std::collections::HashMap::new();
    {
        // We need a temporary gen just for map_type access
        let tmp_gen = Ue5EditorGen::new(plugin_name, Some(shared_context.clone()), copyright);
        for item in &program.items {
            if let TypedItem::TypeAlias(alias) = item {
                if let kain_core::ast::Type::Function { params, .. } = &alias.ast.target {
                    let cpp_params: Vec<String> = params.iter()
                        .map(|p| tmp_gen.map_type(p))
                        .collect();
                    delegate_param_types.insert(alias.ast.name.clone(), cpp_params);
                }
            }
        }
    }
    
    for item in &program.items {
        if let TypedItem::Struct(st) = item {
            let attrs: Vec<&str> = st.ast.attributes.iter().map(|a| a.name.as_str()).collect();
            
            let (kind, output_name) = if attrs.contains(&"slate") {
                ("Slate", format!("S{}", st.ast.name))
            } else if attrs.contains(&"details") {
                // Strip "Details" suffix if present to avoid duplication
                let base_name = st.ast.name.strip_suffix("Details").unwrap_or(&st.ast.name);
                ("Details", format!("F{}DetailsCustomization", base_name))
            } else if attrs.contains(&"viewport") {
                // Strip "Viewport" suffix if present to avoid duplication
                let base_name = st.ast.name.strip_suffix("Viewport").unwrap_or(&st.ast.name);
                ("Viewport", format!("S{}Viewport", base_name))
            } else if attrs.contains(&"toolbar") {
                ("Toolbar", format!("F{}Extension", st.ast.name))
            } else if attrs.contains(&"asset_editor") {
                ("AssetEditor", format!("F{}Toolkit", st.ast.name))
            } else if attrs.contains(&"editor_module") {
                ("EditorModule", format!("F{}Module", st.ast.name))
            } else if attrs.contains(&"asset_type") {
                ("AssetType", format!("U{}", st.ast.name))
            } else {
                continue; // Not an editor item
            };
            
            // Create a fresh generator with the shared context for correct type resolution
            let mut gen = Ue5EditorGen::new(plugin_name, Some(shared_context.clone()), copyright);
            gen.has_shaders = program_has_shaders;
            gen.delegate_param_types = delegate_param_types.clone();
            
            // Build item-specific includes
            gen.write_item_header_preamble(kind, plugin_name);
            gen.source.push_line("// Generated by KAIN - UE5 Editor Tools");
            gen.source.push_line(&format!("#include \"{}.h\"", output_name));
            if kind == "Slate" || kind == "Details" {
                gen.source.push_line("#include \"SlateOptMacros.h\"");
            }
            
            // For Slate widgets, scan Compose body for references to sibling widgets
            // and add their includes to the .cpp so SNew() can find the full class definition
            if kind == "Slate" {
                let mut sibling_includes: Vec<String> = Vec::new();
                // Scan struct fields for references to other Slate widgets
                for field in &st.ast.fields {
                    if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                        if let Some(header_name) = slate_widget_map.get(name.as_str()) {
                            if *header_name != output_name {
                                sibling_includes.push(header_name.clone());
                            }
                        }
                    }
                }
                // Scan Compose method body for widget constructor calls
                if let Some(compose_fn) = st.ast.methods.iter().find(|m| m.name == "Compose") {
                    collect_widget_refs_from_block(&compose_fn.body, &slate_widget_map, &output_name, &mut sibling_includes);
                }
                // Deduplicate and add includes
                sibling_includes.sort();
                sibling_includes.dedup();
                for sibling in &sibling_includes {
                    gen.source.push_line(&format!("#include \"{}.h\"", sibling));
                }
            }
            
            gen.write_blank_source();
            
            // Generate this specific item
            gen.gen_item(item);
            
            // Collect detail registrations
            detail_registrations.extend(gen.detail_registrations.clone());
            
            items.push(EditorItem {
                name: output_name,
                kind: kind.to_string(),
                header: gen.header.build(),
                source: gen.source.build(),
            });
        }
    }
    
    Ok(items)
}

/// Recursively scan a block for references to other Slate widgets (by constructor call name)
fn collect_widget_refs_from_block(
    block: &kain_core::ast::Block,
    widget_map: &std::collections::HashMap<String, String>,
    self_name: &str,
    out: &mut Vec<String>,
) {
    for stmt in &block.stmts {
        match stmt {
            kain_core::ast::Stmt::Let { value: Some(expr), .. } => {
                collect_widget_refs_from_expr(expr, widget_map, self_name, out);
            }
            kain_core::ast::Stmt::Expr(expr) => {
                collect_widget_refs_from_expr(expr, widget_map, self_name, out);
            }
            kain_core::ast::Stmt::Return(Some(expr), _) => {
                collect_widget_refs_from_expr(expr, widget_map, self_name, out);
            }
            _ => {}
        }
    }
}

/// Recursively scan an expression for references to other Slate widgets
fn collect_widget_refs_from_expr(
    expr: &kain_core::ast::Expr,
    widget_map: &std::collections::HashMap<String, String>,
    self_name: &str,
    out: &mut Vec<String>,
) {
    match expr {
        kain_core::ast::Expr::Call { callee, args, .. } => {
            if let kain_core::ast::Expr::Ident(name, _) = &**callee {
                if let Some(header_name) = widget_map.get(name.as_str()) {
                    if *header_name != self_name {
                        out.push(header_name.clone());
                    }
                }
            }
            for arg in args {
                collect_widget_refs_from_expr(&arg.value, widget_map, self_name, out);
            }
        }
        kain_core::ast::Expr::MethodCall { receiver, args, .. } => {
            collect_widget_refs_from_expr(receiver, widget_map, self_name, out);
            for arg in args {
                collect_widget_refs_from_expr(&arg.value, widget_map, self_name, out);
            }
        }
        kain_core::ast::Expr::Ident(name, _) => {
            if let Some(header_name) = widget_map.get(name.as_str()) {
                if *header_name != self_name {
                    out.push(header_name.clone());
                }
            }
        }
        _ => {}
    }
}

struct StringBuilder {
    lines: Vec<String>,
}

impl StringBuilder {
    fn new() -> Self {
        Self { lines: Vec::new() }
    }

    fn push_line(&mut self, text: &str) {
        self.lines.push(format!("{}\n", text));
    }

    fn build(&self) -> String {
        self.lines.join("")
    }
}

struct Ue5EditorGen {
    header: StringBuilder,
    source: StringBuilder,
    indent: usize,
    context: Ue5Context,
    plugin_name: String,
    /// Track detail customization registrations for module startup
    detail_registrations: Vec<String>,
    /// Whether the plugin has shaders (needs directory mapping in StartupModule)
    has_shaders: bool,
    /// Map of delegate type alias names → their C++ parameter types
    /// e.g. "OnToolExecuted" → ["EEToolCategory"], "OnValueChanged" → ["float"]
    /// Built from program TypeAlias items, used to populate SlateGenerator's delegate_param_map
    delegate_param_types: std::collections::HashMap<String, Vec<String>>,
}

impl Ue5EditorGen {
    fn new(plugin_name: &str, runtime_context: Option<Ue5Context>, copyright: Option<&str>) -> Self {
        let context = runtime_context.unwrap_or_else(|| Ue5Context::new("EditorTools", copyright));
        
        Self {
            header: StringBuilder::new(),
            source: StringBuilder::new(),
            indent: 0,
            context,
            plugin_name: plugin_name.to_string(),
            detail_registrations: Vec::new(),
            has_shaders: false,
            delegate_param_types: std::collections::HashMap::new(),
        }
    }

    /// Write per-item header preamble with only the includes needed for this kind
    fn write_item_header_preamble(&mut self, kind: &str, plugin_name: &str) {
        self.header.push_line("// Generated by KAIN - UE5 Editor Tools");
        self.header.push_line("#pragma once");
        self.header.push_line("#include \"CoreMinimal.h\"");
        
        // For Slate widgets and editor tools, include the EditorTypes header
        // This provides all runtime types + delegates without circular dependencies
        if kind == "Slate" || kind == "Details" || kind == "Viewport" || kind == "AssetEditor" {
            self.header.push_line(&format!("#include \"{}EditorTypes.h\"", plugin_name));
        } else {
            // For modules and other types, include main plugin header
            self.header.push_line(&format!("#include \"{}.h\"", plugin_name));
        }
        
        match kind {
            "Slate" => {
                self.header.push_line("#include \"Widgets/SCompoundWidget.h\"");
                self.header.push_line("#include \"Widgets/DeclarativeSyntaxSupport.h\"");
                self.header.push_line("#include \"Widgets/Views/SListView.h\"");
                self.header.push_line("#include \"Widgets/Input/SButton.h\"");
                self.header.push_line("#include \"Widgets/Input/SCheckBox.h\"");
                self.header.push_line("#include \"Widgets/Input/SComboBox.h\"");
                self.header.push_line("#include \"Widgets/Input/SEditableTextBox.h\"");
                self.header.push_line("#include \"Widgets/Input/SSlider.h\"");
                self.header.push_line("#include \"Widgets/Input/SSpinBox.h\"");
                self.header.push_line("#include \"Widgets/Text/STextBlock.h\"");
                self.header.push_line("#include \"Widgets/Images/SImage.h\"");
                self.header.push_line("#include \"Widgets/Layout/SScrollBox.h\"");
                self.header.push_line("#include \"Widgets/Layout/SSplitter.h\"");
                self.header.push_line("#include \"Widgets/Layout/SBorder.h\"");
                self.header.push_line("#include \"Widgets/Colors/SColorBlock.h\"");
            }
            "Details" => {
                self.header.push_line("#include \"IDetailCustomization.h\"");
                self.header.push_line("#include \"DetailLayoutBuilder.h\"");
                self.header.push_line("#include \"DetailCategoryBuilder.h\"");
                self.header.push_line("#include \"DetailWidgetRow.h\"");
                self.header.push_line("#include \"PropertyEditorModule.h\"");
                self.header.push_line("#include \"Widgets/Input/SSpinBox.h\"");
                self.header.push_line("#include \"Widgets/Input/SButton.h\"");
                self.header.push_line("#include \"Widgets/Colors/SColorBlock.h\"");
            }
            "Viewport" => {
                self.header.push_line("#include \"SEditorViewport.h\"");
                self.header.push_line("#include \"EditorViewportClient.h\"");
                self.header.push_line("#include \"PreviewScene.h\"");
            }
            "Toolbar" => {
                self.header.push_line("#include \"Framework/MultiBox/MultiBoxBuilder.h\"");
                self.header.push_line("#include \"Styling/AppStyle.h\"");
            }
            "AssetEditor" => {
                self.header.push_line("#include \"Toolkits/AssetEditorToolkit.h\"");
            }
            "EditorModule" => {
                self.header.push_line("#include \"Modules/ModuleInterface.h\"");
                self.header.push_line("#include \"Modules/ModuleManager.h\"");
                if self.has_shaders {
                    self.header.push_line("#include \"Interfaces/IPluginManager.h\"");
                    self.header.push_line("#include \"ShaderCore.h\"");
                }
            }
            "AssetType" => {
                self.header.push_line("#include \"Engine/DataAsset.h\"");
                self.header.push_line("#include \"AssetTypeActions_Base.h\"");
                self.header.push_line("#include \"AssetTypeCategories.h\"");
            }
            _ => {}
        }
        
        self.write_blank_header();
    }

    fn push_indent(&mut self) {
        self.indent += 1;
    }

    fn pop_indent(&mut self) {
        if self.indent > 0 {
            self.indent -= 1;
        }
    }

    fn indent_str(&self) -> String {
        "\t".repeat(self.indent)
    }

    fn write_header(&mut self, line: &str) {
        let indented = format!("{}{}", self.indent_str(), line);
        self.header.push_line(&indented);
    }

    fn write_source(&mut self, line: &str) {
        let indented = format!("{}{}", self.indent_str(), line);
        self.source.push_line(&indented);
    }
    
    fn write_blank_header(&mut self) {
        self.header.push_line("");
    }

    fn write_blank_source(&mut self) {
        self.source.push_line("");
    }

    fn gen_program(&mut self, program: &TypedProgram) -> Ue5EditorOutput {
        // Detect what features are used
        let has_asset_types = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "asset_type"))
        });
        let has_slate = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "slate"))
        });
        let has_modules = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "editor_module"))
        });
        let has_details = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "details"))
        });
        let has_viewports = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "viewport"))
        });
        let has_toolbars = program.items.iter().any(|item| {
            matches!(item, TypedItem::Struct(st) if st.ast.attributes.iter().any(|a| a.name == "toolbar"))
        });
        
        // Track features for automatic dependency management
        if has_slate || has_details || has_viewports {
            self.context.use_feature("Slate");
        }
        if has_asset_types {
            self.context.use_feature("Projects");
        }
        if has_details {
            self.context.use_feature("PropertyEditor");
        }
        if has_viewports {
            self.context.use_feature("AdvancedPreviewScene");
        }

        // Build delegate parameter type map from program type aliases
        // e.g. type OnToolExecuted = delegate(EToolCategory) → "OnToolExecuted" → ["EEToolCategory"]
        for item in &program.items {
            if let TypedItem::TypeAlias(alias) = item {
                if let Type::Function { params, .. } = &alias.ast.target {
                    let cpp_params: Vec<String> = params.iter()
                        .map(|p| self.map_type(p))
                        .collect();
                    self.delegate_param_types.insert(alias.ast.name.clone(), cpp_params);
                }
            }
        }

        // === HEADER ===
        self.header.push_line("// Generated by KAIN - UE5 Editor Tools");
        self.header.push_line("#pragma once");
        self.header.push_line("#include \"CoreMinimal.h\"");
        
        // Include main plugin header to access delegates and types
        self.header.push_line(&format!("#include \"{}.h\"", self.plugin_name));
        
        // Feature-based includes
        if has_asset_types {
            self.header.push_line("#include \"Engine/DataAsset.h\"");
        }
        
        if has_slate {
            self.header.push_line("#include \"Widgets/SCompoundWidget.h\"");
            self.header.push_line("#include \"Widgets/DeclarativeSyntaxSupport.h\"");
            self.header.push_line("#include \"Widgets/Views/SListView.h\"");
            self.header.push_line("#include \"Widgets/Views/STreeView.h\"");
            self.header.push_line("#include \"Widgets/Input/SButton.h\"");
            self.header.push_line("#include \"Widgets/Input/SCheckBox.h\"");
            self.header.push_line("#include \"Widgets/Input/SComboBox.h\"");
            self.header.push_line("#include \"Widgets/Input/SEditableTextBox.h\"");
            self.header.push_line("#include \"Widgets/Input/SSlider.h\"");
            self.header.push_line("#include \"Widgets/Input/SSpinBox.h\"");
            self.header.push_line("#include \"Widgets/Text/STextBlock.h\"");
            self.header.push_line("#include \"Widgets/Images/SImage.h\"");
            self.header.push_line("#include \"Widgets/Layout/SScrollBox.h\"");
            self.header.push_line("#include \"Widgets/Layout/SSplitter.h\"");
            self.header.push_line("#include \"Widgets/Layout/SBorder.h\"");
        }
        
        if has_details {
            self.header.push_line("#include \"IDetailCustomization.h\"");
            self.header.push_line("#include \"DetailLayoutBuilder.h\"");
            self.header.push_line("#include \"DetailCategoryBuilder.h\"");
            self.header.push_line("#include \"DetailWidgetRow.h\"");
            self.header.push_line("#include \"PropertyEditorModule.h\"");
        }
        
        if has_viewports {
            self.header.push_line("#include \"SEditorViewport.h\"");
            self.header.push_line("#include \"EditorViewportClient.h\"");
            self.header.push_line("#include \"PreviewScene.h\"");
        }
        
        if has_asset_types {
            self.header.push_line("#include \"AssetTypeActions_Base.h\"");
            self.header.push_line("#include \"AssetTypeCategories.h\"");
        }
        
        if has_modules {
            self.header.push_line("#include \"Modules/ModuleInterface.h\"");
            self.header.push_line("#include \"Modules/ModuleManager.h\"");
        }
        
        self.write_blank_header();

        // === SOURCE ===
        self.source.push_line("// Generated by KAIN - UE5 Editor Tools");
        self.source.push_line(&format!("#include \"{}Editor.h\"", self.plugin_name));
        if has_slate || has_details {
            self.source.push_line("#include \"SlateOptMacros.h\"");
        }
        self.write_blank_source();

        // Process all items
        for item in &program.items {
            self.gen_item(item);
        }

        Ue5EditorOutput {
            header: self.header.build(),
            source: self.source.build(),
        }
    }

    fn gen_item(&mut self, item: &TypedItem) {
        match item {
            TypedItem::Struct(st) => {
                let attrs: Vec<&str> = st.ast.attributes.iter().map(|a| a.name.as_str()).collect();
                
                if attrs.contains(&"slate") {
                    self.gen_slate_widget(st);
                } else if attrs.contains(&"details") {
                    self.gen_details_customization(st);
                } else if attrs.contains(&"viewport") {
                    self.gen_viewport(st);
                } else if attrs.contains(&"toolbar") {
                    self.gen_toolbar(st);
                } else if attrs.contains(&"asset_editor") {
                    self.gen_asset_editor(st);
                } else if attrs.contains(&"editor_module") {
                    self.gen_editor_module(st);
                } else if attrs.contains(&"asset_type") {
                    self.gen_asset_type(st);
                }
            },
            _ => {}
        }
    }

    fn gen_slate_widget(&mut self, st: &kain_core::types::TypedStruct) {
        let mut slate_gen = SlateGenerator::new().with_context(self.context.clone());
        
        // Register delegate parameter types for event fields so the bridge
        // can generate correct Broadcast() calls with default args
        for field in &st.ast.fields {
            let is_event = field.attributes.iter().any(|a| a.name == "event") ||
                           field.name.starts_with("on_") || field.name.starts_with("On");
            if is_event {
                // Resolve the field's type name to its delegate param types
                if let Type::Named { name, .. } = &field.ty {
                    if let Some(params) = self.delegate_param_types.get(name) {
                        slate_gen.register_delegate_params(&field.name, params.clone());
                    }
                }
            }
        }
        
        // Generate header (class declaration)
        let header_code = slate_gen.generate_widget(st);
        self.header.push_line(&header_code);
        self.write_blank_header();

        // Generate source (implementation)
        let widget_name = format!("S{}", st.ast.name);
        let source_code = slate_gen.generate_construct_impl(st, &widget_name);
        self.source.push_line(&source_code);
        self.write_blank_source();
    }
    
    fn gen_details_customization(&mut self, st: &kain_core::types::TypedStruct) {
        let mut details_gen = DetailsGenerator::new();
        
        let (header_code, source_code) = details_gen.generate_customization(st);
        self.header.push_line(&header_code);
        self.write_blank_header();
        
        self.source.push_line(&source_code);
        self.write_blank_source();
        
        // Store registration for module startup
        let registration = details_gen.generate_registration(st);
        self.detail_registrations.push(registration);
    }
    
    fn gen_viewport(&mut self, st: &kain_core::types::TypedStruct) {
        let mut viewport_gen = ViewportGenerator::new();
        
        let (header_code, source_code) = viewport_gen.generate_viewport(st);
        self.header.push_line(&header_code);
        self.write_blank_header();
        
        self.source.push_line(&source_code);
        self.write_blank_source();
    }
    
    fn gen_toolbar(&mut self, st: &kain_core::types::TypedStruct) {
        let toolbar_name = &st.ast.name;
        
        // Header: toolbar extension class
        self.write_header(&format!("class F{}Extension", toolbar_name));
        self.write_header("{");
        self.write_header("public:");
        self.push_indent();
        self.write_header("static void RegisterToolbar(FToolBarBuilder& Builder);");
        self.write_header("");
        
        // Button handlers from @button fields/methods
        for field in &st.ast.fields {
            if field.attributes.iter().any(|a| a.name == "button") {
                self.write_header(&format!("static void On{}();", field.name));
            }
        }
        for method in &st.ast.methods {
            if method.attributes.iter().any(|a| a.name == "button") {
                self.write_header(&format!("static void {}();", method.name));
            }
        }
        
        // Toggle fields
        for field in &st.ast.fields {
            if field.attributes.iter().any(|a| a.name == "toggle") {
                self.write_header(&format!("static bool {};\n", field.name));
            }
        }
        
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();
        
        // Source: RegisterToolbar implementation
        self.write_source(&format!("void F{}Extension::RegisterToolbar(FToolBarBuilder& Builder)", toolbar_name));
        self.write_source("{");
        self.push_indent();
        
        for field in &st.ast.fields {
            if let Some(btn_attr) = field.attributes.iter().find(|a| a.name == "button") {
                let label = btn_attr.args.first()
                    .and_then(|a| if let kain_core::ast::Expr::String(s, _) = a { Some(s.clone()) } else { None })
                    .unwrap_or_else(|| field.name.clone());
                
                // Look for icon attribute
                let icon = field.attributes.iter()
                    .find(|a| a.name == "icon" || a.name == "button")
                    .and_then(|a| a.args.get(1))
                    .and_then(|a| if let kain_core::ast::Expr::String(s, _) = a { Some(s.clone()) } else { None });
                
                self.write_source(&format!(
                    "Builder.AddToolBarButton(FUIAction(FExecuteAction::CreateStatic(&F{}Extension::On{})),",
                    toolbar_name, field.name
                ));
                self.push_indent();
                self.write_source("NAME_None,");
                self.write_source(&format!("FText::FromString(TEXT(\"{}\")),", label));
                self.write_source(&format!("FText::FromString(TEXT(\"{}\")),", label));
                if let Some(icon_name) = icon {
                    self.write_source(&format!("FSlateIcon(FAppStyle::GetAppStyleSetName(), \"{}\"));", icon_name));
                } else {
                    self.write_source("FSlateIcon());");
                }
                self.pop_indent();
                self.write_source("");
            }
        }
        
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        // Button handler stubs
        for method in &st.ast.methods {
            if method.attributes.iter().any(|a| a.name == "button") {
                self.write_source(&format!("void F{}Extension::{}()", toolbar_name, method.name));
                self.write_source("{");
                self.push_indent();
                self.write_source("// TODO: Implement toolbar action");
                self.pop_indent();
                self.write_source("}");
                self.write_blank_source();
            }
        }
    }
    
    fn gen_asset_editor(&mut self, st: &kain_core::types::TypedStruct) {
        let editor_name = &st.ast.name;
        let class_name = format!("F{}Toolkit", editor_name);
        
        // Forward-declare any viewport widget classes referenced as members
        for field in &st.ast.fields {
            let field_attrs: Vec<&str> = field.attributes.iter().map(|a| a.name.as_str()).collect();
            if field_attrs.contains(&"viewport") {
                let raw_name = if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                    name.clone()
                } else {
                    self.map_type(&field.ty)
                };
                let widget_name = format!("S{}", raw_name);
                self.write_header(&format!("class {};", widget_name));
            }
        }
        self.write_header("");
        
        // Header: FAssetEditorToolkit subclass
        self.write_header(&format!("class {} : public FAssetEditorToolkit", class_name));
        self.write_header("{");
        self.write_header("public:");
        self.push_indent();
        
        self.write_header(&format!("{}();", class_name));
        self.write_header(&format!("virtual ~{}();", class_name));
        self.write_header("");
        self.write_header("void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InAsset);");
        self.write_header("");
        
        // FAssetEditorToolkit interface
        self.write_header("virtual FName GetToolkitFName() const override;");
        self.write_header("virtual FText GetBaseToolkitName() const override;");
        self.write_header("virtual FString GetWorldCentricTabPrefix() const override;");
        self.write_header("virtual FLinearColor GetWorldCentricTabColorScale() const override;");
        self.write_header("virtual void OnClose() override;");
        self.write_header("");
        
        // Custom methods from struct
        for method in &st.ast.methods {
            self.write_header(&format!("void {}();", method.name));
        }
        
        self.pop_indent();
        self.write_header("private:");
        self.push_indent();
        
        // Member variables for sub-components
        for field in &st.ast.fields {
            let field_attrs: Vec<&str> = field.attributes.iter().map(|a| a.name.as_str()).collect();
            if field_attrs.contains(&"asset") {
                self.write_header(&format!("TWeakObjectPtr<UObject> EditingAsset;"));
            } else if field_attrs.contains(&"viewport") {
                // Viewport widgets use S-prefix (Slate), not F-prefix (struct).
                // Extract raw type name and apply S-prefix directly.
                let raw_name = if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                    name.clone()
                } else {
                    self.map_type(&field.ty)
                };
                let widget_name = format!("S{}", raw_name);
                self.write_header(&format!("TSharedPtr<{}> ViewportWidget;", widget_name));
            } else if field_attrs.contains(&"details") {
                self.write_header("TSharedPtr<IDetailsView> DetailsView;");
            }
        }
        
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();
        
        // Source: basic implementation
        self.write_source(&format!("{}::{}() {{}}", class_name, class_name));
        self.write_source(&format!("{}::~{}() {{}}", class_name, class_name));
        self.write_blank_source();
        
        self.write_source(&format!("FName {}::GetToolkitFName() const", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source(&format!("return FName(\"{}\");", editor_name));
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        self.write_source(&format!("FText {}::GetBaseToolkitName() const", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source(&format!("return FText::FromString(TEXT(\"{}\"));", editor_name));
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        self.write_source(&format!("FString {}::GetWorldCentricTabPrefix() const", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source(&format!("return TEXT(\"{}\");", editor_name));
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        self.write_source(&format!("FLinearColor {}::GetWorldCentricTabColorScale() const", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source("return FLinearColor::White;");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        self.write_source(&format!("void {}::OnClose()", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source("FAssetEditorToolkit::OnClose();");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
    }

    fn map_type(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                let ue_name = match name.as_str() {
                    "Int" | "int" | "i32" => "int32".to_string(),
                    "Float" | "float" | "f32" => "float".to_string(),
                    "Double" | "f64" => "double".to_string(),
                    "Bool" | "bool" => "bool".to_string(),
                    "String" | "str" => "FString".to_string(),
                    "Name" => "FName".to_string(),
                    "Vec2" => "FVector2D".to_string(),
                    "Vec3" | "Vector" => "FVector".to_string(),
                    "Vec4" => "FVector4".to_string(),
                    "Rot" | "Rotator" => "FRotator".to_string(),
                    "Quat" => "FQuat".to_string(),
                    "Transform" => "FTransform".to_string(),
                    "Color" | "LinearColor" => "FLinearColor".to_string(),
                    "Text" => "FText".to_string(),
                    "Brush" => "const FSlateBrush*".to_string(),
                    "Margin" => "FMargin".to_string(),
                    "Byte" | "uint8" => "uint8".to_string(),
                    "Int64" | "i64" => "int64".to_string(),
                    _ => {
                        // Check user-defined types registered in context
                        if let Some(header) = self.context.type_to_header.get(name).cloned() {
                            self.context.need_header(header);
                        }
                        if self.context.is_delegate(name) {
                            return naming::to_struct_name(name);
                        }
                        if self.context.is_component(name) {
                            return format!("{}*", naming::to_uobject_name(name));
                        }
                        if self.context.is_enum(name) {
                            return naming::to_enum_name(name);
                        }
                        if self.context.is_struct(name) {
                            return naming::to_struct_name(name);
                        }
                        if self.context.is_actor(name) {
                            return format!("{}*", naming::to_actor_name(name));
                        }

                        // === ENGINE KNOWLEDGE FALLBACK ===
                        // Check EngineKnowledge for engine types (components, actors, enums, structs)
                        let kb = &self.context.knowledge;

                        // Check type alias first (e.g. "NiagaraComponent" -> "UNiagaraComponent")
                        if let Some(resolved) = kb.resolve_type_alias(name) {
                            if let Some(header) = kb.get_include(resolved) {
                                self.context.need_header(header.to_string());
                            }
                            if let Some(module) = kb.get_module_for_type(resolved) {
                                self.context.need_module(module.to_string());
                            }
                            if kb.is_engine_component(resolved) {
                                return format!("{}*", resolved);
                            }
                            if kb.is_engine_actor(resolved) {
                                return format!("{}*", resolved);
                            }
                            return resolved.to_string();
                        }

                        // Auto-resolve include for this engine type
                        if let Some(header) = kb.get_include(name) {
                            self.context.need_header(header.to_string());
                        }

                        // Auto-add module dependency
                        if let Some(module) = kb.get_module_for_type(name) {
                            self.context.need_module(module.to_string());
                        }

                        // Engine component -> pointer type
                        if kb.is_engine_component(name) {
                            let prefixed = if name.starts_with('U') { name.to_string() } else { format!("U{}", name) };
                            return format!("{}*", prefixed);
                        }
                        // Engine actor -> pointer type
                        if kb.is_engine_actor(name) {
                            let prefixed = if name.starts_with('A') { name.to_string() } else { format!("A{}", name) };
                            return format!("{}*", prefixed);
                        }
                        // Engine enum -> E prefix
                        if kb.is_engine_enum(name) {
                            return if name.starts_with('E') { name.to_string() } else { format!("E{}", name) };
                        }
                        // Engine struct -> F prefix
                        if kb.is_engine_struct(name) {
                            return if name.starts_with('F') { name.to_string() } else { format!("F{}", name) };
                        }

                        // Unknown type - return as-is
                        name.clone()
                    }
                };

                // Handle generic containers (TArray<T>, TMap<K,V>, TSet<T>)
                if !generics.is_empty() {
                    let generic_args: Vec<String> = generics.iter().map(|g| self.map_type(g)).collect();
                    return format!("{}<{}>", ue_name, generic_args.join(", "));
                }

                ue_name
            },
            Type::Unit(_) => "void".to_string(),
            Type::Function { .. } => "FSimpleDelegate".to_string(),
            _ => "auto".to_string(),
        }
    }
    
    fn gen_editor_module(&mut self, st: &kain_core::types::TypedStruct) {
        let module_name = &st.ast.name;
        let class_name = format!("F{}Module", module_name);

        // Header
        self.write_header(&format!("class {} : public IModuleInterface", class_name));
        self.write_header("{");
        self.write_header("public:");
        self.push_indent();
        self.write_header("virtual void StartupModule() override;");
        self.write_header("virtual void ShutdownModule() override;");
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();

        // Source
        self.write_source(&format!("void {}::StartupModule()", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"{} has started!\"));", module_name));
        
        // Register shader directory mapping with duplicate guard
        // UE5 sometimes auto-maps /Plugin/{Name} for plugins with Shaders/ folders,
        // but this is not guaranteed. Check first to avoid duplicate assert.
        if self.has_shaders {
            self.write_source("");
            self.write_source("// Register shader directory (guarded against duplicate auto-registration)");
            self.write_source(&format!(
                "if (!AllShaderSourceDirectoryMappings().Contains(TEXT(\"/Plugin/{}\")))",
                self.plugin_name
            ));
            self.write_source("{");
            self.push_indent();
            self.write_source(&format!(
                "FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT(\"{}\"))->GetBaseDir(), TEXT(\"Shaders\"));",
                self.plugin_name
            ));
            self.write_source(&format!(
                "AddShaderSourceDirectoryMapping(TEXT(\"/Plugin/{}\"), PluginShaderDir);",
                self.plugin_name
            ));
            self.pop_indent();
            self.write_source("}");
        }
        
        // Insert detail customization registrations
        for registration in &self.detail_registrations.clone() {
            self.write_source("");
            self.write_source("// Register detail customization");
            self.source.push_line(registration);
        }
        
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        self.write_source(&format!("void {}::ShutdownModule()", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"{} has shut down!\"));", module_name));
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();

        // Module name must match .uplugin and Build.cs (plugin_name, not struct name)
        self.write_source(&format!("IMPLEMENT_MODULE({}, {})", class_name, self.plugin_name));
        self.write_blank_source();
    }

    fn gen_asset_type(&mut self, st: &kain_core::types::TypedStruct) {
        let asset_name = &st.ast.name;
        let module_api = "GENERATED_API";
        
        // Header: UDataAsset definition
        self.write_header("UCLASS(BlueprintType)");
        self.write_header(&format!("class {} U{} : public UDataAsset", module_api, asset_name));
        self.write_header("{");
        self.write_header("GENERATED_BODY()");
        self.write_header("public:");
        self.push_indent();
        for field in &st.ast.fields {
            self.write_header("UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Asset\")");
            self.write_header(&format!("{} {};", self.map_type(&field.ty), field.name));
        }
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();

        // Source: AssetTypeActions
        let actions_name = format!("FAssetTypeActions_{}", asset_name);
        
        self.write_source(&format!("class {} : public FAssetTypeActions_Base", actions_name));
        self.write_source("{");
        self.write_source("public:");
        self.push_indent();
        self.write_source(&format!("virtual FText GetName() const override {{ return NSLOCTEXT(\"AssetTypeActions\", \"AssetTypeActions_{}\", \"{}\"); }}", asset_name, asset_name));
        self.write_source("virtual FColor GetTypeColor() const override { return FColor::White; }");
        self.write_source(&format!("virtual UClass* GetSupportedClass() const override {{ return U{}::StaticClass(); }}", asset_name));
        self.write_source("virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }");
        self.pop_indent();
        self.write_source("};");
        self.write_blank_source();
    }
}
