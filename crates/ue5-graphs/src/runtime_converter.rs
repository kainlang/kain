//! AST to Runtime IR Converter
//!
//! Converts KAIN AST graph definitions to runtime graph IR.

use crate::error::{GraphError, Result};
use crate::runtime_ir::*;
use kain_core::ast;

/// Convert a KAIN AST GraphRuntimeDef to RuntimeGraph IR
///
/// This is the main entry point for converting @graph_runtime definitions
/// from the KAIN AST into the intermediate representation used for code generation.
pub fn convert_graph_runtime_to_ir(ast: &ast::GraphRuntimeDef) -> Result<RuntimeGraph> {
    let mut graph = RuntimeGraph::new(&ast.name);

    // Convert node types (NodeDataDef -> RuntimeNodeData)
    for node_data_ast in &ast.node_types {
        let node_data = convert_node_data_def(node_data_ast)?;
        graph.add_node_type(node_data);
    }

    // Convert instance definition (GraphInstanceDef -> RuntimeInstance)
    if let Some(instance_ast) = &ast.instance {
        graph.instance_def = convert_instance_def(instance_ast, &ast.name)?;
    } else {
        // Create default instance if not specified
        graph.instance_def = RuntimeInstance::new(format!("{}Instance", ast.name));
    }

    // Extract graph properties from attributes
    graph.properties = extract_graph_properties(&ast.attributes);

    Ok(graph)
}

/// Convert a NodeDataDef to RuntimeNodeData
fn convert_node_data_def(ast: &ast::NodeDataDef) -> Result<RuntimeNodeData> {
    // Extract category from attributes or use default
    let category = extract_category(&ast.attributes).unwrap_or_else(|| "Default".to_string());
    let mut node = RuntimeNodeData::new(&ast.name, category);

    // Convert input pins
    for input_ast in &ast.input_pins {
        let pin = convert_pin_def(input_ast, PinDirection::Input)?;
        node.add_input_pin(pin);
    }

    // Convert output pins
    for output_ast in &ast.output_pins {
        let pin = convert_pin_def(output_ast, PinDirection::Output)?;
        node.add_output_pin(pin);
    }

    // Convert properties (Field -> RuntimeProperty)
    for field_ast in &ast.properties {
        let property = convert_field_to_property(field_ast)?;
        node.add_property(property);
    }

    // Convert execution logic if present
    if let Some(execute_block) = &ast.execute_logic {
        node.execute_logic = Some(ExecuteLogic::KainExpr(block_to_string(execute_block)));
    }

    // Extract node metadata from attributes
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "color" => {
                if let Some(color) = parse_color_attribute(attr) {
                    node.color = Some(color);
                }
            }
            "icon" => {
                if let Some(icon) = parse_string_attribute(attr) {
                    node.icon = Some(icon);
                }
            }
            "tooltip" => {
                if let Some(tooltip) = parse_string_attribute(attr) {
                    node.tooltip = Some(tooltip);
                }
            }
            _ => {}
        }
    }

    Ok(node)
}

/// Convert a GraphInstanceDef to RuntimeInstance
fn convert_instance_def(ast: &ast::GraphInstanceDef, graph_name: &str) -> Result<RuntimeInstance> {
    let mut instance = RuntimeInstance::new(format!("{}Instance", graph_name));

    // Convert state fields (Field -> RuntimeProperty)
    for field_ast in &ast.state {
        let property = convert_field_to_property(field_ast)?;
        instance.add_state_field(property);
    }

    // Convert methods (Function -> RuntimeMethod)
    for func_ast in &ast.methods {
        let method = convert_function_to_method(func_ast)?;
        instance.add_method(method);
    }

    // Check for replication and savegame attributes
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "replicated" => instance.is_replicated = true,
            "savegame" => instance.is_savegame = true,
            _ => {}
        }
    }

    Ok(instance)
}

/// Convert a PinDef to RuntimePin
fn convert_pin_def(ast: &ast::PinDef, direction: PinDirection) -> Result<RuntimePin> {
    let pin_type = convert_type_to_pin_type(&ast.ty)?;

    let mut pin = match direction {
        PinDirection::Input => RuntimePin::input(&ast.name, pin_type),
        PinDirection::Output => RuntimePin::output(&ast.name, pin_type),
    };

    pin.is_array = ast.is_array;

    // Extract default value
    if let Some(default_expr) = &ast.default {
        pin.default_value = Some(expr_to_string(default_expr));
    }

    // Extract tooltip from attributes
    for attr in &ast.attributes {
        if attr.name == "tooltip" {
            if let Some(tooltip) = parse_string_attribute(attr) {
                pin.tooltip = Some(tooltip);
            }
        }
    }

    Ok(pin)
}

/// Convert a Field to RuntimeProperty
fn convert_field_to_property(ast: &ast::Field) -> Result<RuntimeProperty> {
    let property_type = convert_type_to_pin_type(&ast.ty)?;

    let mut specifiers = Vec::new();

    // Extract specifiers from attributes
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "edit_anywhere" | "editanywhere" => specifiers.push(PropertySpecifier::EditAnywhere),
            "edit_defaults" | "editdefaults" => {
                specifiers.push(PropertySpecifier::EditDefaultsOnly)
            }
            "visible" | "visibleanywhere" => specifiers.push(PropertySpecifier::VisibleAnywhere),
            "blueprint_readonly" => specifiers.push(PropertySpecifier::BlueprintReadOnly),
            "blueprint_readwrite" => specifiers.push(PropertySpecifier::BlueprintReadWrite),
            "replicated" => specifiers.push(PropertySpecifier::Replicated),
            "savegame" => specifiers.push(PropertySpecifier::SaveGame),
            "transient" => specifiers.push(PropertySpecifier::Transient),
            "category" => {
                if let Some(cat) = parse_string_attribute(attr) {
                    specifiers.push(PropertySpecifier::Category(cat));
                }
            }
            _ => {}
        }
    }

    let default_value = ast.default.as_ref().map(expr_to_string);

    // Extract tooltip from attributes
    let tooltip = ast
        .attributes
        .iter()
        .find(|attr| attr.name == "tooltip")
        .and_then(parse_string_attribute);

    Ok(RuntimeProperty {
        name: ast.name.clone(),
        property_type,
        is_array: false, // TODO: Detect array types from Type
        default_value,
        specifiers,
        tooltip,
    })
}

/// Convert a Function to RuntimeMethod
fn convert_function_to_method(ast: &ast::Function) -> Result<RuntimeMethod> {
    // Convert parameters
    let params = ast
        .params
        .iter()
        .map(|param| RuntimeParam {
            name: param.name.clone(),
            param_type: convert_type_to_pin_type(&param.ty).unwrap_or(RuntimePinType::Wildcard),
            is_array: false, // TODO: Detect array types
        })
        .collect();

    // Convert return type
    let return_type = ast
        .return_type
        .as_ref()
        .and_then(|ty| convert_type_to_pin_type(ty).ok());

    // Convert body
    let body = block_to_string(&ast.body);

    // Extract function specifiers from attributes
    let mut specifiers = Vec::new();
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "blueprint" | "blueprint_callable" => {
                specifiers.push(FunctionSpecifier::BlueprintCallable);
            }
            "blueprint_pure" => {
                specifiers.push(FunctionSpecifier::BlueprintPure);
            }
            "blueprint_event" => {
                specifiers.push(FunctionSpecifier::BlueprintNativeEvent);
            }
            "category" => {
                if let Some(cat) = parse_string_attribute(attr) {
                    specifiers.push(FunctionSpecifier::Category(cat));
                }
            }
            _ => {}
        }
    }

    Ok(RuntimeMethod {
        name: ast.name.clone(),
        params,
        return_type,
        body,
        specifiers,
    })
}

/// Convert KAIN Type to RuntimePinType
fn convert_type_to_pin_type(ty: &ast::Type) -> Result<RuntimePinType> {
    match ty {
        ast::Type::Named { name, .. } => {
            match name.as_str() {
                "Bool" => Ok(RuntimePinType::Bool),
                "Int" => Ok(RuntimePinType::Int),
                "Int64" => Ok(RuntimePinType::Int64),
                "Float" => Ok(RuntimePinType::Float),
                "String" => Ok(RuntimePinType::String),
                "Name" => Ok(RuntimePinType::Name),
                "Text" => Ok(RuntimePinType::Text),
                "Vec3" | "Vector" => Ok(RuntimePinType::Vector),
                "Rotator" => Ok(RuntimePinType::Rotator),
                "Transform" => Ok(RuntimePinType::Transform),
                "Color" | "LinearColor" => Ok(RuntimePinType::Color),
                "Exec" => Ok(RuntimePinType::Exec),
                "Class" => Ok(RuntimePinType::Object("UClass".to_string())),
                "NodeData" => Ok(RuntimePinType::Object("UNodeData".to_string())),
                "GraphInstance" => Ok(RuntimePinType::Object("UGraphInstance".to_string())),
                _ => {
                    // Check if it's an object, struct, or enum by prefix
                    if name.starts_with('U') || name.starts_with('A') {
                        Ok(RuntimePinType::Object(name.clone()))
                    } else if name.starts_with('F') {
                        Ok(RuntimePinType::Struct(name.clone()))
                    } else if name.starts_with('E') {
                        Ok(RuntimePinType::Enum(name.clone()))
                    } else {
                        // Assume struct for unprefixed types
                        Ok(RuntimePinType::Struct(format!("F{}", name)))
                    }
                }
            }
        }
        ast::Type::Array(element, _, _) => {
            // For arrays, return the element type (is_array flag set elsewhere)
            convert_type_to_pin_type(element)
        }
        ast::Type::Slice(element, _) => {
            // Slices treated like arrays
            convert_type_to_pin_type(element)
        }
        ast::Type::Ref { inner, .. } => {
            // References - unwrap to inner type
            convert_type_to_pin_type(inner)
        }
        ast::Type::Option(inner, _) => {
            // Option types - unwrap to inner type
            convert_type_to_pin_type(inner)
        }
        _ => {
            let type_desc = match ty {
                ast::Type::Function { .. } => "function type",
                ast::Type::Tuple { .. } => "tuple type",
                ast::Type::Result { .. } => "Result type",
                _ => "complex type",
            };
            Err(GraphError::ASTConversion(format!(
                "Unsupported type for runtime pin: {}",
                type_desc
            )))
        }
    }
}

/// Extract category from attributes
fn extract_category(attributes: &[ast::Attribute]) -> Option<String> {
    attributes
        .iter()
        .find(|attr| attr.name == "category")
        .and_then(parse_string_attribute)
}

/// Legacy function for backward compatibility
#[deprecated(note = "Use convert_graph_runtime_to_ir instead")]
pub fn convert_runtime_graph(ast: &ast::GraphEditorDef) -> Result<RuntimeGraph> {
    let mut graph = RuntimeGraph::new(&ast.name);

    // Check if this is a runtime graph
    let is_runtime = ast
        .attributes
        .iter()
        .any(|attr| attr.name == "runtime_graph");
    if !is_runtime {
        return Err(GraphError::ASTConversion(format!(
            "Graph '{}' is not marked with @runtime_graph attribute",
            ast.name
        )));
    }

    // Convert node types
    for node_type_ast in &ast.node_types {
        let node_data = convert_node_type_legacy(node_type_ast)?;
        graph.add_node_type(node_data);
    }

    // Set up instance definition
    graph.instance_def = RuntimeInstance::new(format!("{}Instance", ast.name));

    // Extract graph properties from attributes
    graph.properties = extract_graph_properties(&ast.attributes);

    Ok(graph)
}

/// Legacy node type converter
fn convert_node_type_legacy(ast: &ast::NodeTypeDef) -> Result<RuntimeNodeData> {
    let category = ast
        .category
        .clone()
        .unwrap_or_else(|| "Default".to_string());
    let mut node = RuntimeNodeData::new(&ast.name, category);

    // Convert input pins
    for input_ast in &ast.inputs {
        let pin = convert_pin(input_ast, PinDirection::Input)?;
        node.add_input_pin(pin);
    }

    // Convert output pins
    for output_ast in &ast.outputs {
        let pin = convert_pin(output_ast, PinDirection::Output)?;
        node.add_output_pin(pin);
    }

    // Convert properties
    for prop_ast in &ast.properties {
        let property = convert_property(prop_ast)?;
        node.add_property(property);
    }

    // Extract node metadata from attributes
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "color" => {
                if let Some(color) = parse_color_attribute(attr) {
                    node.color = Some(color);
                }
            }
            "icon" => {
                if let Some(icon) = parse_string_attribute(attr) {
                    node.icon = Some(icon);
                }
            }
            "tooltip" => {
                if let Some(tooltip) = parse_string_attribute(attr) {
                    node.tooltip = Some(tooltip);
                }
            }
            _ => {}
        }
    }

    Ok(node)
}

/// Legacy pin converter
fn convert_pin(ast: &ast::PinDef, direction: PinDirection) -> Result<RuntimePin> {
    let pin_type = convert_type_to_pin_type(&ast.ty)?;

    let mut pin = match direction {
        PinDirection::Input => RuntimePin::input(&ast.name, pin_type),
        PinDirection::Output => RuntimePin::output(&ast.name, pin_type),
    };

    pin.is_array = ast.is_array;

    // Extract default value
    if let Some(default_expr) = &ast.default {
        pin.default_value = Some(expr_to_string(default_expr));
    }

    // Extract tooltip from attributes
    for attr in &ast.attributes {
        if attr.name == "tooltip" {
            if let Some(tooltip) = parse_string_attribute(attr) {
                pin.tooltip = Some(tooltip);
            }
        }
    }

    Ok(pin)
}

/// Legacy property converter
fn convert_property(ast: &ast::PropertyDef) -> Result<RuntimeProperty> {
    let property_type = convert_type_to_pin_type(&ast.ty)?;

    let mut specifiers = Vec::new();

    // Extract specifiers from attributes
    for attr in &ast.attributes {
        match attr.name.as_str() {
            "edit_anywhere" => specifiers.push(PropertySpecifier::EditAnywhere),
            "edit_defaults" => specifiers.push(PropertySpecifier::EditDefaultsOnly),
            "visible" => specifiers.push(PropertySpecifier::VisibleAnywhere),
            "blueprint_readonly" => specifiers.push(PropertySpecifier::BlueprintReadOnly),
            "blueprint_readwrite" => specifiers.push(PropertySpecifier::BlueprintReadWrite),
            "replicated" => specifiers.push(PropertySpecifier::Replicated),
            "savegame" => specifiers.push(PropertySpecifier::SaveGame),
            "transient" => specifiers.push(PropertySpecifier::Transient),
            "category" => {
                if let Some(cat) = parse_string_attribute(attr) {
                    specifiers.push(PropertySpecifier::Category(cat));
                }
            }
            _ => {}
        }
    }

    let default_value = ast.default.as_ref().map(expr_to_string);

    Ok(RuntimeProperty {
        name: ast.name.clone(),
        property_type,
        is_array: false, // TODO: Extract from type
        default_value,
        specifiers,
        tooltip: None,
    })
}

/// Extract graph properties from attributes
fn extract_graph_properties(attributes: &[ast::Attribute]) -> RuntimeGraphProperties {
    let mut props = RuntimeGraphProperties::default();

    for attr in attributes {
        match attr.name.as_str() {
            "parallel_execution" => {
                props.allow_parallel_execution = true;
                props.execution_mode = ExecutionMode::Parallel;
            }
            "event_driven" => {
                props.execution_mode = ExecutionMode::EventDriven;
            }
            "max_depth" => {
                if let Some(depth) = parse_int_attribute(attr) {
                    props.max_execution_depth = depth;
                }
            }
            "debug_logging" => {
                props.enable_debug_logging = true;
            }
            _ => {}
        }
    }

    props
}

/// Parse a color attribute (e.g., @color(1.0, 0.5, 0.0, 1.0))
fn parse_color_attribute(attr: &ast::Attribute) -> Option<[f32; 4]> {
    // Try to extract color from attribute arguments
    if attr.args.len() >= 3 {
        let r = extract_float_from_expr(&attr.args[0])?;
        let g = extract_float_from_expr(&attr.args[1])?;
        let b = extract_float_from_expr(&attr.args[2])?;
        let a = if attr.args.len() >= 4 {
            extract_float_from_expr(&attr.args[3])?
        } else {
            1.0
        };
        Some([r, g, b, a])
    } else {
        // Default color
        Some([0.8, 0.8, 0.8, 1.0])
    }
}

/// Parse a string attribute (e.g., @tooltip("This is a tooltip"))
fn parse_string_attribute(attr: &ast::Attribute) -> Option<String> {
    if !attr.args.is_empty() {
        extract_string_from_expr(&attr.args[0])
    } else {
        None
    }
}

/// Parse an integer attribute (e.g., @max_depth(100))
fn parse_int_attribute(attr: &ast::Attribute) -> Option<i32> {
    if !attr.args.is_empty() {
        extract_int_from_expr(&attr.args[0])
    } else {
        None
    }
}

/// Extract float value from expression
fn extract_float_from_expr(expr: &ast::Expr) -> Option<f32> {
    match expr {
        ast::Expr::Float(val, _) => Some(*val as f32),
        ast::Expr::Int(val, _) => Some(*val as f32),
        _ => None,
    }
}

/// Extract string value from expression
fn extract_string_from_expr(expr: &ast::Expr) -> Option<String> {
    match expr {
        ast::Expr::String(s, _) => Some(s.clone()),
        _ => None,
    }
}

/// Extract integer value from expression
fn extract_int_from_expr(expr: &ast::Expr) -> Option<i32> {
    match expr {
        ast::Expr::Int(val, _) => Some(*val as i32),
        _ => None,
    }
}

/// Convert an expression to a string representation
fn expr_to_string(expr: &ast::Expr) -> String {
    match expr {
        ast::Expr::Int(val, _) => val.to_string(),
        ast::Expr::Float(val, _) => val.to_string(),
        ast::Expr::String(s, _) => format!("\"{}\"", s),
        ast::Expr::Bool(b, _) => b.to_string(),
        ast::Expr::Ident(name, _) => name.clone(),
        ast::Expr::None(_) => "nullptr".to_string(),
        _ => "/* <complex_expression> */".to_string(),
    }
}

/// Convert a block to a string representation
fn block_to_string(block: &ast::Block) -> String {
    // Simple conversion - just format the statements
    let mut result = String::new();
    for stmt in &block.stmts {
        result.push_str("    // <statement>\n");
    }
    result
}
