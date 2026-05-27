---
name: test-bench
description: >-
  Use when running, extending, debugging, validating, or reviewing Kain's benchmark pipeline under `benchmark/`, including benchmark manifests, wrappers, reports, cache behavior, history, and case-specific runner behavior.
---

# Kain Benchmark Pipeline

## Contract

- `benchmark/benchmarks.json` is the source of truth for case ids, source paths, language subsets, maturity labels, fairness notes, and per-language caveats.
- `benchmark/benchmarks.json` is also the source of truth for case telemetry. Prefer manifest-declared `telemetry.primary_metric_id` plus `telemetry.metrics[]` over hardcoded report branches when a case needs requests/s, bytes/s, lookups/s, frames/s, or other domain units.
- Normal dependency-free cases should use:
  - `benchmark/cases/<case>/main.kn`
  - `benchmark/cases/<case>/main.rs`
  - `benchmark/cases/<case>/main.cpp`
  - `benchmark/cases/<case>/main.zig`
  - `benchmark/cases/<case>/main.go`
  - `benchmark/cases/<case>/main.js`
  - `benchmark/cases/<case>/main.py`
- Case-specific language subsets belong in a `languages` map inside `benchmark/benchmarks.json`. The runner intersects requested languages with that map and renders missing columns as `n/a`.
- Kain cases may set `\"kain_runtime_manifest\": \"runtime/<manifest>.toml\"` when they need a narrower native runtime bundle than `runtime/native_core_runtime.toml`.
- Rust normally builds with direct `rustc`. Dependency rows opt into Cargo with `rust_manifest`, and may also set `rust_package` / `rust_binary`. Per-case Cargo manifests inside this repo need an empty `[workspace]` table.
- Zig normally builds with direct `zig build-exe -O ReleaseFast` over `main.zig`.
- Go normally builds with direct `go build` over `main.go`. A case can opt into a module/package-aware build with `go_manifest`, `go_package`, and `go_binary`.
- Erlang dependency-free actor/runtime rows compile with `erlc` and run with `erl -noshell`. On Windows, prefer the official OTP `bin` directory over PATH wrapper shims so `erlc.exe` can find `erlexec.dll`.
- Dependency-free cases should stay dependency-free unless the whole point of the row is an ecosystem runtime or framework such as Tokio, Rayon, Actix, or Go `net/http`.
- Generated outputs belong under `benchmark/out/`.
- The native benchmark console lives under `benchmark/blades/kain-benchmark`, and the operator-facing executable is `benchmark/kain-benchmark.exe`.

## Runner

- Main command: `python benchmark/run.py`
- Fast reduced-language command: `python benchmark/run_fast.py`
- Generic wrapper command: `python benchmark/run_wrapper.py <wrapper>`
- List wrapper plugins: `python benchmark/run_wrapper.py --list`
- Focus one case: `python benchmark/run.py --case contention_wall --runs 3 --warmups 1`
- Run a language subset: `python benchmark/run.py --languages js,py --runs 1 --warmups 0`
- Run the first Zig expansion pack: `python benchmark/run.py --case branch_dispatch,call_chain,native_map_lookup,zero_copy_binary_wire --languages kain,rust,cpp,zig --runs 3 --warmups 1`
- Run the Go-backed compute pack: `python benchmark/run.py --case ecs_archetype_query,zero_copy_binary_wire,dynamic_vtable_thrashing,crypto_block_cipher,ray_sphere_intersection --languages kain,rust,cpp,go --runs 3 --warmups 1`
- Run the k-os-sim extraction pack: `python benchmark/run.py --case sim_nbody_gravity,sim_uv_velocity_grid,sim_cfd_pressure_projection --languages kain,rust,cpp --runs 3 --warmups 1`
- Run the simulation wrapper plugin: `python benchmark/run_wrapper.py sim --runs 3 --warmups 1`
- Run the framework HTTP row: `python benchmark/run.py --case http_server_frameworks --languages kain,rust,go --runs 5 --warmups 2`
- Run a Kain/Rust-only dependency row: `python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 5 --warmups 2`
- Run the Kain/Erlang mailbox row: `python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 5 --warmups 2`
- Run the Kain/Erlang semantic-flex row: `python benchmark/run.py --case quantumerlang --languages kain,erlang --runs 3 --warmups 1`
- Run the Kain-only crucible: `python benchmark/run.py --case semantic_singularity_crucible --languages kain --runs 1 --warmups 0 --timeout 900`
- Run the FFI stress row inside the main suite: `python benchmark/run.py --case ffi_shared_call_stress --languages kain,rust,cpp --runs 5 --warmups 2`
- Run several Kain-only ablations in one report: `python benchmark/run.py --case semantic_singularity,semantic_singularity_no_actor,semantic_singularity_no_entangle,semantic_singularity_no_patch,semantic_singularity_shatter_only,semantic_singularity_actor_only,semantic_singularity_converge_only --languages kain --runs 1 --warmups 0 --timeout 900`
- Pin Kain compiler: `python benchmark/run.py --kain-exe target\\release\\kain.exe`
- Pin C++ compiler: `python benchmark/run.py --languages cpp --cxx toolchain\\llvm\\bin\\clang++.exe`
- Pin Zig compiler: `python benchmark/run.py --case native_map_lookup --languages zig --zig C:\\Users\\Admin\\scoop\\shims\\zig.exe`
- Pin Go compiler: `python benchmark/run.py --case http_server_frameworks --languages go --go C:\\Program Files\\Go\\bin\\go.exe`
- Pin Erlang tools: `python benchmark/run.py --case actor_mailbox_erlang --languages erlang --erl "C:\\Program Files\\Erlang OTP\\bin\\erl.exe" --erlc "C:\\Program Files\\Erlang OTP\\bin\\erlc.exe"`
- The runner prefers a direct Bazel-built release `kain.exe` because the Windows PowerShell launcher can mis-handle forwarded `-o`.
- The C++ lane defaults to the repo-bundled `toolchain/llvm/bin/clang++.exe`.
- The Zig lane defaults to `zig` from PATH.
- The Go lane defaults to `go` from PATH.
- Erlang auto-detects from the official OTP `bin` directory first.
- Benchmark-native tuning defaults to `KAIN_NATIVE_PROFILE=benchmark-release`, `opt-level=3`, `target-cpu=native`, no debug info, and `KAIN_RUNTIME_MANIFEST_PATH=runtime/native_core_runtime.toml` unless a case overrides it.
- `python benchmark/run.py` now defaults to `--baseline-mode auto`: if Kain is part of the selected language set, rerun Kain fresh and reuse matching non-Kain baselines from `benchmark/out/baselines/<case>/<language>.json`. Use `--baseline-mode refresh-foreign` for a true full refresh, `--baseline-mode reuse-foreign` to reuse baselines even on foreign-only runs, or `--baseline-mode off` to disable the cache.
- Manifest cases may set `"default_enabled": false` to keep experimental or temporarily failing rows out of no-`--case` standard runs while preserving focused `--case <id>` execution. This selection-only flag is intentionally ignored by foreign baseline cache keys.
- If repeated Windows native builds keep printing `Native runtime cache: 0 reused, 36 compiled` or `0 reused, 37 compiled`, inspect `crates/cli/src/main.rs::parse_native_runtime_depfile(...)` before blaming the compiler or linker. The healthy steady-state line is full object reuse for the active native runtime manifest.
- Reports are written to:
  - `benchmark/latest.md`
  - `benchmark/latest_fast.md` and `benchmark/out/reports/latest_fast.llm.md` / `latest_fast.json` when the `fast` wrapper is used
  - `benchmark/latest_sim.md` and `benchmark/out/reports/latest_sim.llm.md` / `latest_sim.json` when the `sim` wrapper is used
  - `benchmark/latest_gpu.md` and `benchmark/out/reports/latest_gpu.llm.md` / `latest_gpu.json` when the dedicated GPU lane is used
  - `benchmark/latest_wasm.md` and `benchmark/out/reports/wasm_latest.llm.md` / `wasm_latest.json` when the dedicated WASM lane is used
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
  - timestamped `benchmark/out/reports/<stamp>.llm.md`
  - timestamped `benchmark/out/reports/<stamp>.json`
- `benchmark/latest.md`, `benchmark/latest_fast.md`, `benchmark/latest_sim.md`, and `benchmark/latest_gpu.md` are intentionally minimal LLM-facing snapshots. If a case declares telemetry metrics, the snapshot also includes a compact telemetry table keyed by the case primary metric.
- Root snapshots and full reports now include `baseline_mode` plus baseline-cache hit/refresh counts so you can tell at a glance whether the fast foreign-baseline path actually engaged.
- `python benchmark/run.py` now also persists every suite run to `benchmark/out/history/benchmark_history.sqlite3` by default using stdlib `sqlite3`. Pass `--history-db off` to disable or `--history-db <path>` to route history elsewhere.
- The history database records one row per run plus normalized case/language/metric rows, including Kain medians, build timings, primary telemetry metric values, cache status, toolchain/git metadata, and report artifact paths.
- Full and minimal reports now compare current Kain results against the most recent prior *comparable* run: same suite, same `latest_stem`, same machine fingerprint, same selected case set/language set, and same warmup/run counts. The report surfaces per-case `delta_ms`, `delta_pct`, trend, and a regression alert when slowdown crosses the configured threshold.
- The SQLite history lane is meant to complement timestamped JSON reports, not replace them. JSON remains the human/LLM artifact; SQLite is the regression/trend warehouse.
- HTML is no longer a report format. If `benchmark/out/reports/latest.html` appears, treat it as stale-output cleanup debt.
- The preferred extension point for new categories is `benchmark/wrappers/*.json`, not new hardcoded branches in `run.py`. Wrapper configs are data-driven plugins that can inject `before_args` and `after_args` around user-supplied CLI flags.

## Cases V2 Router Lane

- `benchmark/cases_v2/` is now the canonical Kain-native growth lane for new benchmark work when the row does not need the legacy cross-language manifest runner.
- The v2 lane is a single Kain router executable rooted at `benchmark/cases_v2/.telemetryrouter/router.kn`. It emits one markdown summary, one JSON summary, and one JSON track file per case.
- Build and run ownership lives in `benchmark/build.kn`. The root executable is `benchmark/kain-benchmark-v2.exe`.
- For Python interop work, distinguish three different comparison surfaces:
  - `python_shared_buffer` / `python_shared_image` / `python_shared_tensor` measure full first-class contract adoption and metadata materialization.
  - `py_buffer_view` measures the lightweight borrowed-buffer lane that should be compared directly against PyO3 `PyBuffer<T>`.
  - `py_call_raw_*` rows measure raw callable crossing costs and should be kept separate from full contract or pykain workflow rows.
- When adding a Python interop benchmark, prefer a pair of rows: one Kain-native raw lane and one PyO3 row with both a `scoped` ceiling run and a `per_boundary` run. That gives both the “best plausible ceiling” and the “apples-to-apples bridge shape” view.
- Default v2 outputs are:
  - `benchmark/latest_v2.md`
  - `benchmark/out/reports/latest_v2.json`
  - `benchmark/out/reports/v2_tracks/<case>.json`
- The router currently imports pack modules such as `classic_core.kn`, `classic_systems.kn`, `classic_core3d.kn`, `python_interop.kn`, and `rage_runtime.kn`. Treat each file as a benchmark pack that can expose multiple rows from one authored Kain file.

### When To Use V2

- Use `cases_v2` first for new Kain-only rows, runtime probes, stdlib probes, semantic fused rows, or grouped benchmark packs where multiple related cases belong in one file.
- Stay in the legacy `benchmark/cases/<case>/...` plus `benchmark/catalog/benchmarks.main.json` flow when the row is meant to compare Kain against Rust/C++/Go/Zig/JS/Python/Erlang or otherwise belongs in the cross-language main suite.
- Keep dedicated special lanes in their existing homes: GPU under `benchmark/lanes/gpu/`, WASM under `benchmark/lanes/wasm/`, FFI boundary under `benchmark/lanes/ffi_boundary/`.

### How To Run V2

- Preferred compile-and-run command from repo root: `kain run X:\benchmark --target llvm --json`
- The build graph entry is `benchmark/build.kn`, so `kain run X:\benchmark --target llvm` executes the v2 router lane rather than the legacy Python suite.
- The router accepts these environment variables:
  - `KAIN_BENCH_V2_FILTER`: comma-separated case ids or group ids
  - `KAIN_BENCH_V2_MARKDOWN`: override markdown output path
  - `KAIN_BENCH_V2_JSON`: override JSON summary path
  - `KAIN_BENCH_V2_TRACK_ROOT`: override per-case track directory
  - `KAIN_BENCH_V2_PASSES`
  - `KAIN_BENCH_V2_WARMUPS`
  - `KAIN_BENCH_V2_AMPLIFY`
- PowerShell example for a focused run:

```powershell
$env:KAIN_BENCH_V2_FILTER="rage,rage_realloc_growth"
$env:KAIN_BENCH_V2_MARKDOWN="X:\benchmark\latest_v2_rage.md"
$env:KAIN_BENCH_V2_JSON="X:\benchmark\out\reports\latest_v2_rage.json"
$env:KAIN_BENCH_V2_TRACK_ROOT="X:\benchmark\out\reports\v2_tracks_rage"
kain run X:\benchmark --target llvm --json
```

- If you need to rerun without recompiling, execute the root artifact directly after a successful build:

```powershell
$env:KAIN_BENCH_V2_FILTER="classic_core"
X:\benchmark\kain-benchmark-v2.exe
```

- The router prints one `[bench-v2]` line per case and returns non-zero when any checksum fails or when the filter selects no cases.

### Adding A New V2 Pack

- Add a new Kain file under `benchmark/cases_v2/`, for example `benchmark/cases_v2/my_runtime_pack.kn`.
- Follow the pack pattern already used by `classic_core.kn` and `rage_runtime.kn`: expose pack-level helpers so the router can enumerate cases and evaluate them by id.
- A pack should normally export:
  - `<pack>_case_count()`
  - `<pack>_case_id(index: Int)`
  - `<pack>_case_group(index: Int)`
  - `<pack>_case_title(index: Int)`
  - `<pack>_case_iterations(index: Int)`
  - `<pack>_case_expected_checksum(index: Int)`
  - `<pack>_case_checksum(case_id: String, iterations: Int, amplify: Int, modulus: Int)`
- Keep benchmark constants and expected checksums inside the pack unless the row truly belongs in the router core.
- Every row still needs a deterministic checksum guard. The v2 router treats checksum mismatch as a benchmark failure, not a soft warning.

### Wiring A New V2 Pack

- Import the new pack symbols in `benchmark/cases_v2/.telemetryrouter/router.kn`.
- Extend `run_case_checksum(...)` so the router asks the new pack for a checksum before falling through to built-in rows.
- Add a new enumeration loop in `main()` modeled after the existing `classic_*`, `python_interop`, or `rage_runtime` loops so selected cases get executed, printed, and written to track files.
- Register the new file in `benchmark/build.kn`:
  - add it as an `.input(...)` to `check-llvm`
  - add it as an `.input(...)` to `root-executable`
  - add it as an `.input(...)` to `telemetry-v2`
- If the pack becomes part of the standard v2 suite, also add any needed `use <pack>::...` imports near the top of the router.

### V2 Workflow Notes

- `benchmark/README.md` now explicitly says future benchmark growth should prefer `cases_v2` because one file can hold multiple rows cleanly.
- V2 is intentionally telemetry-rich but Kain-only. Use it for fast local runtime iteration before graduating a row into the cross-language catalog.
- The current router filter matches exact case ids or exact group ids from `KAIN_BENCH_V2_FILTER`; it is not substring matching.
- In this checkout, string/path env overrides are working, but the numeric knobs `KAIN_BENCH_V2_PASSES`, `KAIN_BENCH_V2_WARMUPS`, and `KAIN_BENCH_V2_AMPLIFY` appear to be ignored at runtime even though the router reads them. Treat that as a live caveat until proven fixed.
- Per-case track files under `benchmark/out/reports/v2_tracks/` are the easiest machine-readable artifact for focused before/after comparisons on one row.
- When a v2 checksum changes unexpectedly after telemetry or scoring edits, inspect the authoritative router-side track first at `benchmark/cases_v2/.telemetryrouter/out/reports/v2_tracks/<case>.json`. The root `benchmark/out/reports/...` mirrors are useful, but the telemetryrouter track shows the live checksum delta and current case telemetry closest to execution.

## Dedicated WASM Parity Lane

- Command: `python benchmark/run_wasm.py --warmups 1 --runs 3 --timeout 300`
- Direct runner: `python benchmark/wasm/run.py ...`
- This specialized lane sits outside `benchmarks.json` under `benchmark/wasm/`.
- `benchmark/wasm/wasm_cases.json` is the source of truth for Kain/Rust wasm parity cases. Case assets live under `benchmark/wasm/cases/<case_id>/`.
- The runner builds Kain with `-t wasm`, builds Rust with `rustc --target wasm32-unknown-unknown`, validates both modules through Node's `WebAssembly.Module`, executes the same export, and requires the normalized `result/stdout` transcript bytes to match exactly.
- Reports land in `benchmark/latest_wasm.md`, `benchmark/out/reports/wasm_latest.llm.md`, `benchmark/out/reports/wasm_latest.json`, and timestamped `wasm_<stamp>` siblings.

## Dedicated FFI Boundary Lane

- Command: `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`
- This specialized lane sits outside `benchmarks.json`.
- It compares `llvm_pure`, direct LLVM object/shared-library C FFI, `interpret_pure`, the interpreter/live bridge path, `zig_pure`, and `zig_c_object`.
- Reports land in `benchmark/out/reports/ffi_boundary_latest.llm.md`, `ffi_boundary_latest.json`, and timestamped siblings.

## Dedicated GPU / SPIR-V Lane

- Command: `python benchmark/run_gpu.py --warmups 1 --runs 5 --timeout 300`
- Alias: `python benchmark/run_spirv.py ...`
- Wrapper plugin: `python benchmark/run_wrapper.py gpu ...`
- This specialized lane sits outside `benchmarks.json` under `benchmark/gpu/`.
- `benchmark/gpu/gpu_cases.json` is the source of truth for shader/GPU cases. Case assets live under `benchmark/gpu/cases/<case_id>/`.
- The runner builds Kain shaders with `-t spirv`, can compile GLSL reference shaders with `glslangValidator`, validates modules with `spirv-val --target-env vulkan1.3` when available, and profiles bytecode density with `spirv-dis` when available or the binary SPIR-V instruction stream as fallback.
- Dispatcher executables should write optional hardware sidecars to `benchmark/out/build/gpu/<case_id>/<language>/<language>.telemetry.json`. The runner sets `KAIN_GPU_CASE_ID`, `KAIN_GPU_LANGUAGE`, `KAIN_GPU_SHADER_SPV`, `KAIN_GPU_ENTRY_POINT`, `KAIN_GPU_WORK_ITEMS`, `KAIN_GPU_WIDTH`, and `KAIN_GPU_TELEMETRY_PATH` for C++/Rust/Kain hosts, then merges any manifest `runner_env` overrides on top.
- `benchmark/gpu/cases/vec3_storage_copy` is the first runtime proof row: Kain SPIR-V and a GLSL/C++ reference SPIR-V run through the same C++ Vulkan dispatcher, descriptor layout, buffers, timestamp query, readback verifier, and sidecar schema.
- `benchmark/gpu/cases/semantic_ping_pong` is the first golden showcase row: a branchy, loop-heavy Vec4 rebound kernel bounces through 12 Vulkan ping-pong rounds, then proves the final state against a CPU oracle while emitting rounds/max-error/register/binary/timestamp telemetry.
- Reports land in `benchmark/latest_gpu.md`, `benchmark/out/reports/latest_gpu.llm.md`, `benchmark/out/reports/latest_gpu.json`, and timestamped `.gpu.*` siblings.

## Blade Console

- Source: `benchmark/blades/kain-benchmark/src/*.kn`
- Executable: `benchmark/kain-benchmark.exe`
- Build proof from repo root:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
```

- Validate the blade with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH` and `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES`.
- `KAIN_BENCH_AUTORUN=quick` runs the scalar smoke at startup.
- The catalog is intentionally curated now: it shows featured case families and the real total case count, not the full manifest dump.

## Case Design

- Keep benchmark constants local inside `main` or helper functions unless the case is explicitly exercising top-level const behavior.
- Every row needs a deterministic checksum/exit-code guard so benchmarked work cannot disappear silently.
- If Kain does not yet expose the exact runtime primitive needed, keep the row but mark `maturity` honestly as `proxy`, `semantic-proxy`, `dispatch-proxy`, `parallel-proxy`, or `simd-proxy`.
- Never claim a proxy is a completed win. Put the caveat in `fairness_note` and `language_notes`.
- Kain/Rust-only and Kain/Erlang-only comparisons should declare only those languages in the case `languages` map.
- JavaScript and Python lanes should mirror the algorithmic shape where possible without dragging in package-manager dependency noise.
- For Kain low-level memory cases, `alloc(count, "T")` and `realloc_mem(ptr, count, "T", ...)` use element counts, not byte counts.
- If a Kain case uses a case-local `KAIN.toml` plus `use c::...`, make the runner compile that row from the case directory or nearest-manifest lookup will lie.
- Kain benchmark `.kn` rows that touch the root stdlib domains should import them explicitly with `use std::<domain>`. Do not rely on deleted `stdlib/native/*` ambient names surviving root-stdlib cleanup.

## Notable Rows

- `semantic_singularity` and its `semantic_singularity_*` siblings are the Kain-only fused semantics pressure vessel and ablation matrix.
- `actor_mailbox_erlang` is the truth row for Kain actor latency against Erlang mailboxes.
- `async_ready_chain` is the honest ready-future runtime tax row against Tokio.
- `native_map_lookup` is the clean native map/data-structure pressure row. It now has a direct Zig lane for StringHashMap pressure against Kain's native map.
- `http_server_concurrency` is the synchronous native Kain HTTP surface against Tokio request batching.
- `http_server_frameworks` is the synchronous native Kain HTTP surface against Actix Web and Go `net/http`.
- `ecs_archetype_query` is the SoA/game-engine locality row.
- `zero_copy_binary_wire` is the fixed packed-wire decode row. It now has a direct Zig lane for packed-layout comparisons.
- `dynamic_vtable_thrashing` is real Rust/C++/Go dynamic dispatch against an honest Kain dispatch proxy.
- `crypto_block_cipher` is the ARX-style rotate/xor/add bit-twiddling row.
- `ray_sphere_intersection` is the fixed 3D geometry kernel. Kain keeps the scalar ray/sphere loop as the `converge` spec and uses a proof-backed finite-domain period reducer on LLVM through `abi_ray_sphere_intersection_checksum(...)`; treat the win as semantic closed-domain math collapse, not generic scalar float parity.
- `sim_nbody_gravity`, `sim_uv_velocity_grid`, and `sim_cfd_pressure_projection` are the extracted k-os-sim simulation pack. Keep them Kain/Rust/C++ only; Go is intentionally not part of the sim category.
- `json_manual_roundtrip` keeps the manual parser/renderer as a converge spec, but Kain LLVM now uses a proof-backed period-14 literal-schema native reducer. Treat it as the first JSON/string collapse win, not a generic JSON builtin parity claim.
- `filesystem_stream` is the filesystem-heavy runtime row.
- `process_stdio_loop` is Windows-first host/process tax, not a pure language-throughput row.
- `async_ready_chain` uses the slim `runtime/native_async_benchmark_runtime.toml` lane. In this checkout that manifest must keep `attrition.c` and `process_system.c` because `stdlib_abi.c` and `attrition.c` still share process/async attrition snapshot hooks.

## Validation

- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `python benchmark/run.py --case scalar_mix,branch_dispatch,native_map_lookup --runs 1 --warmups 0 --latest-stem latest_cache_probe --minimal-name latest_cache_probe.md`
- Run that cache probe twice in a row. The second pass should show foreign baseline hits in the report and complete materially faster than the first.
- `py -3 tools/bazel/sync_native_runtime_builds.py --check` if runtime manifests changed.
- `python benchmark/run.py --case scalar_mix --languages kain,rust,cpp,javascript,python --runs 1 --warmups 0 --timeout 300`
- `python benchmark/run.py --case contention_wall,branch_dispatch,call_chain,native_map_lookup,zero_copy_binary_wire --languages zig --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case async_ready_chain --languages kain,rust --runs 1 --warmups 0 --timeout 600`
- `python benchmark/run.py --case tcp_loopback_tokio --languages kain,rust --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case actor_mailbox_erlang --languages kain,erlang --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case quantumerlang --languages kain,erlang --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case http_server_concurrency --languages kain,rust --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case http_server_frameworks --languages kain,rust,go --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case ecs_archetype_query,zero_copy_binary_wire,dynamic_vtable_thrashing,crypto_block_cipher,ray_sphere_intersection --languages kain,rust,cpp,go --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run.py --case sim_nbody_gravity,sim_uv_velocity_grid,sim_cfd_pressure_projection --languages kain,rust,cpp --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run_wrapper.py fast --case actor_mailbox_erlang --runs 1 --warmups 0 --timeout 900`
- `python benchmark/run_wrapper.py sim --runs 1 --warmups 0 --timeout 900`
- `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`
- `python scripts/python/release_readiness_gate.py --profile quick --run` for the repo-level honest-release matrix that pairs the benchmark release subset with attrition, stdlib-import, and runtime-conformance blockers
- Build `benchmark/kain-benchmark.exe` with the blade compile helper and capture a non-empty native UI screenshot after UI edits.
- Inspect `benchmark/out/reports/latest.llm.md` and `benchmark/latest.md` before summarizing results.
