#!/usr/bin/env python3
"""
kain-native — Zero-friction native binary emitter for Kain

Bridges kain build (LLVM IR) → clang (native binary).
Makes kain build --target llvm produce executables/DLLs in one shot,
the way it should work natively.

Usage:
  kain-native src/file.kn                    → src/file.exe       (default: exe)
  kain-native src/file.kn --emit sharedlib   → src/file.dll
  kain-native src/file.kn --emit staticlib   → src/file.lib
  kain-native src/file.kn --emit object      → src/file.obj
  kain-native src/file.kn --emit llvm-ir     → src/file.ll        (passthrough)

The vision: this script exists only until the compiler proper
absorbs --emit and native linking into kain build itself.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


# ── Clang discovery (mirrors install_layout.rs resolution) ──────────

CLANG_CANDIDATES = [
    # KAIN_CLANG_PATH env var
    lambda: os.environ.get("KAIN_CLANG_PATH"),
    # Bundled: toolchain/llvm/bin/clang.exe relative to repo root
    lambda: _find_bundled_clang(),
    # PATH
    lambda: shutil.which("clang") or shutil.which("clang.exe"),
    # System installs
    lambda: _first_existing([
        r"C:\Program Files\LLVM\bin\clang.exe",
        r"C:\Program Files (x86)\LLVM\bin\clang.exe",
    ]),
]

def _find_bundled_clang() -> str | None:
    """Search ancestor directories for toolchain/llvm/bin/clang."""
    candidates = [
        "toolchain/llvm/bin/clang.exe",
        "toolchain/llvm/bin/clang",
        "llvm/bin/clang.exe",
        "llvm/bin/clang",
    ]
    d = Path.cwd()
    for _ in range(20):
        for c in candidates:
            p = d / c
            if p.exists():
                return str(p)
        parent = d.parent
        if parent == d:
            break
        d = parent
    return None

def _first_existing(paths: list[str]) -> str | None:
    for p in paths:
        if os.path.exists(p):
            return p
    return None

def find_clang() -> str:
    for candidate_fn in CLANG_CANDIDATES:
        result = candidate_fn()
        if result and (result == "clang" or os.path.exists(result)):
            return result
    return "clang"  # fallback, let it fail with clear error

def find_repo_root() -> Path:
    d = Path.cwd()
    for _ in range(20):
        if (d / "AGENTS.md").exists() and (d / "CATALOG.md").exists():
            return d
        parent = d.parent
        if parent == d:
            break
        d = parent
    return Path.cwd()


# ── Build pipeline ──────────────────────────────────────────────────

def run_kain_build(source: Path, target: str = "llvm") -> tuple[Path, str]:
    """Run kain build, return (output_dir, source_text)."""
    args = [
        "kain", "build", str(source),
        "--target", target,
    ]
    result = subprocess.run(args, capture_output=True, text=True, timeout=120)
    
    if result.returncode != 0:
        print(f"kain build failed:\n{result.stderr}\n{result.stdout}")
        sys.exit(1)
    
    # Parse the report to find the output .ll path
    report_match = None
    for line in result.stdout.split("\n") + result.stderr.split("\n"):
        if "Report:" in line:
            report_match = line.split("Report:")[-1].strip().rstrip("\\")
            break
    
    if report_match:
        try:
            report = json.loads(Path(report_match).read_text())
            for unit in report.get("units", []):
                for output in unit.get("outputs", []):
                    if output.endswith(".ll"):
                        ll_path = Path(output)
                        return ll_path.parent, Path(source).read_text()
        except (json.JSONDecodeError, KeyError, FileNotFoundError):
            pass
    
    # Fallback: search .kain/out for the .ll file
    repo = find_repo_root()
    out_dir = repo / ".kain" / "out"
    stem = source.stem
    for ll_file in out_dir.rglob(f"{stem}.ll"):
        return ll_file.parent, source.read_text()
    
    print("Could not find compiled .ll output from kain build")
    sys.exit(1)


def postprocess_llvm_ir(ll_path: Path, emit: str) -> Path | None:
    """Post-process LLVM IR for DLL export. Returns path to (possibly modified) .ll."""
    if emit != "sharedlib":
        return ll_path
    
    content = ll_path.read_text()
    modified = False
    lines = content.split("\n")
    new_lines = []
    
    for line in lines:
        # Make all non-internal function definitions dllexport
        if line.startswith("define ") and "internal" not in line:
            line = line.replace("define ", "define dllexport ", 1)
            modified = True
        elif line.startswith("define internal "):
            # For shared libraries, make internal functions dllexport too
            # (caller might need them)
            line = line.replace("define internal ", "define dllexport ", 1)
            modified = True
        new_lines.append(line)
    
    if modified:
        new_ll = ll_path.with_suffix(".export.ll")
        new_ll.write_text("\n".join(new_lines))
        return new_ll
    
    return ll_path


def compile_with_clang(ll_path: Path, output: Path, emit: str, source_text: str = "") -> None:
    """Invoke clang to produce the final binary."""
    clang = find_clang()
    print(f"  clang: {clang}")
    
    # Auto-detect: if Kain source doesn't use std::runtime, skip libc
    uses_runtime = "use std::runtime" in source_text or "use std::process" in source_text
    uses_libc = uses_runtime or "use std::fs" in source_text or "use std::os" in source_text
    
    cmd = [clang, "-O2", "-Wno-override-module"]
    
    if emit == "sharedlib":
        cmd.extend(["-shared"])
        if not uses_libc:
            cmd.extend(["-nostdlib", "-Wl,-noentry"])
    elif emit == "exe":
        if not uses_libc:
            # Pure computation, no libc needed
            cmd.extend(["-nostdlib", "-Wl,/entry:main"])
    elif emit == "staticlib":
        obj = output.with_suffix(".obj")
        subprocess.run([clang, "-c", "-O2", str(ll_path), "-o", str(obj)], check=True)
        ar = shutil.which("llvm-ar") or shutil.which("ar") or "llvm-ar"
        subprocess.run([ar, "rcs", str(output), str(obj)], check=True)
        if obj.exists():
            obj.unlink()
        print(f"  -> {output}")
        return
    elif emit == "object":
        cmd.extend(["-c"])
    elif emit == "llvm-ir":
        shutil.copy2(ll_path, output)
        print(f"  -> {output}")
        return
    
    cmd.append(str(ll_path))
    cmd.extend(["-o", str(output)])
    
    if sys.platform == "win32":
        if uses_libc:
            _apply_msvc_env(cmd)
        cmd.append("-Wl,/subsystem:console")
    
    print(f"  linking...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"  clang failed:\n{result.stderr}\n{result.stdout}")
        sys.exit(1)
    
    print(f"  → {output} ({_format_size(output)})")


def _apply_msvc_env(cmd: list[str]) -> None:
    """Mirrors apply_windows_msvc_link_env in install_layout.rs."""
    # Add MSVC library search paths
    lib_paths = []
    for env_var in ["LIB", "VCINSTALLDIR"]:
        if env_var in os.environ:
            for p in os.environ[env_var].split(";"):
                p = p.strip()
                if p and os.path.isdir(p):
                    lib_paths.append(p)
    
    # Visual Studio 2022
    for edition in ["BuildTools", "Community", "Professional", "Enterprise", "Preview"]:
        base = Path(f"C:/Program Files (x86)/Microsoft Visual Studio/2022/{edition}/VC/Tools/MSVC")
        if base.exists():
            versions = sorted([d for d in base.iterdir() if d.is_dir()], reverse=True)
            if versions:
                lib_paths.append(str(versions[0] / "lib" / "x64"))
    
    # Windows SDK
    for kits_root in [r"C:\Program Files (x86)\Windows Kits\10", r"C:\Program Files\Windows Kits\10"]:
        kits = Path(kits_root) / "Lib"
        if kits.exists():
            versions = sorted([d for d in kits.iterdir() if d.is_dir()], reverse=True)
            if versions:
                v = versions[0]
                lib_paths.append(str(v / "ucrt" / "x64"))
                lib_paths.append(str(v / "um" / "x64"))


def _format_size(path: Path) -> str:
    size = path.stat().st_size
    if size < 1024:
        return f"{size}B"
    elif size < 1024 * 1024:
        return f"{size / 1024:.1f}KB"
    else:
        return f"{size / (1024 * 1024):.1f}MB"


# ── Main ────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="kain-native — Zero-friction native binary emitter for Kain",
    )
    parser.add_argument("source", type=Path, help="Kain source file (.kn)")
    parser.add_argument(
        "--emit", choices=["exe", "sharedlib", "staticlib", "object", "llvm-ir"],
        default="exe",
        help="Output artifact type (default: exe)",
    )
    parser.add_argument(
        "-o", "--output", type=Path,
        help="Output path (default: derived from source name + emit extension)",
    )
    parser.add_argument(
        "--keep-ir", action="store_true",
        help="Keep intermediate .ll file",
    )
    args = parser.parse_args()
    
    source = args.source.resolve()
    if not source.exists():
        print(f"Source not found: {source}")
        sys.exit(1)
    
    emit = args.emit
    
    # Determine output path
    ext_map = {
        "exe": ".exe" if sys.platform == "win32" else "",
        "sharedlib": ".dll" if sys.platform == "win32" else ".so",
        "staticlib": ".lib" if sys.platform == "win32" else ".a",
        "object": ".obj" if sys.platform == "win32" else ".o",
        "llvm-ir": ".ll",
    }
    
    if args.output:
        output = args.output.resolve()
    else:
        output = source.with_suffix(ext_map.get(emit, ""))
    
    print(f"═══ kain-native {source.name} → {emit} ═══")
    print(f"  source: {source}")
    print(f"  emit:   {emit}")
    print(f"  output: {output}")
    print()
    
    # Step 1: Compile to LLVM IR
    print("── kain build --target llvm ──")
    out_dir, source_text = run_kain_build(source)
    ll_path = out_dir / f"{source.stem}.ll"
    
    if not ll_path.exists():
        print(f"  .ll not found at expected path: {ll_path}")
        sys.exit(1)
    
    print(f"  IR: {ll_path} ({_format_size(ll_path)})")
    
    # Step 2: Post-process IR for shared libraries
    ll_path = postprocess_llvm_ir(ll_path, emit)
    if ll_path and ll_path.suffix == ".export.ll":
        print(f"  export IR: {ll_path}")
    
    # Step 3: Compile with clang
    print(f"\n── clang → {emit} ──")
    compile_with_clang(ll_path or out_dir / f"{source.stem}.ll", output, emit, source_text)
    
    # Cleanup
    if ll_path and ll_path.suffix == ".export.ll" and ll_path.exists():
        ll_path.unlink()
    
    print(f"\n✅ {emit}: {output}")


if __name__ == "__main__":
    main()
