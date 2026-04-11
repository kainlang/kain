from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

TOOLS_ROOT = Path(__file__).resolve().parents[1] / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from ouroboros_pathing import discover_workspace_context, executable_candidates


CONTEXT = discover_workspace_context(__file__)
DEFAULT_PIPELINE_ROOT = CONTEXT.ouroboros_root / "out" / "selfhost" / "pipeline"
DEFAULT_PHASE2_ROOT = CONTEXT.ouroboros_root / "out" / "selfhost" / "phase2"
DEFAULT_REPAIRED_ROOT = CONTEXT.ouroboros_root / "out" / "selfhost" / "phase2_repaired"


def read_json_file(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def get_pipeline_step(summary: dict[str, Any] | None, step_id: str) -> dict[str, Any] | None:
    if not summary:
        return None
    for step in summary.get("steps", []):
        if step.get("id") == step_id:
            return step
    return None


def get_duplicate_type_blockers(front_error_source: dict[str, Any] | None) -> list[dict[str, Any]]:
    if not front_error_source:
        return []

    duplicates_by_symbol: dict[str, dict[str, Any]] = {}
    for error in front_error_source.get("front_errors", []):
        if error.get("code") != "E0428":
            continue
        text = error.get("text") or ""
        marker = "the name `"
        if marker not in text or "` is defined multiple times" not in text:
            continue
        symbol = text.split(marker, 1)[1].split("`", 1)[0]
        entry = duplicates_by_symbol.setdefault(
            symbol,
            {
                "symbol": symbol,
                "occurrences": 0,
                "files": set(),
                "lines": set(),
            },
        )
        entry["occurrences"] += 1
        if error.get("file"):
            entry["files"].add(error["file"])
        if error.get("line") is not None:
            entry["lines"].add(int(error["line"]))

    return [
        {
            "symbol": symbol,
            "occurrences": entry["occurrences"],
            "files": sorted(entry["files"]),
            "lines": sorted(entry["lines"]),
        }
        for symbol, entry in sorted(duplicates_by_symbol.items())
    ]


def stage2_binary_status(phase2_root: Path, repaired_root: Path) -> dict[str, Any]:
    candidates: list[Path] = []
    for root in (repaired_root, phase2_root):
        for profile in ("debug", "release"):
            for binary_name in executable_candidates("kain"):
                candidates.append(root / "stage2_workspace" / "target" / profile / binary_name)
    for candidate in candidates:
        if candidate.exists():
            return {"exists": True, "path": candidate.as_posix()}
    return {"exists": False, "path": None}


def main() -> int:
    parser = argparse.ArgumentParser(description="Emit machine-readable Ouroboros selfhost status")
    parser.add_argument("--pipeline-root", type=Path, default=DEFAULT_PIPELINE_ROOT)
    parser.add_argument("--phase2-root", type=Path, default=DEFAULT_PHASE2_ROOT)
    parser.add_argument("--repaired-root", type=Path, default=DEFAULT_REPAIRED_ROOT)
    args = parser.parse_args()

    core_summary = read_json_file(args.pipeline_root / "phase2-core_summary.json")
    full_summary = read_json_file(args.pipeline_root / "phase2-full_summary.json")
    repair_report = read_json_file(args.repaired_root / "phase2_repair_report.json")
    front_errors = read_json_file(args.repaired_root / "front_errors.json")
    phase1_report = read_json_file(CONTEXT.ouroboros_root / "out" / "selfhost" / "phase1_report.json")
    phase2_report = read_json_file(args.phase2_root / "phase2_report.json")
    phase2_repaired_report = read_json_file(args.repaired_root / "phase2_report.json")

    bucket_counts = {}
    if repair_report:
        bucket_counts = (
            repair_report.get("after", {}).get("bucket_counts", {})
            or repair_report.get("before", {}).get("bucket_counts", {})
        )

    hotspots = []
    if repair_report:
        hotspots = list(repair_report.get("files_still_failing_hardest", []))[:20]

    core_check_step = get_pipeline_step(core_summary, "core_check")
    phase2_build_source = phase2_repaired_report
    phase2_build_report_path = args.repaired_root / "phase2_report.json"
    if not phase2_build_source or (
        "stage2_build_log_path" not in phase2_build_source
        and "stage2_build_exit_code" not in phase2_build_source
    ):
        phase2_build_source = phase2_report
        phase2_build_report_path = args.phase2_root / "phase2_report.json"

    front_error_source = front_errors
    if not front_error_source and core_summary:
        front_error_source = core_summary.get("front_errors")
    duplicate_type_blockers = get_duplicate_type_blockers(front_error_source)

    front_blocker = None
    if front_error_source and front_error_source.get("front_errors"):
        front = front_error_source["front_errors"][0]
        front_text = (front.get("text") or "").splitlines()[0].strip() if front.get("text") else None
        front_blocker = {
            "code": front.get("code"),
            "bucket": front.get("bucket"),
            "file": front.get("file"),
            "line": front.get("line"),
            "col": front.get("col"),
            "summary": front_text,
        }

    inventory_inputs = []
    if phase1_report:
        for entry in phase1_report.get("inventory_inputs", []):
            path_text = entry.get("path")
            inventory_inputs.append(
                {
                    "inventory_key": entry.get("inventory_key"),
                    "path": path_text,
                    "byte_size": entry.get("byte_size"),
                    "exists": Path(path_text).exists() if path_text else False,
                }
            )

    payload = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "phase2_core": core_summary,
        "phase2_full": full_summary,
        "phase2_core_check": {
            "success": core_check_step.get("success") if core_check_step else None,
            "returncode": core_check_step.get("returncode") if core_check_step else None,
            "log_path": core_check_step.get("log_path") if core_check_step else None,
        },
        "phase2_build_evidence": {
            "report_path": phase2_build_report_path.as_posix(),
            "stage2_build_success": phase2_build_source.get("stage2_build_success") if phase2_build_source else None,
            "stage2_build_artifact": phase2_build_source.get("stage2_build_artifact") if phase2_build_source else None,
            "stage2_build_log_path": phase2_build_source.get("stage2_build_log_path") if phase2_build_source else None,
            "stage2_build_exit_code": phase2_build_source.get("stage2_build_exit_code") if phase2_build_source else None,
        },
        "phase1_inventory_evidence": {
            "report_path": (CONTEXT.ouroboros_root / "out" / "selfhost" / "phase1_report.json").as_posix(),
            "inventory_dir": phase1_report.get("inventory_dir") if phase1_report else None,
            "inventory_inputs": inventory_inputs,
        },
        "latest_logs": {
            "phase2_core": (args.pipeline_root / "phase2-core_summary.json").as_posix(),
            "phase2_full": (args.pipeline_root / "phase2-full_summary.json").as_posix(),
            "repaired_report": (args.repaired_root / "phase2_repair_report.json").as_posix(),
            "core_check": (args.repaired_root / "stage2_workspace" / "stage2_kain-core_check.log").as_posix(),
            "full_build": (args.repaired_root / "stage2_workspace" / "stage2_build.log").as_posix(),
        },
        "blocker_bucket_counts": bucket_counts,
        "front_blocker": front_blocker,
        "front_errors": front_error_source,
        "duplicate_type_blockers": duplicate_type_blockers,
        "top_blocker_signatures": hotspots,
        "stage2_binary": stage2_binary_status(args.phase2_root, args.repaired_root),
    }
    print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
