# Kain Multi-Language Benchmarks

This folder is the native benchmark lane for Kain LLVM against Rust LLVM, C++ on Clang, JavaScript on Node, and Python on CPython.

The contract is intentionally simple:

- Normal benchmarks in `cases/<case>/` should have dependency-free `main.kn`, `main.rs`, `main.cpp`, `main.js`, and `main.py` sources.
- A manifest case can explicitly declare a `languages` map to become Kain/Rust-only, Kain/C++-only, etc. Missing selected languages show as `n/a` in reports instead of failing the whole suite.
- Rust normally builds through direct `rustc`; dependency benchmarks can opt into Cargo with `rust_manifest`, `rust_package`, and `rust_binary`. Put an empty `[workspace]` in those per-case Cargo manifests so Cargo does not accidentally attach them to the repo root workspace.
- Build time is recorded separately; timed samples run the already-built executables.
- The runner prefers a release-built `kain.exe`, pins Kain benchmark links to `runtime/native_core_runtime.toml`, and passes a benchmark-native tuning profile into the Kain compiler unless you override it.
- Every run writes `out/reports/latest.llm.md`, a timestamped `.llm.md` report, and `out/reports/latest.json`. Stale `latest.html` is removed.
- The report includes a maturity/fairness note per case. Some pressure tests are honest proxies until Kain exposes the matching runtime primitive directly in LLVM.

Current pressure cases:

- `contention_wall`: Rust and C++ use 100-thread atomic contention versus Kain `collapse`; JavaScript and Python use scalar proxy lanes so the report does not confuse runtime lock/GIL overhead with language semantics.
- `ghost_mirror`: std/socket TCP loopback payload transfer for Rust/C++/JavaScript/Python versus Kain entangle-backed world mirroring plus payload mutation.
- `evolutionary_loop`: runtime feature-detected lane choice versus Kain `converge` / `orchestrate` dispatch syntax.
- `tcp_loopback_tokio`: Kain native TCP loopback versus Rust Tokio TCP accept/connect/read/write.
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

Known Kain gaps exposed while shaping these cases:

- Scalar `match` in the standalone branch hot loop built but trapped at runtime, so `branch_dispatch` currently uses equivalent `if` dispatch.
- Method receiver field access in the struct benchmark hit a native codegen gap, so `struct_method` uses `score_pair(pair)` instead of `pair.score()`.
- Dynamic value capture into a Kain `async` ready future failed checksum during the async benchmark spike; `async_ready_chain` currently uses the known-good `return async 2` shape and should stay that way until async capture lowering is fixed.

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

The runner prefers a direct Bazel-built release `kain.exe` to avoid the Windows PowerShell launcher `-o` forwarding ambiguity. Use `--kain-exe` or `KAIN_EXE` to pin a specific compiler. The C++ lane defaults to the repo-bundled `toolchain/llvm/bin/clang++.exe`; use `--cxx` or `CXX` only when you intentionally want a different `clang++`/`g++`-style driver. Kain benchmark builds set `KAIN_RUNTIME_MANIFEST_PATH` to the lean core runtime manifest; use the broad runtime manifest only for app/vendor/UI lanes. Use `--kain-native-profile`, `--kain-native-opt-level`, `--kain-native-target-cpu`, and `--kain-native-debug-info` only if you are intentionally changing the native benchmark tuning.
