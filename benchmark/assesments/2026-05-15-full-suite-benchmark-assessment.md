# 2026-05-15 Full Benchmark Suite Assessment

## Executive Summary

- Full suite status: `PASS`
- Canonical reports:
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
- Languages reflected correctly in the final report: `kain`, `rust`, `javascript`, `python`
- Winners by case:
  - `kain`: `2/14` (`contention_wall`, `ghost_mirror`)
  - `rust`: `12/14`
- Geomean of median runtimes across all 14 cases:
  - `rust`: `20.689 ms`
  - `kain`: `26.833 ms`
  - `javascript`: `72.775 ms`
  - `python`: `371.719 ms`
- Relative geomean:
  - Kain is `1.297x` Rust's geomean median
  - Kain is `0.369x` JavaScript's geomean median

Kain only loses to JavaScript in two cases in the clean run:

| case | kain median ms | javascript median ms | js faster by |
| --- | ---: | ---: | ---: |
| `string_ops` | 150.708 | 50.691 | `2.973x` |
| `struct_method` | 68.631 | 49.773 | `1.379x` |

`option_result` is nearly tied and still Kain-favored:

| case | kain median ms | javascript median ms |
| --- | ---: | ---: |
| `option_result` | 45.047 | 46.830 |

## What I Fixed To Get A Clean Suite

The suite did not start from a trustworthy state. I had to repair both native runtime compilation and a Windows runtime-cache flake before the final run could complete with `overall_ok = true`.

Relevant source changes:

- `runtime/native/src/core/kain_runtime_services.c`
  - Removed a dead vendor-table dependency and refreshed probing to use the live native net/process tables only.
- `runtime/native/src/core/kain_runtime_native_stdlib.c`
  - Replaced invalid POSIX `stat` usage on Windows with `GetFileAttributesExA` metadata handling.
- `runtime/native/src/platform/win32/kain_runtime_win32_shared.c`
  - Implemented the missing Win32 frame timer helpers needed by the app host link step.
- `crates/cli/src/main.rs`
  - Hardened the native runtime object cache on Windows:
    - clear stale object-slot artifacts before recompilation
    - remove lingering `.tmp` object files
    - retry transient `permission denied` failures from clang object-file renames

After those fixes, `python benchmark/run.py --timeout 300` exited `0` and produced a fully populated report.

## Result Snapshot

Selected clean-run medians:

| case | kain | rust | javascript | python | winner |
| --- | ---: | ---: | ---: | ---: | --- |
| `contention_wall` | 12.291 | 1645.041 | 159.774 | 7143.591 | `kain` |
| `ghost_mirror` | 13.466 | 51.019 | 334.897 | 206.008 | `kain` |
| `evolutionary_loop` | 28.969 | 24.047 | 89.793 | 1018.141 | `rust` |
| `string_ops` | 150.708 | 9.141 | 50.691 | 326.041 | `rust` |
| `struct_method` | 68.631 | 12.170 | 49.773 | 532.557 | `rust` |
| `option_result` | 45.047 | 9.545 | 46.830 | 154.398 | `rust` |

Interpretation:

- Kain already has two strong semantic wins where the language/runtime model is not pretending to be a clone of ordinary heap-heavy object code.
- Rust still owns the broad geomean because the current LLVM lane boxes too many values and leaves too much work in the runtime.
- JavaScript only wins where V8 is allowed to stay on a specialized fast path while Kain lowers simple source patterns into heap traffic and repeated runtime calls.

## Why Kain Lags JavaScript In The Current Outliers

### 1. `string_ops` is dominated by runtime string churn, not raw arithmetic

The benchmark source is tiny and scalar:

- `benchmark/cases/string_ops/main.kn`

But the generated LLVM path is not tiny:

- `benchmark/out/build/string_ops/kain/string_ops.ll`

The hot loop repeatedly does all of the following inside `starts_with_at` and `find_substring`:

- calls `@len(i8*)` over and over
- calls `@char_at(i8*, i64)` for each character comparison
- calls `@deep_eq(i8*, i8*)` on one-character strings
- emits `rc_retain` / `rc_release` traffic around those values
- re-runs const-string init guards (`@__kain_init_const_STRING_TEXT`, `@__kain_init_const_STRING_NEEDLE`, `@__kain_init_const_STRING_TAIL`) before loads from immutable globals

The runtime implementation makes the cost concrete:

- `runtime/native/src/core/kain_runtime_core.c:269`
  - `string_new` allocates and copies
- `runtime/native/src/core/kain_runtime_core.c:667`
  - `char_at` returns a freshly allocated one-character string
- `runtime/native/src/core/kain_runtime_core.c:752`
  - `len` on strings calls `strlen`
- `runtime/native/src/core/kain_runtime_core.c:1445`
  - `deep_eq` dispatches by runtime type-tag and then uses `strcmp` for strings

So Kain is turning one code-unit compare into multiple runtime calls, heap allocations, reference-count bumps, and C-library string scans. JavaScript is not "magically better at strings" here; V8 is just not paying that tax for this shape of loop.

### 2. `struct_method` heap-allocates a tiny POD record on every iteration

The benchmark source:

- `benchmark/cases/struct_method/main.kn`

The generated LLVM:

- `benchmark/out/build/struct_method/kain/struct_method.ll:10479`

`make_pair` currently lowers to:

- compute `%BenchPair` size
- call `@KAIN_alloc`
- fill fields in heap memory
- return `%BenchPair*`

`main` then calls `make_pair` and `score_pair` once per iteration:

- `benchmark/out/build/struct_method/kain/struct_method.ll:10542`
- `benchmark/out/build/struct_method/kain/struct_method.ll:10546`

There is no corresponding `rc_release` in that hot loop. For this benchmark shape, Kain is paying heap-allocation cost on a two-field aggregate that should never have left registers or, at worst, the stack.

This is exactly the kind of pattern V8's escape analysis is designed to eliminate when the object does not escape, and exactly the kind of pattern LLVM's scalar-replacement passes can optimize when the frontend exposes aggregates as stack values instead of opaque heap objects.

### 3. `option_result` is still boxed, it just happens to be closer to JavaScript already

The benchmark source:

- `benchmark/cases/option_result/main.kn`

The generated LLVM:

- `benchmark/out/build/option_result/kain/option_result.ll:10478`

Current lowering behavior:

- `maybe_value` allocates a tagged box for `None` or `Some(Int)`
- `parse_value` allocates a tagged box for `Err(String)` or `Ok(Int)`
- `Err("skip")` also calls `string_new` before it gets boxed
- `main` branches on tags and then releases the boxed values

This is visible in the LLVM backend itself:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:963`
  - tagged boxes are built with `KAIN_alloc`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:1031`
  - value-to-tagged-box lowering is still heap-based
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:6259`
  - `is_err` is tag inspection over boxed data

This case is almost tied with JavaScript even with the boxing overhead, which means it is a high-value optimization target: a scalar-tag lowering would likely move it from "almost tied" to clearly ahead.

## Why JavaScript Can Beat This Lowering

The repo evidence above is the primary cause. The external engine evidence explains why V8 can exploit the gap:

- V8 tracks object shapes and uses hidden classes / fast properties to make stable property loads cheap.
  - https://v8.dev/docs/hidden-classes
  - https://v8.dev/blog/fast-properties
- V8's optimizing tiers collect runtime feedback and then generate specialized machine code for the shapes and types actually seen.
  - https://v8.dev/blog/maglev
- V8 escape analysis can avoid explicit heap allocation when a created object does not escape the function.
  - https://v8.dev/blog/disabling-escape-analysis
- LLVM's `sroa` pass only helps when aggregates are exposed as allocas/SSA-friendly memory, not when the frontend immediately hides them behind `KAIN_alloc`.
  - https://llvm.org/docs/Passes.html
  - https://llvm.org/doxygen/SROA_8h.html

My inference from those sources plus the current Kain IR is:

- V8 is free to specialize the `struct_method` object pattern into something much closer to scalar field movement.
- Kain is preventing LLVM from doing equivalent work because the frontend has already committed to heap identity.
- V8 can keep string loops on specialized internal string paths, while Kain currently routes string indexing and equality through heap strings and generic runtime equality.

## How To Get Kain Past Rust

### Priority 1. Stop boxing non-escaping POD structs and tuples

Target:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:5427`

Current problem:

- `Expr::Struct` emits `KAIN_alloc` unconditionally.

Recommended direction:

- introduce escape analysis in the LLVM frontend
- lower non-escaping POD structs/tuples by value
- prefer SSA or entry-block `alloca` for aggregates that stay intra-function
- only box when identity/address stability actually escapes through storage, returns, trait objects, actor mailboxes, or foreign/runtime boundaries

Why this matters:

- it directly attacks `struct_method`
- it also removes allocator pressure and refcount traffic in many non-benchmark workloads
- it finally gives LLVM SROA and mem2reg a chance to do real work

### Priority 2. Lower small `Option` / `Result` shapes to scalar tags plus payloads

Target:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:963`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:1031`

Recommended direction:

- for register-sized payloads, keep `tag` and `payload` in SSA
- only materialize boxed tagged objects when the value escapes
- special-case `Option<Int>`, `Result<Int, &'static str>`, and similar "thin payload" shapes first
- intern static error strings instead of rebuilding them with `string_new`

Why this matters:

- `option_result` is already near parity with Node
- removing box traffic here is a direct path to pushing Kain clearly ahead of JavaScript on this case
- the same machinery will benefit actor/result-heavy code elsewhere

### Priority 3. Rebuild string lowering around value semantics, not heap-string semantics

Targets:

- `runtime/native/src/core/kain_runtime_core.c:269`
- `runtime/native/src/core/kain_runtime_core.c:667`
- `runtime/native/src/core/kain_runtime_core.c:752`
- `runtime/native/src/core/kain_runtime_core.c:1445`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs:3155`

Recommended direction:

- make immutable string literals load as compile-time data + known length
- hoist or eliminate runtime const-init calls for string literals in hot loops
- represent strings as a fat pointer / slice / `(ptr,len)` style value in the LLVM lane
- make `len` an O(1) field load, not `strlen`
- make `char_at` return a code unit or small scalar for ASCII fast paths, not a heap `String`
- add a dedicated char/byte equality path so one-character compare does not route through `deep_eq`
- specialize ASCII substring search with a compact fast path before falling back to generic string logic

Why this matters:

- `string_ops` is the clearest "Kain lost to its own lowering" case in the whole suite
- the current design is paying allocation plus `strlen` plus `strcmp` for work that could be register math and byte loads

### Priority 4. Lean into the semantics that already beat Rust

The current wins are not accidents:

- `contention_wall`
- `ghost_mirror`

Those cases are Kain wins because the semantics are different, not because the generic backend is already universally stronger. The path past Rust is not "be more JavaScript" or "be more C-like everywhere." It is:

- remove needless generic heap identity where it is not semantically required
- keep exploiting Kain-native semantics like compiler-owned mirror/state/authority paths where they remove synchronization or transport cost entirely

If Kain extends those zero-copy / ownership-aware semantics while also eliminating the current boxing tax, it can beat Rust in more than the two current headline cases.

## Formal Verification Backing

I ran two solver-backed checks to validate the semantics of the most obvious de-boxing directions:

1. `z3/reports/20260515T114406Z-benchmark_struct_method_scalarization_equivalence_clean.json`
   - result: `unsat`
   - meaning: there is no 64-bit counterexample where the scalarized `score_pair(make_pair(seed))` formula differs from the current aggregate formula

2. `z3/reports/20260515T114406Z-benchmark_option_result_scalarization_equivalence_clean.json`
   - result: `unsat`
   - meaning: there is no 64-bit counterexample where the current boxed benchmark logic differs from a scalar tag+payload form for the `maybe_value` / `parse_value` branches used in this benchmark

These proofs do not claim "the optimized implementation is faster." They do prove that two major optimization directions discussed above are semantics-preserving for the benchmark logic we are targeting.

## Bottom Line

Kain is not losing to JavaScript because JavaScript is fundamentally a better fit for these workloads. Kain is losing where the current LLVM lowering is still pretending tiny structs, tagged unions, and one-character strings need full heap identity and generic runtime services.

The shortest path to move Kain ahead of JavaScript in the current outliers, and closer to or past Rust overall, is:

1. scalarize non-escaping structs and tuples
2. de-box small `Option` / `Result` values
3. replace heap-string char/len/equality paths with direct value-level string operations

Once those three land, this benchmark suite should look materially different.
