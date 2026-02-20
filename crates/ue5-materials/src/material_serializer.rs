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
    enum_property::EnumProperty,
    material_input_property::{
        ColorMaterialInputProperty, ExpressionInputProperty, MaterialExpression,
    },
    Property,
};
use unreal_asset_base::types::vector::Color;
use ordered_float::OrderedFloat;

use crate::material_graph::*;

// ---------------------------------------------------------------------------
// MaterialAssetBuilder — programmatic .uasset creation for UE5 materials
// ---------------------------------------------------------------------------

/// Builds a UE5 Material .uasset file programmatically using the unreal_asset library.
///
/// Usage:
/// ```ignore
/// let mut b = MaterialAssetBuilder::new("M_Test");
/// let c1 = b.add_constant_node(0.5);
/// let c2 = b.add_constant_node(0.3);
/// let add = b.add_add_node(c1, c2);
/// b.connect_to_base_color(add);
/// let bytes = b.build()?;
/// std::fs::write("M_Test.uasset", bytes)?;
/// ```
pub struct MaterialAssetBuilder {
    asset: Asset<Cursor<Vec<u8>>>,
    material_name: String,

    // Import indices (negative PackageIndex values)
    engine_import: PackageIndex,
    core_uobject_import: PackageIndex,
    material_class_import: PackageIndex,

    // Expression class imports — lazily added as needed
    class_imports: HashMap<String, PackageIndex>,

    // The material export is always export index 0 (PackageIndex 1)
    material_export_index: PackageIndex,

    // Expression exports: maps builder node_id -> export PackageIndex (1-based positive)
    node_exports: Vec<PackageIndex>,

    // Track which material outputs are connected
    output_connections: HashMap<String, usize>, // output_name -> node_id

    // Material properties
    blend_mode: u8,
    shading_model: u8,
    two_sided: bool,

    // Node positions for editor layout
    next_node_x: i32,
    next_node_y: i32,
}

impl MaterialAssetBuilder {
    /// Create a new builder for a material with the given name.
    /// `engine_target` controls the UE version the asset is built for.
    /// Use `KainEngineTarget::default()` if you don't need to target a specific
    /// version.
    pub fn new(material_name: &str, engine_target: KainEngineTarget) -> Self {
        let mut asset = Asset::new_empty(engine_target.as_serializer_version());

        // Log if we're serializing below the requested target version
        if engine_target.is_above_serializer_ceiling() {
            // Non-fatal: the 5.2 format is accepted by UE5.3+
            // (Epic maintains backwards format compatibility).
            log::debug!(
                "MaterialAssetBuilder: targeting UE {engine_target} but serializing \
                 at {} format (highest known; UE{} will accept this file)",
                engine_target.serializer_ceiling(),
                engine_target,
            );
        }

        // ── Core imports every material needs (via shared ImportBuilder) ────
        let core_uobject_import = ImportBuilder::get_or_add_package(&mut asset, "/Script/CoreUObject");
        let engine_import = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");
        let material_class_import = ImportBuilder::get_or_add_class(&mut asset, "Material", engine_import);
        let mut builder = MaterialAssetBuilder {
            asset,
            material_name: material_name.to_string(),
            engine_import,
            core_uobject_import,
            material_class_import,
            class_imports: HashMap::new(),
            material_export_index: PackageIndex::new(1), // first export is 1-based
            node_exports: Vec::new(),
            output_connections: HashMap::new(),
            blend_mode: 0,    // Opaque
            shading_model: 1, // DefaultLit (MSM_DefaultLit = 1)
            two_sided: false,
            next_node_x: -400,
            next_node_y: 0,
        };

        // Create the main Material export (index 1)
        builder.create_material_export();

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

    // ── Material export creation ───────────────────────────────────────────

    fn create_material_export(&mut self) {
        let mat_name = self.asset.add_fname(&self.material_name);

        let base = BaseExport {
            class_index: self.material_class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: PackageIndex::new(0),
            object_name: mat_name,
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

        // Material back-reference
        let mat_ref_name = self.asset.add_fname("Material");
        all_props.push(
            ObjectProperty {
                name: mat_ref_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: self.material_export_index,
            }
            .into(),
        );

        let base = BaseExport {
            class_index: class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: self.material_export_index,
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

    /// Create a MaterialExpression reference for wiring inputs.
    fn make_expression_ref(&mut self, node_id: usize, output_index: i32) -> MaterialExpression {
        let export_idx = self.node_exports[node_id];
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

    // ── Phase 0 & 1: Core node types ───────────────────────────────────────

    // -- Constants --

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

    // -- Arithmetic (2-input) --

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

    pub fn add_cross_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionCrossProduct", a, b)
    }

    pub fn add_min_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionMin", a, b)
    }

    pub fn add_max_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionMax", a, b)
    }

    pub fn add_power_node(&mut self, base: usize, exponent: usize) -> usize {
        let base_prop = self.make_input_property("Base", base, 0);
        let exp_prop = self.make_input_property("Exponent", exponent, 0);
        self.add_expression_export("MaterialExpressionPower", vec![base_prop, exp_prop])
    }

    pub fn add_distance_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionDistance", a, b)
    }

    pub fn add_append_node(&mut self, a: usize, b: usize) -> usize {
        self.add_binary_op_node("MaterialExpressionAppendVector", a, b)
    }

    // -- Unary operations --

    fn add_unary_op_node(&mut self, ue_class: &str, input: usize) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);
        self.add_expression_export(ue_class, vec![input_prop])
    }

    pub fn add_normalize_node(&mut self, input: usize) -> usize {
        let prop = self.make_input_property("VectorInput", input, 0);
        self.add_expression_export("MaterialExpressionNormalize", vec![prop])
    }

    pub fn add_length_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionLength", input)
    }

    pub fn add_abs_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionAbs", input)
    }

    pub fn add_saturate_node(&mut self, input: usize) -> usize {
        self.add_unary_op_node("MaterialExpressionSaturate", input)
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

    // -- 3-input operations --

    pub fn add_lerp_node(&mut self, a: usize, b: usize, alpha: usize) -> usize {
        let a_prop = self.make_input_property("A", a, 0);
        let b_prop = self.make_input_property("B", b, 0);
        let alpha_prop = self.make_input_property("Alpha", alpha, 0);
        self.add_expression_export(
            "MaterialExpressionLinearInterpolate",
            vec![a_prop, b_prop, alpha_prop],
        )
    }

    pub fn add_clamp_node(&mut self, input: usize, min: usize, max: usize) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);
        let min_prop = self.make_input_property("Min", min, 0);
        let max_prop = self.make_input_property("Max", max, 0);
        self.add_expression_export(
            "MaterialExpressionClamp",
            vec![input_prop, min_prop, max_prop],
        )
    }

    // -- Texture sampling --

    /// Add a plain TextureSample node (non-parameter, no exposed name).
    pub fn add_texture_sample_node(&mut self, uv: Option<usize>) -> usize {
        let mut props: Vec<Property> = Vec::new();
        if let Some(uv_node) = uv {
            props.push(self.make_input_property("Coordinates", uv_node, 0));
        }
        self.add_expression_export("MaterialExpressionTextureSample", props)
    }

    pub fn add_texture_sample_parameter(&mut self, param_name: &str, uv: Option<usize>) -> usize {
        let pname = self.asset.add_fname("ParameterName");
        let mut props: Vec<Property> = vec![StrProperty {
            name: pname,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: Some(param_name.to_string()),
        }
        .into()];

        if let Some(uv_node) = uv {
            props.push(self.make_input_property("Coordinates", uv_node, 0));
        }

        self.add_expression_export("MaterialExpressionTextureSampleParameter2D", props)
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

    pub fn add_component_mask_node(
        &mut self,
        input: usize,
        r: bool,
        g: bool,
        b: bool,
        a: bool,
    ) -> usize {
        let input_prop = self.make_input_property("Input", input, 0);

        let r_name = self.asset.add_fname("R");
        let g_name = self.asset.add_fname("G");
        let b_name = self.asset.add_fname("B");
        let a_name = self.asset.add_fname("A");

        let mut props = vec![input_prop];
        props.push(
            BoolProperty {
                name: r_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: r,
            }
            .into(),
        );
        props.push(
            BoolProperty {
                name: g_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: g,
            }
            .into(),
        );
        props.push(
            BoolProperty {
                name: b_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: b,
            }
            .into(),
        );
        props.push(
            BoolProperty {
                name: a_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: a,
            }
            .into(),
        );

        self.add_expression_export("MaterialExpressionComponentMask", props)
    }

    // -- Parameters --

    pub fn add_scalar_parameter_node(&mut self, param_name: &str, default: f32) -> usize {
        let pname = self.asset.add_fname("ParameterName");
        let default_name = self.asset.add_fname("DefaultValue");
        let props = vec![
            Property::from(StrProperty {
                name: pname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(param_name.to_string()),
            }),
            Property::from(FloatProperty {
                name: default_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: OrderedFloat(default),
            }),
        ];
        self.add_expression_export("MaterialExpressionScalarParameter", props)
    }

    pub fn add_color_parameter_node(&mut self, param_name: &str, default: [f32; 4]) -> usize {
        let pname = self.asset.add_fname("ParameterName");
        let default_name = self.asset.add_fname("DefaultValue");
        let struct_type = self.asset.add_fname("LinearColor");

        let color_struct = StructProperty {
            name: default_name,
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
                    OrderedFloat(default[0]),
                    OrderedFloat(default[1]),
                    OrderedFloat(default[2]),
                    OrderedFloat(default[3]),
                ),
            }
            .into()],
        };

        let props = vec![
            Property::from(StrProperty {
                name: pname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(param_name.to_string()),
            }),
            color_struct.into(),
        ];
        self.add_expression_export("MaterialExpressionVectorParameter", props)
    }

    pub fn add_vector_parameter_node(&mut self, param_name: &str, default: [f32; 3]) -> usize {
        let pname = self.asset.add_fname("ParameterName");
        let default_name = self.asset.add_fname("DefaultValue");
        let struct_type = self.asset.add_fname("LinearColor");

        let color_struct = StructProperty {
            name: default_name,
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
                    OrderedFloat(default[0]),
                    OrderedFloat(default[1]),
                    OrderedFloat(default[2]),
                    OrderedFloat(1.0),
                ),
            }
            .into()],
        };

        let props = vec![
            Property::from(StrProperty {
                name: pname,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(param_name.to_string()),
            }),
            color_struct.into(),
        ];
        self.add_expression_export("MaterialExpressionVectorParameter", props)
    }

    // ── Phase 2: Advanced features ─────────────────────────────────────────

    // -- Time node (with dedup) --

    pub fn add_time_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionTime", Vec::new())
    }

    // -- Fresnel --

    pub fn add_fresnel_node(&mut self, exponent: usize, base_reflect: usize) -> usize {
        let exp_prop = self.make_input_property("ExponentIn", exponent, 0);
        let base_prop = self.make_input_property("BaseReflectFractionIn", base_reflect, 0);
        self.add_expression_export("MaterialExpressionFresnel", vec![exp_prop, base_prop])
    }

    // -- Panner (UV scroll) --

    pub fn add_panner_node(&mut self, coordinate: usize, speed_x: f32, speed_y: f32) -> usize {
        let coord_prop = self.make_input_property("Coordinate", coordinate, 0);
        let sx_name = self.asset.add_fname("SpeedX");
        let sy_name = self.asset.add_fname("SpeedY");
        let props = vec![
            coord_prop,
            FloatProperty {
                name: sx_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: OrderedFloat(speed_x),
            }
            .into(),
            FloatProperty {
                name: sy_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: OrderedFloat(speed_y),
            }
            .into(),
        ];
        self.add_expression_export("MaterialExpressionPanner", props)
    }

    // -- Rotator (UV rotate) --

    pub fn add_rotator_node(
        &mut self,
        coordinate: usize,
        center: Option<usize>,
        angle: Option<usize>,
    ) -> usize {
        let mut props = vec![self.make_input_property("Coordinate", coordinate, 0)];
        if let Some(c) = center {
            props.push(self.make_input_property("CenterPoint", c, 0));
        }
        if let Some(a) = angle {
            props.push(self.make_input_property("Time", a, 0));
        }
        self.add_expression_export("MaterialExpressionRotator", props)
    }

    // -- MaterialFunctionCall --

    pub fn add_material_function_call(
        &mut self,
        function_path: &str,
        inputs: &[(String, usize)],
    ) -> usize {
        let mut props = Vec::new();

        // Create an import for the MaterialFunction asset
        // function_path should be like "/Game/Materials/Functions/MF_MyFunction.MF_MyFunction"
        let func_import = ImportBuilder::resolve_object_import(
            &mut self.asset,
            function_path,
        );

        // Set the MaterialFunction property to reference the imported function
        let func_name = self.asset.add_fname("MaterialFunction");
        props.push(
            ObjectProperty {
                name: func_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: func_import,
            }
            .into(),
        );

        // Wire up function inputs
        for (input_name, node_id) in inputs {
            props.push(self.make_input_property(input_name, *node_id, 0));
        }

        self.add_expression_export("MaterialExpressionMaterialFunctionCall", props)
    }

    // -- Custom HLSL --

    pub fn add_custom_hlsl_node(
        &mut self,
        code: &str,
        output_type: &CustomOutputType,
        inputs: &[(String, usize)],
    ) -> usize {
        let code_name = self.asset.add_fname("Code");
        let output_type_name = self.asset.add_fname("OutputType");

        let cmot_value_str = match output_type {
            CustomOutputType::Float1 => "CMOT_Float1",
            CustomOutputType::Float2 => "CMOT_Float2",
            CustomOutputType::Float3 => "CMOT_Float3",
            CustomOutputType::Float4 => "CMOT_Float4",
        };
        let output_enum_type = self.asset.add_fname("ECustomMaterialOutputType");
        let output_value = self.asset.add_fname(cmot_value_str);

        let mut props: Vec<Property> = vec![
            StrProperty {
                name: code_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(code.to_string()),
            }
            .into(),
            EnumProperty {
                name: output_type_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                enum_type: Some(output_enum_type),
                inner_type: None,
                value: Some(output_value),
            }
            .into(),
        ];

        for (input_name, node_id) in inputs {
            props.push(self.make_input_property(input_name, *node_id, 0));
        }

        self.add_expression_export("MaterialExpressionCustom", props)
    }

    // ── Phase 7.3: Material Layers ─────────────────────────────────────────

    /// Add a material layer blend node (two-layer blend)
    /// Blends base_layer and blend_layer using the specified blend mode and alpha
    pub fn add_material_layer_node(
        &mut self,
        base: usize,
        blend: usize,
        mode: &LayerBlendMode,
        alpha: usize,
    ) -> usize {
        // Material layers in UE5 are implemented using lerp/add/multiply nodes
        // We generate the appropriate node based on blend mode
        match mode {
            LayerBlendMode::Lerp => {
                // Simple lerp between base and blend
                self.add_lerp_node(base, blend, alpha)
            }
            LayerBlendMode::Add => {
                // Multiply blend by alpha, then add to base
                let scaled_blend = self.add_multiply_node(blend, alpha);
                self.add_add_node(base, scaled_blend)
            }
            LayerBlendMode::Multiply => {
                // Lerp between base and (base * blend) using alpha
                let multiplied = self.add_multiply_node(base, blend);
                self.add_lerp_node(base, multiplied, alpha)
            }
            LayerBlendMode::Overlay => {
                // Overlay: a < 0.5 ? 2*a*b : 1 - 2*(1-a)*(1-b)
                // Implemented as lerp(2*a*b, 1-(1-a)*(1-b)*2, a) blended by alpha
                let two = self.add_constant_node(2.0);
                let one = self.add_constant_node(1.0);
                // Multiply path: 2 * base * blend
                let mul_part = self.add_multiply_node(base, blend);
                let mul_doubled = self.add_multiply_node(mul_part, two);
                // Screen path: 1 - 2*(1-base)*(1-blend)
                let inv_base = self.add_subtract_node(one, base);
                let inv_blend = self.add_subtract_node(one, blend);
                let screen_inner = self.add_multiply_node(inv_base, inv_blend);
                let screen_doubled = self.add_multiply_node(screen_inner, two);
                let screen_part = self.add_subtract_node(one, screen_doubled);
                // Lerp between multiply and screen using base as selector
                let overlay = self.add_lerp_node(mul_doubled, screen_part, base);
                // Apply blend alpha
                self.add_lerp_node(base, overlay, alpha)
            }
            LayerBlendMode::Screen => {
                // Screen: 1 - (1-a)*(1-b)
                // Simplified: lerp towards white based on blend
                let one = self.add_constant_node(1.0);
                let inv_base = self.add_subtract_node(one, base);
                let inv_blend = self.add_subtract_node(one, blend);
                let multiplied = self.add_multiply_node(inv_base, inv_blend);
                let screen = self.add_subtract_node(one, multiplied);
                self.add_lerp_node(base, screen, alpha)
            }
        }
    }

    /// Add a multi-layer blend node (3+ layers)
    /// Blends multiple layers sequentially using the specified blend modes and alphas
    /// layers[0] is the base, subsequent layers are blended on top
    pub fn add_material_layer_blend_node(
        &mut self,
        layers: &[usize],
        modes: &[LayerBlendMode],
        alphas: &[usize],
    ) -> usize {
        if layers.is_empty() {
            panic!("MaterialLayerBlend requires at least one layer");
        }
        
        if layers.len() == 1 {
            // Single layer - just return it
            return layers[0];
        }
        
        if modes.len() != layers.len() - 1 || alphas.len() != layers.len() - 1 {
            panic!(
                "MaterialLayerBlend: modes and alphas must have length = layers.len() - 1. \
                 Got {} layers, {} modes, {} alphas",
                layers.len(),
                modes.len(),
                alphas.len()
            );
        }
        
        // Start with the base layer
        let mut result = layers[0];
        
        // Blend each subsequent layer on top
        for i in 1..layers.len() {
            result = self.add_material_layer_node(
                result,
                layers[i],
                &modes[i - 1],
                alphas[i - 1],
            );
        }
        
        result
    }

    // ── Phase 7.4: World-Space Operations ──────────────────────────────────

    // -- World Position --

    pub fn add_world_position_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionWorldPosition", Vec::new())
    }

    pub fn add_world_normal_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionVertexNormalWS", Vec::new())
    }

    pub fn add_absolute_world_position_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionAbsoluteWorldPosition", Vec::new())
    }

    pub fn add_camera_position_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionCameraPositionWS", Vec::new())
    }

    pub fn add_object_position_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionObjectPositionWS", Vec::new())
    }

    pub fn add_object_orientation_node(&mut self) -> usize {
        self.add_expression_export("MaterialExpressionObjectOrientation", Vec::new())
    }

    // -- Triplanar Sampling --
    // This is a complex node that requires multiple texture samples and blending
    // We'll create a custom HLSL implementation for now
    pub fn add_triplanar_sample_node(
        &mut self,
        texture: usize,
        world_position: Option<usize>,
        blend_sharpness: f32,
    ) -> usize {
        // Get world position (use provided or create new)
        let world_pos = match world_position {
            Some(pos) => pos,
            None => self.add_world_position_node(),
        };

        // Get world normal
        let world_normal = self.add_world_normal_node();

        // Create triplanar sampling HLSL code
        let code = format!(
            r#"// Triplanar texture sampling
// Calculate blend weights based on world normal
float3 blendWeights = abs(WorldNormal);
blendWeights = pow(blendWeights, {});
blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z);

// Sample texture from each axis
float2 uvX = WorldPosition.zy;
float2 uvY = WorldPosition.xz;
float2 uvZ = WorldPosition.xy;

float4 texX = Texture2DSample(Tex, TexSampler, uvX);
float4 texY = Texture2DSample(Tex, TexSampler, uvY);
float4 texZ = Texture2DSample(Tex, TexSampler, uvZ);

// Blend samples
return texX * blendWeights.x + texY * blendWeights.y + texZ * blendWeights.z;"#,
            blend_sharpness
        );

        // Create custom HLSL node with texture, world position, and world normal inputs
        let inputs = vec![
            ("Tex".to_string(), texture),
            ("WorldPosition".to_string(), world_pos),
            ("WorldNormal".to_string(), world_normal),
        ];

        self.add_custom_hlsl_node(&code, &CustomOutputType::Float4, &inputs)
    }

    // ── Material output connections ────────────────────────────────────────

    pub fn connect_to_base_color(&mut self, node_id: usize) {
        self.output_connections.insert("BaseColor".to_string(), node_id);
    }

    pub fn connect_to_metallic(&mut self, node_id: usize) {
        self.output_connections.insert("Metallic".to_string(), node_id);
    }

    pub fn connect_to_specular(&mut self, node_id: usize) {
        self.output_connections.insert("Specular".to_string(), node_id);
    }

    pub fn connect_to_roughness(&mut self, node_id: usize) {
        self.output_connections.insert("Roughness".to_string(), node_id);
    }

    pub fn connect_to_emissive(&mut self, node_id: usize) {
        self.output_connections
            .insert("EmissiveColor".to_string(), node_id);
    }

    pub fn connect_to_opacity(&mut self, node_id: usize) {
        self.output_connections.insert("Opacity".to_string(), node_id);
    }

    pub fn connect_to_normal(&mut self, node_id: usize) {
        self.output_connections.insert("Normal".to_string(), node_id);
    }

    pub fn connect_to_ambient_occlusion(&mut self, node_id: usize) {
        self.output_connections
            .insert("AmbientOcclusion".to_string(), node_id);
    }

    pub fn connect_to_world_position_offset(&mut self, node_id: usize) {
        self.output_connections
            .insert("WorldPositionOffset".to_string(), node_id);
    }

    // ── Material properties ────────────────────────────────────────────────

    pub fn set_blend_mode(&mut self, mode: &BlendMode) {
        self.blend_mode = match mode {
            BlendMode::Opaque => 0,
            BlendMode::Masked => 1,
            BlendMode::Translucent => 2,
            BlendMode::Additive => 3,
            BlendMode::Modulate => 4,
        };
    }

    pub fn set_shading_model(&mut self, model: &ShadingModel) {
        self.shading_model = match model {
            ShadingModel::Unlit => 0,
            ShadingModel::DefaultLit => 1,
            ShadingModel::Subsurface => 2,
            ShadingModel::PreintegratedSkin => 3,
            ShadingModel::ClearCoat => 4,
            ShadingModel::SubsurfaceProfile => 5,
            ShadingModel::TwoSidedFoliage => 6,
            ShadingModel::Hair => 7,
            ShadingModel::Cloth => 8,
            ShadingModel::Eye => 9,
        };
    }

    pub fn set_two_sided(&mut self, two_sided: bool) {
        self.two_sided = two_sided;
    }

    // ── Feature 7.1: Dynamic parameter exposure ────────────────────────────

    /// Mark a parameter as runtime-modifiable via MaterialInstanceDynamic.
    /// This is a no-op at the .uasset level (parameters are always accessible),
    /// but signals to the C++ wrapper generator that this parameter should be
    /// exposed in the UKainMaterialParameterCollection class.
    pub fn mark_parameter_dynamic(&mut self, _param_name: &str) {
        // No-op: UE5 material parameters are always accessible via MID.
        // This method exists for API consistency with MaterialGraph::mark_parameter_dynamic
        // and to signal intent to the C++ wrapper generator.
    }

    // ── Build: finalize and serialize ──────────────────────────────────────

    /// Finalize the material and serialize to .uasset bytes.
    pub fn build(mut self) -> Result<Vec<u8>, String> {
        // Build the Expressions array property on the material export
        self.finalize_material_export();

        // Rebuild name map to ensure all FNames are registered
        self.asset.rebuild_name_map();

        // depends_map is managed internally by the Asset writer

        // Write to bytes
        let mut output = Cursor::new(Vec::new());
        self.asset
            .write_data(&mut output, None)
            .map_err(|e| format!("Failed to write .uasset: {}", e))?;

        Ok(output.into_inner())
    }

    fn finalize_material_export(&mut self) {
        let mut mat_props: Vec<Property> = Vec::new();

        // Add material output connections as StructProperty wrapping ColorMaterialInput
        // UE5 serializes material outputs as StructProperty with struct_type = "ColorMaterialInput"
        // The StructProperty custom serialization path then delegates to ColorMaterialInputProperty
        let connections: Vec<(String, usize)> =
            self.output_connections.iter().map(|(k, v)| (k.clone(), *v)).collect();

        for (output_name, node_id) in &connections {
            let expr = self.make_expression_ref(*node_id, 0);

            let prop_name = self.asset.add_fname(output_name);
            let struct_type = self.asset.add_fname("ColorMaterialInput");

            let inner = ColorMaterialInputProperty {
                name: prop_name.clone(),
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                material_expression: expr,
                value: Default::default(),
            };

            let wrapper = StructProperty {
                name: prop_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                struct_type: Some(struct_type),
                struct_guid: Some(Default::default()),
                serialize_none: true,
                value: vec![inner.into()],
            };
            mat_props.push(wrapper.into());
        }

        // Build Expressions array (references to all expression exports)
        if !self.node_exports.is_empty() {
            let expr_name = self.asset.add_fname("Expressions");
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
            mat_props.push(arr_prop.into());
        }

        // Blend mode — UE5 serializes as EnumProperty(EBlendMode)
        // Only write if non-default (Opaque = 0)
        if self.blend_mode != 0 {
            let bm_name = self.asset.add_fname("BlendMode");
            let bm_enum_type = self.asset.add_fname("EBlendMode");
            let bm_value_str = blend_mode_fname(self.blend_mode);
            let bm_value = self.asset.add_fname(bm_value_str);
            mat_props.push(
                EnumProperty {
                    name: bm_name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    enum_type: Some(bm_enum_type),
                    inner_type: None,
                    value: Some(bm_value),
                }
                .into(),
            );
        }

        // Shading model — UE5 serializes as EnumProperty(EMaterialShadingModel)
        // Only write if non-default (DefaultLit = 1)
        if self.shading_model != 1 {
            let sm_name = self.asset.add_fname("ShadingModel");
            let sm_enum_type = self.asset.add_fname("EMaterialShadingModel");
            let sm_value_str = shading_model_fname(self.shading_model);
            let sm_value = self.asset.add_fname(sm_value_str);
            mat_props.push(
                EnumProperty {
                    name: sm_name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    enum_type: Some(sm_enum_type),
                    inner_type: None,
                    value: Some(sm_value),
                }
                .into(),
            );
        }

        // Two-sided
        if self.two_sided {
            let ts_name = self.asset.add_fname("TwoSided");
            mat_props.push(
                BoolProperty {
                    name: ts_name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: true,
                }
                .into(),
            );
        }

        // Update the material export's properties
        if let Some(Export::NormalExport(ref mut normal)) =
            self.asset.asset_data.exports.first_mut()
        {
            normal.properties = mat_props;
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 3: Graph conversion — MaterialGraph → MaterialAssetBuilder → bytes
// ---------------------------------------------------------------------------

/// Convert a MaterialGraph IR to .uasset bytes.
///
/// `engine_target` controls the UE version the asset is built for.
/// Pass `KainEngineTarget::default()` if you don't need to target a specific version.
pub fn serialize_material_graph(graph: &MaterialGraph, engine_target: KainEngineTarget) -> Result<Vec<u8>, String> {
    let mut builder = MaterialAssetBuilder::new(&format!("M_{}", graph.name), engine_target);

    // Configure material properties
    builder.set_blend_mode(&graph.properties.blend_mode);
    builder.set_shading_model(&graph.properties.shading_model);
    builder.set_two_sided(graph.properties.two_sided);

    // Map graph node IDs → builder node IDs
    let mut node_map: HashMap<String, usize> = HashMap::new();

    // Process nodes in order (assumes topological sort from ast_converter)
    for node in &graph.nodes {
        let builder_id = convert_node(&mut builder, &node.node_type, &node_map)?;
        node_map.insert(node.id.clone(), builder_id);
    }

    // Connect outputs
    if let Some(ref id) = graph.outputs.base_color {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_base_color(nid);
        }
    }
    if let Some(ref id) = graph.outputs.metallic {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_metallic(nid);
        }
    }
    if let Some(ref id) = graph.outputs.specular {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_specular(nid);
        }
    }
    if let Some(ref id) = graph.outputs.roughness {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_roughness(nid);
        }
    }
    if let Some(ref id) = graph.outputs.emissive {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_emissive(nid);
        }
    }
    if let Some(ref id) = graph.outputs.opacity {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_opacity(nid);
        }
    }
    if let Some(ref id) = graph.outputs.normal {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_normal(nid);
        }
    }
    if let Some(ref id) = graph.outputs.ambient_occlusion {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_ambient_occlusion(nid);
        }
    }
    if let Some(ref id) = graph.outputs.world_position_offset {
        if let Some(&nid) = node_map.get(id) {
            builder.connect_to_world_position_offset(nid);
        }
    }

    builder.build()
}

/// Resolve a node ID reference to a builder node ID.
fn resolve(node_map: &HashMap<String, usize>, id: &str) -> Result<usize, String> {
    node_map
        .get(id)
        .copied()
        .ok_or_else(|| {
            let available: Vec<&str> = node_map.keys().map(|s| s.as_str()).collect();
            format!(
                "Unknown node reference '{}'. Available nodes: [{}]",
                id,
                if available.len() <= 10 {
                    available.join(", ")
                } else {
                    format!("{} ... ({} total)", available[..10].join(", "), available.len())
                }
            )
        })
}

/// Convert a single MaterialNodeType into builder calls.
fn convert_node(
    builder: &mut MaterialAssetBuilder,
    node_type: &MaterialNodeType,
    node_map: &HashMap<String, usize>,
) -> Result<usize, String> {
    match node_type {
        // Constants
        MaterialNodeType::ConstantFloat { value } => Ok(builder.add_constant_node(*value)),
        MaterialNodeType::ConstantVec3 { value } | MaterialNodeType::ConstantVector3 { value } => {
            Ok(builder.add_constant3_node(value[0], value[1], value[2]))
        }
        MaterialNodeType::ConstantVec4 { value } | MaterialNodeType::ConstantVector4 { value } => {
            Ok(builder.add_constant4_node(value[0], value[1], value[2], value[3]))
        }

        // Parameters
        MaterialNodeType::ScalarParameter { name, default } => {
            Ok(builder.add_scalar_parameter_node(name, *default))
        }
        MaterialNodeType::VectorParameter { name, default } => {
            Ok(builder.add_vector_parameter_node(name, *default))
        }
        MaterialNodeType::ColorParameter { name, default } => {
            Ok(builder.add_color_parameter_node(name, *default))
        }

        // Arithmetic
        MaterialNodeType::Add { a, b } => {
            Ok(builder.add_add_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Subtract { a, b } => {
            Ok(builder.add_subtract_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Multiply { a, b } => {
            Ok(builder.add_multiply_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Divide { a, b } => {
            Ok(builder.add_divide_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Dot { a, b } | MaterialNodeType::DotProduct { a, b } => {
            Ok(builder.add_dot_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Cross { a, b } => {
            Ok(builder.add_cross_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Min { a, b } => {
            Ok(builder.add_min_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Max { a, b } => {
            Ok(builder.add_max_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Power { base, exponent } => Ok(builder.add_power_node(
            resolve(node_map, base)?,
            resolve(node_map, exponent)?,
        )),
        MaterialNodeType::Distance { a, b } => {
            Ok(builder.add_distance_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }
        MaterialNodeType::Append { a, b } | MaterialNodeType::AppendVector { a, b } => {
            Ok(builder.add_append_node(resolve(node_map, a)?, resolve(node_map, b)?))
        }

        // Unary
        MaterialNodeType::Normalize { input } => {
            Ok(builder.add_normalize_node(resolve(node_map, input)?))
        }
        MaterialNodeType::Length { input } => {
            Ok(builder.add_length_node(resolve(node_map, input)?))
        }
        MaterialNodeType::Abs { input } => Ok(builder.add_abs_node(resolve(node_map, input)?)),
        MaterialNodeType::Saturate { input } => {
            Ok(builder.add_saturate_node(resolve(node_map, input)?))
        }
        MaterialNodeType::Frac { input } => Ok(builder.add_frac_node(resolve(node_map, input)?)),
        MaterialNodeType::Floor { input } => {
            Ok(builder.add_floor_node(resolve(node_map, input)?))
        }
        MaterialNodeType::Ceil { input } => Ok(builder.add_ceil_node(resolve(node_map, input)?)),
        MaterialNodeType::Round { input } => {
            Ok(builder.add_round_node(resolve(node_map, input)?))
        }
        MaterialNodeType::Sqrt { input } => Ok(builder.add_sqrt_node(resolve(node_map, input)?)),
        MaterialNodeType::Exp { input } => {
            // UE5 doesn't have a direct Exp node; use Custom HLSL
            let node = resolve(node_map, input)?;
            Ok(builder.add_custom_hlsl_node(
                "return exp(Input);",
                &CustomOutputType::Float1,
                &[("Input".to_string(), node)],
            ))
        }
        MaterialNodeType::Log { input } => {
            let node = resolve(node_map, input)?;
            Ok(builder.add_custom_hlsl_node(
                "return log(Input);",
                &CustomOutputType::Float1,
                &[("Input".to_string(), node)],
            ))
        }
        MaterialNodeType::Sine { input } => Ok(builder.add_sine_node(resolve(node_map, input)?)),
        MaterialNodeType::Cosine { input } => {
            Ok(builder.add_cosine_node(resolve(node_map, input)?))
        }

        // 3-input
        MaterialNodeType::Lerp { a, b, alpha } => Ok(builder.add_lerp_node(
            resolve(node_map, a)?,
            resolve(node_map, b)?,
            resolve(node_map, alpha)?,
        )),
        MaterialNodeType::Clamp { input, min, max } => Ok(builder.add_clamp_node(
            resolve(node_map, input)?,
            resolve(node_map, min)?,
            resolve(node_map, max)?,
        )),

        // Textures
        MaterialNodeType::TextureSample { texture_input, uv_input } => {
            let uv = uv_input
                .as_ref()
                .and_then(|id| node_map.get(id))
                .copied();
            let node_id = builder.add_texture_sample_node(uv);
            // Wire texture_input if provided (otherwise the node has no texture set)
            if let Some(tex_id) = texture_input {
                if let Some(&tex_node) = node_map.get(tex_id) {
                    // The texture input is wired via the Texture object property
                    // on the expression export. This is handled by the builder
                    // through an input property on the "Texture" pin.
                    let _ = tex_node; // TODO: Wire texture object reference when add_texture_sample_node supports it
                }
            }
            Ok(node_id)
        }
        MaterialNodeType::TextureSampleParameter2D {
            param_name,
            uv_input,
            ..
        } => {
            let uv = uv_input
                .as_ref()
                .and_then(|id| node_map.get(id))
                .copied();
            Ok(builder.add_texture_sample_parameter(param_name, uv))
        }
        MaterialNodeType::TextureCoordinate { index, .. } => {
            Ok(builder.add_texture_coordinate_node(*index))
        }
        MaterialNodeType::ComponentMask { input, mask } => {
            let node = resolve(node_map, input)?;
            let (r, g, b, a) = parse_mask(mask);
            Ok(builder.add_component_mask_node(node, r, g, b, a))
        }

        // Fresnel
        MaterialNodeType::Fresnel {
            exponent,
            base_reflect_fraction,
        } => Ok(builder.add_fresnel_node(
            resolve(node_map, exponent)?,
            resolve(node_map, base_reflect_fraction)?,
        )),

        // Time
        MaterialNodeType::Time => Ok(builder.add_time_node()),

        // UV manipulation
        MaterialNodeType::UVScroll {
            uv_input,
            offset_x,
            offset_y,
        } => {
            let uv = resolve(node_map, uv_input)?;
            // offset_x/y are node IDs referencing speed value nodes.
            // To produce animated scrolling (like MaterialExpressionPanner),
            // multiply each speed by Time, append into a float2, and add to UV.
            let ox = resolve(node_map, offset_x)?;
            let oy = resolve(node_map, offset_y)?;
            let time = builder.add_time_node();
            let scroll_x = builder.add_multiply_node(ox, time);
            let scroll_y = builder.add_multiply_node(oy, time);
            let scroll_vec = builder.add_append_node(scroll_x, scroll_y);
            Ok(builder.add_add_node(uv, scroll_vec))
        }
        MaterialNodeType::UVScale {
            uv_input,
            scale_x,
            scale_y,
        } => {
            let uv = resolve(node_map, uv_input)?;
            // scale_x/y are node IDs referencing scale value nodes
            let sx = resolve(node_map, scale_x)?;
            let sy = resolve(node_map, scale_y)?;
            let scale_vec = builder.add_append_node(sx, sy);
            Ok(builder.add_multiply_node(uv, scale_vec))
        }
        MaterialNodeType::UVRotate {
            uv_input,
            angle,
            center,
        } => {
            let uv = resolve(node_map, uv_input)?;
            let angle_node = node_map.get(angle.as_str()).copied();
            let center_node = center.as_ref().and_then(|(cx, _)| node_map.get(cx.as_str()).copied());
            Ok(builder.add_rotator_node(uv, center_node, angle_node))
        }

        // Shader integration
        MaterialNodeType::MaterialFunctionCall {
            function_path,
            inputs,
        } => {
            let resolved_inputs: Vec<(String, usize)> = inputs
                .iter()
                .enumerate()
                .map(|(i, id)| Ok((format!("Input_{}", i), resolve(node_map, id)?)))
                .collect::<Result<Vec<_>, String>>()?;
            Ok(builder.add_material_function_call(function_path, &resolved_inputs))
        }

        // Custom HLSL
        MaterialNodeType::CustomHLSL {
            code,
            output_type,
            inputs,
        } => {
            // CustomInput has name + input_type but no node_id field.
            // Inputs are positional — wire them to any nodes that share
            // the same name in the node_map (best-effort resolution).
            let resolved: Vec<(String, usize)> = inputs
                .iter()
                .map(|ci| {
                    let node_id = node_map.get(&ci.name).copied()
                        .ok_or_else(|| format!(
                            "CustomHLSL input '{}' references unknown node — \
                             ensure the input name matches a declared node or variable ID",
                            ci.name
                        ))?;
                    Ok((ci.name.clone(), node_id))
                })
                .collect::<Result<Vec<_>, String>>()?;
            Ok(builder.add_custom_hlsl_node(code, output_type, &resolved))
        }

        // World-Space Operations (Phase 7.4)
        MaterialNodeType::WorldPosition => Ok(builder.add_world_position_node()),
        MaterialNodeType::WorldNormal => Ok(builder.add_world_normal_node()),
        MaterialNodeType::AbsoluteWorldPosition => Ok(builder.add_absolute_world_position_node()),
        MaterialNodeType::CameraPosition => Ok(builder.add_camera_position_node()),
        MaterialNodeType::ObjectPosition => Ok(builder.add_object_position_node()),
        MaterialNodeType::ObjectOrientation => Ok(builder.add_object_orientation_node()),
        MaterialNodeType::TriplanarSample {
            texture,
            world_position,
            blend_sharpness,
        } => {
            let tex_node = resolve(node_map, texture)?;
            let world_pos = world_position
                .as_ref()
                .map(|id| resolve(node_map, id))
                .transpose()?;
            Ok(builder.add_triplanar_sample_node(tex_node, world_pos, *blend_sharpness))
        }

        // Material Layers (Phase 7.3)
        MaterialNodeType::MaterialLayer {
            base_layer,
            blend_layer,
            blend_mode,
            alpha,
        } => {
            let base = resolve(node_map, base_layer)?;
            let blend = resolve(node_map, blend_layer)?;
            let alpha_node = resolve(node_map, alpha)?;
            Ok(builder.add_material_layer_node(base, blend, blend_mode, alpha_node))
        }
        MaterialNodeType::MaterialLayerBlend {
            layers,
            blend_modes,
            alphas,
        } => {
            let layer_nodes: Result<Vec<usize>, String> = layers
                .iter()
                .map(|id| resolve(node_map, id))
                .collect();
            let alpha_nodes: Result<Vec<usize>, String> = alphas
                .iter()
                .map(|id| resolve(node_map, id))
                .collect();
            Ok(builder.add_material_layer_blend_node(
                &layer_nodes?,
                blend_modes,
                &alpha_nodes?,
            ))
        }
    }
}

/// Map a BlendMode integer to the exact UE5 EBlendMode FName string.
/// Values confirmed from UE5.4/5.7 EngineTypes.h.
fn blend_mode_fname(mode: u8) -> &'static str {
    match mode {
        0 => "BLEND_Opaque",
        1 => "BLEND_Masked",
        2 => "BLEND_Translucent",
        3 => "BLEND_Additive",
        4 => "BLEND_Modulate",
        5 => "BLEND_AlphaComposite",
        6 => "BLEND_AlphaHoldout",
        _ => "BLEND_Opaque",
    }
}

/// Map a ShadingModel integer to the exact UE5 EMaterialShadingModel FName string.
/// Values confirmed from UE5.4/5.7 EngineTypes.h.
fn shading_model_fname(model: u8) -> &'static str {
    match model {
        0 => "MSM_Unlit",
        1 => "MSM_DefaultLit",
        2 => "MSM_Subsurface",
        3 => "MSM_PreintegratedSkin",
        4 => "MSM_ClearCoat",
        5 => "MSM_SubsurfaceProfile",
        6 => "MSM_TwoSidedFoliage",
        7 => "MSM_Hair",
        8 => "MSM_Cloth",
        9 => "MSM_Eye",
        10 => "MSM_SingleLayerWater",
        11 => "MSM_ThinTranslucent",
        _ => "MSM_DefaultLit",
    }
}

/// Parse a mask string like "rgb", "r", "rg", "rgba" into boolean flags.
fn parse_mask(mask: &str) -> (bool, bool, bool, bool) {
    let m = mask.to_lowercase();
    (
        m.contains('r'),
        m.contains('g'),
        m.contains('b'),
        m.contains('a'),
    )
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_constant_material() {
        let mut builder = MaterialAssetBuilder::new("M_TestConstant", KainEngineTarget::default());
        let c = builder.add_constant3_node(1.0, 0.0, 0.0);
        builder.connect_to_base_color(c);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty(), "output should not be empty");
        // Verify magic number
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_add_node_material() {
        let mut builder = MaterialAssetBuilder::new("M_TestAdd", KainEngineTarget::default());
        let c1 = builder.add_constant_node(0.5);
        let c2 = builder.add_constant_node(0.3);
        let add = builder.add_add_node(c1, c2);
        builder.connect_to_base_color(add);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_complex_material() {
        let mut builder = MaterialAssetBuilder::new("M_Complex", KainEngineTarget::default());

        let tex = builder.add_texture_sample_parameter("Albedo", None);
        let roughness = builder.add_scalar_parameter_node("Roughness", 0.8);
        let metallic = builder.add_constant_node(0.0);
        let tint = builder.add_vector_parameter_node("Tint", [1.0, 0.8, 0.6]);
        let tinted = builder.add_multiply_node(tex, tint);

        builder.connect_to_base_color(tinted);
        builder.connect_to_roughness(roughness);
        builder.connect_to_metallic(metallic);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_graph_conversion() {
        let mut graph = MaterialGraph::new("TestShader".to_string());

        graph.nodes.push(MaterialNode {
            id: "c1".to_string(),
            node_type: MaterialNodeType::ConstantFloat { value: 0.5 },
            position: (0, 0),
        });
        graph.nodes.push(MaterialNode {
            id: "c2".to_string(),
            node_type: MaterialNodeType::ConstantFloat { value: 0.3 },
            position: (0, 150),
        });
        graph.nodes.push(MaterialNode {
            id: "add".to_string(),
            node_type: MaterialNodeType::Add {
                a: "c1".to_string(),
                b: "c2".to_string(),
            },
            position: (200, 75),
        });

        graph.outputs.base_color = Some("add".to_string());

        let bytes = serialize_material_graph(&graph, KainEngineTarget::default()).expect("serialization should succeed");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);
    }

    #[test]
    fn test_all_node_types() {
        let mut builder = MaterialAssetBuilder::new("M_AllNodes", KainEngineTarget::default());

        // Constants
        let c1 = builder.add_constant_node(1.0);
        let c2 = builder.add_constant_node(0.5);
        let v3 = builder.add_constant3_node(1.0, 0.0, 0.0);
        let v4 = builder.add_constant4_node(1.0, 0.0, 0.0, 1.0);

        // Arithmetic
        let add = builder.add_add_node(c1, c2);
        let sub = builder.add_subtract_node(c1, c2);
        let mul = builder.add_multiply_node(c1, c2);
        let div = builder.add_divide_node(c1, c2);
        let dot = builder.add_dot_node(v3, v3);
        let pow = builder.add_power_node(c1, c2);
        let lerp = builder.add_lerp_node(c1, c2, c2);
        let clamp = builder.add_clamp_node(c1, c2, c2);
        let min = builder.add_min_node(c1, c2);
        let max = builder.add_max_node(c1, c2);

        // Unary
        let norm = builder.add_normalize_node(v3);
        let len = builder.add_length_node(v3);
        let abs = builder.add_abs_node(c1);
        let sat = builder.add_saturate_node(c1);
        let frac = builder.add_frac_node(c1);
        let floor = builder.add_floor_node(c1);
        let ceil = builder.add_ceil_node(c1);
        let sqrt = builder.add_sqrt_node(c1);
        let sine = builder.add_sine_node(c1);
        let cos = builder.add_cosine_node(c1);

        // Texture
        let uv = builder.add_texture_coordinate_node(0);
        let tex = builder.add_texture_sample_parameter("Albedo", Some(uv));
        let mask = builder.add_component_mask_node(tex, true, true, true, false);

        // Parameters
        let sp = builder.add_scalar_parameter_node("MyScalar", 0.5);
        let vp = builder.add_vector_parameter_node("MyVector", [1.0, 0.0, 0.0]);

        // Time
        let time = builder.add_time_node();

        builder.connect_to_base_color(mask);
        builder.connect_to_roughness(sp);

        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_simple_layer_blend() {
        let mut builder = MaterialAssetBuilder::new("M_LayerBlend", KainEngineTarget::default());
        
        // Create two base colors to blend
        let base = builder.add_constant3_node(1.0, 0.0, 0.0); // Red
        let overlay = builder.add_constant3_node(0.0, 0.0, 1.0); // Blue
        let alpha = builder.add_constant_node(0.5); // 50% blend
        
        // Blend using lerp mode
        let blended = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Lerp, alpha);
        builder.connect_to_base_color(blended);
        
        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_multiple_layer_stack() {
        let mut builder = MaterialAssetBuilder::new("M_MultiLayer", KainEngineTarget::default());
        
        // Create three layers
        let layer1 = builder.add_constant3_node(1.0, 0.0, 0.0); // Red base
        let layer2 = builder.add_constant3_node(0.0, 1.0, 0.0); // Green overlay
        let layer3 = builder.add_constant3_node(0.0, 0.0, 1.0); // Blue top
        
        let alpha1 = builder.add_constant_node(0.5);
        let alpha2 = builder.add_constant_node(0.3);
        
        // Blend all three layers
        let blended = builder.add_material_layer_blend_node(
            &[layer1, layer2, layer3],
            &[LayerBlendMode::Lerp, LayerBlendMode::Add],
            &[alpha1, alpha2],
        );
        
        builder.connect_to_base_color(blended);
        
        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_layer_alpha_control() {
        let mut builder = MaterialAssetBuilder::new("M_DynamicLayer", KainEngineTarget::default());
        
        // Create base and overlay
        let base = builder.add_constant3_node(0.2, 0.2, 0.2); // Dark gray
        let overlay = builder.add_constant3_node(1.0, 1.0, 1.0); // White
        
        // Use a parameter for dynamic alpha control
        let alpha = builder.add_scalar_parameter_node("BlendAmount", 0.5);
        
        // Test different blend modes
        let lerp_blend = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Lerp, alpha);
        let add_blend = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Add, alpha);
        let multiply_blend = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Multiply, alpha);
        
        // Use the lerp blend for output
        builder.connect_to_base_color(lerp_blend);
        
        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }

    #[test]
    fn test_all_blend_modes() {
        let mut builder = MaterialAssetBuilder::new("M_AllBlendModes", KainEngineTarget::default());
        
        let base = builder.add_constant3_node(0.5, 0.5, 0.5);
        let overlay = builder.add_constant3_node(0.8, 0.2, 0.3);
        let alpha = builder.add_constant_node(0.7);
        
        // Test all blend modes compile
        let _lerp = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Lerp, alpha);
        let _add = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Add, alpha);
        let _multiply = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Multiply, alpha);
        let _overlay = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Overlay, alpha);
        let screen = builder.add_material_layer_node(base, overlay, &LayerBlendMode::Screen, alpha);
        
        builder.connect_to_base_color(screen);
        
        let bytes = builder.build().expect("build should succeed");
        assert!(!bytes.is_empty());
    }
}
