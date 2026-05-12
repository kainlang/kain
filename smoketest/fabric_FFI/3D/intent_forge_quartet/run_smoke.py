#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import pathlib
import shutil
import stat
import subprocess
import sys


def resolve_repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[3]


def resolve_kain_binary(repo_root: pathlib.Path) -> pathlib.Path:
    candidates = [
        repo_root / "target" / "debug" / "kain",
        repo_root / "target" / "release" / "kain",
        repo_root / "target" / "debug" / "kain.exe",
        repo_root / "target" / "release" / "kain.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    from_path = shutil.which("kain")
    if from_path:
        return pathlib.Path(from_path).resolve()
    raise RuntimeError("Unable to resolve the kain binary.")


def build_env(repo_root: pathlib.Path) -> dict[str, str]:
    env = os.environ.copy()
    clang_candidates = [
        repo_root / "toolchain" / "llvm" / "bin" / "clang",
        repo_root / "toolchain" / "llvm" / "bin" / "clang.exe",
    ]
    for candidate in clang_candidates:
        if candidate.exists():
            env["KAIN_CLANG_PATH"] = str(candidate.resolve())
            break
    env["KAIN_STDLIB_PATH"] = str((repo_root / "stdlib").resolve())
    env["KAIN_RUNTIME_C_PATH"] = str((repo_root / "runtime" / "kain_runtime.c").resolve())
    env["KAIN_RUNTIME_MANIFEST_PATH"] = str((repo_root / "runtime" / "native_runtime.toml").resolve())
    return env


def run_checked(command: list[str], cwd: pathlib.Path, env: dict[str, str]) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(command)}\n{result.stdout}{result.stderr}"
        )
    return f"{result.stdout}{result.stderr}"


def assert_contains(values: list[str], expected_value: str, label: str) -> None:
    if expected_value not in values:
        raise RuntimeError(f"Missing expected {label} '{expected_value}'.")


def resolve_native_executable(native_app_dir: pathlib.Path) -> pathlib.Path:
    candidates = []
    for candidate in native_app_dir.iterdir():
        if not candidate.is_file():
            continue
        if candidate.suffix.lower() == ".exe":
            candidates.append(candidate)
            continue
        if candidate.suffix:
            continue
        if os.access(candidate, os.X_OK) or candidate.stat().st_mode & stat.S_IXUSR:
            candidates.append(candidate)
    if not candidates:
        raise RuntimeError(f"Unable to resolve a native executable inside {native_app_dir}")
    return sorted(candidates)[0]


def load_json(path: pathlib.Path) -> dict:
    if not path.exists():
        raise RuntimeError(f"Missing expected JSON artifact: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    smoke_dir = pathlib.Path(__file__).resolve().parent
    repo_root = resolve_repo_root()
    source_path = smoke_dir / "smoke.kn"
    native_app_dir = smoke_dir / "native-app"
    output_dir = smoke_dir / "output"
    summary_path = output_dir / "summary.txt"
    env = build_env(repo_root)
    kain_binary = resolve_kain_binary(repo_root)

    output_dir.mkdir(parents=True, exist_ok=True)

    run_text = run_checked([str(kain_binary), "run", str(source_path)], cwd=repo_root, env=env)
    if "105" not in run_text:
        raise RuntimeError(f"Expected kain run to return 105. Output: {run_text}")

    run_checked(
        [
            str(kain_binary),
            "build",
            "native-ui",
            str(source_path),
            "--app-name",
            "intent-forge-quartet",
            "--window-title",
            "Intent Forge Quartet",
            "-o",
            str(native_app_dir),
        ],
        cwd=repo_root,
        env=env,
    )

    executable_path = resolve_native_executable(native_app_dir)
    runtime_contract = load_json(native_app_dir / "kain_runtime_contract.json")
    realtime_bundle = load_json(native_app_dir / "kain_realtime_app_bundle.json")
    native_bundle = load_json(native_app_dir / "generated" / "native_app_bundle.json")

    if runtime_contract.get("active_world", {}).get("name") != "IntentForge":
        raise RuntimeError("Runtime contract active_world did not resolve to IntentForge.")
    if realtime_bundle.get("active_world", {}).get("name") != "IntentForge":
        raise RuntimeError("Realtime bundle active_world did not resolve to IntentForge.")

    for capability in [
        "patch.transactions",
        "converge.dispatch",
        "world.native-ui",
        "world.viewport3d",
        "world.web",
        "world.ue5",
        "orchestrate.pipeline",
    ]:
        assert_contains(
            [item["key"] for item in runtime_contract.get("required_capabilities", [])],
            capability,
            "runtime capability",
        )
        assert_contains(list(realtime_bundle.get("tool_caps", [])), capability, "tool capability")

    if len(runtime_contract.get("patches", [])) < 2:
        raise RuntimeError("Runtime contract is missing the authored patch bindings.")
    if len(runtime_contract.get("converges", [])) < 1:
        raise RuntimeError("Runtime contract is missing the converge binding.")
    if len(runtime_contract.get("worlds", [])) < 1:
        raise RuntimeError("Runtime contract is missing the world binding.")
    if len(runtime_contract.get("orchestrations", [])) < 1:
        raise RuntimeError("Runtime contract is missing the orchestration binding.")

    if len(realtime_bundle.get("worlds", [])) < 1:
        raise RuntimeError("Realtime bundle is missing the world binding.")
    tree = native_bundle.get("output", {}).get("tree", {})
    root_id = tree.get("root")
    root_node = tree.get("nodes", {}).get(str(root_id))
    if not root_node or len(root_node.get("children", [])) < 1:
        raise RuntimeError("Native app bundle did not materialize a root UI tree.")

    summary_lines = [
        f"kain: {kain_binary}",
        "run_result: 105",
        f"native_executable: {executable_path}",
        f"runtime_contract: {native_app_dir / 'kain_runtime_contract.json'}",
        f"realtime_bundle: {native_app_dir / 'kain_realtime_app_bundle.json'}",
        f"native_bundle: {native_app_dir / 'generated' / 'native_app_bundle.json'}",
    ]
    summary_path.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
