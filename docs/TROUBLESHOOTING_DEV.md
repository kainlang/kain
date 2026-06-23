# Developer Troubleshooting Guide

A collection of common development issues with exact fixes. This guide covers
the build and sync pipeline for the Kain native runtime C library and compiler.
Each entry has a root-cause explanation, step-by-step fix, and verification steps.

---

## Quick Reference Table

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Changes to `runtime/native/src/*.c` don't affect compiled `.exe` | Stale `kain_runtime.lib` | [Rebuild and sync the runtime](#entry-1-stale-runtime--how-to-rebuild-and-sync-the-native-c-runtime) |
| Build fails with `undefined symbol: ui_render_frame` or `ui_layout_resolve` | New UI `.c` files not in runtime manifest | [Add missing source files to manifest](#entry-2-missing-c-source-files-in-the-runtime-manifest) |
| `bazel build //runtime:native_core_runtime` says "nothing to build" despite changed `.c` files | Bazel disk cache returning stale objects | [Invalidate Bazel cache](#entry-3-bazel-cache-not-picking-up-changes-to-c-files) |
| `kain_sync_binary` doesn't update `kain_runtime.lib` | Tool only syncs compiler binary; runtime lib must be rebuilt separately | [Sync runtime lib explicitly](#entry-4-kain_sync_binary-does-not-sync-the-runtime-library) |
| Linker can't find `kain_runtime.lib` or says file not found | `KAIN_RUNTIME_LIB_PATH` env var missing/wrong or `.lib` absent from `~/.kain/lib/` | [Check runtime lib resolution](#entry-5-runtime-library-not-found-by-the-kain-compiler) |
| `.lib` file is locked, can't overwrite | Running process has the file open | [Kill blocking processes](#entry-6-file-lock-preventing-lib-sync) |
| Bazel server timeout or slow startup | Cold server after reboot or idle | [Warm the Bazel server](#entry-7-bazel-server-cold-or-unresponsive) |

---

## Entry 1: Stale Runtime — How to Rebuild and Sync the Native C Runtime

### Root Cause

The Kain compiler (`kain.exe`) links every compiled `.exe` against a **precompiled static library** called `kain_runtime.lib` (Windows) or `libkain_runtime.a` (Linux/macOS). This library contains all the native C runtime code, including the UI system, actor scheduler, memory allocators, input handling, etc.

When you modify a C source file under `runtime/native/src/`, **the change does NOT automatically propagate** to `kain_runtime.lib`. You must explicitly:

1. Build the fresh `.obj` files via Bazel
2. Archive them into a new `.lib`
3. Place the `.lib` where the Kain compiler can find it

### Step-by-Step Fix

#### Step 1: Build the runtime with Bazel

```powershell
# From the repo root (X:/)
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
```

The `--disk_cache=` flag disables the Bazel disk cache, forcing a full
re-evaluation of all C source files. Without this flag, Bazel may return
stale cached results.

Expected output includes `Compiling runtime/native/src/...` lines for each
changed file, followed by `Build completed successfully`.

#### Step 2: Find the compiled `.obj` files

Bazel places compiled object files at:

```
Z:/_b/output-user-root/{hash}/execroot/_main/bazel-out/x64_windows-opt/bin/
    runtime/_objs/native_core_runtime_c/*.obj
```

You can find this path with:

```powershell
bazel info bazel-bin --config=dev
```

#### Step 3: Archive `.obj` files into `kain_runtime.lib`

On Windows, use the MSVC librarian (`lib.exe`):

```powershell
# Find lib.exe (usually in MSVC toolchain)
# Common location:
#   "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.x\bin\Hostx64\x64\lib.exe"

# Navigate to the obj directory
cd Z:/_b/output-user-root/{hash}/execroot/_main/bazel-out/x64_windows-opt/bin/
    runtime/_objs/native_core_runtime_c

# Archive all .obj files (excluding .params files)
lib.exe /NOLOGO /OUT:kain_runtime.lib *.obj

# Copy the new lib to where the Kain compiler expects it
copy /Y kain_runtime.lib X:/.kain/lib/kain_runtime.lib
```

On Linux/macOS, the Bazel `cc_library` produces a `.a` file directly:

```bash
cp bazel-bin/runtime/libnative_core_runtime.a ~/.kain/lib/libkain_runtime.a
```

#### Step 4: Verify the sync

```powershell
# Check the timestamp
dir X:/.kain/lib/kain_runtime.lib

# Rebuild a Kain test file
kain build path/to/your/file.kn --target llvm
```

### Pitfalls

- **File lock**: If the `.lib` is in use by a running process, kill it first.
- **Wrong Bazel config**: Use `--config=dev` for development. `--config=release` creates a different output path (`x64_windows-opt` vs `x64_windows-dbg`).
- **Bazel path translation on Windows**: When using MSYS2/Git Bash, `//runtime:native_core_runtime` gets mangled to a filesystem path. Use PowerShell or `python -c "subprocess.run(...)"` instead.
- **`kain_sync_binary` doesn't sync the runtime**: See [Entry 4](#entry-4-kain_sync_binary-does-not-sync-the-runtime-library).

---

## Entry 2: Missing C Source Files in the Runtime Manifest

### Root Cause

The `native_core_runtime.toml` manifest at `X:/runtime/native_core_runtime.toml`
lists every C source file that should be compiled into `kain_runtime.lib`.
New `.c` files added to `runtime/native/src/` must be manually added to this
manifest, otherwise they won't be compiled into the library, and the linker
will report `undefined symbol` errors.

### Symptom

```
lld-link: error: undefined symbol: ui_render_frame
>>> referenced by kain_runtime.lib(ui_host_adapter.obj)
```

Or other undefined symbols referencing functions that exist in `.c` files
in `runtime/native/src/` but aren't in `kain_runtime.lib`.

### Step-by-Step Fix

#### Step 1: Identify missing files

```powershell
# Check which .c files in runtime/native/src/ are NOT in the manifest
$manifest = Get-Content X:/runtime/native_core_runtime.toml
Get-ChildItem -Recurse X:/runtime/native/src/*.c | ForEach-Object {
    $name = $_.Name
    if (-not ($manifest -match [regex]::Escape($name))) {
        Write-Host "MISSING: $name"
    }
}
```

#### Step 2: Add files to the manifest

Edit `X:/runtime/native_core_runtime.toml` and add the missing files
to the `sources` list (or `windows_sources`/`linux_sources`/`macos_sources`
for platform-specific files). Keep them in alphabetical order within the
same directory group.

Example — adding UI files:

```toml
sources = [
    ...
    "native/src/ui/ui_color.c",
    "native/src/ui/ui_layout.c",
    "native/src/ui/ui_renderer.c",
    ...
]
```

#### Step 3: Regenerate the Bazel manifest data

```powershell
python tools/bazel/sync_native_runtime_builds.py
```

This regenerates `runtime/runtime_manifest_data.bzl` from the TOML file.

#### Step 4: Rebuild and sync

```powershell
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
# Then archive the .obj files (see Entry 1, Step 3)
```

### Verification

```powershell
# Check that the new .obj files are in the build output
dir Z:/_b/output-user-root/{hash}/execroot/_main/bazel-out/x64_windows-opt/bin/
    runtime/_objs/native_core_runtime_c/*.obj | Select-String "layout|color|renderer"
```

---

## Entry 3: Bazel Cache Not Picking Up Changes to C Files

### Root Cause

Bazel uses **content hashing** for its action cache — it computes a hash of
each input file's content, the compiler flags, and other action inputs. When
the hash matches a previously cached result, Bazel skips recompilation.

However, there are cases where Bazel's analysis cache or disk cache causes it
to miss file changes:

1. **Disk cache is stale** — Bazel stores action outputs in `--disk_cache`.
   If the disk cache has an entry for the old content hash of a file, it
   returns it even if the file on disk has changed (only possible if the
   content hash matches, which is extremely rare for actual edits).

2. **Analysis cache stale** — Bazel's in-memory analysis cache might not
   re-scan the file system for modified files.

3. **Uncommitted changes** — Bazel respects the filesystem state, but the
   action cache key includes file content hashes. If you've touched the file
   without changing content (reset mtime), Bazel may not recompile.

### Step-by-Step Fix

```powershell
# Option 1: Disable the disk cache for the build (easiest)
bazel build //runtime:native_core_runtime --config=dev --disk_cache=

# Option 2: Touch the file to force re-evaluation
param([string]$file)
Set-ItemProperty -Path $file -Name LastWriteTime -Value (Get-Date)

# Option 3: Clean the runtime build artifacts (nuclear option)
bazel clean --expunge
bazel build //runtime:native_core_runtime --config=dev
```

### Prevention

The `--disk_cache=` flag should be used whenever you're working on C runtime
files and need to ensure Bazel re-evaluates them. The flag disables the disk
cache for that single invocation without affecting the rest of your cache.

---

## Entry 4: `kain_sync_binary` Does Not Sync the Runtime Library

### Root Cause

Despite what `BAZEL.md` says, the `kain_sync_binary` MCP tool currently
**only syncs the Rust compiler binary** (`kain.exe`) to `~/.kain/bin/`. It
does **NOT** build or sync `kain_runtime.lib` as part of its sync process.
The runtime library must be synced separately (see Entry 1).

The legacy Python sync script (`scripts/python/kain_bazel_sync.py sync`)
DOES include a `sync_runtime_library()` function that builds and archives
the runtime, but the MCP tool appears to use a different code path.

### Step-by-Step Fix

```powershell
# Step 1: Build the runtime
bazel build //runtime:native_core_runtime --config=dev --disk_cache=

# Step 2: Find the obj directory
for /f %i in ('bazel info bazel-bin --config=dev') do set BAZEL_BIN=%i

# Step 3: Archive into kain_runtime.lib
lib.exe /NOLOGO /OUT:%USERPROFILE%\.kain\lib\kain_runtime.lib ^
    "%BAZEL_BIN%/runtime/_objs/native_core_runtime_c/*.obj"
```

### Using the Legacy Script

```powershell
python scripts/python/kain_bazel_sync.py sync --skip-build
```

This will re-archive the existing `.obj` files without rebuilding. Omit
`--skip-build` to rebuild first.

---

## Entry 5: Runtime Library Not Found by the Kain Compiler

### Root Cause

When you run `kain build --target llvm`, the compiler links the generated
LLVM IR against `kain_runtime.lib`. It finds the lib through this priority
order:

1. **`KAIN_RUNTIME_LIB_PATH`** environment variable (explicit path override)
2. **`$KAIN_HOME/lib/kain_runtime.lib`** (toolchain install, where `KAIN_HOME`
   defaults to `~/.kain/`)

If neither location has the file, the build fails with:

```
source uses runtime features but no native runtime library found.
Ensure a precompiled runtime archive exists at $KAIN_HOME/lib/kain_runtime.lib
(or set KAIN_RUNTIME_LIB_PATH).
```

### Diagnostic

```powershell
# Check the env var
echo %KAIN_RUNTIME_LIB_PATH%
# Check for the file
dir %KAIN_HOME%\lib\kain_runtime.lib
# or
dir %USERPROFILE%\.kain\lib\kain_runtime.lib
# Check what the compiler sees
kain doctor
```

### Fix

Make sure the lib exists at one of the two locations. You can also set the
env var explicitly:

```powershell
set KAIN_RUNTIME_LIB_PATH=X:\.kain\lib\kain_runtime.lib
```

---

## Entry 6: File Lock Preventing .lib Sync

### Symptom

```
Permission denied: 'X:/.kain/lib/kain_runtime.lib'
Cannot copy: file in use
```

### Root Cause

A running process (usually `kain.exe`, `ui_test.exe`, or another compiled Kain
executable) has the `.lib` file open. On Windows, static libraries are not
typically locked during execution unless you're in a debugger or the file
was memory-mapped.

### Fix

```powershell
# Kill any running Kain processes
taskkill /F /IM kain.exe
taskkill /F /IM ui_test.exe
# Or more broadly
Get-Process | Where-Object { $_.ProcessName -match 'kain|ui_test|KainUI' } | Stop-Process -Force

# Then retry the copy
```

---

## Entry 7: Bazel Server Cold or Unresponsive

### Symptom

- Commands hang for 30-90 seconds before producing output
- `kain_bazel action:'build'` times out

### Diagnosis

```powershell
kain_bazel action:'server' server_action:'status'
# or
bazel info server_pid --config=dev
```

### Fix

```powershell
# Start/warm the server
kain_bazel action:'server' server_action:'start'

# Or manually
bazel info --config=dev

# Restart if unresponsive
kain_bazel action:'server' server_action:'restart'
```

---

## Appendix: Key Files and Paths

| What | Path |
|------|------|
| Runtime C manifest | `X:/runtime/native_core_runtime.toml` |
| Generated Bazel data | `X:/runtime/runtime_manifest_data.bzl` |
| Bazel BUILD file | `X:/runtime/BUILD.bazel` |
| Runtime source files | `X:/runtime/native/src/` |
| Installed runtime lib | `X:/.kain/lib/kain_runtime.lib` |
| Compiler binary | `X:/.kain/bin/kain.exe` |
| Bazel output base | `Z:/_b/output-user-root/{hash}/execroot/_main/bazel-out/x64_windows-opt/bin/` |
| Bazel runtime objects | `.../runtime/_objs/native_core_runtime_c/` |
| Build system docs | `X:/docs/BAZEL.md` |
| Install layout module | `X:/crates/core/src/install_layout.rs` |
| Linker resolution | `X:/crates/build/src/native_link.rs` |
| Sync script (legacy) | `X:/scripts/python/kain_bazel_sync.py` |
| Manifest generator | `X:/tools/bazel/sync_native_runtime_builds.py` |

---

## Appendix: Quick Command Reference

### Full Runtime Rebuild (the One True Sequence)

```powershell
# 1. Update manifest if adding new .c files
# Edit runtime/native_core_runtime.toml
python tools/bazel/sync_native_runtime_builds.py

# 2. Build with Bazel (no disk cache)
bazel build //runtime:native_core_runtime --config=dev --disk_cache=

# 3. Archive .objs into .lib
for /f %i in ('bazel info bazel-bin --config=dev') do set BB=%i
lib.exe /NOLOGO /OUT:%USERPROFILE%\.kain\lib\kain_runtime.lib ^
    "%BB%/runtime/_objs/native_core_runtime_c/*.obj"

# 4. Verify
kain build your_file.kn --target llvm
```
