"""Find native-core synchronization and shared-state gaps."""

from __future__ import annotations

import argparse
import re

from _runtime_scan_common import (
    ATOMIC_TOKENS,
    DESTROY_TOKENS,
    LOCK_TOKENS,
    ONCE_TOKENS,
    extract_functions,
    function_line_excerpt,
    iter_core_sources,
    print_top_table,
    stable_relpath,
    write_json_and_csv,
)

COUNT_WRITE_RE = re.compile(r"(\b\w*count\w*\b\s*(?:\+\+|\+=\s*1))|(\b\w*count\w*\b\s*=\s*\b\w*count\w*\b\s*\+\s*1)")
GLOBAL_WRITE_RE = re.compile(r"\bg_[A-Za-z_]\w*\b\s*=")
SHARED_SLOT_RE = re.compile(r"\[\s*[^]]*count[^]]*\s*\]")


def analyze_function(func: dict) -> list[dict]:
    body = func["body"]
    lines = func["lines"]
    findings: list[dict] = []
    has_lock = any(token in body for token in LOCK_TOKENS)
    has_once = any(token in body for token in ONCE_TOKENS)
    has_atomic = any(token in body for token in ATOMIC_TOKENS)
    init_mentions = [idx for idx, line in enumerate(lines, start=func["start_line"]) if "initialized" in line]
    destroy_mentions = [idx for idx, line in enumerate(lines, start=func["start_line"]) if any(token in line for token in DESTROY_TOKENS)]
    count_mentions = [idx for idx, line in enumerate(lines, start=func["start_line"]) if COUNT_WRITE_RE.search(line)]
    global_writes = [idx for idx, line in enumerate(lines, start=func["start_line"]) if GLOBAL_WRITE_RE.search(line)]
    shared_slot_lines = [idx for idx, line in enumerate(lines, start=func["start_line"]) if SHARED_SLOT_RE.search(line)]

    if init_mentions and ("init" in func["name"] or "ensure_initialized" in func["name"]):
        if not (has_lock or has_once or has_atomic):
            findings.append(
                {
                    "score": 95,
                    "kind": "plain-init-race",
                    "file": stable_relpath(func["file"]),
                    "function": func["name"],
                    "start_line": func["start_line"],
                    "evidence_line": init_mentions[0],
                    "details": "lazy init flag is checked and/or written without lock, once primitive, or atomic guard",
                    "excerpt": function_line_excerpt(lines),
                }
            )

    if count_mentions and not (has_lock or has_atomic):
        findings.append(
            {
                "score": 88,
                "kind": "lost-update-counter",
                "file": stable_relpath(func["file"]),
                "function": func["name"],
                "start_line": func["start_line"],
                "evidence_line": count_mentions[0],
                "details": "count-like state mutates without lock or atomic primitive",
                "excerpt": function_line_excerpt(lines),
            }
        )

    if shared_slot_lines and count_mentions and not (has_lock or has_atomic):
        findings.append(
            {
                "score": 92,
                "kind": "shared-slot-overwrite",
                "file": stable_relpath(func["file"]),
                "function": func["name"],
                "start_line": func["start_line"],
                "evidence_line": shared_slot_lines[0],
                "details": "count-derived slot selection and count increment both happen without synchronization",
                "excerpt": function_line_excerpt(lines),
            }
        )

    if global_writes and not (has_lock or has_once or has_atomic):
        findings.append(
            {
                "score": 75,
                "kind": "unsynced-global-write",
                "file": stable_relpath(func["file"]),
                "function": func["name"],
                "start_line": func["start_line"],
                "evidence_line": global_writes[0],
                "details": "writes global state without lock, once primitive, or atomic primitive",
                "excerpt": function_line_excerpt(lines),
            }
        )

    if destroy_mentions and "unlock" in body and not has_once:
        findings.append(
            {
                "score": 55,
                "kind": "teardown-after-unlock",
                "file": stable_relpath(func["file"]),
                "function": func["name"],
                "start_line": func["start_line"],
                "evidence_line": destroy_mentions[0],
                "details": "tears down synchronization or heap state after unlocking; review lifetime assumptions",
                "excerpt": function_line_excerpt(lines),
            }
        )

    return findings


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--min-score", type=int, default=50)
    args = parser.parse_args()

    findings: list[dict] = []
    for source_path in iter_core_sources():
        for func in extract_functions(source_path):
            findings.extend(analyze_function(func))

    findings.sort(key=lambda row: row["score"], reverse=True)
    interesting = [row for row in findings if row["score"] >= args.min_score]
    json_path, csv_path = write_json_and_csv("sync_findings", findings)

    print("Native Core Sync Gap Scanner")
    print(f"Findings:      {len(findings)}")
    print(f"Interesting:   {len(interesting)} (score >= {args.min_score})")
    print(f"JSON:          {json_path}")
    print(f"CSV:           {csv_path}")
    print()
    print_top_table(
        "Top synchronization findings",
        interesting,
        [
            ("score", 8),
            ("kind", 24),
            ("file", 24),
            ("function", 40),
            ("evidence_line", 14),
            ("details", 72),
        ],
        limit=30,
    )


if __name__ == "__main__":
    main()
