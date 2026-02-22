//! # ue5-graphs
//!
//! UE5 graph editor codegen for KAIN compiler.
//!
//! This crate generates UE5 graph editors (UEdGraph, UEdGraphNode, UEdGraphSchema)
//! from KAIN source code. It follows the same pattern as ue5-materials:
//!
//! ```text
//! KAIN AST → Graph IR → Binary .uasset + C++ Factory
//! ```
//!
//! ## Architecture
//!
//! - `graph_ir`: Intermediate representation for graph editors
//! - `ast_converter`: Converts KAIN AST to Graph IR
//! - `factory_generator`: Generates C++ .h/.cpp files
//! - `binary_serializer`: Generates binary .uasset files
//! - `node_types`: Built-in node type definitions
//! - `schema_builder`: Graph schema generation
//! - `runtime_codegen`: Runtime graph instance and node data generation

pub mod graph_ir;
pub mod ast_converter;
pub mod factory_generator;
pub mod binary_serializer;
pub mod node_types;
pub mod schema_builder;
pub mod runtime_codegen;
pub mod error;

// Runtime graph IR modules
pub mod runtime_ir;
pub mod runtime_converter;

pub use graph_ir::*;
pub use ast_converter::*;
pub use factory_generator::*;
// pub use binary_serializer::*; // TODO: Fix binary serializer dependencies
pub use node_types::*;
pub use schema_builder::*;
pub use runtime_codegen::*;
pub use error::*;

// Re-export runtime IR types
pub use runtime_ir::{
    RuntimeGraph, RuntimeNodeData, RuntimeInstance, RuntimePin,
    RuntimePinType, RuntimeProperty, RuntimeMethod, RuntimeParam,
    ExecuteLogic, RuntimeGraphProperties, ExecutionMode,
    PropertySpecifier, FunctionSpecifier, PinDirection,
};
pub use runtime_converter::{convert_runtime_graph, convert_graph_runtime_to_ir};

// Re-export runtime codegen types
pub use runtime_codegen::{InstanceOutput, InstanceGenerator, generate_graph_instance};

/// Output from graph editor generation
#[derive(Debug, Clone)]
pub struct GraphEditorOutput {
    /// Binary .uasset file content
    pub uasset: Vec<u8>,
    /// C++ header file content
    pub header: String,
    /// C++ source file content
    pub source: String,
}

/// Generate a complete graph editor from KAIN AST
///
/// # Example
///
/// ```ignore
/// use ue5_graphs::generate_graph_editor;
///
/// let output = generate_graph_editor(&graph_def, "MyPlugin")?;
/// std::fs::write("MyGraph.uasset", &output.uasset)?;
/// std::fs::write("MyGraphFactory.h", &output.header)?;
/// std::fs::write("MyGraphFactory.cpp", &output.source)?;
/// ```
pub fn generate_graph_editor(
    ast: &kain_core::ast::GraphEditorDef,
    plugin_name: &str,
) -> Result<GraphEditorOutput> {
    // Convert AST to IR
    let ir = convert_graph_editor(ast)?;
    
    // Generate C++ factory code
    let generator = factory_generator::FactoryGenerator::new(ir.clone(), plugin_name);
    let factory_output = generator.generate()?;
    
    // Combine all headers into one file (simplified for now)
    let mut header = String::new();
    header.push_str(&factory_output.base_node_header.1);
    header.push_str("\n\n");
    for (_, node_header) in &factory_output.node_headers {
        header.push_str(node_header);
        header.push_str("\n\n");
    }
    header.push_str(&factory_output.schema_header.1);
    header.push_str("\n\n");
    header.push_str(&factory_output.graph_header.1);
    
    // Combine all sources into one file (simplified for now)
    let mut source = String::new();
    source.push_str(&factory_output.base_node_source.1);
    source.push_str("\n\n");
    for (_, node_source) in &factory_output.node_sources {
        source.push_str(node_source);
        source.push_str("\n\n");
    }
    source.push_str(&factory_output.schema_source.1);
    source.push_str("\n\n");
    source.push_str(&factory_output.graph_source.1);
    
    // Generate binary .uasset (TODO: Implement binary serializer)
    let uasset = Vec::new();
    
    Ok(GraphEditorOutput {
        uasset,
        header,
        source,
    })
}

/// Output from runtime graph generation
#[derive(Debug, Clone)]
pub struct RuntimeOutput {
    /// GraphInstance header and source
    pub instance_files: InstanceOutput,
    
    /// GraphData header and source (optional)
    pub graph_data_files: Option<(String, String, String, String)>,
}

/// Generate a complete runtime graph system from GraphEditor IR
///
/// This generates the runtime execution system for a graph:
/// - GraphInstance class (manages graph execution state)
/// - GraphNodeData class (base class for node data)
/// - GraphData class (optional, for graph asset data)
///
/// # Example
///
/// ```ignore
/// use ue5_graphs::{GraphEditor, generate_runtime_graph};
///
/// let graph = GraphEditor::new("CombatGraph");
/// let output = generate_runtime_graph(&graph, "CombatPlugin")?;
/// 
/// // Write instance files
/// std::fs::write("CombatGraphInstance.h", &output.instance_files.instance_header.1)?;
/// std::fs::write("CombatGraphInstance.cpp", &output.instance_files.instance_source.1)?;
/// std::fs::write("CombatGraphNodeData.h", &output.instance_files.node_data_header.1)?;
/// std::fs::write("CombatGraphNodeData.cpp", &output.instance_files.node_data_source.1)?;
/// ```
pub fn generate_runtime_graph(
    ast: &GraphEditor,
    plugin_name: &str,
) -> Result<RuntimeOutput> {
    // Generate instance files
    let instance_files = generate_graph_instance(ast, plugin_name)?;
    
    // TODO: Generate graph data files if needed
    let graph_data_files = None;
    
    Ok(RuntimeOutput {
        instance_files,
        graph_data_files,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_crate_compiles() {
        // Basic smoke test
        assert!(true);
    }
}