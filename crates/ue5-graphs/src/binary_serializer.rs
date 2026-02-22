//! Binary .uasset Serializer for Graph Editors
//!
//! Generates UE5-compatible binary .uasset files that can be opened in the editor.
//! Follows the pattern established by ue5-materials/material_serializer.rs

use std::collections::HashMap;
use std::io::Cursor;

use unreal_asset::{
    exports::{BaseExport, Export, ExportBaseTrait, NormalExport},
    flags::EObjectFlags,
    types::PackageIndex,
    Asset, Import,
};
use unreal_asset_properties::{
    int_property::{IntProperty, BoolProperty},
    object_property::ObjectProperty,
    str_property::StrProperty,
    array_property::ArrayProperty,
    Property,
};

use crate::{GraphEditor, NodeType, PinDefinition, PinType, GraphError, Result};

// ---------------------------------------------------------------------------
// GraphAssetBuilder — programmatic .uasset creation for UE5 graph editors
// ---------------------------------------------------------------------------

/// Builds a UE5 Graph Editor .uasset file programmatically using the unreal_asset library.
///
/// Usage:
/// ```ignore
/// let mut builder = GraphAssetBuilder::new("CombatGraph");
/// builder.add_node_type(&node_type);
/// let bytes = builder.build()?;
/// std::fs::write("CombatGraph.uasset", bytes)?;
/// ```
pub struct GraphAssetBuilder {
    asset: Asset<Cursor<Vec<u8>>>,
    graph_name: String,

    // Import indices (negative PackageIndex values)
    engine_import: PackageIndex,
    core_uobject_import: PackageIndex,
    graph_class_import: PackageIndex,
    node_class_import: PackageIndex,
    pin_class_import: PackageIndex,
    schema_class_import: PackageIndex,

    // Node type class imports — lazily added as needed
    class_imports: HashMap<String, PackageIndex>,

    // The graph export is always export index 0 (PackageIndex 1)
    graph_export_index: PackageIndex,

    // Node type exports: maps node type name -> export PackageIndex (1-based positive)
    node_type_exports: Vec<PackageIndex>,

    // Schema export
    schema_export_index: Option<PackageIndex>,

    // Node positions for editor layout
    next_node_x: i32,
    next_node_y: i32,
}

impl GraphAssetBuilder {
    /// Create a new builder for a graph editor with the given name.
    pub fn new(graph_name: &str) -> Self {
        // Use UE5.2 serializer version (same as material serializer)
        let mut asset = Asset::new_empty(unreal_asset::engine_version::EngineVersion::VER_UE5_2);

        // ── Core imports every graph needs ────
        let core_uobject_import = Self::get_or_add_package(&mut asset, "/Script/CoreUObject");
        let engine_import = Self::get_or_add_package(&mut asset, "/Script/Engine");
        
        // Graph editor base classes
        let graph_class_import = Self::get_or_add_class(&mut asset, "EdGraph", engine_import);
        let node_class_import = Self::get_or_add_class(&mut asset, "EdGraphNode", engine_import);
        let pin_class_import = Self::get_or_add_class(&mut asset, "EdGraphPin", engine_import);
        let schema_class_import = Self::get_or_add_class(&mut asset, "EdGraphSchema", engine_import);

        let mut builder = GraphAssetBuilder {
            asset,
            graph_name: graph_name.to_string(),
            engine_import,
            core_uobject_import,
            graph_class_import,
            node_class_import,
            pin_class_import,
            schema_class_import,
            class_imports: HashMap::new(),
            graph_export_index: PackageIndex::new(1), // first export is 1-based
            node_type_exports: Vec::new(),
            schema_export_index: None,
            next_node_x: -400,
            next_node_y: 0,
        };

        // Create the main Graph export (index 1)
        builder.create_graph_export();

        builder
    }

    // ── Import helpers ─────────────────────────────────────────────────────

    /// Get or add a package import
    fn get_or_add_package(asset: &mut Asset<Cursor<Vec<u8>>>, package_path: &str) -> PackageIndex {
        // Check if package already exists
        for (idx, import) in asset.imports.iter().enumerate() {
            if import.object_name.get_content(|s| s == package_path) {
                return PackageIndex::new(-((idx + 1) as i32));
            }
        }

        // Add new package import
        let package_name = asset.add_fname(package_path);
        let class_package = asset.add_fname("Package");

        let import = Import {
            class_package: class_package.clone(),
            class_name: class_package,
            outer_index: PackageIndex::new(0),
            object_name: package_name,
            optional: false,
        };

        asset.imports.push(import);
        PackageIndex::new(-(asset.imports.len() as i32))
    }

    /// Get or add a class import
    fn get_or_add_class(
        asset: &mut Asset<Cursor<Vec<u8>>>,
        class_name: &str,
        package_index: PackageIndex,
    ) -> PackageIndex {
        // Check if class already exists
        for (idx, import) in asset.imports.iter().enumerate() {
            if import.object_name.get_content(|s| s == class_name) 
                && import.outer_index == package_index {
                return PackageIndex::new(-((idx + 1) as i32));
            }
        }

        // Add new class import
        let class_fname = asset.add_fname(class_name);
        let class_type = asset.add_fname("Class");
        let core_uobject = asset.add_fname("/Script/CoreUObject");

        let import = Import {
            class_package: core_uobject,
            class_name: class_type,
            outer_index: package_index,
            object_name: class_fname,
            optional: false,
        };

        asset.imports.push(import);
        PackageIndex::new(-(asset.imports.len() as i32))
    }

    /// Get or create an import for a node type class
    fn get_node_type_class_import(&mut self, class_name: &str) -> PackageIndex {
        if let Some(&idx) = self.class_imports.get(class_name) {
            return idx;
        }

        let idx = Self::get_or_add_class(
            &mut self.asset,
            class_name,
            self.engine_import,
        );

        self.class_imports.insert(class_name.to_string(), idx);
        idx
    }

    // ── Graph export creation ──────────────────────────────────────────────

    fn create_graph_export(&mut self) {
        let graph_name = self.asset.add_fname(&self.graph_name);

        let base = BaseExport {
            class_index: self.graph_class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: PackageIndex::new(0),
            object_name: graph_name,
            object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
            is_asset: true,
            ..Default::default()
        };

        let normal = NormalExport {
            base_export: base,
            extras: Vec::new(),
            properties: Vec::new(), // populated during build()
        };

        self.asset
            .asset_data
            .exports
            .push(Export::NormalExport(normal));
    }

    // ── Node position helper ───────────────────────────────────────────────

    fn next_position(&mut self) -> (i32, i32) {
        let pos = (self.next_node_x, self.next_node_y);
        self.next_node_y += 150;
        if self.next_node_y > 1500 {
            self.next_node_y = 0;
            self.next_node_x -= 300;
        }
        pos
    }

    // ── Node type export creation ──────────────────────────────────────────

    /// Create a node type export and return its export index
    pub fn add_node_type_export(&mut self, node_type: &NodeType) -> PackageIndex {
        let class_import = self.get_node_type_class_import("EdGraphNode");

        let obj_name = self.asset.add_fname(&format!("{}_{}", node_type.name, self.node_type_exports.len()));
        let (pos_x, pos_y) = self.next_position();

        // Build properties for the node type
        let mut props: Vec<Property> = Vec::new();

        // Node position
        let node_pos_x_name = self.asset.add_fname("NodePosX");
        props.push(
            IntProperty {
                name: node_pos_x_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: pos_x,
            }
            .into(),
        );

        let node_pos_y_name = self.asset.add_fname("NodePosY");
        props.push(
            IntProperty {
                name: node_pos_y_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: pos_y,
            }
            .into(),
        );

        // Node title (name)
        let node_title_name = self.asset.add_fname("NodeTitle");
        props.push(
            StrProperty {
                name: node_title_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(node_type.name.clone()),
            }
            .into(),
        );

        // Node category
        let node_category_name = self.asset.add_fname("NodeCategory");
        props.push(
            StrProperty {
                name: node_category_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(node_type.category.clone()),
            }
            .into(),
        );

        // Node tooltip (if present)
        if let Some(ref tooltip) = node_type.tooltip {
            let tooltip_name = self.asset.add_fname("NodeTooltip");
            props.push(
                StrProperty {
                    name: tooltip_name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: Some(tooltip.clone()),
                }
                .into(),
            );
        }

        // Graph back-reference
        let graph_ref_name = self.asset.add_fname("Graph");
        props.push(
            ObjectProperty {
                name: graph_ref_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: self.graph_export_index,
            }
            .into(),
        );

        let base = BaseExport {
            class_index: class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: self.graph_export_index,
            object_name: obj_name,
            object_flags: EObjectFlags::RF_PUBLIC,
            ..Default::default()
        };

        let normal = NormalExport {
            base_export: base,
            extras: Vec::new(),
            properties: props,
        };

        self.asset
            .asset_data
            .exports
            .push(Export::NormalExport(normal));

        // Export indices are 1-based
        let export_index = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
        self.node_type_exports.push(export_index);

        export_index
    }

    // ── Schema export creation ─────────────────────────────────────────────

    /// Create a schema export
    pub fn create_schema_export(&mut self) {
        let schema_name = self.asset.add_fname(&format!("{}_Schema", self.graph_name));

        let mut props: Vec<Property> = Vec::new();

        // Graph back-reference
        let graph_ref_name = self.asset.add_fname("Graph");
        props.push(
            ObjectProperty {
                name: graph_ref_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: self.graph_export_index,
            }
            .into(),
        );

        let base = BaseExport {
            class_index: self.schema_class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: self.graph_export_index,
            object_name: schema_name,
            object_flags: EObjectFlags::RF_PUBLIC,
            ..Default::default()
        };

        let normal = NormalExport {
            base_export: base,
            extras: Vec::new(),
            properties: props,
        };

        self.asset
            .asset_data
            .exports
            .push(Export::NormalExport(normal));

        let export_index = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
        self.schema_export_index = Some(export_index);
    }

    // ── Build: finalize and serialize ──────────────────────────────────────

    /// Finalize the graph and serialize to .uasset bytes.
    pub fn build(mut self) -> Result<Vec<u8>> {
        // Finalize the graph export with node references
        self.finalize_graph_export();

        // Rebuild name map to ensure all FNames are registered
        self.asset.rebuild_name_map();

        // Write to bytes
        let mut output = Cursor::new(Vec::new());
        self.asset
            .write_data(&mut output, None)
            .map_err(|e| GraphError::BinarySerialization(format!("Failed to write .uasset: {}", e)))?;

        Ok(output.into_inner())
    }

    fn finalize_graph_export(&mut self) {
        let mut graph_props: Vec<Property> = Vec::new();

        // Build Nodes array (references to all node type exports)
        if !self.node_type_exports.is_empty() {
            let nodes_name = self.asset.add_fname("Nodes");
            let inner_type = self.asset.add_fname("ObjectProperty");

            let node_values: Vec<Property> = self
                .node_type_exports
                .iter()
                .map(|&idx| {
                    ObjectProperty {
                        name: nodes_name.clone(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        value: idx,
                    }
                    .into()
                })
                .collect();

            let arr_prop = ArrayProperty {
                name: nodes_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                array_type: Some(inner_type),
                value: node_values,
                dummy_property: None,
            };
            graph_props.push(arr_prop.into());
        }

        // Add schema reference if present
        if let Some(schema_idx) = self.schema_export_index {
            let schema_name = self.asset.add_fname("Schema");
            graph_props.push(
                ObjectProperty {
                    name: schema_name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: schema_idx,
                }
                .into(),
            );
        }

        // Update the graph export's properties
        if let Some(Export::NormalExport(ref mut normal)) =
            self.asset.asset_data.exports.first_mut()
        {
            normal.properties = graph_props;
        }
    }
}

// ---------------------------------------------------------------------------
// Graph conversion — GraphEditor → GraphAssetBuilder → bytes
// ---------------------------------------------------------------------------

/// Convert a GraphEditor IR to .uasset bytes.
pub fn serialize_graph_editor(graph: &GraphEditor) -> Result<Vec<u8>> {
    let mut builder = GraphAssetBuilder::new(&graph.name);

    // Add all node types
    for node_type in &graph.node_types {
        builder.add_node_type_export(node_type);
    }

    // Create schema export
    builder.create_schema_export();

    builder.build()
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Serialize a graph editor to binary .uasset format
pub fn serialize(graph: &GraphEditor) -> Result<Vec<u8>> {
    serialize_graph_editor(graph)
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{GraphEditor, NodeType, PinDefinition, PinType, GraphSchema, GraphProperties};

    #[test]
    fn test_simple_graph_serialization() {
        let mut graph = GraphEditor::new("TestGraph");
        
        let node_type = NodeType {
            name: "InputNode".to_string(),
            category: "Input".to_string(),
            inputs: vec![],
            outputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
            ],
            properties: Vec::new(),
            color: Some([1.0, 0.0, 0.0, 1.0]),
            icon: None,
            tooltip: Some("Input node".to_string()),
            execution_logic: None,
        };

        graph.add_node_type(node_type);

        let bytes = serialize(&graph).expect("serialization should succeed");
        assert!(!bytes.is_empty(), "output should not be empty");
        // Verify magic number
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_multiple_node_types() {
        let mut graph = GraphEditor::new("CombatGraph");

        // Input node
        graph.add_node_type(NodeType {
            name: "InputNode".to_string(),
            category: "Combat/Input".to_string(),
            inputs: vec![],
            outputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
            ],
            properties: Vec::new(),
            color: Some([0.0, 1.0, 0.0, 1.0]),
            icon: None,
            tooltip: Some("Combat input".to_string()),
            execution_logic: None,
        });

        // Execution node
        graph.add_node_type(NodeType {
            name: "ExecutionNode".to_string(),
            category: "Combat/Execution".to_string(),
            inputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
                PinDefinition {
                    name: "Damage".to_string(),
                    pin_type: PinType::Float,
                    is_array: false,
                    default_value: Some("10.0".to_string()),
                    tooltip: Some("Damage amount".to_string()),
                },
            ],
            outputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
            ],
            properties: Vec::new(),
            color: Some([1.0, 1.0, 0.0, 1.0]),
            icon: None,
            tooltip: Some("Execute combat action".to_string()),
            execution_logic: None,
        });

        let bytes = serialize(&graph).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_graph_with_schema() {
        let mut graph = GraphEditor::new("SchemaGraph");

        graph.add_node_type(NodeType {
            name: "TestNode".to_string(),
            category: "Test".to_string(),
            inputs: vec![],
            outputs: vec![],
            properties: Vec::new(),
            color: None,
            icon: None,
            tooltip: None,
            execution_logic: None,
        });

        let bytes = serialize(&graph).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_empty_graph() {
        let graph = GraphEditor::new("EmptyGraph");

        let bytes = serialize(&graph).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_node_with_all_pin_types() {
        let mut graph = GraphEditor::new("AllPinTypes");

        graph.add_node_type(NodeType {
            name: "AllPinsNode".to_string(),
            category: "Test".to_string(),
            inputs: vec![
                PinDefinition {
                    name: "ExecIn".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
                PinDefinition {
                    name: "BoolIn".to_string(),
                    pin_type: PinType::Bool,
                    is_array: false,
                    default_value: Some("true".to_string()),
                    tooltip: None,
                },
                PinDefinition {
                    name: "IntIn".to_string(),
                    pin_type: PinType::Int,
                    is_array: false,
                    default_value: Some("42".to_string()),
                    tooltip: None,
                },
                PinDefinition {
                    name: "FloatIn".to_string(),
                    pin_type: PinType::Float,
                    is_array: false,
                    default_value: Some("3.14".to_string()),
                    tooltip: None,
                },
                PinDefinition {
                    name: "StringIn".to_string(),
                    pin_type: PinType::String,
                    is_array: false,
                    default_value: Some("Hello".to_string()),
                    tooltip: None,
                },
            ],
            outputs: vec![
                PinDefinition {
                    name: "ExecOut".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
            ],
            properties: Vec::new(),
            color: Some([0.5, 0.5, 0.5, 1.0]),
            icon: None,
            tooltip: Some("Node with all pin types".to_string()),
            execution_logic: None,
        });

        let bytes = serialize(&graph).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }
}
