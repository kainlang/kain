#!/usr/bin/env python3
"""
Kain Distribution Packager
Builds portable .zip (Windows) and .tar.gz (Linux/macOS) distributions
from the Bazel-built compiler and runtime artifacts.

All output stays inside packaging/ — nothing at repo root.

Usage:
    python packaging/build_package.py
    python packaging/build_package.py --platform linux
    python packaging/build_package.py --version 0.9.0
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import zipfile
import tarfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SYSTEM = platform.system()

# Windows console needs UTF-8 for checkmarks
if SYSTEM == "Windows":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# ── Version ────────────────────────────────────────────────────────────────
def get_version():
    try:
        tag = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            capture_output=True, text=True, cwd=REPO_ROOT
        ).stdout.strip()
        if tag:
            return tag.lstrip("v")
    except Exception:
        pass
    return "0.8.0"


# ── Platform ───────────────────────────────────────────────────────────────
def platform_tag():
    arch = platform.machine().lower()
    if arch in ("amd64", "x86_64"):
        arch = "x64"
    sys_map = {"Windows": "windows", "Linux": "linux", "Darwin": "darwin"}
    return f"{sys_map.get(SYSTEM, 'unknown')}-{arch}"


def platform_name():
    return platform_tag().split("-")[0]  # windows, linux, darwin


# ── Output paths — everything under packaging/ ─────────────────────────────
def stage_dir(version: str):
    return REPO_ROOT / "packaging" / "stage" / f"kain-{version}-{platform_tag()}"


def output_dir():
    return REPO_ROOT / "packaging" / platform_name()


# ── Find built artifacts ──────────────────────────────────────────────────
def find_kain_exe():
    # .kain/bin first (fastest)
    fallback = REPO_ROOT / ".kain" / "bin" / "kain.exe"
    if fallback.exists():
        return fallback
    # Known Bazel output paths
    try:
        result = subprocess.run(
            ["bazel", "info", "output_base", "--config=dev"],
            capture_output=True, text=True, cwd=REPO_ROOT
        )
        output_base = Path(result.stdout.strip())
        for candidate in [
            output_base / "execroot/_main/bazel-out/x64_windows-opt/bin/crates/cli/kain.exe",
            output_base / "execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe",
        ]:
            if candidate.exists():
                return candidate
    except Exception:
        pass
    return None


def find_runtime_lib():
    lib = REPO_ROOT / ".kain" / "lib" / "kain_runtime.lib"
    return lib if lib.exists() else None


def find_stdlib():
    stdlib = REPO_ROOT / "stdlib"
    return stdlib if stdlib.exists() else None


# ── Bundle DLLs (Windows) ─────────────────────────────────────────────────
def bundle_dlls(stage_bin: Path):
    if SYSTEM != "Windows":
        return
    candidates = {
        "python312.dll": [
            Path(os.environ.get("PYTHON312_DLL", "")),
            Path(os.environ["LOCALAPPDATA"]) / "Programs" / "Python" / "Python312" / "python312.dll",
            Path(os.environ["LOCALAPPDATA"]) / "Programs" / "Python" / "Python313" / "python312.dll",
            Path("C:/Users") / os.environ["USERNAME"] / "AppData/Local/Programs/Python/Python312/python312.dll",
        ],
        "libclang.dll": [
            Path(os.environ.get("LIBCLANG_PATH", "")) / "libclang.dll",
            Path("C:/Program Files/LLVM/bin/libclang.dll"),
            Path("C:/Program Files (x86)/LLVM/bin/libclang.dll"),
        ],
        "vcruntime140.dll": [Path("C:/Windows/System32/vcruntime140.dll")],
        "vcruntime140_1.dll": [Path("C:/Windows/System32/vcruntime140_1.dll")],
    }
    for dll, paths in candidates.items():
        copied = False
        for p in paths:
            if not p.exists():
                continue
            try:
                shutil.copy2(p, stage_bin / dll)
                print(f"  v bundled {dll}")
                copied = True
                break
            except (PermissionError, OSError):
                continue
        if not copied:
            print(f"  x {dll} not found")

# ── Bundle LLVM toolchain (clang, lld, etc.) ──────────────────────────────
def bundle_llvm_toolchain(stage_bin: Path):
    """Copy LLVM binaries needed for `kain build --target llvm`."""
    llvm_bin = None
    candidates = [
        Path(os.environ.get("LIBCLANG_PATH", "")),
        Path("C:/Program Files/LLVM/bin"),
        Path("C:/Program Files (x86)/LLVM/bin"),
    ]
    for p in candidates:
        if (p / "clang.exe").exists():
            llvm_bin = p
            break
    if not llvm_bin:
        print("  x LLVM toolchain not found, skipping")
        return

    # Create toolchain directory matching install_layout.rs expectations
    tc_dir = stage_bin.parent / "toolchain" / "llvm" / "bin"
    tc_dir.mkdir(parents=True, exist_ok=True)

    # Core compiler + linker tools
    essentials = [
        "clang.exe",
        "clang++.exe",
        "clang-cl.exe",
        "lld-link.exe",
        "ld.lld.exe",
        "wasm-ld.exe",
        "llvm-ar.exe",
        "llvm-lib.exe",
        "llvm-profdata.exe",
        "llvm-objcopy.exe",
        "llvm-objdump.exe",
        "llvm-symbolizer.exe",
        "llvm-mt.exe",
        "llvm-rc.exe",
        "llc.exe",
        "llvm-dlltool.exe",
        "llvm-cov.exe",
        "llvm-ml.exe",
    ]
    copied = 0
    for exe in essentials:
        src = llvm_bin / exe
        if src.exists():
            shutil.copy2(src, tc_dir / exe)
            copied += 1
    # Duplicate the multi-call binaries into bin/ so they're on PATH
    for exe in ["clang.exe", "lld-link.exe", "wasm-ld.exe"]:
        src = llvm_bin / exe
        if src.exists():
            shutil.copy2(src, stage_bin / exe)
    print(f"  v bundled LLVM toolchain ({copied} tools)")
def stage_package(version: str, stage_dir: Path):
    print(f"\n{'='*60}")
    print(f"  Kain Distribution Packager")
    print(f"  Version: {version}")
    print(f"  Platform: {platform_tag()}")
    print(f"  Stage: {stage_dir}")
    print(f"{'='*60}\n")

    for d in ["bin", "lib", "stdlib"]:
        (stage_dir / d).mkdir(parents=True, exist_ok=True)

    # 1. Compiler
    kain_exe = find_kain_exe()
    if kain_exe:
        dest = stage_dir / "bin" / "kain.exe"
        shutil.copy2(kain_exe, dest)
        print(f"  ✓ kain.exe ({dest.stat().st_size / 1024 / 1024:.1f} MB)")
    else:
        print("  ✗ kain.exe not found — run `bazel build //:kain` first")
        sys.exit(1)

    # 2. Runtime lib
    runtime_lib = find_runtime_lib()
    if runtime_lib:
        shutil.copy2(runtime_lib, stage_dir / "lib" / "kain_runtime.lib")
        print(f"  ✓ kain_runtime.lib ({runtime_lib.stat().st_size / 1024:.0f} KB)")
    else:
        print("  ⚠ kain_runtime.lib not found")

    # 3. Stdlib
    stdlib = find_stdlib()
    if stdlib:
        for item in stdlib.iterdir():
            if item.is_file() and item.suffix == ".kn":
                shutil.copy2(item, stage_dir / "stdlib" / item.name)
            elif item.is_dir():
                shutil.copytree(item, stage_dir / "stdlib" / item.name, dirs_exist_ok=True)
        print(f"  ✓ stdlib/ ({sum(1 for _ in stdlib.rglob('*.kn'))} .kn files)")
    else:
        print("  ✗ stdlib/ not found")
        sys.exit(1)

    # 4. DLLs
    if SYSTEM == "Windows":
        bundle_dlls(stage_dir / "bin")
    else:
        print("  ℹ skipping DLL bundling (non-Windows)")

    # 5. LLVM toolchain (clang, lld, etc.)
    if SYSTEM == "Windows":
        bundle_llvm_toolchain(stage_dir / "bin")
    else:
        print("  i skipping LLVM toolchain (non-Windows)")
    # 5. Setup script
    setup_src = REPO_ROOT / "packaging" / "setup.py"
    if setup_src.exists():
        shutil.copy2(setup_src, stage_dir / "setup.py")
        print("  ✓ setup.py")

    # 6. Config
    cfg_src = REPO_ROOT / ".kain" / "config.toml"
    if cfg_src.exists():
        shutil.copy2(cfg_src, stage_dir / "config.toml")
        print("  ✓ config.toml")

    # 7. Manifest
    manifest = {
        "version": version,
        "platform": platform_tag(),
        "git_commit": subprocess.run(
            ["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=REPO_ROOT
        ).stdout.strip(),
        "files": {
            "bin": [p.name for p in (stage_dir / "bin").iterdir() if p.is_file()],
            "lib": [p.name for p in (stage_dir / "lib").iterdir() if p.is_file()],
        }
    }
    with open(stage_dir / "install_manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)
    print("  ✓ install_manifest.json")

    # 8. License files
    license_locations = [REPO_ROOT / "packaging" / "windows" / "assets", REPO_ROOT]
    for lic in ["LICENSE.txt", "LICENSE.python.txt", "LICENSE.llvm.txt"]:
        src = None
        for base in license_locations:
            candidate = base / lic
            if candidate.exists():
                src = candidate
                break
        if src:
            shutil.copy2(src, stage_dir / lic)
            print(f"  ✓ {lic}")

    print(f"\n  ✅ Stage complete: {stage_dir}")


# ── Archives ───────────────────────────────────────────────────────────────
def create_zip(stage_dir: Path, output: Path):
    print(f"\n  Creating {output.name}...")
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for file in stage_dir.rglob("*"):
            if file.is_file():
                arcname = str(file.relative_to(stage_dir.parent))
                zf.write(file, arcname)
    print(f"  ✅ {output.name} ({output.stat().st_size / 1024 / 1024:.1f} MB)")


def create_tarball(stage_dir: Path, output: Path):
    print(f"\n  Creating {output.name}...")
    with tarfile.open(output, "w:gz") as tf:
        for file in stage_dir.rglob("*"):
            if file.is_file():
                arcname = str(file.relative_to(stage_dir.parent))
                tf.add(file, arcname)
    print(f"  ✅ {output.name} ({output.stat().st_size / 1024 / 1024:.1f} MB)")


# ── CLI ────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Build Kain distribution package")
    parser.add_argument("--version", default=get_version(), help="Version string")
    parser.add_argument("--platform", choices=["windows", "linux", "darwin"], help="Override platform")
    parser.add_argument("--stage-only", action="store_true", help="Only stage files, don't archive")
    parser.add_argument("--clean", action="store_true", help="Clean stage directory first")
    args = parser.parse_args()

    global SYSTEM
    if args.platform:
        SYSTEM = {"windows": "Windows", "linux": "Linux", "darwin": "Darwin"}[args.platform]

    sdir = stage_dir(args.version)
    if args.clean and sdir.exists():
        shutil.rmtree(sdir)

    stage_package(args.version, sdir)

    if args.stage_only:
        return

    out_dir = output_dir()
    out_dir.mkdir(parents=True, exist_ok=True)
    if SYSTEM == "Windows":
        archive = out_dir / f"kain-{args.version}-{platform_tag()}.zip"
        create_zip(sdir, archive)
    else:
        archive = out_dir / f"kain-{args.version}-{platform_tag()}.tar.gz"
        create_tarball(sdir, archive)

    print(f"\n{'='*60}")
    print(f"  ✅ Distribution package ready: {archive}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
