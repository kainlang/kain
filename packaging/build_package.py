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
import stat
import subprocess
import sys
import zipfile
import tarfile
from pathlib import Path


def _rmtree_onexc(func, path, exc):
    """Retry rmtree failures caused by read-only files on Windows."""
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception:
        pass


def rmtree(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path, onexc=_rmtree_onexc)


def copy_file(src: Path, dst: Path) -> None:
    """copy2 that tolerates a read-only destination (idempotent re-runs)."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        try:
            os.chmod(dst, stat.S_IWRITE)
        except OSError:
            pass
    shutil.copy2(src, dst)
    try:
        os.chmod(dst, stat.S_IWRITE)
    except OSError:
        pass

REPO_ROOT = Path(__file__).resolve().parent.parent
SYSTEM = platform.system()
if SYSTEM == "Windows":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


# ── Version ────────────────────────────────────────────────────────────────
VERSION_FILE = REPO_ROOT / "packaging" / "VERSION"


def get_version():
    """Resolve the version to package.

    Order of precedence:
      1. packaging/VERSION  — the canonical, reviewed release version
      2. nearest git tag    — fallback for untracked checkouts
      3. 0.0.0              — never silently reuse a stale release number
    """
    if VERSION_FILE.exists():
        value = VERSION_FILE.read_text(encoding="utf-8").strip()
        if value:
            return value.lstrip("v")
    try:
        tag = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            capture_output=True, text=True, cwd=REPO_ROOT
        ).stdout.strip()
        if tag:
            return tag.lstrip("v")
    except Exception:
        pass
    return "0.0.0"


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
    """Locate the native C runtime static library.

    The sync layer installs it under ``.kain/lib/<target-triple>/``; older
    layouts kept a flat copy at ``.kain/lib/kain_runtime.lib``.  Prefer the
    triple-scoped artifact, then fall back to the flat copy.
    """
    candidates = []
    lib_root = REPO_ROOT / ".kain" / "lib"
    if lib_root.is_dir():
        # Newest triple-scoped library first.
        triples = sorted(
            (p for p in lib_root.iterdir() if p.is_dir()),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )
        for triple in triples:
            for name in ("kain_runtime.lib", "libkain_runtime.a"):
                candidates.append(triple / name)
        for name in ("kain_runtime.lib", "libkain_runtime.a"):
            candidates.append(lib_root / name)
    for c in candidates:
        if c.exists():
            return c
    return None


def find_stdlib():
    s = REPO_ROOT / "stdlib"
    return s if s.exists() else None


def llvm_bin_dirs():
    """Candidate directories that may hold a clang.exe + friends."""
    dirs = []
    for env_key in ("LIBCLANG_PATH", "LLVM_PATH", "LLVM_BIN"):
        raw = os.environ.get(env_key)
        if not raw:
            continue
        p = Path(raw)
        dirs.append(p)
        dirs.append(p / "bin")
    dirs.extend([
        Path("C:/scoop/apps/llvm/current/bin"),
        Path("C:/Program Files/LLVM/bin"),
        Path("C:/Program Files (x86)/LLVM/bin"),
    ])
    out = []
    seen = set()
    for d in dirs:
        try:
            key = str(d).lower()
        except Exception:
            continue
        if key in seen:
            continue
        seen.add(key)
        if (d / "clang.exe").exists():
            out.append(d)
    return out


# ── Bundle DLLs ────────────────────────────────────────────────────────────
def bundle_dlls(stage_bin: Path):
    if SYSTEM != "Windows":
        return
    kain_bin = REPO_ROOT / ".kain" / "bin"
    scoop_python = Path("C:/scoop/apps/python312/current")
    local_appdata = Path(os.environ.get("LOCALAPPDATA", "")) if os.environ.get("LOCALAPPDATA") else None
    explicit_py_dll = Path(os.environ["PYTHON312_DLL"]) if os.environ.get("PYTHON312_DLL") else None

    py_dll_dirs = [kain_bin, scoop_python]
    if local_appdata:
        py_dll_dirs.append(local_appdata / "Programs" / "Python" / "Python312")
    if explicit_py_dll:
        py_dll_dirs.insert(0, explicit_py_dll.parent)

    llvm_dirs = llvm_bin_dirs()

    def unique(paths):
        out, seen = [], set()
        for p in paths:
            if p is None:
                continue
            key = str(p).lower()
            if key in seen:
                continue
            seen.add(key)
            out.append(p)
        return out

    candidates = {
        "python312.dll": unique(d / "python312.dll" for d in py_dll_dirs),
        "python3.dll": unique(d / "python3.dll" for d in py_dll_dirs),
        "libclang.dll": unique(list(d / "libclang.dll" for d in llvm_dirs) + [kain_bin / "libclang.dll"]),
        "vcruntime140.dll": unique([kain_bin / "vcruntime140.dll", Path("C:/Windows/System32/vcruntime140.dll")]),
        "vcruntime140_1.dll": unique([kain_bin / "vcruntime140_1.dll", Path("C:/Windows/System32/vcruntime140_1.dll")]),
    }
    for dll, paths in candidates.items():
        copied = False
        for p in paths:
            if not p.exists() or not p.is_file():
                continue
            try:
                copy_file(p, stage_bin / dll)
                print(f"  v bundled {dll} ({p})")
                copied = True
                break
            except (PermissionError, OSError) as err:
                print(f"  ! could not copy {dll} from {p}: {err}")
                continue
        if not copied:
            print(f"  x {dll} not found")


# ── Bundle LLVM toolchain ─────────────────────────────────────────────────
def bundle_llvm_toolchain(stage_bin: Path):
    if SYSTEM != "Windows":
        return
    dirs = llvm_bin_dirs()
    if not dirs:
        print("  x LLVM not found, skipping")
        return
    llvm_bin = dirs[0]
    tc_dir = stage_bin.parent / "toolchain" / "llvm" / "bin"
    tc_dir.mkdir(parents=True, exist_ok=True)
    essentials = [
        "clang.exe", "clang++.exe", "clang-cl.exe",
        "lld-link.exe", "ld.lld.exe", "wasm-ld.exe",
        "llvm-ar.exe", "llvm-lib.exe", "llvm-profdata.exe",
        "llvm-objcopy.exe", "llvm-objdump.exe", "llvm-symbolizer.exe",
        "llvm-mt.exe", "llvm-rc.exe", "llc.exe", "llvm-dlltool.exe",
        "llvm-cov.exe", "llvm-ml.exe", "llvm-ranlib.exe",
    ]
    copied = 0
    missing = []
    for exe in essentials:
        src = llvm_bin / exe
        if src.exists():
            copy_file(src, tc_dir / exe)
            copied += 1
        else:
            missing.append(exe)
    # libclang.dll is loaded by kain.exe itself; Windows resolves DLLs from the
    # executable's own directory first, so keep a copy beside the binary.
    libclang = llvm_bin / "libclang.dll"
    if libclang.exists():
        copy_file(libclang, stage_bin / "libclang.dll")
    # NOTE: the compiler discovers clang/lld/wasm-ld via KAIN_HOME's
    # toolchain/llvm/bin (install_layout::CLANG_CANDIDATE_SUFFIXES), so we do
    # NOT duplicate the ~250 MB of driver binaries into bin/.
    print(f"  v bundled LLVM toolchain from {llvm_bin} ({copied} tools)")
    if missing:
        print(f"  ! missing (non-fatal): {', '.join(missing)}")


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
        copy_file(kain_exe, sdir / "bin" / "kain.exe")
        print(f"  v kain.exe ({kain_exe.stat().st_size / 1024 / 1024:.1f} MB)")
    else:
        print("  x kain.exe not found")
        sys.exit(1)

    # 1b. kn alias — the CLI dispatches on argv[0], so a copy is sufficient.
    if kain_exe:
        kn_src = kain_exe.parent / "kn.exe"
        copy_file(kn_src if kn_src.exists() else kain_exe, sdir / "bin" / "kn.exe")
        print("  v kn.exe (alias)")

    # 2. Runtime
    rlib = find_runtime_lib()
    if rlib:
        copy_file(rlib, sdir / "lib" / "kain_runtime.lib")
        print(f"  v kain_runtime.lib ({rlib.stat().st_size / 1024:.0f} KB)")
    else:
        print("  x kain_runtime.lib not found")

    # 3. Stdlib
    stdlib = find_stdlib()
    if stdlib:
        for item in stdlib.iterdir():
            if item.is_file() and item.suffix == ".kn":
                copy_file(item, sdir / "stdlib" / item.name)
            elif item.is_dir():
                shutil.copytree(item, sdir / "stdlib" / item.name, dirs_exist_ok=True)
        print(f"  v stdlib/ ({sum(1 for _ in stdlib.rglob('*.kn'))} .kn files)")
    else:
        print("  x stdlib/ not found")
        sys.exit(1)

    # 3b. Native runtime manifest + C sources.
    # The compiler resolves `runtime/native_core_runtime.toml` relative to the
    # install home and hashes every source it declares as a build input, so a
    # distribution without this tree cannot link a user program. Shipping only
    # the prebuilt lib/kain_runtime.lib is NOT sufficient.
    runtime_src = REPO_ROOT / "runtime"
    if not runtime_src.is_dir():
        print("  x runtime/ not found")
        sys.exit(1)
    dest_runtime = sdir / "runtime"
    dest_runtime.mkdir(parents=True, exist_ok=True)
    for name in ("native_core_runtime.toml", "native_runtime.toml", "runtime.c"):
        src = runtime_src / name
        if src.exists():
            copy_file(src, dest_runtime / name)
    native = runtime_src / "native"
    if native.is_dir():
        shutil.copytree(
            native,
            dest_runtime / "native",
            ignore=shutil.ignore_patterns("test", "*.o", "*.obj", "*.a", "*.lib"),
            dirs_exist_ok=True,
        )
    print(
        "  v runtime/ (manifest + "
        f"{sum(1 for _ in (dest_runtime / 'native').rglob('*') if _.is_file())} native files)"
    )

    # 4. DLLs
    bundle_dlls(sdir / "bin")

    # 5. LLVM toolchain
    bundle_llvm_toolchain(sdir / "bin")

    # 6. Setup scripts
    for f in ["setup.py", "setup.bat"]:
        src = REPO_ROOT / "packaging" / f
        if src.exists():
            copy_file(src, sdir / f)
            print(f"  v {f}")

    # 6b. Documentation shipped with the distribution
    for doc in ["docs/SETUP.md"]:
        src = REPO_ROOT / doc
        if src.exists():
            dest = sdir / doc
            dest.parent.mkdir(parents=True, exist_ok=True)
            copy_file(src, dest)
            print(f"  v {doc}")

    # 7. Config
    cfg = REPO_ROOT / ".kain" / "config.toml"
    if cfg.exists():
        copy_file(cfg, sdir / "config.toml")
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
    iscc = shutil.which("iscc")
    if iscc:
        iscc = Path(iscc)
    else:
        for p in [
            Path("C:/scoop/shims/iscc.exe"),
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

    # Tracked canonical script first; legacy gitignored location as fallback.
    iss = None
    for candidate in [
        REPO_ROOT / "packaging" / "kain.iss",
        REPO_ROOT / "packaging" / "windows" / "kain.iss",
    ]:
        if candidate.exists():
            iss = candidate
            break
    if iss is None:
        print("  x kain.iss not found")
        return None

    out = output_dir() / f"kain-installer-{version}-x64.exe"
    print(f"\n  Compiling Inno Setup installer from {iss}...")
    r = subprocess.run(
        [str(iscc), f"/dMyAppVersion={version}", f"/dSourceDir={sdir.resolve()}", str(iss),
         f"/O{output_dir()}", f"/Fkain-installer-{version}-x64"],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if r.returncode == 0 and out.exists():
        print(f"  v Built: {out.name} ({out.stat().st_size / 1024 / 1024:.1f} MB)")
        return out
    else:
        print(f"  x Inno Setup failed (exit {r.returncode})")
        for line in (r.stdout + "\n" + r.stderr).split("\n"):
            if "error" in line.lower() or "warning" in line.lower():
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
        rmtree(sdir)

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
