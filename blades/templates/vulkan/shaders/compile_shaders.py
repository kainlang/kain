#!/usr/bin/env python3
"""
Compile GLSL shaders to SPIR-V for the Vulkan template.

Requires: glslc (from Vulkan SDK or shaderc)
  - Windows: `scoop install vulkan` or download from vulkan.lunarg.com
  - Linux:   `apt install glslang-tools` or `apt install vulkan-sdk`
  - macOS:   `brew install glslang`

Usage:
  python shaders/compile_shaders.py

The .spv files are read by src/shaders.kn at runtime via
graphics_shader_spirv_from_file().
"""

import subprocess
import sys
from pathlib import Path

SHADER_DIR = Path(__file__).parent
SHADERS = [
    ("vert.glsl", "vert.spv", "vertex"),
    ("frag.glsl", "frag.spv", "fragment"),
]


def find_glslc():
    """Find the glslc compiler."""
    for name in ["glslc", "glslc.exe"]:
        result = subprocess.run(
            ["where", name] if sys.platform == "win32" else ["which", name],
            capture_output=True, text=True,
        )
        if result.returncode == 0:
            return result.stdout.strip().split("\n")[0]
    return None


def main():
    glslc_path = find_glslc()
    if glslc_path is None:
        print("ERROR: glslc not found in PATH.")
        print("Install Vulkan SDK: https://vulkan.lunarg.com/")
        print("Or: scoop install vulkan  (Windows)")
        print("Or: apt install glslang-tools  (Linux)")
        sys.exit(1)

    print(f"Using glslc: {glslc_path}")

    ok = True
    for src_name, out_name, stage in SHADERS:
        src_path = SHADER_DIR / src_name
        out_path = SHADER_DIR / out_name

        if not src_path.exists():
            print(f"ERROR: Source file not found: {src_path}")
            ok = False
            continue

        cmd = [glslc_path, f"-fshader-stage={stage}", str(src_path), "-o", str(out_path)]
        print(f"Compiling {src_name} → {out_name} ...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            print(f"  ERROR: {result.stderr.strip()}")
            ok = False
        else:
            size = out_path.stat().st_size
            print(f"  OK ({size} bytes)")

    if ok:
        print("\nAll shaders compiled successfully.")
        print("The Vulkan template can now load them via graphics_shader_spirv_from_file().")
    else:
        print("\nSome shaders failed to compile. Fix errors above.")
        sys.exit(1)


if __name__ == "__main__":
    main()
