# Kain Feedback Log

## 2026-05-22 - stdlib module loading / typechecker

### StringIntMap Shadow: stdlib/collections.kn loaded twice under multi-import workspace
- Categories: correctness, regression, developer-experience
- Status: Patched
- Surface: typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: struct 'StringIntMap' shadows an existing type symbol` at `stdlib/collections.kn:88` — fires even on completely unrelated source files as long as they use `use std::runtime` alongside any second import that transitively reaches `std::collections`
- Workflow impact: Blocked the entire smoketest workspace (`kain check smoketest/src/main.kn --target llvm`) for the full session. Every file imports `std::runtime`; adding `std::alloc` or `std::text` (both of which do `use std::collections`) caused collections.kn to load twice and the typechecker to error on re-registration. Renaming `StringIntMap` to avoid the shadow did not help because the bug is the double-load, not the name. Three stdlib lanes had to be completely stubbed until you shipped the module dedup fix.
- Minimal repro: Any `.kn` file with `use std::runtime` plus `use std::alloc` (or `use std::text`) in a multi-file workspace. `kain check <file> --target llvm`
- Evidence: `error[TYPE:KAIN-TYPE-0001]: struct 'StringIntMap' shadows an existing type symbol --> stdlib/collections.kn:88:5`
- Suggested direction: Module registry dedup was the confirmed fix. Verify the dedup applies globally to the workspace import graph, not just per-file, so that any two files importing the same stdlib module through different transitive paths don't double-register its types.

---

## 2026-05-22 - typechecker / effect system

### comptime block constants are not visible in runtime function bodies
- Categories: correctness, developer-experience
- Status: Active
- Surface: typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: Unknown identifier 'SMOKE_COMPTIME_MAGIC'` when referencing a constant declared inside a `comptime:` block from a normal `pub fn` in the same file
- Workflow impact: Constants that semantically belong at comptime (e.g., version tags, surface counts, magic values) cannot be reused at runtime without duplicating them as top-level `const`. The `comptime` block in the specimen uses constants like `MYTHIC_SURFACE_COUNT` without issues, so either the specimen pattern works differently than expected or there is a scoping discrepancy between the spec and the live typechecker.
- Minimal repro: `comptime:\n    const FOO: Int = 42\npub fn bar() -> Int:\n    return FOO` — `kain check <file> --target llvm` errors on FOO
- Evidence: `error[TYPE:KAIN-TYPE-0001]: Unknown identifier 'SMOKE_COMPTIME_MAGIC' --> smoketest/src/semantics/comptime.kn:9`
- Suggested direction: Decide whether `comptime` constants should be promoted to module scope (accessible at runtime as regular consts) or remain strictly compile-time only with a clear diagnostic that says so. If they are compile-time only, the language spec/specimen should be updated to avoid implying they can be used in runtime fn bodies.

---

## 2026-05-22 - stdlib / naming

### stdlib function name collision: first_error defined in both std::diagnostics and user code
- Categories: correctness, developer-experience
- Status: Active
- Surface: stdlib / typechecker
- Symptom: `error[TYPE:KAIN-TYPE-0001]: function 'first_error' collides with an existing global from function --> stdlib/diagnostics.kn:58` when a user-authored workspace file defines a helper function named `first_error`
- Workflow impact: `first_error` is a natural helper name for any harness that tracks first-failing lane. The stdlib silently owns this name in the global namespace, causing a collision that is only surfaced at check time with no warning that the name is reserved. Required a rename to `smoke_first_error` throughout `main.kn`.
- Minimal repro: Define `fn first_error(offset: Int, result: Int) -> Int:` in any file that also imports `use std::runtime` (which triggers stdlib global loading). `kain check <file> --target llvm`
- Evidence: `error[TYPE:KAIN-TYPE-0001]: function 'first_error' collides with an existing global from function --> smoketest/src/main.kn:44, label 58:5: previous function 'first_error' is here (stdlib/diagnostics.kn:58:5)`
- Suggested direction: Either namespace stdlib globals under a module prefix at the typechecker level (so `first_error` in user code doesn't collide with `diagnostics::first_error`), or document all globally-injected stdlib names somewhere scannable so authors know which names are reserved. The current situation is silent until check time.

---

## 2026-05-22 - Windows launcher / check workflow

### Parallel `kain check` commands can race launcher cache replacement
- Categories: developer-experience, performance
- Status: Active
- Surface: tooling
- Symptom: One of four parallel `kain check <file> --target llvm` invocations failed with `Move-FileAtomically : Exception calling "Replace" with "4" argument(s): "Unable to move the replacement file to the file to be replaced."`
- Workflow impact: Parallel targeted checks are attractive for smoke triage, but the shared Windows Bazel launcher path can fail before Kain checking starts. This can look like a language failure unless rerun serially.
- Minimal repro: Launch multiple `kain check smoketest/src/... --target llvm` commands concurrently from `D:\Kain-Lang` immediately after a Bazel sync or launcher refresh.
- Evidence: Failure came from `scripts/windows/launch-bazel-cli.ps1:530` while three sibling targeted checks passed; rerunning the failed command serially passed.
- Suggested direction: Add a cross-process lock or retry loop around launcher replacement in `launch-bazel-cli.ps1`, or make `kain check` skip launcher cache replacement when the active synced binary already matches the stamp.
