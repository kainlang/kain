#!/usr/bin/env python3
"""Run the template-level 3D validation checks for this workspace.

This is a small, durable entrypoint for CI and regeneration flows. It keeps the
shared scene spine check easy to invoke as one command and leaves room for more
3D template validations later without growing ad hoc shell glue.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KAIN_TOML = ROOT / "KAIN.toml"
SCENE_SPINE = ROOT / "tools" / "validation" / "validate_scene_spine.py"

EXPECTED_PRIMARY_APP = "universal_3d_workbench"
EXPECTED_DEFAULT_HOST = "native_ui"


def load_toml_like_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        raise SystemExit(f"missing required template file: {path}")


def require_token(text: str, token: str, label: str, problems: list[str]) -> None:
    if token not in text:
        problems.append(f"missing {label}: {token}")


def run_check(command: list[str]) -> None:
    completed = subprocess.run(command, cwd=ROOT, text=True)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)


def main() -> int:
    problems: list[str] = []
    kain_toml = load_toml_like_text(KAIN_TOML)
    require_token(kain_toml, f'primary_app = "{EXPECTED_PRIMARY_APP}"', "primary app binding", problems)
    require_token(kain_toml, f'default_host = "{EXPECTED_DEFAULT_HOST}"', "default host binding", problems)

    if problems:
        print("Template 3D validation failed:")
        for problem in problems:
            print(f"- {problem}")
        return 1

    run_check([sys.executable, str(SCENE_SPINE)])
    print("Template 3D validation passed for the scene spine and main workbench launch bindings.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
