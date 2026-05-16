---
name: native-crash-forensics
description: Use when a native Kain executable crashes or becomes frame-count-sensitive and you need a machine-level answer from Windows crash dumps, LLDB backtraces, objdump disassembly, emitted LLVM IR, and blade frame/host reports rather than broad repo spelunking.
---

# Native Crash Forensics

Use this skill in `D:\Kain-Lang` when:

- a blade-root executable crashes and Windows produced a dump under `%LOCALAPPDATA%\CrashDumps`
- a native UI or graphics app dies after a repeatable frame count
- you need the first Kain-owned frame, crash PC, assembly window, or emitted LLVM `alloca` evidence

## Primary Tool

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\crash-forensics\analyze_native_crash.ps1 `
  -ExePath D:\Kain-Lang\blades\kaintana-test\kaintana-test.exe `
  -DumpPath $env:LOCALAPPDATA\CrashDumps\kaintana-test.exe.12345.dmp `
  -LlvmPath D:\Kain-Lang\blades\kaintana-test\.kain\out\kaintana-test\kaintana-test.ll `
  -FrameReportPath D:\Kain-Lang\blades\kaintana-test\.kain\run\kaintana_test_desktop_frame.txt `
  -HostReportPath D:\Kain-Lang\blades\kaintana-test\.kain\run\kaintana_test_desktop_host.txt
```

The script writes:

- a summary report
- a raw LLDB log
- a raw `llvm-objdump` log

Default output root is beside the exe under `.kain/forensics/`.

## What It Correlates

- Windows minidump stop reason and exception code from LLDB
- first app-owned frame from the backtrace
- crash PC disassembly from `llvm-objdump`
- non-entry `alloca` hits from the emitted `.ll`
- last completed frame evidence from app/host reports

## Repro Workflow

1. Increase frame pressure instead of waiting blindly:
   - `blades/kaintana-test/run.ps1 -FrameBudget <N>`
   - `blades/pong/run.ps1 -FrameBudget <N>`
2. If the app crashes, collect the new dump from `%LOCALAPPDATA%\CrashDumps`.
3. Run `analyze_native_crash.ps1` with the matching exe plus `.ll` and report paths.
4. If the crash is deterministic at one frame count, treat non-entry `alloca` as a prime suspect until the IR scan says otherwise.

## Known Durable Finding

The first proven native UI crash family on this host was not Kaintana. Old runtime-owned Win32/GL workbench binaries consistently crashed with `0xc00000fd` stack overflow inside GDI text rendering after re-entering the archived modal-menu path around `TrackPopupMenuEx(...)`. If you see that signature again, suspect archived Win32/GL menu reentrancy before blaming the new blade-owned desktop or Vulkan presenters.
