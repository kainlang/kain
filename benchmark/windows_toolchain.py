from __future__ import annotations

import os
from pathlib import Path


def split_env_paths(name: str) -> list[Path]:
    raw = os.environ.get(name)
    if not raw:
        return []
    return [Path(part) for part in raw.split(os.pathsep) if part]


def push_existing_unique_path(paths: list[Path], candidate: Path) -> None:
    if candidate.is_dir() and candidate not in paths:
        paths.append(candidate)


def discover_latest_child_dir(root: Path) -> Path | None:
    if not root.is_dir():
        return None
    children = [entry for entry in root.iterdir() if entry.is_dir()]
    if not children:
        return None
    children.sort(key=lambda path: path.name, reverse=True)
    return children[0]


def append_visual_studio_msvc_lib_dirs(paths: list[Path]) -> None:
    roots = [
        Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022"),
    ]
    editions = ["BuildTools", "Community", "Professional", "Enterprise", "Preview"]

    for root in roots:
        if not root.is_dir():
            continue
        for edition in editions:
            tools_root = root / edition / "VC" / "Tools" / "MSVC"
            version_dir = discover_latest_child_dir(tools_root)
            if version_dir is not None:
                push_existing_unique_path(paths, version_dir / "lib" / "x64")


def append_windows_sdk_lib_dirs(
    paths: list[Path], sdk_root: Path, explicit_version: str | None = None
) -> None:
    lib_root = sdk_root / "Lib"
    if not lib_root.is_dir():
        return

    if explicit_version:
        version_root = lib_root / explicit_version
        if version_root.is_dir():
            push_existing_unique_path(paths, version_root / "ucrt" / "x64")
            push_existing_unique_path(paths, version_root / "um" / "x64")
            return

    version_root = discover_latest_child_dir(lib_root)
    if version_root is not None:
        push_existing_unique_path(paths, version_root / "ucrt" / "x64")
        push_existing_unique_path(paths, version_root / "um" / "x64")


def append_windows_kits_lib_dirs(paths: list[Path]) -> None:
    for root in (
        Path(r"C:\Program Files (x86)\Windows Kits\10"),
        Path(r"C:\Program Files\Windows Kits\10"),
    ):
        append_windows_sdk_lib_dirs(paths, root)


def resolve_windows_msvc_link_search_paths() -> list[Path]:
    paths: list[Path] = []

    vc_tools_dir = os.environ.get("VCToolsInstallDir")
    if vc_tools_dir:
        push_existing_unique_path(paths, Path(vc_tools_dir) / "lib" / "x64")

    windows_sdk_dir = os.environ.get("WindowsSdkDir")
    if windows_sdk_dir:
        windows_sdk_version = os.environ.get("WindowsSDKLibVersion", "").strip()
        if windows_sdk_version:
            windows_sdk_version = windows_sdk_version.rstrip("\\/")
        append_windows_sdk_lib_dirs(
            paths,
            Path(windows_sdk_dir),
            windows_sdk_version or None,
        )

    append_visual_studio_msvc_lib_dirs(paths)
    append_windows_kits_lib_dirs(paths)
    return paths


def windows_msvc_link_env_overrides() -> dict[str, str]:
    if os.name != "nt":
        return {}

    search_paths = resolve_windows_msvc_link_search_paths()
    for existing in split_env_paths("LIB"):
        push_existing_unique_path(search_paths, existing)

    if not search_paths:
        return {}

    return {"LIB": os.pathsep.join(str(path) for path in search_paths)}
