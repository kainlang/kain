#!/usr/bin/env python3
import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


EXPECTED_BLADES = {
    "blade-workspace-smoke",
    "gpu-compute",
    "native-filter",
    "native-metrics",
    "signal-console",
    "signal-math",
    "synthetic-reporter",
}


def resolve_repo_paths() -> tuple[Path, Path]:
    script_path = Path(__file__).resolve()
    lab_root = script_path.parent.parent
    repo_root = lab_root.parent.parent
    return repo_root, lab_root


def platform_dynlib_name(stem: str) -> str:
    system = platform.system().lower()
    if system == "windows":
        return f"{stem}.dll"
    if system == "darwin":
        return f"lib{stem}.dylib"
    return f"lib{stem}.so"


def find_kain_binary(repo_root: Path) -> str:
    env_binary = os.environ.get("KAIN_BIN")
    if env_binary:
        return env_binary

    binary_name = "kain.exe" if platform.system().lower() == "windows" else "kain"
    repo_binary = repo_root / "target" / "debug" / binary_name
    if repo_binary.exists():
        return str(repo_binary)

    path_binary = shutil.which("kain")
    if path_binary:
        return path_binary

    raise RuntimeError(
        "could not find a kain binary; build one with `cargo build -p cli` or set KAIN_BIN"
    )


def find_clang(repo_root: Path) -> str:
    candidates = [
        os.environ.get("KAIN_CLANG_PATH"),
        shutil.which("clang"),
        str(repo_root / "toolchain" / "llvm" / "bin" / "clang.exe"),
        r"C:\LLVM-21\bin\clang.exe",
        r"C:\Program Files\LLVM\bin\clang.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    raise RuntimeError("could not find clang; set KAIN_CLANG_PATH to a working clang binary")


def smoke_env(repo_root: Path, clang: str) -> dict[str, str]:
    env = os.environ.copy()
    env.setdefault("KAIN_STDLIB_PATH", str(repo_root / "stdlib"))
    env.setdefault("KAIN_RUNTIME_C_PATH", str(repo_root / "runtime" / "kain_runtime.c"))
    env.setdefault(
        "KAIN_RUNTIME_MANIFEST_PATH",
        str(repo_root / "runtime" / "native_runtime.toml"),
    )
    env.setdefault("KAIN_CLANG_PATH", clang)
    llvm_bin = repo_root / "toolchain" / "llvm" / "bin"
    if llvm_bin.exists():
        env["PATH"] = f"{llvm_bin}{os.pathsep}{env.get('PATH', '')}"
    return env


def run_command(
    args: list[str],
    *,
    cwd: Path,
    env: dict[str, str],
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
    if completed.returncode != 0:
        raise RuntimeError(f"command failed with exit code {completed.returncode}: {' '.join(args)}")
    if capture_json:
        try:
            completed.parsed_json = json.loads(extract_json_payload(completed.stdout))
        except json.JSONDecodeError as error:
            raise RuntimeError(f"command did not emit valid JSON: {error}") from error
    return completed


def extract_json_payload(stdout: str) -> str:
    object_index = stdout.find("{")
    array_index = stdout.find("[")
    candidates = [index for index in [object_index, array_index] if index >= 0]
    if not candidates:
        raise RuntimeError("command output did not contain a JSON object or array")
    return stdout[min(candidates):]


def clean_generated_outputs(lab_root: Path, clean_cache: bool) -> None:
    outputs = (lab_root / "outputs").resolve()
    paths = [outputs]
    if clean_cache:
        paths.append((lab_root / ".kain").resolve())
    lab_resolved = lab_root.resolve()
    for path in paths:
        if path.exists():
            if lab_resolved not in [path, *path.parents]:
                raise RuntimeError(f"refusing to remove path outside lab root: {path}")
            shutil.rmtree(path)


def build_native_filter(lab_root: Path, clang: str, env: dict[str, str]) -> Path:
    native_root = lab_root / "blades" / "native_filter" / "native"
    source = native_root / "blade_filter.c"
    output = native_root / platform_dynlib_name("blade_filter")
    command = [clang, "-shared", "-O2"]
    if platform.system().lower() != "windows":
        command.append("-fPIC")
    command.extend([str(source), "-o", str(output)])
    run_command(command, cwd=lab_root, env=env)
    (native_root / "blade_filter.build.txt").write_text(f"{output.name}\n", encoding="utf-8")
    return output


def assert_blade_workspace(kain: str, lab_root: Path, env: dict[str, str]) -> None:
    list_result = run_command(
        [kain, "blades", "list", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    )
    blades = {blade["name"]: blade for blade in list_result.parsed_json["blades"]}
    missing = sorted(EXPECTED_BLADES - set(blades))
    if missing:
        raise RuntimeError(f"blade list missed expected blades: {missing}")

    graph_result = run_command(
        [kain, "blades", "graph", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    )
    edges = {(edge["from"], edge["to"]) for edge in graph_result.parsed_json}
    expected_edges = {
        ("signal-console", "signal-math"),
        ("signal-console", "native-filter"),
        ("signal-console", "native-metrics"),
        ("signal-console", "gpu-compute"),
    }
    missing_edges = sorted(expected_edges - edges)
    if missing_edges:
        raise RuntimeError(f"blade graph missed expected edges: {missing_edges}")

    check_result = run_command(
        [kain, "blades", "check", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    )
    if not check_result.parsed_json["ok"]:
        raise RuntimeError("blade check returned ok=false")

    equip_signal = run_command(
        [kain, "equip", "signal-console", "--path", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    if equip_signal["kind"] != "app" or len(equip_signal["dependencies"]) < 4:
        raise RuntimeError("signal-console equip payload did not include the expected app graph")

    equip_native_metrics = run_command(
        [kain, "equip", "native-metrics", "--path", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    if not equip_native_metrics.get("cargo_manifest"):
        raise RuntimeError("native-metrics did not resolve a Cargo manifest")
    if equip_native_metrics.get("rust_crate_name") != "blade_smoke_native_metrics":
        raise RuntimeError("native-metrics did not expose the expected Rust crate name")

    equip_synthetic = run_command(
        [kain, "equip", "synthetic-reporter", "--path", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    if equip_synthetic.get("discovery_source") != "cargo-manifest":
        raise RuntimeError("synthetic-reporter was not discovered as a Cargo-only blade")

    equip_gpu = run_command(
        [kain, "equip", "gpu-compute", "--path", ".", "--json"],
        cwd=lab_root,
        env=env,
        capture_json=True,
    ).parsed_json
    if "BladeSmokeCopy" not in equip_gpu.get("compute_keys", []):
        raise RuntimeError("gpu-compute did not expose BladeSmokeCopy")


def assert_fabric_and_gpu(kain: str, lab_root: Path, env: dict[str, str], include_vulkan: bool) -> None:
    run_command(
        [kain, "fabric", "validate", "--manifest", "KAIN.fabric.toml"],
        cwd=lab_root,
        env=env,
    )
    run_command(
        [kain, "fabric", "validate", "--manifest", "KAIN.gpu.fabric.toml"],
        cwd=lab_root,
        env=env,
    )
    run_command(
        [kain, "fabric", "run", "--manifest", "KAIN.fabric.toml"],
        cwd=lab_root,
        env=env,
    )

    run_command(
        [
            kain,
            "gpu-artifacts",
            "blades/gpu_compute/shaders/gpu_step.kn",
            "--output",
            "outputs/gpu/blade_smoke_gpu",
        ],
        cwd=lab_root,
        env=env,
    )
    expected_gpu_outputs = [
        lab_root / "outputs" / "gpu" / "blade_smoke_gpu" / "gpu_step.spv",
        lab_root / "outputs" / "gpu" / "blade_smoke_gpu" / "gpu_step.gpu.rs",
        lab_root / "outputs" / "gpu" / "blade_smoke_gpu" / "gpu_step.reflect.json",
    ]
    missing_gpu_outputs = [path for path in expected_gpu_outputs if not path.exists()]
    if missing_gpu_outputs:
        raise RuntimeError(f"gpu artifact command missed outputs: {missing_gpu_outputs}")

    report_root = lab_root / "outputs" / "fabric" / "reports"
    if not report_root.exists() or not any(report_root.rglob("*.json")):
        raise RuntimeError("CPU Fabric run did not emit JSON reports")

    if include_vulkan:
        run_command(
            [kain, "fabric", "run", "--manifest", "KAIN.gpu.fabric.toml"],
            cwd=lab_root,
            env=env,
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the full local Kain blades workspace smoke.")
    parser.add_argument(
        "--include-vulkan",
        action="store_true",
        help="also dispatch the GPU Fabric manifest; requires a working Vulkan compute runtime",
    )
    parser.add_argument(
        "--clean-cache",
        action="store_true",
        help="remove lab-local .kain bridge caches before running",
    )
    args = parser.parse_args()

    repo_root, lab_root = resolve_repo_paths()
    kain = find_kain_binary(repo_root)
    clang = find_clang(repo_root)
    env = smoke_env(repo_root, clang)

    clean_generated_outputs(lab_root, args.clean_cache)
    native_output = build_native_filter(lab_root, clang, env)
    if not native_output.exists():
        raise RuntimeError(f"native sidecar was not produced: {native_output}")

    assert_blade_workspace(kain, lab_root, env)
    assert_fabric_and_gpu(kain, lab_root, env, args.include_vulkan)

    print("PASS: blades workspace smoke completed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
