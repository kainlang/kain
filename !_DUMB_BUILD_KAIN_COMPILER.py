#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""!_DUMB_BUILD_KAIN_COMPILER.py -- THE OMEGA BUILD SCRIPT.
================================================================
Builds the Kain compiler on ANYTHING: Windows, Linux, macOS, WSL.

  Windows (normal) :  py !_DUMB_BUILD_KAIN_COMPILER.py --yes
  Windows (via WSL):  py !_DUMB_BUILD_KAIN_COMPILER.py --wsl --yes
  Linux / macOS    :  python3 !_DUMB_BUILD_KAIN_COMPILER.py --yes

WHY THE LEADING "!": ASCII "!" (0x21) sorts before "." (0x2E), so this
file sits ABOVE .bazelignore at the very top of the repo file listing
in Explorer / Finder / ls. That is intentional. This is the front door.

WHAT IT DOES (no Bazel, no cmake, no feature flags):
  1. Detects your OS (+ WSL distros on Windows, + "am I inside WSL").
  2. Checks for: cargo/rustc, clang, archiver (ar/llvm-ar/llvm-lib), git.
     Missing anything? With --yes it installs it via the native package
     manager (winget/choco/scoop, apt/dnf/pacman/apk/zypper, brew)
     or rustup. Without --yes it just tells you what to install.
  3. Builds the C runtime with clang directly (no make needed):
     runtime/native/src/core/*.c + platform file -> kain_runtime.lib/.a
  4. Builds the compiler at max speed, plain release (opt-level 3,
     non-debug, default features = everything, no gating):
        cargo build --release -p cli --jobs N
  5. Installs into --install-dir (default ~/.kain): bin/, stdlib/,
     runtime/, install_manifest.json
  6. Adds bin/ to your user PATH (unless --no-path).
  7. Verifies: kain --version + layout sanity.

Stdlib only. No third-party Python deps. ASCII output only (Windows
consoles mangle UTF-8, so no checkmarks/emoji here).
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
SCRIPT_NAME = Path(__file__).name

# ---------------------------------------------------------------- banner ---

BANNER = r"""
================================================================================
  !_DUMB_BUILD_KAIN_COMPILER.py  --  KAIN OMEGA BUILD  --  NO BAZEL. JUST WORKS.
================================================================================
"""


# ---------------------------------------------------------------- helpers ---

def log(msg: str) -> None:
    print(f"[build] {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"[warn]  {msg}", flush=True)


def err(msg: str) -> None:
    print(f"[ERROR] {msg}", file=sys.stderr, flush=True)


def die(msg: str, code: int = 1) -> None:
    err(msg)
    raise SystemExit(code)


def run(cmd: list[str], cwd: Path | None = None, env: dict | None = None,
        timeout: int | None = None) -> subprocess.CompletedProcess:
    """Run a command, no shell, no pipes. Returns the completed process."""
    return subprocess.run(cmd, cwd=str(cwd) if cwd else None,
                          env=env, capture_output=True, timeout=timeout)


def run_live(cmd: list[str], cwd: Path | None = None, env: dict | None = None) -> int:
    """Run a command with output streaming to the terminal. Returns exit code."""
    proc = subprocess.Popen([str(c) for c in cmd],
                            cwd=str(cwd) if cwd else None, env=env)
    try:
        return proc.wait()
    except KeyboardInterrupt:
        proc.kill()
        return 130


def decode_bytes(data: bytes) -> str:
    """Decode subprocess bytes. WSL on Windows emits UTF-16; handle that."""
    if b"\x00" in data:
        try:
            return data.decode("utf-16")
        except Exception:
            pass
    for enc in ("utf-8", "cp1252", "mbcs"):
        try:
            return data.decode(enc)
        except Exception:
            continue
    return data.decode("utf-8", errors="replace")


# ---------------------------------------------------------------- OS detect ---

def detect_os() -> dict:
    system = platform.system().lower()  # windows | linux | darwin | ...
    in_wsl = False
    if system == "linux":
        try:
            ver = Path("/proc/version").read_text(errors="replace").lower()
            if "microsoft" in ver or "wsl" in ver:
                in_wsl = True
        except OSError:
            pass
        if os.environ.get("WSL_DISTRO_NAME") or os.environ.get("WSL_INTEROP"):
            in_wsl = True
    return {
        "system": system,
        "is_windows": system == "windows",
        "is_linux": system == "linux",
        "is_macos": system == "darwin",
        "in_wsl": in_wsl,
        "machine": platform.machine(),
        "cpu_count": os.cpu_count() or 4,
    }


def wsl_distros() -> list[str]:
    """List installed WSL distros (Windows only). Empty list if no WSL."""
    wsl = shutil.which("wsl") or shutil.which("wsl.exe")
    if not wsl:
        return []
    try:
        proc = subprocess.run([wsl, "--list", "--quiet"],
                              capture_output=True, timeout=30)
        text = decode_bytes(proc.stdout)
    except Exception:
        return []
    distros = [ln.strip().strip("\x00").strip() for ln in text.splitlines()]
    return [d for d in distros if d and d.lower() != "docker-desktop"]


# ---------------------------------------------------------------- CLI ---

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog=SCRIPT_NAME,
        description="DUMB Kain compiler build: works on Windows/Linux/macOS/WSL. No Bazel.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "examples:\n"
            "  py !_DUMB_BUILD_KAIN_COMPILER.py --yes\n"
            "  py !_DUMB_BUILD_KAIN_COMPILER.py --wsl --yes\n"
            "  python3 !_DUMB_BUILD_KAIN_COMPILER.py --yes --jobs 8 --install-dir ~/kain\n"
            "  python3 !_DUMB_BUILD_KAIN_COMPILER.py --dry-run   (print plan, change nothing)\n"
        ),
    )
    p.add_argument("--yes", action="store_true",
                   help="Allow auto-install of missing tools (package managers / rustup).")
    p.add_argument("--install-deps", action="store_true",
                   help="Alias for --yes (install missing toolchain automatically).")
    p.add_argument("-j", "--jobs", default="auto",
                   help="Build parallelism: N or 'auto' (default: auto = all cores).")
    p.add_argument("--install-dir", type=Path, default=None,
                   help="Where to place the compiler (default: ~/.kain).")
    p.add_argument("--no-path", action="store_true",
                   help="Do NOT touch PATH. Default is to add <install>/bin to user PATH.")
    p.add_argument("--wsl", nargs="?", const="auto", default=None, metavar="DISTRO",
                   help="Windows only: redo this build inside WSL (optionally: --wsl Ubuntu).")
    p.add_argument("--wsl-setup", action="store_true",
                   help="Windows only: install Ubuntu WSL + inside-WSL toolchain, then stop.")
    p.add_argument("--skip-runtime", action="store_true", help="Skip the C runtime build.")
    p.add_argument("--skip-compiler", action="store_true", help="Skip cargo build.")
    p.add_argument("--skip-install", action="store_true", help="Skip install into --install-dir.")
    p.add_argument("--skip-verify", action="store_true", help="Skip final verification.")
    p.add_argument("--clean", action="store_true",
                   help="Wipe the install dir before installing.")
    p.add_argument("--dry-run", action="store_true",
                   help="Print the plan and exit. Changes nothing.")
    p.add_argument("--verbose", action="store_true", help="Show full tool versions and commands.")
    p.add_argument("--inside-wsl", action="store_true", help=argparse.SUPPRESS)  # set by --wsl re-exec
    return p.parse_args(argv)


def resolve_jobs(raw: str, cpu_count: int) -> int:
    if isinstance(raw, str) and raw.strip().lower() == "auto":
        return max(1, cpu_count)
    try:
        n = int(raw)
    except ValueError:
        die(f"--jobs must be a number or 'auto', got: {raw!r}")
    if n < 1:
        die("--jobs must be >= 1")
    return n


# ---------------------------------------------------------------- preflight ---

def probe_version(cmd: list[str]) -> str:
    try:
        proc = run(cmd, timeout=30)
        if proc.returncode != 0:
            return ""
        return decode_bytes(proc.stdout).splitlines()[0].strip() if proc.stdout else ""
    except Exception:
        return ""


def find_libclang_dir(clang_bin: str) -> str:
    """Best-effort LIBCLANG_PATH: look next to clang and via llvm-config."""
    clang_path = Path(clang_bin)
    bindir = clang_path.parent
    libdir = bindir.parent / "lib"
    names = ("libclang.dll", "libclang.so", "libclang.dylib")
    for d in (bindir, libdir):
        for n in names:
            if (d / n).exists():
                return str(d)
    for cfg in ("llvm-config", "llvm-config-21"):
        cfg_path = shutil.which(cfg)
        if cfg_path:
            try:
                proc = run([cfg_path, "--libdir"], timeout=15)
                if proc.returncode == 0:
                    d = decode_bytes(proc.stdout).strip()
                    for n in names:
                        if (Path(d) / n).exists():
                            return d
            except Exception:
                pass
    return ""


def find_windows_crt_includes() -> tuple[list[str], str]:
    """Locate MSVC + Windows SDK headers so MSVC-target clang finds string.h.
    Returns (include_dirs, note). Empty list = not found (caller falls back
    to MinGW-gnu target). Mirrors the repo's own MSVC discovery logic."""
    incs: list[str] = []
    note = ""
    # 1. vswhere (proper channel)
    vswhere = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
    vs_root = ""
    if vswhere.exists():
        try:
            proc = run([str(vswhere), "-latest", "-products", "*",
                        "-property", "installationPath"], timeout=30)
            for line in decode_bytes(proc.stdout).splitlines():
                cand = line.strip()
                if cand and Path(cand).exists():
                    vs_root = cand
                    break
        except Exception:
            pass
    # 2. filesystem fallback (vswhere misses BuildTools-only installs)
    if not vs_root:
        for base in (Path("C:/Program Files (x86)/Microsoft Visual Studio"),
                     Path("C:/Program Files/Microsoft Visual Studio")):
            if not base.exists():
                continue
            for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
                msvc = base / edition / "VC" / "Tools" / "MSVC"
                if msvc.exists():
                    vs_root = str(base / edition)
                    break
            if vs_root:
                break
    if vs_root:
        versions = sorted((Path(vs_root) / "VC" / "Tools" / "MSVC").glob("*"))
        versions = [v for v in versions if (v / "include").exists()]
        if versions:
            incs.append(str(versions[-1] / "include"))  # newest MSVC only
            note = f"msvc={versions[-1].name}"
    # 3. Windows SDK (ucrt + shared + um), newest kit wins
    kits = Path("C:/Program Files (x86)/Windows Kits/10/Include")
    if kits.exists():
        kitvers = sorted([d for d in kits.iterdir() if d.is_dir()],
                         key=lambda d: [int(x) if x.isdigit() else 0
                                        for x in d.name.split(".")])
        if kitvers:
            kit = kitvers[-1]
            for sub in ("ucrt", "shared", "um"):
                if (kit / sub).exists():
                    incs.append(str(kit / sub))
            note += f" sdk={kit.name}" if note else f"sdk={kit.name}"
    return incs, note.strip()


def find_mingw_gnu_flags() -> tuple[list[str], str]:
    """Fallback: MinGW-w64 headers + gnu target for clang."""
    gcc = shutil.which("gcc")
    if not gcc:
        return [], ""
    try:
        proc = run([gcc, "-print-sysroot"], timeout=15)
        sysroot = decode_bytes(proc.stdout).strip()
    except Exception:
        sysroot = ""
    cands = []
    if sysroot:
        cands.append(str(Path(sysroot) / "include"))
    mingw_bin = Path(gcc).parent
    for inc in sorted(mingw_bin.parent.glob("*/include")):
        cands.append(str(inc))
    cands = [c for c in cands if Path(c, "string.h").exists() or (Path(c) / "_mingw.h").exists()]
    if cands:
        return ["--target=x86_64-pc-windows-gnu", f"-I{cands[0]}"], f"mingw={cands[0]}"
    return [], ""


def preflight(osinfo: dict, verbose: bool) -> dict:
    """Check every tool. Returns status dict; prints a table."""
    cargo = shutil.which("cargo")
    rustc = shutil.which("rustc")
    clang = shutil.which("clang")
    ar = shutil.which("ar") or shutil.which("llvm-ar") or shutil.which("llvm-ar.exe")
    if osinfo["is_windows"] and not ar:
        ar = shutil.which("llvm-lib") or shutil.which("lib.exe")
    git = shutil.which("git")
    make = shutil.which("make") or shutil.which("nmake") or shutil.which("mingw32-make")

    cargo_ver = probe_version(["cargo", "--version"]) if cargo else ""
    rustc_ver = probe_version(["rustc", "--version"]) if rustc else ""
    clang_ver = probe_version(["clang", "--version"]) if clang else ""
    git_ver = probe_version(["git", "--version"]) if git else ""

    status = {
        "cargo": cargo or "", "cargo_ver": cargo_ver,
        "rustc": rustc or "", "rustc_ver": rustc_ver,
        "clang": clang or "", "clang_ver": clang_ver,
        "ar": ar or "",
        "git": git or "", "git_ver": git_ver,
        "make": make or "",
        "libclang_dir": find_libclang_dir(clang) if clang else "",
    }
    print("")
    print("  TOOL         STATUS")
    print("  -----------  --------------------------------------------------")
    print(f"  OS           {platform.system()} ({osinfo['machine']})"
          + ("  [inside WSL]" if osinfo["in_wsl"] else ""))
    print(f"  cargo        {(cargo_ver or 'MISSING')}")
    print(f"  rustc        {(rustc_ver or 'MISSING')}")
    print(f"  clang        {(clang_ver or 'MISSING')}")
    print(f"  archiver     {(ar or 'MISSING (need ar, llvm-ar, or llvm-lib)')}")
    print(f"  git          {(git_ver or 'MISSING')}")
    print(f"  LIBCLANG     {(status['libclang_dir'] or 'not found (clang-sys may still cope)')}")
    if verbose:
        print(f"  make         {(make or 'not present (fine: script compiles C directly)')}")
        print(f"  repo         {REPO_ROOT}")
        print(f"  cores        {osinfo['cpu_count']}")
    print("")
    # LLVM major warning: inkwell in this repo pins llvm21.
    if clang_ver and "21." not in clang_ver and "22." not in clang_ver and "20." not in clang_ver:
        warn(f"unusual clang version ({clang_ver}). Repo pins LLVM 21 (inkwell llvm21-1); "
             "build may still work if libclang is compatible, else install LLVM 21.")
    return status


def missing_tools(status: dict, osinfo: dict) -> list[str]:
    need = []
    if not status["cargo"] or not status["rustc"]:
        need.append("rustup (cargo/rustc)")
    if not status["clang"]:
        need.append("clang")
    if not status["ar"]:
        need.append("archiver (ar / llvm-ar / llvm-lib)")
    if not status["git"]:
        need.append("git")
    return need


# ---------------------------------------------------------------- ensure ---

WINDOWS_PM = [
    (["winget", "install", "--id", "LLVM.LLVM", "-e",
      "--accept-package-agreements", "--accept-source-agreements"], "winget"),
    (["choco", "install", "llvm", "-y"], "choco"),
    (["scoop", "install", "llvm"], "scoop"),
]
LINUX_PM = [
    (["apt-get", "install", "-y", "clang", "lld", "llvm", "binutils"], "apt-get"),
    (["dnf", "install", "-y", "clang", "lld", "llvm", "binutils"], "dnf"),
    (["pacman", "-Sy", "--noconfirm", "clang", "lld", "llvm", "binutils"], "pacman"),
    (["zypper", "install", "-y", "clang", "lld", "llvm", "binutils"], "zypper"),
    (["apk", "add", "clang", "llvm", "lld", "build-base", "binutils"], "apk"),
]
MACOS_PM = [
    (["brew", "install", "llvm"], "brew"),
]


def sudo_wrap(cmd: list[str], osinfo: dict) -> list[str]:
    if (osinfo["is_linux"] or osinfo["is_macos"]) and os.geteuid() != 0 and shutil.which("sudo"):
        return ["sudo"] + cmd
    return cmd


def try_installer(cmd: list[str], pm: str, osinfo: dict, dry: bool) -> bool:
    full = sudo_wrap(cmd, osinfo) if pm in ("apt-get", "dnf", "pacman", "zypper", "apk") else cmd
    log(f"installing via {pm}: {' '.join(full)}")
    if dry:
        return False
    try:
        return run_live(full) == 0
    except Exception as e:
        warn(f"{pm} failed: {e}")
        return False


def ensure_rustup(osinfo: dict, dry: bool) -> bool:
    """Install rustup (stable). Returns True if cargo now exists."""
    if shutil.which("cargo"):
        return True
    log("cargo missing -> installing rustup (stable toolchain)")
    if dry:
        return False
    if osinfo["is_windows"]:
        winget = shutil.which("winget")
        if winget:
            if run_live([winget, "install", "--id", "Rustlang.Rustup", "-e",
                         "--accept-package-agreements", "--accept-source-agreements"]) == 0:
                pass
        # fallback: rustup-init.exe direct download via powershell
        if not shutil.which("cargo"):
            home = Path.home()
            exe = home / "rustup-init.exe"
            dl = (f"powershell -NoProfile -Command \"Invoke-WebRequest "
                  f"-Uri https://win.rustup.rs/x86_64 -OutFile '{exe}'\"")
            log("fetching rustup-init.exe ...")
            if subprocess.call(dl, shell=True) == 0 and exe.exists():
                run_live([str(exe), "-y", "--default-toolchain", "stable", "--profile", "minimal"])
                try:
                    exe.unlink()
                except OSError:
                    pass
    else:
        if shutil.which("curl"):
            rc = subprocess.call("curl --proto '=https' --tlsv1.2 -sSf "
                                 "https://sh.rustup.rs | sh -s -- -y --default-toolchain stable "
                                 "--profile minimal", shell=True)
            if rc != 0:
                warn("rustup script failed")
        else:
            warn("no curl; install rustup from https://rustup.rs manually")
    # pick up ~/.cargo/bin in this process
    cargo_bin = str(Path.home() / ".cargo" / "bin")
    if Path(cargo_bin).exists() and cargo_bin not in os.environ.get("PATH", ""):
        os.environ["PATH"] = cargo_bin + os.pathsep + os.environ.get("PATH", "")
        log(f"added to session PATH: {cargo_bin}")
    return shutil.which("cargo") is not None


def ensure_toolchain(osinfo: dict, status: dict, allow_install: bool, dry: bool) -> dict:
    need = missing_tools(status, osinfo)
    if not need:
        log("toolchain complete: cargo, clang, archiver, git all present.")
        return status
    warn(f"missing: {', '.join(need)}")
    if not allow_install:
        die("Rerun with --yes to auto-install the missing tools (else install them by hand).")
    if dry:
        log("(dry-run) would install: " + ", ".join(need))
        return status
    if not status["cargo"] or not status["rustc"]:
        if not ensure_rustup(osinfo, dry):
            die("rustup install did not produce cargo. Install Rust by hand: https://rustup.rs")
    if not status["clang"] or not status["ar"]:
        pm_lists = WINDOWS_PM if osinfo["is_windows"] else (MACOS_PM if osinfo["is_macos"] else LINUX_PM)
        installed = False
        for cmd, pm in pm_lists:
            if not shutil.which(cmd[0]):
                continue
            if try_installer(cmd, pm, osinfo, dry):
                installed = True
                break
        if not installed:
            die("could not install clang automatically. Install LLVM 21 by hand, then rerun.")
    if not status["git"]:
        warn("git still missing; continuing (only needed for version stamping).")
    return preflight(osinfo, verbose=False)


# ---------------------------------------------------------------- runtime ---

RUNTIME_SRC = REPO_ROOT / "runtime" / "native" / "src"
RUNTIME_CORE = RUNTIME_SRC / "core"
RUNTIME_PLATFORM = RUNTIME_SRC / "platform"

# Mirror of runtime/native/Makefile exclusions (bench + optional backends).
SKIP_SUFFIXES = ("_benchmark.c",)
SKIP_PREFIXES = ("python_runtime", "cuda_runtime")


def collect_runtime_sources(osinfo: dict) -> list[Path]:
    srcs = []
    if RUNTIME_CORE.exists():
        for c in sorted(RUNTIME_CORE.glob("*.c")):
            if c.name.endswith(SKIP_SUFFIXES):
                continue
            if c.name.startswith(SKIP_PREFIXES):
                continue
            srcs.append(c)
    plat = RUNTIME_PLATFORM / "platform.c"
    if plat.exists():
        srcs.append(plat)
    if osinfo["is_windows"]:
        win = RUNTIME_PLATFORM / "win32" / "win32_shared.c"
        if win.exists():
            srcs.append(win)
    else:
        lin = RUNTIME_PLATFORM / "linux" / "linux_shared.c"
        if lin.exists():
            srcs.append(lin)
    return srcs


def build_runtime(osinfo: dict, status: dict, jobs: int, dry: bool, verbose: bool) -> Path:
    """Compile C runtime with clang directly (no make/cmake). Returns the .lib/.a path."""
    outdir = REPO_ROOT / "target" / "dumb" / "runtime"
    objdir = outdir / "obj"
    libname = "kain_runtime.lib" if osinfo["is_windows"] else "libkain_runtime.a"
    libpath = outdir / libname

    srcs = collect_runtime_sources(osinfo)
    if not srcs:
        die(f"no C sources found under {RUNTIME_SRC} -- repo checkout incomplete?")
    log(f"runtime: {len(srcs)} C files -> {libname}  ({jobs} jobs)")

    clang = status["clang"]
    inc_root = REPO_ROOT / "runtime" / "native"
    base_flags = ["-std=c11", "-O2",
                  f"-I{inc_root / 'include'}",
                  f"-I{inc_root / 'src' / 'core'}",
                  f"-I{inc_root / 'src' / 'platform'}", "-g"]
    if osinfo["is_windows"]:
        base_flags += ["-D_CRT_SECURE_NO_WARNINGS", "-D_CRT_NONSTDC_NO_WARNINGS"]
        # MSVC-target clang needs MSVC + SDK headers for <string.h> etc.
        crt_incs, crt_note = find_windows_crt_includes()
        if crt_incs:
            base_flags += [f"-I{d}" for d in crt_incs]
            log(f"runtime: windows CRT headers ({crt_note})")
        else:
            gnu_flags, gnu_note = find_mingw_gnu_flags()
            if gnu_flags:
                base_flags += gnu_flags
                warn(f"runtime: no MSVC headers found, using MinGW fallback ({gnu_note})")
            else:
                die("runtime: no MSVC headers AND no MinGW headers. Install VS Build Tools "
                    "(Desktop C++ workload) or MinGW-w64, then rerun.")
    else:
        # Linux/macOS: expose GNU extensions (CPU_ZERO, pthread affinity) + PIC
        # so the static lib links cleanly into PIE executables.
        base_flags += ["-D_GNU_SOURCE", "-fPIC", "-pthread"]

    if dry:
        log(f"(dry-run) would compile {len(srcs)} files with {clang} and archive to {libpath}")
        return libpath

    objdir.mkdir(parents=True, exist_ok=True)

    def compile_one(src: Path) -> Path:
        obj = objdir / (src.stem + (".obj" if osinfo["is_windows"] else ".o"))
        cmd = [clang, "-c", str(src)] + base_flags + ["-o", str(obj)]
        # (flags above already carry absolute -I paths; keep relative-safe:)
        proc = run(cmd, cwd=inc_root)
        if proc.returncode != 0:
            raise RuntimeError(f"clang failed on {src.name}:\n{decode_bytes(proc.stderr)[-3000:]}")
        return obj

    t0 = time.time()
    objs: list[Path] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futs = {pool.submit(compile_one, s): s for s in srcs}
        done = 0
        for fut in concurrent.futures.as_completed(futs):
            src = futs[fut]
            try:
                objs.append(fut.result())
            except Exception as e:
                die(f"runtime compile failed: {e}")
            done += 1
            if verbose or done % 10 == 0:
                log(f"  compiled {done}/{len(srcs)}")
    log(f"runtime objects done in {time.time() - t0:.1f}s")

    ar = status["ar"]
    ar_base = Path(ar).name.lower()
    if "llvm-lib" in ar_base or ar_base == "lib.exe":
        cmd = [ar, "/OUT:" + str(libpath)] + [str(o) for o in sorted(objs)]
    else:
        cmd = [ar, "rcs", str(libpath)] + [str(o) for o in sorted(objs)]
    if libpath.exists():
        libpath.unlink()
    proc = run(cmd, cwd=inc_root)
    if proc.returncode != 0:
        die(f"archive failed ({ar}):\n{decode_bytes(proc.stderr)[-3000:]}")
    size_kb = libpath.stat().st_size // 1024
    log(f"runtime OK: {libpath} ({size_kb} KB, {len(objs)} objects)")
    return libpath


# ---------------------------------------------------------------- compiler ---

def write_dumb_build_info() -> Path | None:
    """Stamp crates/cli/data/build_info.env as a dumb-cargo build.
    The committed file carries a stale bazel-dev stamp and build.rs prefers
    file > env > git, so without this overlay every dumb-built binary would
    report Bazel provenance. Returns backup path (or None) for restore."""
    info = REPO_ROOT / "crates" / "cli" / "data" / "build_info.env"
    backup = info.with_suffix(".env.dumb-bak")
    try:
        def git(args: list[str]) -> str:
            try:
                proc = run(["git"] + args, cwd=REPO_ROOT, timeout=30)
                return decode_bytes(proc.stdout).strip() if proc.returncode == 0 else ""
            except Exception:
                return ""
        sha = git(["rev-parse", "--short=12", "HEAD"]) or "unknown"
        count = git(["rev-list", "--count", "HEAD"]) or "0"
        dirty = "dirty" if git(["status", "--porcelain"]) else "clean"
        if info.exists() and not backup.exists():
            shutil.copy2(info, backup)
        info.parent.mkdir(parents=True, exist_ok=True)
        info.write_text(
            f"KAIN_BUILD_NUMBER=dumb-{sha}-{count}\n"
            f"KAIN_BUILD_TRACKING_MODE=dumb-cargo\n"
            f"KAIN_BUILD_GIT_SHA={sha}\n"
            f"KAIN_BUILD_GIT_COMMIT_COUNT={count}\n"
            f"KAIN_BUILD_GIT_DIRTY={dirty}\n"
            f"SOURCE_DATE_EPOCH={int(time.time())}\n",
            encoding="utf-8")
        log(f"build stamp: dumb-{sha}-{count} ({dirty})")
        return backup if backup.exists() else None
    except Exception as e:
        warn(f"build-info overlay failed ({e}); build continues with repo stamp.")
        return backup if backup.exists() else None


def restore_build_info(backup: Path | None) -> None:
    info = REPO_ROOT / "crates" / "cli" / "data" / "build_info.env"
    try:
        if backup and backup.exists():
            shutil.copy2(backup, info)
            backup.unlink()
            log("build stamp: repo build_info.env restored (no repo dirt).")
    except Exception as e:
        warn(f"could not restore build_info.env ({e}); check git status.")


def build_compiler(osinfo: dict, status: dict, jobs: int, dry: bool) -> Path:
    """cargo build --release -p cli. Default features = full compiler. No gating."""
    cmd = ["cargo", "build", "--release", "-p", "cli", "--jobs", str(jobs)]
    log(f"compiler: {' '.join(cmd)}   (opt-level 3, non-debug, all default features)")
    if dry:
        log("(dry-run) would run cargo build now")
        return REPO_ROOT / "target" / "release" / ("kain.exe" if osinfo["is_windows"] else "kain")
    env = os.environ.copy()
    if status.get("clang"):
        env.setdefault("KAIN_CLANG_PATH", status["clang"])
    if status.get("libclang_dir"):
        env.setdefault("LIBCLANG_PATH", status["libclang_dir"])
    if not shutil.which("llvm-config"):
        # llvm-sys (inkwell) needs an LLVM prefix when llvm-config is absent.
        # Derive it from the clang install (../.. of <prefix>/bin/clang).
        try:
            prefix = str(Path(status["clang"]).resolve().parent.parent)
            env.setdefault("LLVM_SYS_211_PREFIX", prefix)
        except Exception:
            pass
    cargo_bin = str(Path.home() / ".cargo" / "bin")
    if Path(cargo_bin).exists() and cargo_bin not in env.get("PATH", ""):
        env["PATH"] = cargo_bin + os.pathsep + env.get("PATH", "")
    t0 = time.time()
    overlay = None if dry else write_dumb_build_info()
    try:
        rc = run_live(cmd, cwd=REPO_ROOT, env=env)
    finally:
        restore_build_info(overlay)  # always: never leave repo dirt behind
    if rc != 0:
        die(f"cargo build failed (exit {rc}). Fix the errors above and rerun; "
            "--skip-compiler to skip this step.")
    log(f"compiler OK in {(time.time() - t0) / 60:.1f} min")
    exe = REPO_ROOT / "target" / "release" / ("kain.exe" if osinfo["is_windows"] else "kain")
    if not exe.exists():
        die(f"cargo said OK but {exe} is missing?!")
    return exe


# ---------------------------------------------------------------- install ---

def default_install_dir() -> Path:
    return Path.home() / ".kain"


def cargo_target_dir() -> Path:
    try:
        proc = run(["cargo", "metadata", "--no-deps", "--format-version", "1"],
                   cwd=REPO_ROOT, timeout=60)
        if proc.returncode == 0:
            meta = json.loads(decode_bytes(proc.stdout))
            if meta.get("target_directory"):
                return Path(meta["target_directory"])
    except Exception:
        pass
    return REPO_ROOT / "target"


def report_lock_holders(exe_name: str) -> None:
    """Best-effort: show which PIDs hold a same-named exe (Windows tasklist)."""
    try:
        if os.name == "nt":
            proc = run(["tasklist", "/FI", f"IMAGENAME eq {exe_name}", "/FO", "CSV"],
                       timeout=15)
            lines = [ln for ln in decode_bytes(proc.stdout).splitlines()
                     if exe_name.lower() in ln.lower()]
            for ln in lines[:5]:
                warn(f"  holder: {ln}")
            if lines:
                warn("kill those PIDs (or stop kain LSP/watchers) and rerun, "
                     "or leave the .old file until next reboot.")
    except Exception:
        pass


def safe_install_binary(src: Path, dst: Path) -> None:
    """Copy src -> dst, surviving Windows file locks (running exe, AV scan).
    Strategy: retry direct copy (transient locks), else rename-swap: NTFS
    allows RENAMING an in-use file even though it forbids overwriting it."""
    for attempt in range(3):
        try:
            shutil.copy2(src, dst)
            return
        except PermissionError:
            if attempt < 2:
                time.sleep(2)
    warn(f"{dst.name} is locked (usually a running kain/LSP/watcher process).")
    report_lock_holders(dst.name)
    new_tmp = dst.with_name(dst.name + ".new")
    old_backup = dst.with_name(dst.stem + ".old" + dst.suffix)
    shutil.copy2(src, new_tmp)  # staging a fresh file never conflicts
    try:
        if old_backup.exists():
            try:
                old_backup.unlink()
            except OSError:
                old_backup = dst.with_name(
                    f"{dst.stem}.old-{int(time.time())}{dst.suffix}")
        dst.rename(old_backup)  # works even when dst is a running exe
        new_tmp.rename(dst)
    except OSError as e:
        try:
            if new_tmp.exists():
                new_tmp.unlink()
        except OSError:
            pass
        die(f"cannot replace locked {dst.name}: {e}. "
            f"Stop every '{dst.name}' process and rerun.")
    warn(f"old binary preserved at {old_backup.name} -- delete it once "
         f"all {dst.name} processes are restarted.")


def install_layout(osinfo: dict, install_dir: Path, runtime_lib: Path,
                   dry: bool, clean: bool) -> dict:
    bindir = install_dir / "bin"
    stdlib_dst = install_dir / "stdlib"
    runtime_dst = install_dir / "runtime"
    log(f"install: -> {install_dir}")

    if dry:
        log("(dry-run) would copy binaries + stdlib/ + runtime/ + write manifest")
        return {"bin": bindir}

    if clean and install_dir.exists():
        log(f"clean: wiping {install_dir}")
        shutil.rmtree(install_dir)

    for d in (bindir, install_dir / "generated", install_dir / "packages",
              install_dir / "tooling", install_dir / "cache",
              install_dir / "toolchain" / "llvm" / "bin"):
        d.mkdir(parents=True, exist_ok=True)

    # 1. binaries from cargo target dir
    tdir = cargo_target_dir() / "release"
    ext = ".exe" if osinfo["is_windows"] else ""
    installed_bins = []
    for name in ("kain", "blade", "kn"):
        src = tdir / f"{name}{ext}"
        if not src.exists():
            if name == "kain":
                die(f"expected compiler binary at {src}. Run without --skip-compiler first.")
            warn(f"optional binary missing, skipping: {src.name}")
            continue
        dst = bindir / f"{name}{ext}"
        safe_install_binary(src, dst)
        installed_bins.append(str(dst))
        log(f"  bin: {src.name} -> {dst}")

    # 2. freshly built runtime lib next to the installed tree
    if runtime_lib.exists():
        dst_lib = install_dir / "lib"
        dst_lib.mkdir(parents=True, exist_ok=True)
        shutil.copy2(runtime_lib, dst_lib / runtime_lib.name)
        log(f"  lib: {runtime_lib.name} -> {dst_lib / runtime_lib.name}")
    else:
        warn(f"runtime lib not found at {runtime_lib} (build it: rerun without --skip-runtime)")

    # 3. stdlib/ + runtime/ trees (same layout as install_kain.py)
    for src_dir, dst_dir in ((REPO_ROOT / "stdlib", stdlib_dst),
                             (REPO_ROOT / "runtime", runtime_dst)):
        if not src_dir.exists():
            warn(f"repo dir missing, skipping: {src_dir}")
            continue
        if dst_dir.exists():
            shutil.rmtree(dst_dir)
        shutil.copytree(src_dir, dst_dir)
        log(f"  tree: {src_dir.name}/ -> {dst_dir} ")

    # root stdlib.kn bundle (the 39k-line dump) lives at repo root too
    root_stdlib = REPO_ROOT / "stdlib.kn"
    if root_stdlib.exists():
        shutil.copy2(root_stdlib, stdlib_dst / "stdlib.kn")
        log("  tree: stdlib.kn (root bundle) -> stdlib/stdlib.kn")

    manifest = {
        "built_by": SCRIPT_NAME,
        "built_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "os": platform.system(),
        "machine": platform.machine(),
        "repo": str(REPO_ROOT),
        "install_dir": str(install_dir),
        "binaries": installed_bins,
        "runtime_lib": runtime_lib.name,
        "stdlib_path": str(stdlib_dst),
        "runtime_manifest": str(runtime_dst / "native_core_runtime.toml"),
    }
    with open(install_dir / "install_manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    log("  manifest: install_manifest.json")
    return {"bin": bindir, "manifest": manifest}


# ---------------------------------------------------------------- PATH ---

def setup_path_windows(bindir: Path, dry: bool) -> None:
    entry = str(bindir)
    try:
        import winreg  # stdlib on Windows
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment",
                            0, winreg.KEY_READ | winreg.KEY_WRITE) as key:
            try:
                cur, _ = winreg.QueryValueEx(key, "Path")
            except FileNotFoundError:
                cur = ""
            parts = [p for p in cur.split(";") if p]
            if entry.lower() in (p.lower() for p in parts):
                log("PATH already contains bin/ (user env). Nothing to do.")
                return
            new = cur + (";" if cur and not cur.endswith(";") else "") + entry
            if dry:
                log(f"(dry-run) would append to HKCU user PATH: {entry}")
                return
            winreg.SetValueEx(key, "Path", 0, winreg.REG_EXPAND_SZ, new)
            log(f"PATH updated (HKCU): {entry}")
            log("NOTE: restart your terminal to pick it up.")
            try:
                import ctypes
                from ctypes import wintypes
                HWND_BROADCAST, WM_SETTINGCHANGE = 0xFFFF, 0x1A
                SMTO_ABORTIFHUNG = 0x0002
                ctypes.windll.user32.SendMessageTimeoutW(
                    HWND_BROADCAST, WM_SETTINGCHANGE, 0, "Environment",
                    SMTO_ABORTIFHUNG, 5000, None)
            except Exception:
                pass
    except Exception as e:
        warn(f"winreg PATH update failed ({e}); falling back to setx")
        if dry:
            log(f"(dry-run) would run: setx PATH \"%PATH%;{entry}\"")
            return
        rc = run_live(["setx", "PATH", f"%PATH%;{entry}"])
        if rc != 0:
            warn("setx failed. Add this by hand in Settings > Environment variables:")
            print(f"    {entry}")


def setup_path_unix(bindir: Path, dry: bool) -> None:
    entry = str(bindir)
    block = (f"\n# >>> KAIN ({SCRIPT_NAME}) >>>\n"
             f'export PATH="{entry}:$PATH"\n'
             f"# <<< KAIN <<<\n")
    touched = []
    for rc in (Path.home() / ".bashrc", Path.home() / ".zshrc"):
        try:
            if not rc.exists():
                continue
            text = rc.read_text(encoding="utf-8", errors="replace")
            if entry in text:
                continue
            if dry:
                log(f"(dry-run) would append PATH block to {rc}")
                touched.append(str(rc))
                continue
            with open(rc, "a", encoding="utf-8") as f:
                f.write(block)
            touched.append(str(rc))
        except OSError as e:
            warn(f"cannot update {rc}: {e}")
    if touched:
        log(f"PATH block written to: {', '.join(touched)} (restart shell to pick up)")
    else:
        log("PATH already set or no rc files found.")
    print(f'  (manual: export PATH="{entry}:$PATH")')


def setup_path(osinfo: dict, bindir: Path, dry: bool, skip: bool) -> None:
    if skip:
        log("--no-path: skipping PATH setup.")
        print(f'  (manual: add to PATH: {bindir})')
        return
    if osinfo["is_windows"] and not osinfo["in_wsl"]:
        setup_path_windows(bindir, dry)
    else:
        setup_path_unix(bindir, dry)


# ---------------------------------------------------------------- verify ---

def verify(osinfo: dict, install_dir: Path, dry: bool) -> bool:
    if dry:
        log("(dry-run) would run: kain --version")
        return True
    bindir = install_dir / "bin"
    ext = ".exe" if (osinfo["is_windows"] and not osinfo["in_wsl"]) else ""
    kain = bindir / f"kain{ext}"
    if not kain.exists():  # fall back to cargo target dir (with --skip-install)
        kain = cargo_target_dir() / "release" / f"kain{ext}"
    if not kain.exists():
        err(f"verify: no kain binary at {kain}")
        return False
    proc = run([str(kain), "--version"], timeout=60)
    out = (decode_bytes(proc.stdout) + decode_bytes(proc.stderr)).strip()
    ok = proc.returncode == 0
    print(f"  kain --version -> {(out.splitlines()[0] if out else '(no output)')} "
          f"[{'OK' if ok else 'FAIL'}]")
    for label, p in (("stdlib/", install_dir / "stdlib"),
                     ("runtime manifest", install_dir / "runtime" / "native_core_runtime.toml"),
                     ("manifest", install_dir / "install_manifest.json")):
        print(f"  {label}: {'OK' if p.exists() else 'MISSING'} ({p})")
    libdir = install_dir / "lib"
    lib_ok = libdir.exists() and any(libdir.glob("kain_runtime.*"))
    print(f"  runtime lib: {'OK' if lib_ok else 'MISSING'} ({libdir}) ")
    return ok and lib_ok


# ---------------------------------------------------------------- WSL ---

def strip_wsl_flag(argv: list[str]) -> list[str]:
    """Remove --wsl and its optional value so the inner run does not loop."""
    out, i = [], 0
    while i < len(argv):
        a = argv[i]
        if a == "--wsl":
            if i + 1 < len(argv) and not argv[i + 1].startswith("-"):
                i += 2
            else:
                i += 1
            continue
        if a.startswith("--wsl="):
            i += 1
            continue
        out.append(a)
        i += 1
    return out


def wsl_reexec(distro: str | None, osinfo: dict) -> int:
    """Re-run this script inside WSL. Returns the inner exit code."""
    wsl = shutil.which("wsl") or shutil.which("wsl.exe")
    if not wsl:
        die("WSL not found. Enable it: wsl --install -d Ubuntu (admin terminal), reboot, rerun.")
    distros = wsl_distros()
    target = distro
    if not target or target == "auto":
        target = distros[0] if distros else None
    if not target:
        die("No WSL distro installed. Run with --wsl-setup to install Ubuntu first.")
    if target not in distros:
        warn(f"distro {target!r} not in installed list {distros}; trying anyway.")
    log(f"WSL build: re-entering via distro '{target}' ...")
    proc = run([wsl, "wslpath", "-a", str(REPO_ROOT / SCRIPT_NAME)], timeout=30)
    linux_script = decode_bytes(proc.stdout).strip()
    if not linux_script:
        die("wslpath translation failed. Is WSL healthy? (wsl --list --verbose)")
    inner = strip_wsl_flag(sys.argv[1:]) + ["--inside-wsl"]
    cmd = [wsl, "-d", target, "--", "python3", linux_script] + inner
    log(f"exec: wsl -d {target} -- python3 {linux_script} ...")
    return run_live(cmd)


def wsl_setup(osinfo: dict, dry: bool) -> int:
    """Install Ubuntu WSL + inside-WSL toolchain. Windows only."""
    wsl = shutil.which("wsl") or shutil.which("wsl.exe")
    if not wsl:
        die("WSL launcher not found. This needs Windows 10 2004+ / Windows 11.")
    distros = wsl_distros()
    if not distros:
        log("installing Ubuntu under WSL (needs elevation; reboot may follow) ...")
        if dry:
            log("(dry-run) would run: wsl --install -d Ubuntu")
            return 0
        rc = run_live([wsl, "--install", "-d", "Ubuntu"])
        if rc != 0:
            die("wsl --install failed. Run it by hand in an admin terminal, reboot, rerun.")
        distros = ["Ubuntu"]
    target = distros[0]
    log(f"setting up inside-WSL toolchain in '{target}' ...")
    setup_cmd = ("sudo apt-get update && sudo apt-get install -y clang lld llvm binutils git "
                 "curl build-essential python3 && "
                 "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | "
                 "sh -s -- -y --default-toolchain stable --profile minimal")
    if dry:
        log(f"(dry-run) would run inside {target}: {setup_cmd}")
        return 0
    return run_live([wsl, "-d", target, "--", "bash", "-lc", setup_cmd])


# ---------------------------------------------------------------- main ---

def main(argv: list[str] | None = None) -> int:
    print(BANNER)
    print("  cmake note: this repo has NO CMake build (no CMakeLists.txt).")
    print("  You need: cargo/rustc + clang + archiver + git. That is all.")
    print("")
    args = parse_args(argv)
    allow_install = bool(args.yes or args.install_deps)
    jobs = resolve_jobs(args.jobs, os.cpu_count() or 4)
    install_dir = (args.install_dir or default_install_dir()).expanduser().resolve()
    osinfo = detect_os()

    print(f"  repo         : {REPO_ROOT}")
    print(f"  os           : {platform.system()} ({osinfo['machine']})"
          + ("  [inside WSL]" if osinfo["in_wsl"] else ""))
    print(f"  jobs         : {jobs}  (cores detected: {osinfo['cpu_count']})")
    print(f"  install-dir  : {install_dir}")
    print(f"  path setup   : {'OFF (--no-path)' if args.no_path else 'ON'}")
    print(f"  auto-install : {'ON (--yes)' if allow_install else 'OFF (rerun with --yes to fix missing tools)'}")
    print("")

    # -- WSL branch (Windows host only) --
    if args.wsl is not None and not args.inside_wsl:
        if not osinfo["is_windows"]:
            die("--wsl only makes sense on Windows (you are already on linux/mac).")
        if args.dry_run:
            log("(dry-run) would re-exec this script inside WSL")
            return 0
        return wsl_reexec(None if args.wsl == "auto" else args.wsl, osinfo)

    if args.wsl_setup:
        if not osinfo["is_windows"]:
            die("--wsl-setup is Windows-only.")
        return wsl_setup(osinfo, args.dry_run)

    if args.wsl is not None and osinfo["is_windows"] and args.inside_wsl:
        pass  # fell through after re-exec; continue as a linux build
    elif osinfo["is_windows"] and not args.inside_wsl:
        distros = wsl_distros()
        if distros and args.wsl is None:
            log(f"WSL detected ({', '.join(distros)}). Native Windows build continues; "
                "rerun with --wsl to build inside WSL instead.")

    if args.dry_run:
        log("DRY RUN -- plan only, changing nothing.")
        status = preflight(osinfo, verbose=args.verbose)
        need = missing_tools(status, osinfo)
        log(f"would need: {', '.join(need) if need else 'nothing (toolchain complete)'}")
        n_src = len(collect_runtime_sources(osinfo))
        log(f"would compile {n_src} runtime C files (clang -O2 -std=c11) -> "
            f"{'kain_runtime.lib' if osinfo['is_windows'] or args.wsl else 'libkain_runtime.a'}")
        log(f"would run: cargo build --release -p cli --jobs {jobs}")
        log(f"would install to: {install_dir}  (bin/ stdlib/ runtime/ manifest)")
        setup_path(osinfo, install_dir / "bin", dry=True, skip=args.no_path)
        return 0

    # -- 1. toolchain --
    status = preflight(osinfo, verbose=args.verbose)
    status = ensure_toolchain(osinfo, status, allow_install, dry=False)

    # -- 2. runtime --
    runtime_lib = REPO_ROOT / "target" / "dumb" / "runtime" / (
        "kain_runtime.lib" if osinfo["is_windows"] else "libkain_runtime.a")
    if args.skip_runtime:
        log("--skip-runtime: reusing existing lib if present.")
    else:
        runtime_lib = build_runtime(osinfo, status, jobs, dry=False, verbose=args.verbose)

    # -- 3. compiler (max speed: release = opt-level 3, non-debug, all features) --
    if args.skip_compiler:
        log("--skip-compiler: skipping cargo build.")
    else:
        build_compiler(osinfo, status, jobs, dry=False)

    # -- 4. install --
    if args.skip_install:
        log("--skip-install: binaries stay in target/release/.")
        bindir = cargo_target_dir() / "release"
    else:
        info = install_layout(osinfo, install_dir, runtime_lib, dry=False, clean=args.clean)
        bindir = info["bin"]

    # -- 5. PATH --
    setup_path(osinfo, bindir, dry=False, skip=args.no_path)

    # -- 6. verify --
    if args.skip_verify:
        log("--skip-verify: done (unverified).")
        return 0
    ok = verify(osinfo, install_dir if not args.skip_install else REPO_ROOT, dry=False)
    print("")
    if ok:
        log("DUMB BUILD COMPLETE. Open a NEW terminal, then:  kain --version")
    else:
        err("Build finished but verification FAILED. See lines above.")
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        err("interrupted.")
        sys.exit(130)
