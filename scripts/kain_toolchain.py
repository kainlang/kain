#!/usr/bin/env python3
"""
Kain Universal Toolchain Discovery
===================================
Discovers C/C++ toolchain paths on any platform and outputs the correct
compiler/linker flags to stdout. Works on Windows (MSVC), macOS (Xcode CLT),
and Linux (GCC/Clang standard paths).

Usage:
    python kain_toolchain.py              # prints export commands for shell
    python kain_toolchain.py --cflags     # compiler flags only
    python kain_toolchain.py --ldflags    # linker flags only
    eval $(python kain_toolchain.py)      # source into current shell

Never again hunt for libcmt.lib or oldnames.lib.
"""

import os
import sys
import subprocess
import json
from pathlib import Path


def find_msvc():
    """Discover MSVC and Windows SDK library paths on Windows."""
    result = {"lib_paths": [], "include_paths": []}

    if sys.platform != "win32":
        return result

    # VS 2022 install locations
    vs_candidates = []
    program_files = os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
        vs_candidates.append(Path(program_files) / "Microsoft Visual Studio" / "2022" / edition)

    # Fallback: try older VS versions
    for year in ("2019", "2017"):
        for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
            vs_candidates.append(
                Path(program_files) / "Microsoft Visual Studio" / year / edition
            )

    vs_root = None
    for candidate in vs_candidates:
        if candidate.exists():
            vs_root = candidate
            break

    if vs_root is None:
        return result

    # MSVC toolchain version (latest)
    msvc_base = vs_root / "VC" / "Tools" / "MSVC"
    if msvc_base.exists():
        versions = sorted(
            [d for d in msvc_base.iterdir() if d.is_dir()], reverse=True
        )
        if versions:
            msvc_ver = versions[0]
            result["lib_paths"].append(str(msvc_ver / "lib" / "x64"))
            result["include_paths"].append(str(msvc_ver / "include"))

    # Windows SDK
    sdk_base = Path(program_files) / "Windows Kits" / "10"
    if sdk_base.exists():
        sdk_include = sdk_base / "Include"
        sdk_lib = sdk_base / "Lib"
        if sdk_include.exists():
            versions = sorted(
                [d for d in sdk_include.iterdir() if d.is_dir()], reverse=True
            )
            if versions:
                sdk_ver = versions[0]
                result["include_paths"].extend(
                    [
                        str(sdk_ver / "ucrt"),
                        str(sdk_ver / "shared"),
                        str(sdk_ver / "um"),
                        str(sdk_ver / "winrt"),
                    ]
                )
        if sdk_lib.exists():
            versions = sorted(
                [d for d in sdk_lib.iterdir() if d.is_dir()], reverse=True
            )
            if versions:
                sdk_ver = versions[0]
                result["lib_paths"].extend(
                    [
                        str(sdk_ver / "um" / "x64"),
                        str(sdk_ver / "ucrt" / "x64"),
                    ]
                )

    return result


def find_xcode():
    """Discover macOS Xcode CLT include/lib paths."""
    result = {"lib_paths": [], "include_paths": []}

    if sys.platform != "darwin":
        return result

    # Xcode command line tools SDK
    try:
        sdk_path = subprocess.check_output(
            ["xcrun", "--show-sdk-path"], text=True
        ).strip()
        if sdk_path:
            result["include_paths"].append(os.path.join(sdk_path, "usr", "include"))
            result["lib_paths"].append(os.path.join(sdk_path, "usr", "lib"))
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return result


def find_linux():
    """Discover standard Linux include/lib paths."""
    result = {"lib_paths": [], "include_paths": []}

    if sys.platform == "win32" or sys.platform == "darwin":
        return result

    # Standard system paths — clang usually handles these automatically
    for path in ("/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu"):
        if os.path.isdir(path):
            result["lib_paths"].append(path)

    for path in ("/usr/include", "/usr/include/x86_64-linux-gnu"):
        if os.path.isdir(path):
            result["include_paths"].append(path)

    return result


def discover():
    """Run all platform discoverers. First match wins."""
    for finder in (find_msvc, find_xcode, find_linux):
        result = finder()
        if result["include_paths"] or result["lib_paths"]:
            return result
    return {"lib_paths": [], "include_paths": []}


def format_shell_exports(result):
    """Output shell export commands."""
    lines = []

    # Library search paths
    if result["lib_paths"]:
        if sys.platform == "win32":
            # Windows: set LIB env var for lld-link
            lib_paths = ";".join(result["lib_paths"])
            lines.append(f'export LIB="{lib_paths}"')
        else:
            # Unix: use LIBRARY_PATH for the linker
            lib_paths = ":".join(result["lib_paths"])
            lines.append(f'export LIBRARY_PATH="{lib_paths}"')

    # Include paths
    if result["include_paths"]:
        if sys.platform == "win32":
            inc_paths = ";".join(result["include_paths"])
            lines.append(f'export INCLUDE="{inc_paths}"')
        else:
            inc_paths = ":".join(result["include_paths"])
            lines.append(f'export C_INCLUDE_PATH="{inc_paths}"')

    return "\n".join(lines)


def format_cflags(result):
    """Output -I flags for compiler."""
    flags = []
    for p in result["include_paths"]:
        flags.append(f"-I{p}")
    return " ".join(flags)


def format_ldflags(result):
    """Output -L flags for linker."""
    flags = []
    for p in result["lib_paths"]:
        flags.append(f"-L{p}")
        # On Windows, also pass via -Wl,-libpath for lld-link
        if sys.platform == "win32":
            flags.append(f"-Wl,-libpath:{p}")
    return " ".join(flags)


def main():
    result = discover()

    if "--json" in sys.argv:
        print(json.dumps(result, indent=2))
    elif "--cflags" in sys.argv:
        print(format_cflags(result))
    elif "--ldflags" in sys.argv:
        print(format_ldflags(result))
    else:
        print(format_shell_exports(result))


if __name__ == "__main__":
    main()
