#!/usr/bin/env python3
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


EXPECTED_BLADES = {
    "build-kn-system-smoke",
    "smoke-helper",
}

EXPECTED_TASK_IDS = {
    "build-kn-system-smoke:check-llvm",
    "build-kn-system-smoke:source-tests",
    "build-kn-system-smoke:z3-proof",
    "build-kn-system-smoke:cargo-helper",
    "build-kn-system-smoke:bridge-c",
    "build-kn-system-smoke:gpu-smoke",
    "build-kn-system-smoke:fabric-validate",
    "build-kn-system-smoke:node-ish",
    "build-kn-system-smoke:bun-ish",
    "build-kn-system-smoke:skip-unavailable",
    "build-kn-system-smoke:root-executable",
    "build-kn-system-smoke:bench-json",
    "build-kn-system-smoke:attrition-json",
    "build-kn-system-smoke:certify",
    "smoke-helper:helper-check",
}


def resolve_repo_paths() -> tuple[Path, Path]:
    script_path = Path(__file__).resolve()
    lab_root = script_path.parent.parent
    repo_root = lab_root.parent.parent
    return repo_root, lab_root


def platform_binary_name(stem: str) -> str:
    return f"{stem}.exe" if platform.system().lower() == "windows" else stem


def find_binary(repo_root: Path, env_key: str, stem: str) -> str:
    env_binary = os.environ.get(env_key)
    if env_binary:
        return env_binary

    repo_binary = repo_root / "target" / "debug" / platform_binary_name(stem)
    if repo_binary.exists():
        return str(repo_binary)

    path_binary = shutil.which(stem)
    if path_binary:
        return path_binary

    raise RuntimeError(
        f"could not find a {stem} binary; build one with `cargo build -p cli` or set {env_key}"
    )


def find_blade_binary(repo_root: Path, kain_binary: str) -> str:
    env_binary = os.environ.get("BLADE_BIN")
    if env_binary:
        return env_binary

    kain_sibling = Path(kain_binary).with_name(platform_binary_name("blade"))
    if kain_sibling.exists():
        return str(kain_sibling)

    return find_binary(repo_root, "BLADE_BIN", "blade")


def smoke_env(repo_root: Path) -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("KAIN_STDLIB_PATH", str(repo_root / "stdlib"))
    env.setdefault("KAIN_RUNTIME_C_PATH", str(repo_root / "runtime" / "runtime.c"))
    env.setdefault(
        "KAIN_RUNTIME_MANIFEST_PATH",
        str(repo_root / "runtime" / "native_runtime.toml"),
    )
    llvm_bin = repo_root / "toolchain" / "llvm" / "bin"
    if llvm_bin.exists():
        env["PATH"] = f"{llvm_bin}{os.pathsep}{env.get('PATH', '')}"
    return env


def extract_json_payload(stdout: str) -> str:
    decoder = json.JSONDecoder()
    fallback_payload: str | None = None
    for index, char in enumerate(stdout):
        if char not in "{[":
            continue
        try:
            _, end = decoder.raw_decode(stdout[index:])
        except json.JSONDecodeError:
            continue
        payload = stdout[index : index + end]
        if not stdout[index + end :].strip():
            return payload
        fallback_payload = payload
    if fallback_payload is None:
        raise RuntimeError("command output did not contain a JSON object or array")
    return fallback_payload


def run_command(
    args: list[str],
    *,
    cwd: Path,
    env: dict[str, str],
    expect_success: bool = True,
    capture_json: bool = False,
) -> subprocess.CompletedProcess:
    print(f"$ {' '.join(args)}", flush=True)
    completed = subprocess.run(
        args,
        cwd=str(cwd),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
    if expect_success and completed.returncode != 0:
        raise RuntimeError(f"command failed with exit code {completed.returncode}: {' '.join(args)}")
    if not expect_success and completed.returncode == 0:
        raise RuntimeError(f"command unexpectedly succeeded: {' '.join(args)}")
    if capture_json:
        completed.parsed_json = json.loads(extract_json_payload(completed.stdout))
    return completed


def clean_generated_outputs(lab_root: Path) -> None:
    paths = [
        (lab_root / ".kain").resolve(),
        (lab_root / "bin").resolve(),
        (lab_root / "outputs").resolve(),
    ]
    lab_resolved = lab_root.resolve()
    for path in paths:
        if not path.exists():
            continue
        if lab_resolved not in [path, *path.parents]:
            raise RuntimeError(f"refusing to remove path outside lab root: {path}")
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()


def assert_workspace_shape(kain: str, lab_root: Path, env: dict[str, str]) -> None:
    blade_list = run_command(
        [kain, "blades", "list", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    names = {blade["name"] for blade in blade_list["blades"]}
    missing = sorted(EXPECTED_BLADES - names)
    if missing:
        raise RuntimeError(f"workspace discovery missed blades: {missing}")

    blade_graph = run_command(
        [kain, "blades", "graph", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    edges = {(edge["from"], edge["to"]) for edge in blade_graph}
    if ("build-kn-system-smoke", "smoke-helper") not in edges:
        raise RuntimeError("workspace graph missed the build-kn-system-smoke -> smoke-helper edge")


def assert_dry_run(blade: str, lab_root: Path, env: dict[str, str]) -> None:
    report = run_command(
        [blade, "build", ".", "--dry-run", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    tasks = {task["id"]: task for task in report["tasks"]}
    missing = sorted(EXPECTED_TASK_IDS - set(tasks))
    if missing:
        raise RuntimeError(f"dry-run report missed tasks: {missing}")
    check = tasks["build-kn-system-smoke:check-llvm"]
    if check["status"] != "planned":
        raise RuntimeError(f"expected planned dry-run status for check task, got {check['status']}")
    if check.get("matrix_axes") != ["target=llvm"]:
        raise RuntimeError(f"check task matrix axes drifted: {check.get('matrix_axes')}")
    if check.get("telemetry") != ["llm.evidence"]:
        raise RuntimeError(f"check task telemetry drifted: {check.get('telemetry')}")
    certify = tasks["build-kn-system-smoke:certify"]
    if certify.get("certifies") != ["build-kn-system-smoke.local"]:
        raise RuntimeError(f"certify subjects drifted: {certify.get('certifies')}")


def find_task(report: dict, task_id: str) -> dict:
    for task in report["tasks"]:
        if task["id"] == task_id:
            return task
    raise RuntimeError(f"build report missed task {task_id}")


def assert_file(path: Path) -> None:
    if not path.exists() or path.stat().st_size == 0:
        raise RuntimeError(f"expected non-empty artifact at {path}")


def assert_actual_build(blade: str, lab_root: Path, env: dict[str, str]) -> None:
    report = run_command(
        [blade, "build", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    if report["status"] != "succeeded":
        raise RuntimeError("blade build report did not succeed")

    for task_id in EXPECTED_TASK_IDS - {"build-kn-system-smoke:skip-unavailable"}:
        task = find_task(report, task_id)
        if task["status"] not in {"succeeded", "cached"}:
            raise RuntimeError(f"task {task_id} did not succeed: {task['status']}")

    skipped = find_task(report, "build-kn-system-smoke:skip-unavailable")
    if skipped["status"] != "skipped":
        raise RuntimeError(f"capability-gated task was expected to skip, got {skipped['status']}")

    root_exe = lab_root / "bin" / platform_binary_name("build-kn-system-smoke")
    assert_file(root_exe)
    assert_file(lab_root / "outputs" / "native" / "smoke_bridge.native")
    assert_file(lab_root / "outputs" / "gpu" / "smoke_shader.spv")
    assert_file(lab_root / "outputs" / "gpu" / "smoke_shader.gpu.rs")
    assert_file(lab_root / "outputs" / "gpu" / "smoke_shader.reflect.json")
    assert_file(lab_root / "outputs" / "gpu" / "smoke_shader.shader_bundle.json")
    assert_file(lab_root / "outputs" / "node" / "node-ish.json")
    assert_file(lab_root / "outputs" / "bun" / "bun-ish.json")
    assert_file(lab_root / "outputs" / "evidence" / "benchmark.json")
    assert_file(lab_root / "outputs" / "evidence" / "attrition.json")

    cargo_task = find_task(report, "build-kn-system-smoke:cargo-helper")
    cargo_output_root = Path(cargo_task["outputs"][0])
    cargo_binary = platform_binary_name("build_kn_system_smoke_cargo")
    matches = list(cargo_output_root.rglob(cargo_binary))
    if not matches:
        raise RuntimeError(f"cargo helper did not materialize {cargo_binary} under {cargo_output_root}")

    root_task = find_task(report, "build-kn-system-smoke:root-executable")
    evidence_reports = [Path(path) for path in root_task["outputs"] if path.endswith("kain-evidence.json")]
    if not evidence_reports:
        raise RuntimeError("root executable task did not expose an evidence report")
    for report_path in evidence_reports:
        assert_file(report_path)

    for evidence_task_id in [
        "build-kn-system-smoke:bench-json",
        "build-kn-system-smoke:attrition-json",
        "build-kn-system-smoke:certify",
    ]:
        task = find_task(report, evidence_task_id)
        report_paths = [Path(path) for path in task["outputs"] if path.endswith("kain-evidence.json")]
        if not report_paths:
            raise RuntimeError(f"{evidence_task_id} missed kain-evidence.json output")
        for report_path in report_paths:
            assert_file(report_path)

    events_path = Path(report["events_path"])
    assert_file(events_path)
    if not any(line.strip() for line in events_path.read_text(encoding="utf-8").splitlines()):
        raise RuntimeError("build event stream was empty")


def assert_failure_fixture(blade: str, fixture_root: Path, env: dict[str, str], needle: str) -> None:
    completed = run_command(
        [blade, "build", ".", "--dry-run", "--json"],
        cwd=fixture_root,
        env=env,
        expect_success=False,
    )
    if needle not in completed.stdout:
        raise RuntimeError(f"expected failure output to contain {needle!r}")


def assert_failure_fixtures(blade: str, lab_root: Path, env: dict[str, str]) -> None:
    fixtures = lab_root / "fixtures"
    assert_failure_fixture(
        blade,
        fixtures / "duplicate-task-ids",
        env,
        "duplicate build task id detected",
    )
    assert_failure_fixture(
        blade,
        fixtures / "output-collision",
        env,
        "build output collision",
    )


def main() -> int:
    repo_root, lab_root = resolve_repo_paths()
    kain = find_binary(repo_root, "KAIN_BIN", "kain")
    blade = find_blade_binary(repo_root, kain)
    env = smoke_env(repo_root)

    clean_generated_outputs(lab_root)
    assert_workspace_shape(kain, lab_root, env)
    assert_dry_run(blade, lab_root, env)
    assert_actual_build(blade, lab_root, env)
    assert_failure_fixtures(blade, lab_root, env)

    print("PASS: build.kn system smoke completed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
