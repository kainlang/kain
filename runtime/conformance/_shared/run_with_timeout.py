#!/usr/bin/env python3
"""Run a command with a hard timeout and propagate its exit status."""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
from typing import Sequence


TIMEOUT_EXIT_CODE = 124


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a command with a timeout.")
    parser.add_argument(
        "--seconds",
        type=float,
        required=True,
        help="Maximum runtime in seconds before the command is terminated.",
    )
    parser.add_argument(
        "--label",
        default="command",
        help="Human-readable label used in timeout messages.",
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="Command to run. Prefix with -- to terminate option parsing.",
    )
    args = parser.parse_args(argv)

    if args.seconds <= 0:
        parser.error("--seconds must be greater than 0")
    if not args.command:
        parser.error("missing command to execute")

    if args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("missing command to execute")

    return args


def _terminate_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return

    if os.name == "nt":
        process.terminate()
    else:
        os.killpg(process.pid, signal.SIGTERM)

    try:
        process.wait(timeout=5)
        return
    except subprocess.TimeoutExpired:
        pass

    if process.poll() is None:
        if os.name == "nt":
            process.kill()
        else:
            os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def main(argv: Sequence[str]) -> int:
    args = _parse_args(argv)

    popen_kwargs = {}
    if os.name != "nt":
        popen_kwargs["preexec_fn"] = os.setsid
    else:
        popen_kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP

    process = subprocess.Popen(args.command, **popen_kwargs)
    try:
        return process.wait(timeout=args.seconds)
    except subprocess.TimeoutExpired:
        print(
            f"[timeout] {args.label} exceeded {args.seconds:g}s and was terminated.",
            file=sys.stderr,
        )
        _terminate_process(process)
        return TIMEOUT_EXIT_CODE


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
