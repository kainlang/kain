#!/usr/bin/env python3
"""
Validate the Kain DCC parity matrix.

This validator is intentionally structural. It proves that parity claims live in
one machine-readable inventory with stable ids, real repo paths, explicit
owners, and validation hooks.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any


FEATURE_ID_PATTERN = re.compile(r"^[a-z0-9]+(?:\.[a-z0-9_]+)+$")
VALID_HOOK_KINDS = {"command", "scenario", "benchmark", "doc"}
DEFAULT_APP_MANIFEST = Path("apps/kain-fabric-dcc-suite/config/app_manifest.json")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_manifest_path(repo_root: Path, manifest_override: str | None) -> Path:
    if manifest_override:
        manifest_path = Path(manifest_override)
        if not manifest_path.is_absolute():
            manifest_path = (repo_root / manifest_path).resolve()
        return manifest_path
    return (repo_root / DEFAULT_APP_MANIFEST).resolve()


def resolve_matrix_path(repo_root: Path, matrix_override: str | None, manifest_override: str | None) -> Path:
    if matrix_override:
        override_path = Path(matrix_override)
        return override_path if override_path.is_absolute() else (repo_root / override_path).resolve()

    manifest_path = resolve_manifest_path(repo_root, manifest_override)
    if not manifest_path.exists():
        raise SystemExit(f"App manifest not found: {manifest_path}")

    manifest = load_json(manifest_path)
    relative_matrix_path = manifest.get("manifests", {}).get("dcc_parity_matrix")
    if not relative_matrix_path:
        raise SystemExit(f"App manifest does not define manifests.dcc_parity_matrix: {manifest_path}")

    app_root = manifest_path.parent.parent
    return (app_root / relative_matrix_path).resolve()


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


def validate_existing_path(repo_root: Path, path_text: str, errors: list[str], context: str) -> None:
    resolved = (repo_root / path_text).resolve()
    if not resolved.exists():
        errors.append(f"{context}: path does not exist -> {path_text}")


def validate_matrix(repo_root: Path, matrix_path: Path) -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    matrix = load_json(matrix_path)

    if matrix.get("schema_version") != 1:
        errors.append(f"{matrix_path}: unsupported schema_version {matrix.get('schema_version')!r}")

    statuses = require_list(matrix, "status_definitions", errors, "status_definitions")
    domains = require_list(matrix, "domains", errors, "domains")
    baselines = require_list(matrix, "baseline_families", errors, "baseline_families")
    features = require_list(matrix, "features", errors, "features")

    status_ids: list[str] = []
    for status in statuses:
        status_id = require_string(status, "id", errors, "status_definitions[]")
        if status_id:
            status_ids.append(status_id)
    if len(status_ids) != len(set(status_ids)):
        errors.append("status_definitions: duplicate status id detected")
    valid_status_ids = set(status_ids)

    domain_ids: list[str] = []
    for domain in domains:
        domain_id = require_string(domain, "id", errors, "domains[]")
        if domain_id:
            domain_ids.append(domain_id)
    if len(domain_ids) != len(set(domain_ids)):
        errors.append("domains: duplicate domain id detected")
    valid_domain_ids = set(domain_ids)

    baseline_ids: list[str] = []
    for baseline in baselines:
        baseline_id = require_string(baseline, "id", errors, "baseline_families[]")
        if baseline_id:
            baseline_ids.append(baseline_id)
        for source_path in baseline.get("sources", []):
            if isinstance(source_path, str) and source_path.strip():
                validate_existing_path(repo_root, source_path, errors, f"baseline_families[{baseline_id}]")
            else:
                errors.append(f"baseline_families[{baseline_id}]: invalid source path entry")
    if len(baseline_ids) != len(set(baseline_ids)):
        errors.append("baseline_families: duplicate baseline id detected")
    valid_baseline_ids = set(baseline_ids)

    feature_ids: set[str] = set()
    hook_ids: set[str] = set()
    domain_counter: Counter[str] = Counter()
    status_counter: Counter[str] = Counter()

    for feature in features:
        feature_id = require_string(feature, "id", errors, "features[]")
        feature_context = f"features[{feature_id or '<missing-id>'}]"
        if feature_id:
            if not FEATURE_ID_PATTERN.fullmatch(feature_id):
                errors.append(f"{feature_context}: invalid feature id format")
            if feature_id in feature_ids:
                errors.append(f"{feature_context}: duplicate feature id")
            feature_ids.add(feature_id)

        domain = require_string(feature, "domain", errors, feature_context)
        if domain and domain not in valid_domain_ids:
            errors.append(f"{feature_context}: unknown domain '{domain}'")
        if domain:
            domain_counter[domain] += 1

        status = require_string(feature, "status", errors, feature_context)
        if status and status not in valid_status_ids:
            errors.append(f"{feature_context}: unknown status '{status}'")
        if status:
            status_counter[status] += 1

        feature_baselines = require_list(feature, "baseline_families", errors, feature_context)
        for baseline_id in feature_baselines:
            if not isinstance(baseline_id, str) or baseline_id not in valid_baseline_ids:
                errors.append(f"{feature_context}: unknown baseline family '{baseline_id}'")

        reference_sources = require_list(feature, "reference_sources", errors, feature_context)
        for index, source in enumerate(reference_sources):
            if not isinstance(source, dict):
                errors.append(f"{feature_context}: reference_sources[{index}] must be an object")
                continue
            source_path = require_string(source, "path", errors, f"{feature_context}.reference_sources[{index}]")
            if source_path:
                validate_existing_path(repo_root, source_path, errors, f"{feature_context}.reference_sources[{index}]")

        current_surfaces = require_list(feature, "current_kain_surfaces", errors, feature_context)
        for index, surface in enumerate(current_surfaces):
            if not isinstance(surface, dict):
                errors.append(f"{feature_context}: current_kain_surfaces[{index}] must be an object")
                continue
            surface_path = require_string(surface, "path", errors, f"{feature_context}.current_kain_surfaces[{index}]")
            if surface_path:
                validate_existing_path(repo_root, surface_path, errors, f"{feature_context}.current_kain_surfaces[{index}]")

        owners = require_list(feature, "owners", errors, feature_context)
        for index, owner in enumerate(owners):
            if not isinstance(owner, dict):
                errors.append(f"{feature_context}: owners[{index}] must be an object")
                continue
            require_string(owner, "subsystem", errors, f"{feature_context}.owners[{index}]")
            owned_paths = require_list(owner, "owned_paths", errors, f"{feature_context}.owners[{index}]")
            for owned_path in owned_paths:
                if not isinstance(owned_path, str) or not owned_path.strip():
                    errors.append(f"{feature_context}.owners[{index}]: invalid owned_paths entry")
                    continue
                validate_existing_path(repo_root, owned_path, errors, f"{feature_context}.owners[{index}]")

        hooks = require_list(feature, "validation_hooks", errors, feature_context)
        for index, hook in enumerate(hooks):
            if not isinstance(hook, dict):
                errors.append(f"{feature_context}: validation_hooks[{index}] must be an object")
                continue
            hook_id = require_string(hook, "id", errors, f"{feature_context}.validation_hooks[{index}]")
            hook_kind = require_string(hook, "kind", errors, f"{feature_context}.validation_hooks[{index}]")
            require_string(hook, "target", errors, f"{feature_context}.validation_hooks[{index}]")
            if hook_kind and hook_kind not in VALID_HOOK_KINDS:
                errors.append(f"{feature_context}: invalid validation hook kind '{hook_kind}'")
            if hook_id:
                if hook_id in hook_ids:
                    errors.append(f"{feature_context}: duplicate validation hook id '{hook_id}'")
                hook_ids.add(hook_id)

        if status in {"implemented", "validated"} and not current_surfaces:
            errors.append(f"{feature_context}: implemented or validated features must list current_kain_surfaces")

    summary = {
        "matrix_path": str(matrix_path),
        "feature_count": len(features),
        "domain_counts": dict(sorted(domain_counter.items())),
        "status_counts": dict(sorted(status_counter.items())),
    }
    return errors, summary


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the Kain DCC parity matrix.")
    parser.add_argument(
        "--matrix",
        help="Path to the parity matrix JSON. Defaults to the path declared in the flagship app manifest.",
    )
    parser.add_argument(
        "--manifest",
        help="Path to the flagship app manifest. Defaults to apps/kain-fabric-dcc-suite/config/app_manifest.json.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print the summary report as JSON.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    matrix_path = resolve_matrix_path(repo_root, args.matrix, args.manifest)
    errors, summary = validate_matrix(repo_root, matrix_path)

    if args.json:
        payload = {"ok": not errors, "summary": summary, "errors": errors}
        print(json.dumps(payload, indent=2))
    else:
        print(f"Parity matrix: {summary['matrix_path']}")
        print(f"Feature count: {summary['feature_count']}")
        print("Domain counts:")
        for domain, count in summary["domain_counts"].items():
            print(f"  - {domain}: {count}")
        print("Status counts:")
        for status, count in summary["status_counts"].items():
            print(f"  - {status}: {count}")

    if errors:
        if not args.json:
            print("Errors:")
            for error in errors:
                print(f"  - {error}")
        return 1

    if not args.json:
        print("Parity matrix validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
