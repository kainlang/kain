#!/usr/bin/env python3
"""Rank Kain benchmark cases that beat selected foreign baselines.

This script is intentionally dependency-free so agents can rerun it before
translation work instead of trusting a stale skill reference.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def _median(case: dict[str, Any], language: str) -> float | None:
    run = case.get("run") or {}
    entry = run.get(language) or {}
    value = entry.get("median_ms")
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _source(case: dict[str, Any], language: str) -> str:
    source = case.get("source") or {}
    return str(source.get(language) or "")


def _default_report(repo: Path) -> Path:
    latest = repo / "benchmark" / "out" / "reports" / "latest.json"
    if latest.exists():
        return latest
    return repo / "benchmark" / "out" / "reports" / "20260520T005049Z.json"


def _rank_cases(report: dict[str, Any], comparators: list[str], implemented_only: bool) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for case in report.get("cases", []):
        maturity = str(case.get("maturity") or "")
        if implemented_only and maturity != "implemented":
            continue
        kain_ms = _median(case, "kain")
        if kain_ms is None or kain_ms <= 0:
            continue
        ratios: dict[str, float] = {}
        medians: dict[str, float] = {"kain": kain_ms}
        ok = True
        for language in comparators:
            median = _median(case, language)
            if median is None or median <= kain_ms:
                ok = False
                break
            medians[language] = median
            ratios[language] = median / kain_ms
        if not ok:
            continue
        rows.append(
            {
                "id": case.get("id") or "",
                "title": case.get("title") or case.get("id") or "",
                "maturity": maturity,
                "min_ratio": min(ratios.values()),
                "ratios": ratios,
                "medians": medians,
                "kain_source": _source(case, "kain"),
                "fairness_note": case.get("fairness_note") or "",
            }
        )
    rows.sort(key=lambda row: row["min_ratio"], reverse=True)
    return rows


def _fmt(value: float) -> str:
    return f"{value:.3f}".rstrip("0").rstrip(".")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=".", help="Repository root. Default: current directory.")
    parser.add_argument("--report", help="Benchmark JSON report. Default: benchmark/out/reports/latest.json.")
    parser.add_argument("--top", type=int, default=10, help="Number of rows to print.")
    parser.add_argument(
        "--languages",
        default="rust,cpp",
        help="Comma-separated foreign baselines Kain must beat. Default: rust,cpp.",
    )
    parser.add_argument(
        "--implemented-only",
        action="store_true",
        help="Exclude proxy/semantic-proxy/dispatch rows.",
    )
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    report_path = Path(args.report).resolve() if args.report else _default_report(repo)
    comparators = [item.strip() for item in args.languages.split(",") if item.strip()]
    report = json.loads(report_path.read_text(encoding="utf-8"))
    rows = _rank_cases(report, comparators, args.implemented_only)

    generated_at = report.get("generated_at") or "unknown"
    print(f"# Kain Translation Benchmark Compass")
    print()
    print(f"- report: `{report_path}`")
    print(f"- generated_at: `{generated_at}`")
    print(f"- comparators: `{', '.join(comparators)}`")
    print()
    print("| rank | case | maturity | min x | medians ms | Kain source |")
    print("| ---: | --- | --- | ---: | --- | --- |")
    for rank, row in enumerate(rows[: args.top], start=1):
        medians = ", ".join(f"{lang} {_fmt(ms)}" for lang, ms in row["medians"].items())
        print(
            f"| {rank} | `{row['id']}` | `{row['maturity']}` | "
            f"{row['min_ratio']:.2f} | {medians} | `{row['kain_source']}` |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
