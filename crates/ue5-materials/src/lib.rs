pub mod ast_converter;
pub mod material_factory;
pub mod material_function_builder;
pub mod material_graph;
pub mod material_nodes;
pub mod material_serializer;

// Phase 7.5: Vertex shader tests
// #[cfg(test)]
// mod vertex_shader_tests;  // TODO: Fix compilation errors (Span is private, CallArg needs span field)

pub use ast_converter::*;
pub use material_factory::*;
pub use material_function_builder::*;
pub use material_graph::*;
pub use material_nodes::*;
pub use material_serializer::*;
