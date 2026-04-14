#!/usr/bin/env python3
"""
Coverage tests for the DCC parity harness and parity-summary materializer seam.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_dcc_parity_harness import (  # noqa: E402
    DEFAULT_INCLUDED_STATUSES,
    SCENARIO_HANDLERS,
    SCENARIO_KIND,
    ScenarioContext,
    collect_scenario_entries,
)
from validate_dcc_parity_matrix import load_json, resolve_matrix_path  # noqa: E402


def load_materialize_session_state_module():
    repo_root = Path(__file__).resolve().parents[2]
    module_path = repo_root / "apps/kain-fabric-dcc-suite/scripts/materialize_session_state.py"
    spec = importlib.util.spec_from_file_location("dcc_materialize_session_state", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load module from {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def live_context() -> ScenarioContext:
    repo_root = Path(__file__).resolve().parents[2]
    matrix_path = resolve_matrix_path(repo_root, None, None)
    matrix = load_json(matrix_path)
    return ScenarioContext(repo_root=repo_root, matrix_path=matrix_path, matrix=matrix)


def test_non_reference_only_scenarios_have_registered_handlers() -> None:
    context = live_context()
    entries = collect_scenario_entries(
        context.matrix,
        included_statuses=set(DEFAULT_INCLUDED_STATUSES),
        selected_feature_ids=set(),
        selected_targets=set(),
    )
    missing_targets = sorted(
        hook["target"] for _feature, hook in entries if hook["kind"] == SCENARIO_KIND and hook["target"] not in SCENARIO_HANDLERS
    )
    assert not missing_targets, f"missing parity harness handlers: {missing_targets}"


def test_live_parity_summary_matches_matrix_schema() -> None:
    context = live_context()
    module = load_materialize_session_state_module()
    summary = module.parity_matrix_summary(context.matrix)
    expected_feature_count = len(context.matrix["features"])
    expected_scenario_count = sum(
        1
        for feature in context.matrix["features"]
        for hook in feature.get("validation_hooks", [])
        if hook.get("kind") == SCENARIO_KIND
    )
    assert summary["feature_count"] == expected_feature_count
    assert summary["capability_count"] == expected_feature_count
    assert summary["scenario_count"] == expected_scenario_count
    assert sum(summary["status_counts"].values()) == expected_feature_count
    assert sum(summary["domain_counts"].values()) == expected_feature_count


def main() -> int:
    try:
        test_non_reference_only_scenarios_have_registered_handlers()
        test_live_parity_summary_matches_matrix_schema()
    except AssertionError as error:
        print(f"[FAIL] {error}")
        return 1
    except Exception as error:  # pragma: no cover - test runner fallback
        print(f"[FAIL] {error}")
        return 1

    print("[OK] run_dcc_parity_harness.py tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
