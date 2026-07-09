#!/usr/bin/env python3
"""
Kain Distribution Packager
Builds portable .zip (Windows), .tar.gz (Linux/macOS), and .exe installer (Windows)
from Bazel-built compiler and runtime artifacts.

All output stays inside packaging/.
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
    m = {"Windows": "windows", "Linux": "linux", "Darwin": "darwin"}
    return f"{m.get(SYSTEM, 'unknown')}-{arch}"


def platform_name():
    return platform_tag().split("-")[0]


def stage_dir(version: str):
    return REPO_ROOT / "packaging" / "stage" / f"kain-{version}-{platform_tag()}"


def output_dir():
    return REPO_ROOT / "packaging" / platform_name()


# ── Find artifacts ─────────────────────────────────────────────────────────
def find_kain_exe():
    fb = REPO_ROOT / ".kain" / "bin" / "kain.exe"
    if fb.exists():
        return fb
    try:
        r = subprocess.run(["bazel", "info", "output_base", "--config=dev"],
                           capture_output=True, text=True, cwd=REPO_ROOT)
        ob = Path(r.stdout.strip())
        for c in [ob / "execroot/_main/bazel-out/x64_windows-opt/bin/crates/cli/kain.exe",
                  ob / "execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe"]:
            if c.exists():
                return c
    except Exception:
        pass
    return None


def find_runtime_lib():
    lib = REPO_ROOT / ".kain" / "lib" / "kain_runtime.lib"
    return lib if lib.exists() else None


def find_stdlib():
    s = REPO_ROOT / "stdlib"
    return s if s.exists() else None


# ── Bundle DLLs ────────────────────────────────────────────────────────────
def bundle_dlls(stage_bin: Path):
    if SYSTEM != "Windows":
        return
    candidates = {
        "python312.dll": [
            Path(os.environ.get("PYTHON312_DLL", "")),
            Path(os.environ["LOCALAPPDATA"]) / "Programs" / "Python" / "Python312" / "python312.dll",
            Path("C:/Users") / os.environ["USERNAME"] / "AppData/Local/Programs/Python/Python312/python312.dll",
        ],
        "libclang.dll": [
            Path(os.environ.get("LIBCLANG_PATH", "")) / "libclang.dll",
            Path("C:/Program Files/LLVM/bin/libclang.dll"),
        ],
        "vcruntime140.dll": [Path("C:/Windows/System32/vcruntime140.dll")],
        "vcruntime140_1.dll": [Path("C:/Windows/System32/vcruntime140_1.dll")],
    }
    for dll, paths in candidates.items():
        for p in paths:
            if not p.exists():
                continue
            try:
                shutil.copy2(p, stage_bin / dll)
                print(f"  v bundled {dll}")
                break
            except (PermissionError, OSError):
                continue
        else:
            print(f"  x {dll} not found")


# ── Bundle LLVM toolchain ─────────────────────────────────────────────────
def bundle_llvm_toolchain(stage_bin: Path):
    if SYSTEM != "Windows":
        return
    llvm_bin = None
    for p in [Path(os.environ.get("LIBCLANG_PATH", "")),
              Path("C:/Program Files/LLVM/bin"),
              Path("C:/Program Files (x86)/LLVM/bin")]:
        if (p / "clang.exe").exists():
            llvm_bin = p
            break
    if not llvm_bin:
        print("  x LLVM not found, skipping")
        return
    tc_dir = stage_bin.parent / "toolchain" / "llvm" / "bin"
    tc_dir.mkdir(parents=True, exist_ok=True)
    essentials = [
        "clang.exe", "clang++.exe", "clang-cl.exe",
        "lld-link.exe", "ld.lld.exe", "wasm-ld.exe",
        "llvm-ar.exe", "llvm-lib.exe", "llvm-profdata.exe",
        "llvm-objcopy.exe", "llvm-objdump.exe", "llvm-symbolizer.exe",
        "llvm-mt.exe", "llvm-rc.exe", "llc.exe", "llvm-dlltool.exe",
        "llvm-cov.exe", "llvm-ml.exe",
    ]
    copied = 0
    for exe in essentials:
        src = llvm_bin / exe
        if src.exists():
            shutil.copy2(src, tc_dir / exe)
            copied += 1
    # Also copy key drivers to bin/ for PATH
    for exe in ["clang.exe", "lld-link.exe", "wasm-ld.exe"]:
        src = llvm_bin / exe
        if src.exists():
            shutil.copy2(src, stage_bin / exe)
    print(f"  v bundled LLVM toolchain ({copied} tools)")


# ── Stage files ────────────────────────────────────────────────────────────
def stage_package(version: str, sdir: Path):
    print(f"\n{'='*60}")
    print(f"  Kain Distribution Packager")
    print(f"  Version: {version}")
    print(f"  Platform: {platform_tag()}")
    print(f"  Stage: {sdir}")
    print(f"{'='*60}\n")

    for d in ["bin", "lib", "stdlib"]:
        (sdir / d).mkdir(parents=True, exist_ok=True)

    # 1. Compiler
    kain_exe = find_kain_exe()
    if kain_exe:
        shutil.copy2(kain_exe, sdir / "bin" / "kain.exe")
        print(f"  v kain.exe ({kain_exe.stat().st_size / 1024 / 1024:.1f} MB)")
    else:
        print("  x kain.exe not found")
        sys.exit(1)

    # 2. Runtime
    rlib = find_runtime_lib()
    if rlib:
        shutil.copy2(rlib, sdir / "lib" / "kain_runtime.lib")
        print(f"  v kain_runtime.lib ({rlib.stat().st_size / 1024:.0f} KB)")
    else:
        print("  x kain_runtime.lib not found")

    # 3. Stdlib
    stdlib = find_stdlib()
    if stdlib:
        for item in stdlib.iterdir():
            if item.is_file() and item.suffix == ".kn":
                shutil.copy2(item, sdir / "stdlib" / item.name)
            elif item.is_dir():
                shutil.copytree(item, sdir / "stdlib" / item.name, dirs_exist_ok=True)
        print(f"  v stdlib/ ({sum(1 for _ in stdlib.rglob('*.kn'))} .kn files)")
    else:
        print("  x stdlib/ not found")
        sys.exit(1)

    # 4. DLLs
    bundle_dlls(sdir / "bin")

    # 5. LLVM toolchain
    bundle_llvm_toolchain(sdir / "bin")

    # 6. Setup scripts
    for f in ["setup.py", "setup.bat"]:
        src = REPO_ROOT / "packaging" / f
        if src.exists():
            shutil.copy2(src, sdir / f)
            print(f"  v {f}")

    # 7. Config
    cfg = REPO_ROOT / ".kain" / "config.toml"
    if cfg.exists():
        shutil.copy2(cfg, sdir / "config.toml")
        print(f"  v config.toml")

    # 8. Manifest
    mf = {
        "version": version,
        "platform": platform_tag(),
        "git_commit": subprocess.run(
            ["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=REPO_ROOT
        ).stdout.strip(),
    }
    with open(sdir / "install_manifest.json", "w") as f:
        json.dump(mf, f, indent=2)
    print("  v install_manifest.json")

    print(f"\n  v Stage complete: {sdir}")


# ── Create zip ─────────────────────────────────────────────────────────────
def create_zip(sdir: Path, output: Path):
    print(f"\n  Creating {output.name}...")
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for file in sdir.rglob("*"):
            if file.is_file():
                zf.write(file, str(file.relative_to(sdir.parent)))
    print(f"  v {output.name} ({output.stat().st_size / 1024 / 1024:.1f} MB)")


# ── Create tar.gz ──────────────────────────────────────────────────────────
def create_tarball(sdir: Path, output: Path):
    print(f"\n  Creating {output.name}...")
    with tarfile.open(output, "w:gz") as tf:
        for file in sdir.rglob("*"):
            if file.is_file():
                tf.add(file, str(file.relative_to(sdir.parent)))
    print(f"  v {output.name} ({output.stat().st_size / 1024 / 1024:.1f} MB)")


# ── Build Inno Setup installer ─────────────────────────────────────────────
def build_installer(version: str, sdir: Path):
    """Compile the Inno Setup installer from staged files."""
    iscc = None
    for p in [
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Inno Setup 6" / "iscc.exe",
        Path("C:/Program Files (x86)/Inno Setup 6/iscc.exe"),
        Path("C:/Program Files/Inno Setup 6/iscc.exe"),
    ]:
        if p.exists():
            iscc = p
            break
    if not iscc:
        print("  x Inno Setup not found — skipping installer")
        return None

    iss = REPO_ROOT / "packaging" / "windows" / "kain.iss"
    if not iss.exists():
        print(f"  x {iss} not found")
        return None

    out = output_dir() / f"kain-installer-{version}-x64.exe"
    print(f"\n  Compiling Inno Setup installer...")
    r = subprocess.run(
        [str(iscc), f"/dMyAppVersion={version}", str(iss),
         f"/O{output_dir()}", f"/Fkain-installer-{version}-x64"],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if r.returncode == 0 and out.exists():
        print(f"  v Built: {out.name} ({out.stat().st_size / 1024 / 1024:.1f} MB)")
        return out
    else:
        print(f"  x Inno Setup failed (exit {r.returncode})")
        for line in r.stdout.split("\n"):
            if "error" in line.lower():
                print(f"    {line.strip()}")
        return None


# ── CLI ────────────────────────────────────────────────────────────────────
def main():
    p = argparse.ArgumentParser(description="Build Kain distribution")
    p.add_argument("--version", default=get_version())
    p.add_argument("--platform", choices=["windows", "linux", "darwin"])
    p.add_argument("--stage-only", action="store_true")
    p.add_argument("--clean", action="store_true")
    args = p.parse_args()

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
        a = out_dir / f"kain-{args.version}-{platform_tag()}.zip"
        create_zip(sdir, a)
        print(f"\n  v Zip: {a}")
    else:
        a = out_dir / f"kain-{args.version}-{platform_tag()}.tar.gz"
        create_tarball(sdir, a)
        print(f"\n  v Tarball: {a}")

    if SYSTEM == "Windows":
        inst = build_installer(args.version, sdir)

    print(f"\n{'='*60}")
    print(f"  Done — artifacts in {output_dir()}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
