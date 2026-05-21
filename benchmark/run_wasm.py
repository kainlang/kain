#!/usr/bin/env python3
"""Compatibility shim for the dedicated Kain-vs-Rust WASM benchmark lane."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


BENCHMARK_ROOT = Path(__file__).resolve().parent
RUNNER = BENCHMARK_ROOT / "wasm" / "run.py"


def main() -> int:
    command = [sys.executable, str(RUNNER), *sys.argv[1:]]
    completed = subprocess.run(command, cwd=str(BENCHMARK_ROOT.parent))
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
