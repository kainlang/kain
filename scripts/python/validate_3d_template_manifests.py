#!/usr/bin/env python3
"""
Validate the Kain 3D template manifest projection layer.

This validator keeps the 3D template manifest-driven by checking that
projection rows resolve back to the shared source registry, and that the
high-level engine system, runtime app, workspace preset, GPU kernel, and
tensor pipeline surfaces stay structurally sound.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any


DEFAULT_TEMPLATE_ROOT = Path("apps/3D")


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_path(repo_root: Path, maybe_relative: str | Path) -> Path:
    path = Path(maybe_relative)
    return path if path.is_absolute() else (repo_root / path).resolve()


def require_string(record: dict[str, Any], field: str, errors: list[str], context: str) -> str:
    value = record.get(field)
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{context}: expected non-empty string for '{field}'")
        return ""
    return value


def require_list(record: dict[str, Any], field: str, errors: list[str], context: str) -> list[Any]:
    value = record.get(field)
    if not isinstance(value, list) or not value:
        errors.append(f"{context}: expected non-empty list for '{field}'")
        return []
    return value


def validate_sources(repo_root: Path, template_root: Path, errors: list[str]) -> dict[str, dict[str, Any]]:
    sources_path = resolve_path(repo_root, template_root / "manifests/sources.json")
    sources = load_json(sources_path)
    if not isinstance(sources, list) or not sources:
        errors.append(f"{sources_path}: expected a non-empty JSON array")
        return {}

    index: dict[str, dict[str, Any]] = {}
    duplicate_ids: list[str] = []
    for idx, source in enumerate(sources):
        context = f"sources[{idx}]"
        if not isinstance(source, dict):
            errors.append(f"{context}: expected object")
            continue
        source_id = require_string(source, "id", errors, context)
        require_string(source, "source_path", errors, context)
        if source_id:
            if source_id in index:
                duplicate_ids.append(source_id)
            else:
                index[source_id] = source
    if duplicate_ids:
        errors.append(f"sources: duplicate id(s) detected -> {', '.join(sorted(set(duplicate_ids)))}")
    return index


def validate_runtime_apps(repo_root: Path, template_root: Path, source_index: dict[str, dict[str, Any]], errors: list[str]) -> set[str]:
    runtime_apps_path = resolve_path(repo_root, template_root / "manifests/runtime_apps.json")
    runtime_apps = load_json(runtime_apps_path)
    if not isinstance(runtime_apps, list) or not runtime_apps:
        errors.append(f"{runtime_apps_path}: expected a non-empty JSON array")
        return set()

    app_ids: set[str] = set()
    for idx, app in enumerate(runtime_apps):
        context = f"runtime_apps[{idx}]"
        if not isinstance(app, dict):
            errors.append(f"{context}: expected object")
            continue
        app_id = require_string(app, "id", errors, context)
        source_id = require_string(app, "source_id", errors, context)
        require_string(app, "label", errors, context)
        require_string(app, "namespace", errors, context)
        require_string(app, "runtime_kind", errors, context)
        require_string(app, "host_kind", errors, context)
        outputs = require_list(app, "outputs", errors, context)
        if source_id and source_id not in source_index:
            errors.append(f"{context}: unknown source_id -> {source_id}")
        if source_id and app_id and source_id == app.get("id"):
            errors.append(f"{context}: source_id should reference the shared source registry, not mirror the runtime app id")
        output_targets: Counter[str] = Counter()
        for out_idx, output in enumerate(outputs):
            output_context = f"{context}.outputs[{out_idx}]"
            if not isinstance(output, dict):
                errors.append(f"{output_context}: expected object")
                continue
            target = require_string(output, "target", errors, output_context)
            path = require_string(output, "path", errors, output_context)
            if target:
                output_targets[target] += 1
            if path:
                resolved = resolve_path(repo_root, template_root / path)
                if not resolved.parts:
                    errors.append(f"{output_context}: invalid output path -> {path}")
        repeated_targets = [target for target, count in output_targets.items() if count > 1]
        if repeated_targets:
            errors.append(f"{context}: duplicate output target(s) detected -> {', '.join(sorted(repeated_targets))}")
        if app_id:
            if app_id in app_ids:
                errors.append(f"{context}: duplicate app id -> {app_id}")
            app_ids.add(app_id)
    return app_ids


def validate_workspace_presets(repo_root: Path, template_root: Path, source_index: dict[str, dict[str, Any]], runtime_app_ids: set[str], errors: list[str]) -> None:
    presets_path = resolve_path(repo_root, template_root / "manifests/workspace_presets.json")
    presets = load_json(presets_path)
    if not isinstance(presets, list) or not presets:
        errors.append(f"{presets_path}: expected a non-empty JSON array")
        return

    preset_ids: list[str] = []
    for idx, preset in enumerate(presets):
        context = f"workspace_presets[{idx}]"
        if not isinstance(preset, dict):
            errors.append(f"{context}: expected object")
            continue
        preset_id = require_string(preset, "id", errors, context)
        source_id = require_string(preset, "source_id", errors, context)
        require_string(preset, "label", errors, context)
        require_string(preset, "preset_kind", errors, context)
        require_string(preset, "focus_lane", errors, context)
        runtime_app_id = require_string(preset, "runtime_app_id", errors, context)
        require_string(preset, "host_kind", errors, context)
        if source_id and source_id not in source_index:
            errors.append(f"{context}: unknown source_id -> {source_id}")
        if runtime_app_id and runtime_app_id not in runtime_app_ids:
            errors.append(f"{context}: unknown runtime_app_id -> {runtime_app_id}")
        if preset_id:
            preset_ids.append(preset_id)
    if len(preset_ids) != len(set(preset_ids)):
        errors.append("workspace_presets: duplicate preset id detected")


def validate_engine_systems(repo_root: Path, template_root: Path, source_index: dict[str, dict[str, Any]], errors: list[str]) -> None:
    systems_path = resolve_path(repo_root, template_root / "manifests/engine_systems.json")
    systems = load_json(systems_path)
    if not isinstance(systems, list) or not systems:
        errors.append(f"{systems_path}: expected a non-empty JSON array")
        return

    system_ids: list[str] = []
    for idx, system in enumerate(systems):
        context = f"engine_systems[{idx}]"
        if not isinstance(system, dict):
            errors.append(f"{context}: expected object")
            continue
        system_id = require_string(system, "id", errors, context)
        source_id = require_string(system, "source_id", errors, context)
        require_string(system, "label", errors, context)
        require_string(system, "lane", errors, context)
        require_string(system, "description", errors, context)
        if source_id and source_id not in source_index:
            errors.append(f"{context}: unknown source_id -> {source_id}")
        if system_id:
            system_ids.append(system_id)
    if len(system_ids) != len(set(system_ids)):
        errors.append("engine_systems: duplicate system id detected")


def validate_gpu_kernels(repo_root: Path, template_root: Path, source_index: dict[str, dict[str, Any]], errors: list[str]) -> set[str]:
    kernels_path = resolve_path(repo_root, template_root / "manifests/gpu_kernels.json")
    kernels = load_json(kernels_path)
    if not isinstance(kernels, list) or not kernels:
        errors.append(f"{kernels_path}: expected a non-empty JSON array")
        return set()

    kernel_ids: set[str] = set()
    for idx, kernel in enumerate(kernels):
        context = f"gpu_kernels[{idx}]"
        if not isinstance(kernel, dict):
            errors.append(f"{context}: expected object")
            continue
        kernel_id = require_string(kernel, "id", errors, context)
        source_id = require_string(kernel, "source_id", errors, context)
        require_string(kernel, "stage", errors, context)
        require_string(kernel, "tensor_role", errors, context)
        require_string(kernel, "entry", errors, context)
        if source_id and source_id not in source_index:
            errors.append(f"{context}: unknown source_id -> {source_id}")
        if kernel_id:
            if kernel_id in kernel_ids:
                errors.append(f"{context}: duplicate kernel id -> {kernel_id}")
            kernel_ids.add(kernel_id)
    return kernel_ids


def validate_tensor_pipelines(repo_root: Path, template_root: Path, kernel_ids: set[str], errors: list[str]) -> None:
    pipelines_path = resolve_path(repo_root, template_root / "manifests/tensor_pipelines.json")
    pipelines = load_json(pipelines_path)
    if not isinstance(pipelines, list) or not pipelines:
        errors.append(f"{pipelines_path}: expected a non-empty JSON array")
        return

    pipeline_ids: list[str] = []
    for idx, pipeline in enumerate(pipelines):
        context = f"tensor_pipelines[{idx}]"
        if not isinstance(pipeline, dict):
            errors.append(f"{context}: expected object")
            continue
        pipeline_id = require_string(pipeline, "id", errors, context)
        require_string(pipeline, "label", errors, context)
        require_string(pipeline, "domain", errors, context)
        require_string(pipeline, "priority", errors, context)
        require_string(pipeline, "residency", errors, context)
        passes = require_list(pipeline, "passes", errors, context)
        if pipeline_id:
            pipeline_ids.append(pipeline_id)
        for pass_idx, pass_id_value in enumerate(passes):
            if not isinstance(pass_id_value, str) or not pass_id_value.strip():
                errors.append(f"{context}.passes[{pass_idx}]: expected non-empty string")
                continue
            if pass_id_value not in kernel_ids:
                errors.append(f"{context}: unknown pass id -> {pass_id_value}")
    if len(pipeline_ids) != len(set(pipeline_ids)):
        errors.append("tensor_pipelines: duplicate pipeline id detected")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the Kain 3D template manifest projection layer")
    parser.add_argument("--repo-root", default=".", help="Repository root (default: current directory)")
    parser.add_argument("--template-root", default=str(DEFAULT_TEMPLATE_ROOT), help="3D template root (default: apps/3D)")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    template_root = resolve_path(repo_root, args.template_root)

    errors: list[str] = []
    source_index = validate_sources(repo_root, template_root, errors)
    kernel_ids = validate_gpu_kernels(repo_root, template_root, source_index, errors)
    validate_engine_systems(repo_root, template_root, source_index, errors)
    runtime_app_ids = validate_runtime_apps(repo_root, template_root, source_index, errors)
    validate_workspace_presets(repo_root, template_root, source_index, runtime_app_ids, errors)
    validate_tensor_pipelines(repo_root, template_root, kernel_ids, errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print("3D template manifests validated: sources, GPU kernels, engine systems, runtime apps, workspace presets, and tensor pipelines are structurally consistent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
