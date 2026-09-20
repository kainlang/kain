# Packaging — Kain Distribution Builder

One Python script to build distributable packages for all platforms.

## Files

| File | What | Ships inside distro? |
|------|------|---------------------|
| `build_package.py` | Builds `.zip` (Windows), `.tar.gz` (Linux/macOS), and `.exe` (Inno Setup, Windows) from Bazel artifacts | ❌ |
| `VERSION` | Canonical release version read by `build_package.py` and the release tooling | ❌ |
| `kain.iss` | Inno Setup 6 script for the Windows installer | ❌ |
| `setup.py` | User-facing setup script — adds Kain to PATH, sets `KAIN_HOME` | ✅ Yes |
| `setup.bat` | Double-click Windows setup wrapper | ✅ Yes |

## Version

The version packaged is read from `packaging/VERSION` (highest precedence), then
the nearest git tag. **Bump `packaging/VERSION` *and* `VERSION_OVERRIDE` in
`tools/bazel/sync_rust_builds.py` together** — the latter is what
`kain --version` / `env!("CARGO_PKG_VERSION")` report in Bazel builds.

> Historical bug: v0.8.1 was tagged but shipped a binary reporting `0.8.0`
> because `VERSION_OVERRIDE` was never bumped.

## Usage

```bash
# Build the distribution (zip + installer) for the version in VERSION
python packaging/build_package.py

# Explicit version (also stamps install_manifest.json)
python packaging/build_package.py --version 0.8.2

# Cross-platform packaging
python packaging/build_package.py --platform linux

# Stage files without archiving (inspect what goes in)
python packaging/build_package.py --stage-only

# Rebuild cleanly (removes the previous stage first)
python packaging/build_package.py --clean
```

Prerequisites:

- A built compiler at `.kain/bin/kain.exe` and runtime at `.kain/lib/**`
  (produced by `bazel build //:kain //runtime:native_core_runtime --config=dev`
  then copied into `.kain/`).
- LLVM on `LIBCLANG_PATH` / `LLVM_PATH`, or at `C:\scoop\apps\llvm\current\bin`.
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`iscc` on PATH) for the `.exe`.

## What goes into the distribution

| Path | Why |
|------|-----|
| `bin/kain.exe`, `bin/kn.exe` | The compiler + alias |
| `bin/*.dll` | `libclang`, `python312`, `vcruntime*` — runtime deps |
| `lib/kain_runtime.lib` | Precompiled native C runtime archive |
| `stdlib/` | 65+ Kain stdlib modules |
| `runtime/native_core_runtime.toml` + `runtime/native/**` | Runtime manifest and the C sources it declares — **required to link any program** |
| `toolchain/llvm/bin/` | Bundled clang / lld / llvm-* tools |
| `setup.py`, `setup.bat` | One-command environment setup |
| `docs/SETUP.md` | Setup guide |
| `config.toml`, `install_manifest.json` | Default config + provenance |

## What the User Does

```bash
# Installer (recommended)
kain-installer-0.8.2-x64.exe     # tick "Add Kain to PATH"

# …or portable zip
unzip kain-0.8.2-windows-x64.zip -d D:\tools\kain
cd D:\tools\kain
python setup.py                  # adds to user PATH, sets KAIN_HOME
python setup.py info             # show version, files
python setup.py uninstall        # remove from environment

# Verify
kain doctor
```
