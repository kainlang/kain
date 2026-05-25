---
name: test-crash-forensics
description: Use when a native Kain executable or blade crashes, hangs, or becomes frame-count-sensitive and you need a machine-level answer from crash dumps, LLDB backtraces, disassembly, emitted LLVM IR, and frame or host reports. Use this to produce root-cause evidence and deterministic repros, not to implement the underlying runtime, UI, graphics, or package subsystem directly.
---

# Test Crash Forensics

Use this skill for post-crash evidence gathering and deterministic native repro work. It owns the path from dump to first Kain-owned frame, crash PC, disassembly window, and correlated emitted-IR evidence.

## Trigger Surface

- A native Kain executable crashed and Windows produced a dump under `%LOCALAPPDATA%\CrashDumps`.
- A blade or presenter dies after a repeatable frame count.
- You need the first Kain-owned frame, the crash PC, or a disassembly and IR window instead of broad repo spelunking.
- A task is about explaining a native failure, not implementing the fix yet.

## Ownership Boundary

- This skill owns dump collection, LLDB and `llvm-objdump` correlation, emitted `.ll` inspection, and frame or host report analysis.
- Once the root cause is localized, hand the implementation fix to the owning skill such as `runtime-core`, `runtime-gpu`, `package-kaintana`, or `lang-ui`.
- Do not use this skill for speculative subsystem edits before you have evidence.
- Do not let crash forensics turn into generic benchmark or attrition work; it is the machine-level failure-investigation lane.

## Source Of Truth

- `tools/crash-forensics/analyze_native_crash.ps1`: primary correlation script.
- `%LOCALAPPDATA%\CrashDumps`: Windows dump location.
- Blade-local emitted LLVM IR under `.kain/out/.../*.ll`.
- Blade-local frame and host reports under `.kain/run/*frame*.txt` and `.kain/run/*host*.txt`.
- Repro launchers such as `blades/kaintana-test/run.ps1` and `blades/pong/run.ps1`.

## Working Rules

1. Use matching exe, dump, LLVM IR, frame report, and host report from the same crash family.
2. Increase frame pressure intentionally instead of waiting blindly for a slow repro.
3. The first Kain-owned frame matters more than the full backtrace wall of host frames.
4. If the crash is deterministic at one frame count, treat non-entry `alloca` or runaway recursion as prime suspects until evidence rules them out.
5. Keep durable signatures written down. The known historical `0xc00000fd` Win32/GL modal-menu stack overflow should not be rediscovered from scratch.

## Validation

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\crash-forensics\analyze_native_crash.ps1 `
  -ExePath blades\kaintana-test\kaintana-test.exe `
  -DumpPath $env:LOCALAPPDATA\CrashDumps\kaintana-test.exe.12345.dmp `
  -LlvmPath blades\kaintana-test\.kain\out\kaintana-test\kaintana-test.ll `
  -FrameReportPath blades\kaintana-test\.kain\run\kaintana_test_desktop_frame.txt `
  -HostReportPath blades\kaintana-test\.kain\run\kaintana_test_desktop_host.txt
blades\kaintana-test\run.ps1 -FrameBudget 300
blades\pong\run.ps1 -FrameBudget 300
```

## Known Durable Finding

The first proven native UI crash family on this host was not Kaintana. Old runtime-owned Win32/GL workbench binaries consistently crashed with `0xc00000fd` stack overflow inside GDI text rendering after re-entering the archived modal-menu path around `TrackPopupMenuEx(...)`. If that signature appears again, suspect archived Win32/GL menu reentrancy before blaming blade-owned desktop or Vulkan presenters.
