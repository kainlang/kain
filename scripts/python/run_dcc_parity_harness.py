#!/usr/bin/env python3
"""
Execute parity-harness scenarios declared by the DCC parity matrix.

This harness is intentionally pragmatic. It proves the highest-priority shared,
sculpt, and painter seams with executable structural checks so the matrix can
drive implementation instead of drifting into static prose.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Callable

from validate_dcc_parity_matrix import load_json, resolve_matrix_path


SCENARIO_KIND = "scenario"
DEFAULT_INCLUDED_STATUSES = {"scaffolded", "in_progress", "implemented", "validated"}


@dataclass
class ScenarioContext:
    repo_root: Path
    matrix_path: Path
    matrix: dict[str, Any]


@dataclass
class ScenarioResult:
    feature_id: str
    feature_status: str
    hook_id: str
    target: str
    outcome: str
    summary: str
    details: list[str]


ScenarioHandler = Callable[[ScenarioContext, dict[str, Any], dict[str, Any]], tuple[str, str, list[str]]]


def repo_json(context: ScenarioContext, relative_path: str) -> Any:
    return load_json(context.repo_root / relative_path)


def repo_text(context: ScenarioContext, relative_path: str) -> str:
    return (context.repo_root / relative_path).read_text(encoding="utf-8")


def run_python_script(context: ScenarioContext, relative_path: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(context.repo_root / relative_path)],
        cwd=context.repo_root,
        capture_output=True,
        text=True,
        check=False,
    )


def make_result(
    feature: dict[str, Any],
    hook: dict[str, Any],
    outcome: str,
    summary: str,
    details: list[str],
) -> ScenarioResult:
    return ScenarioResult(
        feature_id=feature["id"],
        feature_status=feature["status"],
        hook_id=hook["id"],
        target=hook["target"],
        outcome=outcome,
        summary=summary,
        details=details,
    )


def require_contains(text: str, needles: list[str], context_label: str) -> list[str]:
    return [f"{context_label}: missing '{needle}'" for needle in needles if needle not in text]


def tool_by_id(tool_catalog: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {tool["id"]: tool for tool in tool_catalog["tools"]}


def workspace_mode_ids(workspace_modes: dict[str, Any]) -> set[str]:
    return {mode["id"] for mode in workspace_modes["modes"]}


def shader_by_id(shader_catalog: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {shader["id"]: shader for shader in shader_catalog["shader_catalog"]}


def mesh_document_ids(mesh_contract: dict[str, Any]) -> set[str]:
    return {document["id"] for document in mesh_contract["mesh_documents"]}


def collect_scenario_entries(
    matrix: dict[str, Any],
    included_statuses: set[str],
    selected_feature_ids: set[str],
    selected_targets: set[str],
) -> list[tuple[dict[str, Any], dict[str, Any]]]:
    entries: list[tuple[dict[str, Any], dict[str, Any]]] = []
    for feature in matrix["features"]:
        if feature["status"] not in included_statuses:
            continue
        if selected_feature_ids and feature["id"] not in selected_feature_ids:
            continue
        for hook in feature.get("validation_hooks", []):
            if hook.get("kind") != SCENARIO_KIND:
                continue
            if selected_targets and hook["target"] not in selected_targets:
                continue
            entries.append((feature, hook))
    return entries


def scenario_shared_workbench_registry_shell_materializes(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    session_result = run_python_script(context, "apps/kain-fabric-dcc-suite/scripts/materialize_session_state.py")
    if session_result.returncode != 0:
        return (
            "failed",
            "Shared session materializer failed before shell generation.",
            [session_result.stderr.strip() or session_result.stdout.strip() or "unknown materialize_session_state.py failure"],
        )

    shell_result = run_python_script(context, "apps/kain-fabric-dcc-suite/scripts/materialize_shell.py")
    if shell_result.returncode != 0:
        return (
            "failed",
            "Workbench shell materializer failed.",
            [shell_result.stderr.strip() or shell_result.stdout.strip() or "unknown materialize_shell.py failure"],
        )

    generated_path = context.repo_root / "apps/kain-fabric-dcc-suite/generated/main.generated.kn"
    if not generated_path.exists():
        return ("failed", "Workbench shell was not generated.", [str(generated_path)])

    generated_text = generated_path.read_text(encoding="utf-8")
    missing = require_contains(
        generated_text,
        ["component App():", "Workspace Modes", "Registry Inspectors", "parity capabilities:", "Viewport Stage"],
        "generated/main.generated.kn",
    )
    if missing:
        return ("failed", "Workbench shell materialized, but required registry/parity surfaces are missing.", missing)

    return (
        "passed",
        "Workbench shell materializes from the registry-owned app state and exposes parity telemetry.",
        [generated_path.relative_to(context.repo_root).as_posix()],
    )


def scenario_shared_session_snapshot_roundtrip(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    result = run_python_script(context, "apps/kain-fabric-dcc-suite/scripts/materialize_session_state.py")
    if result.returncode != 0:
        return (
            "failed",
            "Session materializer failed.",
            [result.stderr.strip() or result.stdout.strip() or "unknown materialize_session_state.py failure"],
        )

    runtime_snapshot_path = context.repo_root / "apps/kain-fabric-dcc-suite/state/runtime_snapshot.json"
    session_document_path = context.repo_root / "apps/kain-fabric-dcc-suite/state/session_document.json"
    if not runtime_snapshot_path.exists() or not session_document_path.exists():
        return (
            "failed",
            "Session materializer did not produce the expected state sidecars.",
            [str(runtime_snapshot_path), str(session_document_path)],
        )

    runtime_snapshot = load_json(runtime_snapshot_path)
    session_document = load_json(session_document_path)
    parity_summary = runtime_snapshot.get("dcc_suite_state", {}).get("parity_matrix")
    if not isinstance(parity_summary, dict):
        return ("failed", "Runtime snapshot does not expose a parity summary.", [str(runtime_snapshot_path)])

    expected_feature_count = len(context.matrix["features"])
    expected_scenario_count = sum(
        1
        for feature in context.matrix["features"]
        for validation_hook in feature.get("validation_hooks", [])
        if validation_hook.get("kind") == SCENARIO_KIND
    )
    expected_status_counts = dict(sorted(Counter(feature["status"] for feature in context.matrix["features"]).items()))
    expected_domain_counts = dict(sorted(Counter(feature["domain"] for feature in context.matrix["features"]).items()))

    errors: list[str] = []
    if parity_summary.get("capability_count") != expected_feature_count:
        errors.append(
            f"runtime_snapshot parity capability_count={parity_summary.get('capability_count')} expected={expected_feature_count}"
        )
    if parity_summary.get("feature_count") != expected_feature_count:
        errors.append(
            f"runtime_snapshot parity feature_count={parity_summary.get('feature_count')} expected={expected_feature_count}"
        )
    if parity_summary.get("scenario_count") != expected_scenario_count:
        errors.append(
            f"runtime_snapshot parity scenario_count={parity_summary.get('scenario_count')} expected={expected_scenario_count}"
        )
    if parity_summary.get("status_counts") != expected_status_counts:
        errors.append("runtime_snapshot parity status_counts drift from live matrix")
    if parity_summary.get("domain_counts") != expected_domain_counts:
        errors.append("runtime_snapshot parity domain_counts drift from live matrix")

    session_manifest_reference = session_document.get("dcc_parity_matrix")
    manifest_registry = session_document.get("dcc_suite_state", {}).get("manifest_registry", {})
    if session_manifest_reference != "config/dcc_parity_matrix.json" and manifest_registry.get("dcc_parity_matrix") != "config/dcc_parity_matrix.json":
        errors.append("session_document does not retain the dcc_parity_matrix reference")

    if errors:
        return ("failed", "Session/runtime parity summary drifted from the live matrix schema.", errors)

    return (
        "passed",
        "Session and runtime snapshot sidecars round-trip the live parity summary correctly.",
        [
            runtime_snapshot_path.relative_to(context.repo_root).as_posix(),
            session_document_path.relative_to(context.repo_root).as_posix(),
        ],
    )


def scenario_shared_undo_redo_and_restore(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    reducers_text = repo_text(context, "apps/kain-fabric-dcc-suite/session/reducers.kn")
    handlers_text = repo_text(context, "apps/kain-fabric-dcc-suite/session/command_handlers.kn")
    history_terms_present = any(token in reducers_text or token in handlers_text for token in ["undo", "redo", "history"])

    details = [
        "session reducers and command handlers exist and are the right ownership seam for history work",
        "a parity-grade shared undo/redo contract is not yet explicit in the current DCC session modules",
    ]
    if history_terms_present:
        details.append("history-related vocabulary exists, but no scenario-grade shared restore contract is proven yet")
    return ("pending", "Undo/redo and restore are still an in-progress parity seam.", details)


def scenario_sculpt_brush_registry_and_modes(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    tools = tool_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/tool_catalog.json"))
    sculpt_pipeline = repo_json(context, "apps/kain-fabric-dcc-suite/config/sculpt_pipeline.json")
    workspace_modes = workspace_mode_ids(repo_json(context, "apps/kain-fabric-dcc-suite/config/workspace_modes.json"))

    errors: list[str] = []
    for tool_id in ["clay_sculpt", "voxel_rebuild"]:
        if tool_id not in tools:
            errors.append(f"tool_catalog.json: missing {tool_id}")
    brush = sculpt_pipeline.get("brush", {})
    for field in ["radius_milli", "strength_milli", "falloff_milli"]:
        if field not in brush:
            errors.append(f"sculpt_pipeline.json: missing brush.{field}")
    if "sculpt" not in workspace_modes:
        errors.append("workspace_modes.json: missing sculpt mode")

    if errors:
        return ("failed", "Sculpt brush registry or workspace-mode seams are incomplete.", errors)

    return (
        "passed",
        "Sculpt brush and mode scaffolds are registry-owned in the flagship DCC app.",
        ["clay_sculpt", "voxel_rebuild", "config/sculpt_pipeline.json", "workspace mode: sculpt"],
    )


def scenario_sculpt_transform_gizmo_and_space(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    gizmo_registry = repo_json(context, "apps/kain-fabric-dcc-suite/config/gizmo_registry.json")
    tools = tool_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/tool_catalog.json"))

    profile_ids = {profile["id"] for profile in gizmo_registry["profiles"]}
    binding_ids = {binding["id"]: binding for binding in gizmo_registry["viewport_bindings"]}
    errors: list[str] = []

    if "dcc_transform_universal" not in profile_ids:
        errors.append("gizmo_registry.json: missing dcc_transform_universal profile")
    if "primary_dcc_viewport" not in binding_ids:
        errors.append("gizmo_registry.json: missing primary_dcc_viewport binding")

    for tool_id in ["select", "poly_extrude", "control_rig_edit"]:
        tool = tools.get(tool_id)
        if tool is None:
            errors.append(f"tool_catalog.json: missing {tool_id}")
            continue
        if not tool.get("gizmo_enabled"):
            errors.append(f"tool_catalog.json: {tool_id} is not gizmo-enabled")
        if tool.get("gizmo_profile_id") != "dcc_transform_universal":
            errors.append(f"tool_catalog.json: {tool_id} does not use dcc_transform_universal")
        if tool.get("default_gizmo_mode") not in {"translate", "rotate", "scale"}:
            errors.append(f"tool_catalog.json: {tool_id} has an invalid default_gizmo_mode")
        if tool.get("default_gizmo_space") not in {"world", "local"}:
            errors.append(f"tool_catalog.json: {tool_id} has an invalid default_gizmo_space")

    if errors:
        return ("failed", "Transform gizmo registry drifted from the expected DCC shell contract.", errors)

    return (
        "passed",
        "Transform gizmo profiles and default spaces remain registry-owned and wired to the flagship viewport.",
        ["dcc_transform_universal", "primary_dcc_viewport", "select/poly_extrude/control_rig_edit"],
    )


def scenario_sculpt_topology_rebuild_and_remesh(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    tools = tool_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/tool_catalog.json"))
    mesh_contract = repo_json(context, "apps/kain-fabric-dcc-suite/config/mesh_resource_contract.json")
    topology_manifest = context.repo_root / "apps/kain-fabric-dcc-suite/fabric/intents/topology_rebuild.fabric.toml"

    errors: list[str] = []
    if "voxel_rebuild" not in tools:
        errors.append("tool_catalog.json: missing voxel_rebuild tool")
    mesh_ids = mesh_document_ids(mesh_contract)
    for required_id in ["topology_output_mesh_document", "topology_history_mesh_document"]:
        if required_id not in mesh_ids:
            errors.append(f"mesh_resource_contract.json: missing {required_id}")
    if not topology_manifest.exists():
        errors.append("fabric/intents/topology_rebuild.fabric.toml is missing")

    if errors:
        return ("failed", "Topology rebuild/remesh seams are incomplete.", errors)

    return (
        "passed",
        "Topology rebuild and remesh scaffolds remain wired through tool, mesh-contract, and Fabric-intent seams.",
        ["voxel_rebuild", "topology_output_mesh_document", "topology_history_mesh_document"],
    )


def scenario_sculpt_preview_materials_and_export(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    shader_catalog = shader_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/shader_catalog.json"))
    material_authoring_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/material_authoring_projection.kn")
    material_export_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/material_texture_export_projection.kn")

    errors: list[str] = []
    for shader_id in ["material_bake_preview", "material_channel_pack"]:
        if shader_id not in shader_catalog:
            errors.append(f"shader_catalog.json: missing {shader_id}")
    errors.extend(
        require_contains(
            material_authoring_text,
            ["authoring_mode", "texture_set_id", "export_preset"],
            "src/material_authoring_projection.kn",
        )
    )
    errors.extend(
        require_contains(
            material_export_text,
            ["channel_pack_profile", "gltf_pbr+usd_preview+native_ui", "material_texture_export_report"],
            "src/material_texture_export_projection.kn",
        )
    )

    if errors:
        return ("failed", "Preview-material or export scaffolds drifted from the expected material lane seams.", errors)

    return (
        "passed",
        "Preview-material and packed-export scaffolds are wired through the material projection and shader catalog seams.",
        ["material_bake_preview", "material_channel_pack", "material_authoring_projection", "material_texture_export_projection"],
    )


def scenario_sculpt_primitive_import_and_gltf_export(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    primitive_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/primitive_mesh_authoring.kn")
    import_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/mesh_import_projection.kn")
    ingest_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/asset_ingest_step.kn")
    mesh_contract = repo_json(context, "apps/kain-fabric-dcc-suite/config/mesh_resource_contract.json")

    errors: list[str] = []
    errors.extend(
        require_contains(
            primitive_text,
            ["primitive_templates", "startup_primitive_id", "mesh://primitives/authored/definitions"],
            "src/primitive_mesh_authoring.kn",
        )
    )
    errors.extend(
        require_contains(
            import_text,
            ["mesh://imports/current/payloads", "mesh://topology/output/current"],
            "src/mesh_import_projection.kn",
        )
    )
    errors.extend(require_contains(ingest_text, [".gltf", ".glb"], "src/asset_ingest_step.kn"))

    mesh_ids = mesh_document_ids(mesh_contract)
    for required_id in ["imported_mesh_payload_document", "authored_primitive_definition_document", "active_editable_mesh_document"]:
        if required_id not in mesh_ids:
            errors.append(f"mesh_resource_contract.json: missing {required_id}")

    if errors:
        return ("failed", "Primitive/import seams are incomplete or no longer advertise glTF intake.", errors)

    return (
        "passed",
        "Primitive authoring, import intake, and active-edit-target routing remain wired for the sculpt lane.",
        ["primitive templates", "gltf/glb intake", "active editable mesh contract"],
    )


def scenario_paint_layer_stack_and_opacity(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    studio_app_text = repo_text(context, "apps/kain-canvas-forge/helpers/client/studio_app.tsx")
    errors = require_contains(
        studio_app_text,
        ["type PaintLayer", "opacity: number;", "toggleLayerVisibility", "createNewLayer", "globalAlpha = layer.opacity"],
        "apps/kain-canvas-forge/helpers/client/studio_app.tsx",
    )
    if errors:
        return ("failed", "Canvas Forge no longer proves the current layer-stack and opacity seam.", errors)

    return (
        "passed",
        "Canvas Forge still proves layered paint rows, visibility toggles, and opacity-aware compositing for the composite painter baseline.",
        ["PaintLayer", "toggleLayerVisibility", "createNewLayer", "globalAlpha = layer.opacity"],
    )


def scenario_paint_brush_alpha_erase_symmetry(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    brushes = repo_json(context, "apps/kain-canvas-forge/manifests/brushes.json")
    studio_app_text = repo_text(context, "apps/kain-canvas-forge/helpers/client/studio_app.tsx")

    details: list[str] = []
    if not any("eraser" in brush["id"] for brush in brushes):
        return ("failed", "Brush registry no longer exposes an eraser preset.", ["apps/kain-canvas-forge/manifests/brushes.json"])
    details.append("brush registry includes an eraser preset")
    if "activeToolId === \"eraser\"" not in studio_app_text and "Erasing" not in studio_app_text:
        return ("failed", "Paint surface no longer exposes eraser behavior.", ["apps/kain-canvas-forge/helpers/client/studio_app.tsx"])
    details.append("paint surface still exposes eraser behavior")

    if "symmetry" not in studio_app_text.lower():
        details.append("symmetry vocabulary is still missing from the current paint proof")
        return ("pending", "Brush presets and erase behavior exist, but symmetry parity is not yet proven in the flagship/native lane.", details)

    return ("passed", "Brush presets, eraser behavior, and symmetry are all present in the current painter proof.", details)


def scenario_paint_material_channels(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    tool_catalog = tool_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/tool_catalog.json"))
    main_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/main.kn")
    material_authoring_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/material_authoring_projection.kn")

    errors: list[str] = []
    if "material_layer_paint" not in tool_catalog:
        errors.append("tool_catalog.json: missing material_layer_paint")
    errors.extend(
        require_contains(
            main_text,
            ["channels=basecolor+normal+roughness+metallic+ao+height+emissive"],
            "src/main.kn",
        )
    )
    errors.extend(
        require_contains(
            material_authoring_text,
            ["texture_set_id", "layer_stack_id", "export_preset"],
            "src/material_authoring_projection.kn",
        )
    )

    if errors:
        return ("failed", "Painter material-channel scaffolds drifted from the expected channel contract.", errors)

    return (
        "passed",
        "The flagship app still advertises layered texture-set authoring across the expected material channels.",
        ["material_layer_paint", "basecolor+normal+roughness+metallic+ao+height+emissive"],
    )


def scenario_paint_live_3d_preview(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    shader_catalog = shader_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/shader_catalog.json"))
    studio_app_text = repo_text(context, "apps/kain-canvas-forge/helpers/client/studio_app.tsx")

    errors: list[str] = []
    for shader_id in ["material_bake_preview", "material_paint_runtime_preview"]:
        if shader_id not in shader_catalog:
            errors.append(f"shader_catalog.json: missing {shader_id}")
    errors.extend(
        require_contains(
            studio_app_text,
            ["import * as THREE from \"three\";", "OrbitControls", "WebGLRenderer", "MeshStandardMaterial"],
            "apps/kain-canvas-forge/helpers/client/studio_app.tsx",
        )
    )

    if errors:
        return ("failed", "Painter live-preview seams are incomplete.", errors)

    return (
        "passed",
        "The composite painter baseline still includes a live Three.js preview plus flagship-native preview shader ownership.",
        ["material_bake_preview", "material_paint_runtime_preview", "THREE preview"],
    )


def scenario_paint_texture_and_packed_export(
    context: ScenarioContext,
    _feature: dict[str, Any],
    _hook: dict[str, Any],
) -> tuple[str, str, list[str]]:
    tool_catalog = tool_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/tool_catalog.json"))
    shader_catalog = shader_by_id(repo_json(context, "apps/kain-fabric-dcc-suite/config/shader_catalog.json"))
    export_text = repo_text(context, "apps/kain-fabric-dcc-suite/src/material_texture_export_projection.kn")

    errors: list[str] = []
    if "texture_bake" not in tool_catalog:
        errors.append("tool_catalog.json: missing texture_bake")
    if "material_channel_pack" not in shader_catalog:
        errors.append("shader_catalog.json: missing material_channel_pack")
    errors.extend(
        require_contains(
            export_text,
            ["channel_pack_profile", "metalrough+orm+height+emissive", "material_texture_export_report"],
            "src/material_texture_export_projection.kn",
        )
    )

    if errors:
        return ("failed", "Painter packed-export seams are incomplete.", errors)

    return (
        "passed",
        "Packed texture export remains wired through the flagship material-export projection and channel-pack shader seam.",
        ["texture_bake", "material_channel_pack", "metalrough+orm+height+emissive"],
    )


SCENARIO_HANDLERS: dict[str, ScenarioHandler] = {
    "dcc.shared.workbench.registry_shell_materializes": scenario_shared_workbench_registry_shell_materializes,
    "dcc.shared.session.snapshot_roundtrip": scenario_shared_session_snapshot_roundtrip,
    "dcc.shared.undo_redo_and_restore": scenario_shared_undo_redo_and_restore,
    "dcc.sculpt.brush_registry_and_modes": scenario_sculpt_brush_registry_and_modes,
    "dcc.sculpt.transform_gizmo_and_space": scenario_sculpt_transform_gizmo_and_space,
    "dcc.sculpt.topology_rebuild_and_remesh": scenario_sculpt_topology_rebuild_and_remesh,
    "dcc.sculpt.preview_materials_and_export": scenario_sculpt_preview_materials_and_export,
    "dcc.sculpt.primitive_import_and_gltf_export": scenario_sculpt_primitive_import_and_gltf_export,
    "dcc.paint.layer_stack_and_opacity": scenario_paint_layer_stack_and_opacity,
    "dcc.paint.brush_alpha_erase_symmetry": scenario_paint_brush_alpha_erase_symmetry,
    "dcc.paint.material_channels": scenario_paint_material_channels,
    "dcc.paint.live_3d_preview": scenario_paint_live_3d_preview,
    "dcc.paint.texture_and_packed_export": scenario_paint_texture_and_packed_export,
}


def run_harness(
    context: ScenarioContext,
    included_statuses: set[str],
    selected_feature_ids: set[str],
    selected_targets: set[str],
) -> list[ScenarioResult]:
    results: list[ScenarioResult] = []
    for feature, hook in collect_scenario_entries(context.matrix, included_statuses, selected_feature_ids, selected_targets):
        handler = SCENARIO_HANDLERS.get(hook["target"])
        if handler is None:
            results.append(
                make_result(
                    feature,
                    hook,
                    "pending",
                    "No executable parity-harness handler exists for this scenario yet.",
                    [hook["target"]],
                )
            )
            continue

        outcome, summary, details = handler(context, feature, hook)
        results.append(make_result(feature, hook, outcome, summary, details))
    return results


def print_text_report(results: list[ScenarioResult]) -> None:
    outcome_counts = Counter(result.outcome for result in results)
    print(f"Scenario count: {len(results)}")
    print("Outcome counts:")
    for outcome, count in sorted(outcome_counts.items()):
        print(f"  - {outcome}: {count}")
    print("Scenario results:")
    for result in results:
        print(f"  - [{result.outcome}] {result.feature_id} :: {result.target}")
        print(f"    {result.summary}")
        for detail in result.details:
            print(f"    detail: {detail}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run executable DCC parity harness scenarios.")
    parser.add_argument(
        "--matrix",
        help="Path to the parity matrix JSON. Defaults to the path declared in the flagship app manifest.",
    )
    parser.add_argument(
        "--manifest",
        help="Path to the flagship app manifest. Defaults to apps/kain-fabric-dcc-suite/config/app_manifest.json.",
    )
    parser.add_argument(
        "--feature",
        action="append",
        default=[],
        help="Feature id to run. Repeat to select more than one.",
    )
    parser.add_argument(
        "--scenario",
        action="append",
        default=[],
        help="Scenario target id to run. Repeat to select more than one.",
    )
    parser.add_argument(
        "--include-reference-only",
        action="store_true",
        help="Include reference_only features in the harness run.",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail when any scenario is pending as well as failed.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print the harness report as JSON.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    matrix_path = resolve_matrix_path(repo_root, args.matrix, args.manifest)
    matrix = load_json(matrix_path)
    included_statuses = set(DEFAULT_INCLUDED_STATUSES)
    if args.include_reference_only:
        included_statuses.add("reference_only")

    context = ScenarioContext(repo_root=repo_root, matrix_path=matrix_path, matrix=matrix)
    results = run_harness(
        context,
        included_statuses=included_statuses,
        selected_feature_ids=set(args.feature),
        selected_targets=set(args.scenario),
    )

    outcome_counts = dict(sorted(Counter(result.outcome for result in results).items()))
    payload = {
        "ok": not any(result.outcome == "failed" for result in results)
        and (not args.strict or not any(result.outcome == "pending" for result in results)),
        "matrix_path": str(matrix_path),
        "scenario_count": len(results),
        "outcome_counts": outcome_counts,
        "results": [asdict(result) for result in results],
    }

    if args.json:
        print(json.dumps(payload, indent=2))
    else:
        print(f"Parity harness: {matrix_path}")
        print_text_report(results)

    if any(result.outcome == "failed" for result in results):
        return 1
    if args.strict and any(result.outcome == "pending" for result in results):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
