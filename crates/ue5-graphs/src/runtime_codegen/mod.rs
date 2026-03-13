//! Runtime codegen for graph systems
//!
//! Generates runtime C++ classes for graph execution:
//! - NodeData classes (UGraphNodeData + subclasses)
//! - PinData classes (UPinData)
//! - GraphData classes (UGraphData)
//! - GraphInstance classes (UGraphInstance)
//! - Asset classes (UGraphAsset)

pub mod asset_gen;
pub mod graph_data_gen;
pub mod instance_gen;
pub mod node_data_gen;

pub use asset_gen::{generate_graph_asset, AssetGenerator, AssetOutput};
pub use graph_data_gen::{generate_graph_data_header, generate_graph_data_source};
pub use instance_gen::{generate_graph_instance, InstanceGenerator, InstanceOutput};
pub use node_data_gen::*;
