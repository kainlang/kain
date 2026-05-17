#!/usr/bin/env python3
"""
Data-driven benchmark wrapper launcher.

Wrapper configs live under benchmark/wrappers/*.json and forward their declared
arguments into benchmark/run.py without requiring edits to the core runner.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


BENCHMARK_ROOT = Path(__file__).resolve().parent
WRAPPER_ROOT = BENCHMARK_ROOT / "wrappers"


def load_wrapper(wrapper_name: str) -> dict[str, Any]:
    path = WRAPPER_ROOT / f"{wrapper_name}.json"
    if not path.exists():
        available = ", ".join(sorted(item.stem for item in WRAPPER_ROOT.glob("*.json"))) or "none"
        raise FileNotFoundError(f"unknown wrapper '{wrapper_name}'. Available wrappers: {available}")
    with path.open("r", encoding="utf-8") as handle:
        wrapper = json.load(handle)
    if not isinstance(wrapper, dict):
        raise ValueError(f"wrapper config must be a JSON object: {path}")
    return wrapper


def string_list(value: Any, field_name: str, wrapper_name: str) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        raise ValueError(f"wrapper '{wrapper_name}' field '{field_name}' must be a string array")
    return value


def build_command(wrapper_name: str, wrapper: dict[str, Any], forwarded_args: list[str]) -> list[str]:
    runner = str(wrapper.get("runner", "run.py"))
    runner_path = (BENCHMARK_ROOT / runner).resolve()
    if not runner_path.exists():
        raise FileNotFoundError(f"wrapper '{wrapper_name}' runner does not exist: {runner_path}")
    before_args = string_list(wrapper.get("before_args"), "before_args", wrapper_name)
    after_args = string_list(wrapper.get("after_args"), "after_args", wrapper_name)
    return [sys.executable, str(runner_path), *before_args, *forwarded_args, *after_args]


def available_wrappers() -> list[tuple[str, str]]:
    wrappers: list[tuple[str, str]] = []
    for path in sorted(WRAPPER_ROOT.glob("*.json")):
        description = ""
        try:
            with path.open("r", encoding="utf-8") as handle:
                wrapper = json.load(handle)
            if isinstance(wrapper, dict):
                description = str(wrapper.get("description", ""))
        except Exception:
            description = "(invalid wrapper config)"
        wrappers.append((path.stem, description))
    return wrappers


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wrapper", nargs="?", help="Wrapper name from benchmark/wrappers/<name>.json")
    parser.add_argument("--list", action="store_true", help="List available wrappers")
    parser.add_argument("--print-command", action="store_true", help="Print the resolved command before running it")
    return parser.parse_known_args()


def main() -> int:
    args, forwarded_args = parse_args()
    if args.list:
        for name, description in available_wrappers():
            suffix = f" - {description}" if description else ""
            print(f"{name}{suffix}")
        return 0
    if not args.wrapper:
        available = ", ".join(name for name, _ in available_wrappers()) or "none"
        print(f"wrapper name required. Available wrappers: {available}", file=sys.stderr)
        return 1
    wrapper = load_wrapper(args.wrapper)
    command = build_command(args.wrapper, wrapper, forwarded_args)
    if args.print_command:
        print(" ".join(command))
    completed = subprocess.run(command, cwd=str(BENCHMARK_ROOT.parent))
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
