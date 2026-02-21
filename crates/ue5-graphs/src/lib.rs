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

pub mod graph_ir;
pub mod ast_converter;
pub mod factory_generator;
pub mod binary_serializer;
pub mod node_types;
pub mod schema_builder;
pub mod error;

pub use graph_ir::*;
pub use ast_converter::*;
pub use factory_generator::*;
pub use binary_serializer::*;
pub use node_types::*;
pub use schema_builder::*;
pub use error::*;

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
/// std::fs::write("MyGraph.h", &output.header)?;
/// std::fs::write("MyGraph.cpp", &output.source)?;
/// ```
pub fn generate_graph_editor(
    ast: &kain_core::ast::GraphEditorDef,
    plugin_name: &str,
) -> Result<GraphEditorOutput> {
    // Convert AST to IR
    let ir = ASTConverter::convert(ast)?;
    
    // Generate binary .uasset
    let uasset = BinarySerializer::serialize(&ir)?;
    
    // Generate C++ factory code
    let (header, source) = FactoryGenerator::generate(&ir, plugin_name)?;
    
    Ok(GraphEditorOutput {
        uasset,
        header,
        source,
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
