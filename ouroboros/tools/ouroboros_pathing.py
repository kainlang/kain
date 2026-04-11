from __future__ import annotations

import os
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class WorkspaceContext:
    repo_root: Path
    ouroboros_root: Path


def _looks_like_ouroboros_root(path: Path) -> bool:
    return (
        (path / "docs" / "selfhost").exists()
        and (path / "automation").exists()
        and (path / "tools").exists()
    )


def discover_workspace_context(anchor: str | Path) -> WorkspaceContext:
    env_ouroboros_root = os.environ.get("OUROBOROS_ROOT")
    env_repo_root = os.environ.get("KAIN_REPO_ROOT")
    if env_ouroboros_root:
        ouroboros_root = Path(env_ouroboros_root).expanduser().resolve()
        repo_root = (
            Path(env_repo_root).expanduser().resolve()
            if env_repo_root
            else ouroboros_root.parent.resolve()
        )
        return WorkspaceContext(repo_root=repo_root, ouroboros_root=ouroboros_root)

    anchor_path = Path(anchor).expanduser().resolve()
    search_root = anchor_path if anchor_path.is_dir() else anchor_path.parent
    for candidate in (search_root, *search_root.parents):
        if _looks_like_ouroboros_root(candidate):
            return WorkspaceContext(
                repo_root=candidate.parent.resolve(),
                ouroboros_root=candidate.resolve(),
            )
        nested = candidate / "ouroboros"
        if _looks_like_ouroboros_root(nested):
            return WorkspaceContext(
                repo_root=candidate.resolve(),
                ouroboros_root=nested.resolve(),
            )

    raise RuntimeError(
        f"Unable to locate Ouroboros workspace roots from {anchor_path}"
    )


def executable_candidates(base_name: str) -> list[str]:
    if os.name == "nt":
        return [f"{base_name}.exe", base_name]
    return [base_name, f"{base_name}.exe"]


def seed_runtime_defaults(context: WorkspaceContext) -> dict[str, str]:
    return {
        "repo_root": context.repo_root.as_posix(),
        "ouroboros_root": context.ouroboros_root.as_posix(),
        "python_executable": Path(sys.executable).resolve().as_posix(),
        "kain_binary_name": executable_candidates("kain")[0],
    }


def resolve_template_defaults(
    raw_defaults: dict[str, object] | None,
    runtime_defaults: dict[str, str],
) -> dict[str, str]:
    resolved = dict(runtime_defaults)
    pending = {
        key: str(value)
        for key, value in (raw_defaults or {}).items()
    }
    for _ in range(8):
        changed = False
        for key, template in pending.items():
            rendered = template.format(**resolved)
            if resolved.get(key) != rendered:
                resolved[key] = rendered
                changed = True
        if not changed:
            break
    return resolved
