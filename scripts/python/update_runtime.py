#!/usr/bin/env python3
"""BUILD + ARCHIVE + SYNC + VERIFY the Kain native C runtime library.

Exit codes:
    0   success
    1   build failure (bazel)
    2   archive failure (librarian)
    3   verification failure (kain doctor / smoke test)
    4   environment / file-not-found failure
    9   bug in this script

Usage:
    py -3 scripts/python/update_runtime.py
    py -3 scripts/python/update_runtime.py --skip-build
    py -3 scripts/python/update_runtime.py --check
    py -3 scripts/python/update_runtime.py --manifest
    py -3 scripts/python/update_runtime.py --help

WHEN IT FAILS:
  - Every error prints an EXACT explanation of what went wrong.
  - Every error prints WHAT TO CHECK with the exact paths.
  - See X:/docs/TROUBLESHOOTING_DEV.md for detailed entries:
      Entry 1: stale runtime rebuild
      Entry 2: missing .c files in manifest
      Entry 3: Bazel cache
      Entry 4: sync pipeline
      Entry 5: runtime lib not found
      Entry 6: file locks
      Entry 7: cold Bazel server
      Entry 8: this script
"""

from __future__ import annotations

import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Sequence


# ── CONSTANTS ──────────────────────────────────────────────────────────────

SCRIPT_DIR   = Path(__file__).resolve().parent
REPO_ROOT    = SCRIPT_DIR.parent.parent

RUNTIME_TOML        = REPO_ROOT / "runtime" / "native_core_runtime.toml"
MANIFEST_GENERATOR  = REPO_ROOT / "tools" / "bazel" / "sync_native_runtime_builds.py"
MANIFEST_BZL        = REPO_ROOT / "runtime" / "runtime_manifest_data.bzl"
KAIN_HOME_LIB       = REPO_ROOT / ".kain" / "lib"
KAIN_BIN_DIR        = REPO_ROOT / ".kain" / "bin"
RUNTIME_SRC_DIR     = REPO_ROOT / "runtime" / "native" / "src"
BAZEL_TARGET        = "//runtime:native_core_runtime"
TROUBLESHOOTING_DOC = "X:/docs/TROUBLESHOOTING_DEV.md"

VERBOSE = False


# ── HELPERS ────────────────────────────────────────────────────────────────

def warn(msg: str) -> None:
    print(f"  [WARN] {msg}", file=sys.stderr)
    sys.stderr.flush()

def fail(msg: str, exit_code: int = 1, docs_entry: str | None = None) -> None:
    sys.stdout.flush()
    sys.stderr.flush()
    print(f"\n  [FAIL] {msg}", file=sys.stderr)
    if docs_entry:
        print(f"         See {TROUBLESHOOTING_DOC}#{docs_entry}", file=sys.stderr)
    print(f"         See {TROUBLESHOOTING_DOC} for detailed troubleshooting.", file=sys.stderr)
    sys.stderr.flush()
    sys.exit(exit_code)

def info(msg: str) -> None:
    print(f"  [info] {msg}")
    sys.stdout.flush()

def dbg(msg: str) -> None:
    if VERBOSE:
        print(f"  [debug] {msg}")
        sys.stdout.flush()

def ts() -> str:
    return time.strftime("%H:%M:%S")

def fmt_bytes(n: int) -> str:
    if n < 1024: return f"{n} B"
    if n < 1024*1024: return f"{n/1024:.1f} KB"
    return f"{n/(1024*1024):.1f} MB"


def is_windows() -> bool:
    return platform.system().lower() == "windows"


def run_capture(
    cmd: Sequence[str | Path],
    cwd: Path | None = None,
    env: dict[str, str] | None = None,
) -> tuple[int, str, str]:
    """Run a command, return (exit_code, stdout, stderr)."""
    try:
        p = subprocess.Popen(
            [str(a) for a in cmd],
            cwd=str(cwd) if cwd else None,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        out, err = p.communicate()
        return p.returncode, out, err
    except FileNotFoundError:
        return -1, "", f"command not found: {cmd[0]}"
    except OSError as e:
        return -1, "", str(e)


def run_live(
    cmd: Sequence[str | Path],
    cwd: Path | None = None,
    env: dict[str, str] | None = None,
) -> int:
    """Run a command with live output, return exit code."""
    try:
        p = subprocess.Popen(
            [str(a) for a in cmd],
            cwd=str(cwd) if cwd else None,
            env=env,
        )
        p.wait()
        return p.returncode
    except FileNotFoundError:
        print(f"  [FAIL] command not found: {cmd[0]}", file=sys.stderr)
        return -1
    except OSError as e:
        print(f"  [FAIL] {e}", file=sys.stderr)
        return -1


def hash_file(path: Path, size_limit: int = 65536) -> str:
    h = hashlib.sha256()
    try:
        h.update(path.read_bytes()[:size_limit])
    except OSError:
        pass
    return h.hexdigest()[:16]


# ── STEP 0: Environment checks ─────────────────────────────────────────────

def check_environment() -> None:
    """Verify every external tool exists before we start building."""
    print(f"  [{ts()}] checking environment...")

    # 1. Python
    dbg(f"python: {sys.executable}")

    # 2. Bazel
    bazel = shutil.which("bazel")
    if not bazel:
        fail(
            "bazel not found on PATH.\n"
            "       CHECK: 'where bazel' or 'bazel version'\n"
            "       FIX:   Install Bazelisk or Bazel, ensure it's on PATH.\n"
            "       See:   X:/docs/BAZEL.md",
            exit_code=4, docs_entry="entry-7-bazel-server-cold-or-unresponsive",
        )
    dbg(f"bazel: {bazel}")

    # 3. Repo root sanity
    if not (REPO_ROOT / "MODULE.bazel").exists() and not (REPO_ROOT / ".git").exists():
        fail(
            f"repo root not found at {REPO_ROOT}\n"
            f"       Run this script from X:/ or a subdirectory of the Kain repo.\n"
            f"       CHECK: does X:/MODULE.bazel exist?",
            exit_code=4,
        )

    # 4. Runtime .toml manifest
    if not RUNTIME_TOML.exists():
        fail(
            f"runtime manifest not found at {RUNTIME_TOML}\n"
            f"       CHECK: is the repo cloned? did you delete runtime/native_core_runtime.toml?\n"
            f"       CHECK: ls {RUNTIME_TOML.parent}",
            exit_code=4,
        )

    # 5. Runtime source directory
    if not RUNTIME_SRC_DIR.exists():
        fail(
            f"runtime source directory not found at {RUNTIME_SRC_DIR}\n"
            f"       CHECK: ls {RUNTIME_SRC_DIR.parent}",
            exit_code=4,
        )

    # 6. .kain/lib/ exists or can be created
    KAIN_HOME_LIB.mkdir(parents=True, exist_ok=True)
    if not KAIN_HOME_LIB.exists():
        fail(
            f"cannot create {KAIN_HOME_LIB}\n"
            f"       CHECK: disk permissions on {KAIN_HOME_LIB.parent}",
            exit_code=4,
        )

    # 7. On Windows: check for a librarian
    if is_windows():
        lib_tool = _find_lib_exe()
        if not lib_tool:
            warn(
                "no librarian found (llvm-lib or lib.exe).\n"
                "         Archiving .obj -> .lib will FAIL.\n"
                "         Install LLVM (for llvm-lib) or Visual Studio Build Tools (for lib.exe).\n"
                "         Or run: scoop install llvm\n"
                "         Then re-run this script."
            )
        else:
            dbg(f"librarian: {lib_tool}")

    print(f"  [{ts()}] environment OK")


# ── STEP 1: Build with Bazel ───────────────────────────────────────────────

def step_build(bazel_config: str) -> None:
    """Build the native C runtime with Bazel."""
    env = dict(os.environ)
    if "KAIN_BAZEL_PYTHON" in env:
        env["KAIN_BAZEL_PYTHON"] = sys.executable

    # ── Check for stale .bazelrc issues ──
    # Bazel sometimes uses --disk_cache in .bazelrc that returns stale results
    print(f"  [{ts()}] bazel build {BAZEL_TARGET} --config={bazel_config} --disk_cache=")

    start = time.time()
    rc = run_live(
        ["bazel", "build", BAZEL_TARGET,
         f"--config={bazel_config}", "--disk_cache="],
        REPO_ROOT, env,
    )
    elapsed = time.time() - start

    if rc != 0:
        print(f"\n  [FAIL] Bazel build FAILED after {elapsed:.1f}s (exit code {rc})")
        print(f"         Target: {BAZEL_TARGET}")
        print(f"         Config: {bazel_config}")
        print(f"         Repo:   {REPO_ROOT}")
        print()
        print(f"  COMMON FIXES:")
        print(f"    a) Cold Bazel server -> restart it:")
        print(f"         bazel shutdown")
        print(f"         bazel build {BAZEL_TARGET} --config={bazel_config}")
        print(f"    b) Disk full?         -> check Z:/ drive space")
        print(f"    c) Corrupted cache?   -> bazel clean --expunge")
        print(f"    d) Wrong config?      -> --config=dev (default) or --config=release")
        print(f"    e) Missing deps?      -> check the full error output above")
        print()
        print(f"  RAW OUTPUT (last 30 lines of stderr if available):")
        # Re-run to capture output
        _, out, err = run_capture(
            ["bazel", "build", BAZEL_TARGET,
             f"--config={bazel_config}", "--disk_cache="],
            REPO_ROOT, env,
        )
        for line in (err or out).splitlines()[-30:]:
            print(f"    | {line}")
        fail(
            f"Bazel build failed for {BAZEL_TARGET}",
            exit_code=1, docs_entry="entry-3-bazel-cache-not-picking-up-changes-to-c-files",
        )

    print(f"  [{ts()}] build OK ({elapsed:.1f}s)")


# ── STEP 2: Resolve Bazel-bin directory ────────────────────────────────────

def step_resolve_bazel_bin(bazel_config: str) -> Path:
    """Run `bazel info bazel-bin` and return the output directory."""
    env = dict(os.environ)

    # Retry logic: Bazel server may still be cold
    last_err = ""
    for attempt in range(3):
        rc, out, err = run_capture(
            ["bazel", "info", "bazel-bin", f"--config={bazel_config}"],
            REPO_ROOT, env,
        )
        if rc == 0:
            lines = [ln.strip() for ln in out.splitlines() if ln.strip()]
            if lines:
                p = Path(lines[-1]).resolve()
                dbg(f"bazel-bin: {p}")
                return p
            warn(f"bazel info bazel-bin: empty output (attempt {attempt+1})")
        else:
            last_err = err
            warn(f"bazel info bazel-bin: rc={rc} (attempt {attempt+1})")
        time.sleep(2)

    fail(
        f"bazel info bazel-bin failed after 3 attempts.\n"
        f"       CHECK: Is the Bazel server alive?\n"
        f"         'bazel info server_pid --config={bazel_config}'\n"
        f"       Last error:\n"
        f"         {last_err[:500]}",
        exit_code=4, docs_entry="entry-7-bazel-server-cold-or-unresponsive",
    )


# ── STEP 3: Archive into static library ────────────────────────────────────

def _find_lib_exe() -> str | None:
    """Find the best available librarian tool."""
    exe = shutil.which("llvm-lib")
    if exe:
        info(f"using llvm-lib: {exe}")
        return exe

    exe = shutil.which("lib.exe")
    if exe:
        info(f"using MSVC lib.exe: {exe}")
        return exe

    # Scan common MSVC installation paths
    pf86 = os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    pf   = os.environ.get("ProgramFiles", "C:\\Program Files")
    bases = [
        f"{pf86}\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
        f"{pf86}\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
        f"{pf}\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
        f"{pf}\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC",
        f"{pf86}\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC",
        f"{pf86}\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC",
    ]
    for base in bases:
        if not os.path.isdir(base):
            continue
        try:
            for entry in sorted(os.listdir(base)):
                for host in ("Hostx64", "Hostarm64"):
                    lp = os.path.join(base, entry, "bin", host, "x64", "lib.exe")
                    dbg(f"  looking: {lp}")
                    if os.path.isfile(lp):
                        info(f"using MSVC lib.exe: {lp}")
                        return lp
        except OSError:
            continue
    return None


def step_archive_windows(bazel_bin: Path) -> None:
    """Windows: archive .obj files into kain_runtime.lib."""
    obj_dir = bazel_bin / "runtime" / "_objs" / "native_core_runtime_c"
    obj_files = sorted(obj_dir.glob("*.obj")) if obj_dir.exists() else []

    if not obj_files:
        # Try without the _c suffix (some configs produce different dir names)
        obj_dir2 = bazel_bin / "runtime" / "_objs" / "native_core_runtime"
        obj_files2 = sorted(obj_dir2.glob("*.obj")) if obj_dir2.exists() else []
        if obj_files2:
            obj_dir = obj_dir2
            obj_files = obj_files2
            info(f"found .obj files at {obj_dir}")
        else:
            # List what IS in the objs directory for debugging
            objs_root = bazel_bin / "runtime" / "_objs"
            contents = ""
            if objs_root.exists():
                contents = "\n".join(
                    f"         {str(p.relative_to(objs_root))}"
                    for p in sorted(objs_root.rglob("*.obj"))
                )[:500]
            fail(
                f"no .obj files found. Expected at: {obj_dir}\n"
                f"       CHECK: Did bazel build actually run?\n"
                f"       CHECK: Is the config correct? (--config=dev, --config=release, etc.)\n"
                f"       CHECK: libs in {objs_root}:\n{contents}\n"
                f"       Retry: py -3 scripts/python/update_runtime.py (without --skip-build)",
                exit_code=2, docs_entry="entry-1-stale-runtime--how-to-rebuild-and-sync-the-native-c-runtime",
            )

    dst_path = KAIN_HOME_LIB / "kain_runtime.lib"
    tmp_path = dst_path.with_name(f"{dst_path.name}.tmp.{os.getpid()}")

    lib_exe = _find_lib_exe()
    if lib_exe is None:
        fail(
            "no librarian found (llvm-lib or lib.exe).\n"
            "       FIX: Install LLVM:\n"
            "         scoop install llvm\n"
            "       FIX: Or install Visual Studio Build Tools:\n"
            "         winget install Microsoft.VisualStudio.2022.BuildTools\n"
            "       FIX: Or run from a Developer Command Prompt.\n"
            "       Then re-run this script.",
            exit_code=2, docs_entry="entry-1-stale-runtime--how-to-rebuild-and-sync-the-native-c-runtime",
        )

    cmd = [lib_exe, "/NOLOGO", "/OUT:" + str(tmp_path)] + [str(f) for f in obj_files]
    info(f"archiving {len(obj_files)} .obj files into {dst_path.name} ...")
    dbg(f"  cmd: {lib_exe} /NOLOGO /OUT:{tmp_path.name} ... ({len(obj_files)} .obj files)")

    rc, out, err = run_capture(cmd, REPO_ROOT)
    if rc != 0:
        # Try to clean up partial temp file
        try:
            tmp_path.unlink(missing_ok=True)
        except OSError:
            pass
        fail(
            f"librarian ({os.path.basename(lib_exe)}) FAILED (exit code {rc}).\n"
            f"       CHECK: Is the .obj directory in the right location?\n"
            f"         {obj_dir}\n"
            f"       CHECK: Are the .obj files valid? Try manually:\n"
            f"         {lib_exe} /NOLOGO /OUT:{tmp_path} {obj_dir / '*.obj'}\n"
            f"       Last error output:\n"
            f"         {err[:400] if err else out[:400]}",
            exit_code=2, docs_entry="entry-1-stale-runtime--how-to-rebuild-and-sync-the-native-c-runtime",
        )

    # ── Atomic replace ──
    try:
        if dst_path.exists():
            os.replace(tmp_path, dst_path)
        else:
            shutil.move(str(tmp_path), str(dst_path))
        info(f"{dst_path.name} written ({fmt_bytes(dst_path.stat().st_size)})")
    except PermissionError:
        staged = dst_path.with_name(f"{dst_path.name}.pending.{os.getpid()}")
        shutil.move(str(tmp_path), str(staged))
        warn(
            f"{dst_path.name} is LOCKED by a running process.\n"
            f"         Replacement staged at: {staged}\n"
            f"         Kill locking processes and copy manually:\n"
            f"           taskkill /F /IM kain.exe\n"
            f"           taskkill /F /IM *.exe  (your app)\n"
            f"           copy /Y \"{staged}\" \"{dst_path}\""
        )
        fail(
            f"could not update {dst_path.name} -- file locked.\n"
            f"       Run: taskkill /F /IM kain.exe\n"
            f"       Then: copy /Y \"{staged}\" \"{dst_path}\"\n"
            f"       Then re-run this script.",
            exit_code=2, docs_entry="entry-6-file-lock-preventing-lib-sync",
        )


def step_archive_posix(bazel_bin: Path) -> None:
    """POSIX: copy the .a produced by cc_library directly."""
    candidates = [
        bazel_bin / "runtime" / "libnative_core_runtime.a",
        bazel_bin / "runtime" / "libnative_core_runtime.so",
    ]
    src_path = next((c for c in candidates if c.exists()), None)

    if not src_path:
        fail(
            f"static library not found.\n"
            f"       CHECKED:\n"
            f"         {candidates[0]}\n"
            f"         {candidates[1]}\n"
            f"       ls {bazel_bin / 'runtime'}/lib*.a\n"
            f"       Did 'bazel build //runtime:native_core_runtime' succeed?",
            exit_code=2, docs_entry="entry-1-stale-runtime--how-to-rebuild-and-sync-the-native-c-runtime",
        )

    dst_path = KAIN_HOME_LIB / "libkain_runtime.a"
    try:
        shutil.copy2(src_path, dst_path)
        info(f"{dst_path.name} written ({fmt_bytes(src_path.stat().st_size)})")
    except PermissionError:
        fail(
            f"could not write {dst_path} -- file locked.\n"
            f"       FIX: kill the locking process, then re-run.",
            exit_code=2, docs_entry="entry-6-file-lock-preventing-lib-sync",
        )


# ── STEP 4: Verify ─────────────────────────────────────────────────────────

def step_verify() -> None:
    """Verify the runtime library is in place and healthy."""
    lib_name = "kain_runtime.lib" if is_windows() else "libkain_runtime.a"
    lib_path = KAIN_HOME_LIB / lib_name

    if not lib_path.exists():
        fail(
            f"runtime library NOT found at {lib_path}\n"
            f"       The archive step did not produce the expected file.\n"
            f"       CHECK: Is {KAIN_HOME_LIB} writable?\n"
            f"       CHECK: ls {KAIN_HOME_LIB}",
            exit_code=3, docs_entry="entry-5-runtime-library-not-found-by-the-kain-compiler",
        )

    mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(lib_path.stat().st_mtime))
    size  = lib_path.stat().st_size
    print(f"  [{ts()}] [OK] {lib_path.name}")
    print(f"       path : {lib_path}")
    print(f"       size : {fmt_bytes(size)}")
    print(f"       mtime: {mtime}")
    print(f"       sha  : {hash_file(lib_path)}")

    # Check env var
    env_lib = os.environ.get("KAIN_RUNTIME_LIB_PATH", "")
    if env_lib:
        ep = Path(env_lib)
        status = "[OK]" if ep.exists() else "[WARN] file not found"
        print(f"       KAIN_RUNTIME_LIB_PATH: {env_lib}  {status}")
    else:
        print(f"       KAIN_RUNTIME_LIB_PATH: (unset, uses $KAIN_HOME/lib/ -- OK)")

    # Run kain doctor if available
    kain_exe = KAIN_BIN_DIR / ("kain.exe" if is_windows() else "kain")
    if kain_exe.exists():
        print(f"  [{ts()}] running kain doctor...", end=" ", flush=True)
        rc, out, err = run_capture([str(kain_exe), "doctor"], REPO_ROOT)
        if rc != 0:
            print("warnings")
            for line in out.splitlines()[-10:]:
                print(f"    | {line}")
            warn(
                "kain doctor reported warnings.\n"
                "         This is not always fatal, but check the output.\n"
                "         Re-run: kain doctor"
            )
        else:
            print("ok")
    else:
        warn(
            f"kain binary not found at {kain_exe}\n"
            f"         Skipping kain doctor check.\n"
            f"         The runtime library is still updated correctly.",
        )


def step_compile_test() -> None:
    """Compile a minimal Kain file to prove the linker can find the runtime."""
    kain_exe = KAIN_BIN_DIR / ("kain.exe" if is_windows() else "kain")
    if not kain_exe.exists():
        warn(
            f"kain binary not found at {kain_exe}\n"
            f"         Skipping compile test.\n"
            f"         Runtime library was still archived successfully."
        )
        return

    tmp_dir = Path(tempfile.mkdtemp(prefix="kain_runtime_test_"))
    try:
        test_file = tmp_dir / "runtime_smoke_test.kn"
        test_file.write_text(
            "use std::runtime\n"
            "\n"
            "fn main() -> Int:\n"
            "    let init = runtime_init()\n"
            "    if init != 0:\n"
            "        return 100 + init\n"
            "    let ok = runtime_heap_validate()\n"
            "    let shutdown = runtime_shutdown()\n"
            "    if shutdown != 0:\n"
            "        return 200 + shutdown\n"
            "    return 0\n"
        )

        print(f"  [{ts()}] compiling smoke test against fresh runtime...", end=" ", flush=True)

        rc, out, err = run_capture(
            [str(kain_exe), "build", str(test_file), "--target", "llvm"],
            REPO_ROOT,
        )

        if rc != 0:
            print("FAIL")
            print(f"    | {err[:500] if err else out[:500]}")
            fail(
                f"smoke test compilation FAILED (exit code {rc}).\n"
                f"       The runtime library may be corrupt or incompatible.\n"
                f"       Try: py -3 scripts/python/update_runtime.py --skip-build\n"
                f"       If that works, the Bazel build produced bad .obj files.\n"
                f"       If that also fails, run manually:\n"
                f"         {kain_exe} build test.kn --target llvm\n"
                f"       And check the full error output.",
                exit_code=3, docs_entry="entry-5-runtime-library-not-found-by-the-kain-compiler",
            )

        print("ok")
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


# ── STEP 0.5: Check manifest completeness ─────────────────────────────────

def step_check_manifest() -> None:
    """Compare the TOML manifest against the source tree.
    Non-fatal: warns about files missing from the manifest."""
    if not RUNTIME_TOML.exists():
        return
    if not RUNTIME_SRC_DIR.exists():
        return

    toml_text = RUNTIME_TOML.read_text(encoding="utf-8")
    manifest_srcs: set[str] = set()
    for line in toml_text.splitlines():
        s = line.strip().strip('",')
        if s.endswith(".c"):
            manifest_srcs.add(s)

    all_c = sorted(RUNTIME_SRC_DIR.rglob("*.c"))
    manifest_c = {RUNTIME_TOML.parent / s for s in manifest_srcs if s}
    missing = sorted(f for f in all_c if f not in manifest_c)

    if missing:
        warn(
            f"{len(missing)} .c files in runtime/native/src/ are NOT in the manifest:\n"
            f"         {RUNTIME_TOML.name}"
        )
        for f in missing[:15]:
            print(f"           - {f.relative_to(REPO_ROOT)}")
        if len(missing) > 15:
            print(f"           ... and {len(missing) - 15} more")
        info(
            f"If you added new .c files, add them to {RUNTIME_TOML.name}\n"
            f"         then re-run with: py -3 scripts/python/update_runtime.py --manifest"
        )


# ── STEP 0: Regenerate manifest data (optional) ────────────────────────────

def step_regenerate_manifest() -> None:
    """Regenerate runtime_manifest_data.bzl from the TOML manifest."""
    if not MANIFEST_GENERATOR.exists():
        warn(
            f"manifest generator not found at {MANIFEST_GENERATOR}\n"
            f"         Skipping manifest regeneration.",
        )
        return

    py = sys.executable
    print(f"  [{ts()}] regenerating Bazel manifest from {RUNTIME_TOML.name}...", end=" ", flush=True)
    rc, out, err = run_capture([py, str(MANIFEST_GENERATOR)], REPO_ROOT)

    if rc != 0:
        print("FAIL")
        print(f"    | {err[:500]}")
        fail(
            f"manifest generator failed (exit code {rc}).\n"
            f"       CHECK: Is {RUNTIME_TOML} valid TOML?\n"
            f"       Try manually: py -3 tools/bazel/sync_native_runtime_builds.py",
            exit_code=4,
        )
    print("ok")
    if out.strip():
        for line in out.strip().splitlines():
            print(f"    | {line}")
    info(f"regenerated: {MANIFEST_BZL}")


# ── CHECK MODE ─────────────────────────────────────────────────────────────

def do_check() -> int:
    """Check current state -- no modifications."""
    sys.stdout.flush()
    print("=== Kain Runtime Library -- Check ===\n")

    lib_name = "kain_runtime.lib" if is_windows() else "libkain_runtime.a"
    lib_path = KAIN_HOME_LIB / lib_name

    print(f"Repository root : {REPO_ROOT}")
    print(f"Library target  : {lib_path}")
    print(f"Runtime source  : {RUNTIME_SRC_DIR}")
    print(f"Manifest        : {RUNTIME_TOML}")
    print(f"Target          : {BAZEL_TARGET}")
    print()

    if lib_path.exists():
        mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(lib_path.stat().st_mtime))
        age   = (time.time() - lib_path.stat().st_mtime) / 3600
        print(f"[OK] Runtime library exists:")
        print(f"   path : {lib_path}")
        print(f"   size : {fmt_bytes(lib_path.stat().st_size)}")
        print(f"   age  : {age:.1f} hours (mtime: {mtime})")
        print(f"   sha  : {hash_file(lib_path)}")
    else:
        print(f"[MISSING] Runtime library NOT found at {lib_path}")

    # Check env
    env_lib = os.environ.get("KAIN_RUNTIME_LIB_PATH", "")
    print(f"\nKAIN_RUNTIME_LIB_PATH = {env_lib or '(unset)'}")
    if env_lib:
        print(f"  resolves to: {Path(env_lib).resolve() if Path(env_lib).exists() else 'NOT FOUND'}")

    # Check tools
    bazel = shutil.which("bazel")
    print(f"\nbazel     : {bazel or 'NOT FOUND'}")
    if is_windows():
        lib_tool = _find_lib_exe()
        print(f"librarian : {lib_tool or 'NOT FOUND (llvm-lib or lib.exe needed)'}")

    # Manifest check
    step_check_manifest()

    print(f"\nFor full help: see {TROUBLESHOOTING_DOC}")
    return 0


# ── MAIN ───────────────────────────────────────────────────────────────────

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build, archive, sync, and verify the Kain native C runtime library.",
        epilog=(
            "Examples:\n"
            "  py -3 scripts/python/update_runtime.py                       # full build + sync\n"
            "  py -3 scripts/python/update_runtime.py --skip-build           # re-archive only\n"
            "  py -3 scripts/python/update_runtime.py --check                # status check\n"
            "  py -3 scripts/python/update_runtime.py --manifest             # regen manifest\n"
            "  py -3 scripts/python/update_runtime.py --config release       # release build\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--skip-build", action="store_true",
                        help="Skip the Bazel build step; re-archive existing .obj files.")
    parser.add_argument("--no-verify", action="store_true",
                        help="Skip kain doctor and compile-test verification steps.")
    parser.add_argument("--manifest", action="store_true",
                        help="Also regenerate the Bazel manifest data from the TOML file.")
    parser.add_argument("--check", action="store_true",
                        help="Check current library state without any modifications.")
    parser.add_argument("--config", default="dev",
                        help="Bazel config to use (default: dev).")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print debug-level detail.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    global VERBOSE
    args = parse_args(argv)
    VERBOSE = args.verbose

    if args.check:
        return do_check()

    sys.stdout.flush()
    print("=== Kain Runtime Library -- Update ===\n")

    # Step 0: Environment checks
    print("-- Step 0/5: Environment checks --")
    sys.stdout.flush()
    check_environment()

    # Step 0.5: Regenerate manifest if requested
    if args.manifest:
        print("\n-- Step 0.5/5: Regenerate Bazel manifest data --")
        step_regenerate_manifest()

    # Step 1: Build
    print(f"\n-- Step 1/5: Build ({BAZEL_TARGET}) --")
    if not args.skip_build:
        print(f"  config: {args.config}")
        step_build(args.config)
    else:
        info("skipped (--skip-build)")

    # Step 2: Resolve output directory
    print(f"\n-- Step 2/5: Resolve output directory --")
    bazel_bin = step_resolve_bazel_bin(args.config)
    info(f"bazel-bin: {bazel_bin}")

    # Step 3: Archive
    print(f"\n-- Step 3/5: Archive into static library --")
    if is_windows():
        step_archive_windows(bazel_bin)
    else:
        step_archive_posix(bazel_bin)

    # Step 4: Verify + manifest sanity
    print(f"\n-- Step 4/5: Verify library --")
    step_verify()

    print(f"\n-- Step 5/5: Manifest sanity check --")
    step_check_manifest()

    # Compile test (non-fatal if it fails but we still generated the .lib)
    if not args.no_verify:
        print()
        step_compile_test()

    print()
    print("=== [OK] Runtime update complete ===")
    info(f"lib: {KAIN_HOME_LIB / ('kain_runtime.lib' if is_windows() else 'libkain_runtime.a')}")
    info(f"see: {TROUBLESHOOTING_DOC}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n  [info] interrupted by user")
        sys.exit(9)
    except Exception as e:
        print(f"\n  [BUG] unexpected error in update_runtime.py: {e}", file=sys.stderr)
        print(f"       This is a bug in the script, not your environment.", file=sys.stderr)
        print(f"       Report: the script crashed with: {type(e).__name__}: {e}", file=sys.stderr)
        sys.exit(9)
