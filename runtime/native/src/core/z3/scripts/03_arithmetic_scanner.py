"""Scan native-core arithmetic and size math hotspots."""

from __future__ import annotations

import argparse
import re

from _runtime_scan_common import (
    ALLOC_SINK_TOKENS,
    arithmetic_ops_in_line,
    extract_functions,
    iter_core_sources,
    nearby_guard,
    print_top_table,
    stable_relpath,
    write_json_and_csv,
)

CAST_RE = re.compile(r"\((?:size_t|uintptr_t|uint\d+_t|int\d+_t|long|unsigned long)\)")


def analyze_function(func: dict, min_risk: int) -> list[dict]:
    rows: list[dict] = []
    for offset, raw_line in enumerate(func["lines"]):
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("#"):
            continue
        operators = arithmetic_ops_in_line(stripped)
        if not operators:
            continue
        if not any(
            hint in stripped.lower()
            for hint in ("size", "length", "capacity", "count", "offset", "stride", "bytes", "slot", "index", "probe")
        ):
            continue

        guard_hits = nearby_guard(func["lines"], offset, window=6)
        alloc_hits = [token for token in ALLOC_SINK_TOKENS if token in stripped]
        score = 0
        reasons: list[str] = []

        if not guard_hits:
            score += 30
            reasons.append("no nearby overflow or range guard")
        if alloc_hits:
            score += 20
            reasons.append("size math feeds allocation or copy sink")
        if "*" in operators:
            score += 20
            reasons.append("multiplication can amplify overflow")
        if "<<" in operators:
            score += 20
            reasons.append("shift-based growth math")
        if CAST_RE.search(stripped):
            score += 15
            reasons.append("explicit cast in arithmetic site")
        if "+ 1" in stripped or "+1" in stripped or "1u" in stripped:
            score += 10
            reasons.append("sentinel/null byte style increment")
        if "->" in stripped or "[" in stripped:
            score += 10
            reasons.append("pointer or indexed arithmetic")

        if score < min_risk:
            continue

        rows.append(
            {
                "risk_score": score,
                "file": stable_relpath(func["file"]),
                "function": func["name"],
                "line": func["start_line"] + offset,
                "operators": "|".join(operators),
                "alloc_hits": "|".join(alloc_hits),
                "guard_hits": "|".join(guard_hits),
                "has_cast": int(bool(CAST_RE.search(stripped))),
                "expression": stripped[:160],
                "reasons": " | ".join(reasons),
            }
        )
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--min-risk", type=int, default=30)
    args = parser.parse_args()

    rows: list[dict] = []
    for source_path in iter_core_sources():
        for func in extract_functions(source_path):
            rows.extend(analyze_function(func, args.min_risk))

    rows.sort(key=lambda row: row["risk_score"], reverse=True)
    json_path, csv_path = write_json_and_csv("arithmetic_sites", rows)

    print("Native Core Arithmetic Scanner")
    print(f"Sites:          {len(rows)}")
    print(f"JSON:           {json_path}")
    print(f"CSV:            {csv_path}")
    print()
    print_top_table(
        "Top arithmetic hotspots",
        rows,
        [
            ("risk_score", 10),
            ("file", 24),
            ("function", 38),
            ("line", 10),
            ("operators", 12),
            ("guard_hits", 24),
            ("reasons", 68),
        ],
        limit=30,
    )


if __name__ == "__main__":
    main()
