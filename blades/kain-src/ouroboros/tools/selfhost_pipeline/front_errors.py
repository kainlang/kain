from __future__ import annotations

import json
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class FrontError:
    code: str | None
    text: str
    file: str | None
    line: int | None
    col: int | None
    bucket: str


def parse_error_blocks(log_text: str) -> list[str]:
    blocks: list[str] = []
    current: list[str] = []
    for line in log_text.splitlines():
        if line.startswith("error"):
            if current:
                blocks.append("\n".join(current).strip())
            current = [line]
        elif current:
            if line.startswith("warning"):
                blocks.append("\n".join(current).strip())
                current = []
            else:
                current.append(line)
    if current:
        blocks.append("\n".join(current).strip())
    return [block for block in blocks if block]


def load_taxonomy(taxonomy_path: Path) -> list[dict[str, Any]]:
    if not taxonomy_path.exists():
        return []
    payload = json.loads(taxonomy_path.read_text(encoding="utf-8"))
    return payload.get("buckets", [])


def classify(block: str, buckets: list[dict[str, Any]]) -> str:
    for bucket in buckets:
        bucket_id = bucket.get("id", "unknown")
        if bucket_id == "unknown":
            continue
        for pattern in bucket.get("regexes", []):
            if re.search(pattern, block, flags=re.IGNORECASE | re.MULTILINE):
                return bucket_id
    return "unknown"


def extract_front_errors(log_path: Path, taxonomy_path: Path, limit: int = 25) -> dict[str, Any]:
    if not log_path.exists():
        return {
            "exists": False,
            "log_path": log_path.as_posix(),
            "front_errors": [],
            "bucket_counts": {},
        }

    buckets = load_taxonomy(taxonomy_path)
    errors: list[FrontError] = []
    for block in parse_error_blocks(log_path.read_text(encoding="utf-8", errors="ignore"))[:limit]:
        first = block.splitlines()[0] if block else ""
        code = None
        if first.startswith("error[") and "]" in first:
            code = first.split("[", 1)[1].split("]", 1)[0]
        file = None
        line = None
        col = None
        for raw in block.splitlines():
            stripped = raw.strip()
            if stripped.startswith("-->"):
                loc = stripped.removeprefix("-->").strip()
                parts = loc.rsplit(":", 2)
                if len(parts) == 3:
                    file = parts[0]
                    try:
                        line = int(parts[1])
                        col = int(parts[2])
                    except ValueError:
                        line = None
                        col = None
                break
        errors.append(
            FrontError(
                code=code,
                text=block,
                file=file,
                line=line,
                col=col,
                bucket=classify(block, buckets),
            )
        )

    counts = Counter(error.bucket for error in errors)
    return {
        "exists": True,
        "log_path": log_path.as_posix(),
        "front_errors": [error.__dict__ for error in errors],
        "bucket_counts": dict(counts),
    }


def render_front_errors_markdown(front_payload: dict[str, Any]) -> str:
    lines = ["# Front Errors", ""]
    lines.append(f"- Log: `{front_payload.get('log_path', '<missing>')}`")
    lines.append(f"- Exists: `{front_payload.get('exists', False)}`")
    lines.append("")
    lines.append("## Bucket counts")
    lines.append("")
    counts = front_payload.get("bucket_counts", {})
    if not counts:
        lines.append("- none")
    else:
        for key, value in sorted(counts.items()):
            lines.append(f"- `{key}`: `{value}`")
    lines.append("")
    lines.append("## First errors")
    lines.append("")
    for item in front_payload.get("front_errors", []):
        header = item.get("code") or "error"
        file = item.get("file") or "<unknown>"
        line = item.get("line")
        col = item.get("col")
        lines.append(f"### `{header}` — `{item.get('bucket', 'unknown')}`")
        lines.append("")
        lines.append(f"- Location: `{file}:{line}:{col}`")
        lines.append("")
        lines.append("```text")
        lines.append(item.get("text", ""))
        lines.append("```")
        lines.append("")
    if not front_payload.get("front_errors"):
        lines.append("- none")
    lines.append("")
    return "\n".join(lines)
