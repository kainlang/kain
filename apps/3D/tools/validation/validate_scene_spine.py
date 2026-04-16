#!/usr/bin/env python3
"""Validate that the shared 3D scene spine stays registered in the template manifests.

This keeps the reusable 3D contracts aligned with the manifest-driven template
instead of letting scene/viewport/interaction/mesh/lighting lanes drift into
app-local one-offs.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE_SYSTEMS = ROOT / "manifests" / "engine_systems.json"
SOURCES = ROOT / "manifests" / "sources.json"
RUNTIME_APPS = ROOT / "manifests" / "runtime_apps.json"
WORKSPACE_PRESETS = ROOT / "manifests" / "workspace_presets.json"

EXPECTED_SYSTEMS = {
    "scene_runtime": "three_d_scene_runtime",
    "scene_exchange_runtime": "three_d_scene_exchange_runtime",
    "scene_semantics_runtime": "three_d_scene_semantics_runtime",
    "scene_bundle_runtime": "three_d_scene_bundle_runtime",
    "viewport_runtime": "three_d_viewport_runtime",
    "camera_runtime": "three_d_camera_runtime",
    "interaction_runtime": "three_d_interaction_runtime",
    "mesh_runtime": "three_d_mesh_runtime",
    "lighting_runtime": "three_d_lighting_runtime",
}

EXPECTED_RUNTIME_APP = {
    "id": "universal_3d_workbench",
    "source_id": "universal_3d_workbench_app",
    "host_kind": "native_ui",
}

EXPECTED_WORKSPACE_PRESET = {
    "id": "universal_3d_workbench",
    "source_id": "universal_3d_workbench_app",
    "host_kind": "native_ui",
}


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise SystemExit(f"missing required manifest: {path}")
    except json.JSONDecodeError as exc:
        raise SystemExit(f"invalid JSON in {path}: {exc}") from exc


def main() -> int:
    engine_systems = load_json(ENGINE_SYSTEMS)
    sources = load_json(SOURCES)
    runtime_apps = load_json(RUNTIME_APPS)
    workspace_presets = load_json(WORKSPACE_PRESETS)

    if not isinstance(engine_systems, list):
        raise SystemExit(f"expected {ENGINE_SYSTEMS} to contain a JSON array")
    if not isinstance(sources, list):
        raise SystemExit(f"expected {SOURCES} to contain a JSON array")
    if not isinstance(runtime_apps, list):
        raise SystemExit(f"expected {RUNTIME_APPS} to contain a JSON array")
    if not isinstance(workspace_presets, list):
        raise SystemExit(f"expected {WORKSPACE_PRESETS} to contain a JSON array")

    source_ids = {entry.get("id") for entry in sources if isinstance(entry, dict)}
    system_by_id = {entry.get("id"): entry for entry in engine_systems if isinstance(entry, dict)}
    runtime_app_by_id = {entry.get("id"): entry for entry in runtime_apps if isinstance(entry, dict)}
    workspace_preset_by_id = {
        entry.get("id"): entry for entry in workspace_presets if isinstance(entry, dict)
    }

    problems: list[str] = []
    for system_id, source_id in EXPECTED_SYSTEMS.items():
        system = system_by_id.get(system_id)
        if system is None:
            problems.append(f"missing engine system: {system_id}")
            continue
        if system.get("source_id") != source_id:
            problems.append(
                f"{system_id} has source_id={system.get('source_id')!r}, expected {source_id!r}"
            )
        if source_id not in source_ids:
            problems.append(f"missing source registry entry: {source_id}")

    runtime_app = runtime_app_by_id.get(EXPECTED_RUNTIME_APP["id"])
    if runtime_app is None:
        problems.append(f"missing runtime app: {EXPECTED_RUNTIME_APP['id']}")
    else:
        for key, expected_value in EXPECTED_RUNTIME_APP.items():
            if runtime_app.get(key) != expected_value:
                problems.append(
                    f"runtime app {EXPECTED_RUNTIME_APP['id']} has {key}={runtime_app.get(key)!r}, expected {expected_value!r}"
                )

    workspace_preset = workspace_preset_by_id.get(EXPECTED_WORKSPACE_PRESET["id"])
    if workspace_preset is None:
        problems.append(f"missing workspace preset: {EXPECTED_WORKSPACE_PRESET['id']}")
    else:
        for key, expected_value in EXPECTED_WORKSPACE_PRESET.items():
            if workspace_preset.get(key) != expected_value:
                problems.append(
                    f"workspace preset {EXPECTED_WORKSPACE_PRESET['id']} has {key}={workspace_preset.get(key)!r}, expected {expected_value!r}"
                )

    if problems:
        print("Scene spine validation failed:")
        for problem in problems:
            print(f"- {problem}")
        return 1

    print(
        "Scene spine validation passed for the shared scene spine, the primary 3D runtime app, and the universal 3D workspace lane."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
