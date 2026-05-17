#!/usr/bin/env python3
"""
Focused tests for the release-readiness gate.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from release_readiness_gate import evaluate_policy  # noqa: E402


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def make_policy() -> dict[str, object]:
    return {
        "schema_version": 1,
        "profiles": {
            "quick": {
                "run_hooks": ["benchmark.release_subset", "attrition.release_subset"],
                "evaluate_checks": [
                    "benchmark.release_subset",
                    "attrition.release_subset",
                    "imports.release_cases",
                    "coverage.quick",
                ],
            }
        },
        "hooks": [
            {
                "id": "benchmark.release_subset",
                "cwd": ".",
                "command": ["{python}", "-c", "print('benchmark hook placeholder')"],
                "artifacts": ["reports/benchmark.json"],
            },
            {
                "id": "attrition.release_subset",
                "cwd": ".",
                "command": ["{python}", "-c", "print('attrition hook placeholder')"],
                "artifacts": ["reports/attrition.json"],
            },
        ],
        "checks": [
            {
                "id": "benchmark.release_subset",
                "kind": "benchmark_report",
                "manifest_path": "benchmarks.json",
                "report_path": "reports/benchmark.json",
                "forbidden_fragments": ["not yet parity-safe"],
                "required_cases": [
                    {
                        "id": "ray_sphere_intersection",
                        "required_languages": ["kain"],
                        "allowed_maturity": ["implemented"],
                    },
                    {
                        "id": "gpu_graphics_submit",
                        "required_languages": ["kain"],
                        "allowed_maturity": ["implemented"],
                    },
                ],
            },
            {
                "id": "attrition.release_subset",
                "kind": "attrition_report",
                "report_path": "reports/attrition.json",
                "required_cases": ["kain_float_array_literal_indexing"],
                "require_suite_passed": True,
            },
            {
                "id": "imports.release_cases",
                "kind": "source_imports",
                "rules": [
                    {
                        "id": "imports.rule.graphics",
                        "path": "cases/gpu_graphics_submit/main.kn",
                        "required_imports": ["use std::graphics"],
                    }
                ],
            },
            {
                "id": "coverage.quick",
                "kind": "coverage_matrix",
                "features": [
                    {
                        "id": "native_llvm.float_array_literal_indexing",
                        "label": "Float Array Literal Indexing",
                        "summary": "Dedicated float-array coverage",
                        "current_surfaces": [
                            "cases/ray_sphere_intersection/main.kn",
                            "cases/kain_float_array_literal_indexing/main.kn",
                        ],
                        "owners": [
                            {
                                "subsystem": "benchmark",
                                "owned_paths": ["cases/ray_sphere_intersection/main.kn"],
                            }
                        ],
                        "required_evidence": [
                            "benchmark.case.ray_sphere_intersection",
                            "attrition.case.kain_float_array_literal_indexing",
                        ],
                    },
                    {
                        "id": "root_stdlib.graphics_surface",
                        "label": "Root Stdlib Graphics Surface",
                        "summary": "Graphics import coverage",
                        "current_surfaces": ["cases/gpu_graphics_submit/main.kn"],
                        "owners": [
                            {
                                "subsystem": "benchmark",
                                "owned_paths": ["cases/gpu_graphics_submit/main.kn"],
                            }
                        ],
                        "required_evidence": [
                            "benchmark.case.gpu_graphics_submit",
                            "imports.rule.graphics",
                        ],
                    },
                ],
            },
        ],
    }


def seed_repo(repo_root: Path, ray_note: str) -> dict[str, object]:
    write_json(repo_root / "release/readiness_policy.json", make_policy())
    write_json(repo_root / "benchmarks.json", {"cases": []})
    write_text(repo_root / "cases/gpu_graphics_submit/main.kn", "use std::graphics\nfn main() -> Int:\n    return 0\n")
    write_text(repo_root / "cases/ray_sphere_intersection/main.kn", "fn main() -> Int:\n    return 0\n")
    write_text(repo_root / "cases/kain_float_array_literal_indexing/main.kn", "fn main() -> Int:\n    return 0\n")
    write_json(
        repo_root / "reports/benchmark.json",
        {
            "cases": [
                {
                    "id": "ray_sphere_intersection",
                    "maturity": "implemented",
                    "fairness_note": "Clean geometry row.",
                    "language_notes": {"kain": ray_note},
                    "build": {"kain": {"ok": True}},
                    "run": {"kain": {"ok": True, "median_ms": 1.0}},
                },
                {
                    "id": "gpu_graphics_submit",
                    "maturity": "implemented",
                    "fairness_note": "Clean graphics row.",
                    "language_notes": {"kain": ""},
                    "build": {"kain": {"ok": True}},
                    "run": {"kain": {"ok": True, "median_ms": 2.0}},
                },
            ]
        },
    )
    write_json(
        repo_root / "reports/attrition.json",
        {
            "suite_passed": True,
            "cases": [
                {
                    "case_id": "kain_float_array_literal_indexing",
                    "run": {
                        "parsed": {
                            "passed": True,
                            "overall_status": 0,
                            "run_failure": "",
                            "validate_failure": "",
                        }
                    },
                }
            ],
        },
    )
    return load_policy(repo_root)


def load_policy(repo_root: Path) -> dict[str, object]:
    return json.loads((repo_root / "release/readiness_policy.json").read_text(encoding="utf-8"))


def test_release_gate_accepts_clean_reports() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        repo_root = Path(temp_dir)
        policy = seed_repo(repo_root, ray_note="")
        payload = evaluate_policy(repo_root, policy, "quick", execute_hooks=False)
        assert payload["ok"], f"expected gate to pass, got errors: {payload['errors']}"


def test_release_gate_rejects_forbidden_benchmark_caveat() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        repo_root = Path(temp_dir)
        policy = seed_repo(repo_root, ray_note="literal float-array indexing was not yet parity-safe")
        payload = evaluate_policy(repo_root, policy, "quick", execute_hooks=False)
        assert not payload["ok"], "expected gate to fail when forbidden caveat is present"
        joined = "\n".join(payload["errors"])
        assert "not yet parity-safe" in joined


def main() -> int:
    try:
        test_release_gate_accepts_clean_reports()
        test_release_gate_rejects_forbidden_benchmark_caveat()
    except AssertionError as error:
        print(f"[FAIL] {error}")
        return 1

    print("[OK] release_readiness_gate.py tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
