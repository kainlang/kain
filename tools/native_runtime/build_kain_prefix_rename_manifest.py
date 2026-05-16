#!/usr/bin/env python3
"""Build a data-driven rename manifest for the old Kain C runtime prefixes."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


LOWER_DOMAIN_PREFIXES = (
    "actor_",
    "async_",
    "backend_",
    "bitfield_",
    "compatibility_",
    "contract_",
    "converge_",
    "cpu_",
    "device_",
    "diagnostics_",
    "display_",
    "entangle_",
    "host_bridge_",
    "machine_",
    "memory_",
    "ownership_",
    "platform_",
    "realtime_",
    "reflection_",
    "renderer_",
    "scene_",
    "services_",
    "union_",
    "version_",
    "viewport_",
    "win32_",
)

UPPER_DOMAIN_PREFIXES = tuple(prefix.upper() for prefix in LOWER_DOMAIN_PREFIXES)


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def parser() -> argparse.ArgumentParser:
    repo_root = repo_root_from_script()
    p = argparse.ArgumentParser(description="Generate runtime prefix rename manifest.")
    p.add_argument(
        "--inventory",
        default=str(repo_root / "runtime" / "native" / "kain_prefixed_symbol_inventory.json"),
    )
    p.add_argument(
        "--out",
        default=str(repo_root / "runtime" / "native" / "kain_prefix_rename_manifest.json"),
    )
    p.add_argument("--repo-root", default=str(repo_root))
    return p


def clean_lower(old: str) -> str:
    if old.startswith("kain_native_"):
        return "abi_" + old[len("kain_native_") :]
    if old.startswith("kain_runtime_native_stdlib_"):
        return "stdlib_abi_" + old[len("kain_runtime_native_stdlib_") :]
    if old == "kain_runtime_native_stdlib":
        return "stdlib_abi"
    if old.startswith("kain_runtime_graphics_"):
        return "graphics_bundle_" + old[len("kain_runtime_graphics_") :]
    if old.startswith("kain_runtime_"):
        tail = old[len("kain_runtime_") :]
        if tail.startswith(LOWER_DOMAIN_PREFIXES):
            return tail
        return "runtime_" + tail
    return old


def clean_upper(old: str) -> str:
    if old.startswith("KAIN_NATIVE_"):
        return "ABI_" + old[len("KAIN_NATIVE_") :]
    if old.startswith("KAIN_RUNTIME_NATIVE_STDLIB_"):
        return "STDLIB_ABI_" + old[len("KAIN_RUNTIME_NATIVE_STDLIB_") :]
    if old == "KAIN_RUNTIME_NATIVE_STDLIB":
        return "STDLIB_ABI"
    if old.startswith("KAIN_RUNTIME_GRAPHICS_"):
        return "GRAPHICS_BUNDLE_" + old[len("KAIN_RUNTIME_GRAPHICS_") :]
    if old.startswith("KAIN_RUNTIME_"):
        tail = old[len("KAIN_RUNTIME_") :]
        if tail.startswith(UPPER_DOMAIN_PREFIXES):
            return tail
        return "RUNTIME_" + tail
    return old


def clean_identifier(old: str) -> str:
    if old.startswith("KAIN_"):
        return clean_upper(old)
    return clean_lower(old)


def clean_file_stem(stem: str) -> str:
    special = {
        "kain_runtime_native_stdlib": "stdlib_abi",
        "kain_runtime_graphics": "graphics_bundle",
        "kain_runtime_ui": "ui_bundle",
        "kain_native_graphics_system": "graphics_system",
        "kain_native_input_system": "input_system",
        "kain_native_net_system": "net_system",
        "kain_native_process_system": "process_system",
        "kain_native_ui_system": "ui_system",
        "kain_native_ui_system_internal": "ui_system_internal",
        "kain_native_ui_host_adapter": "ui_host_adapter",
        "kain_ui_compiled_bundle": "ui_compiled_bundle",
        "kain_ui_hot_reload": "ui_hot_reload",
        "kain_ui_runtime": "ui_runtime",
        "kain_runtime_core": "core",
        "kain_runtime_linux_shared": "linux_shared",
        "kain_runtime_win32_shared": "win32_shared",
        "kain_runtime": "runtime",
    }
    if stem in special:
        return special[stem]
    if stem.startswith("kain_runtime_"):
        return stem[len("kain_runtime_") :]
    if stem.startswith("kain_native_"):
        return stem[len("kain_native_") :]
    if stem.startswith("kain_ui_"):
        return stem[len("kain_") :]
    return stem


def collect_file_renames(repo_root: Path) -> list[dict[str, str]]:
    candidate_roots = [
        repo_root / "runtime" / "native" / "include",
        repo_root / "runtime" / "native" / "src",
        repo_root / "runtime" / "native" / "tests",
        repo_root / "runtime" / "native" / "src" / "core" / "z3" / "assumptions",
        repo_root / "runtime" / "native" / "src" / "ui" / "z3" / "assumptions",
    ]
    files: list[Path] = [repo_root / "runtime" / "kain_runtime.c"]
    for root in candidate_roots:
        if root.exists():
            files.extend(path for path in root.rglob("*") if path.is_file())

    renames = []
    for path in sorted(set(files)):
        stem = path.stem
        if not (
            stem.startswith("kain_runtime")
            or stem.startswith("kain_native")
            or stem.startswith("kain_ui")
        ):
            continue
        new_stem = clean_file_stem(stem)
        if new_stem == stem:
            continue
        new_path = path.with_name(new_stem + path.suffix)
        renames.append(
            {
                "old_path": path.relative_to(repo_root).as_posix(),
                "new_path": new_path.relative_to(repo_root).as_posix(),
            }
        )
    return renames


def build_manifest(inventory: dict, repo_root: Path) -> dict:
    identifier_renames = []
    for old, entry in inventory["symbols"].items():
        new = clean_identifier(old)
        if new == old:
            continue
        identifier_renames.append(
            {
                "old": old,
                "new": new,
                "prefix_group": entry.get("prefix_group", ""),
                "occurrence_count": entry.get("occurrence_count", 0),
                "files": entry.get("files", []),
            }
        )

    collision_buckets: dict[str, list[str]] = defaultdict(list)
    for rename in identifier_renames:
        collision_buckets[rename["new"]].append(rename["old"])
    collisions = {
        new: sorted(old_names)
        for new, old_names in sorted(collision_buckets.items())
        if len(old_names) > 1
    }

    file_renames = collect_file_renames(repo_root)
    file_collision_buckets: dict[str, list[str]] = defaultdict(list)
    for rename in file_renames:
        file_collision_buckets[rename["new_path"]].append(rename["old_path"])
    file_collisions = {
        new: sorted(old_paths)
        for new, old_paths in sorted(file_collision_buckets.items())
        if len(old_paths) > 1
    }

    return {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "source_inventory": str(Path(inventory.get("root", "")).as_posix()),
        "policy": {
            "kain_native_lowercase": "abi_<tail>",
            "kain_runtime_lowercase": "domain tail when domain-qualified; otherwise runtime_<tail>",
            "kain_native_uppercase": "ABI_<tail>",
            "kain_runtime_uppercase": "domain tail when domain-qualified; otherwise RUNTIME_<tail>",
            "file_names": "drop kain_native/kain_runtime/kain_ui prefixes with collision-safe specials",
        },
        "identifier_rename_count": len(identifier_renames),
        "file_rename_count": len(file_renames),
        "identifier_collisions": collisions,
        "file_collisions": file_collisions,
        "identifier_renames": sorted(identifier_renames, key=lambda item: item["old"]),
        "file_renames": sorted(file_renames, key=lambda item: item["old_path"]),
    }


def main() -> int:
    args = parser().parse_args()
    repo_root = Path(args.repo_root).resolve()
    inventory = json.loads(Path(args.inventory).read_text(encoding="utf-8"))
    manifest = build_manifest(inventory, repo_root)
    out = Path(args.out).resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"identifier_renames={manifest['identifier_rename_count']}")
    print(f"file_renames={manifest['file_rename_count']}")
    print(f"identifier_collisions={len(manifest['identifier_collisions'])}")
    print(f"file_collisions={len(manifest['file_collisions'])}")
    print(f"manifest={out}")
    if manifest["identifier_collisions"] or manifest["file_collisions"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
