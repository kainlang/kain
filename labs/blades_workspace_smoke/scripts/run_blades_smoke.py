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


def platform_binary_name(stem: str) -> str:
    return f"{stem}.exe" if platform.system().lower() == "windows" else stem


def platform_dynlib_name(stem: str) -> str:
    system = platform.system().lower()
    if system == "windows":
        return f"{stem}.dll"
    if system == "darwin":
        return f"lib{stem}.dylib"
    return f"lib{stem}.so"


def find_binary(repo_root: Path, env_key: str, stem: str) -> str:
    env_binary = os.environ.get(env_key)
    if env_binary:
        return env_binary

    binary_name = platform_binary_name(stem)
    repo_binary = repo_root / "target" / "debug" / binary_name
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
    env.setdefault("KAIN_RUNTIME_C_PATH", str(repo_root / "runtime" / "kain_runtime.c"))
    env.setdefault(
        "KAIN_RUNTIME_MANIFEST_PATH",
        str(repo_root / "runtime" / "native_runtime.toml"),
    )
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


def clean_generated_outputs(lab_root: Path, clean_cache: bool) -> None:
    paths = [(lab_root / "outputs").resolve()]
    if clean_cache:
        paths.append((lab_root / ".kain").resolve())
    lab_resolved = lab_root.resolve()
    for path in paths:
        if path.exists():
            if lab_resolved not in [path, *path.parents]:
                raise RuntimeError(f"refusing to remove path outside lab root: {path}")
            shutil.rmtree(path)


def assert_blade_build(
    blade: str,
    lab_root: Path,
    env: dict[str, str],
    include_vulkan: bool,
    clean_cache: bool,
) -> None:
    args = [blade, "build", ".", "--json"]
    if include_vulkan:
        args.append("--include-vulkan")
    if clean_cache:
        args.append("--clean")
    build_report = run_command(args, cwd=lab_root, env=env, capture_json=True).parsed_json

    if build_report["status"] != "succeeded":
        raise RuntimeError("blade build report did not succeed")

    tasks = {task["id"]: task for task in build_report["tasks"]}
    expected_fragments = [
        "c:native-filter:blade_filter",
        "cargo:native-metrics",
        "cargo:synthetic-reporter",
        "gpu:gpu-compute:gpu_step",
        "gpu:gpu-compute:nebula_field",
        "gpu:gpu-compute:spectral_lattice",
        "blade-check",
        "fabric-validate:kain-fabric",
        "fabric-run:kain-fabric",
        "fabric-validate:kain-gpu-fabric",
    ]
    missing = [fragment for fragment in expected_fragments if not any(fragment in task_id for task_id in tasks)]
    if missing:
        raise RuntimeError(f"blade build report missed expected tasks: {missing}")

    for task in build_report["tasks"]:
        if task["status"] not in {"succeeded", "cached"}:
            raise RuntimeError(f"blade build task did not succeed: {task}")

    native_output = (
        lab_root
        / "blades"
        / "native_filter"
        / "native"
        / platform_dynlib_name("blade_filter")
    )
    if not native_output.exists():
        raise RuntimeError(f"native sidecar was not materialized: {native_output}")

    gpu_outputs = sorted((lab_root / ".kain" / "out").rglob("*.spv"))
    expected_spirv = {"gpu_step.spv", "nebula_field.spv", "spectral_lattice.spv"}
    actual_spirv = {path.name for path in gpu_outputs}
    if not expected_spirv.issubset(actual_spirv):
        raise RuntimeError(f"blade build did not emit expected GPU artifacts: {sorted(expected_spirv - actual_spirv)}")

    report_root = lab_root / "outputs" / "fabric" / "reports"
    if not report_root.exists() or not any(report_root.rglob("*.json")):
        raise RuntimeError("CPU Fabric run did not emit JSON reports")

    if include_vulkan:
        gpu_report_root = lab_root / "outputs" / "gpu-fabric" / "reports"
        if not gpu_report_root.exists() or not any(gpu_report_root.rglob("*.json")):
            raise RuntimeError("GPU Fabric run did not emit JSON reports")

    assert_singularity_atlas(lab_root, env)


def assert_singularity_atlas(lab_root: Path, env: dict[str, str]) -> None:
    executable = find_lab_executable(lab_root, "blade_singularity_atlas")
    output_root = lab_root / "outputs" / "singularity-atlas"
    run_command(
        [
            str(executable),
            "--workspace",
            str(lab_root),
            "--output",
            str(output_root),
        ],
        cwd=lab_root,
        env=env,
    )

    report_path = output_root / "atlas.json"
    html_path = output_root / "index.html"
    svg_path = output_root / "atlas.svg"
    ppm_path = output_root / "atlas.ppm"
    for path in [report_path, html_path, svg_path, ppm_path]:
        if not path.exists() or path.stat().st_size == 0:
            raise RuntimeError(f"singularity atlas did not emit {path}")

    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("shader_count", 0) < 3:
        raise RuntimeError("singularity atlas did not see all shader artifact sets")
    if report.get("total_spirv_bytes", 0) <= 0:
        raise RuntimeError("singularity atlas reported empty SPIR-V output")
    for compute_key in ["BladeSmokeCopy", "BladeNebulaForge", "BladeSpectralLattice"]:
        if compute_key not in report.get("compute_keys", []):
            raise RuntimeError(f"singularity atlas missed compute key {compute_key}")


def find_lab_executable(lab_root: Path, stem: str) -> Path:
    binary_name = platform_binary_name(stem)
    matches = sorted((lab_root / ".kain" / "out").rglob(binary_name))
    if not matches:
        raise RuntimeError(f"could not find lab executable {binary_name} under .kain/out")
    return matches[-1]


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
    for compute_key in ["BladeSmokeCopy", "BladeNebulaForge", "BladeSpectralLattice"]:
        if compute_key not in equip_gpu.get("compute_keys", []):
            raise RuntimeError(f"gpu-compute did not expose {compute_key}")


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
        help="remove lab-local build/cache artifacts before running",
    )
    args = parser.parse_args()

    repo_root, lab_root = resolve_repo_paths()
    kain = find_binary(repo_root, "KAIN_BIN", "kain")
    blade = find_blade_binary(repo_root, kain)
    env = smoke_env(repo_root)

    clean_generated_outputs(lab_root, args.clean_cache)
    assert_blade_build(blade, lab_root, env, args.include_vulkan, args.clean_cache)
    assert_blade_workspace(kain, lab_root, env)

    print("PASS: blades workspace smoke completed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
