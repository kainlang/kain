#!/usr/bin/env python3
"""
Minimal validation checks for the DCC parity matrix validator.
"""

from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from validate_dcc_parity_matrix import resolve_matrix_path, validate_matrix


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def make_feature(identifier: str, status: str = "validated") -> dict[str, object]:
    return {
        "id": identifier,
        "domain": "shared",
        "baseline_families": ["shared_native_dcc"],
        "label": "Example Feature",
        "summary": "Example summary",
        "reference_sources": [
            {
                "path": ".reference/example.md",
                "kind": "product_oracle",
                "note": "Example reference"
            }
        ],
        "current_kain_surfaces": [
            {
                "path": "apps/kain-fabric-dcc-suite/config/example.json",
                "note": "Example owned file"
            }
        ],
        "owners": [
            {
                "subsystem": "example_subsystem",
                "owned_paths": [
                    "apps/kain-fabric-dcc-suite/config/example.json"
                ]
            }
        ],
        "status": status,
        "validation_hooks": [
            {
                "id": f"{identifier}.scenario",
                "kind": "scenario",
                "target": identifier
            }
        ],
        "notes": "Example note"
    }


def make_matrix(feature: dict[str, object]) -> dict[str, object]:
    return {
        "schema_version": 1,
        "status_definitions": [
            {"id": "reference_only", "label": "Reference Only", "summary": "Only in baseline."},
            {"id": "validated", "label": "Validated", "summary": "Validated in Kain."}
        ],
        "domains": [
            {"id": "shared", "label": "Shared", "summary": "Shared domain."}
        ],
        "baseline_families": [
            {
                "id": "shared_native_dcc",
                "label": "Shared Native DCC",
                "sources": [".reference/example.md"]
            }
        ],
        "features": [feature]
    }


def seed_repo(repo_root: Path, matrix: dict[str, object]) -> Path:
    write_json(
        repo_root / "apps/kain-fabric-dcc-suite/config/app_manifest.json",
        {
            "manifests": {
                "dcc_parity_matrix": "config/dcc_parity_matrix.json"
            }
        },
    )
    write_json(repo_root / "apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json", matrix)
    write_json(repo_root / "apps/kain-fabric-dcc-suite/config/example.json", {"ok": True})
    (repo_root / ".reference").mkdir(parents=True, exist_ok=True)
    (repo_root / ".reference/example.md").write_text("example", encoding="utf-8")
    return repo_root / "apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json"


def test_validator_accepts_well_formed_matrix() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        repo_root = Path(temp_dir)
        matrix_path = seed_repo(repo_root, make_matrix(make_feature("shared.example_feature")))
        resolved_path = resolve_matrix_path(repo_root, None, None)
        assert resolved_path == matrix_path.resolve()
        errors, summary = validate_matrix(repo_root, matrix_path)
        assert not errors, f"expected no validation errors, got: {errors}"
        assert summary["feature_count"] == 1
        assert summary["status_counts"]["validated"] == 1


def test_validator_rejects_duplicate_ids_and_missing_paths() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        repo_root = Path(temp_dir)
        feature = make_feature("shared.duplicate_feature")
        broken_feature = make_feature("shared.duplicate_feature")
        broken_feature["current_kain_surfaces"] = [
            {
                "path": "apps/kain-fabric-dcc-suite/config/missing.json",
                "note": "Missing path"
            }
        ]
        broken_feature["owners"] = [
            {
                "subsystem": "broken_subsystem",
                "owned_paths": [
                    "apps/kain-fabric-dcc-suite/config/missing.json"
                ]
            }
        ]
        matrix_path = seed_repo(repo_root, make_matrix(feature))
        matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
        matrix["features"].append(broken_feature)
        matrix_path.write_text(json.dumps(matrix, indent=2), encoding="utf-8")

        errors, _summary = validate_matrix(repo_root, matrix_path)
        joined = "\n".join(errors)
        assert "duplicate feature id" in joined
        assert "path does not exist -> apps/kain-fabric-dcc-suite/config/missing.json" in joined


def main() -> int:
    try:
        test_validator_accepts_well_formed_matrix()
        test_validator_rejects_duplicate_ids_and_missing_paths()
    except AssertionError as error:
        print(f"[FAIL] {error}")
        return 1

    print("[OK] validate_dcc_parity_matrix.py tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
