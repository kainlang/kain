//! Editor Module Intermediate Representation
//!
//! This module defines the IR structures for editor modules
//! and provides conversion from AST to IR.
//!
//! The EditorModule system supports:
//! - Menu entry registration with FUICommandList
//! - Toolbar button extensions
//! - Toolbar widget extensions
//! - Editor ticker registration

use kain_core::ast::{
    EditorModuleDef, MenuEntryDef, ToolbarButtonDef, ToolbarPosition, ToolbarWidgetDef,
};

/// Editor module intermediate representation
/// Represents an editor module with menu entries, toolbar buttons, and widgets
#[derive(Debug, Clone)]
pub struct EditorModuleIR {
    /// Name of the editor module (without F prefix)
    pub name: String,

    /// Menu entries to register
    pub menu_entries: Vec<MenuEntryIR>,

    /// Toolbar buttons to register
    pub toolbar_buttons: Vec<ToolbarButtonIR>,

    /// Toolbar widgets to register
    pub toolbar_widgets: Vec<ToolbarWidgetIR>,
}

/// A single menu entry in the editor
#[derive(Debug, Clone)]
pub struct MenuEntryIR {
    /// Menu path (e.g., "Tools/Weapons")
    pub path: String,

    /// Display label
    pub label: String,

    /// Callback method name
    pub method_name: String,

    /// Optional icon name
    pub icon: Option<String>,

    /// Optional tooltip
    pub tooltip: Option<String>,
}

/// A toolbar button in the editor
#[derive(Debug, Clone)]
pub struct ToolbarButtonIR {
    /// Toolbar section (e.g., "Content")
    pub section: String,

    /// Optional label
    pub label: Option<String>,

    /// Icon name
    pub icon: String,

    /// Callback method name
    pub method_name: String,

    /// Optional tooltip
    pub tooltip: Option<String>,
}

/// A toolbar widget in the editor
#[derive(Debug, Clone)]
pub struct ToolbarWidgetIR {
    /// Toolbar section (e.g., "CameraSpeed")
    pub section: String,

    /// Position in toolbar
    pub position: ToolbarPositionIR,

    /// Widget type/class name
    pub widget_type: String,
}

/// Position specification for toolbar widgets
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ToolbarPositionIR {
    Before,
    After,
    Start,
    End,
}

/// Convert an editor module definition from AST to EditorModuleIR
///
/// # Arguments
/// * `editor_module` - The editor module definition from AST
///
/// # Returns
/// * `Ok(EditorModuleIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_editor_module_ir(
    editor_module: &EditorModuleDef,
) -> Result<EditorModuleIR, String> {
    // Convert menu entries
    let mut menu_entries = Vec::new();
    for menu_entry_def in &editor_module.menu_entries {
        let menu_entry_ir = convert_menu_entry(menu_entry_def)?;
        menu_entries.push(menu_entry_ir);
    }

    // Convert toolbar buttons
    let mut toolbar_buttons = Vec::new();
    for toolbar_button_def in &editor_module.toolbar_buttons {
        let toolbar_button_ir = convert_toolbar_button(toolbar_button_def)?;
        toolbar_buttons.push(toolbar_button_ir);
    }

    // Convert toolbar widgets
    let mut toolbar_widgets = Vec::new();
    for toolbar_widget_def in &editor_module.toolbar_widgets {
        let toolbar_widget_ir = convert_toolbar_widget(toolbar_widget_def)?;
        toolbar_widgets.push(toolbar_widget_ir);
    }

    Ok(EditorModuleIR {
        name: editor_module.name.clone(),
        menu_entries,
        toolbar_buttons,
        toolbar_widgets,
    })
}

/// Convert a menu entry definition to MenuEntryIR
fn convert_menu_entry(menu_entry_def: &MenuEntryDef) -> Result<MenuEntryIR, String> {
    Ok(MenuEntryIR {
        path: menu_entry_def.path.clone(),
        label: menu_entry_def.label.clone(),
        method_name: menu_entry_def.method.name.clone(),
        icon: menu_entry_def.icon.clone(),
        tooltip: menu_entry_def.tooltip.clone(),
    })
}

/// Convert a toolbar button definition to ToolbarButtonIR
fn convert_toolbar_button(
    toolbar_button_def: &ToolbarButtonDef,
) -> Result<ToolbarButtonIR, String> {
    Ok(ToolbarButtonIR {
        section: toolbar_button_def.section.clone(),
        label: toolbar_button_def.label.clone(),
        icon: toolbar_button_def.icon.clone(),
        method_name: toolbar_button_def.method.name.clone(),
        tooltip: toolbar_button_def.tooltip.clone(),
    })
}

/// Convert a toolbar widget definition to ToolbarWidgetIR
fn convert_toolbar_widget(
    toolbar_widget_def: &ToolbarWidgetDef,
) -> Result<ToolbarWidgetIR, String> {
    let position = match toolbar_widget_def.position {
        ToolbarPosition::Before => ToolbarPositionIR::Before,
        ToolbarPosition::After => ToolbarPositionIR::After,
        ToolbarPosition::Start => ToolbarPositionIR::Start,
        ToolbarPosition::End => ToolbarPositionIR::End,
    };

    Ok(ToolbarWidgetIR {
        section: toolbar_widget_def.section.clone(),
        position,
        widget_type: toolbar_widget_def.widget_type.clone(),
    })
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Block, EditorModuleDef, Function, MenuEntryDef, ToolbarButtonDef};
    use kain_core::span::Span;

    fn dummy_span() -> Span {
        Span::new(0, 0)
    }

    fn make_dummy_function(name: &str) -> Function {
        Function {
            name: name.to_string(),
            generics: vec![],
            params: vec![],
            return_type: None,
            effects: vec![],
            body: Block {
                stmts: vec![],
                span: dummy_span(),
            },
            visibility: kain_core::ast::Visibility::Public,
            attributes: vec![],
            span: dummy_span(),
        }
    }

    #[test]
    fn test_convert_simple_editor_module() {
        let menu_entry = MenuEntryDef {
            path: "Tools/Weapons".to_string(),
            label: "Open Weapon Editor".to_string(),
            method: make_dummy_function("on_open_editor"),
            icon: Some("Icons.Weapon".to_string()),
            tooltip: Some("Open the weapon editor".to_string()),
            attributes: vec![],
            span: dummy_span(),
        };

        let toolbar_button = ToolbarButtonDef {
            section: "Content".to_string(),
            label: Some("Quick Create".to_string()),
            icon: "Icons.Weapon".to_string(),
            method: make_dummy_function("on_quick_create"),
            tooltip: Some("Quick create weapon".to_string()),
            attributes: vec![],
            span: dummy_span(),
        };

        let editor_module = EditorModuleDef {
            name: "WeaponEditorModule".to_string(),
            menu_entries: vec![menu_entry],
            toolbar_buttons: vec![toolbar_button],
            toolbar_widgets: vec![],
            methods: vec![],
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_to_editor_module_ir(&editor_module).unwrap();

        assert_eq!(ir.name, "WeaponEditorModule");
        assert_eq!(ir.menu_entries.len(), 1);
        assert_eq!(ir.menu_entries[0].path, "Tools/Weapons");
        assert_eq!(ir.menu_entries[0].label, "Open Weapon Editor");
        assert_eq!(ir.menu_entries[0].method_name, "on_open_editor");

        assert_eq!(ir.toolbar_buttons.len(), 1);
        assert_eq!(ir.toolbar_buttons[0].section, "Content");
        assert_eq!(ir.toolbar_buttons[0].icon, "Icons.Weapon");
        assert_eq!(ir.toolbar_buttons[0].method_name, "on_quick_create");
    }

    #[test]
    fn test_convert_menu_entry() {
        let menu_entry_def = MenuEntryDef {
            path: "Tools/MyTool".to_string(),
            label: "My Tool".to_string(),
            method: make_dummy_function("open_tool"),
            icon: None,
            tooltip: None,
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_menu_entry(&menu_entry_def).unwrap();

        assert_eq!(ir.path, "Tools/MyTool");
        assert_eq!(ir.label, "My Tool");
        assert_eq!(ir.method_name, "open_tool");
        assert!(ir.icon.is_none());
        assert!(ir.tooltip.is_none());
    }

    #[test]
    fn test_convert_toolbar_button() {
        let toolbar_button_def = ToolbarButtonDef {
            section: "MySection".to_string(),
            label: Some("Button".to_string()),
            icon: "Icons.Test".to_string(),
            method: make_dummy_function("on_click"),
            tooltip: Some("Click me".to_string()),
            attributes: vec![],
            span: dummy_span(),
        };

        let ir = convert_toolbar_button(&toolbar_button_def).unwrap();

        assert_eq!(ir.section, "MySection");
        assert_eq!(ir.label, Some("Button".to_string()));
        assert_eq!(ir.icon, "Icons.Test");
        assert_eq!(ir.method_name, "on_click");
        assert_eq!(ir.tooltip, Some("Click me".to_string()));
    }
}
