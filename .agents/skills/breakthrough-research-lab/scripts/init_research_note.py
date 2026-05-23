#!/usr/bin/env python3
"""Create a repo-local research note skeleton for a collaborative session."""

from __future__ import annotations

import argparse
import re
import sys
from datetime import datetime
from pathlib import Path


NOTE_TEMPLATE = """# {title}

- Date: {date}
- Status: active
- Repo Root: `{repo_root}`
- Session Slug: `{slug}`

## Research Question

{question}

## Constraints

- [ ]

## Hypothesis Lattice

### Baseline
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

### Unconventional
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

### Moonshot
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

## Mathematical Model

- Variables:
- Invariants:
- Objective:
- Bad states:
- Simplifying assumptions:

## Z3 Claims

1. ...
2. ...

## Evidence And Sources

- Local:
- External:

## Dead Ends

- None yet.

## Conclusion

Pending.
"""


def normalize_slug(value: str) -> str:
    normalized = value.strip().lower()
    normalized = re.sub(r"[^a-z0-9]+", "-", normalized)
    normalized = normalized.strip("-")
    normalized = re.sub(r"-{2,}", "-", normalized)
    return normalized


def title_from_slug(slug: str) -> str:
    return " ".join(part.capitalize() for part in slug.split("-"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a timestamped research note under <repo-root>/research.",
    )
    parser.add_argument("--repo-root", required=True, help="Repository root for the research note")
    parser.add_argument("--slug", required=True, help="Short session slug; normalized to hyphen-case")
    parser.add_argument("--question", default="[fill in the frontier question]", help="Initial research question")
    parser.add_argument("--title", help="Optional title override")
    parser.add_argument("--force", action="store_true", help="Overwrite an existing note for the same date and slug")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    if not repo_root.exists() or not repo_root.is_dir():
        print(f"[ERROR] Repo root does not exist or is not a directory: {repo_root}", file=sys.stderr)
        return 1

    slug = normalize_slug(args.slug)
    if not slug:
        print("[ERROR] Slug must contain at least one letter or digit.", file=sys.stderr)
        return 1

    research_dir = repo_root / "research"
    research_dir.mkdir(parents=True, exist_ok=True)

    today = datetime.now().strftime("%Y-%m-%d")
    title = args.title.strip() if args.title else title_from_slug(slug)
    note_path = research_dir / f"{today}-{slug}.md"

    if note_path.exists() and not args.force:
        print(
            f"[ERROR] Research note already exists: {note_path}. Use --force to overwrite.",
            file=sys.stderr,
        )
        return 1

    content = NOTE_TEMPLATE.format(
        title=title,
        date=today,
        repo_root=repo_root,
        slug=slug,
        question=args.question.strip() or "[fill in the frontier question]",
    )
    note_path.write_text(content, encoding="utf-8")
    print(note_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
