#!/usr/bin/env python3
"""Rank suspicious constants and bit tricks for solver-guided optimization."""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Iterable


TEXT_EXTENSIONS = {
    ".c", ".h", ".cc", ".cpp", ".hpp", ".rs", ".kn", ".ks", ".ts", ".tsx",
    ".js", ".jsx", ".go", ".py", ".zig", ".odin", ".java", ".cs", ".glsl",
    ".hlsl", ".wgsl", ".metal", ".smt2", ".yaml", ".yml", ".toml",
}

DEFAULT_EXCLUDES = {
    ".git", "target", "node_modules", "dist", "build", ".bazel-cache",
    ".kain", "__pycache__", ".tmp", "tmp",
}

NUMBER_RE = re.compile(
    r"(?P<number>\b0x[0-9a-fA-F_]+(?:u?ll|ul|u|l)?\b|\b\d[\d_]*(?:u?ll|ul|u|l)?\b)"
)

BITWISE_RE = re.compile(r"(&&?|\|\|?|\^|<<|>>|~|rotate|rotl|rotr|popcount|ctz|clz|debruijn)", re.I)
BRANCH_RE = re.compile(r"\b(if|else|switch|case|match|for|while)\b")
HOT_WORD_RE = re.compile(
    r"(hash|mask|slot|index|probe|token|lookup|table|capacity|ring|flag|state|kind|selector|dispatch|runtime|allocator)",
    re.I,
)


def parse_int_literal(text: str) -> int | None:
    suffix_stripped = re.sub(r"(u?ll|ul|u|l)$", "", text.replace("_", ""), flags=re.I)
    try:
        return int(suffix_stripped, 16 if suffix_stripped.lower().startswith("0x") else 10)
    except ValueError:
        return None


def is_power_of_two(value: int) -> bool:
    return value > 0 and (value & (value - 1)) == 0


def score_literal(value: int, literal: str, line: str) -> tuple[int, list[str]]:
    score = 0
    reasons: list[str] = []

    if literal.lower().startswith("0x"):
        score += 3
        reasons.append("hex-literal")
    if value > 255:
        score += 1
        reasons.append("nontrivial-width")
    if value > 0xFFFF_FFFF:
        score += 2
        reasons.append("u64-scale")
    if is_power_of_two(value):
        score += 2
        reasons.append("power-of-two")
    if is_power_of_two(value + 1):
        score += 3
        reasons.append("all-ones-mask")
    if value & 1 and value > 0xFFFF:
        score += 2
        reasons.append("odd-multiplier-candidate")
    if BITWISE_RE.search(line):
        score += 4
        reasons.append("bitwise-neighborhood")
    if BRANCH_RE.search(line):
        score += 1
        reasons.append("branch-neighborhood")
    if HOT_WORD_RE.search(line):
        score += 2
        reasons.append("hot-name-neighborhood")
    if value in {33, 29, 13, 27, 37, 64, 128, 256, 512, 1024}:
        score += 1
        reasons.append("common-shift-or-capacity")

    return score, reasons


def iter_files(paths: Iterable[Path], max_bytes: int, excludes: set[str]) -> Iterable[Path]:
    for root in paths:
        if root.is_file():
            if root.suffix.lower() in TEXT_EXTENSIONS and root.stat().st_size <= max_bytes:
                yield root
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if any(part in excludes for part in path.parts):
                continue
            if path.suffix.lower() not in TEXT_EXTENSIONS:
                continue
            try:
                if path.stat().st_size > max_bytes:
                    continue
            except OSError:
                continue
            yield path


def scan_file(path: Path) -> list[dict[str, object]]:
    findings: list[dict[str, object]] = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return findings

    for line_number, line in enumerate(text.splitlines(), start=1):
        for match in NUMBER_RE.finditer(line):
            literal = match.group("number")
            value = parse_int_literal(literal)
            if value is None:
                continue
            score, reasons = score_literal(value, literal, line)
            if score < 4:
                continue
            findings.append(
                {
                    "score": score,
                    "path": str(path),
                    "line": line_number,
                    "literal": literal,
                    "value": value,
                    "bit_width": max(1, math.ceil(value.bit_length() / 8) * 8),
                    "reasons": reasons,
                    "snippet": line.strip()[:220],
                }
            )
    return findings


def print_markdown(findings: list[dict[str, object]], limit: int) -> None:
    print("# Magic Candidate Scan")
    print()
    if not findings:
        print("No high-signal candidates found.")
        return
    for item in findings[:limit]:
        reasons = ", ".join(item["reasons"])  # type: ignore[arg-type]
        print(f"- score {item['score']}: `{item['literal']}` at `{item['path']}:{item['line']}`")
        print(f"  - reasons: {reasons}")
        print(f"  - snippet: `{item['snippet']}`")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="Files or directories to scan")
    parser.add_argument("--json", action="store_true", help="Emit JSON instead of Markdown")
    parser.add_argument("--limit", type=int, default=80, help="Maximum findings to print")
    parser.add_argument("--max-bytes", type=int, default=1_000_000, help="Skip files larger than this")
    parser.add_argument("--exclude", action="append", default=[], help="Additional path part to exclude")
    args = parser.parse_args()

    excludes = DEFAULT_EXCLUDES | set(args.exclude)
    findings: list[dict[str, object]] = []
    for path in iter_files(args.paths, args.max_bytes, excludes):
        findings.extend(scan_file(path))
    findings.sort(key=lambda item: (-int(item["score"]), str(item["path"]), int(item["line"])))

    if args.json:
        print(json.dumps(findings[: args.limit], indent=2))
    else:
        print_markdown(findings, args.limit)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
