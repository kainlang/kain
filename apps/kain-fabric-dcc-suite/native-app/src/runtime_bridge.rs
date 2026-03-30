use crate::bridge_contract::{
    NativeBridgeContract,
    CONTRACT_ROOT_REPORT_URI,
    CONTRACT_ROOT_URI,
    MESH_ACTIVE_EDIT_TARGET_ID,
    MESH_ACTIVE_EDIT_TARGET_URI,
    MESH_AUTHORED_PRIMITIVE_DOCUMENT_ID,
    MESH_AUTHORED_PRIMITIVE_URI,
    MESH_CONTRACT_DOCUMENT_ID,
    MESH_CONTRACT_DOCUMENT_URI,
    MESH_CONTRACT_REPORT_ID,
    MESH_CONTRACT_REPORT_PATH,
    MESH_CONTRACT_REPORT_URI,
    MESH_IMPORTED_PAYLOAD_DOCUMENT_ID,
    MESH_IMPORTED_PAYLOAD_URI,
    MESH_TOPOLOGY_OUTPUT_DOCUMENT_ID,
    MESH_TOPOLOGY_OUTPUT_URI,
    TOPOLOGY_HISTORY_DOCUMENT_ID,
    TOPOLOGY_HISTORY_DOCUMENT_URI,
    TOPOLOGY_HISTORY_REPORT_ID,
    TOPOLOGY_HISTORY_REPORT_PATH,
    TOPOLOGY_HISTORY_REPORT_URI,
};

use std::{
    collections::BTreeSet,
    fs,
    path::{Path, PathBuf},
    thread,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use serde_json::{json, Value};

const BRIDGE_POLL_INTERVAL_MS: u64 = 150;

const TOPOLOGY_HISTORY_REBUILD_REPORT: &str = "state/topology_history_report.json";

#[derive(Clone, Debug)]
pub struct LiveBridgePaths {
    pub command_queue_path: PathBuf,
    pub session_document_path: PathBuf,
    pub runtime_snapshot_path: PathBuf,
    pub mirrored_session_document_paths: Vec<PathBuf>,
    pub mirrored_runtime_snapshot_paths: Vec<PathBuf>,
}

#[derive(Clone, Debug, Default, serde::Deserialize)]
struct RuntimeCommandRequest {
    #[serde(default)]
    command_id: String,
    #[serde(default)]
    label: String,
    #[serde(default)]
    intent: String,
    #[serde(default)]
    surface: String,
    #[serde(default)]
    source: String,
    #[serde(default)]
    requested_at: String,
}

pub fn spawn_live_bridge(paths: LiveBridgePaths) {
    thread::spawn(move || {
        let _ = run_live_bridge_loop(paths);
    });
}

fn run_live_bridge_loop(paths: LiveBridgePaths) -> Result<(), String> {
    ensure_parent_directory(&paths.command_queue_path)?;
    ensure_parent_directory(&paths.session_document_path)?;
    ensure_parent_directory(&paths.runtime_snapshot_path)?;

    if !paths.command_queue_path.exists() {
        fs::write(&paths.command_queue_path, "").map_err(|err| err.to_string())?;
    }

    let mut processed_line_count = 0usize;
    loop {
        process_command_queue(&paths, &mut processed_line_count)?;
        thread::sleep(Duration::from_millis(BRIDGE_POLL_INTERVAL_MS));
    }
}

fn process_command_queue(
    paths: &LiveBridgePaths,
    processed_line_count: &mut usize,
) -> Result<(), String> {
    let queue_contents = fs::read_to_string(&paths.command_queue_path).unwrap_or_default();
    let command_lines = queue_contents
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .collect::<Vec<_>>();

    if command_lines.len() < *processed_line_count {
        *processed_line_count = 0;
    }
    if command_lines.len() == *processed_line_count {
        return Ok(());
    }

    let mut session_document =
        load_json_document(&paths.session_document_path, &paths.mirrored_session_document_paths);
    let mut runtime_snapshot =
        load_json_document(&paths.runtime_snapshot_path, &paths.mirrored_runtime_snapshot_paths);

    for command_line in &command_lines[*processed_line_count..] {
        let request = serde_json::from_str::<RuntimeCommandRequest>(command_line).unwrap_or_default();
        *processed_line_count += 1;
        if request.command_id.is_empty() {
            continue;
        }
        apply_command_request(
            &mut session_document,
            &mut runtime_snapshot,
            &request,
            *processed_line_count,
            paths,
            &NativeBridgeContract::new(),
        );
    }

    write_json_document(
        &paths.session_document_path,
        &paths.mirrored_session_document_paths,
        &session_document,
    )?;
    write_json_document(
        &paths.runtime_snapshot_path,
        &paths.mirrored_runtime_snapshot_paths,
        &runtime_snapshot,
    )?;
    Ok(())
}

fn apply_command_request(
    session_document: &mut Value,
    runtime_snapshot: &mut Value,
    request: &RuntimeCommandRequest,
    processed_command_count: usize,
    paths: &LiveBridgePaths,
    contract: &NativeBridgeContract,
) {
    ensure_mode_for_command(session_document, runtime_snapshot, &request.command_id);

    match request.command_id.as_str() {
        "runtime.reload" => {}
        "project.bootstrap" => apply_project_bootstrap(session_document, runtime_snapshot),
        "workspace.switch_mode" => apply_workspace_switch(session_document, runtime_snapshot),
        "mesh.open_document" => apply_mesh_open_document(session_document, processed_command_count),
        "mesh.set_edit_target" => apply_mesh_set_edit_target(session_document, processed_command_count),
        "mesh.set_authoring_policy" => apply_mesh_set_authoring_policy(session_document),
        "mesh.create_primitive" => apply_mesh_create_primitive(
            session_document,
            runtime_snapshot,
            processed_command_count,
        ),
        "mesh.import_asset" => apply_mesh_import_asset(session_document, processed_command_count),
        "mesh.edit_topology" => apply_mesh_edit_topology(session_document),
        "mesh.rebuild_topology" => apply_mesh_rebuild_topology(session_document),
        "mesh.subdivide" => apply_mesh_subdivide(session_document),
        "mesh.pack_uv" => apply_mesh_pack_uv(session_document),
        "tool.activate" => apply_tool_cycle(session_document, runtime_snapshot),
        "gizmo.set_mode" => apply_gizmo_mode_cycle(session_document),
        "gizmo.set_space" => apply_gizmo_space_toggle(session_document),
        "gizmo.toggle_snap" => apply_gizmo_snap_toggle(session_document),
        "asset.ingest_package" => apply_asset_ingest(session_document, processed_command_count),
        "selection.set" => apply_selection_change(session_document, processed_command_count),
        "sculpt.apply_stroke" => apply_sculpt_stroke(session_document, runtime_snapshot),
        "topology.rebuild" => apply_topology_rebuild(session_document),
        "rig.sync_controls" => apply_rig_sync(session_document, runtime_snapshot),
        "sim.tick" => apply_sim_tick(session_document, runtime_snapshot),
        "material.author_texture_set" => {
            apply_texture_set_authoring(session_document, processed_command_count)
        }
        "material.paint_layer" => apply_material_paint(session_document, runtime_snapshot),
        "material.edit_svg_mask" => apply_svg_mask_edit(session_document),
        "material.bake_preview" => apply_material_bake_preview(session_document),
        "material.export_textures" => apply_material_export(session_document),
        "render.review_capture" => apply_render_review_capture(session_document, runtime_snapshot),
        "render.preview" => apply_render_preview(session_document, runtime_snapshot),
        "compositor.rebuild" => apply_compositor_rebuild(session_document, runtime_snapshot),
        "publish.package" => apply_publish_package(session_document, runtime_snapshot),
        "tensor.train_step" => apply_tensor_train_step(session_document),
        "tensor.infer_step" => apply_tensor_infer_step(session_document),
        _ => {
            if let Some(seam) = contract.seam_for_command(&request.command_id) {
                set_string_at_path(
                    session_document,
                    &["reports", seam.report_key],
                    format!("{}/{}", contract.report_uri, seam.topic),
                );
                set_bool_at_path(session_document, &["dirty", seam.active_dirty_key], true);
            }
            set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
        }
    }

    sync_runtime_snapshot_from_session(
        session_document,
        runtime_snapshot,
        request,
        processed_command_count,
        paths,
    );
}

fn apply_project_bootstrap(session_document: &mut Value, runtime_snapshot: &Value) {
    if let Some(first_mode) = get_string_vec_at_path(session_document, &["workspace", "available_modes"])
        .first()
        .cloned()
    {
        set_string_at_path(session_document, &["workspace", "active_mode"], first_mode.clone());
        if let Some(tool_id) = first_tool_for_lane(runtime_snapshot, &first_mode) {
            apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, &tool_id);
        }
    }
    set_string_at_path(session_document, &["gizmo", "mode"], "translate");
    set_string_at_path(session_document, &["gizmo", "space"], "world");
    set_bool_at_path(session_document, &["gizmo", "snap_enabled"], false);
    set_bool_at_path(session_document, &["gizmo", "visible"], true);
    set_string_at_path(session_document, &["automation", "last_audit_status"], "ready");
    set_all_dirty_flags(session_document, false);
}

fn apply_workspace_switch(session_document: &mut Value, runtime_snapshot: &Value) {
    let available_modes = get_string_vec_at_path(session_document, &["workspace", "available_modes"]);
    let current_mode = get_string_at_path(session_document, &["workspace", "active_mode"])
        .unwrap_or_default();
    if let Some(next_mode) = cycle_string_value(&available_modes, &current_mode) {
        set_string_at_path(session_document, &["workspace", "active_mode"], next_mode.clone());
        if let Some(tool_id) = first_tool_for_lane(runtime_snapshot, &next_mode) {
            apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, &tool_id);
        }
    }
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_tool_cycle(session_document: &mut Value, runtime_snapshot: &Value) {
    let active_mode = get_string_at_path(session_document, &["workspace", "active_mode"])
        .unwrap_or_default();
    let lane_tools = collect_tool_ids_for_lane(runtime_snapshot, &active_mode);
    let available_tools = if lane_tools.is_empty() {
        collect_registry_ids(runtime_snapshot, &["dcc_suite_state", "available_tools"], "id")
    } else {
        lane_tools
    };
    let current_tool = get_string_at_path(session_document, &["tooling", "active_tool"])
        .unwrap_or_default();
    if let Some(next_tool) = cycle_string_value(&available_tools, &current_tool) {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, &next_tool);
    }
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_gizmo_mode_cycle(session_document: &mut Value) {
    let current_mode = get_string_at_path(session_document, &["gizmo", "mode"]).unwrap_or_default();
    let modes = vec![
        "translate".to_string(),
        "rotate".to_string(),
        "scale".to_string(),
    ];
    if let Some(next_mode) = cycle_string_value(&modes, &current_mode) {
        set_string_at_path(session_document, &["gizmo", "mode"], next_mode);
    }
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_gizmo_space_toggle(session_document: &mut Value) {
    let current_space = get_string_at_path(session_document, &["gizmo", "space"]).unwrap_or_default();
    let next_space = if current_space.eq_ignore_ascii_case("local") {
        "world"
    } else {
        "local"
    };
    set_string_at_path(session_document, &["gizmo", "space"], next_space);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_gizmo_snap_toggle(session_document: &mut Value) {
    let current_value = get_bool_at_path(session_document, &["gizmo", "snap_enabled"]).unwrap_or(false);
    set_bool_at_path(session_document, &["gizmo", "snap_enabled"], !current_value);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_asset_ingest(session_document: &mut Value, processed_command_count: usize) {
    let staged_package_count =
        increment_i64_at_path(session_document, &["ingest", "staged_package_count"], 1);
    set_string_at_path(
        session_document,
        &["ingest", "last_package_uri"],
        format!("asset://bridge/session_package_{processed_command_count:04}"),
    );
    let package_kind = if staged_package_count % 2 == 0 { "usd" } else { "gltf" };
    set_string_at_path(session_document, &["ingest", "last_package_kind"], package_kind);
    for dirty_key in [
        "asset_dirty",
        "render_dirty",
        "compositor_dirty",
        "publish_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_open_document(session_document: &mut Value, _processed_command_count: usize) {
    set_mesh_contract_report(session_document, "mesh.open_document");
    set_string_at_path(
        session_document,
        &["mesh", "active_document_id"],
        MESH_CONTRACT_DOCUMENT_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_document_uri"],
        MESH_CONTRACT_DOCUMENT_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_uri"],
        MESH_ACTIVE_EDIT_TARGET_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "topology_edit_mode"],
        "object",
    );
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[MESH_ACTIVE_EDIT_TARGET_ID.to_string()],
    );
    for dirty_key in ["render_dirty", "session_needs_save"] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_set_edit_target(session_document: &mut Value, _processed_command_count: usize) {
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_uri"],
        MESH_ACTIVE_EDIT_TARGET_URI,
    );
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[MESH_ACTIVE_EDIT_TARGET_ID.to_string()],
    );
    for dirty_key in ["render_dirty", "session_needs_save"] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_set_authoring_policy(session_document: &mut Value) {
    let policies = vec![
        "mesh_authoring_policy/startup_hybrid".to_string(),
        "mesh_authoring_policy/imported_asset_preferred".to_string(),
        "mesh_authoring_policy/authored_primitives_first".to_string(),
        "mesh_authoring_policy/topology_edit_session".to_string(),
    ];
    let current_policy =
        get_string_at_path(session_document, &["mesh", "mesh_authoring_policy_id"]).unwrap_or_default();
    if let Some(next_policy) = cycle_string_value(&policies, &current_policy) {
        set_string_at_path(
            session_document,
            &["mesh", "mesh_authoring_policy_id"],
            next_policy,
        );
    }
    for dirty_key in ["topology_dirty", "render_dirty", "session_needs_save"] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_create_primitive(
    session_document: &mut Value,
    runtime_snapshot: &Value,
    _processed_command_count: usize,
) {
    set_mesh_contract_report(session_document, "mesh.create_primitive");
    if tool_exists(runtime_snapshot, "select") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "select");
    }
    let primitive_templates = vec![
        "primitive/cube".to_string(),
        "primitive/cylinder".to_string(),
        "primitive/uv_sphere".to_string(),
        "primitive/cone".to_string(),
        "primitive/torus".to_string(),
        "primitive/plane".to_string(),
    ];
    let current_template = get_string_at_path(
        session_document,
        &["mesh", "active_primitive_template_id"],
    )
    .unwrap_or_default();
    if let Some(next_template) = cycle_string_value(&primitive_templates, &current_template) {
        set_string_at_path(
            session_document,
            &["mesh", "active_primitive_template_id"],
            next_template,
        );
    }
    set_string_array_at_path(
        session_document,
        &["mesh", "primitive_templates"],
        &primitive_templates,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_document_id"],
        MESH_AUTHORED_PRIMITIVE_DOCUMENT_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_document_uri"],
        MESH_AUTHORED_PRIMITIVE_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_uri"],
        MESH_ACTIVE_EDIT_TARGET_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        "mesh_authoring_policy/authored_primitives_first",
    );
    set_string_at_path(
        session_document,
        &["mesh", "topology_edit_mode"],
        "object",
    );
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[MESH_ACTIVE_EDIT_TARGET_ID.to_string()],
    );
    for dirty_key in [
        "asset_dirty",
        "topology_dirty",
        "render_dirty",
        "publish_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_import_asset(session_document: &mut Value, _processed_command_count: usize) {
    set_mesh_contract_report(session_document, "mesh.import_asset");
    set_string_at_path(
        session_document,
        &["mesh", "active_document_id"],
        MESH_IMPORTED_PAYLOAD_DOCUMENT_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        "mesh_authoring_policy/imported_asset_preferred",
    );
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[MESH_ACTIVE_EDIT_TARGET_ID.to_string()],
    );
    set_string_at_path(
        session_document,
        &["ingest", "last_package_uri"],
        MESH_IMPORTED_PAYLOAD_URI,
    );
    set_string_at_path(session_document, &["ingest", "last_package_kind"], "gltf");
    increment_i64_at_path(session_document, &["ingest", "staged_package_count"], 1);
    for dirty_key in [
        "asset_dirty",
        "topology_dirty",
        "render_dirty",
        "publish_dirty",
        "tensor_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_edit_topology(session_document: &mut Value) {
    let topology_modes = vec![
        "object".to_string(),
        "vertex".to_string(),
        "edge".to_string(),
        "face".to_string(),
    ];
    let current_mode =
        get_string_at_path(session_document, &["mesh", "topology_edit_mode"]).unwrap_or_default();
    if let Some(next_mode) = cycle_string_value(&topology_modes, &current_mode) {
        set_string_at_path(session_document, &["mesh", "topology_edit_mode"], next_mode);
    }
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        "mesh_authoring_policy/topology_edit_session",
    );
    for dirty_key in [
        "topology_dirty",
        "rig_dirty",
        "render_dirty",
        "publish_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_mesh_rebuild_topology(session_document: &mut Value) {
    set_mesh_contract_report(session_document, "mesh.rebuild_topology");
    set_string_at_path(
        session_document,
        &["mesh", "subdivision_level"],
        "2",
    );
    set_string_at_path(
        session_document,
        &["reports", "topology_history_report_id"],
        TOPOLOGY_HISTORY_REPORT_ID,
    );
    set_string_at_path(
        session_document,
        &["reports", "topology_history_report_uri"],
        TOPOLOGY_HISTORY_REPORT_URI,
    );
    set_string_at_path(
        session_document,
        &["reports", "topology_history_report_path"],
        TOPOLOGY_HISTORY_REPORT_PATH,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_document_id"],
        MESH_TOPOLOGY_OUTPUT_DOCUMENT_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_document_uri"],
        MESH_TOPOLOGY_OUTPUT_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["mesh", "active_edit_target_uri"],
        MESH_ACTIVE_EDIT_TARGET_URI,
    );
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        "mesh_authoring_policy/topology_edit_session",
    );
    set_string_at_path(
        session_document,
        &["topology_history", "history_document_id"],
        TOPOLOGY_HISTORY_DOCUMENT_ID,
    );
    set_string_at_path(
        session_document,
        &["topology_history", "history_document_uri"],
        TOPOLOGY_HISTORY_DOCUMENT_URI,
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_lineage_reason"],
        "topology.rebuild",
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_upstream_topology_report"],
        TOPOLOGY_HISTORY_REBUILD_REPORT,
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_active_edit_target_id"],
        MESH_ACTIVE_EDIT_TARGET_ID,
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_topology_output_id"],
        MESH_TOPOLOGY_OUTPUT_DOCUMENT_ID,
    );
    set_bool_at_path(session_document, &["dirty", "topology_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "rig_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_mesh_subdivide(session_document: &mut Value) {
    set_string_at_path(session_document, &["mesh", "topology_edit_mode"], "subdivide");
    set_string_at_path(
        session_document,
        &["mesh", "subdivision_level"],
        "3",
    );
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        "mesh_authoring_policy/topology_edit_session",
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_lineage_reason"],
        "mesh.subdivide",
    );
    set_bool_at_path(session_document, &["dirty", "topology_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "rig_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_mesh_pack_uv(session_document: &mut Value) {
    set_string_at_path(session_document, &["mesh", "topology_edit_mode"], "uv_pack");
    set_string_at_path(
        session_document,
        &["topology_history", "last_lineage_reason"],
        "mesh.pack_uv",
    );
    set_bool_at_path(session_document, &["dirty", "material_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_selection_change(session_document: &mut Value, processed_command_count: usize) {
    let empty_subobject_ids: Vec<String> = Vec::new();
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[format!("entity/bridge_selection_{processed_command_count:04}")],
    );
    set_string_array_at_path(
        session_document,
        &["selection", "subobject_ids"],
        &empty_subobject_ids,
    );
}

fn apply_sculpt_stroke(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "clay_sculpt") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "clay_sculpt");
    }
    increment_i64_at_path(session_document, &["tooling", "brush_radius"], 2);
    for dirty_key in [
        "sculpt_dirty",
        "topology_dirty",
        "material_dirty",
        "render_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_topology_rebuild(session_document: &mut Value) {
    set_string_at_path(
        session_document,
        &["topology_history", "last_lineage_reason"],
        "topology.rebuild",
    );
    set_string_at_path(
        session_document,
        &["topology_history", "last_upstream_topology_report"],
        TOPOLOGY_HISTORY_REBUILD_REPORT,
    );
    set_bool_at_path(session_document, &["dirty", "topology_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_rig_sync(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "control_rig_edit") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "control_rig_edit");
    }
    set_bool_at_path(session_document, &["dirty", "rig_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "animation_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_sim_tick(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "cache_solver") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "cache_solver");
    }
    let frame = increment_i64_at_path(session_document, &["animation", "frame"], 1);
    set_i64_at_path(session_document, &["simulation", "last_tick_frame"], frame);
    set_bool_at_path(session_document, &["dirty", "simulation_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
}

fn apply_texture_set_authoring(session_document: &mut Value, processed_command_count: usize) {
    set_string_at_path(
        session_document,
        &["materials", "active_texture_set_id"],
        format!("textureset/bridge_udim_{processed_command_count:04}"),
    );
    let resolution = get_i64_at_path(session_document, &["materials", "paint_resolution"])
        .unwrap_or(2048);
    let next_resolution = match resolution {
        2048 => 4096,
        4096 => 8192,
        _ => 2048,
    };
    set_i64_at_path(
        session_document,
        &["materials", "paint_resolution"],
        next_resolution,
    );
    for dirty_key in [
        "material_dirty",
        "render_dirty",
        "compositor_dirty",
        "publish_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], true);
    }
}

fn apply_material_paint(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "material_layer_paint") {
        apply_tool_defaults_from_snapshot(
            session_document,
            runtime_snapshot,
            "material_layer_paint",
        );
    }
    set_bool_at_path(session_document, &["dirty", "material_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_svg_mask_edit(session_document: &mut Value) {
    set_bool_at_path(session_document, &["dirty", "material_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], true);
}

fn apply_material_bake_preview(session_document: &mut Value) {
    set_bool_at_path(session_document, &["dirty", "material_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "render_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
}

fn apply_material_export(session_document: &mut Value) {
    set_bool_at_path(session_document, &["dirty", "material_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
}

fn apply_render_review_capture(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "render_review_capture") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "render_review_capture");
    }
    set_bool_at_path(session_document, &["dirty", "render_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
}

fn apply_render_preview(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "render_preview") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "render_preview");
    }
    set_bool_at_path(session_document, &["dirty", "render_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], true);
}

fn apply_compositor_rebuild(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "comp_rebuild") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "comp_rebuild");
    }
    set_bool_at_path(session_document, &["dirty", "compositor_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], true);
    set_string_at_path(
        session_document,
        &["compositor", "last_rebuild_reason"],
        "live-command-bridge",
    );
}

fn apply_publish_package(session_document: &mut Value, runtime_snapshot: &Value) {
    if tool_exists(runtime_snapshot, "publish_bundle") {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, "publish_bundle");
    }
    set_bool_at_path(session_document, &["dirty", "publish_dirty"], false);
    set_bool_at_path(session_document, &["dirty", "session_needs_save"], false);
    set_string_at_path(session_document, &["automation", "last_audit_status"], "packaged");
}

fn set_mesh_contract_report(session_document: &mut Value, lineage_reason: &str) {
    set_string_at_path(
        session_document,
        &["reports", "mesh_contract_report_id"],
        MESH_CONTRACT_REPORT_ID,
    );
    set_string_at_path(
        session_document,
        &["reports", "mesh_contract_report_uri"],
        MESH_CONTRACT_REPORT_URI,
    );
    set_string_at_path(
        session_document,
        &["reports", "mesh_contract_report_path"],
        MESH_CONTRACT_REPORT_PATH,
    );
    set_string_at_path(
        session_document,
        &["mesh", "mesh_authoring_policy_id"],
        if lineage_reason == "mesh.import_asset" {
            "mesh_authoring_policy/imported_asset_preferred"
        } else if lineage_reason == "mesh.create_primitive" {
            "mesh_authoring_policy/authored_primitives_first"
        } else {
            "mesh_authoring_policy/topology_edit_session"
        },
    );
}

fn apply_tensor_train_step(session_document: &mut Value) {
    set_bool_at_path(session_document, &["dirty", "tensor_dirty"], false);
    set_string_at_path(
        session_document,
        &["automation", "last_audit_status"],
        "tensor-train-dispatched",
    );
}

fn apply_tensor_infer_step(session_document: &mut Value) {
    set_bool_at_path(session_document, &["dirty", "tensor_dirty"], false);
    set_string_at_path(
        session_document,
        &["automation", "last_audit_status"],
        "tensor-infer-dispatched",
    );
}

fn sync_runtime_snapshot_from_session(
    session_document: &mut Value,
    runtime_snapshot: &mut Value,
    request: &RuntimeCommandRequest,
    processed_command_count: usize,
    paths: &LiveBridgePaths,
) {
    let active_mode = get_string_at_path(session_document, &["workspace", "active_mode"])
        .unwrap_or_default();
    let active_tool = get_string_at_path(session_document, &["tooling", "active_tool"])
        .unwrap_or_default();
    let active_mode_label = lookup_registry_label(
        runtime_snapshot,
        &["dcc_suite_state", "workspace_modes"],
        &active_mode,
    )
    .unwrap_or_else(|| active_mode.clone());
    let active_tool_label = lookup_registry_label(
        runtime_snapshot,
        &["dcc_suite_state", "available_tools"],
        &active_tool,
    )
    .unwrap_or_else(|| active_tool.clone());
    let selection_count = get_string_vec_at_path(session_document, &["selection", "entity_ids"]).len();
    let gizmo_mode = get_string_at_path(session_document, &["gizmo", "mode"]).unwrap_or_default();
    let gizmo_space = get_string_at_path(session_document, &["gizmo", "space"]).unwrap_or_default();
    let gizmo_snap = get_bool_at_path(session_document, &["gizmo", "snap_enabled"]).unwrap_or(false);
    let animation_frame = get_i64_at_path(session_document, &["animation", "frame"]).unwrap_or(0);
    let updated_at = now_iso_string();
    let command_label = if request.label.is_empty() {
        request.command_id.clone()
    } else {
        request.label.clone()
    };

    let next_intent_queue = build_next_intent_queue(runtime_snapshot, request);
    let active_intent_ids = intent_queue_ids(&next_intent_queue);
    set_string_array_at_path(session_document, &["jobs", "active_intents"], &active_intent_ids);

    set_value_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "session"],
        session_document.clone(),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "derived", "active_mode_label"],
        active_mode_label.clone(),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "derived", "active_tool_label"],
        active_tool_label.clone(),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "derived", "selection_summary"],
        format!("{selection_count} entity selected"),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "derived", "gizmo_summary"],
        format!(
            "{gizmo_mode} | {gizmo_space} | snap {}",
            if gizmo_snap { "on" } else { "off" }
        ),
    );
    set_i64_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "derived", "queued_intent_count"],
        next_intent_queue.len() as i64,
    );
    set_string_at_path(runtime_snapshot, &["dcc_suite_state", "bridge", "status"], "live");
    set_value_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "presentation"],
        json!({
            "layout": "dock",
            "fixed_workspace_frame": true,
            "dock_regions": ["center", "left", "right", "bottom", "top"],
            "document_flow_surfaces": false,
            "viewport_centered_layout": true,
            "startup_focus_surface": "viewport_stage",
            "startup_lane_rail": "workspace_navigator"
        }),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "bridge", "command_queue_path"],
        paths.command_queue_path.to_string_lossy(),
    );
    set_string_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "bridge", "session_document_path"],
        paths.session_document_path.to_string_lossy(),
    );
    set_i64_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "bridge", "processed_command_count"],
        processed_command_count as i64,
    );
    set_value_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "latest_command"],
        json!({
            "command_id": request.command_id.clone(),
            "label": command_label.clone(),
            "intent": request.intent.clone(),
            "surface": request.surface.clone(),
            "source": request.source.clone(),
            "requested_at": if request.requested_at.is_empty() { updated_at.clone() } else { request.requested_at.clone() },
            "processed_at": updated_at.clone(),
        }),
    );
    set_value_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "intent_queue"],
        Value::Array(next_intent_queue.clone()),
    );

    let recent_session_title = format!("Kain Fabric DCC Suite | {active_mode_label}");
    set_string_at_path(runtime_snapshot, &["sessions", "recent_session_title"], recent_session_title.clone());
    set_i64_at_path(runtime_snapshot, &["sessions", "total_sessions"], 1);
    if let Some(recent_session) = ensure_array_object_at_index(runtime_snapshot, &["recent_sessions"], 0) {
        recent_session.insert("title".to_string(), Value::String(recent_session_title.clone()));
        recent_session.insert("status".to_string(), Value::String("interactive".to_string()));
        recent_session.insert("updated_at".to_string(), Value::String(updated_at.clone()));
        recent_session.insert(
            "message_count".to_string(),
            Value::Number((processed_command_count as u64 + 1).into()),
        );
        recent_session.insert("last_message_role".to_string(), Value::String("system".to_string()));
        recent_session.insert(
            "last_message_preview".to_string(),
            Value::String(format!(
                "{command_label} | mode={active_mode_label} | tool={active_tool_label} | frame={animation_frame}"
            )),
        );
    }
    if let Some(workspace) = ensure_array_object_at_index(runtime_snapshot, &["workspaces"], 0) {
        workspace.insert(
            "recent_session_title".to_string(),
            Value::String(recent_session_title),
        );
    }
    set_string_at_path(runtime_snapshot, &["updated_at"], updated_at);
}

fn build_next_intent_queue(runtime_snapshot: &Value, request: &RuntimeCommandRequest) -> Vec<Value> {
    let mut next_queue = Vec::new();
    next_queue.push(json!({
        "id": request.command_id.clone(),
        "label": if request.label.is_empty() { request.command_id.clone() } else { request.label.clone() },
        "reason": format!("Live command from {}", if request.surface.is_empty() { "native-host" } else { request.surface.as_str() }),
        "graph": if request.intent.is_empty() { request.command_id.clone() } else { request.intent.clone() },
        "debounce_ms": 0,
        "status": "queued",
    }));
    if let Some(existing_entries) = value_at_path(runtime_snapshot, &["dcc_suite_state", "intent_queue"])
        .and_then(Value::as_array)
    {
        for entry in existing_entries {
            if next_queue.len() >= 8 {
                break;
            }
            if entry
                .get("id")
                .and_then(Value::as_str)
                .is_some_and(|id| id == request.command_id.as_str())
            {
                continue;
            }
            next_queue.push(entry.clone());
        }
    }
    next_queue
}

fn intent_queue_ids(intent_queue: &[Value]) -> Vec<String> {
    intent_queue
        .iter()
        .filter_map(|entry| entry.get("id").and_then(Value::as_str))
        .map(|value| value.to_string())
        .collect()
}

fn ensure_mode_for_command(
    session_document: &mut Value,
    runtime_snapshot: &Value,
    command_id: &str,
) {
    let Some(preferred_mode) = preferred_mode_for_command(command_id) else {
        return;
    };
    let available_modes = get_string_vec_at_path(session_document, &["workspace", "available_modes"]);
    if !available_modes.iter().any(|mode| mode == preferred_mode) {
        return;
    }
    let current_mode = get_string_at_path(session_document, &["workspace", "active_mode"]).unwrap_or_default();
    if current_mode == preferred_mode {
        return;
    }
    set_string_at_path(session_document, &["workspace", "active_mode"], preferred_mode);
    if let Some(tool_id) = first_tool_for_lane(runtime_snapshot, preferred_mode) {
        apply_tool_defaults_from_snapshot(session_document, runtime_snapshot, &tool_id);
    }
}

fn preferred_mode_for_command(command_id: &str) -> Option<&'static str> {
    if command_id == "mesh.edit_topology" || command_id == "mesh.rebuild_topology" {
        Some("sculpt_model")
    } else if command_id.starts_with("project.")
        || command_id.starts_with("asset.")
        || command_id.starts_with("mesh.")
        || command_id.starts_with("selection.")
    {
        Some("scene_assembly")
    } else if command_id.starts_with("sculpt.") || command_id.starts_with("topology.") {
        Some("sculpt_model")
    } else if command_id.starts_with("material.") {
        Some("material_lookdev")
    } else if command_id.starts_with("rig.") {
        Some("rig_anim")
    } else if command_id.starts_with("sim.") {
        Some("sim_fx")
    } else if command_id.starts_with("render.") || command_id.starts_with("compositor.") {
        Some("render_comp")
    } else if command_id.starts_with("publish.") || command_id.starts_with("tensor.") {
        Some("publish_automation")
    } else {
        None
    }
}

fn apply_tool_defaults_from_snapshot(
    session_document: &mut Value,
    runtime_snapshot: &Value,
    tool_id: &str,
) {
    set_string_at_path(session_document, &["tooling", "active_tool"], tool_id);
    if let Some(tool_value) = find_array_object_by_field(
        runtime_snapshot,
        &["dcc_suite_state", "available_tools"],
        "id",
        tool_id,
    ) {
        if let Some(default_mode) = tool_value.get("default_gizmo_mode").and_then(Value::as_str) {
            set_string_at_path(session_document, &["gizmo", "mode"], default_mode);
        }
        if let Some(default_space) = tool_value.get("default_gizmo_space").and_then(Value::as_str) {
            set_string_at_path(session_document, &["gizmo", "space"], default_space);
        }
        if let Some(gizmo_enabled) = tool_value.get("gizmo_enabled").and_then(Value::as_bool) {
            set_bool_at_path(session_document, &["gizmo", "visible"], gizmo_enabled);
        }
    }
}

fn first_tool_for_lane(runtime_snapshot: &Value, lane: &str) -> Option<String> {
    value_at_path(runtime_snapshot, &["dcc_suite_state", "available_tools"])
        .and_then(Value::as_array)
        .and_then(|tools| {
            tools
                .iter()
                .find(|tool| tool.get("lane").and_then(Value::as_str) == Some(lane))
                .and_then(|tool| tool.get("id").and_then(Value::as_str))
                .map(|tool_id| tool_id.to_string())
        })
}

fn tool_exists(runtime_snapshot: &Value, tool_id: &str) -> bool {
    find_array_object_by_field(
        runtime_snapshot,
        &["dcc_suite_state", "available_tools"],
        "id",
        tool_id,
    )
    .is_some()
}

fn collect_tool_ids_for_lane(runtime_snapshot: &Value, lane: &str) -> Vec<String> {
    value_at_path(runtime_snapshot, &["dcc_suite_state", "available_tools"])
        .and_then(Value::as_array)
        .map(|tools| {
            tools
                .iter()
                .filter(|tool| tool.get("lane").and_then(Value::as_str) == Some(lane))
                .filter_map(|tool| tool.get("id").and_then(Value::as_str))
                .map(|tool_id| tool_id.to_string())
                .collect::<Vec<_>>()
        })
        .unwrap_or_default()
}

fn lookup_registry_label(value: &Value, path: &[&str], entry_id: &str) -> Option<String> {
    find_array_object_by_field(value, path, "id", entry_id)
        .and_then(|entry| entry.get("label"))
        .and_then(Value::as_str)
        .map(|label| label.to_string())
}

fn collect_registry_ids(value: &Value, path: &[&str], field: &str) -> Vec<String> {
    value_at_path(value, path)
        .and_then(Value::as_array)
        .map(|entries| {
            entries
                .iter()
                .filter_map(|entry| entry.get(field).and_then(Value::as_str))
                .map(|entry_id| entry_id.to_string())
                .collect::<Vec<_>>()
        })
        .unwrap_or_default()
}

fn cycle_string_value(values: &[String], current: &str) -> Option<String> {
    if values.is_empty() {
        return None;
    }
    if let Some(index) = values.iter().position(|value| value == current) {
        Some(values[(index + 1) % values.len()].clone())
    } else {
        Some(values[0].clone())
    }
}

fn set_all_dirty_flags(session_document: &mut Value, dirty_value: bool) {
    for dirty_key in [
        "asset_dirty",
        "sculpt_dirty",
        "topology_dirty",
        "material_dirty",
        "rig_dirty",
        "animation_dirty",
        "simulation_dirty",
        "render_dirty",
        "compositor_dirty",
        "publish_dirty",
        "tensor_dirty",
        "session_needs_save",
    ] {
        set_bool_at_path(session_document, &["dirty", dirty_key], dirty_value);
    }
}

fn ensure_parent_directory(path: &Path) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| err.to_string())?;
    }
    Ok(())
}

fn load_json_document(primary_path: &Path, mirror_paths: &[PathBuf]) -> Value {
    for candidate_path in std::iter::once(primary_path).chain(mirror_paths.iter().map(PathBuf::as_path)) {
        if let Ok(contents) = fs::read_to_string(candidate_path) {
            if let Ok(document) = serde_json::from_str::<Value>(&contents) {
                return document;
            }
        }
    }
    json!({})
}

fn write_json_document(
    primary_path: &Path,
    mirror_paths: &[PathBuf],
    document: &Value,
) -> Result<(), String> {
    let document_json = serde_json::to_string_pretty(document).map_err(|err| err.to_string())?;
    let mut write_targets = BTreeSet::new();
    write_targets.insert(primary_path.to_path_buf());
    for mirror_path in mirror_paths {
        write_targets.insert(mirror_path.clone());
    }
    for target_path in write_targets {
        ensure_parent_directory(&target_path)?;
        fs::write(target_path, &document_json).map_err(|err| err.to_string())?;
    }
    Ok(())
}

fn now_iso_string() -> String {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
        .to_string()
}

fn value_at_path<'a>(value: &'a Value, path: &[&str]) -> Option<&'a Value> {
    let mut current = value;
    for key in path {
        current = current.get(*key)?;
    }
    Some(current)
}

fn ensure_object_value(value: &mut Value) -> &mut serde_json::Map<String, Value> {
    if !value.is_object() {
        *value = json!({});
    }
    value.as_object_mut().expect("object value")
}

fn ensure_array_value(value: &mut Value) -> &mut Vec<Value> {
    if !value.is_array() {
        *value = Value::Array(Vec::new());
    }
    value.as_array_mut().expect("array value")
}

fn set_value_at_path(value: &mut Value, path: &[&str], next_value: Value) {
    if path.is_empty() {
        *value = next_value;
        return;
    }
    let mut current = value;
    for key in &path[..path.len() - 1] {
        let object = ensure_object_value(current);
        current = object.entry((*key).to_string()).or_insert_with(|| json!({}));
    }
    let object = ensure_object_value(current);
    object.insert(path[path.len() - 1].to_string(), next_value);
}

fn set_string_at_path(value: &mut Value, path: &[&str], next_value: impl Into<String>) {
    set_value_at_path(value, path, Value::String(next_value.into()));
}

fn set_bool_at_path(value: &mut Value, path: &[&str], next_value: bool) {
    set_value_at_path(value, path, Value::Bool(next_value));
}

fn set_i64_at_path(value: &mut Value, path: &[&str], next_value: i64) {
    set_value_at_path(value, path, Value::Number(next_value.into()));
}

fn increment_i64_at_path(value: &mut Value, path: &[&str], delta: i64) -> i64 {
    let next_value = get_i64_at_path(value, path).unwrap_or(0) + delta;
    set_i64_at_path(value, path, next_value);
    next_value
}

fn set_string_array_at_path(value: &mut Value, path: &[&str], values: &[String]) {
    set_value_at_path(
        value,
        path,
        Value::Array(values.iter().cloned().map(Value::String).collect()),
    );
}

fn get_string_at_path(value: &Value, path: &[&str]) -> Option<String> {
    value_at_path(value, path)
        .and_then(Value::as_str)
        .map(|value| value.to_string())
}

fn get_bool_at_path(value: &Value, path: &[&str]) -> Option<bool> {
    value_at_path(value, path).and_then(Value::as_bool)
}

fn get_i64_at_path(value: &Value, path: &[&str]) -> Option<i64> {
    value_at_path(value, path).and_then(Value::as_i64)
}

fn get_string_vec_at_path(value: &Value, path: &[&str]) -> Vec<String> {
    value_at_path(value, path)
        .and_then(Value::as_array)
        .map(|values| {
            values
                .iter()
                .filter_map(Value::as_str)
                .map(|value| value.to_string())
                .collect::<Vec<_>>()
        })
        .unwrap_or_default()
}

fn find_array_object_by_field<'a>(
    value: &'a Value,
    path: &[&str],
    field_name: &str,
    wanted_value: &str,
) -> Option<&'a Value> {
    value_at_path(value, path)
        .and_then(Value::as_array)
        .and_then(|entries| {
            entries.iter().find(|entry| {
                entry.get(field_name).and_then(Value::as_str) == Some(wanted_value)
            })
        })
}

fn ensure_array_object_at_index<'a>(
    value: &'a mut Value,
    path: &[&str],
    index: usize,
) -> Option<&'a mut serde_json::Map<String, Value>> {
    let mut current = value;
    for key in path {
        let object = ensure_object_value(current);
        current = object
            .entry((*key).to_string())
            .or_insert_with(|| Value::Array(Vec::new()));
    }
    let array = ensure_array_value(current);
    while array.len() <= index {
        array.push(json!({}));
    }
    array.get_mut(index).map(ensure_object_value)
}


