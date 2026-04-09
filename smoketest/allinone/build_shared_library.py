#!/usr/bin/env python3

import argparse
import pathlib
import platform
import subprocess
import sys


def output_file_name(stem: str) -> str:
    system = platform.system().lower()
    if system == "windows":
        return f"{stem}.dll"
    if system == "darwin":
        return f"lib{stem}.dylib"
    return f"lib{stem}.so"


def build_command(source: pathlib.Path, output: pathlib.Path) -> list[str]:
    command = ["clang", "-shared", "-O2"]
    if platform.system().lower() != "windows":
        command.append("-fPIC")
    command.extend([str(source), "-o", str(output)])
    return command


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a platform-correct shared library for a smoke fixture.")
    parser.add_argument("source", help="C source file to compile")
    parser.add_argument("stem", help="Logical library stem without platform prefix or extension")
    parser.add_argument("stamp", help="Path to a build stamp file to write after success")
    args = parser.parse_args()

    source_path = pathlib.Path(args.source)
    stamp_path = pathlib.Path(args.stamp)
    output_path = source_path.parent / output_file_name(args.stem)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    stamp_path.parent.mkdir(parents=True, exist_ok=True)

    command = build_command(source_path, output_path)
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        return result.returncode

    stamp_path.write_text(f"{output_path.name}\n", encoding="utf-8")
    print(output_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
