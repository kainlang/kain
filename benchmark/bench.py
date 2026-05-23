#!/usr/bin/env python3
"""
Unified benchmark orchestration control plane.

This command wraps legacy benchmark runners with a structured subcommand
interface, manifest tags/suites selectors, and retention-aware cleanup.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sqlite3
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any


BENCHMARK_ROOT = Path(__file__).resolve().parent
REPO_ROOT = BENCHMARK_ROOT.parent
CATALOG_ROOT = BENCHMARK_ROOT / "catalog"
OUT_ROOT = BENCHMARK_ROOT / "out"
REPORT_ROOT = OUT_ROOT / "reports"
SNAPSHOT_ROOT = OUT_ROOT / "snapshots"
BUILD_ROOT = OUT_ROOT / "build"
BASELINE_ROOT = OUT_ROOT / "baselines"
DEFAULT_MANIFEST = BENCHMARK_ROOT / "benchmarks.json"
DEFAULT_SUITES = CATALOG_ROOT / "suites.json"
DEFAULT_RETENTION = CATALOG_ROOT / "retention.json"
DEFAULT_HISTORY_DB = OUT_ROOT / "history" / "benchmark_history.sqlite3"

RUNNER_DEFAULTS_TO_FLAGS = {
    "warmups": "--warmups",
    "runs": "--runs",
    "timeout": "--timeout",
    "languages": "--languages",
    "latest_stem": "--latest-stem",
    "minimal_name": "--minimal-name",
    "baseline_mode": "--baseline-mode",
}


@dataclass
class Selector:
    case_ids: list[str]
    tags: list[str]
    suites: list[str]


def resolve_existing_path(raw: str | Path, *, fallback_root: Path | None = None) -> Path:
    raw_path = Path(raw)
    if raw_path.is_absolute() and raw_path.exists():
        return raw_path
    candidates = [
        Path.cwd() / raw_path,
        REPO_ROOT / raw_path,
        BENCHMARK_ROOT / raw_path,
    ]
    if fallback_root:
        candidates.append(fallback_root / raw_path)
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    # Return a deterministic absolute fallback for error messages.
    if fallback_root:
        return (fallback_root / raw_path).resolve()
    return (REPO_ROOT / raw_path).resolve()


def parse_csv(raw: str | None) -> list[str]:
    if raw is None:
        return []
    return [part.strip() for part in raw.split(",") if part.strip()]


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_manifest(path: Path) -> dict[str, Any]:
    manifest = load_json(path)
    if not isinstance(manifest, dict):
        raise ValueError(f"Manifest must be a JSON object: {path}")

    include_manifest = manifest.get("include_manifest")
    if include_manifest:
        include_path = (BENCHMARK_ROOT / str(include_manifest)).resolve()
        included_manifest = load_manifest(include_path)
        included_cases = included_manifest.get("cases", [])
        included_by_id = {
            case["id"]: case
            for case in included_cases
            if isinstance(case, dict) and "id" in case
        }
        requested_case_ids = manifest.get("case_ids")
        override_cases = {
            case["id"]: case
            for case in manifest.get("cases", [])
            if isinstance(case, dict) and "id" in case
        }
        if requested_case_ids is None:
            selected_case_ids = [
                case["id"]
                for case in included_cases
                if isinstance(case, dict) and "id" in case
            ]
        else:
            selected_case_ids = [str(case_id) for case_id in requested_case_ids]

        selected_cases: list[dict[str, Any]] = []
        missing_case_ids = [case_id for case_id in selected_case_ids if case_id not in included_by_id]
        if missing_case_ids:
            raise ValueError(
                f"unknown included case(s) in {path.name}: {', '.join(missing_case_ids)}"
            )
        for case_id in selected_case_ids:
            merged_case = dict(included_by_id[case_id])
            override_case = override_cases.get(case_id)
            if override_case:
                for key, value in override_case.items():
                    if (
                        key in {"languages", "language_notes"}
                        and isinstance(merged_case.get(key), dict)
                        and isinstance(value, dict)
                    ):
                        merged_case[key] = {**merged_case[key], **value}
                    else:
                        merged_case[key] = value
            selected_cases.append(merged_case)
        manifest["cases"] = selected_cases

    if "cases" not in manifest or not isinstance(manifest["cases"], list):
        raise ValueError(f"manifest must contain a cases array: {path}")
    return manifest


def case_tags(case: dict[str, Any]) -> set[str]:
    tags = case.get("tags")
    if isinstance(tags, list):
        return {str(tag) for tag in tags}
    return set()


def case_suites(case: dict[str, Any]) -> set[str]:
    suites = case.get("suites")
    if isinstance(suites, list):
        return {str(suite) for suite in suites}
    return set()


def ordered_case_ids(manifest: dict[str, Any]) -> list[str]:
    return [str(case["id"]) for case in manifest.get("cases", []) if isinstance(case, dict) and "id" in case]


def indexed_cases(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(case["id"]): case
        for case in manifest.get("cases", [])
        if isinstance(case, dict) and "id" in case
    }


def select_case_ids(
    manifest: dict[str, Any],
    *,
    case_ids: list[str],
    tags: list[str],
    suites: list[str],
) -> list[str] | None:
    if not case_ids and not tags and not suites:
        return None

    order = ordered_case_ids(manifest)
    cases = indexed_cases(manifest)

    missing_cases = [case_id for case_id in case_ids if case_id not in cases]
    if missing_cases:
        raise ValueError(f"unknown case id(s): {', '.join(missing_cases)}")

    selected: set[str] = set(order)

    if suites:
        selected &= {
            case_id
            for case_id, case in cases.items()
            if any(suite in case_suites(case) for suite in suites)
        }

    if tags:
        selected &= {
            case_id
            for case_id, case in cases.items()
            if all(tag in case_tags(case) for tag in tags)
        }

    if case_ids:
        requested = set(case_ids)
        if suites or tags:
            selected &= requested
        else:
            selected = requested

    return [case_id for case_id in order if case_id in selected]


def load_suites(path: Path) -> dict[str, Any]:
    payload = load_json(path)
    if not isinstance(payload, dict):
        raise ValueError(f"Suite catalog must be a JSON object: {path}")
    suites = payload.get("suites")
    if not isinstance(suites, dict):
        raise ValueError(f"Suite catalog is missing a suites object: {path}")
    return payload


def suite_selector_from_config(suite_cfg: dict[str, Any]) -> Selector:
    selector_cfg = suite_cfg.get("selectors")
    if not isinstance(selector_cfg, dict):
        return Selector(case_ids=[], tags=[], suites=[])
    return Selector(
        case_ids=[str(item) for item in selector_cfg.get("case_ids", []) if str(item).strip()],
        tags=[str(item) for item in selector_cfg.get("tags", []) if str(item).strip()],
        suites=[str(item) for item in selector_cfg.get("suites", []) if str(item).strip()],
    )


def add_optional_arg(command: list[str], flag: str, value: Any) -> None:
    if value is None:
        return
    command.extend([flag, str(value)])


def add_optional_flag(command: list[str], flag: str, enabled: bool) -> None:
    if enabled:
        command.append(flag)


def build_run_command(
    args: argparse.Namespace,
    *,
    selected_case_ids: list[str] | None,
    suite_defaults: dict[str, Any] | None,
    suite_manifest: str | None,
) -> list[str]:
    command = [sys.executable, str(BENCHMARK_ROOT / "run.py")]

    manifest_value = args.manifest or suite_manifest or str(DEFAULT_MANIFEST.relative_to(REPO_ROOT))
    add_optional_arg(command, "--manifest", manifest_value)

    if selected_case_ids is not None:
        if not selected_case_ids:
            raise ValueError("selector resolved to zero benchmark cases")
        add_optional_arg(command, "--case", ",".join(selected_case_ids))

    effective_defaults = suite_defaults or {}

    def apply_default_or_arg(default_key: str, arg_key: str, flag: str) -> None:
        arg_value = getattr(args, arg_key)
        if arg_value is not None:
            add_optional_arg(command, flag, arg_value)
            return
        default_value = effective_defaults.get(default_key)
        if default_value is not None:
            add_optional_arg(command, flag, default_value)

    apply_default_or_arg("languages", "languages", "--languages")
    apply_default_or_arg("runs", "runs", "--runs")
    apply_default_or_arg("warmups", "warmups", "--warmups")
    apply_default_or_arg("timeout", "timeout", "--timeout")
    apply_default_or_arg("latest_stem", "latest_stem", "--latest-stem")
    apply_default_or_arg("minimal_name", "minimal_name", "--minimal-name")
    apply_default_or_arg("baseline_mode", "baseline_mode", "--baseline-mode")

    add_optional_arg(command, "--kain-exe", args.kain_exe)
    add_optional_arg(command, "--rustc", args.rustc)
    add_optional_arg(command, "--cxx", args.cxx)
    add_optional_arg(command, "--clang", args.clang)
    add_optional_arg(command, "--zig", args.zig)
    add_optional_arg(command, "--go", args.go)
    add_optional_arg(command, "--erl", args.erl)
    add_optional_arg(command, "--erlc", args.erlc)
    add_optional_arg(command, "--node", args.node)
    add_optional_arg(command, "--python", args.python)

    add_optional_arg(command, "--kain-native-profile", args.kain_native_profile)
    add_optional_arg(command, "--kain-native-opt-level", args.kain_native_opt_level)
    add_optional_arg(command, "--kain-native-target-cpu", args.kain_native_target_cpu)
    add_optional_arg(command, "--kain-native-debug-info", args.kain_native_debug_info)

    history_db = args.history_db
    if history_db is None and "history_db" in effective_defaults:
        history_db = effective_defaults["history_db"]
    add_optional_arg(command, "--history-db", history_db)

    add_optional_flag(command, "--no-build", args.no_build)
    return command


def run_subcommand(args: argparse.Namespace) -> int:
    manifest_path = resolve_existing_path(args.manifest or DEFAULT_MANIFEST, fallback_root=REPO_ROOT)
    manifest = load_manifest(manifest_path)

    suites_path = resolve_existing_path(args.suites or DEFAULT_SUITES, fallback_root=REPO_ROOT)
    suite_catalog = load_suites(suites_path)
    suite_map = suite_catalog.get("suites", {})

    selector_case_ids = parse_csv(args.case)
    selector_tags = parse_csv(args.tag)
    selector_suites = parse_csv(args.suite)

    suite_defaults: dict[str, Any] = {}
    suite_manifest: str | None = None
    for suite_name in selector_suites:
        suite_cfg = suite_map.get(suite_name)
        if not isinstance(suite_cfg, dict):
            raise ValueError(f"unknown suite: {suite_name}")
        suite_defaults.update(suite_cfg.get("defaults", {}))
        if suite_manifest is None and suite_cfg.get("manifest"):
            suite_manifest = str(suite_cfg["manifest"])

    # Expand suite selectors from the catalog before explicit filtering.
    expanded_suite_selectors: list[str] = []
    expanded_suite_tags: list[str] = []
    expanded_suite_case_ids: list[str] = []
    for suite_name in selector_suites:
        suite_cfg = suite_map.get(suite_name)
        if not isinstance(suite_cfg, dict):
            continue
        selector = suite_selector_from_config(suite_cfg)
        expanded_suite_selectors.extend(selector.suites)
        expanded_suite_tags.extend(selector.tags)
        expanded_suite_case_ids.extend(selector.case_ids)

    effective_case_ids = selector_case_ids + expanded_suite_case_ids
    effective_tags = selector_tags + expanded_suite_tags
    effective_suites = expanded_suite_selectors

    selected_case_ids = select_case_ids(
        manifest,
        case_ids=effective_case_ids,
        tags=effective_tags,
        suites=effective_suites,
    )

    command = build_run_command(
        args,
        selected_case_ids=selected_case_ids,
        suite_defaults=suite_defaults,
        suite_manifest=suite_manifest,
    )

    if args.print_command:
        print(" ".join(command))
    completed = subprocess.run(command, cwd=str(REPO_ROOT))
    return completed.returncode


def suite_subcommand(args: argparse.Namespace) -> int:
    suites_path = resolve_existing_path(args.suites or DEFAULT_SUITES, fallback_root=REPO_ROOT)
    suite_catalog = load_suites(suites_path)
    suite_cfg = suite_catalog.get("suites", {}).get(args.suite_name)
    if not isinstance(suite_cfg, dict):
        available = ", ".join(sorted(suite_catalog.get("suites", {}).keys())) or "none"
        raise ValueError(f"unknown suite '{args.suite_name}'. Available suites: {available}")

    runner_rel = str(suite_cfg.get("runner", "run.py"))
    runner_path = resolve_existing_path(BENCHMARK_ROOT / runner_rel)

    defaults = suite_cfg.get("defaults", {})
    if not isinstance(defaults, dict):
        defaults = {}

    command = [sys.executable, str(runner_path)]

    if runner_path.name == "run.py" and runner_path.parent == BENCHMARK_ROOT:
        manifest_rel = str(suite_cfg.get("manifest", DEFAULT_MANIFEST.relative_to(REPO_ROOT)))
        add_optional_arg(command, "--manifest", manifest_rel)

        manifest_path = resolve_existing_path(manifest_rel, fallback_root=REPO_ROOT)
        manifest = load_manifest(manifest_path)
        selector = suite_selector_from_config(suite_cfg)
        selected_case_ids = select_case_ids(
            manifest,
            case_ids=selector.case_ids,
            tags=selector.tags,
            suites=selector.suites,
        )
        if selected_case_ids is not None:
            if not selected_case_ids:
                raise ValueError(f"suite '{args.suite_name}' resolved to zero cases")
            add_optional_arg(command, "--case", ",".join(selected_case_ids))

    for key, flag in RUNNER_DEFAULTS_TO_FLAGS.items():
        value = defaults.get(key)
        if value is not None:
            add_optional_arg(command, flag, value)

    forwarded_args = list(args.forwarded_args)
    if forwarded_args and forwarded_args[0] == "--":
        forwarded_args = forwarded_args[1:]
    command.extend(forwarded_args)

    if args.print_command:
        print(" ".join(command))
    completed = subprocess.run(command, cwd=str(REPO_ROOT))
    return completed.returncode


def list_subcommand(args: argparse.Namespace) -> int:
    manifest_path = resolve_existing_path(args.manifest or DEFAULT_MANIFEST, fallback_root=REPO_ROOT)
    suites_path = resolve_existing_path(args.suites or DEFAULT_SUITES, fallback_root=REPO_ROOT)
    manifest = load_manifest(manifest_path)
    suite_catalog = load_suites(suites_path)

    cases = indexed_cases(manifest)
    ordered = ordered_case_ids(manifest)

    tag_counts: dict[str, int] = {}
    suite_counts: dict[str, int] = {}
    for case_id in ordered:
        case = cases[case_id]
        for tag in case_tags(case):
            tag_counts[tag] = tag_counts.get(tag, 0) + 1
        for suite in case_suites(case):
            suite_counts[suite] = suite_counts.get(suite, 0) + 1

    if args.format == "json":
        payload = {
            "manifest": str(manifest_path),
            "suite_catalog": str(suites_path),
            "case_count": len(ordered),
            "tags": tag_counts,
            "suites": suite_counts,
            "cases": [
                {
                    "id": case_id,
                    "title": str(cases[case_id].get("title", case_id)),
                    "tags": sorted(case_tags(cases[case_id])),
                    "suites": sorted(case_suites(cases[case_id])),
                    "default_enabled": bool(cases[case_id].get("default_enabled", True)),
                }
                for case_id in ordered
            ],
            "catalog_suites": sorted(suite_catalog.get("suites", {}).keys()),
        }
        print(json.dumps(payload, indent=2))
        return 0

    print(f"manifest: {manifest_path}")
    print(f"suite_catalog: {suites_path}")
    print(f"cases: {len(ordered)}")
    print("")
    print("catalog suites:")
    for suite_name in sorted(suite_catalog.get("suites", {}).keys()):
        desc = str(suite_catalog["suites"][suite_name].get("description", ""))
        suffix = f" - {desc}" if desc else ""
        print(f"  {suite_name}{suffix}")
    print("")
    print("tags:")
    for tag in sorted(tag_counts):
        print(f"  {tag}: {tag_counts[tag]}")
    print("")
    print("manifest suite membership:")
    for suite in sorted(suite_counts):
        print(f"  {suite}: {suite_counts[suite]}")
    print("")
    print("cases:")
    for case_id in ordered:
        case = cases[case_id]
        print(
            f"  {case_id}"
            f" | tags={','.join(sorted(case_tags(case))) or '-'}"
            f" | suites={','.join(sorted(case_suites(case))) or '-'}"
            f" | default_enabled={bool(case.get('default_enabled', True))}"
        )
    return 0


def report_subcommand(args: argparse.Namespace) -> int:
    stem = args.stem
    json_path = REPORT_ROOT / f"{stem}.json"
    llm_path = REPORT_ROOT / f"{stem}.llm.md"
    out_snapshot = SNAPSHOT_ROOT / f"{stem}.md"
    legacy_snapshot = BENCHMARK_ROOT / f"{stem}.md"

    print(f"stem: {stem}")
    for label, path in [
        ("json", json_path),
        ("llm", llm_path),
        ("snapshot_out", out_snapshot),
        ("snapshot_legacy", legacy_snapshot),
    ]:
        print(f"{label}: {path} | exists={path.exists()}")

    if args.list_recent:
        print("")
        print("recent reports:")
        if REPORT_ROOT.exists():
            recent = sorted(
                [path for path in REPORT_ROOT.glob("*.json") if path.is_file()],
                key=lambda path: path.stat().st_mtime,
                reverse=True,
            )[: args.list_recent]
            for path in recent:
                stamp = datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc).isoformat()
                print(f"  {path.name} ({stamp})")
        else:
            print("  no reports directory")
    return 0


def collect_kain_medians(report: dict[str, Any]) -> dict[str, float]:
    medians: dict[str, float] = {}
    for case in report.get("cases", []):
        if not isinstance(case, dict):
            continue
        case_id = str(case.get("id", ""))
        if not case_id:
            continue
        run = case.get("run", {})
        if not isinstance(run, dict):
            continue
        kain = run.get("kain", {})
        if not isinstance(kain, dict):
            continue
        median_ms = kain.get("median_ms")
        if median_ms is None:
            continue
        try:
            medians[case_id] = float(median_ms)
        except (TypeError, ValueError):
            continue
    return medians


def find_previous_report(current: Path) -> Path | None:
    if not REPORT_ROOT.exists():
        return None
    candidates = [
        path
        for path in REPORT_ROOT.glob("*.json")
        if path.is_file() and path.resolve() != current.resolve()
    ]
    if not candidates:
        return None
    candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    return candidates[0]


def compare_subcommand(args: argparse.Namespace) -> int:
    current_path = resolve_existing_path(args.current or (REPORT_ROOT / "latest.json"), fallback_root=REPO_ROOT)
    if not current_path.exists():
        raise FileNotFoundError(f"current report missing: {current_path}")

    previous_path: Path | None
    if args.previous:
        previous_path = resolve_existing_path(args.previous, fallback_root=REPO_ROOT)
    else:
        previous_path = find_previous_report(current_path)

    if previous_path is None or not previous_path.exists():
        raise FileNotFoundError("could not resolve a previous report to compare against")

    current_report = load_json(current_path)
    previous_report = load_json(previous_path)
    current_medians = collect_kain_medians(current_report)
    previous_medians = collect_kain_medians(previous_report)

    common = sorted(set(current_medians) & set(previous_medians))
    if not common:
        print(f"current: {current_path}")
        print(f"previous: {previous_path}")
        print("no overlapping Kain case medians found")
        return 0

    print(f"current: {current_path}")
    print(f"previous: {previous_path}")
    print("")
    print("case_id | previous_ms | current_ms | delta_ms | delta_pct | trend")

    regressed = 0
    for case_id in common:
        prev = previous_medians[case_id]
        cur = current_medians[case_id]
        delta = cur - prev
        delta_pct = (delta / prev * 100.0) if prev else 0.0
        trend = "slower" if delta > 0 else "faster" if delta < 0 else "flat"
        if delta > 0:
            regressed += 1
        print(f"{case_id} | {prev:.3f} | {cur:.3f} | {delta:+.3f} | {delta_pct:+.2f}% | {trend}")

    print("")
    print(f"overlapping_cases: {len(common)}")
    print(f"regressed_cases: {regressed}")
    return 0


def load_retention_policy(path: Path, policy_name: str) -> dict[str, Any]:
    payload = load_json(path)
    if not isinstance(payload, dict):
        raise ValueError(f"Retention file must be a JSON object: {path}")
    policies = payload.get("policies")
    if not isinstance(policies, dict):
        raise ValueError(f"Retention file missing 'policies' object: {path}")
    policy = policies.get(policy_name)
    if not isinstance(policy, dict):
        available = ", ".join(sorted(policies.keys())) or "none"
        raise ValueError(f"unknown retention policy '{policy_name}'. Available: {available}")

    inherits = policy.get("inherits")
    if inherits:
        parent = policies.get(str(inherits))
        if not isinstance(parent, dict):
            raise ValueError(f"retention policy '{policy_name}' inherits missing policy '{inherits}'")
        merged = dict(parent)
        merged.update(policy)
        return merged
    return dict(policy)


def older_than(path: Path, *, cutoff: datetime) -> bool:
    modified = datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc)
    return modified < cutoff


def enqueue_remove(path: Path, removed: list[Path], dry_run: bool) -> None:
    removed.append(path)
    if dry_run:
        return
    if path.is_dir():
        shutil.rmtree(path)
    else:
        path.unlink(missing_ok=True)


def prune_tree_by_age(root: Path, *, max_age_days: int, cutoff_now: datetime, dry_run: bool, removed: list[Path]) -> None:
    if max_age_days <= 0 or not root.exists():
        return
    cutoff = cutoff_now - timedelta(days=max_age_days)
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if older_than(path, cutoff=cutoff):
            enqueue_remove(path, removed, dry_run)

    # Remove now-empty directories.
    for directory in sorted([path for path in root.rglob("*") if path.is_dir()], reverse=True):
        if any(directory.iterdir()):
            continue
        enqueue_remove(directory, removed, dry_run)


def clean_subcommand(args: argparse.Namespace) -> int:
    retention_path = resolve_existing_path(args.retention or DEFAULT_RETENTION, fallback_root=REPO_ROOT)
    policy = load_retention_policy(retention_path, args.policy)

    if not bool(policy.get("enabled", True)):
        print(f"retention policy '{args.policy}' is disabled")
        return 0

    dry_run = args.dry_run
    now = datetime.now(timezone.utc)
    removed: list[Path] = []

    root_delete_globs = [str(item) for item in policy.get("root_delete_globs", [])]
    for pattern in root_delete_globs:
        for path in BENCHMARK_ROOT.glob(pattern):
            if path.is_file():
                enqueue_remove(path, removed, dry_run)

    root_delete_exact = [str(item) for item in policy.get("root_delete_exact", [])]
    for name in root_delete_exact:
        path = BENCHMARK_ROOT / name
        if path.exists() and path.is_file():
            enqueue_remove(path, removed, dry_run)

    keep_stems = {str(item) for item in policy.get("report_keep_stems", [])}
    report_max_age_days = int(policy.get("report_max_age_days", 0) or 0)
    report_keep_recent_count = int(policy.get("report_keep_recent_count", 0) or 0)

    report_files: list[Path] = []
    if REPORT_ROOT.exists():
        report_files = [path for path in REPORT_ROOT.glob("*") if path.is_file()]

    keep_files: set[Path] = set()
    for stem in keep_stems:
        keep_files.add(REPORT_ROOT / f"{stem}.json")
        keep_files.add(REPORT_ROOT / f"{stem}.llm.md")

    timestamped_candidates = [path for path in report_files if path not in keep_files]
    timestamped_candidates.sort(key=lambda path: path.stat().st_mtime, reverse=True)

    cutoff = now - timedelta(days=report_max_age_days) if report_max_age_days > 0 else None
    for index, path in enumerate(timestamped_candidates):
        if report_keep_recent_count > 0 and index < report_keep_recent_count:
            continue
        if cutoff is not None and not older_than(path, cutoff=cutoff):
            continue
        enqueue_remove(path, removed, dry_run)

    tmp_delete_globs = [str(item) for item in policy.get("tmp_delete_globs", [])]
    tmp_max_age_days = int(policy.get("tmp_max_age_days", 0) or 0)
    tmp_cutoff = now - timedelta(days=tmp_max_age_days) if tmp_max_age_days > 0 else None
    for pattern in tmp_delete_globs:
        for path in OUT_ROOT.glob(pattern):
            if not path.is_file():
                continue
            if tmp_cutoff is not None and not older_than(path, cutoff=tmp_cutoff):
                continue
            enqueue_remove(path, removed, dry_run)

    build_max_age_days = int(policy.get("build_max_age_days", 0) or 0)
    baseline_max_age_days = int(policy.get("baseline_max_age_days", 0) or 0)
    prune_tree_by_age(BUILD_ROOT, max_age_days=build_max_age_days, cutoff_now=now, dry_run=dry_run, removed=removed)
    prune_tree_by_age(
        BASELINE_ROOT,
        max_age_days=baseline_max_age_days,
        cutoff_now=now,
        dry_run=dry_run,
        removed=removed,
    )

    if not dry_run:
        SNAPSHOT_ROOT.mkdir(parents=True, exist_ok=True)

    print(f"policy: {args.policy}")
    print(f"dry_run: {dry_run}")
    print(f"removed_count: {len(removed)}")
    for path in removed[: args.max_print]:
        print(f"  {path}")
    if len(removed) > args.max_print:
        print(f"  ... {len(removed) - args.max_print} more")
    return 0


def doctor_subcommand(args: argparse.Namespace) -> int:
    required_paths = [
        BENCHMARK_ROOT / "bench.py",
        BENCHMARK_ROOT / "run.py",
        CATALOG_ROOT / "benchmarks.main.json",
        CATALOG_ROOT / "suites.json",
        CATALOG_ROOT / "retention.json",
        BENCHMARK_ROOT / "lanes" / "gpu" / "run_gpu.py",
        BENCHMARK_ROOT / "lanes" / "wasm" / "run.py",
        BENCHMARK_ROOT / "lanes" / "ffi_boundary" / "run.py",
    ]

    failures: list[str] = []
    for path in required_paths:
        if not path.exists():
            failures.append(f"missing required path: {path}")

    manifest = load_manifest(resolve_existing_path(args.manifest or DEFAULT_MANIFEST, fallback_root=REPO_ROOT))
    for case in manifest.get("cases", []):
        if not isinstance(case, dict):
            continue
        case_id = str(case.get("id", ""))
        if not case_id:
            continue
        if not isinstance(case.get("tags"), list) or not case.get("tags"):
            failures.append(f"case missing tags: {case_id}")
        if not isinstance(case.get("suites"), list) or not case.get("suites"):
            failures.append(f"case missing suites: {case_id}")

    history_db = DEFAULT_HISTORY_DB
    history_ok = history_db.exists()
    if history_ok:
        try:
            with sqlite3.connect(history_db) as conn:
                conn.execute("SELECT 1")
        except sqlite3.Error as exc:
            failures.append(f"history db unusable: {history_db} ({exc})")

    root_latest = sorted(BENCHMARK_ROOT.glob("latest*.md"))
    print(f"benchmark_root: {BENCHMARK_ROOT}")
    print(f"catalog_cases: {len(manifest.get('cases', []))}")
    print(f"root_latest_files: {len(root_latest)}")
    print(f"history_db: {history_db} | exists={history_ok}")
    print(f"snapshots_root: {SNAPSHOT_ROOT}")
    print(f"reports_root: {REPORT_ROOT}")

    if failures:
        print("")
        print("doctor_failures:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("doctor_status: PASS")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="Run benchmark cases with selector-aware orchestration")
    run_parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST.relative_to(REPO_ROOT)))
    run_parser.add_argument("--suites", default=str(DEFAULT_SUITES.relative_to(REPO_ROOT)))
    run_parser.add_argument("--case", help="Comma-separated case id selectors")
    run_parser.add_argument("--tag", help="Comma-separated required case tags")
    run_parser.add_argument("--suite", help="Comma-separated suite names from catalog/suites.json")
    run_parser.add_argument("--languages")
    run_parser.add_argument("--runs", type=int)
    run_parser.add_argument("--warmups", type=int)
    run_parser.add_argument("--timeout", type=int)
    run_parser.add_argument("--kain-exe")
    run_parser.add_argument("--rustc")
    run_parser.add_argument("--cxx")
    run_parser.add_argument("--clang")
    run_parser.add_argument("--zig")
    run_parser.add_argument("--go")
    run_parser.add_argument("--erl")
    run_parser.add_argument("--erlc")
    run_parser.add_argument("--node")
    run_parser.add_argument("--python")
    run_parser.add_argument("--kain-native-profile")
    run_parser.add_argument("--kain-native-opt-level")
    run_parser.add_argument("--kain-native-target-cpu")
    run_parser.add_argument("--kain-native-debug-info")
    run_parser.add_argument("--minimal-name")
    run_parser.add_argument("--latest-stem")
    run_parser.add_argument("--history-db")
    run_parser.add_argument("--baseline-mode")
    run_parser.add_argument("--no-build", action="store_true")
    run_parser.add_argument("--print-command", action="store_true")
    run_parser.set_defaults(handler=run_subcommand)

    suite_parser = subparsers.add_parser("suite", help="Run a named suite from catalog/suites.json")
    suite_parser.add_argument("suite_name")
    suite_parser.add_argument("--suites", default=str(DEFAULT_SUITES.relative_to(REPO_ROOT)))
    suite_parser.add_argument("--print-command", action="store_true")
    suite_parser.set_defaults(handler=suite_subcommand)

    list_parser = subparsers.add_parser("list", help="List cases, tags, and suite coverage")
    list_parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST.relative_to(REPO_ROOT)))
    list_parser.add_argument("--suites", default=str(DEFAULT_SUITES.relative_to(REPO_ROOT)))
    list_parser.add_argument("--format", choices=["text", "json"], default="text")
    list_parser.set_defaults(handler=list_subcommand)

    report_parser = subparsers.add_parser("report", help="Resolve report/snapshot paths")
    report_parser.add_argument("--stem", default="latest")
    report_parser.add_argument("--list-recent", type=int, default=0)
    report_parser.set_defaults(handler=report_subcommand)

    compare_parser = subparsers.add_parser("compare", help="Compare Kain medians across two report JSON files")
    compare_parser.add_argument("--current", default=str((REPORT_ROOT / "latest.json").relative_to(REPO_ROOT)))
    compare_parser.add_argument("--previous")
    compare_parser.set_defaults(handler=compare_subcommand)

    clean_parser = subparsers.add_parser("clean", help="Prune benchmark artifacts using retention policy")
    clean_parser.add_argument("--retention", default=str(DEFAULT_RETENTION.relative_to(REPO_ROOT)))
    clean_parser.add_argument("--policy", default="default")
    clean_parser.add_argument("--dry-run", action="store_true")
    clean_parser.add_argument("--max-print", type=int, default=200)
    clean_parser.set_defaults(handler=clean_subcommand)

    doctor_parser = subparsers.add_parser("doctor", help="Validate benchmark folder hygiene and catalog health")
    doctor_parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST.relative_to(REPO_ROOT)))
    doctor_parser.set_defaults(handler=doctor_subcommand)

    args, extras = parser.parse_known_args()
    if args.command == "suite":
        args.forwarded_args = extras
        return args
    if extras:
        parser.error(f"unrecognized arguments: {' '.join(extras)}")
    return args


def main() -> int:
    args = parse_args()
    started = time.perf_counter()
    try:
        code = int(args.handler(args))
    except Exception as exc:
        print(f"bench fatal: {exc}", file=sys.stderr)
        return 1
    elapsed = time.perf_counter() - started
    if getattr(args, "command", "") in {"clean", "doctor", "list", "report", "compare"}:
        print(f"elapsed_s: {elapsed:.3f}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
