from __future__ import annotations

import argparse
import copy
import json
import os
import shutil
import subprocess
import sys
import time
import textwrap
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent.parent
ATTRITION_ROOT = REPO_ROOT / "attrition"
OUT_ROOT = ATTRITION_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build"
REPORT_ROOT = OUT_ROOT / "reports"
DEFAULT_MANIFEST = ATTRITION_ROOT / "attritions.json"
DEFAULT_ROOT_REPORT = ATTRITION_ROOT / "latest.md"


def executable_name(stem: str) -> str:
    return f"{stem}.exe" if os.name == "nt" else stem


def timestamp_utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def resolve_tool(explicit: str | None, env_key: str, default_name: str) -> str:
    if explicit:
        return explicit
    env_value = os.environ.get(env_key)
    if env_value:
        return env_value
    return default_name


def resolve_clang(explicit: str | None) -> str:
    if explicit or os.environ.get("CLANG"):
        return resolve_tool(explicit, "CLANG", "clang")
    bundled = REPO_ROOT / "toolchain" / "llvm" / "bin" / executable_name("clang")
    if bundled.exists():
        return str(bundled.resolve())
    found = shutil.which("clang")
    if found:
        return found
    return "clang"


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def load_toml(path: Path) -> dict[str, Any]:
    def strip_comment(line: str) -> str:
        in_string = False
        escaped = False
        result_chars: list[str] = []
        for ch in line:
            if escaped:
                result_chars.append(ch)
                escaped = False
                continue
            if ch == "\\":
                result_chars.append(ch)
                escaped = True
                continue
            if ch == '"':
                in_string = not in_string
                result_chars.append(ch)
                continue
            if ch == "#" and not in_string:
                break
            result_chars.append(ch)
        return "".join(result_chars).strip()

    def parse_value(text: str) -> Any:
        text = text.strip()
        if text.startswith("[") and text.endswith("]"):
            inner = text[1:-1].strip()
            if not inner:
                return []
            parts: list[str] = []
            current: list[str] = []
            in_string = False
            escaped = False
            for ch in inner:
                if escaped:
                    current.append(ch)
                    escaped = False
                    continue
                if ch == "\\":
                    current.append(ch)
                    escaped = True
                    continue
                if ch == '"':
                    current.append(ch)
                    in_string = not in_string
                    continue
                if ch == "," and not in_string:
                    parts.append("".join(current).strip())
                    current = []
                    continue
                current.append(ch)
            if current:
                parts.append("".join(current).strip())
            return [parse_value(part) for part in parts if part]
        if text.startswith('"') and text.endswith('"'):
            return json.loads(text)
        if text in {"true", "false"}:
            return text == "true"
        try:
            return int(text)
        except ValueError:
            return text

    root: dict[str, Any] = {}
    current_table = root
    pending_key: str | None = None
    pending_lines: list[str] = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = strip_comment(raw_line)
        if not line:
            continue
        if pending_key is not None:
            pending_lines.append(line)
            if line.endswith("]"):
                current_table[pending_key] = parse_value(" ".join(pending_lines))
                pending_key = None
                pending_lines = []
            continue
        if line.startswith("[") and line.endswith("]"):
            table_name = line[1:-1].strip()
            table = root.get(table_name)
            if not isinstance(table, dict):
                table = {}
                root[table_name] = table
            current_table = table
            continue
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if value.startswith("[") and not value.endswith("]"):
            pending_key = key
            pending_lines = [value]
            continue
        current_table[key] = parse_value(value)
    return root


def merged_runtime_options(case_runtime: dict[str, Any], profile_runtime: dict[str, Any]) -> dict[str, Any]:
    merged = dict(case_runtime)
    merged.update(profile_runtime)
    return merged


@dataclass
class BuildProfile:
    name: str
    description: str
    c_flags: list[str]
    runtime: dict[str, Any]


def load_profiles(manifest: dict[str, Any]) -> dict[str, BuildProfile]:
    profiles: dict[str, BuildProfile] = {}
    raw_profiles = manifest.get("profiles", {})
    for name, data in raw_profiles.items():
        profiles[name] = BuildProfile(
            name=name,
            description=str(data.get("description", "")),
            c_flags=[str(flag) for flag in data.get("c_flags", [])],
            runtime={str(k): v for k, v in data.get("runtime", {}).items()},
        )
    return profiles


def run_command(command: list[str], cwd: Path, timeout: int) -> dict[str, Any]:
    started = time.perf_counter()
    result = subprocess.run(
        command,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        timeout=timeout,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    return {
        "command": command,
        "cwd": str(cwd),
        "stdout": result.stdout,
        "stderr": result.stderr,
        "returncode": result.returncode,
        "elapsed_ms": elapsed_ms,
    }


def runtime_manifest_paths(manifest_path: Path) -> tuple[list[Path], list[Path], list[str], list[str]]:
    data = load_toml(manifest_path)
    root = manifest_path.parent
    sources = [root / str(entry) for entry in data.get("sources", [])]
    include_dirs = [root / str(entry) for entry in data.get("include_dirs", [])]
    if os.name == "nt":
        sources.extend(root / str(entry) for entry in data.get("windows_sources", []))
        defines = [str(entry) for entry in data.get("windows_defines", [])]
        link_libraries = [str(entry) for entry in data.get("windows_link_libraries", [])]
    elif sys.platform == "darwin":
        sources.extend(root / str(entry) for entry in data.get("macos_sources", []))
        defines = []
        link_libraries = [str(entry) for entry in data.get("macos_link_libraries", [])]
    else:
        sources.extend(root / str(entry) for entry in data.get("linux_sources", []))
        defines = [str(entry) for entry in data.get("linux_defines", [])]
        link_libraries = [str(entry) for entry in data.get("linux_link_libraries", [])]
    return sources, include_dirs, defines, link_libraries


def build_case(
    case: dict[str, Any],
    profile: BuildProfile,
    clang: str,
    timeout: int,
    manifest_path: Path,
) -> dict[str, Any]:
    case_id = str(case["id"])
    build_dir = BUILD_ROOT / case_id / profile.name
    build_dir.mkdir(parents=True, exist_ok=True)
    exe_path = build_dir / executable_name(case_id)
    sources, include_dirs, defines, link_libraries = runtime_manifest_paths(manifest_path)
    source_path = ATTRITION_ROOT / str(case["source"])
    command = [clang, "-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-function"]
    command.extend(profile.c_flags)
    if os.name == "nt":
        command.extend(["-Xlinker", "/OPT:REF", "-Xlinker", "/OPT:ICF"])
    else:
        command.extend(["-ffunction-sections", "-fdata-sections"])
    for define in defines:
        command.append(f"-D{define}")
    command.append("-I")
    command.append(str((ATTRITION_ROOT / "cases" / "common").resolve()))
    for include_dir in include_dirs:
        command.extend(["-I", str(include_dir.resolve())])
    command.extend(str(path.resolve()) for path in sources)
    command.append(str(source_path.resolve()))
    command.extend(["-o", str(exe_path.resolve())])
    for library in link_libraries:
        command.append(f"-l{library}")
    if not Path(clang).exists() and shutil.which(clang) is None:
        return {
            "ok": False,
            "command": command,
            "error": f"clang executable not found: {clang}",
            "build_dir": str(build_dir),
            "exe_path": str(exe_path),
        }
    result = run_command(command, REPO_ROOT, timeout)
    result["ok"] = result["returncode"] == 0 and exe_path.exists()
    result["build_dir"] = str(build_dir)
    result["exe_path"] = str(exe_path)
    return result


def case_ops(case: dict[str, Any], scale: str, override_ops: int | None) -> int:
    if override_ops is not None:
        return override_ops
    return int(case["ops"][scale])


def runtime_args(
    case: dict[str, Any],
    profile: BuildProfile,
    scale: str,
    seed: int | None,
    sabotage: str | None,
    override_ops: int | None,
) -> dict[str, Any]:
    runtime = merged_runtime_options(
        {str(k): v for k, v in case.get("runtime", {}).items()},
        profile.runtime,
    )
    options: dict[str, Any] = {
        "case_id": str(case["id"]),
        "ops": case_ops(case, scale, override_ops),
        "seed": int(seed if seed is not None else case.get("seed", 1)),
        "determinism_tier": int(case.get("determinism_tier", 1)),
        "virtual_time_enabled": int(runtime.get("virtual_time_enabled", 0)),
        "virtual_time_initial_ms": int(runtime.get("virtual_time_initial_ms", 0)),
        "virtual_time_step_ms": int(runtime.get("virtual_time_step_ms", 1)),
        "poison_on_free": int(runtime.get("poison_on_free", 0)),
        "quarantine_capacity": int(runtime.get("quarantine_capacity", 0)),
        "fragmentation_noise_max_bytes": int(runtime.get("fragmentation_noise_max_bytes", 0)),
        "allocation_fail_after": int(runtime.get("allocation_fail_after", 0)),
        "time_provenance_required": int(runtime.get("time_provenance_required", 0)),
        "sabotage": sabotage or "",
        "expect_failure": int(1 if sabotage and sabotage in case.get("expected_fail_sabotages", []) else 0),
    }
    return options


def attrition_cli_command(
    case_id: str,
    profile_name: str,
    scale: str,
    seed: int,
    sabotage: str,
    ops: int,
) -> str:
    command = ["python", "attrition/run.py", "--case", case_id, "--profile", profile_name, "--scale", scale, "--seed", str(seed), "--ops", str(ops)]
    if sabotage:
        command.extend(["--sabotage", sabotage])
    return " ".join(command)


def run_case_executable(
    exe_path: Path,
    options: dict[str, Any],
    timeout: int,
    cwd: Path,
) -> dict[str, Any]:
    command = [
        str(exe_path.resolve()),
        "--case-id",
        str(options["case_id"]),
        "--ops",
        str(options["ops"]),
        "--seed",
        str(options["seed"]),
        "--determinism-tier",
        str(options["determinism_tier"]),
        "--virtual-time-enabled",
        str(options["virtual_time_enabled"]),
        "--virtual-time-initial-ms",
        str(options["virtual_time_initial_ms"]),
        "--virtual-time-step-ms",
        str(options["virtual_time_step_ms"]),
        "--poison-on-free",
        str(options["poison_on_free"]),
        "--quarantine-capacity",
        str(options["quarantine_capacity"]),
        "--fragmentation-noise-max-bytes",
        str(options["fragmentation_noise_max_bytes"]),
        "--allocation-fail-after",
        str(options["allocation_fail_after"]),
        "--expect-failure",
        str(options["expect_failure"]),
        "--time-provenance-required",
        str(options["time_provenance_required"]),
        "--sabotage",
        str(options["sabotage"]),
    ]
    result = run_command(command, cwd, timeout)
    parsed: dict[str, Any] | None = None
    if result["stdout"].strip():
        try:
            parsed = json.loads(result["stdout"].strip().splitlines()[-1])
        except json.JSONDecodeError:
            parsed = None
    result["parsed"] = parsed
    return result


def failure_family(parsed: dict[str, Any] | None) -> str:
    if not parsed:
        return "unparsed"
    validate_failure = str(parsed.get("validate_failure", "")).strip()
    if validate_failure:
        return f"validate:{validate_failure}"
    run_failure = str(parsed.get("run_failure", "")).strip()
    if run_failure:
        return f"run:{run_failure}"
    return f"status:{parsed.get('overall_status', 'unknown')}"


def minimize_failure(
    case: dict[str, Any],
    profile: BuildProfile,
    exe_path: Path,
    options: dict[str, Any],
    timeout: int,
) -> dict[str, Any] | None:
    if int(options["determinism_tier"]) > 2:
        return None
    if int(options["ops"]) <= 1:
        return None
    initial = run_case_executable(exe_path, options, timeout, REPO_ROOT)
    if initial["parsed"] is None or int(initial["parsed"].get("overall_status", 1)) == 0:
        return None
    lo = 1
    hi = int(options["ops"])
    best_result = initial
    while lo < hi:
        mid = (lo + hi) // 2
        candidate_options = dict(options)
        candidate_options["ops"] = mid
        candidate_result = run_case_executable(exe_path, candidate_options, timeout, REPO_ROOT)
        candidate_failed = candidate_result["parsed"] is not None and int(candidate_result["parsed"].get("overall_status", 1)) != 0
        if candidate_failed:
            best_result = candidate_result
            hi = mid
        else:
            lo = mid + 1
    return {
        "ops": int(best_result["parsed"]["ops"]) if best_result["parsed"] is not None and "ops" in best_result["parsed"] else hi,
        "failure_family": failure_family(best_result["parsed"]),
        "stdout": best_result["stdout"],
        "stderr": best_result["stderr"],
    }


def case_status(parsed: dict[str, Any] | None) -> str:
    if parsed is None:
        return "error"
    expected_failure = bool(parsed.get("expect_failure", 0))
    matched = bool(parsed.get("expected_failure_matched", False))
    passed = bool(parsed.get("passed", False))
    if expected_failure:
        return "expected-fail" if matched else "unexpected-pass"
    return "passed" if passed else "failed"


def ensure_out_dirs() -> None:
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)


def select_cases(manifest: dict[str, Any], case_filter: str | None) -> list[dict[str, Any]]:
    cases = manifest.get("cases", [])
    if not case_filter:
        return cases
    wanted = {part.strip() for part in case_filter.split(",") if part.strip()}
    return [case for case in cases if str(case["id"]) in wanted]


def case_runtime_manifest_path(manifest: dict[str, Any], case: dict[str, Any]) -> Path:
    manifest_value = case.get("runtime_manifest") or manifest.get("runtime_manifest") or "runtime/native_attrition_runtime.toml"
    return REPO_ROOT / str(manifest_value)


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def markdown_report(
    generated_at: str,
    manifest_path: Path,
    profile: BuildProfile,
    scale: str,
    clang: str,
    results: list[dict[str, Any]],
    suite_passed: bool,
) -> str:
    passed_count = sum(1 for result in results if result["status"] == "passed")
    expected_fail_count = sum(1 for result in results if result["status"] == "expected-fail")
    lines = [
        "# Attrition Report",
        "",
        f"- generated_at: `{generated_at}`",
        f"- manifest: `{manifest_path.relative_to(REPO_ROOT)}`",
        f"- profile: `{profile.name}`",
        f"- scale: `{scale}`",
        f"- clang: `{clang}`",
        f"- suite_status: `{'passed' if suite_passed else 'failed'}`",
        f"- passed_cases: `{passed_count}/{len(results)}`",
        f"- expected_fail_cases: `{expected_fail_count}`",
        "",
        "## Cases",
        "",
        "| case | status | ops | runtime_ms | checksum | replay |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]
    for result in results:
        parsed = result.get("parsed") or {}
        lines.append(
            "| "
            f"{result['case_id']} | {result['status']} | {result['options']['ops']} | "
            f"{result['runtime_ms']:.3f} | {parsed.get('checksum', 'n/a')} | "
            f"`{result['replay_command']}` |"
        )
        if result["status"] not in {"passed", "expected-fail"}:
            lines.append("")
            lines.append(f"- failure: `{result.get('failure_family', 'unknown')}`")
        if result.get("minimized_failure"):
            lines.append("")
            lines.append(
                f"- minimized_failure: ops `{result['minimized_failure']['ops']}` family "
                f"`{result['minimized_failure']['failure_family']}`"
            )
    lines.append("")
    return "\n".join(lines)


def root_snapshot_markdown(generated_at: str, profile_name: str, scale: str, suite_passed: bool, report_name: str) -> str:
    return textwrap.dedent(
        f"""\
        # Attrition Latest

        - generated_at: `{generated_at}`
        - profile: `{profile_name}`
        - scale: `{scale}`
        - suite_status: `{'passed' if suite_passed else 'failed'}`
        - detailed_report: `attrition/out/reports/{report_name}`
        """
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Kain attrition pipeline.")
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--case")
    parser.add_argument("--profile", default="release-instrumented")
    parser.add_argument("--scale", default="small", choices=["small", "medium", "full"])
    parser.add_argument("--seed", type=int)
    parser.add_argument("--sabotage", default="")
    parser.add_argument("--ops", type=int)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--clang")
    parser.add_argument("--no-minimize", action="store_true")
    args = parser.parse_args()

    ensure_out_dirs()
    manifest_path = Path(args.manifest).resolve()
    manifest = load_json(manifest_path)
    profiles = load_profiles(manifest)
    if args.profile not in profiles:
        raise SystemExit(f"unknown attrition profile: {args.profile}")
    profile = profiles[args.profile]
    clang = resolve_clang(args.clang)
    selected_cases = select_cases(manifest, args.case)
    if not selected_cases:
        raise SystemExit("no attrition cases selected")

    generated_at = timestamp_utc()
    case_results: list[dict[str, Any]] = []
    suite_passed = True

    for case in selected_cases:
        case_id = str(case["id"])
        manifest_runtime_path = case_runtime_manifest_path(manifest, case)
        build_result = build_case(case, profile, clang, args.timeout, manifest_runtime_path)
        options = runtime_args(case, profile, args.scale, args.seed, args.sabotage, args.ops)
        replay_command = attrition_cli_command(
            case_id,
            profile.name,
            args.scale,
            int(options["seed"]),
            str(options["sabotage"]),
            int(options["ops"]),
        )
        case_entry: dict[str, Any] = {
            "case_id": case_id,
            "title": case.get("title", case_id),
            "description": case.get("description", ""),
            "options": options,
            "runtime_manifest": str(manifest_runtime_path.relative_to(REPO_ROOT)),
            "build": build_result,
            "replay_command": replay_command,
        }
        if not build_result.get("ok", False):
            case_entry["status"] = "build-failed"
            case_entry["runtime_ms"] = 0.0
            case_entry["failure_family"] = str(build_result.get("error", "build failed"))
            suite_passed = False
            case_results.append(case_entry)
            continue

        exe_path = Path(build_result["exe_path"])
        run_result = run_case_executable(exe_path, options, args.timeout, REPO_ROOT)
        case_entry["run"] = run_result
        case_entry["runtime_ms"] = float(run_result["elapsed_ms"])
        case_entry["parsed"] = run_result["parsed"]
        case_entry["status"] = case_status(run_result["parsed"])
        case_entry["failure_family"] = failure_family(run_result["parsed"])
        if not args.no_minimize and case_entry["status"] == "failed":
            case_entry["minimized_failure"] = minimize_failure(case, profile, exe_path, options, args.timeout)
        suite_passed = suite_passed and case_entry["status"] in {"passed", "expected-fail"}
        raw_result_path = Path(build_result["build_dir"]) / "last_result.json"
        if run_result["parsed"] is not None:
            write_json(raw_result_path, run_result["parsed"])
        case_results.append(case_entry)

    report_json = {
        "schema_version": 1,
        "report_kind": "attrition_suite_report",
        "generated_at": generated_at,
        "manifest": str(manifest_path.relative_to(REPO_ROOT)),
        "profile": profile.name,
        "scale": args.scale,
        "clang": clang,
        "suite_passed": suite_passed,
        "cases": case_results,
    }
    report_name = f"{generated_at}.json"
    llm_name = f"{generated_at}.llm.md"
    latest_json_name = "latest.json"
    latest_llm_name = "latest.llm.md"
    report_json_path = REPORT_ROOT / report_name
    latest_json_path = REPORT_ROOT / latest_json_name
    report_md_path = REPORT_ROOT / llm_name
    latest_md_path = REPORT_ROOT / latest_llm_name
    write_json(report_json_path, report_json)
    write_json(latest_json_path, report_json)
    markdown = markdown_report(generated_at, manifest_path, profile, args.scale, clang, case_results, suite_passed)
    write_text(report_md_path, markdown)
    write_text(latest_md_path, markdown)
    write_text(
        DEFAULT_ROOT_REPORT,
        root_snapshot_markdown(generated_at, profile.name, args.scale, suite_passed, llm_name),
    )

    return 0 if suite_passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
