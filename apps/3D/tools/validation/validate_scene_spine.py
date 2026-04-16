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

    if not isinstance(engine_systems, list):
        raise SystemExit(f"expected {ENGINE_SYSTEMS} to contain a JSON array")
    if not isinstance(sources, list):
        raise SystemExit(f"expected {SOURCES} to contain a JSON array")

    source_ids = {entry.get("id") for entry in sources if isinstance(entry, dict)}
    system_by_id = {entry.get("id"): entry for entry in engine_systems if isinstance(entry, dict)}

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

    if problems:
        print("Scene spine validation failed:")
        for problem in problems:
            print(f"- {problem}")
        return 1

    print("Scene spine validation passed for scene, exchange, semantics, bundle, viewport, camera, interaction, mesh, and lighting runtimes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
