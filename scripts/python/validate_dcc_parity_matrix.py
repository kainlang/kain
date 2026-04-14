#!/usr/bin/env python3
"""
Validate the flagship DCC parity matrix.

The matrix is intentionally machine-readable so future agents and tooling can
reason about parity claims without scraping prose or cargo-culting status
language from memory.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


ALLOWED_DOMAINS = {"shared", "sculpt", "painter"}
ALLOWED_STATUSES = {
    "reference_only",
    "scaffolded",
    "prototype",
    "partial",
    "validated",
}
ALLOWED_HOOK_PREFIXES = {"benchmark", "doc", "manual", "scenario", "script", "test"}
FEATURE_ID_PATTERN = re.compile(r"^(shared|sculpt|painter)\.[a-z0-9_]+(?:\.[a-z0-9_]+)*$")


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def repo_relative_exists(repo_root: Path, relative_path: str) -> bool:
    return (repo_root / relative_path).exists()


def validate_hook(repo_root: Path, hook: str) -> str | None:
    if ":" not in hook:
        return f"validation hook '{hook}' must use '<kind>:<value>' format"

    kind, value = hook.split(":", 1)
    if kind not in ALLOWED_HOOK_PREFIXES:
        return f"validation hook '{hook}' uses unsupported kind '{kind}'"
    if not value.strip():
        return f"validation hook '{hook}' must include a non-empty value"
    if kind in {"doc", "script"} and not repo_relative_exists(repo_root, value):
        return f"validation hook '{hook}' points to a missing repo path"
    return None


def validate_parity_matrix(
    matrix: dict[str, Any],
    repo_root: Path,
    runtime_lane_ids: set[str],
) -> list[str]:
    errors: list[str] = []

    if not isinstance(matrix.get("schema_version"), int):
        errors.append("matrix must define integer 'schema_version'")

    capabilities = matrix.get("capabilities")
    if not isinstance(capabilities, list) or not capabilities:
        errors.append("matrix must define a non-empty 'capabilities' list")
        return errors

    seen_ids: set[str] = set()

    for index, capability in enumerate(capabilities):
        prefix = f"capabilities[{index}]"
        if not isinstance(capability, dict):
            errors.append(f"{prefix} must be an object")
            continue

        capability_id = capability.get("id", "")
        if not isinstance(capability_id, str) or not FEATURE_ID_PATTERN.match(capability_id):
            errors.append(f"{prefix}.id must match domain-prefixed feature id format")
        elif capability_id in seen_ids:
            errors.append(f"duplicate capability id '{capability_id}'")
        else:
            seen_ids.add(capability_id)

        domain = capability.get("domain")
        if domain not in ALLOWED_DOMAINS:
            errors.append(f"{prefix}.domain must be one of {sorted(ALLOWED_DOMAINS)}")

        status = capability.get("status")
        if status not in ALLOWED_STATUSES:
            errors.append(f"{prefix}.status must be one of {sorted(ALLOWED_STATUSES)}")

        for field_name in ("label", "family", "summary", "gap_summary"):
            value = capability.get(field_name)
            if not isinstance(value, str) or not value.strip():
                errors.append(f"{prefix}.{field_name} must be a non-empty string")

        reference_sources = capability.get("reference_sources")
        if not isinstance(reference_sources, list) or not reference_sources:
            errors.append(f"{prefix}.reference_sources must be a non-empty list")
        else:
            for source_index, source in enumerate(reference_sources):
                source_prefix = f"{prefix}.reference_sources[{source_index}]"
                if not isinstance(source, dict):
                    errors.append(f"{source_prefix} must be an object")
                    continue
                path = source.get("path")
                feature = source.get("feature")
                if not isinstance(path, str) or not path.strip():
                    errors.append(f"{source_prefix}.path must be a non-empty string")
                elif not repo_relative_exists(repo_root, path):
                    errors.append(f"{source_prefix}.path points to a missing repo path '{path}'")
                if not isinstance(feature, str) or not feature.strip():
                    errors.append(f"{source_prefix}.feature must be a non-empty string")

        kain_surfaces = capability.get("kain_surfaces")
        if not isinstance(kain_surfaces, list) or not kain_surfaces:
            errors.append(f"{prefix}.kain_surfaces must be a non-empty list")
        else:
            for surface in kain_surfaces:
                if not isinstance(surface, str) or not surface.strip():
                    errors.append(f"{prefix}.kain_surfaces entries must be non-empty strings")
                elif not repo_relative_exists(repo_root, surface):
                    errors.append(f"{prefix}.kain_surfaces points to a missing repo path '{surface}'")

        lane_ids = capability.get("runtime_lane_ids")
        if not isinstance(lane_ids, list) or not lane_ids:
            errors.append(f"{prefix}.runtime_lane_ids must be a non-empty list")
        else:
            for lane_id in lane_ids:
                if not isinstance(lane_id, str) or lane_id not in runtime_lane_ids:
                    errors.append(
                        f"{prefix}.runtime_lane_ids contains unknown runtime lane '{lane_id}'"
                    )

        hooks = capability.get("validation_hooks")
        if not isinstance(hooks, list) or not hooks:
            errors.append(f"{prefix}.validation_hooks must be a non-empty list")
        else:
            for hook in hooks:
                if not isinstance(hook, str):
                    errors.append(f"{prefix}.validation_hooks entries must be strings")
                    continue
                hook_error = validate_hook(repo_root, hook)
                if hook_error:
                    errors.append(hook_error)

    return errors


def build_summary(matrix: dict[str, Any]) -> dict[str, Any]:
    domain_counts: Counter[str] = Counter()
    status_counts: Counter[str] = Counter()
    domain_status_counts: dict[str, Counter[str]] = defaultdict(Counter)

    for capability in matrix.get("capabilities", []):
        domain = capability["domain"]
        status = capability["status"]
        domain_counts[domain] += 1
        status_counts[status] += 1
        domain_status_counts[domain][status] += 1

    return {
        "capability_count": len(matrix.get("capabilities", [])),
        "domains": dict(domain_counts),
        "statuses": dict(status_counts),
        "domain_statuses": {
            domain: dict(counter) for domain, counter in domain_status_counts.items()
        },
    }


def validate_app_manifest(repo_root: Path, matrix_path: Path) -> list[str]:
    app_manifest_path = repo_root / "apps/kain-fabric-dcc-suite/config/app_manifest.json"
    app_manifest = load_json(app_manifest_path)
    manifests = app_manifest.get("manifests", {})
    parity_path = manifests.get("dcc_parity_matrix")
    if parity_path != "config/dcc_parity_matrix.json":
        return [
            "apps/kain-fabric-dcc-suite/config/app_manifest.json must expose "
            "'manifests.dcc_parity_matrix = config/dcc_parity_matrix.json'"
        ]
    if (repo_root / "apps/kain-fabric-dcc-suite" / parity_path).resolve() != matrix_path.resolve():
        return ["app manifest parity matrix path does not resolve to the validated matrix file"]
    return []


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    default_matrix = repo_root / "apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json"
    default_runtime_lanes = repo_root / "apps/kain-fabric-dcc-suite/config/runtime_lanes.json"
    parser = argparse.ArgumentParser(description="Validate the flagship DCC parity matrix.")
    parser.add_argument("--repo-root", type=Path, default=repo_root)
    parser.add_argument("--matrix", type=Path, default=default_matrix)
    parser.add_argument("--runtime-lanes", type=Path, default=default_runtime_lanes)
    parser.add_argument("--json", action="store_true", dest="emit_json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    matrix_path = args.matrix.resolve()
    runtime_lanes_path = args.runtime_lanes.resolve()

    if not matrix_path.exists():
        print(f"[ERROR] Missing matrix: {matrix_path}", file=sys.stderr)
        return 1
    if not runtime_lanes_path.exists():
        print(f"[ERROR] Missing runtime lanes: {runtime_lanes_path}", file=sys.stderr)
        return 1

    matrix = load_json(matrix_path)
    runtime_lanes = load_json(runtime_lanes_path)
    runtime_lane_ids = {
        lane["id"] for lane in runtime_lanes.get("runtime_lanes", []) if isinstance(lane, dict)
    }

    errors = validate_parity_matrix(matrix, repo_root, runtime_lane_ids)
    errors.extend(validate_app_manifest(repo_root, matrix_path))
    summary = build_summary(matrix)

    if args.emit_json:
        payload = {"ok": not errors, "errors": errors, "summary": summary}
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        if errors:
            print("[ERROR] DCC parity matrix validation failed")
            for error in errors:
                print(f" - {error}")
        else:
            print("[OK] DCC parity matrix validation passed")
        print(f"  Matrix: {matrix_path}")
        print(f"  Capabilities: {summary['capability_count']}")
        print(f"  Domains: {summary['domains']}")
        print(f"  Statuses: {summary['statuses']}")

    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
