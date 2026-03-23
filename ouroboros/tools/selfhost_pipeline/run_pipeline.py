from __future__ import annotations

import argparse
import json
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from front_errors import extract_front_errors, render_front_errors_markdown


DEFAULT_MANIFEST = Path(r"M:\Code\OuroborosV2\docs\selfhost\pipeline_manifest.json")
DEFAULT_OUT_DIR = Path(r"M:\Code\OuroborosV2\out\selfhost\pipeline")


@dataclass
class StepResult:
    id: str
    command: str
    returncode: int
    success: bool
    log_path: str
    expected_artifacts: list[dict[str, Any]]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def format_template(template: str, defaults: dict[str, str]) -> str:
    return template.format(**defaults)


def ensure_out_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def list_lanes(manifest: dict[str, Any]) -> None:
    for lane in manifest.get("lanes", []):
        print(f"{lane['id']}: {lane.get('description', '')}")


def lane_by_id(manifest: dict[str, Any], lane_id: str) -> dict[str, Any]:
    lane = next((item for item in manifest.get("lanes", []) if item["id"] == lane_id), None)
    if lane is None:
        raise SystemExit(f"Unknown lane: {lane_id}")
    return lane


def expected_artifact_status(expected_artifacts: list[str], defaults: dict[str, str]) -> list[dict[str, Any]]:
    statuses: list[dict[str, Any]] = []
    for artifact in expected_artifacts:
        rendered = format_template(artifact, defaults)
        path = Path(rendered)
        statuses.append(
            {
                "path": path.as_posix(),
                "exists": path.exists(),
            }
        )
    return statuses


def load_json_if_exists(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def blocker_bucket_counts(defaults: dict[str, str]) -> dict[str, int]:
    candidates = [
        Path(defaults["repaired_root"]) / "phase2_repair_report.json",
        Path(defaults["phase2_root"]) / "phase2_report.json",
    ]
    for candidate in candidates:
        payload = load_json_if_exists(candidate)
        if not payload:
            continue
        after = payload.get("after", {}).get("bucket_counts", {})
        before = payload.get("before", {}).get("bucket_counts", {})
        if after:
            return after
        if before:
            return before
    return {}


def front_error_status(defaults: dict[str, str]) -> dict[str, Any]:
    repaired_root = Path(defaults["repaired_root"])
    candidates = [
        repaired_root / "front_errors.json",
        repaired_root / "stage2_workspace" / "front_errors.json",
    ]
    for candidate in candidates:
        if candidate.exists():
            try:
                return json.loads(candidate.read_text(encoding="utf-8"))
            except json.JSONDecodeError:
                pass
    return {"exists": False, "front_errors": [], "bucket_counts": {}}


def stage2_binary_status(defaults: dict[str, str]) -> dict[str, Any]:
    candidates = [
        Path(defaults["repaired_root"]) / "stage2_workspace" / "target" / "debug" / "kain.exe",
        Path(defaults["repaired_root"]) / "stage2_workspace" / "target" / "release" / "kain.exe",
        Path(defaults["phase2_root"]) / "stage2_workspace" / "target" / "debug" / "kain.exe",
        Path(defaults["phase2_root"]) / "stage2_workspace" / "target" / "release" / "kain.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return {"exists": True, "path": candidate.as_posix()}
    return {"exists": False, "path": None}


def run_step(step: dict[str, Any], defaults: dict[str, str], out_dir: Path) -> StepResult:
    step_id = step["id"]
    kind = step["kind"]
    command = format_template(step["command"], defaults)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = out_dir / f"{timestamp}_{step_id}.log"

    if kind == "python":
        args = ["cmd", "/c", f"python {command}"]
    elif kind == "powershell_file":
        args = ["cmd", "/c", f"powershell -NoProfile -ExecutionPolicy Bypass -File {command}"]
    else:
        raise ValueError(f"Unsupported step kind: {kind}")

    timeout = step.get("timeout_seconds")
    result = subprocess.run(
        args,
        capture_output=True,
        text=True,
        errors="ignore",
        timeout=timeout,
    )
    log_path.write_text((result.stdout or "") + (result.stderr or ""), encoding="utf-8")
    if step_id in {"core_check", "full_check"}:
        repaired_root = Path(defaults["repaired_root"])
        taxonomy_path = Path(defaults["repair_docs"]) / "error_taxonomy.json"
        if step_id == "core_check":
            source_log = repaired_root / "stage2_workspace" / "stage2_kain-core_check.log"
        else:
            source_log = repaired_root / "stage2_workspace" / "stage2_build.log"
        front_payload = extract_front_errors(source_log, taxonomy_path)
        (repaired_root / "front_errors.json").write_text(json.dumps(front_payload, indent=2), encoding="utf-8")
        (repaired_root / "front_errors.md").write_text(render_front_errors_markdown(front_payload), encoding="utf-8")
    expected = expected_artifact_status(step.get("expected_logs", []), defaults)
    return StepResult(
        id=step_id,
        command=" ".join(args),
        returncode=result.returncode,
        success=result.returncode == 0,
        log_path=log_path.as_posix(),
        expected_artifacts=expected,
    )


def lane_summary(
    lane_id: str,
    success: bool,
    defaults: dict[str, str],
    steps: list[dict[str, Any]],
    required_artifacts: list[dict[str, Any]],
    executed_lanes: list[str],
) -> dict[str, Any]:
    return {
        "generated_at_utc": utc_now(),
        "lane": lane_id,
        "success": success,
        "executed_lanes": executed_lanes,
        "required_artifacts": required_artifacts,
        "steps": steps,
        "blocker_bucket_counts": blocker_bucket_counts(defaults),
        "front_errors": front_error_status(defaults),
        "stage2_binary": stage2_binary_status(defaults),
    }


def run_lane(
    manifest: dict[str, Any],
    lane_id: str,
    out_dir: Path,
    visited: set[str] | None = None,
    executed: list[str] | None = None,
) -> tuple[int, list[str]]:
    visited = visited or set()
    executed = executed or []
    lane = lane_by_id(manifest, lane_id)
    defaults = {key: str(value) for key, value in manifest.get("defaults", {}).items()}
    ensure_out_dir(out_dir)

    if lane_id in visited:
        return 0, executed
    visited.add(lane_id)

    for dependency in lane.get("dependencies", []):
        code, executed = run_lane(manifest, dependency, out_dir, visited, executed)
        if code != 0:
            return code, executed

    step_results: list[dict[str, Any]] = []
    required = expected_artifact_status(lane.get("required_artifacts", []), defaults)
    if any(not item["exists"] for item in required):
        summary = lane_summary(lane_id, False, defaults, step_results, required, executed.copy())
        summary["failure"] = "missing_required_artifacts"
        summary_path = out_dir / f"{lane_id}_summary.json"
        summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print(json.dumps(summary, indent=2))
        return 2, executed

    continue_on_failure = bool(lane.get("continue_on_failure", False))
    for step in lane.get("steps", []):
        result = run_step(step, defaults, out_dir)
        step_results.append(result.__dict__)
        if not result.success and not continue_on_failure:
            executed.append(lane_id)
            summary = lane_summary(lane_id, False, defaults, step_results, required, executed.copy())
            summary_path = out_dir / f"{lane_id}_summary.json"
            summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
            print(json.dumps(summary, indent=2))
            return result.returncode, executed

    executed.append(lane_id)
    summary = lane_summary(lane_id, True, defaults, step_results, required, executed.copy())
    summary_path = out_dir / f"{lane_id}_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0, executed


def main() -> int:
    parser = argparse.ArgumentParser(description="Manifest-driven Ouroboros selfhost pipeline runner")
    parser.add_argument("command", choices=["list", "run"])
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--lane", default="phase2-core")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    if args.command == "list":
        list_lanes(manifest)
        return 0
    code, _ = run_lane(manifest, args.lane, args.out_dir)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
