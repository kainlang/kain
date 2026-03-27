use std::{
    collections::BTreeSet,
    fs,
    path::{Path, PathBuf},
    thread,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use serde_json::{json, Value};

const BRIDGE_POLL_INTERVAL_MS: u64 = 150;

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
) {
    ensure_mode_for_command(session_document, runtime_snapshot, &request.command_id);

    match request.command_id.as_str() {
        "runtime.reload" => {}
        "project.bootstrap" => apply_project_bootstrap(session_document, runtime_snapshot),
        "workspace.switch_mode" => apply_workspace_switch(session_document, runtime_snapshot),
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
        "render.preview" => apply_render_preview(session_document, runtime_snapshot),
        "compositor.rebuild" => apply_compositor_rebuild(session_document, runtime_snapshot),
        "publish.package" => apply_publish_package(session_document, runtime_snapshot),
        "tensor.train_step" => apply_tensor_train_step(session_document),
        "tensor.infer_step" => apply_tensor_infer_step(session_document),
        _ => set_bool_at_path(
            session_document,
            &["dirty", "session_needs_save"],
            true,
        ),
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

fn apply_selection_change(session_document: &mut Value, processed_command_count: usize) {
    set_string_array_at_path(
        session_document,
        &["selection", "entity_ids"],
        &[format!("entity/bridge_selection_{processed_command_count:04}")],
    );
    set_string_array_at_path(session_document, &["selection", "subobject_ids"], &[]);
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
            "command_id": request.command_id,
            "label": command_label,
            "intent": request.intent,
            "surface": request.surface,
            "source": request.source,
            "requested_at": if request.requested_at.is_empty() { updated_at.clone() } else { request.requested_at.clone() },
            "processed_at": updated_at.clone(),
        }),
    );
    set_value_at_path(
        runtime_snapshot,
        &["dcc_suite_state", "intent_queue"],
        Value::Array(next_intent_queue.clone()),
    );
    set_string_array_at_path(session_document, &["jobs", "active_intents"], &intent_queue_ids(&next_intent_queue));

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
