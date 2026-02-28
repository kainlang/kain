//! Asset Editor Intermediate Representation
//!
//! This module defines the IR structures for asset editor toolkits
//! and provides conversion from AST to IR with proper type mapping.
//!
//! Asset editors combine viewports, details panels, and toolbars into
//! a unified FAssetEditorToolkit that provides a complete editing experience.
//!
//! The AssetEditor system supports:
//! - Viewport panels with custom viewport clients
//! - Details panels with property customization
//! - Toolbar extensions with custom actions
//! - Custom Slate widgets for specialized UI
//! - Tab management and layout configuration

use kain_core::ast::{Struct, Field, Attribute};
use ue5::ue5::context::Ue5Context;

/// Asset editor intermediate representation
/// Represents a complete asset editor toolkit with all its panels and configuration
#[derive(Debug, Clone)]
pub struct AssetEditorIR {
    /// Name of the asset editor (without F prefix or Toolkit suffix)
    /// e.g., "WeaponEditor" → generates "FWeaponEditorToolkit"
    pub name: String,
    
    /// Viewport panel definition (optional)
    pub viewport: Option<ViewportPanelIR>,
    
    /// Details panel definition (optional)
    pub details: Option<DetailsPanelIR>,
    
    /// Toolbar definition (optional)
    pub toolbar: Option<ToolbarIR>,
    
    /// Custom Slate widgets (optional)
    pub custom_widgets: Vec<CustomWidgetIR>,
    
    /// Asset type being edited (optional)
    /// If specified, the editor will be registered for this asset type
    pub asset_type: Option<String>,
    
    /// Tab layout configuration
    pub layout: TabLayoutIR,
    
    /// Custom methods defined on the asset editor
    pub custom_methods: Vec<CustomMethodIR>,
}

/// Viewport panel configuration
#[derive(Debug, Clone)]
pub struct ViewportPanelIR {
    /// Field name in the asset editor struct
    pub field_name: String,
    
    /// Viewport type (e.g., "WeaponPreview" → "SWeaponPreviewViewport")
    pub viewport_type: String,
    
    /// Tab display name
    pub tab_name: String,
    
    /// Tab ID (generated from field name)
    pub tab_id: String,
    
    /// Size coefficient in split layout (0.0-1.0)
    pub size_coefficient: f32,
}

/// Details panel configuration
#[derive(Debug, Clone)]
pub struct DetailsPanelIR {
    /// Field name in the asset editor struct
    pub field_name: String,
    
    /// Tab display name
    pub tab_name: String,
    
    /// Tab ID (generated from field name)
    pub tab_id: String,
    
    /// Size coefficient in split layout (0.0-1.0)
    pub size_coefficient: f32,
    
    /// Whether to show the name area
    pub show_name_area: bool,
    
    /// Whether the panel is lockable
    pub lockable: bool,
}

/// Toolbar configuration
#[derive(Debug, Clone)]
pub struct ToolbarIR {
    /// Field name in the asset editor struct
    pub field_name: String,
    
    /// Toolbar type (e.g., "WeaponTools" → "FWeaponToolsExtension")
    pub toolbar_type: String,
    
    /// Toolbar actions
    pub actions: Vec<ToolbarActionIR>,
}

/// Toolbar action definition
#[derive(Debug, Clone)]
pub struct ToolbarActionIR {
    /// Action name
    pub name: String,
    
    /// Action type (Button, Toggle, Dropdown, Separator)
    pub action_type: ToolbarActionType,
    
    /// Display label
    pub label: String,
    
    /// Icon name (optional)
    pub icon: Option<String>,
    
    /// Tooltip text (optional)
    pub tooltip: Option<String>,
}

/// Toolbar action type
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ToolbarActionType {
    /// Button that executes an action
    Button,
    
    /// Toggle button with on/off state
    Toggle,
    
    /// Dropdown menu
    Dropdown,
    
    /// Visual separator
    Separator,
}

/// Custom Slate widget panel
#[derive(Debug, Clone)]
pub struct CustomWidgetIR {
    /// Field name in the asset editor struct
    pub field_name: String,
    
    /// Widget type (e.g., "Dashboard" → "SDashboard")
    pub widget_type: String,
    
    /// Tab display name
    pub tab_name: String,
    
    /// Tab ID (generated from field name)
    pub tab_id: String,
    
    /// Size coefficient in split layout (0.0-1.0)
    pub size_coefficient: f32,
}

/// Tab layout configuration
#[derive(Debug, Clone)]
pub struct TabLayoutIR {
    /// Layout orientation (Horizontal or Vertical)
    pub orientation: LayoutOrientation,
    
    /// Tab arrangement strategy
    pub arrangement: TabArrangement,
}

/// Layout orientation
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LayoutOrientation {
    /// Horizontal split (left/right)
    Horizontal,
    
    /// Vertical split (top/bottom)
    Vertical,
}

/// Tab arrangement strategy
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TabArrangement {
    /// All tabs in a single stack
    SingleStack,
    
    /// Viewport on left, details on right
    ViewportDetailsHorizontal,
    
    /// Viewport on top, details on bottom
    ViewportDetailsVertical,
    
    /// Custom arrangement
    Custom,
}

/// Custom method on asset editor
#[derive(Debug, Clone)]
pub struct CustomMethodIR {
    /// Method name
    pub name: String,
    
    /// Method parameters (C++ type strings)
    pub params: Vec<(String, String)>,
    
    /// Return type (None for void)
    pub return_type: Option<String>,
    
    /// Method body (C++ code)
    pub body: String,
}

impl Default for TabLayoutIR {
    fn default() -> Self {
        Self {
            orientation: LayoutOrientation::Horizontal,
            arrangement: TabArrangement::ViewportDetailsHorizontal,
        }
    }
}

/// Convert an asset editor struct from AST to AssetEditorIR
///
/// # Arguments
/// * `asset_editor` - The asset editor struct from AST
/// * `ctx` - UE5 compilation context for type mapping
///
/// # Returns
/// * `Ok(AssetEditorIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_asset_editor_ir(
    asset_editor: &Struct,
    ctx: &Ue5Context,
) -> Result<AssetEditorIR, String> {
    // Validate that this is an asset editor struct
    if !has_attribute(&asset_editor.attributes, "asset_editor") {
        return Err(format!(
            "Struct '{}' does not have @asset_editor attribute",
            asset_editor.name
        ));
    }
    
    // Extract viewport panel
    let viewport = extract_viewport_panel(&asset_editor.fields)?;
    
    // Extract details panel
    let details = extract_details_panel(&asset_editor.fields)?;
    
    // Extract toolbar
    let toolbar = extract_toolbar(&asset_editor.fields)?;
    
    // Extract custom widgets
    let custom_widgets = extract_custom_widgets(&asset_editor.fields)?;
    
    // Extract asset type from attributes
    let asset_type = extract_asset_type(&asset_editor.attributes)?;
    
    // Determine tab layout based on panels present
    let layout = determine_tab_layout(&viewport, &details, &custom_widgets);
    
    // Extract custom methods
    let custom_methods = extract_custom_methods(&asset_editor.methods, ctx)?;
    
    Ok(AssetEditorIR {
        name: asset_editor.name.clone(),
        viewport,
        details,
        toolbar,
        custom_widgets,
        asset_type,
        layout,
        custom_methods,
    })
}

/// Extract viewport panel from fields
fn extract_viewport_panel(fields: &[Field]) -> Result<Option<ViewportPanelIR>, String> {
    for field in fields {
        if has_attribute(&field.attributes, "viewport") {
            let viewport_type = extract_type_name(&field.ty)?;
            let tab_name = extract_tab_name(&field.attributes, "Viewport");
            let size_coefficient = extract_size_coefficient(&field.attributes, 0.7);
            
            return Ok(Some(ViewportPanelIR {
                field_name: field.name.clone(),
                viewport_type,
                tab_name: tab_name.clone(),
                tab_id: format!("{}TabId", field.name),
                size_coefficient,
            }));
        }
    }
    
    Ok(None)
}

/// Extract details panel from fields
fn extract_details_panel(fields: &[Field]) -> Result<Option<DetailsPanelIR>, String> {
    for field in fields {
        if has_attribute(&field.attributes, "details") || has_attribute(&field.attributes, "details_panel") {
            let tab_name = extract_tab_name(&field.attributes, "Details");
            let size_coefficient = extract_size_coefficient(&field.attributes, 0.3);
            let show_name_area = extract_bool_attribute(&field.attributes, "show_name_area", false);
            let lockable = extract_bool_attribute(&field.attributes, "lockable", false);
            
            return Ok(Some(DetailsPanelIR {
                field_name: field.name.clone(),
                tab_name: tab_name.clone(),
                tab_id: format!("{}TabId", field.name),
                size_coefficient,
                show_name_area,
                lockable,
            }));
        }
    }
    
    Ok(None)
}

/// Extract toolbar from fields
fn extract_toolbar(fields: &[Field]) -> Result<Option<ToolbarIR>, String> {
    for field in fields {
        if has_attribute(&field.attributes, "toolbar") {
            let toolbar_type = extract_type_name(&field.ty)?;
            
            // Toolbar actions are extracted from the toolbar struct itself
            // For now, return empty actions (will be populated by toolbar IR converter)
            return Ok(Some(ToolbarIR {
                field_name: field.name.clone(),
                toolbar_type,
                actions: Vec::new(),
            }));
        }
    }
    
    Ok(None)
}

/// Extract custom Slate widgets from fields
fn extract_custom_widgets(fields: &[Field]) -> Result<Vec<CustomWidgetIR>, String> {
    let mut widgets = Vec::new();
    
    for field in fields {
        if has_attribute(&field.attributes, "slate") || has_attribute(&field.attributes, "custom_widget") {
            let widget_type = extract_type_name(&field.ty)?;
            let tab_name = extract_tab_name(&field.attributes, &field.name);
            let size_coefficient = extract_size_coefficient(&field.attributes, 0.3);
            
            widgets.push(CustomWidgetIR {
                field_name: field.name.clone(),
                widget_type,
                tab_name: tab_name.clone(),
                tab_id: format!("{}TabId", field.name),
                size_coefficient,
            });
        }
    }
    
    Ok(widgets)
}

/// Extract asset type from attributes
fn extract_asset_type(attributes: &[Attribute]) -> Result<Option<String>, String> {
    for attr in attributes {
        if attr.name == "asset_type" {
            if let Some(first_arg) = attr.args.first() {
                if let kain_core::ast::Expr::Ident(type_name, _) = first_arg {
                    return Ok(Some(type_name.clone()));
                }
                if let kain_core::ast::Expr::String(type_name, _) = first_arg {
                    return Ok(Some(type_name.clone()));
                }
            }
        }
    }
    
    Ok(None)
}

/// Determine tab layout based on panels present
fn determine_tab_layout(
    viewport: &Option<ViewportPanelIR>,
    details: &Option<DetailsPanelIR>,
    custom_widgets: &[CustomWidgetIR],
) -> TabLayoutIR {
    // If we have both viewport and details, use horizontal split
    if viewport.is_some() && details.is_some() {
        return TabLayoutIR {
            orientation: LayoutOrientation::Horizontal,
            arrangement: TabArrangement::ViewportDetailsHorizontal,
        };
    }
    
    // If we have custom widgets, use custom arrangement
    if !custom_widgets.is_empty() {
        return TabLayoutIR {
            orientation: LayoutOrientation::Vertical,
            arrangement: TabArrangement::Custom,
        };
    }
    
    // Default to single stack
    TabLayoutIR {
        orientation: LayoutOrientation::Vertical,
        arrangement: TabArrangement::SingleStack,
    }
}

/// Extract custom methods from asset editor struct
fn extract_custom_methods(
    methods: &[kain_core::ast::Function],
    _ctx: &Ue5Context,
) -> Result<Vec<CustomMethodIR>, String> {
    let mut custom_methods = Vec::new();
    
    for method in methods {
        // Skip methods that are overrides of FAssetEditorToolkit virtuals
        let virtual_methods = [
            "GetToolkitFName",
            "GetBaseToolkitName",
            "GetWorldCentricTabPrefix",
            "GetWorldCentricTabColorScale",
            "OnClose",
        ];
        
        if virtual_methods.contains(&method.name.as_str()) {
            continue;
        }
        
        // Convert method to IR
        // For now, use placeholder body (will be replaced with proper codegen)
        custom_methods.push(CustomMethodIR {
            name: method.name.clone(),
            params: Vec::new(),
            return_type: None,
            body: format!("// TODO: Implement {}", method.name),
        });
    }
    
    Ok(custom_methods)
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Check if an attribute with the given name exists
fn has_attribute(attributes: &[Attribute], name: &str) -> bool {
    attributes.iter().any(|attr| attr.name == name)
}

/// Extract type name from a Type AST node
fn extract_type_name(ty: &kain_core::ast::Type) -> Result<String, String> {
    match ty {
        kain_core::ast::Type::Named { name, .. } => Ok(name.clone()),
        _ => {
            let type_desc = match ty {
                kain_core::ast::Type::Array { .. } => "array type",
                kain_core::ast::Type::Option { .. } => "Option type",
                kain_core::ast::Type::Tuple { .. } => "tuple type",
                kain_core::ast::Type::Function { .. } => "function type",
                _ => "complex type",
            };
            Err(format!("Expected named type, got {}", type_desc))
        }
    }
}

/// Extract tab name from attributes or use default
fn extract_tab_name(attributes: &[Attribute], default: &str) -> String {
    for attr in attributes {
        if attr.name == "tab_name" {
            if let Some(first_arg) = attr.args.first() {
                if let kain_core::ast::Expr::String(name, _) = first_arg {
                    return name.clone();
                }
            }
        }
    }
    
    default.to_string()
}

/// Extract size coefficient from attributes or use default
fn extract_size_coefficient(attributes: &[Attribute], default: f32) -> f32 {
    for attr in attributes {
        if attr.name == "size" || attr.name == "size_coefficient" {
            if let Some(first_arg) = attr.args.first() {
                if let kain_core::ast::Expr::Float(val, _) = first_arg {
                    return *val as f32;
                }
                if let kain_core::ast::Expr::Int(val, _) = first_arg {
                    return *val as f32;
                }
            }
        }
    }
    
    default
}

/// Extract boolean attribute value or use default
fn extract_bool_attribute(attributes: &[Attribute], name: &str, default: bool) -> bool {
    for attr in attributes {
        if attr.name == name {
            if let Some(first_arg) = attr.args.first() {
                if let kain_core::ast::Expr::Bool(val, _) = first_arg {
                    return *val;
                }
                // Also accept "true"/"false" as identifiers
                if let kain_core::ast::Expr::Ident(val, _) = first_arg {
                    match val.as_str() {
                        "true" => return true,
                        "false" => return false,
                        _ => {}
                    }
                }
            }
            // If attribute is present without args, treat as true
            if attr.args.is_empty() {
                return true;
            }
        }
    }
    
    default
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Struct, Field, Attribute, Type, Visibility};
    use kain_core::span::Span;
    
    fn dummy_span() -> Span {
        Span::new(0, 0)
    }
    
    fn make_asset_editor_struct(name: &str) -> Struct {
        Struct {
            name: name.to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![],
            attributes: vec![
                Attribute {
                    name: "asset_editor".to_string(),
                    args: vec![],
                    span: dummy_span(),
                }
            ],
            visibility: Visibility::Public,
            span: dummy_span(),
        }
    }
    
    fn make_viewport_field(name: &str, viewport_type: &str) -> Field {
        Field {
            name: name.to_string(),
            ty: Type::Named {
                name: viewport_type.to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            attributes: vec![
                Attribute {
                    name: "viewport".to_string(),
                    args: vec![],
                    span: dummy_span(),
                }
            ],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: dummy_span(),
        }
    }
    
    fn make_details_field(name: &str) -> Field {
        Field {
            name: name.to_string(),
            ty: Type::Named {
                name: "DetailsView".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            attributes: vec![
                Attribute {
                    name: "details".to_string(),
                    args: vec![],
                    span: dummy_span(),
                }
            ],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: dummy_span(),
        }
    }
    
    #[test]
    fn test_convert_simple_asset_editor() {
        let ctx = ue5::ue5::context::Ue5Context::new("TestPlugin", None);
        
        let asset_editor = make_asset_editor_struct("WeaponEditor");
        
        let ir = convert_to_asset_editor_ir(&asset_editor, &ctx).unwrap();
        
        assert_eq!(ir.name, "WeaponEditor");
        assert!(ir.viewport.is_none());
        assert!(ir.details.is_none());
        assert!(ir.toolbar.is_none());
        assert!(ir.custom_widgets.is_empty());
    }
    
    #[test]
    fn test_convert_asset_editor_with_viewport() {
        let ctx = ue5::ue5::context::Ue5Context::new("TestPlugin", None);
        
        let mut asset_editor = make_asset_editor_struct("WeaponEditor");
        asset_editor.fields.push(make_viewport_field("preview", "WeaponPreview"));
        
        let ir = convert_to_asset_editor_ir(&asset_editor, &ctx).unwrap();
        
        assert_eq!(ir.name, "WeaponEditor");
        assert!(ir.viewport.is_some());
        
        let viewport = ir.viewport.unwrap();
        assert_eq!(viewport.field_name, "preview");
        assert_eq!(viewport.viewport_type, "WeaponPreview");
        assert_eq!(viewport.tab_name, "Viewport");
        assert_eq!(viewport.size_coefficient, 0.7);
    }
    
    #[test]
    fn test_convert_asset_editor_with_details() {
        let ctx = ue5::ue5::context::Ue5Context::new("TestPlugin", None);
        
        let mut asset_editor = make_asset_editor_struct("WeaponEditor");
        asset_editor.fields.push(make_details_field("properties"));
        
        let ir = convert_to_asset_editor_ir(&asset_editor, &ctx).unwrap();
        
        assert_eq!(ir.name, "WeaponEditor");
        assert!(ir.details.is_some());
        
        let details = ir.details.unwrap();
        assert_eq!(details.field_name, "properties");
        assert_eq!(details.tab_name, "Details");
        assert_eq!(details.size_coefficient, 0.3);
        assert!(!details.show_name_area);
        assert!(!details.lockable);
    }
    
    #[test]
    fn test_convert_asset_editor_with_viewport_and_details() {
        let ctx = ue5::ue5::context::Ue5Context::new("TestPlugin", None);
        
        let mut asset_editor = make_asset_editor_struct("WeaponEditor");
        asset_editor.fields.push(make_viewport_field("preview", "WeaponPreview"));
        asset_editor.fields.push(make_details_field("properties"));
        
        let ir = convert_to_asset_editor_ir(&asset_editor, &ctx).unwrap();
        
        assert_eq!(ir.name, "WeaponEditor");
        assert!(ir.viewport.is_some());
        assert!(ir.details.is_some());
        
        // Should use horizontal layout for viewport + details
        assert_eq!(ir.layout.orientation, LayoutOrientation::Horizontal);
        assert_eq!(ir.layout.arrangement, TabArrangement::ViewportDetailsHorizontal);
    }
    
    #[test]
    fn test_extract_type_name() {
        let ty = Type::Named {
            name: "WeaponPreview".to_string(),
            generics: vec![],
            span: dummy_span(),
        };
        
        let type_name = extract_type_name(&ty).unwrap();
        assert_eq!(type_name, "WeaponPreview");
    }
    
    #[test]
    fn test_has_attribute() {
        let attributes = vec![
            Attribute {
                name: "viewport".to_string(),
                args: vec![],
                span: dummy_span(),
            }
        ];
        
        assert!(has_attribute(&attributes, "viewport"));
        assert!(!has_attribute(&attributes, "details"));
    }
    
    #[test]
    fn test_extract_tab_name_default() {
        let attributes = vec![];
        let tab_name = extract_tab_name(&attributes, "DefaultTab");
        assert_eq!(tab_name, "DefaultTab");
    }
    
    #[test]
    fn test_extract_size_coefficient_default() {
        let attributes = vec![];
        let size = extract_size_coefficient(&attributes, 0.5);
        assert!((size - 0.5).abs() < 0.001);
    }
}