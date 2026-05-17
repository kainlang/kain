---
name: kain-benchmark-pipeline
description: Use when adding, changing, running, or reviewing the multi-language benchmark lane under benchmark/, including Kain/Rust/C++/Zig/Go/Erlang/JavaScript/Python cases, benchmark/benchmarks.json, benchmark/run.py, LLM-readable reports, the benchmark blade console, and fairness/maturity notes for Kain pressure tests.
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
- Pin Kain compiler: `python benchmark/run.py --kain-exe D:\\Kain-Lang\\target\\release\\kain.exe`
- Pin C++ compiler: `python benchmark/run.py --languages cpp --cxx D:\\Kain-Lang\\toolchain\\llvm\\bin\\clang++.exe`
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
- If repeated Windows native builds keep printing `Native runtime cache: 0 reused, 36 compiled` or `0 reused, 37 compiled`, inspect `crates/cli/src/main.rs::parse_native_runtime_depfile(...)` before blaming the compiler or linker. The healthy steady-state line is full object reuse for the active native runtime manifest.
- Reports are written to:
  - `benchmark/latest.md`
  - `benchmark/latest_fast.md` and `benchmark/out/reports/latest_fast.llm.md` / `latest_fast.json` when the `fast` wrapper is used
  - `benchmark/latest_sim.md` and `benchmark/out/reports/latest_sim.llm.md` / `latest_sim.json` when the `sim` wrapper is used
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/out/reports/latest.json`
  - timestamped `benchmark/out/reports/<stamp>.llm.md`
  - timestamped `benchmark/out/reports/<stamp>.json`
- `benchmark/latest.md`, `benchmark/latest_fast.md`, and `benchmark/latest_sim.md` are intentionally minimal LLM-facing snapshots. If a case declares telemetry metrics, the snapshot also includes a compact telemetry table keyed by the case primary metric.
- Root snapshots and full reports now include `baseline_mode` plus baseline-cache hit/refresh counts so you can tell at a glance whether the fast foreign-baseline path actually engaged.
- HTML is no longer a report format. If `benchmark/out/reports/latest.html` appears, treat it as stale-output cleanup debt.
- The preferred extension point for new categories is `benchmark/wrappers/*.json`, not new hardcoded branches in `run.py`. Wrapper configs are data-driven plugins that can inject `before_args` and `after_args` around user-supplied CLI flags.

## Dedicated FFI Boundary Lane

- Command: `python benchmark/ffi_boundary/run.py --warmups 2 --runs 5 --timeout 300`
- This specialized lane sits outside `benchmarks.json`.
- It compares `llvm_pure`, direct LLVM object/shared-library C FFI, `interpret_pure`, the interpreter/live bridge path, `zig_pure`, and `zig_c_object`.
- Reports land in `benchmark/out/reports/ffi_boundary_latest.llm.md`, `ffi_boundary_latest.json`, and timestamped siblings.

## Blade Console

- Source: `benchmark/blades/kain-benchmark/src/*.kn`
- Executable: `benchmark/kain-benchmark.exe`
- Build proof from repo root:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry benchmark\blades\kain-benchmark\src\main.kn -OutputName D:\Kain-Lang\benchmark\kain-benchmark.exe -ArtifactRoot .kain\out -VerifyLlvm
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
- `ray_sphere_intersection` is the fixed 3D geometry kernel. In this checkout the Kain row regenerates deterministic seeded rays/spheres inside the hot loop because literal float-array indexing was not yet native-LLVM parity-safe.
- `sim_nbody_gravity`, `sim_uv_velocity_grid`, and `sim_cfd_pressure_projection` are the extracted k-os-sim simulation pack. Keep them Kain/Rust/C++ only; Go is intentionally not part of the sim category.
- `json_manual_roundtrip` and `filesystem_stream` are still the best string-heavy and filesystem-heavy runtime rows.
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
- Build `benchmark/kain-benchmark.exe` with the blade compile helper and capture a non-empty native UI screenshot after UI edits.
- Inspect `benchmark/out/reports/latest.llm.md` and `benchmark/latest.md` before summarizing results.
