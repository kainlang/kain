/// Kismet bytecode emitter for Blueprint event graphs.
///
/// Converts `EventGraphNode` IR into `KismetExpression` bytecode and
/// `FunctionExport` objects that can be embedded in a `.uasset`.
///
/// Architecture (per event):
///   EventGraphNode::BeginPlay { calls }
///       → FunctionExport "ReceiveBeginPlay"  (FUNC_EVENT | FUNC_BLUEPRINTEVENT)
///           bytecode: [call0, call1, ..., ExReturn(ExNothing), ExEndOfScript]
///
///   Additionally, a minimal UberGraphFunction is created for UE5 compatibility:
///       → FunctionExport "ExecuteUbergraph_<Name>"  (FUNC_UBERGRAPHFUNCTION)
///           bytecode: [ExEndOfScript]  (empty — events have inline bytecode)
///
///   Targeted calls (call.target set):
///       → ExContext { object: ExInstanceVariable(target), inner: ExVirtualFunction(name) }

use std::io::Cursor;

use unreal_asset::{
    exports::{
        Export,
        base_export::BaseExport,
        function_export::FunctionExport,
        struct_export::StructExport,
        normal_export::NormalExport,
    },
    flags::{EObjectFlags, EFunctionFlags},
    types::PackageIndex,
    Asset,
};
use unreal_asset_kismet::{
    EExprToken,
    KismetExpression,
    KismetPropertyPointer,
    FieldPath,
    ExVirtualFunction,
    ExContext,
    ExInstanceVariable,
    ExReturn,
    ExNothing,
    ExEndOfScript,
};

use crate::ir::{EventGraphNode, KismetCall};

// ─── Public API ──────────────────────────────────────────────────────────────

/// Result of emitting kismet bytecode for a blueprint's event graph.
pub struct KismetEmitResult {
    /// FunctionExport objects to append to the asset's export list.
    /// Index 0 is always the UberGraphFunction. The rest are event functions.
    pub function_exports: Vec<Export<PackageIndex>>,
    /// Name of the UberGraphFunction (e.g. "ExecuteUbergraph_BP_Player").
    pub ubergraph_name: String,
    /// Names of all event functions (e.g. ["ReceiveBeginPlay", "ReceiveTick"]).
    pub event_function_names: Vec<String>,
}

/// Emit kismet bytecode for all event graph nodes in a blueprint.
///
/// Each event gets a standalone `FunctionExport` with full inline bytecode.
/// A minimal UberGraphFunction is also created for UE5 runtime compatibility.
///
/// Returns `None` if the event graph is empty.
pub fn emit_event_graph(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    bp_name: &str,
    events: &[EventGraphNode],
    gen_class_export: PackageIndex,
    function_class_import: PackageIndex,
) -> Option<KismetEmitResult> {
    if events.is_empty() {
        return None;
    }

    let ubergraph_name = format!("ExecuteUbergraph_{}", bp_name);
    let mut function_exports: Vec<Export<PackageIndex>> = Vec::new();
    let mut event_function_names: Vec<String> = Vec::new();

    // ── Phase 1: Create the UberGraphFunction (minimal, for compatibility) ──
    let ubergraph_fname = asset.add_fname(&ubergraph_name);
    let ubergraph_export = make_function_export(
        function_class_import,
        gen_class_export,
        ubergraph_fname,
        EFunctionFlags::FUNC_UBERGRAPHFUNCTION,
        vec![ExEndOfScript::default().into()],
    );
    function_exports.push(Export::FunctionExport(ubergraph_export));

    // ── Phase 2: Create standalone event functions ──────────────────────────
    for event in events {
        let (ue_event_name, calls) = match event {
            EventGraphNode::BeginPlay { calls } => ("ReceiveBeginPlay", calls.as_slice()),
            EventGraphNode::Tick { calls } => ("ReceiveTick", calls.as_slice()),
            EventGraphNode::CustomEvent { event_name, calls } => {
                (event_name.as_str(), calls.as_slice())
            }
        };

        // Build full inline bytecode for this event
        let mut bytecode: Vec<KismetExpression> = Vec::new();
        for call in calls {
            bytecode.push(emit_kismet_call(asset, call, gen_class_export));
        }
        bytecode.push(ExReturn {
            token: EExprToken::ExReturn,
            return_expression: Box::new(ExNothing::default().into()),
        }.into());
        bytecode.push(ExEndOfScript::default().into());

        let event_fname = asset.add_fname(ue_event_name);
        let event_flags = EFunctionFlags::FUNC_EVENT
            | EFunctionFlags::FUNC_BLUEPRINTEVENT
            | EFunctionFlags::FUNC_PUBLIC;

        let event_export = make_function_export(
            function_class_import,
            gen_class_export,
            event_fname,
            event_flags,
            bytecode,
        );
        function_exports.push(Export::FunctionExport(event_export));
        event_function_names.push(ue_event_name.to_string());
    }

    Some(KismetEmitResult {
        function_exports,
        ubergraph_name,
        event_function_names,
    })
}

// ─── Internal helpers ────────────────────────────────────────────────────────

/// Create a FunctionExport with the given bytecode.
fn make_function_export(
    class_import: PackageIndex,
    outer: PackageIndex,
    name: unreal_asset_base::types::FName,
    flags: EFunctionFlags,
    bytecode: Vec<KismetExpression>,
) -> FunctionExport<PackageIndex> {
    FunctionExport {
        struct_export: StructExport {
            normal_export: NormalExport {
                base_export: BaseExport {
                    class_index: class_import,
                    super_index: PackageIndex::new(0),
                    template_index: PackageIndex::new(0),
                    outer_index: outer,
                    object_name: name,
                    object_flags: EObjectFlags::RF_PUBLIC,
                    ..Default::default()
                },
                extras: Vec::new(),
                properties: Vec::new(),
            },
            field: Default::default(),
            super_struct: PackageIndex::new(0),
            children: Vec::new(),
            loaded_properties: Vec::new(),
            script_bytecode: Some(bytecode),
            script_bytecode_size: 0, // auto-computed by StructExport::write
            script_bytecode_raw: None,
        },
        function_flags: flags,
    }
}

/// Convert a single `KismetCall` IR node into a `KismetExpression`.
///
/// - No target → `ExVirtualFunction` (call on self)
/// - With target → `ExContext { ExInstanceVariable(target), ExVirtualFunction(name) }`
fn emit_kismet_call(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    call: &KismetCall,
    gen_class_export: PackageIndex,
) -> KismetExpression {
    let fname = asset.add_fname(&call.function_name);

    let vfunc: KismetExpression = ExVirtualFunction {
        token: EExprToken::ExVirtualFunction,
        virtual_function_name: fname,
        parameters: Vec::new(),
    }.into();

    match &call.target {
        None => vfunc,
        Some(target_name) => {
            // Wrap in ExContext to call the function on the target object.
            // ExContext reads the target via ExInstanceVariable, then
            // executes the function call in that object's context.
            let target_fname = asset.add_fname(target_name);
            let var_ptr = KismetPropertyPointer::from_new(FieldPath::new(
                vec![target_fname],
                gen_class_export,
            ));

            ExContext {
                token: EExprToken::ExContext,
                object_expression: Box::new(ExInstanceVariable {
                    token: EExprToken::ExInstanceVariable,
                    variable: var_ptr,
                }.into()),
                offset: 0, // skip offset if context is null (0 = no skip, crash on null)
                r_value_pointer: KismetPropertyPointer::default(),
                context_expression: Box::new(vfunc),
            }.into()
        }
    }
}

// ─── Tests ───────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ir::{EventGraphNode, KismetCall};
    use unreal_asset::engine_version::EngineVersion;

    fn make_test_asset() -> Asset<Cursor<Vec<u8>>> {
        Asset::new_empty(EngineVersion::VER_UE5_2)
    }

    fn make_function_class_import(asset: &mut Asset<Cursor<Vec<u8>>>) -> PackageIndex {
        let coreuobject = asset.add_fname("/Script/CoreUObject");
        let package = asset.add_fname("Package");
        asset.imports.push(unreal_asset::Import {
            class_package: coreuobject.clone(),
            class_name: package.clone(),
            outer_index: PackageIndex::new(0),
            object_name: coreuobject.clone(),
            optional: false,
        });
        let coreuobject_idx = PackageIndex::new(-(asset.imports.len() as i32));

        let class_fname = asset.add_fname("Class");
        let func_fname = asset.add_fname("Function");
        asset.imports.push(unreal_asset::Import {
            class_package: coreuobject.clone(),
            class_name: class_fname,
            outer_index: coreuobject_idx,
            object_name: func_fname,
            optional: false,
        });
        PackageIndex::new(-(asset.imports.len() as i32))
    }

    #[test]
    fn test_emit_empty_event_graph() {
        let mut asset = make_test_asset();
        let func_import = make_function_class_import(&mut asset);
        let gen_class = PackageIndex::new(1);

        let result = emit_event_graph(&mut asset, "BP_Test", &[], gen_class, func_import);
        assert!(result.is_none());
    }

    #[test]
    fn test_emit_begin_play() {
        let mut asset = make_test_asset();
        let func_import = make_function_class_import(&mut asset);
        let gen_class = PackageIndex::new(1);

        let events = vec![
            EventGraphNode::begin_play(vec![
                KismetCall::function("InitAbilities"),
                KismetCall::function("SetupHUD"),
            ]),
        ];

        let result = emit_event_graph(&mut asset, "BP_Test", &events, gen_class, func_import);
        assert!(result.is_some());

        let emit = result.unwrap();
        assert_eq!(emit.ubergraph_name, "ExecuteUbergraph_BP_Test");
        assert_eq!(emit.event_function_names, vec!["ReceiveBeginPlay"]);
        // 1 ubergraph + 1 event = 2 function exports
        assert_eq!(emit.function_exports.len(), 2);
    }

    #[test]
    fn test_emit_multiple_events() {
        let mut asset = make_test_asset();
        let func_import = make_function_class_import(&mut asset);
        let gen_class = PackageIndex::new(1);

        let events = vec![
            EventGraphNode::begin_play(vec![
                KismetCall::function("Init"),
            ]),
            EventGraphNode::tick(vec![
                KismetCall::function("UpdateMovement"),
            ]),
            EventGraphNode::custom("OnDamageReceived", vec![
                KismetCall::function("PlayHitReaction"),
                KismetCall::function("UpdateHealthBar"),
            ]),
        ];

        let result = emit_event_graph(&mut asset, "BP_Player", &events, gen_class, func_import);
        assert!(result.is_some());

        let emit = result.unwrap();
        assert_eq!(emit.ubergraph_name, "ExecuteUbergraph_BP_Player");
        assert_eq!(emit.event_function_names.len(), 3);
        assert_eq!(emit.event_function_names[0], "ReceiveBeginPlay");
        assert_eq!(emit.event_function_names[1], "ReceiveTick");
        assert_eq!(emit.event_function_names[2], "OnDamageReceived");
        // 1 ubergraph + 3 events = 4 function exports
        assert_eq!(emit.function_exports.len(), 4);
    }

    #[test]
    fn test_emit_kismet_call_self() {
        let mut asset = make_test_asset();
        let gen_class = PackageIndex::new(1);
        let call = KismetCall::function("DoSomething");
        let expr = emit_kismet_call(&mut asset, &call, gen_class);

        match expr {
            KismetExpression::ExVirtualFunction(vf) => {
                assert_eq!(vf.token, EExprToken::ExVirtualFunction);
                vf.virtual_function_name.get_content(|name| {
                    assert_eq!(name, "DoSomething");
                });
            }
            other => panic!("Expected ExVirtualFunction, got {:?}", other),
        }
    }

    #[test]
    fn test_emit_kismet_call_targeted() {
        let mut asset = make_test_asset();
        let gen_class = PackageIndex::new(1);
        let call = KismetCall::function("SetMaterial").on("Mesh");
        let expr = emit_kismet_call(&mut asset, &call, gen_class);

        match expr {
            KismetExpression::ExContext(ctx) => {
                assert_eq!(ctx.token, EExprToken::ExContext);
                // Inner should be ExInstanceVariable
                match *ctx.object_expression {
                    KismetExpression::ExInstanceVariable(ref iv) => {
                        assert_eq!(iv.token, EExprToken::ExInstanceVariable);
                    }
                    ref other => panic!("Expected ExInstanceVariable, got {:?}", other),
                }
                // Context expression should be ExVirtualFunction
                match *ctx.context_expression {
                    KismetExpression::ExVirtualFunction(ref vf) => {
                        vf.virtual_function_name.get_content(|name| {
                            assert_eq!(name, "SetMaterial");
                        });
                    }
                    ref other => panic!("Expected ExVirtualFunction, got {:?}", other),
                }
            }
            other => panic!("Expected ExContext, got {:?}", other),
        }
    }
}
