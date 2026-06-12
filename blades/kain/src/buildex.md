# BuildPipeline — kainc Build Routines

> This file IS the build pipeline for the self-host compiler.
> Executed by orchestrator.kn via markscript.mks_run_with_vm().
> All > intents dispatch to IVT handlers 200-208 registered by the orchestrator.

## Metadata

| name | target | profile | optimize | lto | entry | source_root | deps | output | runtime | linker | linker_flags | cc | cc_flags | test_root | doc_root |
|------|--------|---------|----------|-----|-------|-------------|------|--------|---------|--------|--------------|----|---------|-----------|----------|
| kainc | llvm | debug | 0 | none | src/main.kn | src/ | | kainc | kain_runtime | clang | | clang | | spec/ | docs/ |

---

## BuildAll

Full compilation pipeline: typecheck → codegen → link → binary.

> compile check "src/"
> compile codegen "src/" --target llvm --profile release
> build link exe
> assert 0
> print "BUILD SUCCESS: kainc.exe produced"

---

## QuickCheck

Fast typecheck-only verification. No codegen, no linking.

> compile check "src/"
> print "QUICK CHECK: All sources typecheck"

---

## JitRun

Compile and execute in-memory via JIT.

> compile jit "src/main.kn"
> print "JIT EXECUTION: Complete"

---

## TestAll

Run the full test suite across all compiler subsystems.

> compile check "src/"
> test run "spec/parser_spec.md"
> test run "spec/typechecker_spec.md"
> test run "spec/codegen_spec.md"
> test run "spec/jit_spec.md"
> test report json
> print "TEST SUITE: Complete"

---

## TestQuick

Run fast test subset — parser + typechecker only.

> test run "spec/parser_spec.md"
> test run "spec/typechecker_spec.md"
> print "QUICK TEST: Parser + typechecker tests pass"

---

## CleanAll

Remove all build artifacts.

> spawn "rm -rf out/"
> await 0
> spawn "rm -f kainc.exe kainc.lib"
> await 0
> print "CLEAN: All artifacts removed"

---

## WatchLoop

Dev loop: rebuild on file change. Intended for use with `mks watch`.

> compile check "src/"
> compile codegen "src/" --target llvm
> build link exe
> print "WATCH: Build complete, waiting for changes..."

---

## SelfHostPhase1

Route compiler through Rust DLL bridge. Transitionary phase — Kain parser replaces Rust piece by piece.

> selfhost phase1 "crates/core"
> selfhost phase1 "crates/sys-codegen"
> print "PHASE 1: Rust DLL bridge verified"

---

## SelfHostPhase2

Pure Kain self-compilation — the OUROBOROS phase. kainc compiles kainc source entirely through Kain.

> selfhost phase2 "blades/kain/src/"
> print "PHASE 2: Self-hosting verified — kainc compiles kainc"

---

## PackageRelease

Full release pipeline: build → test → strip → ship.

> compile check "src/"
> compile codegen "src/" --target llvm --profile release
> build link exe
> test run "spec/parser_spec.md"
> test run "spec/typechecker_spec.md"
> test run "spec/codegen_spec.md"
> test report json
> spawn "strip kainc.exe"
> await 0
> assert 0
> print "RELEASE PACKAGE: kainc.exe is ready"

---

## CIBuild

CI-friendly build with JSON output for automation.

> compile check "src/"
> compile codegen "src/" --target llvm --profile release
> build link exe
> test run "spec/parser_spec.md"
> test run "spec/typechecker_spec.md"
> test report json
> print "CI BUILD: Complete"

---

## Pipeline Map

| Stage | Routine | Intents | Description |
|-------|---------|---------|-------------|
| 01 | BuildAll | compile check, compile codegen, build link, assert | Full build |
| 02 | QuickCheck | compile check | Fast typecheck-only |
| 03 | JitRun | compile jit | In-memory execution |
| 04 | TestAll | compile check, test run x4, test report | Full test suite |
| 05 | TestQuick | test run x2 | Fast test subset |
| 06 | CleanAll | spawn rm, await | Remove artifacts |
| 07 | WatchLoop | compile check, compile codegen, build link | Dev loop |
| 08 | SelfHostPhase1 | selfhost phase1 | Rust DLL bridge |
| 09 | SelfHostPhase2 | selfhost phase2 | Pure Kain ouroboros |
| 10 | PackageRelease | Full pipeline -> strip | Release binary |
| 11 | CIBuild | Full pipeline + JSON | CI automation |
