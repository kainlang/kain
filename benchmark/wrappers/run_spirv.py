#!/usr/bin/env python3
"""Alias for the dedicated GPU/SPIR-V benchmark lane."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


BENCHMARK_ROOT = Path(__file__).resolve().parent
RUNNER = BENCHMARK_ROOT / "lanes" / "gpu" / "run_gpu.py"


def main() -> int:
    command = [sys.executable, str(RUNNER), *sys.argv[1:]]
    completed = subprocess.run(command, cwd=str(BENCHMARK_ROOT.parent))
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
