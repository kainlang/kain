#!/usr/bin/env python3
"""
Fast Kain benchmark wrapper.

Compatibility shim for the data-driven `fast` wrapper config.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


BENCHMARK_ROOT = Path(__file__).resolve().parent
RUN_WRAPPER = BENCHMARK_ROOT / "run_wrapper.py"
WRAPPER_NAME = "fast"


def main() -> int:
    forwarded_args = sys.argv[1:]
    command = [sys.executable, str(RUN_WRAPPER), WRAPPER_NAME, *forwarded_args]
    completed = subprocess.run(command, cwd=str(BENCHMARK_ROOT.parent))
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
