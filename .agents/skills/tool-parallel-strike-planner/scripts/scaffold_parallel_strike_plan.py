#!/usr/bin/env python3
"""Scaffold a same-checkout parallel strike plan folder."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DEFAULT_LANES = (
    ("ALPHA", "first seam"),
    ("CHARLIE", "second seam"),
    ("DELTA", "third seam"),
)


def title_from_slug(slug: str) -> str:
    return " ".join(part.capitalize() for part in slug.replace("_", "-").split("-") if part)


def normalize_lane_name(raw: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_-]+", "", raw).strip("-_")
    return cleaned.upper()


def parse_lane(raw: str) -> tuple[str, str]:
    for separator in (":", "="):
        if separator in raw:
            name_raw, purpose_raw = raw.split(separator, 1)
            break
    else:
        name_raw, purpose_raw = raw, "TODO: define lane purpose"
    name = normalize_lane_name(name_raw)
    purpose = purpose_raw.strip() or "TODO: define lane purpose"
    if not name:
        raise ValueError(f"Invalid lane specifier: {raw!r}")
    return name, purpose


def build_readme(title: str, slug: str, lanes: list[tuple[str, str]]) -> str:
    lane_lines = "\n".join(
        f"{index}. `{name}`: {purpose}" for index, (name, purpose) in enumerate(lanes, start=1)
    )
    ownership_sections = "\n\n".join(
        f"### `{name}`\n\nOwns {purpose}.\n\n- `TODO`: exact owned files\n- `TODO`: exact shared glue file only if unavoidable"
        for name, purpose in lanes
    )
    merge_lines = "\n".join(
        f"{index}. `{name}`" for index, (name, _purpose) in enumerate(lanes, start=1)
    )
    return f"""# {title}

Private {len(lanes)}-agent execution plan for `TODO: describe the strike goal for {slug}`.

## Mission

Ship {len(lanes)} upgrades in parallel without merge sludge:

{lane_lines}

The end state is not `TODO: weak summary`. The end state is:

- `TODO`: user-visible or subsystem-visible outcome
- `TODO`: repo-truth outcome
- `TODO`: proof, smoke, or benchmark outcome

## Current Repo Truth

- `TODO`: key live files and current behavior
- `TODO`: blockers that are structural rather than conceptual
- `TODO`: prepass status if a seam split already landed

## Frozen Boundaries

### Shared rules

- All lanes execute in the same local checkout.
- No worktrees.
- Every lane must be startable immediately.
- If a lane must wait for another lane before it can begin, redesign the boundary or add a prepass.
- Keep edits surgical and ownership-clean in the dirty shared tree.
- Reserve conflict-heavy files to one finisher lane or consolidation.

### Shared public surfaces allowed this pass

- `TODO`: narrow shared folder/file list

## Parallel Ownership Map

{ownership_sections}

## Global Definition Of Done

The strike is complete when all of these are true:

1. `TODO`
2. `TODO`
3. `TODO`
4. `TODO`

## Merge Order

Recommended landing order:

{merge_lines}

Why:

- `TODO`: explain landing order without implying start-order dependencies

## Shared Validation Floor

During lane execution, keep validation lane-local and focused.

Focused lane checks:

```powershell
# TODO: lane-local checks
```

Consolidation checks:

```powershell
# TODO: post-strike checks
```

## Coordination Contract

Every lane brief must leave behind:

- exact files touched
- exact public surfaces changed
- exact proof, benchmark, or smoke artifacts added or rerun
- known compromises
- unresolved seams another lane must consume

No vague `done` claims. Only owned files, proof, and repo truth.
"""


def build_lane_doc(name: str, purpose: str) -> str:
    return f"""# {name}

## Lane

{purpose}

## Mission

Own the `{purpose}` seam without forcing other lanes to wait for this diff first.

## Owns

- `TODO`: exact owned files
- `TODO`: exact shared glue file only if unavoidable

## Do Not Own

- `TODO`: adjacent seams reserved for other lanes

## Deliverables

1. `TODO`
2. `TODO`
3. `TODO`

## Design Direction

- Keep the lane immediately startable in the shared checkout.
- Prefer one thin glue seam over broad cross-lane edits.
- Leave cold-merge files for the finisher lane if this lane does not truly own them.

## Proof Obligations

- `TODO`: solver, proof pack, invariant, or contract checks

## Validation Duties

- `TODO`: lane-local validation commands

## Smoke Target

- `TODO`: one focused smoke, demo, or authored proof lane

## Exit Criteria

- owned files are complete
- validation evidence is honest
- another lane does not need to finish first for this lane to begin
"""


def write_text(path: Path, text: str, force: bool) -> None:
    if path.exists() and not force:
        raise FileExistsError(f"Refusing to overwrite existing file without --force: {path}")
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Scaffold a same-checkout parallel strike plan folder.",
    )
    parser.add_argument("--root", required=True, help="Parent directory for the plan folder")
    parser.add_argument("--slug", required=True, help="Plan folder name, such as cuda-pipeline-strike")
    parser.add_argument("--title", help="Plan title for README.md")
    parser.add_argument(
        "--lane",
        action="append",
        default=[],
        help="Lane definition in NAME:purpose form. Repeat for multiple lanes.",
    )
    parser.add_argument("--force", action="store_true", help="Overwrite existing files")
    args = parser.parse_args()

    slug = args.slug.strip()
    if not slug:
        print("[ERROR] --slug cannot be empty.", file=sys.stderr)
        return 1

    title = args.title.strip() if args.title else title_from_slug(slug)
    lanes = [parse_lane(raw) for raw in args.lane] if args.lane else list(DEFAULT_LANES)
    if len({name for name, _purpose in lanes}) != len(lanes):
        print("[ERROR] Lane names must be unique.", file=sys.stderr)
        return 1

    plan_dir = Path(args.root).resolve() / slug
    plan_dir.mkdir(parents=True, exist_ok=True)

    write_text(plan_dir / "README.md", build_readme(title, slug, lanes), args.force)
    for name, purpose in lanes:
        write_text(plan_dir / f"{name}.md", build_lane_doc(name, purpose), args.force)

    print(f"[OK] Scaffolded plan at {plan_dir}")
    print("[OK] Files:")
    print(f"  - {plan_dir / 'README.md'}")
    for name, _purpose in lanes:
        print(f"  - {plan_dir / f'{name}.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
