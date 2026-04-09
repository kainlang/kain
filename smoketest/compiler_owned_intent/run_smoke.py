#!/usr/bin/env python3

import json
import pathlib
import re
import shutil
import subprocess
import sys


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


def run_checked(command: list[str], cwd: pathlib.Path) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
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


def main() -> int:
    smoke_dir = pathlib.Path(__file__).resolve().parent
    repo_root = smoke_dir.parent.parent
    source_path = smoke_dir / "compiler_owned_intent.kn"
    output_dir = smoke_dir / "output"
    llvm_output_path = output_dir / "compiler_owned_intent.ll"
    runtime_contract_path = output_dir / "compiler_owned_intent.runtime_contract.json"
    realtime_bundle_path = output_dir / "compiler_owned_intent.realtime_app.json"
    summary_path = output_dir / "summary.txt"

    output_dir.mkdir(parents=True, exist_ok=True)
    kain_binary = resolve_kain_binary(repo_root)

    run_text = run_checked([str(kain_binary), "run", str(source_path)], cwd=repo_root)
    if re.search(r"(^|\s)48(\s|$)", run_text) is None:
        raise RuntimeError(f"Expected kain run to return 48. Output: {run_text}")

    run_checked(
        [str(kain_binary), str(source_path), "-t", "llvm", "-o", str(llvm_output_path)],
        cwd=repo_root,
    )

    if not runtime_contract_path.exists():
        raise RuntimeError(f"Missing runtime contract artifact: {runtime_contract_path}")
    if not realtime_bundle_path.exists():
        raise RuntimeError(f"Missing realtime bundle artifact: {realtime_bundle_path}")

    runtime_contract = json.loads(runtime_contract_path.read_text(encoding="utf-8"))
    realtime_bundle = json.loads(realtime_bundle_path.read_text(encoding="utf-8"))

    if len(runtime_contract.get("patches", [])) < 1:
        raise RuntimeError("Runtime contract is missing patches[] output.")
    if len(runtime_contract.get("converges", [])) < 1:
        raise RuntimeError("Runtime contract is missing converges[] output.")
    if len(runtime_contract.get("worlds", [])) < 1:
        raise RuntimeError("Runtime contract is missing worlds[] output.")
    if len(runtime_contract.get("orchestrations", [])) < 1:
        raise RuntimeError("Runtime contract is missing orchestrations[] output.")
    if len(runtime_contract["worlds"][0].get("surfaces", [])) != 4:
        raise RuntimeError("Runtime contract world surface projection count was not 4.")
    if runtime_contract.get("active_world", {}).get("name") != "Studio":
        raise RuntimeError("Runtime contract active_world did not resolve to Studio.")

    runtime_capabilities = [
        capability["key"] for capability in runtime_contract.get("required_capabilities", [])
    ]
    for capability in [
        "patch.transactions",
        "converge.dispatch",
        "world.native-ui",
        "world.viewport3d",
        "world.web",
        "world.ue5",
        "orchestrate.pipeline",
    ]:
        assert_contains(runtime_capabilities, capability, "runtime capability")

    if len(realtime_bundle.get("patches", [])) < 1:
        raise RuntimeError("Realtime bundle is missing patches[] output.")
    if len(realtime_bundle.get("converges", [])) < 1:
        raise RuntimeError("Realtime bundle is missing converges[] output.")
    if len(realtime_bundle.get("worlds", [])) < 1:
        raise RuntimeError("Realtime bundle is missing worlds[] output.")
    if len(realtime_bundle.get("orchestrations", [])) < 1:
        raise RuntimeError("Realtime bundle is missing orchestrations[] output.")
    if len(realtime_bundle["worlds"][0].get("surfaces", [])) != 4:
        raise RuntimeError("Realtime bundle world surface projection count was not 4.")
    if realtime_bundle.get("active_world", {}).get("name") != "Studio":
        raise RuntimeError("Realtime bundle active_world did not resolve to Studio.")

    tool_caps = list(realtime_bundle.get("tool_caps", []))
    for capability in [
        "patch.transactions",
        "converge.dispatch",
        "world.native-ui",
        "world.viewport3d",
        "world.web",
        "world.ue5",
        "orchestrate.pipeline",
    ]:
        assert_contains(tool_caps, capability, "tool capability")

    summary_lines = [
        f"kain: {kain_binary}",
        "run_result: 48",
        f"llvm_output: {llvm_output_path}",
        f"runtime_contract: {runtime_contract_path}",
        f"realtime_bundle: {realtime_bundle_path}",
    ]
    summary_path.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
