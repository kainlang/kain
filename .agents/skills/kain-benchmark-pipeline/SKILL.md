---
name: kain-benchmark-pipeline
description: Use when adding, changing, running, or reviewing the multi-language benchmark lane under benchmark/, including Kain/Rust/C++/Erlang/JavaScript/Python cases, benchmark/benchmarks.json, benchmark/run.py, LLM-readable reports, the benchmark blade console, and fairness/maturity notes for Kain pressure tests.
---

# Kain Benchmark Pipeline

## Contract

- `benchmark/benchmarks.json` is the source of truth for the suite, cases, source paths, maturity labels, language notes, and fairness notes.
- Every normal benchmark case should have dependency-free paired source files:
  - `benchmark/cases/<case>/main.kn`
  - `benchmark/cases/<case>/main.rs`
  - `benchmark/cases/<case>/main.cpp`
  - `benchmark/cases/<case>/main.js`
  - `benchmark/cases/<case>/main.py`
- Case programs should not use external language dependencies unless the benchmark is explicitly about an ecosystem runtime such as Tokio or Rayon.
- Case-specific language subsets are declared with a `languages` map in `benchmark/benchmarks.json`. The runner intersects the requested languages with the case's declared languages and renders unselected/missing global columns as `n/a`.
- Rust normally uses direct `rustc`. Rust dependency cases opt into Cargo by adding `rust_manifest`, and may set `rust_package` / `rust_binary`. Per-case Cargo manifests inside this repo need an empty `[workspace]` table so Cargo does not treat them as orphan members of the root workspace.
- Erlang dependency-free actor/runtime cases compile through `erlc` and run through `erl -noshell`. On Windows, prefer the official OTP `bin` directory over PATH wrapper shims so `erlc.exe` can find `erlexec.dll`.
- Some cases require runner-built support artifacts. `ffi_shared_call_stress` compiles `benchmark/ffi_boundary/native/ffi_boundary.c` into a DLL plus import library under `benchmark/out/build/...`, copies the DLL beside each executable, and must compile the Kain row from the case directory so the case-local `KAIN.toml` for `use c::...` resolves.
- The Python runner may use the standard library for orchestration, timing, JSON, and Markdown report output.
- Generated outputs belong under `benchmark/out/` and should stay ignored except `benchmark/out/.gitignore`.
- The native benchmark console lives under `benchmark/blades/kain-benchmark`, and the user-facing executable is `benchmark/kain-benchmark.exe`.

## Runner

- Main command: `python benchmark/run.py`
- Focus one case: `python benchmark/run.py --case contention_wall --runs 3 --warmups 1`
- Run a language subset: `python benchmark/run.py --languages js,py --runs 1 --warmups 0`
- Run a Kain/Rust-only dependency case: `python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 5 --warmups 2`
- Run the Kain/Erlang mailbox case: `python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 5 --warmups 2`
- Run the shared-library FFI stress row: `python benchmark/run.py --case ffi_shared_call_stress --languages kain,rust,cpp --runs 5 --warmups 2`
- Pin Kain compiler: `python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe`
- Pin C++ compiler: `python benchmark/run.py --languages cpp --cxx D:\Kain-Lang\toolchain\llvm\bin\clang++.exe`
- Pin Erlang tools: `python benchmark/run.py --case actor_mailbox_erlang --languages erlang --erl "C:\Program Files\Erlang OTP\bin\erl.exe" --erlc "C:\Program Files\Erlang OTP\bin\erlc.exe"`
- The runner prefers a direct Bazel-built release `kain.exe` because the Windows PowerShell launcher can mis-handle forwarded `-o`. The C++ lane defaults to the repo-bundled `toolchain/llvm/bin/clang++.exe` and expects a `clang++`/`g++`-style CLI when you override it. Erlang auto-detects from the official OTP `bin` directory first; override `--erl` / `--erlc` only when you intentionally want a different pair.
- Benchmark-native tuning defaults to `KAIN_NATIVE_PROFILE=benchmark-release` with `opt-level=3`, `target-cpu=native`, no debug info, and `KAIN_RUNTIME_MANIFEST_PATH=runtime/native_core_runtime.toml` unless you intentionally override it in code for an app/vendor benchmark.
- Subprocess stdout/stderr is decoded as UTF-8 with replacement so Unicode-heavy case output does not crash report generation on Windows.
- Reports are written to:
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
  - timestamped `benchmark/out/reports/<stamp>.llm.md`
  - timestamped `benchmark/out/reports/<stamp>.json`
- HTML is no longer a report format. If `benchmark/out/reports/latest.html` exists after a run, treat that as stale-output cleanup debt.
- Dedicated FFI boundary lane: `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`
  - This is a specialized benchmark outside `benchmarks.json`; it exists to compare `llvm_pure`, direct LLVM object/shared-library C FFI, `interpret_pure`, the interpreter/live bridge path, `zig_pure`, and `zig_c_object` in one place.
  - Reports land in `benchmark/out/reports/ffi_boundary_latest.llm.md`, `ffi_boundary_latest.json`, and timestamped `*.ffi_boundary.*` siblings.
  - The runner writes its own `benchmark/ffi_boundary/KAIN.toml`, compiles `native/ffi_boundary.c` into both object and shared forms under `benchmark/out/build/ffi_boundary/native/`, and keeps runtime tuning pinned to the normal benchmark-release native profile.
  - Pass `--zig` or set `ZIG` to pin a Zig toolchain. On the current Windows Zig 0.17 dev build, `-femit-bin=path` produces a runnable PE without a `.exe` suffix; the runner handles that.
  - If `kain.exe <file>.kn -t llvm` starts failing with undefined `use c::...` symbols on this lane, inspect the direct CLI compile prep in `crates/cli/src/main.rs`: the LLVM/C path must generate `.kain/cache/c_ffi/.../*.kn` bindings before frontend import resolution, not after.

## Blade Console

- Source: `benchmark/blades/kain-benchmark/src/*.kn`
- Executable: `benchmark/kain-benchmark.exe`
- Build proof from repo root:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
```

- Validate UI with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH` under `benchmark/blades/kain-benchmark/.kain/run/` and `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES`.
- The Win32/GL BMP readback may appear flipped/mirrored in viewers; still assert non-empty screenshot size and inspect enough to prove the UI surface rendered.
- `KAIN_BENCH_AUTORUN=quick` runs the scalar multi-language smoke at startup. Buttons use the same process runner path.

## Case Design

- Keep benchmark constants local inside `main` or helper functions unless the case is explicitly exercising top-level const behavior.
- Include deterministic checksum/exit-code validation so benchmarked work cannot disappear silently.
- If Kain does not yet expose the exact runtime primitive needed, keep the case but mark `maturity` as `proxy`, `semantic-proxy`, or `dispatch-skeleton` in `benchmarks.json`.
- For Kain/Rust-only comparisons such as Tokio/Rayon, declare only those two languages in the case `languages` map; do not add empty C++/JS/Python placeholders.
- For Kain/Erlang actor comparisons, declare only those two languages in the case `languages` map; do not fake C++/Rust/JS/Python actor rows.
- Never claim a proxy is a completed win. Use `fairness_note` and `language_notes` to explain semantic gaps.
- JavaScript and Python lanes should mirror algorithmic shape where possible, but avoid importing npm/pip dependencies or measuring unrelated framework overhead.
- For Kain low-level memory cases, `alloc(count, "T")` and `realloc_mem(ptr, count, "T", ...)` use element counts, not byte counts. Do not pass `sizeof_type("T")` for a single-cell allocation unless you intentionally want `sizeof(T)` elements.
- If a Kain case uses a case-local `KAIN.toml` plus `use c::...`, make the runner compile that row from the case directory or you will get false import-resolution failures from nearest-manifest lookup.

## Current Pressure Cases

- `contention_wall`: Rust and C++ use 100 OS threads and an atomic counter; Kain currently uses a zero-lock `collapse` proxy; JavaScript/Python use scalar proxies to avoid worker/GIL overhead dominating the comparison.
- `ghost_mirror`: Rust/C++/JavaScript/Python use TCP loopback for a 1 MiB payload; Kain uses entangle-backed in-process world mirroring plus helper-owned payload mutation.
- `evolutionary_loop`: Rust/C++/JavaScript/Python use runtime feature detection or equivalent branch dispatch; Kain uses `converge`/`orchestrate` dispatch syntax as the future autotuning slot.
- `ownership_memory`: direct `collapse`/`observe`/`decay` smoke against ordinary boxed/object ownership lanes.
- `tcp_loopback_tokio`: Kain native TCP loopback versus Rust Tokio TCP. This is an implemented networking comparison, but the fairness note must keep saying Kain's current native TCP facade is synchronous around readiness helpers while Rust uses Tokio async IO.
- `http_server_concurrency`: Kain native local HTTP route handling versus Tokio request batches. The request-slot exhaustion bug is fixed, so this row should stay green while still carrying the fairness note that Kain is measuring the synchronous semantic surface against Tokio async request batching.
- `actor_mailbox_erlang`: Kain native LLVM actor ask/reply fanout versus Erlang process mailbox request/reply. Both rows intentionally perform one unmeasured warmup ask per worker so the report reflects steady-state mailbox traffic rather than startup effects.
  The Kain reply leg now uses the dedicated native fast path `kain_actor_reply_port_send(...)` rather than generic mailbox enqueue/dequeue; if performance regresses again, inspect the actor runtime/codegen seam before changing the case.
- `rayon_parallel_reduce`: Rayon parallel iterators versus Kain scalar proxy. Keep `maturity` as `parallel-proxy` until Kain LLVM has proven user-level data-parallel fanout.

## Current Basic Edge Cases

- `branch_dispatch`: scalar branch-heavy dispatch. It uses `if` today because scalar `match` in the standalone hot loop built but trapped at runtime.
- `call_chain`: small function graph in a hot loop.
- `memory_stream`: sequential buffer write/read through Kain helper-owned memory versus ordinary language arrays/vectors.
- `alloc_churn`: many small allocation/write/read/lifetime-end cycles.
- `struct_method`: aggregate construction plus explicit `score_pair(pair)` field access. Avoid receiver method field access until that native codegen gap is fixed.
- `option_result`: Option/Result tagged value creation, branching, and unwrap paths.
- `async_ready_chain`: ready-future async/await overhead versus Tokio current-thread ready futures. Keep the Kain source on the known-good `return async 2` style until dynamic async value capture lowering is repaired; a dynamic-capture version compiled but failed checksum in the benchmark spike.
- `simd_lane_mix`: integer dot product. Rust and C++ use explicit AVX2 when available; Kain remains the scalar SIMD proxy lane until first-class SIMD intrinsics land in the benchmark surface.
- `native_map_lookup`: fixed-key string-hash lookup pressure over a small native map.
- `json_manual_roundtrip`: manual parse plus serialization over two small JSON payload shapes. Keep the case manual until the native LLVM JSON builtins stop failing to link in this checkout.
- `filesystem_stream`: temp-file write, streaming copy, readback, and cleanup over a generated text payload.
- `process_stdio_loop`: repeated `cmd.exe` launch plus stdout capture. Treat it as a Windows-first host-substrate case, not a pure language throughput case.
- `unicode_string_heavy`: UTF-8 substring search over multilingual text and emoji.
- `allocator_large_object_churn`: variable-size large-buffer allocation/touch/readback/release cycles.
- `gpu_graphics_submit`: Kain-only raw native graphics submission path. Keep it Kain-only until the suite grows a comparable bare-metal Rust/C++ graphics lane instead of a framework benchmark.
- `ffi_shared_call_stress`: repeated tiny shared-library calls inside the main suite. Keep the dedicated `benchmark/ffi_boundary` lane for deeper ABI-tax and Zig-neighborhood questions.
- `scalar_mix`: top-level const lowering and a checksum guard.
- `recursive_sum`: recursion and call-stack lowering in a tight loop.
- `string_ops`: ASCII substring search plus string length/indexing over fixed ASCII strings.
  - As of `2026-05-15`, the case intentionally removes dead `% MODULUS` math and uses a boolean branch toggle instead of `% 2` parity math, but keeps substring search on the general path. The specialized win we kept is in the LLVM lowering, not in benchmark-only hand-tuned substring kernels.
  - The LLVM fast path still depends on string-aware const metadata, entry-cached string lengths, direct bytewise `char_at(...) == char_at(...)` lowering, and borrowed internal string params that skip caller retain/callee release churn. If `string_ops` regresses, inspect `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` before blaming the C runtime.
- `array_scan`: fixed-array indexing and weighted accumulation.
- `ffi_boundary` is the dedicated ABI-tax probe, not a fairness-suite case. It is intentionally target-focused so we can answer questions like "how expensive is direct LLVM object linking vs the interpreter bridge?" or "is Kain LLVM in Zig/C territory?" without polluting the multi-language pressure suite.

## Validation

- `python -m py_compile benchmark/run.py`
- Syntax-check all JS/Python case files and compile the C++ lane.
- `python benchmark/run.py --languages cpp --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --case scalar_mix --languages kain,rust,cpp,javascript,python --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 1 --warmups 0 --timeout 600`
- `python benchmark/run.py --case tcp_loopback_tokio --languages kain,rust --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case rayon_parallel_reduce --languages kain,rust --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 1 --warmups 0 --timeout 900`
  If this row fails with a Kain build error mentioning `generated/native_runtime/cache/.../*.obj.tmp` and `Access is denied`, rerun once after the cache quiesces before treating it as a semantic actor regression.
- `python benchmark/run.py --case ffi_shared_call_stress --languages kain,rust,cpp --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case http_server_concurrency --languages kain,rust --runs 1 --warmups 0 --timeout 900`
- `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`
- Build `benchmark/kain-benchmark.exe` with the blade compile helper and capture a non-empty native UI screenshot.
- Inspect `benchmark/out/reports/latest.llm.md` and `latest.json` before summarizing results.
- Inspect `benchmark/out/reports/ffi_boundary_latest.llm.md` and `ffi_boundary_latest.json` before summarizing FFI-boundary claims.
