#!/usr/bin/env python3

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import threading
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def resolve_repo_root() -> Path:
    configured = os.environ.get("KAIN_REPO_ROOT", "").strip()
    if configured:
        return Path(configured).resolve()
    return REPO_ROOT


def resolve_blade_root(repo_root: Path) -> Path:
    return repo_root / "blades" / "kain-mcp"


def resolve_kain_binary(repo_root: Path) -> str:
    override = os.environ.get("KAIN_MCP_KAIN_BIN", "").strip()
    if override:
        return override

    candidates = [
        repo_root / "target" / "debug" / "kain.exe",
        repo_root / "target" / "debug" / "kain",
        repo_root / "target" / "release" / "kain.exe",
        repo_root / "target" / "release" / "kain",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    path_candidate = shutil.which("kain")
    if path_candidate:
        return path_candidate

    raise SystemExit(
        "Could not resolve a Kain binary. Set KAIN_MCP_KAIN_BIN or build the repo-local cli first."
    )


def iter_python_runtime_dirs() -> list[Path]:
    discovered: list[Path] = []
    configured = os.environ.get("KAIN_MCP_PYTHON_DIRS", "").strip()
    if configured:
        for item in configured.split(os.pathsep):
            trimmed = item.strip()
            if trimmed:
                discovered.append(Path(trimmed))

    discovered.append(Path(sys.executable).resolve().parent)

    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if local_app_data:
        python_root = Path(local_app_data) / "Programs" / "Python"
        for child in python_root.glob("Python3*"):
            discovered.append(child)

    ordered: list[Path] = []
    seen: set[str] = set()
    for path in discovered:
        candidate = path.resolve()
        key = str(candidate).lower()
        if key in seen:
            continue
        if not candidate.exists():
            continue
        seen.add(key)
        ordered.append(candidate)
    return ordered


def build_launch_path() -> str:
    existing = os.environ.get("PATH", "")
    prefixes = [str(path) for path in iter_python_runtime_dirs()]
    if not prefixes:
        return existing
    if existing:
        prefixes.append(existing)
    return os.pathsep.join(prefixes)


def forward_stdin(child_stdin) -> None:
    stdin_fd = sys.stdin.fileno()
    try:
        while True:
            chunk = os.read(stdin_fd, 65536)
            if not chunk:
                break
            child_stdin.write(chunk)
            child_stdin.flush()
    finally:
        try:
            child_stdin.close()
        except OSError:
            pass


def forward_stderr(child_stderr) -> None:
    child_fd = child_stderr.fileno()
    stderr_fd = sys.stderr.fileno()
    try:
        while True:
            chunk = os.read(child_fd, 65536)
            if not chunk:
                break
            os.write(stderr_fd, chunk)
    finally:
        try:
            child_stderr.close()
        except OSError:
            pass


def forward_stdout(child_stdout) -> None:
    child_fd = child_stdout.fileno()
    stdout_fd = sys.stdout.fileno()
    first_line_buffer = bytearray()
    first_line_pending = True

    try:
        while True:
            chunk = os.read(child_fd, 65536)
            if not chunk:
                break

            if first_line_pending:
                first_line_buffer.extend(chunk)

                if first_line_buffer.startswith(b"Content-Length:"):
                    os.write(stdout_fd, bytes(first_line_buffer))
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                newline_index = first_line_buffer.find(b"\n")
                if newline_index != -1:
                    first_line = bytes(first_line_buffer[: newline_index + 1])
                    remainder = bytes(first_line_buffer[newline_index + 1 :])
                    if not first_line.startswith(b" KAIN Compiler v"):
                        os.write(stdout_fd, first_line)
                    if remainder:
                        os.write(stdout_fd, remainder)
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                # Prevent an unbounded buffer if the stream does not emit a newline.
                if len(first_line_buffer) > 2048:
                    os.write(stdout_fd, bytes(first_line_buffer))
                    first_line_buffer.clear()
                    first_line_pending = False
                    continue

                continue

            os.write(stdout_fd, chunk)

        if first_line_pending and first_line_buffer:
            os.write(stdout_fd, bytes(first_line_buffer))
    finally:
        try:
            child_stdout.close()
        except OSError:
            pass


def main() -> int:
    repo_root = resolve_repo_root()
    blade_root = resolve_blade_root(repo_root)
    kain_bin = resolve_kain_binary(repo_root)

    env = os.environ.copy()
    env["KAIN_REPO_ROOT"] = str(repo_root)
    env["KAIN_MCP_BLADE_ROOT"] = str(blade_root)
    env["PATH"] = build_launch_path()

    command = [kain_bin, "run", str(blade_root)]
    if len(sys.argv) > 1:
        command.extend(["--", *sys.argv[1:]])

    child = subprocess.Popen(
        command,
        cwd=str(repo_root),
        env=env,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    stdin_thread = threading.Thread(target=forward_stdin, args=(child.stdin,), daemon=True)
    stdout_thread = threading.Thread(target=forward_stdout, args=(child.stdout,), daemon=True)
    stderr_thread = threading.Thread(target=forward_stderr, args=(child.stderr,), daemon=True)

    stdin_thread.start()
    stdout_thread.start()
    stderr_thread.start()

    exit_code = child.wait()
    stdout_thread.join()
    stderr_thread.join()
    stdin_thread.join(timeout=0.1)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
