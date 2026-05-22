"""Catalog native-core functions that look solver-worthy."""

from __future__ import annotations

import argparse
import re

from _runtime_scan_common import (
    ALLOC_SINK_TOKENS,
    ARITHMETIC_OPERATORS,
    ATOMIC_TOKENS,
    DESTROY_TOKENS,
    GUARD_TOKENS,
    LOCK_TOKENS,
    ONCE_TOKENS,
    SIZE_TERMS,
    count_token_hits,
    extract_functions,
    function_line_excerpt,
    iter_core_sources,
    print_top_table,
    stable_relpath,
    write_json_and_csv,
)

INIT_STATE_RE = re.compile(r"\binitialized\b|\binit_state\b")


def analyze_function(func: dict) -> dict:
    body = func["body"]
    lines = func["lines"]
    score = 0
    reasons: list[str] = []
    size_terms = sorted({term for term in SIZE_TERMS if term in body.lower()})
    guard_hits = [token for token in GUARD_TOKENS if token in body]
    lock_hits = [token for token in LOCK_TOKENS if token in body]
    once_hits = [token for token in ONCE_TOKENS if token in body]
    atomic_hits = [token for token in ATOMIC_TOKENS if token in body]
    has_lock = bool(lock_hits)
    has_once = bool(once_hits) or func["name"].endswith("_once") or func["name"].endswith("_init_once")
    has_atomic = bool(atomic_hits)
    destroy_hits = [token for token in DESTROY_TOKENS if token in body]
    alloc_hits = [token for token in ALLOC_SINK_TOKENS if token in body]
    arithmetic_lines = 0
    pointer_lines = 0
    global_write_lines = 0
    init_flag_lines = 0
    count_like_writes = 0

    for raw_line in lines:
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("*"):
            continue
        if any(op in stripped for op in ARITHMETIC_OPERATORS):
            arithmetic_lines += 1
        if "ptr" in stripped or "->" in stripped or "[" in stripped:
            pointer_lines += 1
        if INIT_STATE_RE.search(stripped):
            init_flag_lines += 1
        if "g_" in stripped and "=" in stripped:
            global_write_lines += 1
        if "count" in stripped and ("++" in stripped or "+=" in stripped):
            count_like_writes += 1

    if size_terms:
        score += 15
        reasons.append("touches size/count/capacity vocabulary")
    if arithmetic_lines >= 4:
        score += 15
        reasons.append("multiple arithmetic sites in one function")
    if pointer_lines >= 4:
        score += 10
        reasons.append("dense pointer or indexing traffic")
    if alloc_hits and not guard_hits:
        score += 20
        reasons.append("allocation or copy sink with no explicit overflow guard")
    if init_flag_lines and not (has_lock or has_once or has_atomic):
        score += 35
        reasons.append("lazy init flag with no obvious synchronization")
    if global_write_lines and not (has_lock or has_once or has_atomic):
        score += 20
        reasons.append("writes global state without obvious synchronization")
    if count_like_writes and not (has_lock or has_atomic):
        score += 20
        reasons.append("count-like increment with no lock or atomic")
    if destroy_hits and "unlock" in body and not has_once:
        score += 10
        reasons.append("destructive teardown after lock release")

    return {
        "risk_score": score,
        "file": stable_relpath(func["file"]),
        "function": func["name"],
        "start_line": func["start_line"],
        "end_line": func["end_line"],
        "body_lines": func["end_line"] - func["start_line"] + 1,
        "size_terms": "|".join(size_terms),
        "guard_hits": "|".join(sorted(set(guard_hits))),
        "lock_hits": "|".join(sorted(set(lock_hits))),
        "once_hits": "|".join(sorted(set(once_hits))),
        "atomic_hits": "|".join(sorted(set(atomic_hits))),
        "alloc_hits": "|".join(sorted(set(alloc_hits))),
        "destroy_hits": "|".join(sorted(set(destroy_hits))),
        "arithmetic_lines": arithmetic_lines,
        "pointer_lines": pointer_lines,
        "global_write_lines": global_write_lines,
        "init_flag_lines": init_flag_lines,
        "count_like_writes": count_like_writes,
        "token_density": count_token_hits(body.lower(), SIZE_TERMS),
        "reasons": " | ".join(reasons),
        "signature": func["signature"],
        "excerpt": function_line_excerpt(lines),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--min-risk", type=int, default=10)
    args = parser.parse_args()

    rows: list[dict] = []
    function_count = 0
    source_files = iter_core_sources()
    for source_path in source_files:
        functions = extract_functions(source_path)
        function_count += len(functions)
        for func in functions:
            rows.append(analyze_function(func))

    rows.sort(key=lambda row: row["risk_score"], reverse=True)
    interesting = [row for row in rows if row["risk_score"] >= args.min_risk]
    json_path, csv_path = write_json_and_csv("runtime_function_catalog", rows)

    print("Native Core Function Catalog")
    print(f"Scanned files: {len(source_files)}")
    print(f"Functions:     {function_count}")
    print(f"Interesting:   {len(interesting)} (risk >= {args.min_risk})")
    print(f"JSON:          {json_path}")
    print(f"CSV:           {csv_path}")
    print()
    print_top_table(
        "Top functions by risk",
        interesting,
        [
            ("risk_score", 10),
            ("file", 24),
            ("function", 40),
            ("start_line", 10),
            ("arithmetic_lines", 16),
            ("reasons", 70),
        ],
        limit=25,
    )


if __name__ == "__main__":
    main()
