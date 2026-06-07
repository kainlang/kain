#!/usr/bin/env python3
"""
Scaffold test harnesses from Kain runtime headers.
Reads a header file, extracts function signatures, and generates a
smoke test + fuzz harness + property test skeleton.

Usage:
    python scripts/scaffold_test.py include/memory.h
    python scripts/scaffold_test.py --all          # scaffold every header
    python scripts/scaffold_test.py --fuzz include/actor.h    # fuzz only
    python scripts/scaffold_test.py --smoke include/arena.h   # smoke only
    python scripts/scaffold_test.py --list                    # list all headers
"""

import re
import sys
import os
from pathlib import Path
from typing import List, Tuple


def parse_functions(header_path: str) -> List[Tuple[str, str, str]]:
    """Parse function declarations from a C header.
    Returns list of (return_type, name, params) tuples.
    """
    funcs = []
    with open(header_path) as f:
        content = f.read()

    # Remove comments
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    content = re.sub(r"//.*$", "", content, flags=re.MULTILINE)

    # Match function declarations: return_type name(params);
    # Handles: void func(void);  int func(int x);  void* func(const char* s);
    pattern = re.compile(
        r"^\s*"                              # start of line
        r"((?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:long\s+)?"  # return type start
        r"(?:\w+\s*\**\s*)+)"                   # rest of return type
        r"(\w+)\s*"                            # function name
        r"\(([^)]*)\)\s*;"                     # params + semicolon
    , re.MULTILINE)
    for match in pattern.finditer(content):
        ret = match.group(1).strip()
        name = match.group(2)
        params = match.group(3).strip()
        if not (name.startswith("kain_") or name.startswith("abi_") or name.startswith("__kain_") or name.startswith("KAIN_")):
            continue
        funcs.append((ret, name, params))

    return funcs


def module_name(header_path: str) -> str:
    return Path(header_path).stem


def generate_fuzz(header_path: str, funcs: List[Tuple[str, str, str]]) -> str:
    mod = module_name(header_path)
    header_rel = os.path.relpath(header_path, os.path.dirname(header_path))
    include_name = f'"{header_rel}"'

    lines = [
        f"// Auto-generated fuzz harness for {mod}",
        f"// Generated from: {header_path}",
        f"// {len(funcs)} public functions detected",
        f"",
        f"#include <stdint.h>",
        f"#include <stddef.h>",
        f"#include <stdlib.h>",
        f"#include <string.h>",
        f"#include {include_name}",
        f"",
        f"int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {{",
        f"    if (size < 16) return 0;",
        f"",
        f"    // Extract random parameters from fuzz data",
        f"    uint64_t arg0 = *(uint64_t*)(data + 0);",
        f"    uint64_t arg1 = *(uint64_t*)(data + 8);",
        f"    const uint8_t *payload = data + 16;",
        f"    size_t payload_size = size > 16 ? size - 16 : 0;",
        f"",
    ]

    for _, name, params in funcs:
        param_count = len([p for p in params.split(",") if p.strip()]) if params != "void" else 0
        if param_count == 0:
            lines.append(f"    // {name}() — no params, call directly")
            lines.append(f"    {name}();")
        elif param_count <= 3:
            lines.append(f"    // {name}({params})")
            lines.append(f"    // TODO: derive args from data buffer")
            lines.append(f"    // {name}((int)arg0, (void*)(payload), (size_t)arg1);")
        else:
            lines.append(f"    // {name}({params}) — {param_count} params, needs manual wiring")

    lines.extend([
        "",
        "    return 0;",
        "}",
        "",
    ])
    return "\n".join(lines)


def generate_smoke(header_path: str, funcs: List[Tuple[str, str, str]]) -> str:
    mod = module_name(header_path)
    header_rel = f'"{mod}.h"'

    lines = [
        f"// Auto-generated smoke test for {mod}",
        f"#include <stdio.h>",
        f"#include <stdlib.h>",
        f"#include <assert.h>",
        f"#include {header_rel}",
        f"",
        f"int main(void) {{",
    ]

    for _, name, _ in funcs[:5]:  # first 5 functions
        lines.append(f"    // TODO: call {name}() with valid args")
    if not funcs:
        lines.append("    // TODO: add basic smoke checks")

    lines.extend([
        f"",
        f"    printf(\"smoke_{mod}: PASS\\n\");",
        f"    return 0;",
        f"}}",
        f"",
    ])
    return "\n".join(lines)


def list_headers(runtime_dir: str):
    include = Path(runtime_dir) / "include"
    if include.exists():
        for h in sorted(include.glob("*.h")):
            funcs = parse_functions(str(h))
            if funcs:
                print(f"  {h.name} — {len(funcs)} functions")


def main():
    args = sys.argv[1:]
    runtime_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if "--list" in args:
        list_headers(runtime_dir)
        return

    mode = "all"
    if "--fuzz" in args:
        mode = "fuzz"
    if "--smoke" in args:
        mode = "smoke"
    if "--property" in args:
        mode = "property"

    targets = [a for a in args if not a.startswith("--")]

    if "--all" in args or not targets:
        include_dir = os.path.join(runtime_dir, "include")
        targets = [os.path.join(include_dir, h) for h in os.listdir(include_dir) if h.endswith(".h")]

    for target in targets:
        if not os.path.exists(target):
            print(f"SKIP: {target} (not found)")
            continue

        funcs = parse_functions(target)
        if not funcs:
            print(f"SKIP: {target} (no kain_* functions)")
            continue

        mod = module_name(target)
        test_dir = os.path.join(runtime_dir, "test")

        if mode in ("all", "fuzz"):
            os.makedirs(os.path.join(test_dir, "fuzz"), exist_ok=True)
            out = os.path.join(test_dir, "fuzz", f"fuzz_{mod}.c")
            if not os.path.exists(out):
                with open(out, "w") as f:
                    f.write(generate_fuzz(target, funcs))
                print(f"  fuzz_{mod}.c ({len(funcs)} funcs)")

        if mode in ("all", "smoke"):
            os.makedirs(os.path.join(test_dir, "smoke"), exist_ok=True)
            out = os.path.join(test_dir, "smoke", f"smoke_{mod}.c")
            if not os.path.exists(out):
                with open(out, "w") as f:
                    f.write(generate_smoke(target, funcs))
                print(f"  smoke_{mod}.c ({len(funcs)} funcs)")


if __name__ == "__main__":
    main()
