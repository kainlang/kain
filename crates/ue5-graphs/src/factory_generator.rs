//! C++ Factory Code Generator
//!
//! Generates C++ .h/.cpp files for graph editors from IR

use crate::{GraphEditor, NodeType, PinDefinition, PinType, Result};

/// Output structure for factory generation
#[derive(Debug, Clone)]
pub struct FactoryOutput {
    /// Base node class header (filename, content)
    pub base_node_header: (String, String),
    /// Base node class source (filename, content)
    pub base_node_source: (String, String),
    /// Node class headers (filename, content)
    pub node_headers: Vec<(String, String)>,
    /// Node class sources (filename, content)
    pub node_sources: Vec<(String, String)>,
    /// Schema header (filename, content)
    pub schema_header: (String, String),
    /// Schema source (filename, content)
    pub schema_source: (String, String),
    /// Graph header (filename, content)
    pub graph_header: (String, String),
    /// Graph source (filename, content)
    pub graph_source: (String, String),
}

pub struct FactoryGenerator {
    plugin_name: String,
    graph: GraphEditor,
}

impl FactoryGenerator {
    /// Create a new factory generator
    pub fn new(graph: GraphEditor, plugin_name: impl Into<String>) -> Self {
        Self {
            plugin_name: plugin_name.into(),
            graph,
        }
    }
    
    /// Generate complete factory output
    pub fn generate(&self) -> Result<FactoryOutput> {
        let mut node_headers = Vec::new();
        let mut node_sources = Vec::new();
        
        // Generate base node class
        let base_node_header = self.generate_base_node_header()?;
        let base_node_source = self.generate_base_node_source()?;
        
        // Generate node classes
        for node_type in &self.graph.node_types {
            let (header, source) = self.generate_node_class(node_type)?;
            let node_name = format!("{}{}Node", self.graph.name, node_type.name);
            node_headers.push((format!("{}.h", node_name), header));
            node_sources.push((format!("{}.cpp", node_name), source));
        }
        
        // Generate schema
        let schema_header = self.generate_schema_header()?;
        let schema_source = self.generate_schema_source()?;
        
        // Generate graph
        let graph_header = self.generate_graph_header()?;
        let graph_source = self.generate_graph_source()?;
        
        Ok(FactoryOutput {
            base_node_header: (format!("{}NodeBase.h", self.graph.name), base_node_header),
            base_node_source: (format!("{}NodeBase.cpp", self.graph.name), base_node_source),
            node_headers,
            node_sources,
            schema_header: (format!("{}Schema.h", self.graph.name), schema_header),
            schema_source: (format!("{}Schema.cpp", self.graph.name), schema_source),
            graph_header: (format!("{}.h", self.graph.name), graph_header),
            graph_source: (format!("{}.cpp", self.graph.name), graph_source),
        })
    }
    
    /// Generate base node class header
    fn generate_base_node_header(&self) -> Result<String> {
        let class_name = format!("U{}NodeBase", self.graph.name);
        
        let mut lines = Vec::new();
        
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"EdGraph/EdGraphNode.h\""));
        lines.push(format!("#include \"{}NodeBase.generated.h\"", self.graph.name));
        lines.push(String::new());
        
        lines.push(format!("UCLASS()"));
        lines.push(format!("class {} : public UEdGraphNode", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());
        lines.push(format!("public:"));
        lines.push(format!("\t{}();", class_name));
        lines.push(String::new());
        lines.push(format!("\t// UEdGraphNode interface"));
        lines.push(format!("\tvirtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;"));
        lines.push(format!("\tvirtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;"));
        lines.push(String::new());
        lines.push(format!("\t// Pin color (can be overridden by subclasses)"));
        lines.push(format!("\tvirtual FLinearColor GetPinColor() const;"));
        lines.push(String::new());
        lines.push(format!("\t// Helper for creating custom pins"));
        lines.push(format!("\tUEdGraphPin* CreateCustomPin(EEdGraphPinDirection Direction, FName Name, FName Subcategory = NAME_None);"));
        lines.push(format!("}};"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate base node class source
    fn generate_base_node_source(&self) -> Result<String> {
        let class_name = format!("U{}NodeBase", self.graph.name);
        let schema_class = format!("U{}Schema", self.graph.name);
        
        let mut lines = Vec::new();
        
        lines.push(format!("#include \"{}NodeBase.h\"", self.graph.name));
        lines.push(format!("#include \"{}Schema.h\"", self.graph.name));
        lines.push(format!("#include \"Framework/Commands/GenericCommands.h\""));
        lines.push(format!("#include \"GraphEditorActions.h\""));
        lines.push(String::new());
        
        // Constructor
        lines.push(format!("{}::{}()", class_name, class_name));
        lines.push(format!("{{"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetNodeContextMenuActions
        lines.push(format!("void {}::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tSuper::GetNodeContextMenuActions(Menu, Context);"));
        lines.push(String::new());
        lines.push(format!("\tFToolMenuSection& Section = Menu->AddSection(TEXT(\"DefaultActions\"), FText::FromString(TEXT(\"Default actions\")));"));
        lines.push(format!("\tSection.AddMenuEntry(FGenericCommands::Get().Delete);"));
        lines.push(format!("\tSection.AddMenuEntry(FGenericCommands::Get().Duplicate);"));
        lines.push(format!("\tSection.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // CanCreateUnderSpecifiedSchema
        lines.push(format!("bool {}::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn Schema->IsA<{}>();", schema_class));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetPinColor
        lines.push(format!("FLinearColor {}::GetPinColor() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn FLinearColor::Gray;"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // CreateCustomPin
        lines.push(format!("UEdGraphPin* {}::CreateCustomPin(EEdGraphPinDirection Direction, FName Name, FName Subcategory)", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tFName Category = (Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT(\"Inputs\") : TEXT(\"Outputs\");"));
        lines.push(String::new());
        lines.push(format!("\tUEdGraphPin* Pin = CreatePin(Direction, Category, Name);"));
        lines.push(format!("\tif (Subcategory != NAME_None)"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\tPin->PinType.PinSubCategory = Subcategory;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\treturn Pin;"));
        lines.push(format!("}}"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate node class header and source
    fn generate_node_class(&self, node: &NodeType) -> Result<(String, String)> {
        let header = self.generate_node_header(node)?;
        let source = self.generate_node_source(node)?;
        Ok((header, source))
    }
    
    /// Generate node class header
    fn generate_node_header(&self, node: &NodeType) -> Result<String> {
        let node_stem = format!("{}{}Node", self.graph.name, node.name);
        let class_name = format!("U{}", node_stem);
        let base_class = format!("U{}NodeBase", self.graph.name);
        
        let mut lines = Vec::new();
        
        // Header guard
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"{}NodeBase.h\"", self.graph.name));
        lines.push(format!("#include \"{}.generated.h\"", node_stem));
        lines.push(String::new());
        
        // Class declaration
        lines.push(format!("UCLASS()"));
        lines.push(format!("class {} : public {}", class_name, base_class));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());
        lines.push(format!("public:"));
        lines.push(format!("\t// UEdGraphNode interface"));
        lines.push(format!("\tvirtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;"));
        lines.push(format!("\tvirtual FLinearColor GetNodeTitleColor() const override;"));
        lines.push(format!("\tvirtual void CreateDefaultPins() override;"));
        lines.push(format!("\tvirtual FText GetTooltipText() const override;"));
        
        if !node.category.is_empty() {
            lines.push(format!("\tvirtual FText GetMenuCategory() const override;"));
        }
        
        lines.push(String::new());
        lines.push(format!("\t// Pin color"));
        lines.push(format!("\tvirtual FLinearColor GetPinColor() const override;"));
        lines.push(format!("}};"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate node class source
    fn generate_node_source(&self, node: &NodeType) -> Result<String> {
        let node_stem = format!("{}{}Node", self.graph.name, node.name);
        let class_name = format!("U{}", node_stem);
        let node_title = node.name.clone();
        
        let mut lines = Vec::new();
        
        // Includes
        lines.push(format!("#include \"{}.h\"", node_stem));
        lines.push(String::new());
        lines.push(format!("#define LOCTEXT_NAMESPACE \"{}Schema\"", self.graph.name));
        lines.push(String::new());
        
        // GetNodeTitle
        lines.push(format!("FText {}::GetNodeTitle(ENodeTitleType::Type TitleType) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn LOCTEXT(\"Node.{}.Title\", \"{}\");", node.name, node_title));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetNodeTitleColor
        let color = node.color.unwrap_or([0.5, 0.5, 0.5, 1.0]);
        lines.push(format!("FLinearColor {}::GetNodeTitleColor() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn FLinearColor({:.3}f, {:.3}f, {:.3}f, {:.3}f);", 
            color[0], color[1], color[2], color[3]));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // CreateDefaultPins
        lines.push(format!("void {}::CreateDefaultPins()", class_name));
        lines.push(format!("{{"));
        
        // Create input pins
        for input in &node.inputs {
            let pin_code = self.generate_pin_creation(input, "EGPD_Input");
            lines.push(format!("\t{}", pin_code));
        }
        
        // Create output pins
        for output in &node.outputs {
            let pin_code = self.generate_pin_creation(output, "EGPD_Output");
            lines.push(format!("\t{}", pin_code));
        }
        
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetTooltipText
        lines.push(format!("FText {}::GetTooltipText() const", class_name));
        lines.push(format!("{{"));
        if let Some(tooltip) = &node.tooltip {
            lines.push(format!("\treturn LOCTEXT(\"Node.{}.Tooltip\", \"{}\");", node.name, tooltip));
        } else {
            lines.push(format!("\treturn FText::GetEmpty();"));
        }
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetMenuCategory
        if !node.category.is_empty() {
            lines.push(format!("FText {}::GetMenuCategory() const", class_name));
            lines.push(format!("{{"));
            lines.push(format!("\treturn LOCTEXT(\"Node.{}.Category\", \"{}\");", node.name, node.category));
            lines.push(format!("}}"));
            lines.push(String::new());
        }
        
        // GetPinColor
        lines.push(format!("FLinearColor {}::GetPinColor() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn FLinearColor::Gray;"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        lines.push(format!("#undef LOCTEXT_NAMESPACE"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate pin creation code
    fn generate_pin_creation(&self, pin: &PinDefinition, direction: &str) -> String {
        let pin_category = self.pin_type_to_category(&pin.pin_type);
        let pin_name = &pin.name;
        
        format!(
            "CreatePin(EEdGraphPinDirection::{}, FName(\"{}\"), FName(\"{}\"));",
            direction, pin_category, pin_name
        )
    }
    
    /// Convert pin type to UE5 category
    fn pin_type_to_category(&self, pin_type: &PinType) -> String {
        match pin_type {
            PinType::Exec => "exec".to_string(),
            PinType::Bool => "bool".to_string(),
            PinType::Int => "int".to_string(),
            PinType::Float => "float".to_string(),
            PinType::String => "string".to_string(),
            PinType::Object(class) => format!("object:{}", class),
            PinType::Struct(name) => format!("struct:{}", name),
            PinType::Enum(name) => format!("enum:{}", name),
            PinType::Wildcard => "wildcard".to_string(),
        }
    }
    
    /// Generate schema header
    fn generate_schema_header(&self) -> Result<String> {
        let class_name = format!("U{}Schema", self.graph.name);
        
        let mut lines = Vec::new();
        
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"EdGraph/EdGraphSchema.h\""));
        lines.push(format!("#include \"{}Schema.generated.h\"", self.graph.name));
        lines.push(String::new());
        
        lines.push(format!("UCLASS()"));
        lines.push(format!("class {} : public UEdGraphSchema", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());
        lines.push(format!("public:"));
        lines.push(format!("\t{}();", class_name));
        lines.push(String::new());
        lines.push(format!("\t// UEdGraphSchema interface"));
        lines.push(format!("\tvirtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;"));
        lines.push(format!("\tvirtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const override;"));
        lines.push(format!("\tvirtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;"));
        lines.push(format!("\tvirtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;"));
        lines.push(format!("\tvirtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;"));
        lines.push(format!("\tvirtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;"));
        lines.push(format!("}};"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate schema source
    fn generate_schema_source(&self) -> Result<String> {
        let class_name = format!("U{}Schema", self.graph.name);
        
        let mut lines = Vec::new();
        
        // Includes
        lines.push(format!("#include \"{}Schema.h\"", self.graph.name));
        
        // Include all node headers
        for node_type in &self.graph.node_types {
            lines.push(format!("#include \"{}{}Node.h\"", self.graph.name, node_type.name));
        }
        
        lines.push(String::new());
        lines.push(format!("#define LOCTEXT_NAMESPACE \"{}Schema\"", self.graph.name));
        lines.push(String::new());
        
        // Constructor
        lines.push(format!("{}::{}()", class_name, class_name));
        lines.push(format!("{{"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // GetGraphContextActions
        lines.push(format!("void {}::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\t// TODO: Add context menu actions for node creation"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // CanCreateConnection
        lines.push(format!("const FPinConnectionResponse {}::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tif (PinA == nullptr || PinB == nullptr)"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT(\"Pin.NullPin\", \"One or both pins are null\"));"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\tif (PinA->GetOwningNode() == PinB->GetOwningNode())"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT(\"Pin.SameNode\", \"Both are on the same node\"));"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\tif (PinA->Direction == PinB->Direction)"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT(\"Pin.WrongDirection\", \"Wrong pin direction\"));"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        
        // Check connection rules from schema
        if !self.graph.schema.allowed_connections.is_empty() {
            lines.push(format!("\t// Check connection rules"));
            lines.push(format!("\t// TODO: Implement connection validation based on pin types"));
        }
        
        lines.push(format!("\treturn FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT(\"Pin.Connect\", \"Make connection\"));"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // CreateDefaultNodesForGraph
        lines.push(format!("void {}::CreateDefaultNodesForGraph(UEdGraph& Graph) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\t// Create default nodes if needed"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // BreakNodeLinks
        lines.push(format!("void {}::BreakNodeLinks(UEdGraphNode& TargetNode) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tconst FScopedTransaction Transaction(NSLOCTEXT(\"UnrealEd\", \"GraphEd_BreakNodeLinks\", \"Break Node Links\"));"));
        lines.push(format!("\tSuper::BreakNodeLinks(TargetNode);"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // BreakPinLinks
        lines.push(format!("void {}::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tconst FScopedTransaction Transaction(NSLOCTEXT(\"UnrealEd\", \"GraphEd_BreakPinLinks\", \"Break Pin Links\"));"));
        lines.push(format!("\tSuper::BreakPinLinks(TargetPin, bSendsNodeNotification);"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        // BreakSinglePinLink
        lines.push(format!("void {}::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tconst FScopedTransaction Transaction(NSLOCTEXT(\"UnrealEd\", \"GraphEd_BreakSinglePinLink\", \"Break Pin Link\"));"));
        lines.push(format!("\tSuper::BreakSinglePinLink(SourcePin, TargetPin);"));
        lines.push(format!("}}"));
        lines.push(String::new());
        
        lines.push(format!("#undef LOCTEXT_NAMESPACE"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate graph header
    fn generate_graph_header(&self) -> Result<String> {
        let class_name = format!("U{}", self.graph.name);
        
        let mut lines = Vec::new();
        
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"EdGraph/EdGraph.h\""));
        lines.push(format!("#include \"{}.generated.h\"", self.graph.name));
        lines.push(String::new());
        
        lines.push(format!("UCLASS()"));
        lines.push(format!("class {} : public UEdGraph", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());
        lines.push(format!("public:"));
        lines.push(format!("\t{}();", class_name));
        lines.push(format!("}};"));
        
        Ok(lines.join("\n"))
    }
    
    /// Generate graph source
    fn generate_graph_source(&self) -> Result<String> {
        let class_name = format!("U{}", self.graph.name);
        
        let mut lines = Vec::new();
        
        lines.push(format!("#include \"{}.h\"", self.graph.name));
        lines.push(format!("#include \"{}Schema.h\"", self.graph.name));
        lines.push(String::new());
        
        // Constructor
        lines.push(format!("{}::{}()", class_name, class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tSchema = U{}Schema::StaticClass();", self.graph.name));
        lines.push(format!("}}"));
        
        Ok(lines.join("\n"))
    }
}

/// Public API function for generating factory code
pub fn generate_graph_factory(
    graph: &GraphEditor,
    plugin_name: &str,
) -> Result<FactoryOutput> {
    let generator = FactoryGenerator::new(graph.clone(), plugin_name);
    generator.generate()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{GraphEditor, NodeType, PinDefinition, PinType};

    fn create_test_graph() -> GraphEditor {
        let mut graph = GraphEditor::new("TestGraph");
        
        // Add a simple input node
        let input_node = NodeType {
            name: "Input".to_string(),
            category: "Test/Input".to_string(),
            inputs: vec![],
            outputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: Some("Execution output".to_string()),
                },
            ],
            properties: Vec::new(),
            color: Some([0.0, 1.0, 0.0, 1.0]),
            icon: None,
            tooltip: Some("Input node".to_string()),
            execution_logic: None,
        };
        
        graph.add_node_type(input_node);
        graph
    }

    #[test]
    fn test_factory_generator_creates_output() {
        let graph = create_test_graph();
        let result = generate_graph_factory(&graph, "TestPlugin");
        
        assert!(result.is_ok());
        let output = result.unwrap();
        
        // Should have one node
        assert_eq!(output.node_headers.len(), 1);
        assert_eq!(output.node_sources.len(), 1);
        
        // Should have schema
        assert!(output.schema_header.0.contains("TestGraphSchema.h"));
        assert!(output.schema_source.0.contains("TestGraphSchema.cpp"));
        
        // Should have graph
        assert!(output.graph_header.0.contains("TestGraph.h"));
        assert!(output.graph_source.0.contains("TestGraph.cpp"));
    }

    #[test]
    fn test_node_header_generation() {
        let graph = create_test_graph();
        let generator = FactoryGenerator::new(graph.clone(), "TestPlugin");
        
        let node = &graph.node_types[0];
        let header = generator.generate_node_header(node);
        
        assert!(header.is_ok());
        let header_content = header.unwrap();
        
        // Check for essential elements
        assert!(header_content.contains("class UInputNode"));
        assert!(header_content.contains("GetNodeTitle"));
        assert!(header_content.contains("GetNodeTitleColor"));
        assert!(header_content.contains("CreateDefaultPins"));
    }

    #[test]
    fn test_node_source_generation() {
        let graph = create_test_graph();
        let generator = FactoryGenerator::new(graph.clone(), "TestPlugin");
        
        let node = &graph.node_types[0];
        let source = generator.generate_node_source(node);
        
        assert!(source.is_ok());
        let source_content = source.unwrap();
        
        // Check for implementation
        assert!(source_content.contains("FText UInputNode::GetNodeTitle"));
        assert!(source_content.contains("FLinearColor UInputNode::GetNodeTitleColor"));
        assert!(source_content.contains("void UInputNode::CreateDefaultPins"));
    }

    #[test]
    fn test_schema_generation() {
        let graph = create_test_graph();
        let generator = FactoryGenerator::new(graph.clone(), "TestPlugin");
        
        let header = generator.generate_schema_header();
        let source = generator.generate_schema_source();
        
        assert!(header.is_ok());
        assert!(source.is_ok());
        
        let header_content = header.unwrap();
        let source_content = source.unwrap();
        
        // Check schema class
        assert!(header_content.contains("class UTestGraphSchema"));
        assert!(header_content.contains("GetGraphContextActions"));
        assert!(header_content.contains("CanCreateConnection"));
        
        // Check implementation
        assert!(source_content.contains("UTestGraphSchema::GetGraphContextActions"));
        assert!(source_content.contains("UTestGraphSchema::CanCreateConnection"));
    }

    #[test]
    fn test_graph_generation() {
        let graph = create_test_graph();
        let generator = FactoryGenerator::new(graph.clone(), "TestPlugin");
        
        let header = generator.generate_graph_header();
        let source = generator.generate_graph_source();
        
        assert!(header.is_ok());
        assert!(source.is_ok());
        
        let header_content = header.unwrap();
        let source_content = source.unwrap();
        
        // Check graph class
        assert!(header_content.contains("class UTestGraph"));
        assert!(header_content.contains(": public UEdGraph"));
        
        // Check constructor sets schema
        assert!(source_content.contains("Schema = UTestGraphSchema::StaticClass()"));
    }

    #[test]
    fn test_pin_type_conversion() {
        let graph = create_test_graph();
        let generator = FactoryGenerator::new(graph, "TestPlugin");
        
        assert_eq!(generator.pin_type_to_category(&PinType::Exec), "exec");
        assert_eq!(generator.pin_type_to_category(&PinType::Bool), "bool");
        assert_eq!(generator.pin_type_to_category(&PinType::Int), "int");
        assert_eq!(generator.pin_type_to_category(&PinType::Float), "float");
        assert_eq!(generator.pin_type_to_category(&PinType::String), "string");
        assert_eq!(generator.pin_type_to_category(&PinType::Object("AActor".to_string())), "object:AActor");
    }
}
