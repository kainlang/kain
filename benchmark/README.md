# Kain Multi-Language Benchmarks

This folder is the native benchmark lane for Kain LLVM against Rust LLVM, C++ on Clang, Zig `ReleaseFast`, Go `gc`, Erlang/OTP, and optional JavaScript/Node plus Python/CPython rows when a case declares them.

The contract is intentionally simple:

- Normal benchmarks in `cases/<case>/` should have dependency-free `main.kn`, `main.rs`, `main.cpp`, `main.zig`, `main.go`, `main.js`, and `main.py` sources when those languages participate in the case.
- A manifest case can explicitly declare a `languages` map to become Kain/Rust-only, Kain/Erlang-only, Kain-only, etc. Missing selected languages show as `n/a` in reports instead of failing the whole suite.
- A manifest case can also declare `telemetry.primary_metric_id` plus `telemetry.metrics[]` so reports speak in domain units like requests/s, bytes/s, lookups/s, frames/s, or sim interactions/s instead of only raw `ms`.
- Rust normally builds through direct `rustc`; dependency benchmarks can opt into Cargo with `rust_manifest`, `rust_package`, and `rust_binary`. Put an empty `[workspace]` in those per-case Cargo manifests so Cargo does not accidentally attach them to the repo root workspace.
- Zig builds through direct `zig build-exe -O ReleaseFast` over `main.zig` for dependency-free rows.
- Go normally builds through direct `go build` over `main.go`. A case can opt into a module/package build by adding `go_manifest`, `go_package`, and `go_binary` in `benchmark/benchmarks.json`.
- Erlang cases compile through `erlc` and run through `erl -noshell`; the runner resolves the official OTP `bin` directory on Windows so wrapper scripts do not fail to find `erlexec.dll`.
- Some cases build support artifacts beside the executables. `ffi_shared_call_stress` is the current example: the runner compiles the shared C helper under `benchmark/out/build/...`, copies the DLL beside each built executable, and compiles the Kain row from the case directory so the case-local `KAIN.toml` for `use c::...` resolves correctly.
- Build time is recorded separately; timed samples run the already-built executables.
- The runner prefers a release-built `kain.exe`, pins Kain benchmark links to `runtime/native_core_runtime.toml`, and passes a benchmark-native tuning profile into the Kain compiler unless you override it.
- Foreign baseline caching is now part of the normal inner loop. `python benchmark/run.py` defaults to `--baseline-mode auto`, which means: when Kain is in the selected language set, rerun Kain fresh and reuse cached non-Kain baselines from `benchmark/out/baselines/<case>/<language>.json` when the machine/tool/source/flags/workload key still matches. Use `--baseline-mode refresh-foreign` for a true cross-language refresh, `--baseline-mode reuse-foreign` to force cached foreign baselines even without Kain selected, or `--baseline-mode off` to disable caching completely.
- Every normal run writes `benchmark/latest.md` as the compact root snapshot, plus `out/reports/latest.llm.md`, a timestamped `.llm.md` report, and `out/reports/latest.json`. Stale `latest.html` is removed.
- The canonical default measurement profile is now `3` warmups plus `9` timed runs. Reports also surface `Stability Alerts` when a language lands outlier-heavy samples so future benchmark triage does not confuse machine jitter with a real frontier gap.
- Wrapper plugins now live under `benchmark/wrappers/*.json`. Use `python benchmark/run_wrapper.py --list` to discover fire-and-forget suites without touching `run.py`.
- `python benchmark/run_fast.py` or `python benchmark/run_wrapper.py fast` locks the suite to `kain,rust,cpp,erlang` and writes `benchmark/latest_fast.md` plus `benchmark/out/reports/latest_fast.llm.md` / `latest_fast.json`.
- `python benchmark/run_sim.py` or `python benchmark/run_wrapper.py sim` runs the extracted simulation pack and writes `benchmark/latest_sim.md` plus `benchmark/out/reports/latest_sim.llm.md` / `latest_sim.json`.
- The report includes a maturity/fairness note per case, and telemetry metrics when a case declares them. Some pressure tests are honest proxies until Kain exposes the matching runtime primitive directly in LLVM.
- Subprocess output is decoded as UTF-8 with replacement so the Unicode-heavy cases can report cleanly on Windows.

Current pressure cases:

- `contention_wall`: Rust and C++ use 100-thread atomic contention versus Kain `collapse`; JavaScript and Python use scalar proxy lanes so the report does not confuse runtime lock/GIL overhead with language semantics.
- `ghost_mirror`: std/socket TCP loopback payload transfer for Rust/C++/JavaScript/Python versus Kain entangle-backed world mirroring plus payload mutation.
- `evolutionary_loop`: runtime feature-detected lane choice versus Kain `converge` / `orchestrate` dispatch syntax.
- `tcp_loopback_tokio`: Kain native TCP loopback versus Rust Tokio TCP accept/connect/read/write.
- `http_server_frameworks`: Kain native localhost HTTP route handling versus Actix Web and Go `net/http`.
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
- `struct_method`: aggregate construction plus explicit score function over fields. Kain preserves the scalar loop as the converge spec and now uses a disclosed proof-backed periodic fast lane for the fixed benchmark domain.
- `option_result`: tagged Option/Result creation, branching, and unwrap.
- `async_ready_chain`: immediate ready-future async/await overhead versus Tokio current-thread ready futures.
- `ecs_archetype_query`: shatter/SoA archetype sweep for game-engine locality pressure.
- `zero_copy_binary_wire`: fixed packed wire encode/decode without per-record heap objects.
- `dynamic_vtable_thrashing`: polymorphic dispatch churn; Kain keeps an honest tagged dispatch proxy while Rust/C++/Go use real dynamic dispatch.
- `crypto_block_cipher`: dependency-free ARX-style block-mix row for integer/rotate/xor pressure.
- `ray_sphere_intersection`: fixed ray/sphere geometry kernel for floating-point dot-product, sqrt, and branchy hit testing.
- `sim_nbody_gravity`: extracted k-os-sim quantum/N-body gravity row with deterministic pairwise force accumulation and integration.
- `sim_uv_velocity_grid`: extracted k-os-sim fluid row for UV-space particle updates plus weighted velocity-grid splatting.
- `sim_cfd_pressure_projection`: extracted k-os-sim CFD row for divergence, Jacobi pressure solve, and staggered-grid gradient subtraction.
- `simd_lane_mix`: integer dot-product pressure. Rust and C++ repeat explicit AVX2-style dot passes when available; Kain routes affine power-of-two fill plus the repeated affine-bias dot shape through a proof-backed native kernel behind `converge`.
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
- Native LLVM JSON builtins now link through the Kain-owned `runtime/native/src/core/json.c` ABI. `json_manual_roundtrip` intentionally stays manual because that row measures parser/renderer and literal-schema converge collapse, not generic builtin-runtime availability.
- `http_server_concurrency` no longer fails with request-capacity exhaustion after the incoming-request auto-release fix, but it still measures the current synchronous HTTP surface against Tokio async request batching and remains a meaningful runtime gap.
- `actor_mailbox_erlang` intentionally performs one unmeasured warmup ask per worker before timing. Without that, the current Kain ask/reply path shows a one-off cold-start wobble even though the steady-state checksum is correct. <--- NOTE: this has been deprecated and actors have now moved onto a new system

Run the suite from the repo root:

```powershell
python benchmark/run.py
```

Fast reduced-language pass:

```powershell
python benchmark/run_fast.py
python benchmark/run_wrapper.py fast
```

Simulation pack:

```powershell
python benchmark/run_sim.py
python benchmark/run_wrapper.py sim
```

Useful variants:

```powershell
python benchmark/run.py --runs 9 --warmups 3
python benchmark/run.py --case ownership_memory
python benchmark/run.py --case native_map_lookup --baseline-mode auto
python benchmark/run.py --case native_map_lookup --baseline-mode refresh-foreign
python benchmark/run.py --languages kain,rust,cpp,zig,go,javascript,python
python benchmark/run.py --languages js,py --runs 1 --warmups 0
python benchmark/run.py --case branch_dispatch,call_chain,native_map_lookup,zero_copy_binary_wire --languages kain,rust,cpp,zig --runs 3 --warmups 1
python benchmark/run.py --case ecs_archetype_query,zero_copy_binary_wire,dynamic_vtable_thrashing,crypto_block_cipher,ray_sphere_intersection --languages kain,rust,cpp,go --runs 3 --warmups 1
python benchmark/run.py --case sim_nbody_gravity,sim_uv_velocity_grid,sim_cfd_pressure_projection --languages kain,rust,cpp --runs 3 --warmups 1
python benchmark/run.py --case http_server_frameworks --languages kain,rust,go --runs 5 --warmups 2
python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case tcp_loopback_tokio --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case rayon_parallel_reduce --languages kain,rust --runs 5 --warmups 2
python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 5 --warmups 2
python benchmark/run.py --case ffi_shared_call_stress --languages kain,rust,cpp --runs 5 --warmups 2
python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe
python benchmark/run.py --languages cpp --cxx D:\Kain-Lang\toolchain\llvm\bin\clang++.exe
python benchmark/run.py --case native_map_lookup --languages zig --zig C:\Users\Admin\scoop\shims\zig.exe
python benchmark/run_fast.py --case actor_mailbox_erlang --runs 3 --warmups 1
python benchmark/run_wrapper.py sim --runs 3 --warmups 1
python benchmark/run_wrapper.py --list
```

Native benchmark blade:

```powershell
.\benchmark\kain-benchmark.exe
```

The blade source lives in `benchmark/blades/kain-benchmark`. It renders a compact native UI for the case/language inventory, latest LLM report preview, report paths, quick runs, and full runs. Build it from repo root with:

```powershell
.\.agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
```

The runner prefers a direct Bazel-built release `kain.exe` to avoid the Windows PowerShell launcher `-o` forwarding ambiguity. Use `--kain-exe` or `KAIN_EXE` to pin a specific compiler. The C++ lane defaults to the repo-bundled `toolchain/llvm/bin/clang++.exe`; use `--cxx` or `CXX` only when you intentionally want a different `clang++`/`g++`-style driver. The Zig lane defaults to `zig` from PATH; use `--zig` or `ZIG` only when you intentionally want a different Zig toolchain. The Go lane defaults to `go` from PATH; use `--go` or `GO` only when you intentionally want a different toolchain. Erlang auto-detects from the official OTP `bin` directory first; use `--erl` and `--erlc` only when you intentionally want a different `erl`/`erlc` pair. Kain benchmark builds set `KAIN_RUNTIME_MANIFEST_PATH` to the lean core runtime manifest; use the broad runtime manifest only for app/vendor/UI lanes. Use `--kain-native-profile`, `--kain-native-opt-level`, `--kain-native-target-cpu`, and `--kain-native-debug-info` only if you are intentionally changing the native benchmark tuning. The preferred extension point for new suite categories is `benchmark/wrappers/*.json`: add a new wrapper config with `before_args` and/or `after_args`, then launch it with `python benchmark/run_wrapper.py <name>` instead of splicing more one-off flow into `run.py`. For day-to-day Kain optimization work, leave `--baseline-mode` at `auto`; for nightly or release-grade cross-language truth runs, use `--baseline-mode refresh-foreign`.
