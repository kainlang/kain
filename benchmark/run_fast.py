#!/usr/bin/env python3
"""
Fast Kain benchmark wrapper.

Runs the main benchmark runner with the reduced language set:
Kain LLVM, Rust LLVM, C++ Clang, and Erlang OTP.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


BENCHMARK_ROOT = Path(__file__).resolve().parent
RUNNER = BENCHMARK_ROOT / "run.py"
FAST_LANGUAGES = "kain,rust,cpp,erlang"
FAST_MINIMAL_NAME = "latest_fast.md"


def main() -> int:
    forwarded_args = sys.argv[1:]
    command = [
        sys.executable,
        str(RUNNER),
        *forwarded_args,
        "--languages",
        FAST_LANGUAGES,
        "--minimal-name",
        FAST_MINIMAL_NAME,
    ]
    completed = subprocess.run(command, cwd=str(BENCHMARK_ROOT.parent))
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
