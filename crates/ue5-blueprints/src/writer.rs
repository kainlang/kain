/// Binary .uasset writer for Blueprint assets.
///
/// Phase 2: Uses `unreal_asset` to write real .uasset binaries that land in the
/// Content folder without ever opening the UE5 editor.
///
/// Architecture:
///   BlueprintDef IR
///       → BlueprintBinaryWriter::write()
///       → Asset<Cursor<Vec<u8>>>  (unreal_asset)
///       → Vec<u8>  (raw .uasset bytes)
///       → written to Content/Blueprints/BP_*.uasset

use std::io::Cursor;

use unreal_asset::{
    engine_version::EngineVersion,
    exports::{
        ExportBaseTrait, ExportNormalTrait, Export,
        normal_export::NormalExport,
        base_export::BaseExport,
        class_export::ClassExport,
        struct_export::StructExport,
    },
    flags::{EObjectFlags, EClassFlags},
    types::PackageIndex,
    containers::IndexedMap,
    Asset, Import,
};
use unreal_asset_properties::{
    object_property::ObjectProperty,
    int_property::IntProperty,
    str_property::NameProperty,
    array_property::ArrayProperty,
    Property,
};
use ue5_asset_utils::{ImportBuilder, PropertyDef, PropertyValue};
use ue5_asset_utils::property_converter::convert_property_defs;

use crate::{
    error::{BlueprintError, Result},
    ir::{BlueprintDef, BlueprintEngineVersion, ComponentDef},
    kismet,
};

// ---------------------------------------------------------------------------
// BlueprintBinaryWriter — writes real .uasset bytes for Blueprint assets
// ---------------------------------------------------------------------------

pub struct BlueprintBinaryWriter;

impl BlueprintBinaryWriter {
    /// Write a Blueprint .uasset to a byte buffer.
    ///
    /// Returns the raw .uasset bytes ready to be written to disk.
    /// Path: `<project>/Content/<package_path>/<name>.uasset`
    pub fn write(bp: &BlueprintDef) -> Result<Vec<u8>> {
        let engine_version = map_engine_version(bp.engine_version);
        let mut ctx = BlueprintBuildContext::new(&bp.name, &bp.parent_class, engine_version)?;

        // 1. Add component class imports (one per unique class)
        let mut comp_class_imports: Vec<PackageIndex> = Vec::new();
        for comp in &bp.components {
            let idx = ctx.add_component_class_import(&comp.class_name);
            comp_class_imports.push(idx);
        }

        // 2. Add SCS export + SCS_Node exports + ComponentTemplate exports
        if !bp.components.is_empty() {
            ctx.add_scs_exports(&bp.components, &comp_class_imports);
        }

        // 3. Emit kismet bytecode for event graph (UFunction exports)
        ctx.add_event_graph(&bp.name, &bp.event_graph);

        // 4. Finalize the Blueprint export (export 0) with SCS + Expressions refs
        ctx.finalize_blueprint_export(bp);

        // 5. Set CDO default properties (export index stored in ctx.cdo_export)
        ctx.set_cdo_defaults(&bp.defaults);

        // 6. Serialize
        ctx.build()
    }

    /// Check if the binary writer can handle a given blueprint definition.
    /// Returns Ok(()) if supported, Err with explanation if not yet implemented.
    pub fn check_support(_bp: &BlueprintDef) -> Result<()> {
        // Kismet bytecode emitter is now implemented — all blueprints are supported.
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Internal build context — tracks indices while constructing the asset
// ---------------------------------------------------------------------------

struct BlueprintBuildContext {
    asset: Asset<Cursor<Vec<u8>>>,

    // Import indices (negative PackageIndex)
    _engine_import: PackageIndex,
    _coreuobject_import: PackageIndex,
    _blueprint_class_import: PackageIndex,
    _bp_gen_class_import: PackageIndex,
    scs_class_import: PackageIndex,
    scs_node_class_import: PackageIndex,
    parent_class_import: PackageIndex,
    function_class_import: PackageIndex,

    // Export indices (positive PackageIndex, 1-based)
    blueprint_export: PackageIndex,       // The UBlueprint root object
    bp_gen_class_export: PackageIndex,    // The UBlueprintGeneratedClass
    cdo_export: PackageIndex,             // Default__BP_Name (ClassDefaultObject)
    scs_export: Option<PackageIndex>,     // SimpleConstructionScript (if components exist)
    scs_node_exports: Vec<PackageIndex>,  // SCS_Node exports
    comp_template_exports: Vec<PackageIndex>, // Component template exports
    function_exports: Vec<PackageIndex>,  // UFunction exports (event graph)
}

impl BlueprintBuildContext {
    fn new(name: &str, parent_class: &str, engine_version: EngineVersion) -> Result<Self> {
        let mut asset = Asset::new_empty(engine_version);

        // ── Core imports ─────────────────────────────────────────────────────
        // Import table uses negative indices. The library manages them via
        // asset.imports and PackageIndex::new(-(import_vec_index + 1)).

        // /Script/CoreUObject
        let coreuobject_pkg_name = asset.add_fname("/Script/CoreUObject");
        let package_fname = asset.add_fname("Package");
        asset.imports.push(Import {
            class_package: coreuobject_pkg_name.clone(),
            class_name: package_fname.clone(),
            outer_index: PackageIndex::new(0),
            object_name: coreuobject_pkg_name.clone(),
            optional: false,
        });
        let coreuobject_import = PackageIndex::new(-(asset.imports.len() as i32));

        // /Script/Engine
        let engine_pkg_name = asset.add_fname("/Script/Engine");
        asset.imports.push(Import {
            class_package: coreuobject_pkg_name.clone(),
            class_name: package_fname.clone(),
            outer_index: PackageIndex::new(0),
            object_name: engine_pkg_name.clone(),
            optional: false,
        });
        let engine_import = PackageIndex::new(-(asset.imports.len() as i32));

        // Class import helper
        let class_fname = asset.add_fname("Class");
        let add_class_import = |asset: &mut Asset<Cursor<Vec<u8>>>, class_name: &str, outer: PackageIndex| -> PackageIndex {
            let cname = asset.add_fname(class_name);
            asset.imports.push(Import {
                class_package: coreuobject_pkg_name.clone(),
                class_name: class_fname.clone(),
                outer_index: outer,
                object_name: cname,
                optional: false,
            });
            PackageIndex::new(-(asset.imports.len() as i32))
        };

        // Blueprint class
        let blueprint_class_import = add_class_import(&mut asset, "Blueprint", engine_import);
        // BlueprintGeneratedClass
        let bp_gen_class_import = add_class_import(&mut asset, "BlueprintGeneratedClass", engine_import);
        // SimpleConstructionScript
        let scs_class_import = add_class_import(&mut asset, "SimpleConstructionScript", engine_import);
        // SCS_Node
        let scs_node_class_import = add_class_import(&mut asset, "SCS_Node", engine_import);
        // Function (for UFunction exports — event graph bytecode)
        let function_class_import = add_class_import(&mut asset, "Function", coreuobject_import);

        // ── Parent class import ──────────────────────────────────────────────
        // Parse "/Script/Engine.Actor" → package="/Script/Engine", class="Actor"
        let (parent_pkg_path, parent_class_name) = ImportBuilder::parse_class_path(parent_class);

        // Package import for parent (may reuse engine_import)
        let parent_pkg_import = if parent_pkg_path == "/Script/Engine" {
            engine_import
        } else {
            let ppkg = asset.add_fname(&parent_pkg_path);
            asset.imports.push(Import {
                class_package: coreuobject_pkg_name.clone(),
                class_name: package_fname.clone(),
                outer_index: PackageIndex::new(0),
                object_name: ppkg,
                optional: false,
            });
            PackageIndex::new(-(asset.imports.len() as i32))
        };

        let parent_class_import = add_class_import(&mut asset, &parent_class_name, parent_pkg_import);

        // ── Core exports ─────────────────────────────────────────────────────

        // Export 0: UBlueprint root object
        let bp_obj_name = asset.add_fname(name);
        let bp_export = NormalExport {
            base_export: BaseExport {
                class_index: blueprint_class_import,
                super_index: PackageIndex::new(0),
                template_index: PackageIndex::new(0),
                outer_index: PackageIndex::new(0),
                object_name: bp_obj_name,
                object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
                ..Default::default()
            },
            extras: Vec::new(),
            properties: Vec::new(), // filled in finalize_blueprint_export
        };
        asset.asset_data.exports.push(Export::NormalExport(bp_export));
        let blueprint_export = PackageIndex::new(asset.asset_data.exports.len() as i32);

        // Export 1: UBlueprintGeneratedClass (Name_C) — full ClassExport
        let gen_class_name = asset.add_fname(&format!("{}_C", name));
        let engine_fname = asset.add_fname("Engine");
        let gen_class_export_data = ClassExport {
            struct_export: StructExport {
                normal_export: NormalExport {
                    base_export: BaseExport {
                        class_index: bp_gen_class_import,
                        super_index: parent_class_import,
                        template_index: PackageIndex::new(0),
                        outer_index: PackageIndex::new(0),
                        object_name: gen_class_name,
                        object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
                        ..Default::default()
                    },
                    extras: Vec::new(),
                    properties: Vec::new(), // UberGraphFunction ref set in finalize
                },
                field: Default::default(),
                super_struct: parent_class_import,
                children: Vec::new(),         // populated in finalize after functions added
                loaded_properties: Vec::new(),
                script_bytecode: Some(Vec::new()), // no class-level bytecode
                script_bytecode_size: 0,
                script_bytecode_raw: None,
            },
            func_map: IndexedMap::new(),      // populated in finalize after functions added
            class_flags: EClassFlags::CLASS_NONE,
            class_within: PackageIndex::new(0), // UObject (no restriction)
            class_config_name: engine_fname,
            interfaces: Vec::new(),
            class_generated_by: PackageIndex::new(0),
            deprecated_force_script_order: false,
            cooked: Some(true),
            class_default_object: PackageIndex::new(0), // set in finalize after CDO created
        };
        asset.asset_data.exports.push(Export::ClassExport(gen_class_export_data));
        let bp_gen_class_export = PackageIndex::new(asset.asset_data.exports.len() as i32);

        // Export 2: ClassDefaultObject (Default__Name)
        let cdo_name = asset.add_fname(&format!("Default__{}", name));
        let cdo_export_data = NormalExport {
            base_export: BaseExport {
                class_index: bp_gen_class_export,
                super_index: PackageIndex::new(0),
                template_index: PackageIndex::new(0),
                outer_index: PackageIndex::new(0),
                object_name: cdo_name,
                object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE
                    | EObjectFlags::RF_ARCHETYPE_OBJECT | EObjectFlags::RF_DEFAULT_SUB_OBJECT,
                ..Default::default()
            },
            extras: Vec::new(),
            properties: Vec::new(), // filled in set_cdo_defaults
        };
        asset.asset_data.exports.push(Export::NormalExport(cdo_export_data));
        let cdo_export = PackageIndex::new(asset.asset_data.exports.len() as i32);

        Ok(BlueprintBuildContext {
            asset,
            _engine_import: engine_import,
            _coreuobject_import: coreuobject_import,
            _blueprint_class_import: blueprint_class_import,
            _bp_gen_class_import: bp_gen_class_import,
            scs_class_import,
            scs_node_class_import,
            parent_class_import,
            function_class_import,
            blueprint_export,
            bp_gen_class_export,
            cdo_export,
            scs_export: None,
            scs_node_exports: Vec::new(),
            comp_template_exports: Vec::new(),
            function_exports: Vec::new(),
        })
    }

    // ── Component class imports ──────────────────────────────────────────────

    fn add_component_class_import(&mut self, class_name: &str) -> PackageIndex {
        // Components live in /Script/Engine (UStaticMeshComponent, etc.)
        // For now, assume all component classes are in /Script/Engine.
        let coreuobject_pkg = self.asset.add_fname("/Script/CoreUObject");
        let class_fname = self.asset.add_fname("Class");
        let engine_pkg = self.asset.add_fname("/Script/Engine");
        let package_fname = self.asset.add_fname("Package");

        // Find or create the /Script/Engine package import
        let engine_pkg_idx = ImportBuilder::find_import_by_name(&self.asset, "/Script/Engine")
            .unwrap_or_else(|| {
                self.asset.imports.push(Import {
                    class_package: coreuobject_pkg.clone(),
                    class_name: package_fname,
                    outer_index: PackageIndex::new(0),
                    object_name: engine_pkg,
                    optional: false,
                });
                PackageIndex::new(-(self.asset.imports.len() as i32))
            });

        // Add the component class import
        let comp_class_name = self.asset.add_fname(class_name);
        self.asset.imports.push(Import {
            class_package: coreuobject_pkg,
            class_name: class_fname,
            outer_index: engine_pkg_idx,
            object_name: comp_class_name,
            optional: false,
        });
        PackageIndex::new(-(self.asset.imports.len() as i32))
    }

    // ── SCS + Component exports ──────────────────────────────────────────────

    fn add_scs_exports(&mut self, components: &[ComponentDef], comp_class_imports: &[PackageIndex]) {
        // SimpleConstructionScript export
        let scs_name = self.asset.add_fname("SimpleConstructionScript");
        let scs_export_data = NormalExport {
            base_export: BaseExport {
                class_index: self.scs_class_import,
                super_index: PackageIndex::new(0),
                template_index: PackageIndex::new(0),
                outer_index: self.blueprint_export,
                object_name: scs_name,
                object_flags: EObjectFlags::RF_DEFAULT_SUB_OBJECT,
                ..Default::default()
            },
            extras: Vec::new(),
            properties: Vec::new(), // AllNodes filled after SCS_Nodes are created
        };
        self.asset.asset_data.exports.push(Export::NormalExport(scs_export_data));
        let scs_idx = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
        self.scs_export = Some(scs_idx);

        // For each component: SCS_Node + ComponentTemplate
        for (i, comp) in components.iter().enumerate() {
            // SCS_Node export
            let node_name = self.asset.add_fname(&format!("SCS_Node_{}", i));
            let scs_node_export = NormalExport {
                base_export: BaseExport {
                    class_index: self.scs_node_class_import,
                    super_index: PackageIndex::new(0),
                    template_index: PackageIndex::new(0),
                    outer_index: scs_idx,
                    object_name: node_name,
                    object_flags: EObjectFlags::RF_DEFAULT_SUB_OBJECT,
                    ..Default::default()
                },
                extras: Vec::new(),
                properties: Vec::new(), // filled below
            };
            self.asset.asset_data.exports.push(Export::NormalExport(scs_node_export));
            let scs_node_idx = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
            self.scs_node_exports.push(scs_node_idx);

            // ComponentTemplate export (the actual component instance with property defaults)
            let template_name = self.asset.add_fname(&comp.variable_name);
            let comp_template_export = NormalExport {
                base_export: BaseExport {
                    class_index: comp_class_imports[i],
                    super_index: PackageIndex::new(0),
                    template_index: PackageIndex::new(0),
                    outer_index: self.cdo_export,
                    object_name: template_name,
                    object_flags: EObjectFlags::RF_DEFAULT_SUB_OBJECT,
                    ..Default::default()
                },
                extras: Vec::new(),
                properties: Vec::new(), // component defaults filled below
            };
            self.asset.asset_data.exports.push(Export::NormalExport(comp_template_export));
            let comp_template_idx = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
            self.comp_template_exports.push(comp_template_idx);

            // Wire SCS_Node properties: ComponentClass + ComponentTemplate + InternalVariableName
            let mut node_props: Vec<Property> = Vec::new();

            // ComponentClass = import ref to the component UClass
            let cc_name = self.asset.add_fname("ComponentClass");
            node_props.push(ObjectProperty {
                name: cc_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: comp_class_imports[i],
            }.into());

            // ComponentTemplate = export ref to the template
            let ct_name = self.asset.add_fname("ComponentTemplate");
            node_props.push(ObjectProperty {
                name: ct_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: comp_template_idx,
            }.into());

            // InternalVariableName
            let ivn_name = self.asset.add_fname("InternalVariableName");
            let var_fname = self.asset.add_fname(&comp.variable_name);
            node_props.push(NameProperty {
                name: ivn_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: var_fname,
            }.into());

            // Set SCS_Node properties
            let scs_node_export_idx = (scs_node_idx.index - 1) as usize;
            if let Some(normal) = self.asset.asset_data.exports[scs_node_export_idx].get_normal_export_mut() {
                normal.properties = node_props;
            }

            // Set ComponentTemplate default properties
            if !comp.defaults.is_empty() {
                let comp_props = convert_property_defs(&mut self.asset, &comp.defaults);
                let comp_export_idx = (comp_template_idx.index - 1) as usize;
                if let Some(normal) = self.asset.asset_data.exports[comp_export_idx].get_normal_export_mut() {
                    normal.properties = comp_props;
                }
            }
        }

        // Wire SCS AllNodes array
        let all_nodes_name = self.asset.add_fname("AllNodes");
        let obj_prop_type = self.asset.add_fname("ObjectProperty");
        let node_refs: Vec<Property> = self.scs_node_exports.iter().map(|&idx| {
            ObjectProperty {
                name: all_nodes_name.clone(),
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: idx,
            }.into()
        }).collect();

        let all_nodes_prop = ArrayProperty {
            name: all_nodes_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            array_type: Some(obj_prop_type),
            value: node_refs,
            dummy_property: None,
        };

        // Also need RootNodes (nodes without a parent)
        let root_nodes_name = self.asset.add_fname("RootNodes");
        let obj_prop_type2 = self.asset.add_fname("ObjectProperty");
        let root_refs: Vec<Property> = components.iter().enumerate()
            .filter(|(_, c)| c.attach_parent.is_none())
            .map(|(i, _)| {
                ObjectProperty {
                    name: root_nodes_name.clone(),
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: self.scs_node_exports[i],
                }.into()
            })
            .collect();

        let root_nodes_prop = ArrayProperty {
            name: root_nodes_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            array_type: Some(obj_prop_type2),
            value: root_refs,
            dummy_property: None,
        };

        // Set SCS export properties
        let scs_export_idx = (scs_idx.index - 1) as usize;
        if let Some(normal) = self.asset.asset_data.exports[scs_export_idx].get_normal_export_mut() {
            normal.properties = vec![all_nodes_prop.into(), root_nodes_prop.into()];
        }

        // Wire ChildNodes for parented components
        for (i, comp) in components.iter().enumerate() {
            if let Some(ref parent_name) = comp.attach_parent {
                // Find the parent SCS_Node index
                if let Some(parent_idx) = components.iter().position(|c| c.variable_name == *parent_name) {
                    let child_nodes_name = self.asset.add_fname("ChildNodes");
                    let obj_type = self.asset.add_fname("ObjectProperty");
                    let child_ref = ObjectProperty {
                        name: child_nodes_name.clone(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        value: self.scs_node_exports[i],
                    };
                    let child_arr = ArrayProperty {
                        name: child_nodes_name,
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        array_type: Some(obj_type),
                        value: vec![child_ref.into()],
                        dummy_property: None,
                    };

                    let parent_export_idx = (self.scs_node_exports[parent_idx].index - 1) as usize;
                    if let Some(normal) = self.asset.asset_data.exports[parent_export_idx].get_normal_export_mut() {
                        normal.properties.push(child_arr.into());
                    }
                }
            }
        }
    }

    // ── Event graph (Kismet bytecode) ──────────────────────────────────────

    fn add_event_graph(&mut self, bp_name: &str, events: &[crate::ir::EventGraphNode]) {
        if events.is_empty() {
            return;
        }

        let emit_result = match kismet::emit_event_graph(
            &mut self.asset,
            bp_name,
            events,
            self.bp_gen_class_export,
            self.function_class_import,
        ) {
            Some(r) => r,
            None => return,
        };

        // Append all function exports and track their PackageIndex values
        for func_export in emit_result.function_exports {
            self.asset.asset_data.exports.push(func_export);
            let idx = PackageIndex::new(self.asset.asset_data.exports.len() as i32);
            self.function_exports.push(idx);
        }
    }

    // ── Blueprint export finalization ────────────────────────────────────────

    fn finalize_blueprint_export(&mut self, _bp: &BlueprintDef) {
        let mut props: Vec<Property> = Vec::new();

        // ParentClass
        let parent_name = self.asset.add_fname("ParentClass");
        props.push(ObjectProperty {
            name: parent_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: self.parent_class_import,
        }.into());

        // GeneratedClass
        let gen_class_name = self.asset.add_fname("GeneratedClass");
        props.push(ObjectProperty {
            name: gen_class_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: self.bp_gen_class_export,
        }.into());

        // SimpleConstructionScript (if we have components)
        if let Some(scs_idx) = self.scs_export {
            let scs_prop_name = self.asset.add_fname("SimpleConstructionScript");
            props.push(ObjectProperty {
                name: scs_prop_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: scs_idx,
            }.into());
        }

        // BlueprintSystemVersion
        let bsv_name = self.asset.add_fname("BlueprintSystemVersion");
        props.push(IntProperty {
            name: bsv_name,
            ancestry: Default::default(),
            property_guid: None,
            duplication_index: 0,
            value: 2, // UE5 blueprint system version
        }.into());

        // Set on the blueprint export (export index 0)
        let bp_idx = (self.blueprint_export.index - 1) as usize;
        if let Some(normal) = self.asset.asset_data.exports[bp_idx].get_normal_export_mut() {
            normal.properties = props;
        }

        // Populate the ClassExport native fields on the generated class
        let gen_idx = (self.bp_gen_class_export.index - 1) as usize;

        // Build func_map and children from function exports
        let mut func_map = IndexedMap::new();
        let mut children: Vec<PackageIndex> = Vec::new();
        for &func_idx in &self.function_exports {
            children.push(func_idx);
            // Extract the function name from the export
            let export_arr_idx = (func_idx.index - 1) as usize;
            let fname = self.asset.asset_data.exports[export_arr_idx]
                .get_base_export()
                .object_name
                .clone();
            func_map.insert(fname, func_idx);
        }

        // Tagged properties on the generated class (UBlueprintGeneratedClass UPROPERTY fields)
        let mut gen_class_props: Vec<Property> = Vec::new();

        // UberGraphFunction reference (first function export = ubergraph)
        if !self.function_exports.is_empty() {
            let uber_name = self.asset.add_fname("UberGraphFunction");
            gen_class_props.push(ObjectProperty {
                name: uber_name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: self.function_exports[0],
            }.into());
        }

        // Set all fields on the ClassExport
        if let Export::ClassExport(ref mut class_exp) = self.asset.asset_data.exports[gen_idx] {
            class_exp.class_default_object = self.cdo_export;
            class_exp.func_map = func_map;
            class_exp.struct_export.children = children;
            class_exp.struct_export.normal_export.properties = gen_class_props;
        }
    }

    // ── CDO default properties ──────────────────────────────────────────────

    fn set_cdo_defaults(&mut self, defaults: &[PropertyDef]) {
        if defaults.is_empty() {
            return;
        }
        let props = convert_property_defs(&mut self.asset, defaults);
        let cdo_idx = (self.cdo_export.index - 1) as usize;
        if let Some(normal) = self.asset.asset_data.exports[cdo_idx].get_normal_export_mut() {
            normal.properties = props;
        }
    }

    // ── Serialize to bytes ───────────────────────────────────────────────────

    fn build(mut self) -> Result<Vec<u8>> {
        self.asset.rebuild_name_map();

        let mut output = Cursor::new(Vec::new());
        self.asset
            .write_data(&mut output, None)
            .map_err(|e| BlueprintError::AssetWrite(format!("Failed to write .uasset: {}", e)))?;

        Ok(output.into_inner())
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Map KAIN's engine version enum to unreal_asset's EngineVersion.
fn map_engine_version(v: BlueprintEngineVersion) -> EngineVersion {
    match v {
        BlueprintEngineVersion::Ue5_1 => EngineVersion::VER_UE5_1,
        BlueprintEngineVersion::Ue5_2
        | BlueprintEngineVersion::Ue5_3
        | BlueprintEngineVersion::Ue5_4
        | BlueprintEngineVersion::Ue5_5 => EngineVersion::VER_UE5_2,
    }
}

// parse_class_path, find_import_by_name, split_soft_path, infer_array_inner_type,
// resolve_object_import — all moved to ue5_asset_utils::{ImportBuilder, property_converter}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ir::{BlueprintDef, ComponentDef, PropertyDef};

    #[test]
    fn test_simple_blueprint_no_components() {
        let bp = BlueprintDef::new(
            "BP_Empty",
            "/Game/Test",
            "/Script/Engine.Actor",
        );

        let result = BlueprintBinaryWriter::write(&bp);
        assert!(result.is_ok(), "write failed: {:?}", result.err());
        let bytes = result.unwrap();
        assert!(bytes.len() > 100, "asset too small: {} bytes", bytes.len());
    }

    #[test]
    fn test_blueprint_with_defaults() {
        let bp = BlueprintDef::new(
            "BP_WithDefaults",
            "/Game/Test",
            "/Script/Engine.Actor",
        )
        .add_default(PropertyDef::float("MaxSpeed", 600.0))
        .add_default(PropertyDef::bool("bCanFly", true))
        .add_default(PropertyDef::int("MaxHealth", 100));

        let result = BlueprintBinaryWriter::write(&bp);
        assert!(result.is_ok(), "write failed: {:?}", result.err());
        let bytes = result.unwrap();
        assert!(bytes.len() > 200, "asset too small: {} bytes", bytes.len());
    }

    #[test]
    fn test_blueprint_with_components() {
        let bp = BlueprintDef::new(
            "BP_WithComps",
            "/Game/Test",
            "/Script/Engine.Pawn",
        )
        .add_component(
            ComponentDef::new("CapsuleComponent", "Capsule")
                .with_default(PropertyDef::float("CapsuleRadius", 42.0))
                .with_default(PropertyDef::float("CapsuleHalfHeight", 96.0)),
        )
        .add_component(
            ComponentDef::new("StaticMeshComponent", "Mesh")
                .with_parent("Capsule"),
        )
        .add_default(PropertyDef::float("MaxWalkSpeed", 600.0));

        let result = BlueprintBinaryWriter::write(&bp);
        assert!(result.is_ok(), "write failed: {:?}", result.err());
        let bytes = result.unwrap();
        assert!(bytes.len() > 500, "asset too small: {} bytes", bytes.len());
    }

    #[test]
    fn test_check_support_no_events() {
        let bp = BlueprintDef::new("BP_Test", "/Game/Test", "/Script/Engine.Actor");
        assert!(BlueprintBinaryWriter::check_support(&bp).is_ok());
    }

    #[test]
    fn test_check_support_with_events_supported() {
        use crate::ir::{EventGraphNode, KismetCall};
        let bp = BlueprintDef::new("BP_Test", "/Game/Test", "/Script/Engine.Actor")
            .add_event(EventGraphNode::begin_play(vec![
                KismetCall::function("Init"),
            ]));
        assert!(BlueprintBinaryWriter::check_support(&bp).is_ok());
    }

    #[test]
    fn test_generate_uasset_succeeds_for_events() {
        use crate::ir::{EventGraphNode, KismetCall};
        let bp = BlueprintDef::new("BP_Test", "/Game/Test", "/Script/Engine.Actor")
            .add_event(EventGraphNode::begin_play(vec![
                KismetCall::function("Init"),
            ]));
        let result = crate::generate_uasset(&bp);
        assert!(result.is_ok());
        assert!(result.unwrap().is_some()); // binary generation now works!
    }

    #[test]
    fn test_blueprint_with_events_produces_bytes() {
        use crate::ir::{EventGraphNode, KismetCall};
        let bp = BlueprintDef::new("BP_Player", "/Game/Test", "/Script/Engine.Pawn")
            .add_component(
                ComponentDef::new("CapsuleComponent", "Capsule")
                    .with_default(PropertyDef::float("CapsuleRadius", 42.0)),
            )
            .add_default(PropertyDef::float("MaxWalkSpeed", 600.0))
            .add_event(EventGraphNode::begin_play(vec![
                KismetCall::function("InitAbilities"),
                KismetCall::function("SetupHUD"),
            ]))
            .add_event(EventGraphNode::tick(vec![
                KismetCall::function("UpdateMovement"),
            ]));

        let result = BlueprintBinaryWriter::write(&bp);
        assert!(result.is_ok(), "write failed: {:?}", result.err());
        let bytes = result.unwrap();
        assert!(bytes.len() > 500, "asset too small: {} bytes", bytes.len());
    }

    #[test]
    fn test_generate_uasset_succeeds_for_simple() {
        let bp = BlueprintDef::new("BP_Test", "/Game/Test", "/Script/Engine.Actor")
            .add_default(PropertyDef::float("Speed", 100.0));
        let result = crate::generate_uasset(&bp);
        assert!(result.is_ok());
        assert!(result.unwrap().is_some()); // binary generation succeeded!
    }

    #[test]
    fn test_all_property_types() {
        let bp = BlueprintDef::new("BP_AllProps", "/Game/Test", "/Script/Engine.Actor")
            .add_default(PropertyDef::float("FloatVal", 1.5))
            .add_default(PropertyDef::int("IntVal", 42))
            .add_default(PropertyDef::bool("BoolVal", true))
            .add_default(PropertyDef::str("StrVal", "hello"))
            .add_default(PropertyDef::name_prop("NameVal", "SomeName"))
            .add_default(PropertyDef::vector("VecVal", 1.0, 2.0, 3.0))
            .add_default(PropertyDef::rotator("RotVal", 10.0, 20.0, 30.0))
            .add_default(PropertyDef::color("ColorVal", 1.0, 0.0, 0.0, 1.0))
            .add_default(PropertyDef::enum_val("EnumVal", "EBlendMode", "Additive"));

        let result = BlueprintBinaryWriter::write(&bp);
        assert!(result.is_ok(), "write failed: {:?}", result.err());
    }
}
