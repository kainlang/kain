from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=False), encoding="utf-8")


def render_markdown(report: dict[str, Any]) -> str:
    lines: list[str] = []
    summary = report.get("summary", {})
    lines.append("# Phase2 Selfhost Repair Report")
    lines.append("")
    lines.append(f"- Generated at: `{report.get('generated_at_utc', '<unknown>')}`")
    lines.append(f"- Phase: `{report.get('phase_name', '<unknown>')}`")
    lines.append(f"- Input root: `{report.get('input_root', '<unknown>')}`")
    lines.append(f"- Repaired root: `{report.get('repaired_root', '<unknown>')}`")
    lines.append(f"- Build log: `{report.get('build_log_path', '<missing>')}`")
    lines.append(f"- Input artifacts processed: `{summary.get('artifacts_processed', 0)}`")
    lines.append(f"- Enabled rules: `{summary.get('enabled_rules', 0)}`")
    lines.append(f"- Rules applied: `{summary.get('rules_applied', 0)}`")
    lines.append(f"- Total replacements: `{summary.get('total_replacements', 0)}`")
    lines.append(f"- Recommended next lane: `{report.get('recommended_next_lane', 'phase2-core')}`")
    lines.append("")

    lines.append("## Before / after bucket counts")
    lines.append("")
    lines.append("| Bucket | Before | After |")
    lines.append("| --- | ---: | ---: |")
    before = report.get("before", {}).get("bucket_counts", {})
    after = report.get("after", {}).get("bucket_counts", {})
    bucket_ids = sorted(set(before) | set(after))
    for bucket_id in bucket_ids:
        lines.append(f"| `{bucket_id}` | {before.get(bucket_id, 0)} | {after.get(bucket_id, 0)} |")
    lines.append("")

    lines.append("## Rule hit counts")
    lines.append("")
    rule_hits = sorted(report.get("rule_hits", []), key=lambda item: item.get("replacement_count", 0), reverse=True)
    if not rule_hits:
        lines.append("- none")
    else:
        for item in rule_hits:
            lines.append(
                f"- `{item['rule_id']}`: {item.get('replacement_count', 0)} replacement(s) across {item.get('file_count', 0)} file(s)"
            )
    lines.append("")

    lines.append("## Files most improved")
    lines.append("")
    most_improved = report.get("files_most_improved", [])
    if not most_improved:
        lines.append("- none")
    else:
        for item in most_improved[:15]:
            lines.append(
                f"- `{item['file']}`: {item.get('replacements', 0)} replacement(s), {item.get('rules_triggered', 0)} rule(s)"
            )
    lines.append("")

    lines.append("## Files still failing hardest")
    lines.append("")
    hardest = report.get("files_still_failing_hardest", [])
    if not hardest:
        lines.append("- none")
    else:
        for item in hardest[:15]:
            lines.append(
                f"- `{item['file']}`: {item.get('error_count', 0)} classified error(s)"
            )
    lines.append("")

    lines.append("## Unknown / unclassified failures")
    lines.append("")
    unknowns = report.get("unknown_failures", [])
    if not unknowns:
        lines.append("- none")
    else:
        for item in unknowns[:12]:
            lines.append(f"- `{item}`")
    lines.append("")

    lines.append("## Structural preflight")
    lines.append("")
    preflight = report.get("structural_preflight", {})
    checks = preflight.get("checks", [])
    if not checks:
        lines.append("- none")
    else:
        for item in checks:
            lines.append(f"- `{item['id']}`: failed=`{item.get('failed', False)}`, matches=`{item.get('match_count', 0)}`")
    lines.append("")

    lines.append("## Front Errors")
    lines.append("")
    front_errors = report.get("front_errors", [])
    if not front_errors:
        lines.append("- none")
    else:
        for item in front_errors[:10]:
            location = f"{item.get('file', '<unknown>')}:{item.get('line')}:{item.get('col')}"
            lines.append(f"- `{item.get('bucket', 'unknown')}` `{item.get('code') or 'error'}` at `{location}`")
    lines.append("")

    lines.append("## Rule Promotions")
    lines.append("")
    promotions = report.get("rule_promotions", [])
    if not promotions:
        lines.append("- none")
    else:
        for item in promotions[:20]:
            lines.append(
                f"- `{item.get('rule_id', '<unknown>')}`: `{item.get('promotion_status', 'observed')}` -> `{item.get('target_destination', 'repair-only')}`"
            )
    lines.append("")

    lines.append("## Bootstrap Exceptions")
    lines.append("")
    exceptions = report.get("bounded_bootstrap_exceptions", [])
    if not exceptions:
        lines.append("- none")
    else:
        for item in exceptions[:20]:
            lines.append(
                f"- `{item.get('id', '<unknown>')}`: crate=`{item.get('crate', '<unknown>')}`, via=`{item.get('implemented_via', '<unknown>')}`"
            )
    lines.append("")

    lines.append("## Candidate next manual fixes")
    lines.append("")
    suggestions = report.get("suggested_next_manual_fixes", [])
    if not suggestions:
        lines.append("- none")
    else:
        for item in suggestions:
            lines.append(f"- {item}")
    lines.append("")

    lines.append("## Validation")
    lines.append("")
    validation = report.get("validation", {})
    if not validation:
        lines.append("- not run")
    else:
        for key, value in validation.items():
            if isinstance(value, dict):
                lines.append(f"- `{key}`:")
                for sub_key, sub_value in value.items():
                    lines.append(f"  - `{sub_key}`: `{sub_value}`")
            else:
                lines.append(f"- `{key}`: `{value}`")
    lines.append("")

    return "\n".join(lines)


def write_markdown(path: Path, markdown: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(markdown, encoding="utf-8")
