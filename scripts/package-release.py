#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""package-release.py -- stage + archive a dumb-built Kain install for release.

  Windows :  py scripts/package-release.py --install-dir .kain
  Linux   :  python3 scripts/package-release.py --install-dir ~/.kain

Reads version from packaging/VERSION. Produces:
  packaging/<platform>/kain-<ver>-<platform>-<arch>.zip  (windows)
  packaging/<platform>/kain-<ver>-<platform>-<arch>.tar.gz (linux/darwin)

Layout inside the archive (top dir kain-<ver>-<tag>/):
  bin/  kain[.exe] (+ kn alias copy, blade[.exe], DLLs on windows)
  lib/  kain_runtime.lib / libkain_runtime.a
  stdlib/  full stdlib tree + stdlib.kn bundle
  runtime/ manifest + native C sources (required: compiler hashes them)
  config.toml, install_manifest.json, README-release.txt
"""
from __future__ import annotations

import argparse
import platform
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def plat_tag() -> str:
    arch = platform.machine().lower()
    arch = "x64" if arch in ("amd64", "x86_64") else arch
    sysname = {"Windows": "windows", "Linux": "linux", "Darwin": "darwin"}.get(
        platform.system(), "unknown")
    return f"{sysname}-{arch}"


def get_version() -> str:
    vf = REPO_ROOT / "packaging" / "VERSION"
    if vf.exists() and vf.read_text(encoding="utf-8").strip():
        return vf.read_text(encoding="utf-8").strip().lstrip("v")
    return "0.0.0"


def stage(src: Path, sdir: Path, ver: str, tag: str) -> None:
    is_win = platform.system() == "Windows"
    ext = ".exe" if is_win else ""
    if sdir.exists():
        shutil.rmtree(sdir)
    bind = sdir / "bin"
    bind.mkdir(parents=True)

    kain = src / "bin" / f"kain{ext}"
    if not kain.exists():
        raise SystemExit(f"kain binary not found: {kain}")
    shutil.copy2(kain, bind / f"kain{ext}")
    print(f"  bin/kain{ext} ({kain.stat().st_size / 1048576:.1f} MB)")
    # kn alias: CLI dispatches on argv[0]; a copy is sufficient.
    shutil.copy2(kain, bind / f"kn{ext}")
    for opt in (f"blade{ext}",):
        o = src / "bin" / opt
        if o.exists():
            shutil.copy2(o, bind / opt)
            print(f"  bin/{opt}")
    if is_win:
        for dll in sorted((src / "bin").glob("*.dll")):
            shutil.copy2(dll, bind / dll.name)
        print(f"  bin/*.dll ({len(list(bind.glob('*.dll')))} files)")

    libd = sdir / "lib"
    libd.mkdir(exist_ok=True)
    libname = "kain_runtime.lib" if is_win else "libkain_runtime.a"
    lib = src / "lib" / libname
    if not lib.exists():
        # fall back: freshly built lib in target/dumb/runtime/
        alt = REPO_ROOT / "target" / "dumb" / "runtime" / libname
        if alt.exists():
            lib = alt
    if not lib.exists():
        raise SystemExit(f"runtime lib not found: {lib}")
    shutil.copy2(lib, libd / libname)
    print(f"  lib/{libname} ({lib.stat().st_size / 1024:.0f} KB)")

    for tree in ("stdlib", "runtime"):
        t = src / tree
        if not t.exists():
            raise SystemExit(f"required tree missing: {t}")
        shutil.copytree(t, sdir / tree)
    n_kn = sum(1 for _ in (sdir / "stdlib").rglob("*.kn"))
    print(f"  stdlib/ ({n_kn} .kn files)")
    print(f"  runtime/ (manifest + native sources)")

    for f in ("config.toml", "install_manifest.json"):
        c = src / f
        if c.exists():
            shutil.copy2(c, sdir / f)
    (sdir / "README-release.txt").write_text(
        f"Kain {ver} ({tag}) -- dumb-build portable distribution.\n"
        f"Built with !_DUMB_BUILD_KAIN_COMPILER.py (cargo release + clang C runtime).\n\n"
        f"Contents: bin/kain{ext} (+ kn alias, blade), lib/{libname},\n"
        f"stdlib/, runtime/ (manifest + C sources the compiler hashes at link time).\n\n"
        f"Prerequisites to USE (not bundled): clang 21 + a C archiver (ar/llvm-ar)\n"
        f"on PATH. Linux also needs libasound2 (ALSA) for audio programs.\n\n"
        f"Quick start: add bin/ to PATH, then run: kain --version / kain doctor\n",
        encoding="utf-8")
    print(f"  stage done: {sdir}")


def archive(sdir: Path, outdir: Path, ver: str, tag: str) -> Path:
    outdir.mkdir(parents=True, exist_ok=True)
    base = f"kain-{ver}-{tag}"
    if platform.system() == "Windows":
        out = outdir / f"{base}.zip"
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for f in sorted(sdir.rglob("*")):
                if f.is_file():
                    zf.write(f, str(Path(base) / f.relative_to(sdir)))
    else:
        out = outdir / f"{base}.tar.gz"
        with tarfile.open(out, "w:gz") as tf:
            for f in sorted(sdir.rglob("*")):
                if f.is_file():
                    ti = tf.gettarinfo(str(f), arcname=str(Path(base) / f.relative_to(sdir)))
                    ti.uid = ti.gid = 0
                    ti.uname = ti.gname = ""
                    if f.suffix == "" and (f.stat().st_mode & 0o111):
                        ti.mode = 0o755
                    with open(f, "rb") as fo:
                        tf.addfile(ti, fo)
    print(f"  archive: {out} ({out.stat().st_size / 1048576:.1f} MB)")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--install-dir", type=Path, required=True)
    ap.add_argument("--version", default=get_version())
    ap.add_argument("--stage-only", action="store_true")
    args = ap.parse_args()
    tag = plat_tag()
    sdir = REPO_ROOT / "packaging" / "stage" / f"kain-{args.version}-{tag}"
    print(f"packaging kain {args.version} [{tag}] from {args.install_dir}")
    stage(args.install_dir.expanduser(), sdir, args.version, tag)
    if args.stage_only:
        return 0
    out = archive(sdir, REPO_ROOT / "packaging" / tag.split("-")[0], args.version, tag)
    print(f"DONE: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
