#!/usr/bin/env python3

from __future__ import annotations

import json
from collections import OrderedDict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


APP_ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = APP_ROOT.parent.parent
STATE_ROOT = APP_ROOT / "state"
NATIVE_APP_STATE_ROOT = APP_ROOT / "native-app" / "state"
REPORT_ROOT = APP_ROOT / ".kain" / "fabric" / "reports"

WORKSPACE_LANES = [
    "scene_assembly",
    "sculpt_model",
    "material_lookdev",
    "rig_anim",
    "sim_fx",
    "render_comp",
    "publish_automation",
]

WORKSPACE_DISPLAY_BY_LANE = OrderedDict(
    [
        ("scene_assembly", "Layout"),
        ("sculpt_model", "Model / Sculpt"),
        ("material_lookdev", "Paint / Lookdev"),
        ("rig_anim", "Rig / Animate"),
        ("sim_fx", "Sim"),
        ("render_comp", "Render / Comp"),
        ("publish_automation", "Publish"),
    ]
)


def load_json(relative_path: str) -> Any:
    return json.loads((APP_ROOT / relative_path).read_text(encoding="utf-8"))


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def latest_fabric_report() -> dict[str, Any] | None:
    report_paths = sorted(REPORT_ROOT.rglob("report.json"), key=lambda path: path.stat().st_mtime, reverse=True)
    if not report_paths:
        return None
    return json.loads(report_paths[0].read_text(encoding="utf-8"))


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def new_required_capability_tools(capabilities: list[str]) -> list[dict[str, Any]]:
    tools: list[dict[str, Any]] = []
    for capability in capabilities:
        tool_id = "".join(character if character.isalnum() else "_" for character in capability).lower()
        tools.append(
            OrderedDict(
                [
                    ("id", tool_id),
                    ("label", capability),
                    ("capability", capability),
                    ("approval", "workspace"),
                    ("decision", None),
                    ("scope_decisions", []),
                ]
            )
        )
    return tools


def new_command_snapshots(commands: list[dict[str, Any]]) -> list[dict[str, Any]]:
    snapshots: list[dict[str, Any]] = [
        OrderedDict(
            [
                ("id", "runtime.reload"),
                ("label", "Reload Runtime"),
                ("surface", "titlebar"),
                ("intent", "runtime.reload"),
            ]
        )
    ]
    snapshots.extend(
        OrderedDict(
            [
                ("id", command["id"]),
                ("label", command["label"]),
                ("surface", command["surface"]),
                ("intent", command["intent"]),
            ]
        )
        for command in commands
    )
    return snapshots


def recent_session_preview(fabric_status: str, mode_label: str) -> str:
    return f"DCC suite bridge ready | fabric={fabric_status} | mode={mode_label}"


def viewport_mode_for_workspace_mode(workspace_mode: str, viewport_modes: list[dict[str, Any]]) -> dict[str, Any]:
    mode_by_id = {mode["id"]: mode for mode in viewport_modes}
    if workspace_mode == "sculpt_model":
        return mode_by_id.get("model", mode_by_id.get("layout", {}))
    if workspace_mode == "material_lookdev":
        return mode_by_id.get("lookdev", mode_by_id.get("layout", {}))
    if workspace_mode == "render_comp":
        return mode_by_id.get("render", mode_by_id.get("layout", {}))
    return mode_by_id.get("layout", {})


def mode_registry_entries(viewport_modes: list[dict[str, Any]]) -> list[str]:
    if not viewport_modes:
        return [
            "Layout [layout] => overlay_policy/layout_clear / tool_policy/layout_nav_first / view_profile/layout_blocking",
            "Model [model] => overlay_policy/model_topology / tool_policy/model_edit_first / view_profile/model_topology",
            "Sculpt [sculpt] => overlay_policy/sculpt_brush / tool_policy/sculpt_brush_first / view_profile/sculpt_surface",
            "Paint [paint] => overlay_policy/paint_layers / tool_policy/paint_layer_first / view_profile/paint_authoring",
            "Lookdev [lookdev] => overlay_policy/lookdev_balanced / tool_policy/lookdev_eval / view_profile/lookdev_balanced",
            "Render [render] => overlay_policy/render_review / tool_policy/render_review / view_profile/render_room",
        ]
    return [
        f"{mode['label']} [{mode['id']}] => {mode['overlay_policy_id']} / {mode['tool_policy_id']} / {mode['view_profile_id']}"
        for mode in viewport_modes
    ]


def main() -> None:
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    NATIVE_APP_STATE_ROOT.mkdir(parents=True, exist_ok=True)

    manifest = load_json("config/app_manifest.json")
    modes = load_json("config/workspace_modes.json")["modes"]
    surfaces = load_json("config/surfaces.json")["surfaces"]
    tools = load_json("config/tool_catalog.json")["tools"]
    commands = load_json("config/command_registry.json")["commands"]
    intents = load_json("config/fabric_intents.json")["intents"]
    pipeline = load_json("config/fabric_pipeline.json")["steps"]
    runtime_packs = load_json("config/runtime_packs.json")["runtime_packs"]
    runtime_lanes = load_json("config/runtime_lanes.json")["runtime_lanes"]
    viewport_modes = load_json("config/viewport_modes.json")["modes"]
    resources = load_json("config/resource_kinds.json")["resource_kinds"]
    mesh_contract = load_json("config/mesh_resource_contract.json")
    reports = load_json("config/report_kinds.json")["report_kinds"]
    jobs = load_json("config/automation_jobs.json")["jobs"]
    gizmo_registry = load_json("config/gizmo_registry.json")
    ui_shell = load_json("config/ui_shell.json")
    asset_pipeline = load_json("config/asset_pipeline_manifest.json")["asset_pipeline"]

    latest_report = latest_fabric_report()
    latest_fabric_status = latest_report["status"] if latest_report else "idle"
    now = now_iso()
    session_id = latest_report["session_id"] if latest_report else "dcc-suite-session-bootstrap"

    default_mode_id = "scene_assembly"
    active_mode_id = default_mode_id
    active_mode_label = WORKSPACE_DISPLAY_BY_LANE.get(active_mode_id, active_mode_id)
    recent_session_title = f"{manifest['name']} | {active_mode_label}"
    active_viewport_mode = viewport_mode_for_workspace_mode(active_mode_id, viewport_modes)
    viewport_registry_entries = mode_registry_entries(viewport_modes)

    command_summary = " | ".join(f"{command['label']} [{command['id']}]" for command in commands) or "n/a"
    intent_summary = " | ".join(f"{intent['label']} [{intent['id']}]" for intent in intents) or "n/a"
    runtime_pack_summary = " | ".join(f"{pack['label']} [{pack['id']}]" for pack in runtime_packs) or "n/a"
    runtime_lane_summary = " | ".join(lane["runtime"] for lane in runtime_lanes) or "n/a"
    runtime_lane_registry_summary = (
        " | ".join(f"{lane['label']} [{lane['runtime']}]" for lane in runtime_lanes)
        or "Kain Semantics Lane [kain] | Fabric Orchestration Lane [fabric] | Python Bootstrap Lane [python] | GPU Compute Lane [gpu_compute] | Native C ABI Lane [c_abi] | Rust Analysis Lane [rust_crate] | Node Bridge Lane [node_bridge]"
    )
    power_lane_summary = runtime_lane_registry_summary

    initial_intent_queue: list[dict[str, Any]] = []
    if latest_fabric_status == "idle":
        initial_intent_queue.append(
            OrderedDict(
                [
                    ("id", "project.bootstrap"),
                    ("label", "Bootstrap Suite"),
                    ("reason", "No Fabric report exists yet."),
                    ("graph", "fabric/intents/bootstrap.fabric.toml"),
                    ("debounce_ms", 0),
                    ("status", "recommended"),
                ]
            )
        )

    viewport_frame_feedback = (
        "frame steady / preview responsive" if latest_fabric_status == "succeeded" else "frame warming / preview stabilizing"
    )

    session_document = OrderedDict(
        [
            (
                "project",
                OrderedDict(
                    [
                        ("project_id", "fabric-dcc-suite"),
                        ("project_name", manifest["name"]),
                        ("schema_version", 1),
                    ]
                ),
            ),
            (
                "workspace",
                OrderedDict(
                    [
                        ("active_mode", active_mode_id),
                        ("layout_id", manifest["layout_id"]),
                        ("available_modes", WORKSPACE_LANES),
                    ]
                ),
            ),
            (
                "runtime_lane_registry_entries",
                runtime_lanes,
            ),
            (
                "power_lane_registry_entries",
                runtime_lanes,
            ),
            ("runtime_lane_count", len(runtime_lanes)),
            ("power_lane_count", len(runtime_lanes)),
            ("runtime_lane_summary", runtime_lane_summary),
            ("power_lane_summary", power_lane_summary),
            ("runtime_lane_registry_summary", runtime_lane_registry_summary),
            ("power_lane_registry_summary", power_lane_summary),
            ("runtime_pack_registry_entries", runtime_packs),
            ("runtime_pack_count", len(runtime_packs)),
            ("runtime_pack_summary", runtime_pack_summary),
            ("fabric_intent_registry_entries", intents),
            ("fabric_intent_count", len(intents)),
            ("fabric_intent_summary", intent_summary),
            ("command_registry", commands),
            ("command_registry_entries", commands),
            ("command_count", len(commands)),
            ("command_summary", command_summary),
            (
                "viewport",
                OrderedDict(
                    [
                        ("active_mode", active_viewport_mode.get("id", "layout")),
                        ("overlay_policy_id", active_viewport_mode.get("overlay_policy_id", "overlay_policy/layout_clear")),
                        ("tool_policy_id", active_viewport_mode.get("tool_policy_id", "tool_policy/layout_nav_first")),
                        ("view_profile_id", active_viewport_mode.get("view_profile_id", "view_profile/layout_blocking")),
                        ("hud_density", active_viewport_mode.get("hud_density", "light")),
                        ("mode_count", len(viewport_modes)),
                        ("mode_summary", " | ".join(mode["id"] for mode in viewport_modes)),
                        (
                            "mode_registry_summary",
                            " | ".join(f"{mode['id']} => {mode['overlay_policy_id']}" for mode in viewport_modes),
                        ),
                        ("mode_registry_entries", viewport_registry_entries),
                    ]
                ),
            ),
            (
                "workbench",
                OrderedDict(
                    [
                        ("active_workbench_id", active_mode_id),
                        ("active_tab_group_id", ui_shell["page_tab_group_id"]),
                        ("active_dock_id", "dcc_workbench_pages"),
                        ("active_pane_id", "pane/viewport_stage"),
                        ("last_materialized_shell_path", "generated/main.generated.kn"),
                        ("last_runtime_snapshot_path", "state/runtime_snapshot.json"),
                        (
                            "summary",
                            f"{active_mode_id}:tabs={ui_shell['page_tab_group_id']}:dock=dcc_workbench_pages:pane=pane/viewport_stage",
                        ),
                    ]
                ),
            ),
            (
                "context",
                OrderedDict(
                    [
                        ("active_workspace_id", f"workspace/{active_mode_id}"),
                        ("active_pane_id", "pane/viewport_stage"),
                        ("active_tool_id", "select"),
                        ("active_object_id", "entity/blender_startup_cube"),
                        ("active_edit_target_id", "entity/blender_startup_cube"),
                        ("active_material_id", "material/hero_surface"),
                        ("active_texture_set_id", "textureset/hero_body_udim1001"),
                        ("active_graph_node_id", "graph/lookdev_primary"),
                        ("active_frame", 96),
                        ("active_viewport_mode", active_viewport_mode.get("id", "layout")),
                    ]
                ),
            ),
            (
                "tooling",
                OrderedDict(
                    [
                        ("active_tool", "select"),
                        ("brush_radius", 42),
                        ("brush_strength_percent", 68),
                        ("brush_falloff_profile", "surface_tight"),
                        ("brush_surface_policy", "deformation_aware"),
                        ("uv_policy_id", "uv_policy/udim_tiled_islands"),
                    ]
                ),
            ),
            (
                "gizmo",
                OrderedDict(
                    [
                        ("active_profile_id", "dcc_transform_universal"),
                        ("mode", "translate"),
                        ("space", "world"),
                        ("snap_enabled", False),
                        ("visible", True),
                        ("drag_trigger", "ctrl_primary_drag"),
                    ]
                ),
            ),
            ("selection", OrderedDict([("entity_ids", ["entity/blender_startup_cube"]), ("subobject_ids", [])])),
            (
                "scene",
                OrderedDict(
                    [
                        ("active_document_id", "scene/dcc_suite_startup"),
                        ("active_collection_id", "collection/startup_stage"),
                        ("active_variant", "lookdev"),
                    ]
                ),
            ),
            (
                "mesh",
                OrderedDict(
                    [
                        ("active_document_id", "mesh/dcc_suite_startup_cube"),
                        ("active_edit_target_id", "entity/blender_startup_cube"),
                        ("mesh_authoring_policy_id", "mesh_authoring_policy/startup_hybrid"),
                        ("active_primitive_template_id", "primitive/cube"),
                        ("topology_edit_mode", "object"),
                    ]
                ),
            ),
            (
                "topology_history",
                OrderedDict(
                    [
                        ("history_document_id", "topology_history_mesh_document"),
                        ("history_document_uri", "mesh://topology/history/current"),
                        ("last_lineage_reason", "bootstrap"),
                        ("last_upstream_topology_report", "state/topology_history_report.json"),
                        ("last_active_edit_target_id", "entity/blender_startup_cube"),
                        ("last_topology_output_id", "topology_output_mesh_document"),
                    ]
                ),
            ),
            ("ingest", OrderedDict([("last_package_uri", "asset://starter/kitbash_hangar"), ("last_package_kind", "gltf"), ("staged_package_count", 1)])),
            (
                "asset_pipeline",
                OrderedDict(
                    [
                        ("source_id", asset_pipeline["source_id"]),
                        ("session_route_scope", asset_pipeline["session_route_scope"]),
                        ("source_priority", asset_pipeline["source_priority"]),
                        ("transcode_profiles", asset_pipeline["supported_transcode_profiles"]),
                        ("routed_runtime_ids", asset_pipeline["routed_runtime_ids"]),
                        ("lineage_receipts", asset_pipeline["lineage_receipts"]),
                        (
                            "registry_entries",
                            [
                                asset_pipeline["source_id"],
                                asset_pipeline["lane"],
                                asset_pipeline["session_route_scope"],
                            ],
                        ),
                        ("summary", asset_pipeline["summary"]),
                    ]
                ),
            ),
            ("asset_ingest_summary", "gltf intake ready / 1 staged package"),
            ("asset_ingest_status", "intake ready"),
            ("asset_ingest_count", 1),
            (
                "materials",
                OrderedDict(
                    [
                        ("active_material_id", "material/hero_surface"),
                        ("active_graph_id", "graph/lookdev_primary"),
                        ("active_texture_set_id", "textureset/hero_body_udim1001"),
                        ("active_layer_stack_id", "layerstack/hero_surface_primary"),
                        ("active_svg_document_id", "svg/masks/hero_surface_primary"),
                        ("active_bake_preset_id", "bake/high_precision_curvature"),
                        ("active_export_preset_id", "export/metalrough_orm_painter"),
                        ("preview_profile", "material_preview_balanced"),
                        ("channel_profile", "basecolor_normal_roughness_metallic_ao_height_emissive"),
                        ("layer_response_profile", "paint_fill_filter_smart_stack"),
                        ("brush_response_profile", "deformation_aware_brush_stack"),
                        ("smart_mask_profile", "anchor_driven_wear_and_trim_masks"),
                        ("scan_ingest_profile", "sampler_style_scan_plate_ingest"),
                        ("uv_policy_id", "uv_policy/udim_tiled_islands"),
                        ("texture_set_receipt_id", "receipt/material_texture_set_current"),
                        ("export_receipt_id", "receipt/material_texture_export_current"),
                        ("deformation_surface_id", "surface/hero_deformation_shell"),
                        ("paint_resolution", 4096),
                        ("texel_density_target", 256),
                    ]
                ),
            ),
            (
                "rig",
                OrderedDict(
                    [
                        ("active_rig_id", "rig/hero_body"),
                        ("active_control_set", "controls/anim_main"),
                        ("deformation_profile", "hero_deformation_preview"),
                    ]
                ),
            ),
            ("animation", OrderedDict([("active_clip_id", "clip/blocking_pass_a"), ("frame", 96), ("playback_mode", "paused")])),
            (
                "simulation",
                OrderedDict(
                    [
                        ("cache_profile", "sim_preview_cache"),
                        ("last_tick_frame", 96),
                        ("solver_profile", "cloth_preview"),
                    ]
                ),
            ),
            (
                "evaluation",
                OrderedDict(
                    [
                        ("graph_document_id", "graph/dcc_suite_evaluation"),
                        ("graph_document_uri", "graph://evaluation/current"),
                        ("last_recompute_reason", "bootstrap"),
                        ("last_upstream_dependency_report", "report://fabric/latest"),
                        ("last_cook_output_id", "cook/dcc_suite_primary"),
                    ]
                ),
            ),
            (
                "cache",
                OrderedDict(
                    [
                        ("cache_graph_id", "cache/dcc_suite"),
                        ("cache_graph_uri", "cache://graph/current"),
                        ("last_materialization_reason", "bootstrap"),
                        ("last_materialized_resource_id", "resource/cache/preview"),
                        ("last_cache_report_path", "state/cache_materialize_report.json"),
                    ]
                ),
            ),
            (
                "render",
                OrderedDict(
                    [
                        ("camera_id", "camera/startup_authoring"),
                        ("view_transform", "acescg"),
                        ("render_profile", "viewport_quality"),
                        ("lighting_profile_id", "lighting/default_review"),
                        ("aov_set", "beauty_plus_utility"),
                        ("accumulation_profile", "progressive_preview"),
                        ("denoise_profile", "viewport_temporal_denoise"),
                        ("review_capture_profile", "frame_review_pack"),
                    ]
                ),
            ),
            ("viewport_frame_feedback", viewport_frame_feedback),
            ("compositor", OrderedDict([("active_stack_id", "comp/final_review"), ("last_rebuild_reason", "bootstrap")])),
            (
                "publish",
                OrderedDict(
                    [
                        ("profile_id", "publish/review_daily"),
                        ("target_bundle", "bundle/hero_scene_daily"),
                        ("delivery_channel", "studio_review"),
                    ]
                ),
            ),
            (
                "automation",
                OrderedDict(
                    [
                        ("enabled", True),
                        ("active_job_ids", ["thumbnail_refresh", "nightly_material_rebake", "svg_mask_cache_rebuild"]),
                        ("last_audit_status", "pending"),
                    ]
                ),
            ),
            (
                "reports",
                OrderedDict(
                    [
                        ("mesh_contract_report_id", "mesh_contract_report"),
                        ("mesh_contract_report_uri", "report://mesh/contract"),
                        ("mesh_contract_report_path", "state/mesh_contract_report.json"),
                        ("topology_history_report_id", "topology_history_report"),
                        ("topology_history_report_uri", "report://topology/history"),
                        ("topology_history_report_path", "state/topology_history_report.json"),
                    ]
                ),
            ),
            ("runtime_lane_registry", runtime_lanes),
            ("power_lane_registry", runtime_lanes),
            (
                "assist",
                OrderedDict(
                    [
                        ("context_report_id", "assist_context_report"),
                        ("context_report_uri", "report://assist/context"),
                        ("context_report_path", "state/assist_context_report.json"),
                        ("suggestion_report_id", "assist_suggestion_report"),
                        ("suggestion_report_uri", "report://assist/suggestion"),
                        ("suggestion_report_path", "state/assist_suggestion_report.json"),
                        ("tensor_artifact_id", "assist_tensor_artifact"),
                        ("tensor_artifact_uri", "artifact://assist/tensor-context"),
                        ("tensor_artifact_path", "state/assist_tensor_context.json"),
                        ("assistant_profile", "contextual_tensor_aware"),
                        ("context_summary", f"mode={active_mode_label} | layout={manifest['layout_id']} | tensor=warm"),
                        ("suggestion_summary", "watch selection, mode, and tensor dirty state together"),
                        ("workspace_layout_hint", manifest["layout_id"]),
                    ]
                ),
            ),
            (
                "dirty",
                OrderedDict(
                    [
                        ("asset_dirty", False),
                        ("sculpt_dirty", False),
                        ("topology_dirty", False),
                        ("material_dirty", False),
                        ("rig_dirty", False),
                        ("animation_dirty", False),
                        ("simulation_dirty", False),
                        ("render_dirty", latest_fabric_status != "succeeded"),
                        ("compositor_dirty", False),
                        ("publish_dirty", False),
                        ("tensor_dirty", False),
                        ("session_needs_save", False),
                    ]
                ),
            ),
            (
                "jobs",
                OrderedDict(
                    [
                        ("latest_fabric_session_id", session_id),
                        ("latest_fabric_status", latest_fabric_status),
                        ("active_intents", [intent["id"] for intent in initial_intent_queue]),
                        ("active_jobs", []),
                    ]
                ),
            ),
        ]
    )

    bridge_command_queue_path = str(STATE_ROOT / "command_queue.jsonl")
    bridge_session_path = str(STATE_ROOT / "session_document.json")
    bridge_status = OrderedDict(
        [
            ("status", "ready"),
            ("command_queue_path", bridge_command_queue_path),
            ("session_document_path", bridge_session_path),
            ("processed_command_count", 0),
        ]
    )

    if bridge_status["status"] == "live" and latest_fabric_status == "succeeded":
        runtime_lane_health = "healthy"
        runtime_lane_health_detail = "bridge live / fabric succeeded"
    elif bridge_status["status"] == "live":
        runtime_lane_health = "bridge-live"
        runtime_lane_health_detail = "bridge live / fabric waiting"
    elif latest_fabric_status == "succeeded":
        runtime_lane_health = "fabric-green"
        runtime_lane_health_detail = "fabric succeeded / bridge warming"
    else:
        runtime_lane_health = "warming"
        runtime_lane_health_detail = "bridge warming / fabric warming"

    step_status = [
        OrderedDict(
            [
                ("id", step["id"]),
                ("runtime", step["runtime"]),
                ("status", "pending" if latest_fabric_status == "idle" else latest_fabric_status),
                ("summary", step["summary"]),
            ]
        )
        for step in pipeline
    ]

    runtime_snapshot = OrderedDict(
        [
            ("app_id", manifest["app_id"]),
            ("name", manifest["name"]),
            ("version", manifest["version"]),
            ("window_title", manifest["window_title"]),
            ("root_component", manifest["root_component"]),
            ("layout_id", manifest["layout_id"]),
            ("required_runtime_capabilities", manifest["required_runtime_capabilities"]),
            (
                "panels",
                [
                    OrderedDict(
                        [
                            ("id", surface["id"]),
                            ("title", surface["title"]),
                            ("dock", surface["dock"]),
                            ("kind", surface["kind"]),
                        ]
                    )
                    for surface in surfaces
                ],
            ),
            ("commands", new_command_snapshots(commands)),
            ("runtime_lane_registry", runtime_lanes),
            ("power_lane_registry", runtime_lanes),
            (
                "providers",
                [
                    OrderedDict(
                        [
                            ("id", "native_runtime"),
                            ("label", "Native Runtime"),
                            ("transport", "in-process"),
                            ("profile_kind", "native-ui"),
                            ("supports_tools", True),
                            ("supports_streaming", False),
                            ("active", True),
                            ("profile_configured", True),
                            ("profile_keys", []),
                        ]
                    )
                ],
            ),
            ("tools", new_required_capability_tools(manifest["required_runtime_capabilities"])),
            (
                "sessions",
                OrderedDict(
                    [
                        ("total_sessions", 1),
                        ("active_provider", "native_runtime"),
                        ("recent_session_id", session_id),
                        ("recent_session_title", recent_session_title),
                    ]
                ),
            ),
            (
                "recent_sessions",
                [
                    OrderedDict(
                        [
                            ("id", session_id),
                            ("title", recent_session_title),
                            ("provider_id", "native_runtime"),
                            ("status", latest_fabric_status),
                            ("workspace_root", str(REPO_ROOT)),
                            ("updated_at", now),
                            ("message_count", 1),
                            ("last_message_role", "system"),
                            ("last_message_preview", recent_session_preview(latest_fabric_status, active_mode_label)),
                        ]
                    )
                ],
            ),
            (
                "workspaces",
                [
                    OrderedDict(
                        [
                            ("root", str(REPO_ROOT)),
                            ("session_count", 1),
                            ("recent_session_title", recent_session_title),
                        ]
                    )
                ],
            ),
            (
                "dcc_suite_state",
                OrderedDict(
                    [
                        ("schema_version", 1),
                        (
                            "manifest_registry",
                            OrderedDict(
                                [
                                    ("app_manifest", "config/app_manifest.json"),
                                    ("workspace_modes", "config/workspace_modes.json"),
                                    ("surfaces", "config/surfaces.json"),
                                    ("tool_catalog", "config/tool_catalog.json"),
                                    ("command_registry", "config/command_registry.json"),
                                    ("fabric_pipeline", "config/fabric_pipeline.json"),
                                    ("fabric_intents", "config/fabric_intents.json"),
                                    ("resource_kinds", "config/resource_kinds.json"),
                                    ("mesh_resource_contract", "config/mesh_resource_contract.json"),
                                    ("report_kinds", "config/report_kinds.json"),
                                    ("runtime_packs", "config/runtime_packs.json"),
                                    ("automation_jobs", "config/automation_jobs.json"),
                                    ("gizmo_registry", "config/gizmo_registry.json"),
                                    ("session_schema", "session/session_schema.kn"),
                                    ("session_reducers", "session/reducers.kn"),
                                    ("session_intent_planner", "session/intent_planner.kn"),
                                ]
                            ),
                        ),
                        ("command_registry", commands),
                        ("available_tools", tools),
                        ("workspace_modes", modes),
                        ("surface_registry", surfaces),
                        ("runtime_packs", runtime_packs),
                        ("gizmo_profiles", gizmo_registry["profiles"]),
                        ("viewport_gizmo_bindings", gizmo_registry["viewport_bindings"]),
                        ("resource_store", resources),
                        (
                            "mesh_contract",
                            OrderedDict(
                                [
                                    ("schema_version", mesh_contract["schema_version"]),
                                    ("mesh_documents", mesh_contract["mesh_documents"]),
                                    ("semantic_rules", mesh_contract["semantic_rules"]),
                                ]
                            ),
                        ),
                        ("report_store", reports),
                        ("automation_jobs", jobs),
                        ("intent_queue", initial_intent_queue),
                        ("latest_command", None),
                        (
                            "latest_fabric_run",
                            OrderedDict(
                                [
                                    ("session_id", latest_report["session_id"] if latest_report else None),
                                    ("status", latest_fabric_status),
                                    ("manifest_path", "apps/kain-fabric-dcc-suite/KAIN.fabric.toml"),
                                    ("steps", step_status),
                                ]
                            ),
                        ),
                        ("bridge", bridge_status),
                        ("bridge_status", bridge_status["status"]),
                        (
                            "workbench",
                            OrderedDict(
                                [
                                    ("active_workbench_id", active_mode_id),
                                    ("active_tab_group_id", ui_shell["page_tab_group_id"]),
                                    ("active_dock_id", "dcc_workbench_pages"),
                                    ("active_pane_id", "pane/viewport_stage"),
                                    ("materialized_shell_path", "generated/main.generated.kn"),
                                    ("runtime_snapshot_path", "state/runtime_snapshot.json"),
                                ]
                            ),
                        ),
                        ("runtime_lane_health", runtime_lane_health),
                        ("runtime_lane_health_detail", runtime_lane_health_detail),
                        ("runtime_lane_summary", runtime_lane_summary),
                        ("runtime_lane_registry_summary", runtime_lane_registry_summary),
                        ("power_lane_registry_summary", power_lane_summary),
                        ("runtime_pack_registry_entries", runtime_packs),
                        ("runtime_pack_count", len(runtime_packs)),
                        ("runtime_pack_summary", runtime_pack_summary),
                        ("fabric_intent_registry", intents),
                        ("fabric_intent_registry_entries", intents),
                        ("fabric_intent_count", len(intents)),
                        ("fabric_intent_summary", intent_summary),
                        ("viewport_mode_count", len(viewport_modes)),
                        ("viewport_mode_summary", " | ".join(mode["id"] for mode in viewport_modes)),
                        (
                            "viewport_mode_registry_summary",
                            " | ".join(f"{mode['id']} => {mode['overlay_policy_id']}" for mode in viewport_modes),
                        ),
                        ("viewport_mode_registry_entries", viewport_registry_entries),
                        ("render_preview_chain", "pathtrace -> accumulation -> denoise"),
                        ("viewport_frame_feedback", viewport_frame_feedback),
                        (
                            "extension_seams",
                            [
                                "material lane still projects authoring receipts rather than a true native painter runtime",
                                "tensor lane still reports readiness and plan state rather than executing a full typed tensor artifact contract",
                                "simulation lane still materializes plan-oriented reports rather than a true solver runtime",
                                "compositor lane still materializes rebuild plans rather than a first-class compositor graph runtime",
                            ],
                        ),
                    ]
                ),
            ),
            ("updated_at", now),
        ]
    )

    minimal_reports = {
        "mesh_contract_report.json": OrderedDict(
            [
                ("id", "mesh_contract_report"),
                ("report_uri", "report://mesh/contract"),
                ("summary", "Linux bootstrap materialized the mesh contract placeholder."),
                ("updated_at", now),
            ]
        ),
        "topology_history_report.json": OrderedDict(
            [
                ("id", "topology_history_report"),
                ("report_uri", "report://topology/history"),
                ("summary", "Linux bootstrap materialized the topology history placeholder."),
                ("updated_at", now),
            ]
        ),
        "assist_context_report.json": OrderedDict(
            [
                ("id", "assist_context_report"),
                ("summary", "Context scaffold materialized for the Linux runtime bootstrap."),
                ("updated_at", now),
            ]
        ),
        "assist_suggestion_report.json": OrderedDict(
            [
                ("id", "assist_suggestion_report"),
                ("summary", "Suggestions are scaffolded until the Fabric lane is healthy again."),
                ("updated_at", now),
            ]
        ),
        "assist_tensor_context.json": OrderedDict(
            [
                ("id", "assist_tensor_artifact"),
                ("summary", "Tensor context placeholder for native shell bring-up."),
                ("updated_at", now),
            ]
        ),
    }

    write_json(STATE_ROOT / "runtime_snapshot.json", runtime_snapshot)
    write_json(NATIVE_APP_STATE_ROOT / "runtime_snapshot.json", runtime_snapshot)
    write_json(STATE_ROOT / "session_document.json", session_document)
    write_json(NATIVE_APP_STATE_ROOT / "session_document.json", session_document)

    for file_name, payload in minimal_reports.items():
        write_json(STATE_ROOT / file_name, payload)
        write_json(NATIVE_APP_STATE_ROOT / file_name, payload)

    (STATE_ROOT / "command_queue.jsonl").write_text("", encoding="utf-8")
    (NATIVE_APP_STATE_ROOT / "command_queue.jsonl").write_text("", encoding="utf-8")

    print(f"Materialized {STATE_ROOT / 'runtime_snapshot.json'}")
    print(f"Materialized {STATE_ROOT / 'session_document.json'}")


if __name__ == "__main__":
    main()
