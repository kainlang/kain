//! KAIN Code Generation - UE5 Editor Tools
//! Generates Slate UI, Editor Modules, Asset Types, Details Customizations, and Viewports

use kain_core::types::{TypedProgram, TypedItem};
use kain_core::error::KainResult;
use kain_core::ast::{Type, Expr, Stmt, Block, ElseBranch};
use std::collections::HashSet;

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

/// Check if an attribute name is an editor attribute.
/// Uses EditorAttributesRegistry when available, with a hardcoded fallback
/// to ensure detection works regardless of CWD (e.g., running from testing/CorpusTest/).
pub fn is_editor_attribute(name: &str) -> bool {
    // Hardcoded fallback — these MUST always be recognized as editor attributes
    const BUILTIN_EDITOR_ATTRS: &[&str] = &[
        "slate", "details", "property_customization", "viewport",
        "asset_editor", "editor_module", "commands", "toolbar", "menu",
    ];
    if BUILTIN_EDITOR_ATTRS.contains(&name) {
        return true;
    }
    // Also check the data-driven registry for any additional attributes
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
            } else if st.ast.attributes.iter().any(|a| a.name == "viewport") {
                let base_name = st.ast.name.strip_suffix("Viewport").unwrap_or(&st.ast.name);
                let output_name = format!("S{}Viewport", base_name);
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
        let mut tmp_gen = Ue5EditorGen::new(plugin_name, Some(shared_context.clone()), copyright);
        
        // Register all types with TypeMapper for correct prefix detection
        for item in &program.items {
            match item {
                TypedItem::Enum(e) => {
                    tmp_gen.type_mapper.register_enum(e.ast.name.clone());
                }
                TypedItem::Struct(s) => {
                    if s.ast.attributes.iter().any(|a| a.name == "component") {
                        tmp_gen.type_mapper.register_component(s.ast.name.clone());
                    } else {
                        tmp_gen.type_mapper.register_struct(s.ast.name.clone());
                    }
                }
                TypedItem::Actor(a) => {
                    tmp_gen.type_mapper.register_actor(a.ast.name.clone());
                }
                TypedItem::Component(c) => {
                    tmp_gen.type_mapper.register_component(c.ast.name.clone());
                }
                TypedItem::TypeAlias(alias) => {
                    if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                        tmp_gen.type_mapper.register_delegate(alias.ast.name.clone());
                    }
                }
                _ => {}
            }
        }
        
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
                // Strip trailing "Module" suffix to avoid double-Module file names.
                // e.g. KAIN struct "UltaModule" → "FUltaModule" (not "FUltaModuleModule")
                let base_name = st.ast.name.strip_suffix("Module").unwrap_or(&st.ast.name);
                ("EditorModule", format!("F{}Module", base_name))
            } else if attrs.contains(&"asset_type") {
                ("AssetType", format!("U{}", st.ast.name))
            } else {
                continue; // Not an editor item
            };
            
            // Create a fresh generator with the shared context for correct type resolution
            let mut gen = Ue5EditorGen::new(plugin_name, Some(shared_context.clone()), copyright);
            gen.has_shaders = program_has_shaders;
            gen.delegate_param_types = delegate_param_types.clone();
            
            // Register all types with TypeMapper for correct prefix detection
            for item in &program.items {
                match item {
                    TypedItem::Enum(e) => {
                        gen.type_mapper.register_enum(e.ast.name.clone());
                    }
                    TypedItem::Struct(s) => {
                        if s.ast.attributes.iter().any(|a| a.name == "component") {
                            gen.type_mapper.register_component(s.ast.name.clone());
                        } else {
                            gen.type_mapper.register_struct(s.ast.name.clone());
                        }
                    }
                    TypedItem::Actor(a) => {
                        gen.type_mapper.register_actor(a.ast.name.clone());
                    }
                    TypedItem::Component(c) => {
                        gen.type_mapper.register_component(c.ast.name.clone());
                    }
                    TypedItem::TypeAlias(alias) => {
                        if matches!(alias.ast.target, kain_core::ast::Type::Function { .. }) {
                            gen.type_mapper.register_delegate(alias.ast.name.clone());
                        }
                    }
                    _ => {}
                }
            }
            
            // Build item-specific includes
            // Bug-4 fix: for Slate widgets, scan the Compose body for widget names so
            // we only emit the Slate widget headers that are actually referenced.
            let used_widgets: HashSet<String> = if kind == "Slate" {
                collect_widget_names_from_struct(&st.ast)
            } else {
                HashSet::new()
            };
            gen.write_item_header_preamble(kind, plugin_name, &used_widgets);
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
            } else if kind == "AssetEditor" {
                // Asset editors often SNew() viewport/slate widgets; include their concrete headers.
                let mut widget_includes: Vec<String> = Vec::new();
                for field in &st.ast.fields {
                    if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                        if let Some(header_name) = slate_widget_map.get(name.as_str()) {
                            widget_includes.push(header_name.clone());
                        }
                    }
                }
                widget_includes.sort();
                widget_includes.dedup();
                for widget in &widget_includes {
                    gen.source.push_line(&format!("#include \"{}.h\"", widget));
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
    /// Centralized type mapper - single source of truth for type mapping
    type_mapper: ue5::ue5::types::TypeMapper,
}

impl Ue5EditorGen {
    fn new(plugin_name: &str, runtime_context: Option<Ue5Context>, copyright: Option<&str>) -> Self {
        let context = runtime_context.unwrap_or_else(|| Ue5Context::new("EditorTools", copyright));
        
        // Create TypeMapper with EngineKnowledge from context
        let type_mapper = ue5::ue5::types::TypeMapper::with_knowledge(context.knowledge.clone());
        
        Self {
            header: StringBuilder::new(),
            source: StringBuilder::new(),
            indent: 0,
            context,
            plugin_name: plugin_name.to_string(),
            detail_registrations: Vec::new(),
            has_shaders: false,
            delegate_param_types: std::collections::HashMap::new(),
            type_mapper,
        }
    }

    /// Write per-item header preamble with only the includes needed for this kind.
    /// `used_widgets` is a set of widget-type name strings collected from the struct's
    /// Compose body (only meaningful for `kind == "Slate"`; pass an empty set otherwise).
    fn write_item_header_preamble(&mut self, kind: &str, plugin_name: &str, used_widgets: &HashSet<String>) {
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
                // Always-required foundations
                self.header.push_line("#include \"Widgets/SCompoundWidget.h\"");
                self.header.push_line("#include \"Widgets/DeclarativeSyntaxSupport.h\"");
                // Bug-4 fix: only emit widget includes that are actually used.
                // `used_widgets` was built by scanning the Compose body AST.
                // If the set is empty (scan failed or widget has no body), fall back
                // to including everything so we never break an existing build.
                let emit_all = used_widgets.is_empty();
                macro_rules! cond_include {
                    ($header:expr, $($name:expr),+) => {
                        if emit_all || [$($name),+].iter().any(|n| used_widgets.contains(*n)) {
                            self.header.push_line(&format!("#include \"{}\"", $header));
                        }
                    };
                }
                cond_include!("Widgets/Views/SListView.h",    "ListView", "SListView", "TreeView", "STreeView", "TileView", "STileView");
                cond_include!("Widgets/Input/SButton.h",       "Button", "SButton");
                cond_include!("Widgets/Input/SCheckBox.h",     "CheckBox", "SCheckBox");
                cond_include!("Widgets/Input/SComboBox.h",     "ComboBox", "SComboBox", "ComboButton", "SComboButton");
                cond_include!("Widgets/Input/SEditableTextBox.h", "EditableTextBox", "SEditableTextBox", "EditableText", "SEditableText");
                cond_include!("Widgets/Input/SSlider.h",       "Slider", "SSlider");
                cond_include!("Widgets/Input/SSpinBox.h",      "SpinBox", "SSpinBox");
                cond_include!("Widgets/Text/STextBlock.h",     "TextBlock", "STextBlock", "Text", "Label");
                cond_include!("Widgets/Images/SImage.h",       "Image", "SImage", "shader_image");
                cond_include!("Widgets/Layout/SScrollBox.h",   "ScrollBox", "SScrollBox");
                cond_include!("Widgets/Layout/SSplitter.h",    "Splitter", "SSplitter", "HSplitter", "VSplitter");
                cond_include!("Widgets/Layout/SBorder.h",      "Border", "SBorder");
                cond_include!("Widgets/Colors/SColorBlock.h",  "ColorBlock", "SColorBlock", "Color");
                cond_include!("Widgets/Layout/SBox.h",         "Box", "SBox");
                cond_include!("Widgets/Layout/SGridPanel.h",   "GridPanel", "SGridPanel");
                cond_include!("Widgets/Layout/SWrapBox.h",     "WrapBox", "SWrapBox");
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
        for field in &st.ast.fields {
            if field.attributes.iter().any(|a| a.name == "button") {
                self.write_source(&format!("void F{}Extension::On{}()", toolbar_name, field.name));
                self.write_source("{");
                self.push_indent();
                self.write_source("// Execute toolbar button action");
                self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"Toolbar button '{}' clicked\"));", field.name));
                self.pop_indent();
                self.write_source("}");
                self.write_blank_source();
            }
        }
        
        for method in &st.ast.methods {
            if method.attributes.iter().any(|a| a.name == "button") {
                self.write_source(&format!("void F{}Extension::{}()", toolbar_name, method.name));
                self.write_source("{");
                self.push_indent();
                self.write_source("// Execute toolbar action");
                self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"Toolbar action '{}' executed\"));", method.name));
                self.pop_indent();
                self.write_source("}");
                self.write_blank_source();
            }
        }
        
        // Toggle state variable definitions
        for field in &st.ast.fields {
            if field.attributes.iter().any(|a| a.name == "toggle") {
                self.write_source(&format!("bool F{}Extension::{} = false;", toolbar_name, field.name));
                self.write_blank_source();
            }
        }
    }
    
    fn gen_asset_editor(&mut self, st: &kain_core::types::TypedStruct) {
        let editor_name = &st.ast.name;
        let class_name = format!("F{}Toolkit", editor_name);
        
        // Collect field information for tab spawning
        let mut has_asset = false;
        let mut has_viewport = false;
        let mut has_details = false;
        let mut has_slate_widget = false;
        let mut viewport_type = String::new();
        let mut slate_widget_type = String::new();
        
        for field in &st.ast.fields {
            let field_attrs: Vec<&str> = field.attributes.iter().map(|a| a.name.as_str()).collect();
            if field_attrs.contains(&"asset") {
                has_asset = true;
            } else if field_attrs.contains(&"viewport") {
                has_viewport = true;
                viewport_type = if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                    format!("S{}", name)
                } else {
                    format!("S{}", self.map_type(&field.ty))
                };
            } else if field_attrs.contains(&"details") {
                has_details = true;
            } else if field_attrs.contains(&"slate") {
                has_slate_widget = true;
                slate_widget_type = if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                    format!("S{}", name)
                } else {
                    format!("S{}", self.map_type(&field.ty))
                };
            }
        }
        
        // Forward-declare any viewport/slate widget classes referenced as members
        for field in &st.ast.fields {
            let field_attrs: Vec<&str> = field.attributes.iter().map(|a| a.name.as_str()).collect();
            if field_attrs.contains(&"viewport") || field_attrs.contains(&"slate") {
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
        
        // Methods we provide custom implementations for (use editor_name)
        let custom_methods: std::collections::HashSet<&str> = [
            "GetToolkitFName", "GetBaseToolkitName", "GetWorldCentricTabPrefix",
        ].iter().copied().collect();
        
        // FAssetEditorToolkit interface — data-driven from virtual obligations
        self.write_header("// FAssetEditorToolkit pure virtual overrides");
        self.write_header("virtual FName GetToolkitFName() const override;");
        self.write_header("virtual FText GetBaseToolkitName() const override;");
        self.write_header("virtual FString GetWorldCentricTabPrefix() const override;");
        
        // Auto-generate declarations for remaining obligations from data
        // (collect first to avoid borrow conflict with self.write_header)
        let extra_decls: Vec<String> = if self.context.virtual_obligations.has_obligations("FAssetEditorToolkit") {
            self.context.virtual_obligations.required_method_names("FAssetEditorToolkit")
                .into_iter()
                .filter(|m| !custom_methods.contains(m))
                .filter_map(|m| self.context.virtual_obligations.generate_override_declaration("FAssetEditorToolkit", m))
                .collect()
        } else {
            vec!["virtual FLinearColor GetWorldCentricTabColorScale() const override;".to_string()]
        };
        for decl in &extra_decls {
            self.write_header(decl);
        }
        
        // OnClose is not a pure virtual but we always override it to call Super
        self.write_header("virtual void OnClose() override;");
        self.write_header("");
        
        // Tab spawner methods
        if has_viewport {
            self.write_header("TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);");
        }
        if has_details {
            self.write_header("TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);");
        }
        if has_slate_widget {
            self.write_header("TSharedRef<SDockTab> SpawnDashboardTab(const FSpawnTabArgs& Args);");
        }
        self.write_header("");
        
        // Custom methods from struct (skip any that collide with auto-generated overrides)
        let auto_generated: std::collections::HashSet<&str> = {
            let mut set: std::collections::HashSet<&str> = custom_methods.iter().copied().collect();
            set.insert("OnClose");
            set
        };
        for method in &st.ast.methods {
            if !auto_generated.contains(method.name.as_str()) {
                self.write_header(&format!("void {}();", method.name));
            }
        }
        
        self.pop_indent();
        self.write_header("private:");
        self.push_indent();
        
        // Tab IDs as static constants
        if has_viewport {
            self.write_header("static const FName ViewportTabId;");
        }
        if has_details {
            self.write_header("static const FName DetailsTabId;");
        }
        if has_slate_widget {
            self.write_header("static const FName DashboardTabId;");
        }
        self.write_header("");
        
        // Member variables for sub-components
        let mut emitted_dashboard_member = false;
        for field in &st.ast.fields {
            let field_attrs: Vec<&str> = field.attributes.iter().map(|a| a.name.as_str()).collect();
            if field_attrs.contains(&"viewport") {
                let raw_name = if let kain_core::ast::Type::Named { name, .. } = &field.ty {
                    name.clone()
                } else {
                    self.map_type(&field.ty)
                };
                let widget_name = format!("S{}", raw_name);
                self.write_header(&format!("TSharedPtr<{}> ViewportWidget;", widget_name));
            } else if field_attrs.contains(&"details") {
                self.write_header("TSharedPtr<IDetailsView> DetailsView;");
            } else if field_attrs.contains(&"slate") && !emitted_dashboard_member {
                self.write_header(&format!("TSharedPtr<{}> DashboardWidget;", slate_widget_type));
                emitted_dashboard_member = true;
            }
        }

        // Generated InitEditor/SpawnDetailsTab always reference EditingAsset.
        // Emit it unconditionally to avoid undeclared-identifier errors when @asset is omitted.
        if has_asset || true {
            self.write_header("TWeakObjectPtr<UObject> EditingAsset;");
        }
        
        self.pop_indent();
        self.write_header("};");
        self.write_blank_header();
        
        // Source: Tab ID definitions
        if has_viewport {
            self.write_source(&format!("const FName {}::ViewportTabId(TEXT(\"{}Viewport\"));", class_name, editor_name));
        }
        if has_details {
            self.write_source(&format!("const FName {}::DetailsTabId(TEXT(\"{}Details\"));", class_name, editor_name));
        }
        if has_slate_widget {
            self.write_source(&format!("const FName {}::DashboardTabId(TEXT(\"{}Dashboard\"));", class_name, editor_name));
        }
        if has_viewport || has_details || has_slate_widget {
            self.write_blank_source();
        }
        
        // Source: Constructor and destructor
        self.write_source(&format!("{}::{}() {{}}", class_name, class_name));
        self.write_source(&format!("{}::~{}() {{}}", class_name, class_name));
        self.write_blank_source();
        
        // InitEditor implementation - complete with tab registration and layout
        self.write_source(&format!("void {}::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InAsset)", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source("EditingAsset = InAsset;");
        self.write_source("");
        
        // Create tab layout
        self.write_source(&format!("const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT(\"{}Layout\"))", editor_name));
        self.push_indent();
        self.write_source("->AddArea");
        self.write_source("(");
        self.push_indent();
        self.write_source("FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)");
        self.push_indent();
        
        // Add tabs to layout based on what's available
        if has_viewport && has_details {
            self.write_source("->Split");
            self.write_source("(");
            self.push_indent();
            self.write_source("FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)");
            self.push_indent();
            self.write_source(&format!("->Split(FTabManager::NewStack()->AddTab(ViewportTabId, ETabState::OpenedTab)->SetSizeCoefficient(0.7f))"));
            self.write_source(&format!("->Split(FTabManager::NewStack()->AddTab(DetailsTabId, ETabState::OpenedTab)->SetSizeCoefficient(0.3f))"));
            self.pop_indent();
            self.pop_indent();
            self.write_source(")");
        } else if has_viewport {
            self.write_source(&format!("->Split(FTabManager::NewStack()->AddTab(ViewportTabId, ETabState::OpenedTab))"));
        } else if has_details {
            self.write_source(&format!("->Split(FTabManager::NewStack()->AddTab(DetailsTabId, ETabState::OpenedTab))"));
        }
        
        if has_slate_widget {
            self.write_source(&format!("->Split(FTabManager::NewStack()->AddTab(DashboardTabId, ETabState::OpenedTab)->SetSizeCoefficient(0.3f))"));
        }
        
        self.pop_indent();
        self.pop_indent();
        self.write_source(");");
        self.pop_indent();
        self.write_source("");
        
        // Initialize the toolkit
        self.write_source(&format!("InitAssetEditor(Mode, InitToolkitHost, FName(TEXT(\"{}\")), Layout, true, true, InAsset);", editor_name));
        self.write_source("");
        
        // Register tab spawners
        if has_viewport {
            self.write_source(&format!("TabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &{}::SpawnViewportTab))", class_name));
            self.push_indent();
            self.write_source(&format!(".SetDisplayName(FText::FromString(TEXT(\"Viewport\")))"));
            self.write_source(".SetGroup(WorkspaceMenuCategory.ToSharedRef());");
            self.pop_indent();
        }
        
        if has_details {
            self.write_source(&format!("TabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &{}::SpawnDetailsTab))", class_name));
            self.push_indent();
            self.write_source(".SetDisplayName(FText::FromString(TEXT(\"Details\")))");
            self.write_source(".SetGroup(WorkspaceMenuCategory.ToSharedRef());");
            self.pop_indent();
        }
        
        if has_slate_widget {
            self.write_source(&format!("TabManager->RegisterTabSpawner(DashboardTabId, FOnSpawnTab::CreateSP(this, &{}::SpawnDashboardTab))", class_name));
            self.push_indent();
            self.write_source(".SetDisplayName(FText::FromString(TEXT(\"Dashboard\")))");
            self.write_source(".SetGroup(WorkspaceMenuCategory.ToSharedRef());");
            self.pop_indent();
        }
        
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        // Tab spawner implementations
        if has_viewport {
            self.write_source(&format!("TSharedRef<SDockTab> {}::SpawnViewportTab(const FSpawnTabArgs& Args)", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source(&format!("ViewportWidget = SNew({});", viewport_type));
            self.write_source("");
            self.write_source("return SNew(SDockTab)");
            self.push_indent();
            self.write_source(".Label(FText::FromString(TEXT(\"Viewport\")))");
            self.write_source("[");
            self.push_indent();
            self.write_source("ViewportWidget.ToSharedRef()");
            self.pop_indent();
            self.write_source("];");
            self.pop_indent();
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
        
        if has_details {
            self.write_source(&format!("TSharedRef<SDockTab> {}::SpawnDetailsTab(const FSpawnTabArgs& Args)", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source("FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(\"PropertyEditor\");");
            self.write_source("FDetailsViewArgs DetailsViewArgs;");
            self.write_source("DetailsViewArgs.bUpdatesFromSelection = false;");
            self.write_source("DetailsViewArgs.bLockable = false;");
            self.write_source("DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;");
            self.write_source("");
            self.write_source("DetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);");
            self.write_source("DetailsView->SetObject(EditingAsset.Get());");
            self.write_source("");
            self.write_source("return SNew(SDockTab)");
            self.push_indent();
            self.write_source(".Label(FText::FromString(TEXT(\"Details\")))");
            self.write_source("[");
            self.push_indent();
            self.write_source("DetailsView.ToSharedRef()");
            self.pop_indent();
            self.write_source("];");
            self.pop_indent();
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
        
        if has_slate_widget {
            self.write_source(&format!("TSharedRef<SDockTab> {}::SpawnDashboardTab(const FSpawnTabArgs& Args)", class_name));
            self.write_source("{");
            self.push_indent();
            self.write_source(&format!("DashboardWidget = SNew({});", slate_widget_type));
            self.write_source("");
            self.write_source("return SNew(SDockTab)");
            self.push_indent();
            self.write_source(".Label(FText::FromString(TEXT(\"Dashboard\")))");
            self.write_source("[");
            self.push_indent();
            self.write_source("DashboardWidget.ToSharedRef()");
            self.pop_indent();
            self.write_source("];");
            self.pop_indent();
            self.pop_indent();
            self.write_source("}");
            self.write_blank_source();
        }
        
        // Custom implementations that use editor_name
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
        
        // Auto-generate definitions for remaining obligations from data
        // (collect first to avoid borrow conflict with self.write_source)
        let extra_defs: Vec<String> = if self.context.virtual_obligations.has_obligations("FAssetEditorToolkit") {
            self.context.virtual_obligations.required_method_names("FAssetEditorToolkit")
                .into_iter()
                .filter(|m| !custom_methods.contains(m))
                .filter_map(|m| self.context.virtual_obligations.generate_override_definition("FAssetEditorToolkit", &class_name, m))
                .collect()
        } else {
            vec![format!("FLinearColor {}::GetWorldCentricTabColorScale() const\n{{\n\treturn FLinearColor::White;\n}}", class_name)]
        };
        for def in &extra_defs {
            self.write_source(def);
            self.write_blank_source();
        }
        
        // OnClose: always override to call Super (not a pure virtual, just good practice)
        self.write_source(&format!("void {}::OnClose()", class_name));
        self.write_source("{");
        self.push_indent();
        self.write_source("FAssetEditorToolkit::OnClose();");
        self.pop_indent();
        self.write_source("}");
        self.write_blank_source();
        
        // Custom method implementations
        for method in &st.ast.methods {
            if !auto_generated.contains(method.name.as_str()) {
                self.write_source(&format!("void {}::{}()", class_name, method.name));
                self.write_source("{");
                self.push_indent();
                
                // Generate method body based on method name patterns
                if method.name.starts_with("On") || method.name.starts_with("Handle") {
                    self.write_source("// Event handler implementation");
                    self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"Asset editor event: {}\"));", method.name));
                } else if method.name.starts_with("Update") || method.name.starts_with("Refresh") {
                    self.write_source("// Update editor state");
                    self.write_source("if (ViewportWidget.IsValid())");
                    self.write_source("{");
                    self.push_indent();
                    self.write_source("// Refresh viewport");
                    self.pop_indent();
                    self.write_source("}");
                    self.write_source("if (DetailsView.IsValid())");
                    self.write_source("{");
                    self.push_indent();
                    self.write_source("DetailsView->ForceRefresh();");
                    self.pop_indent();
                    self.write_source("}");
                } else {
                    self.write_source("// Custom editor operation");
                    self.write_source(&format!("UE_LOG(LogTemp, Log, TEXT(\"Asset editor method: {}\"));", method.name));
                }
                
                self.pop_indent();
                self.write_source("}");
                self.write_blank_source();
            }
        }
    }

    /// Map KAIN types to UE5 C++ types using centralized TypeMapper
    /// This eliminates duplicate type mapping logic and prevents double-prefixing bugs
    fn map_type(&self, ty: &Type) -> String {
        self.type_mapper.map_type_string(ty)
    }
    
    fn gen_editor_module(&mut self, st: &kain_core::types::TypedStruct) {
        let module_name = &st.ast.name;
        // Strip trailing "Module" suffix to avoid double-Module class names.
        // e.g. KAIN struct "UltaModule" → class "FUltaModule" (not "FUltaModuleModule")
        // e.g. KAIN struct "UltaDashboardModule" → class "FUltaDashboardModule"
        let base_name = module_name.strip_suffix("Module").unwrap_or(module_name);
        let class_name = format!("F{}Module", base_name);

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
        
        // Unregister detail customizations if any were registered
        if !self.detail_registrations.is_empty() {
            self.write_source("// Unregister detail customizations");
            self.write_source("if (FModuleManager::Get().IsModuleLoaded(\"PropertyEditor\"))");
            self.write_source("{");
            self.push_indent();
            self.write_source("FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(\"PropertyEditor\");");
            self.write_source("PropertyModule.UnregisterCustomClassLayout(FName(TEXT(\"CustomClass\")));");
            self.pop_indent();
            self.write_source("}");
            self.write_source("");
        }
        
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

// ── Bug-4 helpers ────────────────────────────────────────────────────────────

/// Recursively collect every function-call callee name that appears in an
/// expression tree.  These names map 1-to-1 with Slate widget type names
/// (e.g. "Button", "SButton", "ListView", "SListView").
fn collect_callee_names(expr: &Expr, out: &mut HashSet<String>) {
    match expr {
        Expr::Call { callee, args, .. } => {
            if let Expr::Ident(name, _) = callee.as_ref() {
                out.insert(name.clone());
            }
            collect_callee_names(callee, out);
            for a in args { collect_callee_names(&a.value, out); }
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_callee_names(receiver, out);
            for a in args { collect_callee_names(&a.value, out); }
        }
        Expr::Binary { left, right, .. } => {
            collect_callee_names(left, out);
            collect_callee_names(right, out);
        }
        Expr::Unary { operand, .. } => collect_callee_names(operand, out),
        Expr::If { condition, then_branch, else_branch, .. } => {
            collect_callee_names(condition, out);
            collect_callee_names_from_block(then_branch, out);
            if let Some(eb) = else_branch {
                match eb.as_ref() {
                    ElseBranch::Else(b) => collect_callee_names_from_block(b, out),
                    ElseBranch::ElseIf(cond, b, _) => {
                        collect_callee_names(cond, out);
                        collect_callee_names_from_block(b, out);
                    }
                }
            }
        }
        Expr::Array(items, _) | Expr::Tuple(items, _) => {
            for item in items { collect_callee_names(item, out); }
        }
        _ => {}
    }
}

fn collect_callee_names_from_block(block: &Block, out: &mut HashSet<String>) {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Expr(e) => collect_callee_names(e, out),
            Stmt::Return(Some(e), _) => collect_callee_names(e, out),
            Stmt::Let { value: Some(e), .. } => collect_callee_names(e, out),
            Stmt::For { body, .. } | Stmt::While { body, .. } | Stmt::Loop { body, .. } => {
                collect_callee_names_from_block(body, out);
            }
            _ => {}
        }
    }
}

/// Walk every method of `st` and collect all Call callee names.
/// The result is used to decide which Slate widget headers to include.
fn collect_widget_names_from_struct(st: &kain_core::ast::Struct) -> HashSet<String> {
    let mut names = HashSet::new();
    for method in &st.methods {
        collect_callee_names_from_block(&method.body, &mut names);
    }
    names
}
