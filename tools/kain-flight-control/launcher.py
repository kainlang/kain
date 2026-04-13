#!/usr/bin/env python3
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys


def resolve_repo_root() -> pathlib.Path:
    env_root = os.environ.get("KAIN_REPO_ROOT", "").strip()
    if env_root:
        return pathlib.Path(env_root).expanduser().resolve()
    return pathlib.Path(__file__).resolve().parents[2]


def resolve_binary_path(repo_root: pathlib.Path) -> pathlib.Path:
    binary_name = "kain-flight-control.exe" if os.name == "nt" else "kain-flight-control"
    return repo_root / "tools" / "kain-flight-control" / "bin" / binary_name


def main() -> int:
    repo_root = resolve_repo_root()
    tool_root = repo_root / "tools" / "kain-flight-control"
    config_path = tool_root / "config" / "server.toml"
    binary_path = resolve_binary_path(repo_root)

    env = os.environ.copy()
    env["KAIN_REPO_ROOT"] = str(repo_root)

    forwarded_args = sys.argv[1:]

    if binary_path.exists():
        command = [str(binary_path), "--config", str(config_path), *forwarded_args]
        cwd = repo_root
    else:
        go_executable = shutil.which("go")
        if not go_executable:
            print(
                "kain-flight-control launcher could not find a built binary or the `go` executable.",
                file=sys.stderr,
            )
            return 1
        command = [
            go_executable,
            "run",
            "./cmd/kain-flight-control",
            "--config",
            str(config_path),
            *forwarded_args,
        ]
        cwd = tool_root

    completed = subprocess.run(
        command,
        cwd=str(cwd),
        env=env,
        stdin=sys.stdin,
        stdout=sys.stdout,
        stderr=sys.stderr,
        check=False,
    )
    return int(completed.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
