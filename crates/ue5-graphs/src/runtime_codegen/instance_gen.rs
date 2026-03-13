//! GraphInstance Generator
//!
//! Generates runtime GraphInstance classes for graph execution.
//! Based on ComboGraphInstance pattern from BaconCombatGraph.

use crate::{GraphEditor, Result};

/// Output structure for instance generation
#[derive(Debug, Clone)]
pub struct InstanceOutput {
    /// Instance header (filename, content)
    pub instance_header: (String, String),
    /// Instance source (filename, content)
    pub instance_source: (String, String),
    /// NodeData header (filename, content)
    pub node_data_header: (String, String),
    /// NodeData source (filename, content)
    pub node_data_source: (String, String),
}

pub struct InstanceGenerator {
    plugin_name: String,
    graph: GraphEditor,
}

impl InstanceGenerator {
    /// Create a new instance generator
    pub fn new(graph: &GraphEditor, plugin_name: impl Into<String>) -> Self {
        Self {
            plugin_name: plugin_name.into(),
            graph: graph.clone(),
        }
    }

    /// Generate complete instance output
    pub fn generate(&self) -> Result<InstanceOutput> {
        let instance_header = self.generate_instance_header()?;
        let instance_source = self.generate_instance_source()?;
        let node_data_header = self.generate_node_data_header()?;
        let node_data_source = self.generate_node_data_source()?;

        Ok(InstanceOutput {
            instance_header: (format!("{}Instance.h", self.graph.name), instance_header),
            instance_source: (format!("{}Instance.cpp", self.graph.name), instance_source),
            node_data_header: (
                format!("{}GraphNodeData.h", self.graph.name),
                node_data_header,
            ),
            node_data_source: (
                format!("{}GraphNodeData.cpp", self.graph.name),
                node_data_source,
            ),
        })
    }

    /// Generate instance header
    pub fn generate_instance_header(&self) -> Result<String> {
        let class_name = format!("U{}Instance", self.graph.name);
        let node_data_class = format!("U{}GraphNodeData", self.graph.name);
        let asset_class = format!("U{}GraphAsset", self.graph.name);

        let mut lines = Vec::new();

        // Header guard and includes
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"UObject/Object.h\""));
        lines.push(format!(
            "#include \"{}Instance.generated.h\"",
            self.graph.name
        ));
        lines.push(String::new());

        // Forward declarations
        lines.push(format!("class {};", asset_class));
        lines.push(format!("class {};", node_data_class));
        lines.push(String::new());

        // Reset reason enum
        lines.push(format!("UENUM(BlueprintType)"));
        lines.push(format!(
            "enum class E{}ResetReason : uint8",
            self.graph.name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tRETRY,"));
        lines.push(format!("\tRESET,"));
        lines.push(format!("\tEND_GRAPH,"));
        lines.push(format!("\tCOUNT,"));
        lines.push(format!("}};"));
        lines.push(String::new());

        // Delegates
        lines.push(format!("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(F{}InstanceResetDelegate, E{}ResetReason, ResetReason);",
            self.graph.name, self.graph.name));
        lines.push(String::new());

        // Class declaration
        lines.push(format!("UCLASS(Abstract)"));
        lines.push(format!(
            "class {}_API {} : public UObject",
            self.plugin_name.to_uppercase(),
            class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());

        // Public interface
        lines.push(format!("public:"));
        lines.push(format!("\t// Instance lifecycle"));
        lines.push(format!(
            "\tUFUNCTION(BlueprintCallable, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!(
            "\tvirtual bool ResetInstance(E{}ResetReason ResetReason = E{}ResetReason::RESET);",
            self.graph.name, self.graph.name
        ));
        lines.push(String::new());

        lines.push(format!(
            "\tUFUNCTION(BlueprintPure, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!("\tvirtual bool IsValidInstance() const;"));
        lines.push(String::new());

        lines.push(format!(
            "\tUFUNCTION(BlueprintPure, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!("\tconst {}* GetGraphAsset() const;", asset_class));
        lines.push(String::new());

        // Node access
        lines.push(format!("\t// Node access"));
        lines.push(format!(
            "\tUFUNCTION(BlueprintPure, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!(
            "\tconst {}* GetCurrentNode() const;",
            node_data_class
        ));
        lines.push(String::new());

        // Protected interface
        lines.push(format!("protected:"));
        lines.push(format!("\t// Construction and destruction"));
        lines.push(format!(
            "\tvirtual bool ConstructInstance({}* InGraphAsset);",
            asset_class
        ));
        lines.push(format!("\tvirtual void BeginDestroy() override;"));
        lines.push(format!("\tvirtual void SetInstanceActive(bool bActive);"));
        lines.push(String::new());

        lines.push(format!("\t// Graph traversal"));
        lines.push(format!("\tvirtual bool TryProceedGraph();"));
        lines.push(format!(
            "\tvirtual bool SetCurrentNode(const {}* ToNode);",
            node_data_class
        ));
        lines.push(String::new());

        lines.push(format!("\t// Blueprint events"));
        lines.push(format!(
            "\tUFUNCTION(BlueprintNativeEvent, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!("\tbool IsProceedBlocked() const;"));
        lines.push(String::new());

        // Protected properties
        lines.push(format!("protected:"));
        lines.push(format!("\t// Delegates"));
        lines.push(format!("\tUPROPERTY(BlueprintAssignable)"));
        lines.push(format!(
            "\tF{}InstanceResetDelegate OnInstanceReset;",
            self.graph.name
        ));
        lines.push(String::new());

        lines.push(format!("\t// State flags"));
        lines.push(format!(
            "\tUPROPERTY(Transient, BlueprintReadOnly, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!("\tuint8 bInstanceActive : 1;"));
        lines.push(String::new());

        lines.push(format!(
            "\tUPROPERTY(Transient, BlueprintReadOnly, Category = \"{}|Instance\")",
            self.graph.name
        ));
        lines.push(format!("\tuint8 bNeedProceed : 1;"));
        lines.push(String::new());

        // Private members
        lines.push(format!("private:"));
        lines.push(format!("\t// Graph asset reference"));
        lines.push(format!("\tUPROPERTY()"));
        lines.push(format!("\t{}* GraphAsset;", asset_class));
        lines.push(String::new());

        lines.push(format!("\t// Current node tracking"));
        lines.push(format!(
            "\tTWeakObjectPtr<const {}> CurrentNode;",
            node_data_class
        ));
        lines.push(format!("}};"));

        Ok(lines.join("\n"))
    }

    /// Generate instance source
    pub fn generate_instance_source(&self) -> Result<String> {
        let class_name = format!("U{}Instance", self.graph.name);
        let node_data_class = format!("U{}GraphNodeData", self.graph.name);
        let asset_class = format!("U{}GraphAsset", self.graph.name);

        let mut lines = Vec::new();

        // Includes
        lines.push(format!("#include \"{}Instance.h\"", self.graph.name));
        lines.push(format!("#include \"{}GraphNodeData.h\"", self.graph.name));
        lines.push(format!("#include \"{}GraphAsset.h\"", self.graph.name));
        lines.push(String::new());

        // ResetInstance
        lines.push(format!(
            "bool {}::ResetInstance(E{}ResetReason ResetReason)",
            class_name, self.graph.name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tif (!IsValidInstance())"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn false;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\t// Reset to root node"));
        lines.push(format!("\tCurrentNode = nullptr;"));
        lines.push(format!("\tbNeedProceed = false;"));
        lines.push(String::new());
        lines.push(format!("\t// Broadcast reset event"));
        lines.push(format!("\tOnInstanceReset.Broadcast(ResetReason);"));
        lines.push(String::new());
        lines.push(format!("\treturn true;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // IsValidInstance
        lines.push(format!("bool {}::IsValidInstance() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!(
            "\treturn GraphAsset != nullptr && bInstanceActive;"
        ));
        lines.push(format!("}}"));
        lines.push(String::new());

        // GetGraphAsset
        lines.push(format!(
            "const {}* {}::GetGraphAsset() const",
            asset_class, class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\treturn GraphAsset;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // GetCurrentNode
        lines.push(format!(
            "const {}* {}::GetCurrentNode() const",
            node_data_class, class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\treturn CurrentNode.Get();"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // ConstructInstance
        lines.push(format!(
            "bool {}::ConstructInstance({}* InGraphAsset)",
            class_name, asset_class
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tif (InGraphAsset == nullptr)"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn false;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\tGraphAsset = InGraphAsset;"));
        lines.push(format!("\tbInstanceActive = false;"));
        lines.push(format!("\tbNeedProceed = false;"));
        lines.push(String::new());
        lines.push(format!("\treturn true;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // BeginDestroy
        lines.push(format!("void {}::BeginDestroy()", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tSuper::BeginDestroy();"));
        lines.push(String::new());
        lines.push(format!("\t// Clean up references"));
        lines.push(format!("\tCurrentNode = nullptr;"));
        lines.push(format!("\tGraphAsset = nullptr;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // SetInstanceActive
        lines.push(format!(
            "void {}::SetInstanceActive(bool bActive)",
            class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tbInstanceActive = bActive;"));
        lines.push(String::new());
        lines.push(format!("\tif (!bActive)"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\t// Reset state when deactivating"));
        lines.push(format!("\t\tCurrentNode = nullptr;"));
        lines.push(format!("\t\tbNeedProceed = false;"));
        lines.push(format!("\t}}"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // TryProceedGraph
        lines.push(format!("bool {}::TryProceedGraph()", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\tif (!IsValidInstance())"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn false;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\t// Check if proceed is blocked"));
        lines.push(format!("\tif (IsProceedBlocked())"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn false;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\t// TODO: Implement graph traversal logic"));
        lines.push(format!(
            "\t// This should find the next node based on current node and connections"
        ));
        lines.push(String::new());
        lines.push(format!("\tbNeedProceed = false;"));
        lines.push(format!("\treturn true;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // SetCurrentNode
        lines.push(format!(
            "bool {}::SetCurrentNode(const {}* ToNode)",
            class_name, node_data_class
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tif (!IsValidInstance())"));
        lines.push(format!("\t{{"));
        lines.push(format!("\t\treturn false;"));
        lines.push(format!("\t}}"));
        lines.push(String::new());
        lines.push(format!("\tCurrentNode = ToNode;"));
        lines.push(format!("\treturn true;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // IsProceedBlocked_Implementation
        lines.push(format!(
            "bool {}::IsProceedBlocked_Implementation() const",
            class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\t// Default: never blocked"));
        lines.push(format!(
            "\t// Override in Blueprint or C++ subclass for custom logic"
        ));
        lines.push(format!("\treturn false;"));
        lines.push(format!("}}"));

        Ok(lines.join("\n"))
    }

    /// Generate node data header
    pub fn generate_node_data_header(&self) -> Result<String> {
        let class_name = format!("U{}GraphNodeData", self.graph.name);

        let mut lines = Vec::new();

        // Header guard and includes
        lines.push(format!("#pragma once"));
        lines.push(String::new());
        lines.push(format!("#include \"CoreMinimal.h\""));
        lines.push(format!("#include \"UObject/Object.h\""));
        lines.push(format!(
            "#include \"{}GraphNodeData.generated.h\"",
            self.graph.name
        ));
        lines.push(String::new());

        // Class declaration
        lines.push(format!("UCLASS(Abstract)"));
        lines.push(format!(
            "class {}_API {} : public UObject",
            self.plugin_name.to_uppercase(),
            class_name
        ));
        lines.push(format!("{{"));
        lines.push(format!("\tGENERATED_BODY()"));
        lines.push(String::new());

        // Public interface
        lines.push(format!("public:"));
        lines.push(format!("\t// Node identification"));
        lines.push(format!(
            "\tUFUNCTION(BlueprintPure, Category = \"{}|NodeData\")",
            self.graph.name
        ));
        lines.push(format!("\tvirtual FName GetNodeName() const;"));
        lines.push(String::new());

        lines.push(format!(
            "\tUFUNCTION(BlueprintPure, Category = \"{}|NodeData\")",
            self.graph.name
        ));
        lines.push(format!("\tvirtual FText GetNodeDisplayName() const;"));
        lines.push(String::new());

        // Protected properties
        lines.push(format!("protected:"));
        lines.push(format!("\t// Node name"));
        lines.push(format!(
            "\tUPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = \"{}|NodeData\")",
            self.graph.name
        ));
        lines.push(format!("\tFName NodeName;"));
        lines.push(String::new());

        lines.push(format!("\t// Display name"));
        lines.push(format!(
            "\tUPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = \"{}|NodeData\")",
            self.graph.name
        ));
        lines.push(format!("\tFText DisplayName;"));
        lines.push(format!("}};"));

        Ok(lines.join("\n"))
    }

    /// Generate node data source
    pub fn generate_node_data_source(&self) -> Result<String> {
        let class_name = format!("U{}GraphNodeData", self.graph.name);

        let mut lines = Vec::new();

        // Includes
        lines.push(format!("#include \"{}GraphNodeData.h\"", self.graph.name));
        lines.push(String::new());

        // GetNodeName
        lines.push(format!("FName {}::GetNodeName() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!("\treturn NodeName;"));
        lines.push(format!("}}"));
        lines.push(String::new());

        // GetNodeDisplayName
        lines.push(format!("FText {}::GetNodeDisplayName() const", class_name));
        lines.push(format!("{{"));
        lines.push(format!(
            "\treturn DisplayName.IsEmpty() ? FText::FromName(NodeName) : DisplayName;"
        ));
        lines.push(format!("}}"));

        Ok(lines.join("\n"))
    }
}

/// Public API function for generating instance code
pub fn generate_graph_instance(graph: &GraphEditor, plugin_name: &str) -> Result<InstanceOutput> {
    let generator = InstanceGenerator::new(graph, plugin_name);
    generator.generate()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::GraphEditor;

    fn create_test_graph() -> GraphEditor {
        GraphEditor::new("TestGraph")
    }

    #[test]
    fn test_instance_generator_creates_output() {
        let graph = create_test_graph();
        let result = generate_graph_instance(&graph, "TestPlugin");

        assert!(result.is_ok());
        let output = result.unwrap();

        // Should have instance files
        assert!(output.instance_header.0.contains("TestGraphInstance.h"));
        assert!(output.instance_source.0.contains("TestGraphInstance.cpp"));

        // Should have node data files
        assert!(output.node_data_header.0.contains("TestGraphNodeData.h"));
        assert!(output.node_data_source.0.contains("TestGraphNodeData.cpp"));
    }

    #[test]
    fn test_instance_header_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_instance_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Debug: print the header to see what's generated
        println!("Generated header:\n{}", header_content);

        // Check for essential elements
        assert!(
            header_content.contains("class UTestGraphInstance")
                || header_content.contains("UTestGraphInstance"),
            "Header should contain UTestGraphInstance class"
        );
        assert!(header_content.contains("ResetInstance"));
        assert!(header_content.contains("GetCurrentNode"));
        assert!(header_content.contains("TryProceedGraph"));
        assert!(header_content.contains("SetCurrentNode"));
        assert!(header_content.contains("ETestGraphResetReason"));
    }

    #[test]
    fn test_instance_source_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let source = generator.generate_instance_source();

        assert!(source.is_ok());
        let source_content = source.unwrap();

        // Check for implementation
        assert!(source_content.contains("bool UTestGraphInstance::ResetInstance"));
        assert!(source_content.contains("bool UTestGraphInstance::IsValidInstance"));
        assert!(
            source_content.contains("const UTestGraphNodeData* UTestGraphInstance::GetCurrentNode")
        );
        assert!(source_content.contains("bool UTestGraphInstance::TryProceedGraph"));
        assert!(source_content.contains("bool UTestGraphInstance::SetCurrentNode"));
    }

    #[test]
    fn test_node_data_header_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_node_data_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Debug: print the header to see what's generated
        println!("Generated node data header:\n{}", header_content);

        // Check for essential elements
        assert!(
            header_content.contains("class UTestGraphNodeData")
                || header_content.contains("UTestGraphNodeData"),
            "Header should contain UTestGraphNodeData class"
        );
        assert!(header_content.contains("GetNodeName"));
        assert!(header_content.contains("GetNodeDisplayName"));
        assert!(header_content.contains("FName NodeName"));
        assert!(header_content.contains("FText DisplayName"));
    }

    #[test]
    fn test_node_data_source_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let source = generator.generate_node_data_source();

        assert!(source.is_ok());
        let source_content = source.unwrap();

        // Check for implementation
        assert!(source_content.contains("FName UTestGraphNodeData::GetNodeName"));
        assert!(source_content.contains("FText UTestGraphNodeData::GetNodeDisplayName"));
    }

    #[test]
    fn test_reset_reason_enum_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_instance_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Check for reset reason enum
        assert!(header_content.contains("enum class ETestGraphResetReason"));
        assert!(header_content.contains("RETRY"));
        assert!(header_content.contains("RESET"));
        assert!(header_content.contains("END_GRAPH"));
    }

    #[test]
    fn test_delegate_generation() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_instance_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Check for delegate declaration
        assert!(header_content.contains("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam"));
        assert!(header_content.contains("FTestGraphInstanceResetDelegate"));
        assert!(header_content.contains("OnInstanceReset"));
    }

    #[test]
    fn test_blueprint_callable_methods() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_instance_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Check for Blueprint-callable methods
        assert!(header_content.contains("UFUNCTION(BlueprintCallable"));
        assert!(header_content.contains("UFUNCTION(BlueprintPure"));
        assert!(header_content.contains("UFUNCTION(BlueprintNativeEvent"));
    }

    #[test]
    fn test_weak_pointer_usage() {
        let graph = create_test_graph();
        let generator = InstanceGenerator::new(&graph, "TestPlugin");

        let header = generator.generate_instance_header();

        assert!(header.is_ok());
        let header_content = header.unwrap();

        // Check for TWeakObjectPtr usage
        assert!(header_content.contains("TWeakObjectPtr<const UTestGraphNodeData>"));
    }
}
