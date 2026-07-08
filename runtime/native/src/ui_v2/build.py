#!/usr/bin/env python3
"""
Kaintana Build Script — compiles any .c file against the Kaintana C substrate.

Usage:
    python build.py                          # compiles examples/hello_kaintana.c (default)
    python build.py my_demo.c                # compiles a different .c file
    python build.py --backend null           # use the null backend
    python build.py --run                    # compile + run
    python build.py --output demo.exe        # custom output path
    python build.py --gcc /usr/bin/gcc       # custom gcc path

The target .c file is expected to #include the backend .c file directly
(e.g. `#include "backends/terminal/host_terminal.c"`). The build script
compiles the 8 core substrate + 5 runtime core files and links them
together with the target file, WITHOUT separately compiling the backend
(since it's already brought in by the #include).
"""

import argparse
import os
import platform as _platform
import shlex
import subprocess
import sys


# ── Where this script lives ─────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Native runtime paths (relative to ui_v2/) ──────────────────────────
RUNTIME_SRC_CORE = os.path.join(SCRIPT_DIR, "..", "..", "src", "core")
NATIVE_INCLUDE    = os.path.join(SCRIPT_DIR, "..", "..", "include")

# ── Core Kaintana substrate files (always compiled) ────────────────────
CORE_FILES = [
    "tree.c",
    "box_math.c",
    "damage.c",
    "draw_pixels.c",
    "arena.c",
    "hash_table.c",
    "color.c",
    "attr_table.c",
]

# ── Native runtime core files (provides frame, arena, handle, input, etc.) ──
RUNTIME_CORE_FILES = [
    "arena.c",
    "version.c",
    "component_surface.c",
    "handle.c",
    "input_system.c",
]


# ── Runtime stubs (provides string_new when full runtime not linked) ──
STUBS_FILE = "kaintana_runtime_stubs.c"


# ── Windows SDK link libraries
# base.h pulls in winsock2.h, windows.h, windowsx.h, ws2tcpip.h, gl/GL.h
WIN32_LIBS = ["-lws2_32", "-lopengl32", "-lgdi32"]
# Linux/macOS needs pthread for atomic ops in arena.c
POSIX_LIBS = ["-lpthread"]


def resolve_runtime_abs():
    """Return absolute paths for native runtime core .c files."""
    base = os.path.abspath(RUNTIME_SRC_CORE)
    return [os.path.join(base, f) for f in RUNTIME_CORE_FILES]


def resolve_include_args(extra_includes=None):
    """Resolve include paths to -I arguments."""
    dirs = [
        os.path.abspath(NATIVE_INCLUDE),
        os.path.abspath(SCRIPT_DIR),
    ]
    if extra_includes:
        dirs.extend(os.path.abspath(p) for p in extra_includes)
    return [f'-I{d}' for d in dirs if os.path.isdir(d)]


def find_gcc(preferred):
    """Find a usable gcc. On Windows, tries common MinGW locations."""
    if preferred and preferred != "gcc":
        return preferred

    # Try gcc in PATH first
    try:
        subprocess.run(["gcc", "--version"], capture_output=True, check=True)
        return "gcc"
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Common install paths
    candidates = [
        "C:/msys64/ucrt64/bin/gcc.exe",
        "C:/msys64/mingw64/bin/gcc.exe",
        "C:/mingw64/bin/gcc.exe",
        "C:/mingw32/bin/gcc.exe",
        os.path.expanduser("~/scoop/apps/gcc/current/bin/gcc.exe"),
        "F:/scoop/apps/gcc/current/bin/gcc.exe",
        "/usr/bin/gcc",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c

    return "gcc"


def build(source_file, output_path, gcc_path, backend, extra_flags=None, debug=False):
    """
    Run the Kaintana compilation.

    Returns (returncode, stdout, stderr).
    """
    include_args = resolve_include_args()

    # Build the gcc command
    cmd = [gcc_path, "-std=c11", "-Wall", "-Wextra", "-pedantic"]

    # Add debug flags when --debug is used
    if debug:
        cmd.append("-g")
        cmd.append("-DDEBUG")
        print("[build] DEBUG mode: -g -DDEBUG enabled")

    # On Windows, enable ANSI escape codes in terminal
    if sys.platform == "win32":
        cmd.append("-D_WIN32")

    cmd.extend(include_args)

    # Add Kaintana core substrate files
    for f in CORE_FILES:
        p = os.path.join(SCRIPT_DIR, f)
        if os.path.isfile(p):
            cmd.append(p)

# Add native runtime core files
    for p in resolve_runtime_abs():
        if os.path.isfile(p):
            cmd.append(p)

    # Add minimal runtime stubs (string_new etc.)
    stubs_path = os.path.join(SCRIPT_DIR, "kaintana_runtime_stubs.c")
    if os.path.isfile(stubs_path):
        cmd.append(stubs_path)

    # Add the target source file
    source_abs = os.path.abspath(source_file)
    cmd.append(source_abs)

    # Output path
    cmd.append("-o")
    cmd.append(os.path.abspath(output_path))

    # Extra flags
    if extra_flags:
        cmd.extend(shlex.split(extra_flags))

    # Backend define (optional — demo files do their own #include)
    if backend == "null":
        cmd.extend(["-DKAINTANA_BACKEND_NULL=1"])

    # Link system libraries
    if sys.platform == "win32":
        cmd.extend(WIN32_LIBS)
    else:
        cmd.extend(POSIX_LIBS)

    # Print build info
    print(f"[build] Compiler:  {gcc_path}")
    print(f"[build] Source:    {source_abs}")
    print(f"[build] Output:    {os.path.abspath(output_path)}")
    print(f"[build] Backend:   {backend}")
    print(f"[build] Platform:  {sys.platform}")
    print()

    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


def main():
    parser = argparse.ArgumentParser(
        description="Kaintana Build Script — compile .c files against the C substrate"
    )
    parser.add_argument(
        "source", nargs="?",
        default=os.path.join(SCRIPT_DIR, "examples", "hello_kaintana.c"),
        help="Path to the .c file to compile (default: examples/hello_kaintana.c)"
    )
    parser.add_argument(
        "--backend", choices=["terminal", "null", "win32", "all"],
        default="terminal",
        help="UI backend to use (default: terminal)"
    )
    parser.add_argument(
        "--run", action="store_true",
        help="Execute the compiled binary after building"
    )
    parser.add_argument(
        "--gcc", default="gcc",
        help="Path to gcc compiler (default: gcc)"
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Output path for the .exe (default: same dir as source, same stem)"
    )
    parser.add_argument(
        "--extra", default=None,
        help="Extra gcc flags (e.g. '--extra \"-O2 -g\"')"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Print the full compiler command"
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="Build with -g -DDEBUG for diagnostics"
    )

    args = parser.parse_args()

    # Resolve gcc
    gcc = find_gcc(args.gcc)
    print(f"[build] Detected gcc: {gcc}")
    print()

    # Resolve source path
    source_path = args.source
    if not os.path.isabs(source_path):
        source_path = os.path.join(SCRIPT_DIR, source_path)

    if not os.path.isfile(source_path):
        print(f"[error] Source file not found: {source_path}")
        sys.exit(1)

    # Resolve output path
    if args.output:
        output_path = args.output
    else:
        stem = os.path.splitext(os.path.basename(source_path))[0]
        output_path = os.path.join(os.path.dirname(source_path), f"{stem}.exe")

    # Build
    print("=" * 60)
    print("  Kaintana Build")
    print("=" * 60)
    print()

    rc, stdout, stderr = build(
        source_path, output_path, gcc, args.backend, args.extra, args.debug
    )

    if args.verbose:
        print("--- STDOUT ---")
    if stdout:
        sys.stdout.write(stdout)
    if args.verbose:
        print("--- STDERR ---")
    if stderr:
        sys.stderr.write(stderr)

    if rc != 0:
        print(f"\n[FAIL] Build failed with exit code {rc}")
        if not args.verbose:
            print("(Re-run with --verbose for full command)")
        sys.exit(rc)

    size_mb = os.path.getsize(output_path) / (1024 * 1024)
    print(f"\n[OK] Build succeeded: {os.path.abspath(output_path)} ({size_mb:.1f} MB)")
    print()

    # Run the compiled binary if requested
    if args.run:
        print("=" * 60)
        print("  Running...")
        print("=" * 60)
        print()
        try:
            subprocess.run([os.path.abspath(output_path)], check=True)
        except subprocess.CalledProcessError as e:
            print(f"\n[FAIL] Run failed with exit code {e.returncode}")
            sys.exit(e.returncode)
        except FileNotFoundError:
            print(f"\n[FAIL] Binary not found: {output_path}")
            sys.exit(1)

    print("\n[DONE]")


if __name__ == "__main__":
    main()
