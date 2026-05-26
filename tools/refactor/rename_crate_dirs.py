#!/usr/bin/env python3
"""Rename Kain crate folders while preserving package names.

This is a path-only workspace migration:
- crate directory names under crates/ drop the `kain-` prefix
- Cargo package names and dependency keys stay unchanged
- tracked text files get path-shaped rewrites before directories move
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


RENAME_MAP = {
    "kain-3D": "3d",
    "kain-actor": "actor",
    "kain-amalgamate": "amalgamate",
    "kain-asm": "asm",
    "kain-blades": "blades",
    "kain-build": "build",
    "kain-c-ffi": "c-ffi",
    "kain-check": "check",
    "kain-clean": "clean",
    "kain-codebase": "codebase",
    "kain-commands": "commands",
    "kain-core": "core",
    "kain-crate-ffi": "crate-ffi",
    "kain-driver": "driver",
    "kain-entangle": "entangle",
    "kain-fast3d-runtime": "fast3d-runtime",
    "kain-foreign-abi": "foreign-abi",
    "kain-fs": "fs",
    "kain-gpu-runtime": "gpu-runtime",
    "kain-host": "host",
    "kain-host-derive": "host-derive",
    "kain-import": "import",
    "kain-input": "input",
    "kain-interop": "interop",
    "kain-lattice": "lattice",
    "kain-net": "net",
    "kain-node": "node",
    "kain-omni": "omni",
    "kain-ownership": "ownership",
    "kain-process": "process",
    "kain-python": "python",
    "kain-reflect": "reflect",
    "kain-repair": "repair",
    "kain-repl": "repl",
    "kain-run": "run",
    "kain-script": "script",
    "kain-sdk": "sdk",
    "kain-selfhost": "selfhost",
    "kain-stdlib-map": "stdlib-map",
    "kain-sys-codegen": "sys-codegen",
    "kain-test": "test",
    "kain-ui": "ui",
    "kain-ui-native": "ui-native",
    "kain-ui-tauri": "ui-tauri",
    "kain-wasm": "wasm",
    "kain-web": "web",
}

SKIP_PARTS = {
    ".git",
    ".kain",
    "target",
    "__pycache__",
}


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def tracked_files(repo_root: Path) -> list[Path]:
    output = subprocess.check_output(
        ["git", "ls-files", "-z"],
        cwd=repo_root,
    )
    files: list[Path] = []
    for raw in output.split(b"\x00"):
        if not raw:
            continue
        rel = raw.decode("utf-8")
        path = repo_root / rel
        if path.is_file():
            files.append(path)
    return files


def should_skip(path: Path, repo_root: Path) -> bool:
    rel = path.relative_to(repo_root)
    return any(part in SKIP_PARTS for part in rel.parts)


def replacement_pairs() -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    for old, new in sorted(RENAME_MAP.items(), key=lambda item: len(item[0]), reverse=True):
        pairs.extend(
            [
                (f"crates/{old}", f"crates/{new}"),
                (f"crates\\{old}", f"crates\\{new}"),
                (f"../{old}", f"../{new}"),
                (f"..\\{old}", f"..\\{new}"),
                (f"./{old}", f"./{new}"),
                (f".\\{old}", f".\\{new}"),
                (f"/{old}/", f"/{new}/"),
                (f"\\{old}\\", f"\\{new}\\"),
                (f'.join("{old}")', f'.join("{new}")'),
                (f".join('{old}')", f".join('{new}')"),
                (f'.push("{old}")', f'.push("{new}")'),
                (f".push('{old}')", f".push('{new}')"),
            ]
        )
    return pairs


def rewrite_tracked_files(repo_root: Path, dry_run: bool) -> tuple[int, list[str]]:
    pairs = replacement_pairs()
    changed_files: list[str] = []
    for path in tracked_files(repo_root):
        if should_skip(path, repo_root):
            continue
        raw = path.read_bytes()
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            continue
        updated = text
        for old, new in pairs:
            updated = updated.replace(old, new)
        if updated == text:
            continue
        changed_files.append(path.relative_to(repo_root).as_posix())
        if not dry_run:
            path.write_bytes(updated.encode("utf-8"))
    return len(changed_files), changed_files


def rename_directories(repo_root: Path, dry_run: bool) -> list[tuple[str, str]]:
    renamed: list[tuple[str, str]] = []
    crates_dir = repo_root / "crates"
    for old, new in sorted(RENAME_MAP.items()):
        src = crates_dir / old
        dst = crates_dir / new
        if not src.exists():
            raise SystemExit(f"missing source directory: {src}")
        if dst.exists():
            raise SystemExit(f"destination already exists: {dst}")
        renamed.append((src.relative_to(repo_root).as_posix(), dst.relative_to(repo_root).as_posix()))
        if not dry_run:
            src.rename(dst)
    return renamed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(repo_root_from_script()))
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    changed_count, changed_files = rewrite_tracked_files(repo_root, dry_run=args.dry_run)
    renamed_dirs = rename_directories(repo_root, dry_run=args.dry_run)
    print(f"changed_files={changed_count}")
    print(f"renamed_dirs={len(renamed_dirs)}")
    if changed_files:
        print("first_changed_files=")
        for rel in changed_files[:20]:
            print(rel)
    if renamed_dirs:
        print("first_renamed_dirs=")
        for src, dst in renamed_dirs[:20]:
            print(f"{src} -> {dst}")
    if args.dry_run:
        print("dry_run=true")
    return 0


if __name__ == "__main__":
    sys.exit(main())
