#!/usr/bin/env python3

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


EXAMPLE_NAME_PATTERN = re.compile(r"\b\d{2}_[a-z0-9_]+\.kn\b")


def load_manifest(manifest_path: Path) -> dict:
    return json.loads(manifest_path.read_text())


def collect_kn_files(examples_dir: Path) -> set[str]:
    return {path.name for path in examples_dir.glob("*.kn")}


def collect_readme_refs(readme_path: Path) -> set[str]:
    matches = EXAMPLE_NAME_PATTERN.findall(readme_path.read_text())
    return set(matches)


def ensure_suite_consistency(examples_dir: Path, manifest: dict) -> None:
    manifest_names = {entry["path"] for entry in manifest["examples"]}
    file_names = collect_kn_files(examples_dir)
    readme_names = collect_readme_refs(examples_dir / "README.md")

    missing_in_manifest = sorted(file_names - manifest_names)
    missing_on_disk = sorted(manifest_names - file_names)
    missing_in_readme = sorted(manifest_names - readme_names)

    if missing_in_manifest:
        raise SystemExit(
            "examples_manifest.json is missing entries for: "
            + ", ".join(missing_in_manifest)
        )
    if missing_on_disk:
        raise SystemExit(
            "Manifest references files that do not exist: "
            + ", ".join(missing_on_disk)
        )
    if missing_in_readme:
        raise SystemExit(
            "README.md does not reference these examples: "
            + ", ".join(missing_in_readme)
        )


def resolve_kain_binary(repo_root: Path, override: str | None, manifest: dict) -> str:
    if override:
        return override
    env_value = os.environ.get("KAIN_BIN", "").strip()
    if env_value:
        return env_value
    default_binary = manifest.get("default_kain_binary", "./target/debug/kain")
    return str((repo_root / default_binary).resolve())


def resolve_examples(manifest: dict, only: str | None, validation_class: str | None) -> list[dict]:
    selected = manifest["examples"]
    if validation_class:
        selected = [
            entry for entry in selected if entry["validation_class"] == validation_class
        ]
    if only:
        selected = [entry for entry in selected if entry["path"] == only]
    if not selected:
        raise SystemExit("No examples matched the requested filters.")
    return selected


def render_output_base(example_root: Path, example_name: str, operation: dict) -> Path:
    stem = Path(example_name).stem
    if operation["kind"] == "build":
        return example_root / operation["target"] / stem
    return example_root / operation["kind"] / stem


def command_for_operation(kain_bin: str, example_path: Path, output_base: Path, operation: dict) -> list[str]:
    if operation["kind"] == "run":
        return [kain_bin, "run", str(example_path)]
    if operation["kind"] == "build":
        return [
            kain_bin,
            "build",
            str(example_path),
            "-t",
            operation["target"],
            "-o",
            str(output_base),
        ]
    if operation["kind"] == "gpu-artifacts":
        return [
            kain_bin,
            "gpu-artifacts",
            str(example_path),
            "-o",
            str(output_base),
        ]
    raise SystemExit(f"Unsupported operation kind: {operation['kind']}")


def run_operation(command: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        text=True,
        capture_output=True,
        env=env,
        check=False,
    )


def print_operation_header(example_name: str, operation: dict) -> None:
    if operation["kind"] == "build":
        print(f"[build:{operation['target']}] {example_name}")
        return
    print(f"[{operation['kind']}] {example_name}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate the docs/examples Kain suite.")
    parser.add_argument(
        "--kain",
        help="Path to the Kain binary to use. Defaults to KAIN_BIN or the repo-local target/debug binary.",
    )
    parser.add_argument(
        "--only",
        help="Validate a single example file name, for example 11_ultimate_kain_pipeline.kn.",
    )
    parser.add_argument(
        "--class",
        dest="validation_class",
        help="Filter by validation class, for example must_pass_local.",
    )
    parser.add_argument(
        "--output-root",
        help="Keep generated outputs under this directory instead of a temporary directory.",
    )
    parser.add_argument(
        "--keep-output",
        action="store_true",
        help="Do not delete the generated output directory after a successful run.",
    )
    args = parser.parse_args()

    examples_dir = Path(__file__).resolve().parent
    repo_root = examples_dir.parent.parent
    manifest_path = examples_dir / "examples_manifest.json"
    manifest = load_manifest(manifest_path)

    if manifest.get("schema_version") != 1:
        raise SystemExit("Unsupported manifest schema version.")

    ensure_suite_consistency(examples_dir, manifest)
    kain_bin = resolve_kain_binary(repo_root, args.kain, manifest)

    if not Path(kain_bin).exists():
        raise SystemExit(f"Kain binary not found: {kain_bin}")

    if args.output_root:
        output_root = Path(args.output_root).resolve()
        output_root.mkdir(parents=True, exist_ok=True)
        delete_output_root = False
    else:
        output_root = Path(tempfile.mkdtemp(prefix="kain_docs_examples_"))
        delete_output_root = not args.keep_output

    workspace_root = output_root / "workspace"
    workspace_root.mkdir(parents=True, exist_ok=True)

    selected_examples = resolve_examples(manifest, args.only, args.validation_class)
    base_env = os.environ.copy()
    base_env["KAIN_DOCS_EXAMPLE_ROOT"] = str(workspace_root)

    try:
        for entry in selected_examples:
            example_name = entry["path"]
            example_path = examples_dir / example_name
            example_output_root = output_root / Path(example_name).stem
            example_output_root.mkdir(parents=True, exist_ok=True)

            for operation in entry["operations"]:
                print_operation_header(example_name, operation)
                output_base = render_output_base(example_output_root, example_name, operation)
                output_base.parent.mkdir(parents=True, exist_ok=True)
                command = command_for_operation(kain_bin, example_path, output_base, operation)
                result = run_operation(command, base_env)

                if result.returncode != 0:
                    sys.stderr.write(f"Validation failed for {example_name}\n")
                    sys.stderr.write("Command: " + " ".join(command) + "\n")
                    if result.stdout:
                        sys.stderr.write("--- stdout ---\n")
                        sys.stderr.write(result.stdout)
                    if result.stderr:
                        sys.stderr.write("--- stderr ---\n")
                        sys.stderr.write(result.stderr)
                    return result.returncode or 1

        print(f"Validated {len(selected_examples)} example(s).")
        print(f"Output root: {output_root}")
        return 0
    finally:
        if delete_output_root and output_root.exists():
            shutil.rmtree(output_root)


if __name__ == "__main__":
    raise SystemExit(main())
