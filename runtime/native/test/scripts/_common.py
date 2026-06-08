"""
_common.py
──────────
Shared engine for the runtime test automation pipeline.
Reuses the Z3 pipeline's battle-tested function extractor
and adds test-generation primitives on top.

Data flow:
  01_extract_functions.py  ──► data/functions/<file>.json  (per-file)
  02_classify_testability.py ──► data/testable.json         (ranked)
  03_generate_fuzz.py      ──► ../../fuzz/fuzz_<module>.c   (harnesses)
  04_generate_property.py  ──► ../../property/prop_<module>.c
  05_generate_smoke.py     ──► ../../smoke/smoke_<module>.c
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Iterable

# ── Paths ────────────────────────────────────────────────────────────────
SCRIPT_DIR = Path(__file__).resolve().parent
TEST_DIR = SCRIPT_DIR.parent
RUNTIME_DIR = TEST_DIR.parent
CORE_DIR = RUNTIME_DIR / "src" / "core"
INCLUDE_DIR = RUNTIME_DIR / "include"
DATA_DIR = SCRIPT_DIR / "data"
FUNCTIONS_DIR = DATA_DIR / "functions"
TEST_FUZZ_DIR = TEST_DIR / "fuzz"
TEST_PROP_DIR = TEST_DIR / "property"
TEST_SMOKE_DIR = TEST_DIR / "smoke"

# Import the Z3 pipeline's extraction engine (reuse, don't rewrite)
Z3_SCRIPTS = RUNTIME_DIR / "src" / "core" / "z3" / "scripts"
sys.path.insert(0, str(Z3_SCRIPTS))

try:
    from _runtime_scan_common import (
        extract_functions,
        iter_core_sources,
        stable_relpath,
        read_text,
        split_lines,
        compact_spaces,
        write_json_and_csv,
    )
    HAS_Z3_EXTRACTOR = True
except ImportError:
    HAS_Z3_EXTRACTOR = False


# ── Testability classification tokens ────────────────────────────────────
# Functions matching these patterns can be tested directly
EXPORTED_FUNC_PREFIXES = (
    "kain_",
    "abi_",
    "__kain_",
    "KAIN_",
)

# Functions matching these patterns are internal helpers
INTERNAL_PREFIXES = (
    "kain_alloc_cache_",
    "kain_ownership_",
    "kain_actor_table_",
    "kain_async_",
    "kain_memory_",
)

# Signal words that suggest a function is externally testable
TESTABLE_SIGNALS = (
    "void",
    "int ",
    "uint64_t",
    "size_t",
    "const char",
    "KainRuntimeHandle",
    "KainActorId",
    "KainTaskId",
    "KainTimerId",
    "KainDiag*",
)

# Functions that match these are NOT directly testable (need state setup)
COMPLEX_SIGNALS = (
    "static ",
    "inline ",
    "unlocked",
    "_internal",
    "_helper",
    "_proc",
)


def ensure_output_dirs() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    FUNCTIONS_DIR.mkdir(parents=True, exist_ok=True)


def iter_runtime_sources() -> list[Path]:
    """All .c files in the runtime core, skipping benchmark-only files."""
    sources = sorted(CORE_DIR.glob("*.c"))
    return [s for s in sources if "_benchmark" not in s.name]


def extract_all_functions(source_path: Path) -> list[dict]:
    """Extract all functions from a C source file.
    Uses the Z3 pipeline extractor if available, otherwise a fallback regex parser.
    """
    if HAS_Z3_EXTRACTOR:
        return extract_functions(source_path)

    # Fallback: simple brace-counting parser
    lines = split_lines(read_text(source_path))
    functions = []
    i = 0
    while i < len(lines):
        stripped = lines[i].strip()
        if stripped and "(" in stripped and "{" in lines[i] and not stripped.startswith("#"):
            # Naive: assume any line containing ( and { is a function start
            name_match = re.search(r'([A-Za-z_]\w*)\s*\(', stripped)
            if name_match:
                name = name_match.group(1)
                brace_depth = lines[i].count("{") - lines[i].count("}")
                end = i
                while end + 1 < len(lines) and brace_depth > 0:
                    end += 1
                    brace_depth += lines[end].count("{") - lines[end].count("}")
                functions.append({
                    "file": str(source_path),
                    "name": name,
                    "start_line": i + 1,
                    "end_line": end + 1,
                    "signature": compact_spaces(stripped),
                    "body_lines": end - i + 1,
                })
                i = end + 1
                continue
        i += 1
    return functions


def is_exported(func: dict) -> bool:
    """Is this function part of the public ABI surface?"""
    name = func["name"]
    for prefix in EXPORTED_FUNC_PREFIXES:
        if name.startswith(prefix):
            # Exclude internal helpers
            for internal in INTERNAL_PREFIXES:
                if name.startswith(internal):
                    return False
            return True
    return False


def is_static(func: dict) -> bool:
    """Is this function static (file-local)?"""
    sig = func.get("signature", "")
    body = func.get("body", "")
    return "static" in sig.split("(")[0].split() or "static" in body[:200]


def classify_testability(func: dict, header_funcs: set[str]) -> dict:
    """Score a function from 0-100 for how testable it is."""
    name = func["name"]
    sig = func.get("signature", "")
    body = func.get("body", "")
    score = 0
    reasons = []

    # ── Direct testability signals ──
    if name in header_funcs:
        score += 40
        reasons.append("declared_in_header")

    if is_exported(func):
        score += 25
        reasons.append("exported_abi")
    elif not is_static(func):
        score += 15
        reasons.append("non_static")

    if is_static(func):
        score -= 10
        reasons.append("static_internal")

    # ── Parameter count ──
    param_str = sig.split("(")[1].split(")")[0] if "(" in sig else ""
    param_count = len([p for p in param_str.split(",") if p.strip() and p.strip() != "void"])
    if param_count == 0:
        score += 10
        reasons.append("zero_params")
    elif param_count <= 3:
        score += 5
        reasons.append("few_params")
    else:
        score -= param_count * 2
        reasons.append("many_params")

    # ── Body size ──
    body_lines = func.get("body_lines", func.get("end_line", 0) - func.get("start_line", 0) + 1)
    if body_lines < 20:
        score += 10
        reasons.append("small_body")
    elif body_lines < 100:
        score += 5
    else:
        score -= (body_lines // 50)
        reasons.append("large_body")

    # ── State dependencies ──
    if "g_" in body or "static" in body[:200]:
        score -= 5
        reasons.append("global_state")

    if "pthread_" in body or "CreateThread" in body:
        score -= 10
        reasons.append("threading")

    if "malloc" in body or "calloc" in body or "realloc" in body:
        score += 5
        reasons.append("memory_ops")

    return {
        "function": name,
        "file": stable_relpath(func["file"]) if HAS_Z3_EXTRACTOR else func["file"],
        "start_line": func["start_line"],
        "end_line": func["end_line"],
        "body_lines": body_lines,
        "signature": sig,
        "testability_score": max(0, min(100, score + 25)),  # normalize
        "is_exported": is_exported(func),
        "is_static": is_static(func),
        "param_count": param_count,
        "reasons": " | ".join(reasons),
    }


def load_header_declarations() -> set[str]:
    """Extract all function names declared in public headers."""
    names: set[str] = set()
    for header in INCLUDE_DIR.glob("*.h"):
        text = read_text(header)
        for match in re.finditer(r'\b([a-zA-Z_]\w+)\s*\(', text):
            name = match.group(1)
            if name not in ("if", "for", "while", "switch", "return", "sizeof", "defined"):
                names.add(name)
    return names


def dump_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, default=str), encoding="utf-8")


def print_summary_table(title: str, rows: list[dict], columns: list[str], limit: int = 25) -> None:
    print(f"\n{title}")
    print("-" * 100)
    header = " | ".join(c.ljust(w) for c, w in [(c, 12) for c in columns[:4]] + [(c, 20) for c in columns[4:]])
    print(header)
    print("-" * len(header))
    for row in rows[:limit]:
        parts = []
        for c in columns:
            val = str(row.get(c, ""))[:60]
            parts.append(val.ljust(12 if c in ("function", "file") else 20))
        print(" | ".join(parts))
