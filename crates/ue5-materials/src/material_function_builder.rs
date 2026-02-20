use std::collections::HashMap;
use std::io::Cursor;

use unreal_asset::{
    exports::{BaseExport, Export, ExportBaseTrait, NormalExport},
    flags::EObjectFlags,
    types::PackageIndex,
    Asset, Import,
};
use ue5_asset_utils::ImportBuilder;
use ue5_asset_utils::KainEngineTarget;
use unreal_asset_properties::{
    int_property::{BoolProperty, FloatProperty, IntProperty},
    object_property::ObjectProperty,
    str_property::StrProperty,
    struct_property::StructProperty,
    array_property::ArrayProperty,
    color_property::LinearColorProperty,
    material_input_property::{
        ExpressionInputProperty, MaterialExpression,
    },
    Property,
};
use unreal_asset_base::types::vector::Color;
use ordered_float::OrderedFloat;

use crate::material_graph::*;

// ---------------------------------------------------------------------------
// MaterialFunctionBuilder — programmatic .uasset creation for UE5 material functions
// ---------------------------------------------------------------------------

/// Builds a UE5 MaterialFunction .uasset file programmatically using the unreal_asset library.
///
/// Material functions are reusable node graphs that can be called from multiple materials.
/// They have inputs (FunctionInput nodes) and outputs (FunctionOutput nodes).
///
/// Usage:
/// ```ignore
/// let mut b = MaterialFunctionBuilder::new("MF_Multiply");
/// let input_a = b.add_function_input("A", MaterialInputType::Float);
/// let input_b = b.add_function_input("B", MaterialInputType::Float);
/// let multiply = b.add_multiply_node(input_a, input_b);
/// b.add_function_output(multiply);
/// let bytes = b.build()?;
/// std::fs::write("MF_Multiply.uasset", bytes)?;
/// ```
pub struct MaterialFunctionBuilder {
    asset: Asset<Cursor<Vec<u8>>>,
    function_name: String,

    // Import indices (negative PackageIndex values)
    engine_import: PackageIndex,
    core_uobject_import: PackageIndex,
    function_class_import: PackageIndex,

    // Expression class imports — lazily added as needed
    class_imports: HashMap<String, PackageIndex>,

    // The material function export is always export index 0 (PackageIndex 1)
    function_export_index: PackageIndex,

    // Expression exports: maps builder node_id -> export PackageIndex (1-based positive)
    node_exports: Vec<PackageIndex>,

    // Node positions for editor layout
    next_node_x: i32,
    next_node_y: i32,
}

impl MaterialFunctionBuilder {
    /// Create a new builder for a material function with the given name.
    pub fn new(function_name: &str, engine_target: KainEngineTarget) -> Self {
        let mut asset = Asset::new_empty(engine_target.as_serializer_version());

        // Core imports every material function needs
        let core_uobject_import = ImportBuilder::get_or_add_package(&mut asset, "/Script/CoreUObject");
        let engine_import = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");
        let function_class_import = ImportBuilder::get_or_add_class(&mut asset, "MaterialFunction", engine_import);

        let mut builder = MaterialFunctionBuilder {
            asset,
            function_name: function_name.to_string(),
            engine_import,
            core_uobject_import,
            function_class_import,
            class_imports: HashMap::new(),
            function_export_index: PackageIndex::new(1), // first export is 1-based
            node_exports: Vec::new(),
            next_node_x: -400,
            next_node_y: 0,
        };

        // Create the main MaterialFunction export (index 1)
        builder.create_function_export();

        builder
    }

    // ── Import helpers ─────────────────────────────────────────────────────

    /// Get or create an import for a MaterialExpression class.
    fn get_expression_class_import(&mut self, class_name: &str) -> PackageIndex {
        if let Some(&idx) = self.class_imports.get(class_name) {
            return idx;
        }

        let idx = ImportBuilder::get_or_add_class(
            &mut self.asset,
            class_name,
            self.engine_import,
        );

        self.class_imports.insert(class_name.to_string(), idx);
        idx
    }

    // ── MaterialFunction export creation ───────────────────────────────────

    fn create_function_export(&mut self) {
        let func_name = self.asset.add_fname(&self.function_name);

        let base = BaseExport {
            class_index: self.function_class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: PackageIndex::new(0),
            object_name: func_name,
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

    // ── Generic expression node creation ───────────────────────────────────

    /// Create an expression export and return its node_id (0-based index into node_exports).
    fn add_expression_export(
        &mut self,
        ue_class: &str,
        properties: Vec<Property>,
    ) -> usize {
        let node_id = self.node_exports.len();
        let class_import = self.get_expression_class_import(ue_class);

        let obj_name = self.asset.add_fname(&format!("{}_{}", ue_class, node_id));
        let (pos_x, pos_y) = self.next_position();

        // Build properties with editor position
        let mut all_props = properties;

        let ed_x_name = self.asset.add_fname("MaterialExpressionEditorX");
        all_props.push(
            IntProperty {
                name: ed_x_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: pos_x,
            }
            .into(),
        );

        let ed_y_name = self.asset.add_fname("MaterialExpressionEditorY");
        all_props.push(
            IntProperty {
                name: ed_y_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: pos_y,
            }
            .into(),
        );

        // Function back-reference (MaterialFunction instead of Material)
        let func_ref_name = self.asset.add_fname("Function");
        all_props.push(
            ObjectProperty {
                name: func_ref_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: self.function_export_index,
            }
            .into(),
        );

        let base = BaseExport {
            class_index: class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: self.function_export_index,
            object_name: obj_name,
            object_flags: EObjectFlags::RF_PUBLIC,
            ..Default::default()
        };

        let normal = NormalExport {
            base_export: base,
            extras: Vec::new(),
            properties: all_props,
        };

        self.asset
            .asset_data
            .exports
            .push(Export::NormalExport(normal));

        // Export indices are 1-based
        let export_index = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
        self.node_exports.push(export_index);

        node_id
    }

    /// Create an ExpressionInput property wired to a node.
    fn make_input_property(
        &mut self,
        prop_name: &str,
        source_node: usize,
        output_index: i32,
    ) -> Property {
        let name = self.asset.add_fname(prop_name);
        let expr = self.make_expression_ref(source_node, output_index);

        ExpressionInputProperty {
            name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            material_expression: expr,
        }
        .into()
    }

    /// Create a MaterialExpression reference for wiring nodes.
    fn make_expression_ref(&mut self, source_node: usize, output_index: i32) -> MaterialExpression {
        let export_idx = self.node_exports[source_node];
        // Get the object name of the export for the expression_name field
        let export = &self.asset.asset_data.exports[(export_idx.index - 1) as usize];
        let expr_name = export.get_base_export().object_name.clone();

        // All FName fields must be backed by the name map (not Dummy).
        // Empty string FNames are valid — they just mean "no name".
        let empty_name = self.asset.add_fname("None");
        let input_name = self.asset.add_fname("None");

        MaterialExpression {
            name: empty_name,
            extras: vec![0u8; 20],
            output_index,
            input_name,
            expression_name: expr_name,
        }
    }

    // ── Function-specific nodes ────────────────────────────────────────────

    /// Add a FunctionInput node (MaterialExpressionFunctionInput).
    /// Returns the node_id that can be used to wire to other nodes.
    pub fn add_function_input(&mut self, name: &str, input_type: MaterialInputType) -> usize {
        let input_name_fname = self.asset.add_fname("InputName");
        let input_type_fname = self.asset.add_fname("InputType");

        let mut props = Vec::new();
        props.push(
            StrProperty {
                name: input_name_fname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(name.to_string()),
            }
            .into(),
        );

        // Map MaterialInputType to UE5's EFunctionInputType enum
        let input_type_value = match input_type {
            MaterialInputType::Float => 0u8,      // FunctionInput_Scalar
            MaterialInputType::Vec2 => 1u8,       // FunctionInput_Vector2
            MaterialInputType::Vec3 => 2u8,       // FunctionInput_Vector3
            MaterialInputType::Vec4 => 3u8,       // FunctionInput_Vector4
            MaterialInputType::Texture2D => 4u8,  // FunctionInput_Texture2D
            MaterialInputType::Color => 2u8,      // Same as Vec3
        };

        props.push(
            IntProperty {
                name: input_type_fname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: input_type_value as i32,
            }
            .into(),
        );

        self.add_expression_export("MaterialExpressionFunctionInput", props)
    }

    /// Add a FunctionOutput node (MaterialExpressionFunctionOutput).
    /// This connects the function's output to a specific node.
    pub fn add_function_output(&mut self, node_id: usize) {
        let output_name_fname = self.asset.add_fname("OutputName");
        
        let mut props = Vec::new();
        
        // Set output name (default to "Result")
        props.push(
            StrProperty {
                name: output_name_fname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some("Result".to_string()),
            }
            .into(),
        );

        // Connect to the output node
        props.push(self.make_input_property("A", node_id, 0));

        self.add_expression_export("MaterialExpressionFunctionOutput", props);
    }

    // ── Reuse node creation methods from MaterialAssetBuilder ──────────────
    // These are identical to MaterialAssetBuilder but work with Function back-reference

    pub fn add_constant_node(&mut self, value: f32) -> usize {
        let r_name = self.asset.add_fname("R");
        let props = vec![FloatProperty {
            name: r_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: OrderedFloat(value),
        }
        .into()];
        self.add_expression_export("MaterialExpressionConstant", props)
    }

    pub fn add_constant3_node(&mut self, r: f32, g: f32, b: f32) -> usize {
        let name = self.asset.add_fname("Constant");
        let struct_type = self.asset.add_fname("LinearColor");

        let color_struct = StructProperty {
            name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            struct_type: Some(struct_type),
            struct_guid: Some(Default::default()),
            serialize_none: true,
            value: vec![LinearColorProperty {
                name: Default::default(),
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                color: Color::new(
                    OrderedFloat(r),
                    OrderedFloat(g),
                    OrderedFloat(b),
                    OrderedFloat(1.0),
                ),
            }
            .into()],
        };

        self.add_expression_export("MaterialExpressionConstant3Vector", vec![color_struct.into()])
    }

    fn add_binary_op_node(&mut self, ue_class: &str, a: usize, b: usize) -> usize {
        let a_prop = self.make_input_property("A", a, 0);
        let b_prop = self.make_input_property("B", b, 0);
        self.add_expression_export(ue_class, vec![a_prop, b_prop])
    }

    pub fn add_add_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionAdd", a, b)
    }

    pub fn add_subtract_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionSubtract", a, b)
    }

    pub fn add_multiply_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionMultiply", a, b)
    }

    pub fn add_divide_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionDivide", a, b)
    }

    pub fn add_dot_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionDotProduct", a, b)
    }

    pub fn add_power_node(&mut self, base: usize, exponent: usize) -> usize {
        let base_prop = self.make_input_property("Base", base, 0);
        let exp_prop = self.make_input_property("Exponent", exponent, 0);
        self.add_expression_export("MaterialExpressionPower", vec![base_prop, exp_prop])
    }

    pub fn add_lerp_node(&mut self, a: usize, b: usize, alpha: usize) -> usize {
        let a_prop = self.make_input_property("A", a, 0);
        let b_prop = self.make_input_property("B", b, 0);
        let alpha_prop = self.make_input_property("Alpha", alpha, 0);
        self.add_expression_export(
            "MaterialExpressionLinearInterpolate",
            vec![a_prop, b_prop, alpha_prop],
        )
    }

    fn add_unary_op_node(&mut self, ue_class: &str, input: usize) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);
        self.add_expression_export(ue_class, vec![input_prop])
    }

    pub fn add_normalize_node(&mut self, input: usize) -> usize {
        let prop = self.make_input_property("VectorInput", input, 0);
        self.add_expression_export("MaterialExpressionNormalize", vec![prop])
    }

    pub fn add_saturate_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionSaturate", input)
    }

    pub fn add_abs_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionAbs", input)
    }

    pub fn add_frac_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionFrac", input)
    }

    pub fn add_floor_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionFloor", input)
    }

    pub fn add_ceil_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionCeil", input)
    }

    pub fn add_round_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionRound", input)
    }

    pub fn add_sqrt_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionSquareRoot", input)
    }

    pub fn add_sine_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionSine", input)
    }

    pub fn add_cosine_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionCosine", input)
    }

    pub fn add_length_node(&mut self, input: usize) -> usize {
        let prop = self.make_input_property("Input", input, 0);
        self.add_expression_export("MaterialExpressionLength", vec![prop])
    }

    pub fn add_cross_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionCrossProduct", a, b)
    }

    pub fn add_min_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionMin", a, b)
    }

    pub fn add_max_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionMax", a, b)
    }

    pub fn add_distance_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionDistance", a, b)
    }

    pub fn add_append_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionAppendVector", a, b)
    }

    pub fn add_clamp_node(&mut self, input: usize, min: usize, max: usize) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);
        let min_prop = self.make_input_property("Min", min, 0);
        let max_prop = self.make_input_property("Max", max, 0);
        self.add_expression_export("MaterialExpressionClamp", vec![input_prop, min_prop, max_prop])
    }

    pub fn add_constant4_node(&mut self, r: f32, g: f32, b: f32, a: f32) -> usize {
        let name = self.asset.add_fname("Constant");
        let struct_type = self.asset.add_fname("LinearColor");

        let color_struct = StructProperty {
            name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            struct_type: Some(struct_type),
            struct_guid: Some(Default::default()),
            serialize_none: true,
            value: vec![LinearColorProperty {
                name: Default::default(),
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                color: Color::new(
                    OrderedFloat(r),
                    OrderedFloat(g),
                    OrderedFloat(b),
                    OrderedFloat(a),
                ),
            }
            .into()],
        };

        self.add_expression_export("MaterialExpressionConstant4Vector", vec![color_struct.into()])
    }

    pub fn add_time_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionTime", Vec::new())
    }

    pub fn add_texture_coordinate_node(&mut self, index: u32) -> usize {
        let idx_name = self.asset.add_fname("CoordinateIndex");
        let props = vec![IntProperty {
            name: idx_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: index as i32,
        }
        .into()];
        self.add_expression_export("MaterialExpressionTextureCoordinate", props)
    }

    pub fn add_component_mask_node(&mut self, input: usize, r: bool, g: bool, b: bool, a: bool) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);
        let r_name = self.asset.add_fname("R");
        let g_name = self.asset.add_fname("G");
        let b_name = self.asset.add_fname("B");
        let a_name = self.asset.add_fname("A");
        let props = vec![
            input_prop,
            BoolProperty { name: r_name, ancestry: Default::default(), property_guid: None, duplication_index: 0, value: r }.into(),
            BoolProperty { name: g_name, ancestry: Default::default(), property_guid: None, duplication_index: 0, value: g }.into(),
            BoolProperty { name: b_name, ancestry: Default::default(), property_guid: None, duplication_index: 0, value: b }.into(),
            BoolProperty { name: a_name, ancestry: Default::default(), property_guid: None, duplication_index: 0, value: a }.into(),
        ];
        self.add_expression_export("MaterialExpressionComponentMask", props)
    }

    // ── Build: finalize and serialize ──────────────────────────────────────

    /// Finalize the material function and serialize to .uasset bytes.
    pub fn build(mut self) -> Result<Vec<u8>, String> {
        // Build the FunctionExpressions array property on the function export
        self.finalize_function_export();

        // Rebuild name map to ensure all FNames are registered
        self.asset.rebuild_name_map();

        // Write to bytes
        let mut output = Cursor::new(Vec::new());
        self.asset
            .write_data(&mut output, None)
            .map_err(|e| format!("Failed to write .uasset: {}", e))?;

        Ok(output.into_inner())
    }

    fn finalize_function_export(&mut self) {
        let mut func_props: Vec<Property> = Vec::new();

        // Build FunctionExpressions array (references to all expression exports)
        if !self.node_exports.is_empty() {
            let expr_name = self.asset.add_fname("FunctionExpressions");
            let inner_type = self.asset.add_fname("ObjectProperty");

            let expr_values: Vec<Property> = self
                .node_exports
                .iter()
                .map(|&idx| {
                    ObjectProperty {
                        name: expr_name.clone(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        value: idx,
                    }
                    .into()
                })
                .collect();

            let arr_prop = ArrayProperty {
                name: expr_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                array_type: Some(inner_type),
                value: expr_values,
                dummy_property: None,
            };
            func_props.push(arr_prop.into());
        }

        // Update the function export's properties
        if let Some(Export::NormalExport(ref mut normal)) =
            self.asset.asset_data.exports.first_mut()
        {
            normal.properties = func_props;
        }
    }
}

// ---------------------------------------------------------------------------
// MaterialFunction IR type
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct MaterialFunction {
    pub name: String,
    pub inputs: Vec<MaterialFunctionInput>,
    pub nodes: Vec<MaterialNode>,
    pub output: String,  // node_id of output
    pub description: String,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct MaterialFunctionInput {
    pub name: String,
    pub input_type: MaterialInputType,
    pub default_value: Option<String>,
}

/// Convert a MaterialFunction IR to .uasset bytes.
pub fn serialize_material_function(func: &MaterialFunction) -> Result<Vec<u8>, String> {
    let mut builder = MaterialFunctionBuilder::new(&format!("MF_{}", func.name), KainEngineTarget::default());

    // Map graph node IDs → builder node IDs
    let mut node_map: HashMap<String, usize> = HashMap::new();

    // Add function inputs first
    for input in &func.inputs {
        let node_id = builder.add_function_input(&input.name, input.input_type.clone());
        // Function inputs need to be addressable by name in the node graph
        node_map.insert(input.name.clone(), node_id);
    }

    // Process nodes in order
    for node in &func.nodes {
        let builder_id = convert_function_node(&mut builder, &node.node_type, &node_map)?;
        node_map.insert(node.id.clone(), builder_id);
    }

    // Connect output
    if let Some(&output_node_id) = node_map.get(&func.output) {
        builder.add_function_output(output_node_id);
    } else {
        return Err(format!("Output node '{}' not found", func.output));
    }

    builder.build()
}

/// Convert a single MaterialNodeType into builder calls (for functions).
fn convert_function_node(
    builder: &mut MaterialFunctionBuilder,
    node_type: &MaterialNodeType,
    node_map: &HashMap<String, usize>,
) -> Result<usize, String> {
    // Resolve helper
    let resolve = |id: &str| -> Result<usize, String> {
        node_map
            .get(id)
            .copied()
            .ok_or_else(|| format!("Unknown node reference: '{}'", id))
    };

    match node_type {
        // Constants
        MaterialNodeType::ConstantFloat { value } => Ok(builder.add_constant_node(*value)),
        MaterialNodeType::ConstantVec3 { value } | MaterialNodeType::ConstantVector3 { value } => {
            Ok(builder.add_constant3_node(value[0], value[1], value[2]))
        }
        MaterialNodeType::ConstantVec4 { value } | MaterialNodeType::ConstantVector4 { value } => {
            Ok(builder.add_constant4_node(value[0], value[1], value[2], value[3]))
        }

        // Arithmetic (binary)
        MaterialNodeType::Add { a, b } => {
            Ok(builder.add_add_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Subtract { a, b } => {
            Ok(builder.add_subtract_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Multiply { a, b } => {
            Ok(builder.add_multiply_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Divide { a, b } => {
            Ok(builder.add_divide_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Dot { a, b } | MaterialNodeType::DotProduct { a, b } => {
            Ok(builder.add_dot_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Cross { a, b } => {
            Ok(builder.add_cross_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Min { a, b } => {
            Ok(builder.add_min_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Max { a, b } => {
            Ok(builder.add_max_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Distance { a, b } => {
            Ok(builder.add_distance_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Append { a, b } | MaterialNodeType::AppendVector { a, b } => {
            Ok(builder.add_append_node(resolve(a)?, resolve(b)?))
        }
        MaterialNodeType::Power { base, exponent } => {
            Ok(builder.add_power_node(resolve(base)?, resolve(exponent)?))
        }

        // 3-input
        MaterialNodeType::Lerp { a, b, alpha } => {
            Ok(builder.add_lerp_node(resolve(a)?, resolve(b)?, resolve(alpha)?))
        }
        MaterialNodeType::Clamp { input, min, max } => {
            Ok(builder.add_clamp_node(resolve(input)?, resolve(min)?, resolve(max)?))
        }

        // Unary
        MaterialNodeType::Normalize { input } => {
            Ok(builder.add_normalize_node(resolve(input)?))
        }
        MaterialNodeType::Length { input } => {
            Ok(builder.add_length_node(resolve(input)?))
        }
        MaterialNodeType::Abs { input } => Ok(builder.add_abs_node(resolve(input)?)),
        MaterialNodeType::Saturate { input } => {
            Ok(builder.add_saturate_node(resolve(input)?))
        }
        MaterialNodeType::Frac { input } => Ok(builder.add_frac_node(resolve(input)?)),
        MaterialNodeType::Floor { input } => Ok(builder.add_floor_node(resolve(input)?)),
        MaterialNodeType::Ceil { input } => Ok(builder.add_ceil_node(resolve(input)?)),
        MaterialNodeType::Round { input } => Ok(builder.add_round_node(resolve(input)?)),
        MaterialNodeType::Sqrt { input } => Ok(builder.add_sqrt_node(resolve(input)?)),
        MaterialNodeType::Sine { input } => Ok(builder.add_sine_node(resolve(input)?)),
        MaterialNodeType::Cosine { input } => Ok(builder.add_cosine_node(resolve(input)?)),

        // Time
        MaterialNodeType::Time => Ok(builder.add_time_node()),

        // Texture
        MaterialNodeType::TextureCoordinate { index, .. } => {
            Ok(builder.add_texture_coordinate_node(*index))
        }
        MaterialNodeType::ComponentMask { input, mask } => {
            let node = resolve(input)?;
            let m = mask.to_lowercase();
            Ok(builder.add_component_mask_node(
                node,
                m.contains('r'),
                m.contains('g'),
                m.contains('b'),
                m.contains('a'),
            ))
        }

        // Node types that don't make sense in material functions
        _ => Err(format!("Node type {:?} not yet supported in material functions", node_type)),
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_function() {
        let mut builder = MaterialFunctionBuilder::new("MF_Multiply", KainEngineTarget::default());

        let input_a = builder.add_function_input("A", MaterialInputType::Float);
        let input_b = builder.add_function_input("B", MaterialInputType::Float);
        let multiply = builder.add_multiply_node(input_a, input_b);
        builder.add_function_output(multiply);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty(), "output should not be empty");
        // Verify magic number
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_lerp_function() {
        let mut builder = MaterialFunctionBuilder::new("MF_Lerp", KainEngineTarget::default());

        let input_a = builder.add_function_input("A", MaterialInputType::Vec3);
        let input_b = builder.add_function_input("B", MaterialInputType::Vec3);
        let input_alpha = builder.add_function_input("Alpha", MaterialInputType::Float);
        let lerp = builder.add_lerp_node(input_a, input_b, input_alpha);
        builder.add_function_output(lerp);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_normalize_function() {
        let mut builder = MaterialFunctionBuilder::new("MF_Normalize", KainEngineTarget::default());

        let input = builder.add_function_input("Vector", MaterialInputType::Vec3);
        let normalized = builder.add_normalize_node(input);
        builder.add_function_output(normalized);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_function_ir_serialization() {
        let func = MaterialFunction {
            name: "TestMultiply".to_string(),
            inputs: vec![
                MaterialFunctionInput {
                    name: "A".to_string(),
                    input_type: MaterialInputType::Float,
                    default_value: None,
                },
                MaterialFunctionInput {
                    name: "B".to_string(),
                    input_type: MaterialInputType::Float,
                    default_value: None,
                },
            ],
            nodes: vec![
                MaterialNode {
                    id: "mul".to_string(),
                    node_type: MaterialNodeType::Multiply {
                        a: "A".to_string(),
                        b: "B".to_string(),
                    },
                    position: (0, 0),
                },
            ],
            output: "mul".to_string(),
            description: "Multiplies two floats".to_string(),
        };

        let bytes = serialize_material_function(&func).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }
}
