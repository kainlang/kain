# Kain Multi-Language Benchmarks

This folder is the native benchmark lane for Kain LLVM against Rust LLVM, C++ on Clang, Erlang/OTP, and optional JavaScript/Node plus Python/CPython rows when a case declares them.

The contract is intentionally simple:

- Normal benchmarks in `cases/<case>/` should have dependency-free `main.kn`, `main.rs`, `main.cpp`, `main.js`, and `main.py` sources when those languages participate in the case.
- A manifest case can explicitly declare a `languages` map to become Kain/Rust-only, Kain/Erlang-only, Kain-only, etc. Missing selected languages show as `n/a` in reports instead of failing the whole suite.
- Rust normally builds through direct `rustc`; dependency benchmarks can opt into Cargo with `rust_manifest`, `rust_package`, and `rust_binary`. Put an empty `[workspace]` in those per-case Cargo manifests so Cargo does not accidentally attach them to the repo root workspace.
- Erlang cases compile through `erlc` and run through `erl -noshell`; the runner resolves the official OTP `bin` directory on Windows so wrapper scripts do not fail to find `erlexec.dll`.
- Some cases build support artifacts beside the executables. `ffi_shared_call_stress` is the current example: the runner compiles the shared C helper under `benchmark/out/build/...`, copies the DLL beside each built executable, and compiles the Kain row from the case directory so the case-local `KAIN.toml` for `use c::...` resolves correctly.
- Build time is recorded separately; timed samples run the already-built executables.
- The runner prefers a release-built `kain.exe`, pins Kain benchmark links to `runtime/native_core_runtime.toml`, and passes a benchmark-native tuning profile into the Kain compiler unless you override it.
- Every run writes `out/reports/latest.llm.md`, a timestamped `.llm.md` report, and `out/reports/latest.json`. Stale `latest.html` is removed.
- The report includes a maturity/fairness note per case. Some pressure tests are honest proxies until Kain exposes the matching runtime primitive directly in LLVM.
- Subprocess output is decoded as UTF-8 with replacement so the Unicode-heavy cases can report cleanly on Windows.

Current pressure cases:

- `contention_wall`: Rust and C++ use 100-thread atomic contention versus Kain `collapse`; JavaScript and Python use scalar proxy lanes so the report does not confuse runtime lock/GIL overhead with language semantics.
- `ghost_mirror`: std/socket TCP loopback payload transfer for Rust/C++/JavaScript/Python versus Kain entangle-backed world mirroring plus payload mutation.
- `evolutionary_loop`: runtime feature-detected lane choice versus Kain `converge` / `orchestrate` dispatch syntax.
- `tcp_loopback_tokio`: Kain native TCP loopback versus Rust Tokio TCP accept/connect/read/write.
- `http_server_concurrency`: Kain native local HTTP route handling versus Rust Tokio request batches. Kain is still the synchronous semantic proxy lane here, but the old request-slot exhaustion failure is fixed and the case now completes repeatedly.
- `actor_mailbox_erlang`: Kain native LLVM actor ask/reply fanout versus direct Erlang mailbox request/reply over four long-lived workers.
- `rayon_parallel_reduce`: Rayon parallel integer reduction versus Kain scalar proxy, reserved as the future Kain data-parallel fanout slot.

Current basic language-edge cases:

- `branch_dispatch`: branch-heavy scalar dispatch.
- `call_chain`: small-function call graph in a hot loop.
- `memory_stream`: sequential helper-owned buffer write/read.
- `alloc_churn`: many small allocation/lifetime cycles.
- `scalar_mix`: hot scalar loop with top-level const expressions and a checksum guard.
- `recursive_sum`: recursive call-stack lowering in a tight loop.
- `string_ops`: repeated substring search plus string length/indexing over fixed ASCII strings.
- `array_scan`: nested fixed-array indexing and weighted accumulation.
- `struct_method`: aggregate construction plus explicit score function over fields.
- `option_result`: tagged Option/Result creation, branching, and unwrap.
- `async_ready_chain`: immediate ready-future async/await overhead versus Tokio current-thread ready futures.
- `simd_lane_mix`: integer dot-product pressure. Rust and C++ use explicit AVX2 when available; Kain is the scalar SIMD proxy lane today.
- `native_map_lookup`: fixed-key hash-map lookup pressure over string keys.
- `json_manual_roundtrip`: manual parse plus serialization over two small JSON payload shapes.
- `filesystem_stream`: repeated temp-file write, streaming copy, and readback.
- `process_stdio_loop`: repeated shell/process spawn plus stdout capture.
- `unicode_string_heavy`: mixed UTF-8 substring search over multilingual text and emoji.
- `allocator_large_object_churn`: variable-size large-buffer allocation/touch/readback/release cycles.
- `gpu_graphics_submit`: Kain-only raw native graphics session, buffer, pipeline, draw-command, and present submission pressure.
- `ffi_shared_call_stress`: repeated tiny shared-library calls through the normal suite rather than the dedicated FFI probe lane.

Known Kain gaps exposed while shaping these cases:

- Scalar `match` in the standalone branch hot loop built but trapped at runtime, so `branch_dispatch` currently uses equivalent `if` dispatch.
- Method receiver field access in the struct benchmark hit a native codegen gap, so `struct_method` uses `score_pair(pair)` instead of `pair.score()`.
- Dynamic value capture into a Kain `async` ready future failed checksum during the async benchmark spike; `async_ready_chain` currently uses the known-good `return async 2` shape and should stay that way until async capture lowering is fixed.
- Native LLVM JSON builtins currently fail to link in this checkout (`json_parse`, `json_object_new`, `json_object_set`, `json_string`, `json_get_*`), so `json_manual_roundtrip` stays manual and keeps that linker gap visible as telemetry instead of hiding it behind a vendor JSON crate.
- `http_server_concurrency` no longer fails with request-capacity exhaustion after the incoming-request auto-release fix, but it still measures the current synchronous HTTP surface against Tokio async request batching and remains a meaningful runtime gap.
- `actor_mailbox_erlang` intentionally performs one unmeasured warmup ask per worker before timing. Without that, the current Kain ask/reply path shows a one-off cold-start wobble even though the steady-state checksum is correct. <--- NOTE: this has been deprecated and actors have now moved onto a new system

Run the suite from the repo root:

```powershell
python benchmark/run.py
```

Useful variants:

```powershell
python benchmark/run.py --runs 9 --warmups 2
python benchmark/run.py --case ownership_memory
python benchmark/run.py --languages kain,rust,cpp,javascript,python
python benchmark/run.py --languages js,py --runs 1 --warmups 0
python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case tcp_loopback_tokio --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case rayon_parallel_reduce --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 5 --warmups 2
python benchmark/run.py --case ffi_shared_call_stress --languages kain,rust,cpp --runs 5 --warmups 2
python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe
python benchmark/run.py --languages cpp --cxx D:\Kain-Lang\toolchain\llvm\bin\clang++.exe
```

Native benchmark blade:

```powershell
.\benchmark\kain-benchmark.exe
```

The blade source lives in `benchmark/blades/kain-benchmark`. It renders a compact native UI for the case/language inventory, latest LLM report preview, report paths, quick runs, and full runs. Build it from repo root with:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
```

The runner prefers a direct Bazel-built release `kain.exe` to avoid the Windows PowerShell launcher `-o` forwarding ambiguity. Use `--kain-exe` or `KAIN_EXE` to pin a specific compiler. The C++ lane defaults to the repo-bundled `toolchain/llvm/bin/clang++.exe`; use `--cxx` or `CXX` only when you intentionally want a different `clang++`/`g++`-style driver. Erlang auto-detects from the official OTP `bin` directory first; use `--erl` and `--erlc` only when you intentionally want a different `erl`/`erlc` pair. Kain benchmark builds set `KAIN_RUNTIME_MANIFEST_PATH` to the lean core runtime manifest; use the broad runtime manifest only for app/vendor/UI lanes. Use `--kain-native-profile`, `--kain-native-opt-level`, `--kain-native-target-cpu`, and `--kain-native-debug-info` only if you are intentionally changing the native benchmark tuning.
