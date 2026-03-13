//! Blueprint Integration Code Generation
//!
//! This module generates C++ code for Blueprint integration patterns including:
//! - BlueprintNativeEvent functions with _Implementation methods
//! - UK2Node subclasses for custom Blueprint nodes
//! - UK2Node_AsyncAction subclasses for latent Blueprint actions
//! - Blueprint-callable functions (already handled by existing codegen)

use crate::blueprint_ir::{
    AsyncBlueprintIR, AsyncOutputPinIR, BlueprintEventIR, BlueprintParamIR, K2NodeIR, K2PinIR,
    K2PinType,
};

/// Output from Blueprint event code generation
#[derive(Debug, Clone)]
pub struct BlueprintEventCodegenOutput {
    /// Header declaration for the event
    pub header_declaration: String,

    /// Source implementation for the event
    pub source_implementation: String,
}

/// Output from K2Node code generation
#[derive(Debug, Clone)]
pub struct K2NodeCodegenOutput {
    /// K2Node class header file content
    pub header: String,

    /// K2Node class source file content
    pub source: String,

    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Output from async Blueprint node code generation
#[derive(Debug, Clone)]
pub struct AsyncBlueprintCodegenOutput {
    /// Async action class header file content
    pub header: String,

    /// Async action class source file content
    pub source: String,

    /// Additional includes needed
    pub includes: Vec<String>,
}

/// Generate Blueprint event code from IR
///
/// # Arguments
/// * `ir` - The Blueprint event intermediate representation
/// * `class_name` - Name of the containing class (Actor/Component)
/// * `api_macro` - API macro for the plugin (e.g., "MYPLUGIN_API")
///
/// # Returns
/// * `BlueprintEventCodegenOutput` - Generated header and source code
pub fn generate_blueprint_event_code(
    ir: &BlueprintEventIR,
    class_name: &str,
    api_macro: &str,
) -> BlueprintEventCodegenOutput {
    let mut header_declaration = String::new();
    let mut source_implementation = String::new();

    // Generate header declaration
    generate_event_header_declaration(ir, class_name, &mut header_declaration);

    // Generate source implementation
    generate_event_source_implementation(ir, class_name, &mut source_implementation);

    BlueprintEventCodegenOutput {
        header_declaration,
        source_implementation,
    }
}

/// Generate Blueprint event header declaration
fn generate_event_header_declaration(ir: &BlueprintEventIR, class_name: &str, output: &mut String) {
    // Generate UFUNCTION macro
    output.push_str("    UFUNCTION(BlueprintNativeEvent, Category = \"");
    output.push_str(&ir.category);
    output.push_str("\")\n");

    // Generate function signature
    output.push_str("    ");
    if let Some(return_type) = &ir.return_type {
        output.push_str(return_type);
    } else {
        output.push_str("void");
    }
    output.push_str(" ");
    output.push_str(&ir.event_name);
    output.push_str("(");

    // Generate parameters
    let params: Vec<String> = ir.params.iter().map(|param| format_param(param)).collect();
    output.push_str(&params.join(", "));
    output.push_str(");\n\n");

    // Generate _Implementation method declaration
    output.push_str("    virtual ");
    if let Some(return_type) = &ir.return_type {
        output.push_str(return_type);
    } else {
        output.push_str("void");
    }
    output.push_str(" ");
    output.push_str(&ir.event_name);
    output.push_str("_Implementation(");
    output.push_str(&params.join(", "));
    output.push_str(");\n");
}

/// Generate Blueprint event source implementation
fn generate_event_source_implementation(
    ir: &BlueprintEventIR,
    class_name: &str,
    output: &mut String,
) {
    // Generate _Implementation method
    if let Some(return_type) = &ir.return_type {
        output.push_str(return_type);
    } else {
        output.push_str("void");
    }
    output.push_str(" ");
    output.push_str(class_name);
    output.push_str("::");
    output.push_str(&ir.event_name);
    output.push_str("_Implementation(");

    // Generate parameters
    let params: Vec<String> = ir.params.iter().map(|param| format_param(param)).collect();
    output.push_str(&params.join(", "));
    output.push_str(")\n");
    output.push_str("{\n");

    // Generate body
    if let Some(body) = &ir.implementation_body {
        output.push_str("    ");
        output.push_str(body);
        output.push_str("\n");
    } else {
        output.push_str("    // Default implementation\n");
        if ir.return_type.is_some() {
            output.push_str("    return {};\n");
        }
    }

    output.push_str("}\n");
}

/// Format a parameter for C++ function signature
fn format_param(param: &BlueprintParamIR) -> String {
    format!("{} {}", param.cpp_type, param.name)
}

/// Generate K2Node code from IR
///
/// # Arguments
/// * `ir` - The K2Node intermediate representation
/// * `plugin_name` - Name of the plugin (for API macro)
///
/// # Returns
/// * `K2NodeCodegenOutput` - Generated header and source files
pub fn generate_k2node_code(ir: &K2NodeIR, plugin_name: &str) -> K2NodeCodegenOutput {
    let node_class_name = format!("UK2Node_{}", ir.node_name);
    let api_macro = format!("{}_API", plugin_name.to_uppercase());

    let mut header = String::new();
    let mut source = String::new();

    // Generate K2Node header
    generate_k2node_header(ir, &node_class_name, &api_macro, &mut header);

    // Generate K2Node source
    generate_k2node_source(ir, &node_class_name, &mut source);

    K2NodeCodegenOutput {
        header,
        source,
        includes: vec![
            "CoreMinimal.h".to_string(),
            "K2Node.h".to_string(),
            "EdGraph/EdGraphPin.h".to_string(),
            "KismetCompiler.h".to_string(),
        ],
    }
}

/// Generate K2Node class header
fn generate_k2node_header(
    ir: &K2NodeIR,
    node_class_name: &str,
    api_macro: &str,
    output: &mut String,
) {
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"K2Node.h\"\n");
    output.push_str("#include \"EdGraph/EdGraphPin.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.node_name));

    // K2Node class declaration
    output.push_str("/**\n");
    output.push_str(&format!(" * Custom Blueprint node: {}\n", ir.node_title));
    output.push_str(&format!(" * Category: {}\n", ir.category));
    output.push_str(" */\n");
    output.push_str("UCLASS()\n");
    output.push_str(&format!(
        "class {} {} : public UK2Node\n",
        api_macro, node_class_name
    ));
    output.push_str("{\n");
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");

    // Constructor
    output.push_str(&format!("    {}();\n\n", node_class_name));

    // UK2Node interface
    output.push_str("    // UK2Node interface\n");
    output.push_str("    virtual void AllocateDefaultPins() override;\n");
    output.push_str(
        "    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;\n",
    );
    output.push_str("    virtual FText GetTooltipText() const override;\n");
    output.push_str("    virtual FLinearColor GetNodeTitleColor() const override;\n");
    output.push_str("    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;\n");
    output.push_str(
        "    virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;\n\n",
    );

    // Pin name constants
    output.push_str("private:\n");
    output.push_str("    // Pin names\n");
    for pin in &ir.input_pins {
        output.push_str(&format!("    static const FName {}PinName;\n", pin.name));
    }
    for pin in &ir.output_pins {
        output.push_str(&format!("    static const FName {}PinName;\n", pin.name));
    }

    output.push_str("};\n");
}

/// Generate K2Node class source
fn generate_k2node_source(ir: &K2NodeIR, node_class_name: &str, output: &mut String) {
    output.push_str(&format!("#include \"{}.h\"\n", ir.node_name));
    output.push_str("#include \"BlueprintActionDatabaseRegistrar.h\"\n");
    output.push_str("#include \"BlueprintNodeSpawner.h\"\n");
    output.push_str("#include \"K2Node_CallFunction.h\"\n\n");

    // Pin name constants
    for pin in &ir.input_pins {
        output.push_str(&format!(
            "const FName {}::{}PinName(TEXT(\"{}\"));\n",
            node_class_name, pin.name, pin.name
        ));
    }
    for pin in &ir.output_pins {
        output.push_str(&format!(
            "const FName {}::{}PinName(TEXT(\"{}\"));\n",
            node_class_name, pin.name, pin.name
        ));
    }
    output.push_str("\n");

    // Constructor
    output.push_str(&format!("{}::{}()\n", node_class_name, node_class_name));
    output.push_str("{\n");
    output.push_str("}\n\n");

    // AllocateDefaultPins
    output.push_str(&format!(
        "void {}::AllocateDefaultPins()\n",
        node_class_name
    ));
    output.push_str("{\n");
    output.push_str("    Super::AllocateDefaultPins();\n\n");

    // Create input pins
    for pin in &ir.input_pins {
        generate_pin_creation(pin, true, output);
    }

    // Create output pins
    for pin in &ir.output_pins {
        generate_pin_creation(pin, false, output);
    }

    output.push_str("}\n\n");

    // GetNodeTitle
    output.push_str(&format!(
        "FText {}::GetNodeTitle(ENodeTitleType::Type TitleType) const\n",
        node_class_name
    ));
    output.push_str("{\n");
    output.push_str(&format!(
        "    return FText::FromString(TEXT(\"{}\"));\n",
        ir.node_title
    ));
    output.push_str("}\n\n");

    // GetTooltipText
    output.push_str(&format!(
        "FText {}::GetTooltipText() const\n",
        node_class_name
    ));
    output.push_str("{\n");
    output.push_str(&format!(
        "    return FText::FromString(TEXT(\"{}\"));\n",
        ir.node_title
    ));
    output.push_str("}\n\n");

    // GetNodeTitleColor
    output.push_str(&format!(
        "FLinearColor {}::GetNodeTitleColor() const\n",
        node_class_name
    ));
    output.push_str("{\n");
    output.push_str("    return FLinearColor(0.2f, 0.6f, 1.0f);\n");
    output.push_str("}\n\n");

    // ExpandNode
    output.push_str(&format!("void {}::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)\n", node_class_name));
    output.push_str("{\n");
    output.push_str("    Super::ExpandNode(CompilerContext, SourceGraph);\n\n");

    if let Some(expand_logic) = &ir.expand_logic {
        output.push_str("    // Custom expansion logic\n");
        output.push_str(&format!("    {}\n", expand_logic));
    } else {
        output.push_str("    // TODO: Implement node expansion logic\n");
    }

    output.push_str("}\n\n");

    // GetIconAndTint
    output.push_str(&format!(
        "FSlateIcon {}::GetIconAndTint(FLinearColor& OutColor) const\n",
        node_class_name
    ));
    output.push_str("{\n");
    output.push_str("    OutColor = GetNodeTitleColor();\n");
    output
        .push_str("    return FSlateIcon(\"EditorStyle\", \"Kismet.AllClasses.FunctionIcon\");\n");
    output.push_str("}\n");
}

/// Generate pin creation code
fn generate_pin_creation(pin: &K2PinIR, is_input: bool, output: &mut String) {
    let direction = if is_input {
        "EGPD_Input"
    } else {
        "EGPD_Output"
    };
    let pin_category = map_pin_type_to_category(&pin.pin_type);

    output.push_str(&format!(
        "    CreatePin({}, {}, {}PinName);\n",
        direction, pin_category, pin.name
    ));
}

/// Map K2PinType to UE5 pin category
fn map_pin_type_to_category(pin_type: &K2PinType) -> String {
    match pin_type {
        K2PinType::Exec => "UEdGraphSchema_K2::PC_Exec".to_string(),
        K2PinType::Bool => "UEdGraphSchema_K2::PC_Boolean".to_string(),
        K2PinType::Int | K2PinType::Int64 => "UEdGraphSchema_K2::PC_Int".to_string(),
        K2PinType::Float => "UEdGraphSchema_K2::PC_Real".to_string(),
        K2PinType::String => "UEdGraphSchema_K2::PC_String".to_string(),
        K2PinType::Name => "UEdGraphSchema_K2::PC_Name".to_string(),
        K2PinType::Text => "UEdGraphSchema_K2::PC_Text".to_string(),
        K2PinType::Vector => "UEdGraphSchema_K2::PC_Struct".to_string(),
        K2PinType::Rotator => "UEdGraphSchema_K2::PC_Struct".to_string(),
        K2PinType::Transform => "UEdGraphSchema_K2::PC_Struct".to_string(),
        K2PinType::Object(_) => "UEdGraphSchema_K2::PC_Object".to_string(),
        K2PinType::Struct(_) => "UEdGraphSchema_K2::PC_Struct".to_string(),
        K2PinType::Wildcard => "UEdGraphSchema_K2::PC_Wildcard".to_string(),
    }
}

/// Generate async Blueprint node code from IR
///
/// # Arguments
/// * `ir` - The async Blueprint node intermediate representation
/// * `plugin_name` - Name of the plugin (for API macro)
///
/// # Returns
/// * `AsyncBlueprintCodegenOutput` - Generated header and source files
pub fn generate_async_blueprint_code(
    ir: &AsyncBlueprintIR,
    plugin_name: &str,
) -> AsyncBlueprintCodegenOutput {
    let action_class_name = format!("U{}", ir.action_name);
    let api_macro = format!("{}_API", plugin_name.to_uppercase());

    let mut header = String::new();
    let mut source = String::new();

    // Generate async action header
    generate_async_action_header(ir, &action_class_name, &api_macro, &mut header);

    // Generate async action source
    generate_async_action_source(ir, &action_class_name, &mut source);

    AsyncBlueprintCodegenOutput {
        header,
        source,
        includes: vec![
            "CoreMinimal.h".to_string(),
            "Kismet/BlueprintAsyncActionBase.h".to_string(),
            "Engine/LatentActionManager.h".to_string(),
        ],
    }
}

/// Generate async action class header
fn generate_async_action_header(
    ir: &AsyncBlueprintIR,
    action_class_name: &str,
    api_macro: &str,
    output: &mut String,
) {
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Kismet/BlueprintAsyncActionBase.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.action_name));

    // Generate delegate declarations for output pins
    for pin in &ir.output_pins {
        generate_output_delegate_declaration(pin, output);
    }

    // Async action class declaration
    output.push_str("/**\n");
    output.push_str(&format!(" * Async Blueprint action: {}\n", ir.action_name));
    output.push_str(&format!(" * Category: {}\n", ir.category));
    output.push_str(" */\n");
    output.push_str("UCLASS()\n");
    output.push_str(&format!(
        "class {} {} : public UBlueprintAsyncActionBase\n",
        api_macro, action_class_name
    ));
    output.push_str("{\n");
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");

    // Output pin delegates
    for pin in &ir.output_pins {
        output.push_str("    UPROPERTY(BlueprintAssignable)\n");
        output.push_str(&format!("    F{}Delegate {};\n\n", pin.name, pin.name));
    }

    // Factory method
    output.push_str("    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = \"true\", WorldContext = \"WorldContextObject\"), Category = \"");
    output.push_str(&ir.category);
    output.push_str("\")\n");
    output.push_str(&format!(
        "    static {}* {}(UObject* WorldContextObject",
        action_class_name, ir.action_name
    ));

    for param in &ir.input_params {
        output.push_str(", ");
        output.push_str(&format_param(param));
    }
    output.push_str(");\n\n");

    // Activate method
    output.push_str("    virtual void Activate() override;\n\n");

    output.push_str("private:\n");

    // Input parameters as member variables
    for param in &ir.input_params {
        output.push_str(&format!("    {} {};\n", param.cpp_type, param.name));
    }

    output.push_str("};\n");
}

/// Generate output delegate declaration
fn generate_output_delegate_declaration(pin: &AsyncOutputPinIR, output: &mut String) {
    output.push_str(&format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE"));

    if !pin.params.is_empty() {
        output.push_str(&format!("_{}", pin.params.len()));
        output.push_str("Params");
    }

    output.push_str(&format!("(F{}Delegate", pin.name));

    for (i, param) in pin.params.iter().enumerate() {
        output.push_str(", ");
        output.push_str(&param.cpp_type);
        output.push_str(", ");
        output.push_str(&capitalize(&param.name));
    }

    output.push_str(");\n\n");
}

/// Generate async action class source
fn generate_async_action_source(
    ir: &AsyncBlueprintIR,
    action_class_name: &str,
    output: &mut String,
) {
    output.push_str(&format!("#include \"{}.h\"\n\n", ir.action_name));

    // Factory method
    output.push_str(&format!(
        "{}* {}::{}(UObject* WorldContextObject",
        action_class_name, action_class_name, ir.action_name
    ));

    for param in &ir.input_params {
        output.push_str(", ");
        output.push_str(&format_param(param));
    }
    output.push_str(")\n");
    output.push_str("{\n");
    output.push_str(&format!(
        "    {}* Action = NewObject<{}>();\n",
        action_class_name, action_class_name
    ));

    // Assign input parameters
    for param in &ir.input_params {
        output.push_str(&format!("    Action->{} = {};\n", param.name, param.name));
    }

    output.push_str("    return Action;\n");
    output.push_str("}\n\n");

    // Activate method
    output.push_str(&format!("void {}::Activate()\n", action_class_name));
    output.push_str("{\n");
    output.push_str("    Super::Activate();\n\n");

    if let Some(body) = &ir.activate_body {
        output.push_str("    // Custom activation logic\n");
        output.push_str(&format!("    {}\n", body));
    } else {
        output.push_str("    // TODO: Implement activation logic\n");
        output.push_str("    // Call appropriate output delegate when complete\n");
    }

    output.push_str("}\n");
}

/// Capitalize first letter of a string
fn capitalize(s: &str) -> String {
    let mut chars = s.chars();
    match chars.next() {
        None => String::new(),
        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn make_simple_event() -> BlueprintEventIR {
        BlueprintEventIR {
            event_name: "OnCustomEvent".to_string(),
            params: vec![BlueprintParamIR {
                name: "value".to_string(),
                cpp_type: "int32".to_string(),
                is_ref: false,
                is_const: false,
            }],
            return_type: None,
            category: "Events".to_string(),
            implementation_body: Some(
                "UE_LOG(LogTemp, Log, TEXT(\"Event triggered\"));".to_string(),
            ),
        }
    }

    #[test]
    fn test_generate_blueprint_event_header() {
        let ir = make_simple_event();
        let output = generate_blueprint_event_code(&ir, "AMyActor", "MYPLUGIN_API");

        assert!(output
            .header_declaration
            .contains("UFUNCTION(BlueprintNativeEvent"));
        assert!(output
            .header_declaration
            .contains("void OnCustomEvent(int32 value)"));
        assert!(output
            .header_declaration
            .contains("OnCustomEvent_Implementation"));
    }

    #[test]
    fn test_generate_blueprint_event_source() {
        let ir = make_simple_event();
        let output = generate_blueprint_event_code(&ir, "AMyActor", "MYPLUGIN_API");

        assert!(output
            .source_implementation
            .contains("AMyActor::OnCustomEvent_Implementation"));
        assert!(output
            .source_implementation
            .contains("UE_LOG(LogTemp, Log, TEXT(\"Event triggered\"))"));
    }

    #[test]
    fn test_generate_k2node_header() {
        let ir = K2NodeIR {
            node_name: "MyCustomNode".to_string(),
            input_pins: vec![K2PinIR {
                name: "Execute".to_string(),
                pin_type: K2PinType::Exec,
                is_array: false,
                default_value: None,
            }],
            output_pins: vec![K2PinIR {
                name: "Then".to_string(),
                pin_type: K2PinType::Exec,
                is_array: false,
                default_value: None,
            }],
            node_title: "My Custom Node".to_string(),
            category: "Custom".to_string(),
            expand_logic: None,
        };

        let output = generate_k2node_code(&ir, "TestPlugin");

        assert!(output
            .header
            .contains("class TESTPLUGIN_API UK2Node_MyCustomNode"));
        assert!(output.header.contains("public UK2Node"));
        assert!(output.header.contains("AllocateDefaultPins"));
        assert!(output.header.contains("ExpandNode"));
    }

    #[test]
    fn test_generate_async_blueprint_header() {
        let ir = AsyncBlueprintIR {
            action_name: "MyAsyncAction".to_string(),
            input_params: vec![BlueprintParamIR {
                name: "duration".to_string(),
                cpp_type: "float".to_string(),
                is_ref: false,
                is_const: false,
            }],
            output_pins: vec![AsyncOutputPinIR {
                name: "OnCompleted".to_string(),
                params: vec![],
            }],
            activate_body: None,
            category: "Async".to_string(),
        };

        let output = generate_async_blueprint_code(&ir, "TestPlugin");

        assert!(output
            .header
            .contains("class TESTPLUGIN_API UMyAsyncAction"));
        assert!(output.header.contains("public UBlueprintAsyncActionBase"));
        assert!(output.header.contains("UPROPERTY(BlueprintAssignable)"));
        assert!(output.header.contains("FOnCompletedDelegate OnCompleted"));
        assert!(output.header.contains("virtual void Activate() override"));
    }
}
