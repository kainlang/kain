#!/usr/bin/env python3
"""Apply the Kain native runtime prefix rename manifest."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


TEXT_EXTENSIONS = {
    ".bzl",
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hpp",
    ".inc",
    ".json",
    ".kn",
    ".md",
    ".ps1",
    ".py",
    ".rs",
    ".sh",
    ".smt2",
    ".toml",
    ".yaml",
    ".yml",
}

DEFAULT_ROOTS = (
    "runtime",
    "stdlib",
    "crates/sys-codegen",
    "crates/cli",
    "crates/build",
    "tools",
    "blades",
)

SKIP_PARTS = {
    ".git",
    ".kain",
    "cache",
    "generated",
    "out",
    "reports",
    "target",
    "__pycache__",
}

SKIP_FILENAMES = {
    "kain_prefixed_symbol_inventory.json",
    "kain_prefixed_symbol_inventory.md",
    "kain_prefix_rename_manifest.json",
}


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def parser() -> argparse.ArgumentParser:
    repo_root = repo_root_from_script()
    p = argparse.ArgumentParser(description="Apply runtime prefix rename manifest.")
    p.add_argument(
        "--manifest",
        default=str(repo_root / "runtime" / "native" / "kain_prefix_rename_manifest.json"),
    )
    p.add_argument("--repo-root", default=str(repo_root))
    p.add_argument(
        "--roots",
        default=",".join(DEFAULT_ROOTS),
        help="Comma-separated repo-relative roots to scan.",
    )
    p.add_argument("--dry-run", action="store_true")
    return p


def tracked_files(repo_root: Path, roots: tuple[str, ...]) -> list[Path]:
    output = subprocess.check_output(["git", "ls-files"], cwd=repo_root, text=True)
    paths = []
    for line in output.splitlines():
        if not line:
            continue
        normalized = line.replace("\\", "/")
        if roots and not any(normalized == root or normalized.startswith(root.rstrip("/") + "/") for root in roots):
            continue
        path = repo_root / line
        if path.exists() and path.is_file():
            paths.append(path)
    return paths


def should_edit(path: Path, repo_root: Path) -> bool:
    rel = path.relative_to(repo_root)
    if path.name in SKIP_FILENAMES:
        return False
    if any(part in SKIP_PARTS for part in rel.parts):
        return False
    return path.suffix.lower() in TEXT_EXTENSIONS


def build_identifier_regex(identifier_pairs: list[tuple[str, str]]) -> re.Pattern[str]:
    alternation = "|".join(re.escape(old) for old, _new in identifier_pairs)
    return re.compile(rf"\b(?:{alternation})\b")


def apply_text_renames(
    text: str,
    identifier_map: dict[str, str],
    identifier_re: re.Pattern[str],
    path_pairs: list[tuple[str, str]],
) -> str:
    updated = text
    for old_path, new_path in path_pairs:
        updated = updated.replace(old_path, new_path)
        updated = updated.replace(old_path.replace("/", "\\"), new_path.replace("/", "\\"))
        updated = updated.replace(Path(old_path).name, Path(new_path).name)
    return identifier_re.sub(lambda match: identifier_map[match.group(0)], updated)


def main() -> int:
    args = parser().parse_args()
    repo_root = Path(args.repo_root).resolve()
    manifest_path = Path(args.manifest).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("identifier_collisions") or manifest.get("file_collisions"):
        raise SystemExit("manifest has collisions; refusing to apply")

    identifier_pairs = [
        (entry["old"], entry["new"])
        for entry in sorted(manifest["identifier_renames"], key=lambda item: len(item["old"]), reverse=True)
    ]
    identifier_map = dict(identifier_pairs)
    identifier_re = build_identifier_regex(identifier_pairs)
    path_pairs = [
        (entry["old_path"], entry["new_path"])
        for entry in sorted(manifest["file_renames"], key=lambda item: len(item["old_path"]), reverse=True)
    ]
    roots = tuple(root.strip().replace("\\", "/") for root in args.roots.split(",") if root.strip())

    changed_files = []
    for path in tracked_files(repo_root, roots):
        if not should_edit(path, repo_root):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if "kain_" not in text and "KAIN_" not in text:
            continue
        updated = apply_text_renames(text, identifier_map, identifier_re, path_pairs)
        if updated != text:
            changed_files.append(path.relative_to(repo_root).as_posix())
            if not args.dry_run:
                path.write_text(updated, encoding="utf-8", newline="")

    moved_files = []
    for entry in sorted(manifest["file_renames"], key=lambda item: item["old_path"].count("/"), reverse=True):
        src = repo_root / entry["old_path"]
        dst = repo_root / entry["new_path"]
        if not src.exists():
            continue
        if dst.exists():
            raise SystemExit(f"destination already exists: {dst}")
        moved_files.append((entry["old_path"], entry["new_path"]))
        if not args.dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            src.rename(dst)

    print(f"changed_files={len(changed_files)}")
    print(f"moved_files={len(moved_files)}")
    if args.dry_run:
        print("dry_run=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
