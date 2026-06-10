# BAZEL — Build System Survival Guide

This document is your reference when the Kain build system breaks,
binaries are stale, or you need to understand how Bazel fits into
the development workflow.

______________________________________________________________________

## Table of Contents

1. Architecture Overview
1. Bazel Server Lifecycle
1. Building the Compiler
1. The Sync Pipeline
1. Binary Freshness
1. Common Failure Modes
1. Fast Paths

______________________________________________________________________

## 1. Architecture Overview

The Kain repo uses **Bazel** as its build system. The build produces:

| Binary | Source | Bazel Target | Installed To |
|--------|--------|-------------|-------------|
| kain.exe | crates/cli/ | //:kain | ~/.kain/bin/kain.exe |
| kn.exe | crates/cli/ | //:kn | ~/.kain/bin/kn.exe |
| blade.exe | crates/cli/ | //:blade | ~/.kain/bin/blade.exe |

**The build flow:**
Source code -> Bazel compile -> Bazel output base -> Sync -> ~/.kain/bin/

The **kain_sync_binary** tool copies from the Bazel output base to the
active ~/.kain/bin/ directory. This is the canonical way to update
the binary the agent uses.

______________________________________________________________________

## 2. Bazel Server Lifecycle

Bazel runs a persistent Java server. Cold start = **30-90 seconds**.
Keep it warm for fast builds.

### Commands

bazel info server_pid --config=dev - Check server status
bazel info output_base --config=dev - Show output base
bazel info --config=dev - Show full config
bazel build //:kain --config=dev - Warm start (first build = cold)

### Server Management Tools

kain_bazel action:'server' server_action:'status' - Check alive
kain_bazel action:'server' server_action:'start' - Warm up
kain_bazel action:'server' server_action:'stop' - Shutdown
kain_bazel action:'server' server_action:'restart' - Cycle

### Windows Batch Scripts

tools/bazel/bazel_on.bat - Start server
tools/bazel/bazel_off.bat - Stop server

### Output Base Location

Configured in X:/.bazelrc: startup --output_user_root=Z:/\_b/output-user-root

The actual output base is: Z:/\_b/output-user-root/{hash}/
Inside: execroot/\_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe

______________________________________________________________________

## 3. Building the Compiler

### Quick Build

bazel build //:kain --config=dev

### Key Targets

- //:kain - Compiler CLI (--config=dev or --config=release)
- //:kn - Launcher binary
- //:blade - Blade runner
- //runtime:all - Native runtime C libs
- //:developer_smoke_tests - Sanity tests
- //:crate_tests - All Rust crate tests

### Bazel Configs

- --config=dev - Optimized (opt) - Daily development (fast compiler binary)
- --config=debug - Debug (dbg) - Stepping through the Rust compiler, panic backtraces
- --config=release - Optimized (opt) - Release builds, benchmarks
- --config=speed - Optimized + thin LTO - Max binary perf (slower link)

### Build Failures

- Rust failures -> crates/ syntax/types
- C failures -> runtime/native/src/
- Bazel config -> MODULE.bazel or BUILD.bazel
- bazel clean --expunge -> full rebuild (rarely needed)
- bazel shutdown -> restart server (fixes memory issues)
- cargo build -p kain -> bypass Bazel (Rust-only test)

______________________________________________________________________

## 4. The Sync Pipeline

### Using kain_sync_binary (recommended)

kain_sync_binary

This will:

1. Check the Bazel server is alive
1. Find the output base via bazel info output_base
1. Look for existing kain.exe in bazel-out/
1. Build //:kain --config=dev if no binary exists
1. Copy to ~/.kain/bin/kain.exe (with .bak backup)
1. Also sync kn.exe if found
1. Verify with kain doctor

### Using /kain-sync (interactive)

Same thing as an interactive command: /kain-sync

### Manual sync

bazel info output_base --config=dev
copy /Y "Z:\_b\\output-user-root{hash}\\execroot_main\\bazel-out\\x64_windows-dbg\\bin\\crates\\cli\\kain.exe" "%USERPROFILE%.kain\\bin\\kain.exe"
kain doctor

### Sync stamp

Tracked at ~/.kain/state/state/kain_sync_stamp.json

______________________________________________________________________

## 5. Binary Freshness

### Checking Freshness

kain doctor

Key fields: Binary Path, Managed Sync Binary, Managed Sync Binary Match,
Managed Sync Repo Status.

### Using kain_bazel binary_age

kain_bazel action:'binary_age'

### Using kain_bazel freshness

kain_bazel action:'freshness'

### Stale Binary Symptoms

- kain doctor shows old Built At (UTC) timestamp
- Compiler features missing or behaving differently
- Managed Sync Repo Status: drift
- Error messages mention unsupported flags or features

______________________________________________________________________

## 6. Common Failure Modes

### F1: Bazel Server is Cold

Symptom: Commands hang 30-90s.
Fix: kain_bazel action:'server' server_action:'start'

### F2: Binary Not Found in Output Base

Symptom: kain_sync_binary says "No existing binary found".
Fix: kain_bazel action:'build' target:'//:kain' then kain_sync_binary

### F3: Build Fails

Check:

1. Server alive? -> kain_bazel action:'server' server_action:'status'
1. Try directly: bazel build //:kain --config=dev
1. kain doctor for toolchain issues
1. cargo build -p kain to isolate from Bazel

### F4: Permission / File Lock

Symptom: Cannot copy binary, file in use.
Fix: The running kain.exe is locked. Kill the process first.

### F5: Sync Stamp Drift

Symptom: kain doctor shows Managed Sync Repo Status: drift.
Fix: Run kain_sync_binary to rebuild and re-sync.

### F6: PowerShell $\_ Errors

Symptom: $\_FullName : The term '$\_FullName' is not recognized
Cause: Old sync tool had broken PowerShell escaping. Fixed in
current kain-sync.ts which uses Node.js fs instead of PowerShell.

______________________________________________________________________

## 7. Fast Paths

### "Binary is stale, fix it"

kain_bazel action:'build' target:'//:kain'
kain_sync_binary

### "Just check if everything is fresh"

kain_bazel action:'freshness'

### "Bazel is slow/cold"

kain_bazel action:'server' server_action:'start'

### "I don't know what's wrong"

kain doctor
kain_bazel action:'freshness'

- If binary is stale: kain_sync_binary

______________________________________________________________________

## Appendix: Key Paths

| What | Path |
|------|------|
| Repo root | X:/ |
| Bazel user root | Z:/\_b/output-user-root |
| Bazel rc | X:/.bazelrc |
| Kain home | X:/.kain |
| Kain bin | X:/.kain/bin/kain.exe |
| Kain config | X:/.kain/config.toml |
| Sync stamp | X:/.kain/state/state/kain_sync_stamp.json |
| Managed binary dir | X:/.kain/state/bin/ |
| Bazel on script | X:/tools/bazel/bazel_on.bat |
| Bazel off script | X:/tools/bazel/bazel_off.bat |
| Runtime manifest | X:/runtime/native_core_runtime.toml |
| LLVM toolchain | X:/toolchain/llvm/ |
