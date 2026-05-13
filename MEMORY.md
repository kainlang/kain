# Kain Memory

# 2026-05-13 - repo-local `kain_mcp` configs must use repo-relative paths, not `${KAIN_REPO_ROOT}`

The lingering post-reboot `kain_mcp` timeout was not inside the blade runtime.
The installed `C:\Users\Admin\.cargo\bin\kain.exe` answered a real MCP
`initialize` request in about `0.6 s`, including when launched from
`C:\Users\Admin`. The actual break was the repo-local `codex.config.toml` and
root `mcp.json`: both used `${KAIN_REPO_ROOT}` placeholders that Codex treated
as literal text in this lane. New repo sessions could therefore override the
good global MCP block with a broken local block even after a reboot.

What changed:

- Updated repo `codex.config.toml` to use `command = "kain"`,
  `args = ["run", "blades/kain-mcp"]`, `cwd = "."`, and `enabled = true`.
- Updated root `mcp.json` to use the same repo-relative launch contract and
  dropped the fake `KAIN_REPO_ROOT` env indirection.
- Updated `ARCHITECTURE.md` so future agents know the repo-local Codex config
  must stay repo-relative instead of relying on unsupported placeholder
  interpolation.

Formal proof gathered with Z3:

- `kain_mcp_literal_repo_root_placeholder_never_equals_real_repo_root`

That proof encodes the exact strings involved in this checkout and proves the
literal placeholder `${KAIN_REPO_ROOT}` cannot equal the real repo root
`D:\Kain-Lang`, so a client that skips interpolation must mis-resolve the path.

Validation:

- Literal-placeholder smoke:
  `C:\Users\Admin\.cargo\bin\kain.exe run ${KAIN_REPO_ROOT}/blades/kain-mcp`
  from `D:\Kain-Lang` failed immediately with
  `D:\Kain-Lang\${KAIN_REPO_ROOT}\blades\kain-mcp`
- Direct MCP `initialize` smoke against
  `C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp`
  from `D:\Kain-Lang` returned the first `Content-Length` frame in about
  `0.613 s`
- Direct MCP `initialize` smoke against the same command from
  `C:\Users\Admin` returned the first `Content-Length` frame in about `0.594 s`

# 2026-05-13 - `kain_mcp` now boots directly from compiled `kain.exe` without the Python shim

The `kain_mcp` boot lane no longer needs `py` in the default Codex path. The
root cause of the lingering "Starting MCP servers" hang was that direct
`kain.exe run blades/kain-mcp` boot still mixed human CLI output with machine
stdio expectations, and the blade's runtime-policy lookup was brittle when the
repo root was the current working directory.

What changed:

- Updated `blades/kain-mcp/src/runtime_settings.kn` so the blade can resolve
  `blades/kain-mcp/config` correctly when launched from the repo root instead of
  assuming a blade-local cwd.
- Updated `crates/cli/src/main.rs` so the CLI suppresses the human banner on
  non-terminal stdout and also honors `KAIN_NO_BANNER` /
  `KAIN_ENGINE_NO_BANNER`. Machine-facing consumers like MCP and JSON pipes now
  get protocol/data output first instead of a banner line.
- Switched repo `codex.config.toml` and root `mcp.json` to launch
  `kain run ${KAIN_REPO_ROOT}/blades/kain-mcp` directly instead of routing
  through `scripts/python/launch_kain_mcp.py`.
- Switched the live machine `C:\Users\Admin\.codex\config.toml` block to launch
  `C:\Users\Admin\.cargo\bin\kain.exe` directly with
  `run D:\Kain-Lang\blades\kain-mcp`.
- Kept `scripts/python/launch_kain_mcp.py` as the managed-sync fallback and
  launcher-trace path rather than deleting it. It still matters for explicit
  sync/debug workflows, but it is no longer the default Codex boot contract.

Formal proof gathered with Z3:

- `kain_mcp_nonterminal_stdout_never_emits_cli_banner`

That proof encodes the new banner gate and proves there is no model where
stdout is non-terminal yet `suppress_banner` is false.

Validation:

- `cargo build -p cli --target-dir target/codex-kain-mcp-direct`
- Direct stdio `initialize` smoke against
  `target/codex-kain-mcp-direct/debug/kain.exe run D:\Kain-Lang\blades\kain-mcp`
  returned `Content-Length` first and a valid MCP initialize body in about
  `12588 ms`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- Direct stdio `initialize` smoke against
  `C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp`
  returned `Content-Length` first and a valid MCP initialize body in about
  `7631 ms`

# 2026-05-13 - managed sync now keeps doctor metadata and build numbers in sync

The repo-local `kain_mcp` lane now has coherent managed build metadata end to end:
`kain doctor`, the managed sync stamp, and the PATH-installed binary all agree on
the live repo SHA and build number after sync.

What changed:

- Updated `crates/cli/build.rs` so CLI build metadata can be driven by explicit
  managed-sync git env vars and so Cargo watches the active branch ref plus
  `packed-refs`, not only `.git/HEAD`.
- Updated `scripts/windows/sync-kain-source-of-truth.ps1` to inject git metadata
  env vars into the CLI build, derive the next managed build number from both the
  counter file and the previous sync stamp, and parse JSON in a Windows PowerShell 5
  compatible way instead of relying on `ConvertFrom-Json -AsHashtable`.
- Updated `ARCHITECTURE.md` and the `kain-blades-system` skill with the durable
  Windows PowerShell compatibility warning for the managed sync lane.

Formal proof gathered with Z3:

- `kain_managed_build_number_monotonic_from_counter_and_stamp`

That proof shows the new `next = max(counter, stamp) + 1` rule is strictly
monotonic over non-negative stored build numbers.

Validation:

- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- `kain doctor`
- Verified `C:\Users\Admin\.kain\state\build_counter.json` now advances to `2`
- Verified `C:\Users\Admin\.kain\state\kain_sync_stamp.json` now reports
  `build_number = "2"` and current repo SHA

# 2026-05-13 - `kain_mcp` hangs were partly stale-session state; launcher now leaves boot breadcrumbs

The live `kain_mcp` server was healthy when launched with the exact Codex config
shape, but Codex sessions that started before the `.codex/config.toml` edit could
keep waiting against stale MCP state and never even invoke the new launcher block.

What changed:

- Switched both repo `codex.config.toml` and live `C:\Users\Admin\.codex\config.toml`
  to the absolute Windows Python launcher path `C:\Windows\py.exe` instead of bare
  `py`, so MCP startup no longer depends on shell-local PATH resolution.
- Extended `blades/kain-mcp/config/runtime_policy.json` with a data-driven launcher
  trace path and enable flag.
- Extended `scripts/python/launch_kain_mcp.py` so every boot attempt appends JSONL
  breadcrumbs under `~/.kain/state/kain_mcp_launcher_trace.jsonl` for
  `launcher_start`, managed-sync decisions, child spawn, and exit.
- Updated `ARCHITECTURE.md` with the durable operator rule: after changing Codex MCP
  config, restart the session and inspect the launcher trace before assuming the
  Kain server itself is stuck.

Formal proof gathered with Z3:

- `kain_mcp_cooldown_and_sync_start_are_mutually_exclusive`

That proof shows one launcher process cannot both take the cooldown-return path and
reach `managed_sync_start`. If operators see both signals at once, they are looking
at concurrent launches or mixed-session logs rather than a hidden single-process path.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- TOML parse of `C:\Users\Admin\.codex\config.toml`
- TOML parse of repo `codex.config.toml`
- Exact-config MCP initialize smoke via `C:\Windows\py.exe -3 D:\Kain-Lang\scripts\python\launch_kain_mcp.py`
  from repo cwd returned the first frame in about `8257 ms`
- Verified launcher breadcrumbs were written to
  `C:\Users\Admin\.kain\state\kain_mcp_launcher_trace.jsonl`

# 2026-05-13 - `kain_mcp` Codex timeout was a cold-sync budget issue, not a protocol bug

The recent Codex `kain_mcp` startup timeout was caused by managed sync rebuilding
the PATH-installed `kain.exe` before the MCP server answered `initialize`, not by
JSON-RPC framing failure in the blade transport.

What changed:

- Updated repo `codex.config.toml` so the canonical copied MCP block now sets
  `startup_timeout_sec = 300` and explicitly documents that cold managed-sync
  launches can exceed 30 seconds.
- Hardened `scripts/python/launch_kain_mcp.py` with immediate stderr reporting
  of stale-sync reasons plus an explicit `running managed sync before MCP startup`
  message, so future launch delays are diagnosable from Codex logs.
- Hardened `scripts/windows/sync-kain-source-of-truth.ps1` so it can resolve the
  repo root from `scripts/windows` even when the caller does not provide
  `KAIN_REPO_ROOT` and `git rev-parse` discovery is unavailable.

Formal proofs gathered with Z3:

- `kain_mcp_timeout_root_cause_stale_stamp_implies_sync_required`
- `kain_mcp_stale_repo_head_after_new_commit_requires_new_sync_attempt`
- `kain_mcp_startup_non_blocking_if_runnable_binary_exists`
- `kain_mcp_sync_requires_wait_only_when_no_runnable_binary_exists`
- `kain_mcp_sync_lock_contention_never_blocks_if_current_binary_is_still_usable`

These proofs do not claim the Rust/Python/OS build world is mathematically bounded;
they prove the launcher decision logic and stale-stamp predicates. External build
duration still requires an operator timeout budget.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- TOML parse of `C:\\Users\\Admin\\.codex\\config.toml`
- TOML parse of repo `codex.config.toml`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- Cold managed sync rebuild path completed in about `200 s`
- Outside-cwd MCP `initialize` timing smoke: first response in about `8419 ms`

# 2026-05-13 - `kain import crates` adds workspace Rust bundle and blades-mirror modes

The Rust import lane now has a workspace-scale operator command that can either
emit one combined `.kn` file or mirror each discovered Cargo crate into a
blades-style directory tree.

What changed:

- Added the built-in `kain import crates` command metadata and typed routing in
  `crates/kain-commands/commands/import.toml`,
  `crates/kain-commands/src/kain.rs`, and `crates/cli/src/main.rs`.
- Extended `crates/cli/src/import_rust.rs` with workspace root/source-root
  resolution, Cargo crate discovery, shared directory import helpers, combined
  bundle emission, and `--blades` mirroring.
- The new lane auto-detects `./crates`, then `./rust`, then `./src/rust`
  unless `--source-root` overrides it.
- Bundle mode defaults to `<source-root>.kn`; `--blades` defaults to a mirrored
  `.kn` tree under `<workspace-root>/blades`.
- Blades mode preserves the imported Rust file layout and only rewrites the
  extension to `.kn`; it does not synthesize `KAIN.toml` manifests yet.

Validation:

- `cargo test -p kain-commands --target-dir target/codex-import-crates-commands -- --nocapture`
- `cargo test -p cli --lib import_rust --target-dir target/codex-import-crates-cli -- --nocapture`
- `cargo build -p cli --target-dir target/codex-import-crates-bin`
- `target/codex-import-crates-bin/debug/kain.exe import crates --output target/codex-import-crates-smoke/cuda.kn`
  from `reference/cuda`
- `target/codex-import-crates-bin/debug/kain.exe import crates --blades --output target/codex-import-crates-smoke/cuda-blades`
  from `reference/cuda`

Durable note:

- `reference/cuda` is a strong smoke corpus for this lane. In this checkout the
  command auto-detected `reference/cuda/crates`, imported 17 crates and 251
  Rust files, emitted a 3,133,735-byte bundle, mirrored 251 `.kn` files in
  blades mode, and reported 501 lossy-lowering diagnostics across 148 files.
- Use `--source-root` when the workspace root is not the folder that directly
  contains `crates/`, `rust/`, or `src/rust`.

# 2026-05-13 - Managed sync lane proved live and `kain-json` became a runnable blade example

The managed `kain-mcp` sync lane is now proven against the real PATH-installed
`kain.exe`, and `blades/kain-json` is no longer just a loose source folder.

What changed:

- Ran the managed sync install end to end through
  `scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`, which built and
  atomically installed the release CLI into `C:\Users\Admin\.cargo\bin`.
- Verified the live PATH binary with `kain doctor`; it now reports `Build: 1`,
  `Build Tracking: managed`, the managed sync stamp path, repo/runtime/binary
  drift status, and the synced binary fingerprint.
- Proved the canonical MCP launcher from outside the repo cwd with
  `KAIN_REPO_ROOT` set and explicit `KAIN_MCP_KAIN_BIN`, including real MCP
  `initialize`, `fs.read_file`, `kain.check`, `kain.run.plan`, and
  `authoring.example` calls over stdin/stdout.
- Upgraded `blades/kain-json/KAIN.toml` into a real runnable blade manifest and
  added `src/main.kn` as a tiny executable demo that exercises the JSON helpers.

Validation:

- `cargo check -p cli --bins --target-dir target/codex-sync-doctor-live`
- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -ManagedSync`
- `kain doctor`
- `kain check blades/kain-json/src/main.kn`
- `kain run plan blades/kain-json`
- `kain blades build blades/kain-json --json`
- `kain run blades/kain-json`
- External-cwd MCP smoke via `scripts/python/launch_kain_mcp.py`

Durable note:

- The earlier compile blocker for `cargo check -p cli --bins` in this checkout
  is resolved for the current HEAD. If the managed sync lane regresses again,
  re-run the isolated-target-dir CLI check before assuming the launcher is at
  fault.

# 2026-05-13 - SPIR-V codegen gained a durable Z3 proof lane and a Vulkan layout fix

The live SPIR-V backend in `crates/gpu/src/codegen_spirv.rs` now has its own solver-backed
validation lane, and that lane immediately paid for itself by catching a real Vulkan layout bug:
storage buffers holding 3-lane vectors were being decorated with a 12-byte stride instead of the
16-byte base alignment Vulkan expects under std430-style rules.

What changed:

- Fixed `storage_buffer_stride(...)` in `crates/gpu/src/codegen_spirv.rs` so scalar buffers stay
  at 4 bytes, `Vec2`/`IVec2`/`UVec2` stay at 8 bytes, `Vec3`/`IVec3`/`UVec3` stay at 16 bytes,
  `Vec4`/`IVec4`/`UVec4` stay at 16 bytes, and `Mat4` stays at 64 bytes.
- Added a focused unit test in `crates/gpu/src/codegen_spirv.rs` to lock the common storage-buffer
  stride cases to Vulkan base-alignment expectations.
- Added `crates/gpu/tests/spirv_layout.rs`, which compiles a compute shader using
  `StorageBuffer<Vec3>` and validates the emitted module with `spirv-val --target-env vulkan1.3`.
- Added the durable proof pack at `crates/gpu/z3` with `layout`, `constructors`, `control`,
  `full`, and workspace `smoke` lanes. The first curated proofs cover wrapper-layout arithmetic,
  access-chain member-zero safety, vector-constructor component bounds, local-size slot mapping,
  and hoisted-local slot removal.

Validation:

- `cargo test -p gpu --lib storage_buffer_stride_matches_vulkan_base_alignment_for_common_types --target-dir target\\codex-spirv-proof-lib -- --nocapture`
- `cargo test -p gpu --test spirv_layout --target-dir target\\codex-spirv-proof-layout -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\gpu", lane="full")` proved 6/6 cases in `kain.gpu.proofs`.
- `mcp__z3_local__.run_workspace_proofs(project_root="D:\\Kain-Lang", lane="smoke")` still reports unrelated existing counterexamples in `runtime/native/src/ui/z3`, but the new GPU pack passed inside workspace discovery.

Durable design note:

- Treat `crates/gpu/z3` as the mandatory follow-through surface for `codegen_spirv.rs`. If a SPIR-V
  change touches layout arithmetic, vector flattening, access-chain indexing, or hoisted slot
  bookkeeping, update proofs before trusting tests alone.
- Keep pairing solver proofs with an external module validator. The proof pack checks our backend
  arithmetic and indexing invariants; `spirv-val` checks the binary against the Vulkan/SPIR-V rules
  that the solver model intentionally abstracts.

Current known gap in this checkout:

- The new proof lane does not mean the entire Kain shader authoring pipeline is globally green.
  Existing `crates/gpu/tests/spirv_smoke.rs` and `crates/gpu/tests/spirv_execute.rs` still expose
  pre-existing frontend/typechecker issues such as `.xyz` field admission, `group_index`
  resolution, tuple/vector arithmetic compatibility, and old constructor-style casts like `Int(a)`.

# 2026-05-13 - Managed MCP sync lane + deterministic doctor build tracking

The canonical `blades/kain-mcp` launcher/sync surface now has a first-class managed
sync contract for multi-agent environments, and `kain doctor` now reports explicit
managed-sync drift instead of only raw build metadata.

What changed:

- Extended `blades/kain-mcp/config/runtime_policy.json` with a `launcher_sync`
  section (state root, lock path, stamp path, build-counter path, cooldown,
  stale-lock timeout, sync command, runtime stamp files, and `prefer_synced_binary`).
- Reworked `scripts/python/launch_kain_mcp.py` to load policy data, run stale checks
  (`repo_sha` + runtime stamp + binary stamp) on launch, enforce a global sync lock,
  respect cooldown, and call managed sync before boot when stale.
- Reworked `scripts/windows/sync-kain-source-of-truth.ps1` to support managed sync:
  deterministic build counter at `~/.kain/state/build_counter.json`, injected
  `KAIN_BUILD_NUMBER`, atomic swap install for `kain.exe`/`kn.exe`, and stamp writes
  at `~/.kain/state/kain_sync_stamp.json`.
- Updated `crates/cli/build.rs` so build numbers default to explicit unmanaged mode
  instead of timestamp-like pseudo-build IDs; managed numbers now come from sync.
- Added `BUILD_TRACKING_MODE` in `crates/cli/src/lib.rs` and expanded doctor output
  in `crates/cli/src/main.rs` to show managed-sync stamp details, repo drift status,
  managed binary details, and binary-path mismatch warnings.

Durable design note:

- Keep launcher/sync/doctor on one data model (`runtime_policy.json` + sync stamp).
  Avoid reintroducing hardcoded binary paths or hand-written stale logic in one lane.
- The managed sync lane must be resilient to lock contention and failed rebuilds:
  warn and continue with the current binary rather than breaking MCP transport.

Validation:

- `py -3 -m py_compile scripts/python/launch_kain_mcp.py`
- PowerShell parse check for sync script (`[ScriptBlock]::Create(...)`)
- `cargo check -p cli --target-dir target/codex-sync-doctor` (pass)
- `pwsh -File scripts/windows/sync-kain-source-of-truth.ps1 -SkipBuild -ManagedSync` (pass)
- End-to-end MCP stdio smoke through launcher (`initialize`, `tools/list`, `shutdown`) (pass)

Current known blocker in this checkout:

- `cargo check -p cli --bins` / `cargo build -p cli` currently fails in pre-existing
  `crates/kain-build/src/workspace.rs` compile errors unrelated to this sync pass.
  Because of that repo-wide breakage, full binary-level doctor verification from a
  freshly rebuilt CLI was blocked in this turn.

# 2026-05-13 - `kain-core` keyword contracts gained a dedicated Z3 lane

The `crates/kain-core/z3` pack now has a focused `keywords` lane for the compiler-owned
`patch`, `law`, `converge`, and `orchestrate` forms. These proofs stay separate from the
existing arithmetic/parser lanes so future agents can run branch-ordering and runtime
contract checks without digging through the low-level memory suites.

What changed:

- Added `proofs/keywords-patch-cancel-rewinds-only-when-reversible.yaml` to prove the
  patch rewind path only fires for reversible frames.
- Added `proofs/keywords-law-runtime-accepts-only-bool-results.yaml` to prove law
  runtime acceptance is Bool-only.
- Added `proofs/keywords-converge-first-fast-lane-wins-and-spec-fallback.yaml` to prove
  converge selection honors first-match fast lanes and the spec fallback.
- Added `proofs/keywords-orchestrate-rejects-invalid-stage-ordering.yaml` to prove
  orchestrate stage collection rejects late stage declarations, nested items, and bare
  stage calls.
- Wired a new `keywords` lane into `crates/kain-core/z3/z3.toml` and documented it in
  `crates/kain-core/z3/README.md`.

Validation:

- `mcp__z3_local__.check_smt2` proved the patch, law, and converge formulas unsat with
  `include_model=false` and `include_stats=false`.
- `mcp__z3_local__.state_machine_check` proved the orchestrate ordering invariant holds
  within 4 bounded steps.

Durable note:

- Keep these keyword-contract proofs in their own lane. They are branch-ordering and
  runtime-contract checks, not arithmetic proofs, and should stay easy to rerun as a
  group.

# 2026-05-13 - `kain-mcp` launcher transport hardened and request loop de-actorized for stability

The canonical `blades/kain-mcp` lane now survives real MCP stdio clients in this
checkout, including multi-request `tools/call` sessions from outside the repo cwd.

What changed:

- Hardened `scripts/python/launch_kain_mcp.py` into a real transport shim instead
  of a simple `subprocess.call(...)` wrapper.
- Added managed Kain binary resolution order:
  `KAIN_MCP_KAIN_BIN` -> `target/debug` -> `target/release` -> PATH.
- Added managed Windows Python runtime path preloading so repo-built `kain.exe`
  can resolve `pythonXY.dll` reliably without shell-local PATH surgery.
- Added byte-stream stdin/stdout/stderr forwarding using `os.read/os.write` to
  avoid buffered-pipe stalls in MCP sessions.
- Added first-line stdout filtering for the CLI banner (`KAIN Compiler v...`) so
  the MCP `Content-Length` stream starts cleanly.
- Switched `blades/kain-mcp/src/main.kn` from actor-backed request dispatch to a
  direct route loop after reproducing stack-overflow crashes during
  `tools/call` filesystem handlers in the actor context.
- Simplified `fs.list_directory` entry payloads by dropping raw `entry.metadata`
  from MCP structured output.

Durable design note:

- Keep launcher behavior transport-safe first. If the host CLI emits non-protocol
  stdout text, scrub it at the launcher boundary unless/until the CLI gains a
  protocol-safe quiet mode for this lane.
- The direct routing loop is the current stability baseline. Reintroduce actor
  routing only after reproducing and fixing the actor-context stack overflow in
  `kain-mcp` tool handlers.

Validation:

- `target/debug/kain.exe run plan .\\blades\\kain-mcp`
- `target/debug/kain.exe blades build .\\blades\\kain-mcp --json`
- End-to-end MCP stdio smoke via `py -3 scripts/python/launch_kain_mcp.py`:
  `initialize`, `tools/list`, `fs.list_directory`, `fs.read_file`,
  `kain.run.plan`, `authoring.example`, and `shutdown` all returned valid
  JSON-RPC frames.

# 2026-05-13 - `blades/kain-mcp` routing moved behind a dedicated dispatch module

`blades/kain-mcp/src/main.kn` no longer imports every tool handler directly or
owns the entire handler switch chain. Tool routing now goes through
`src/tool_dispatch.kn`, and `main.kn` only asks the dispatcher for a handled
result.

What changed:

- Added `blades/kain-mcp/src/tool_dispatch.kn` with `ToolDispatchResult` and
  `dispatch_tool_handler(...)`.
- Switched `blades/kain-mcp/src/main.kn` to import `dispatch_tool_handler`
  instead of importing every handler function.
- Updated `blades/kain-mcp/KAIN.toml` build task inputs to include
  `src/tool_dispatch.kn`.

Durable design note:

- Adding a new MCP tool now requires updating `config/tools.json` plus
  `src/tool_dispatch.kn`; `src/main.kn` should remain stable unless request
  protocol routing changes.
- `use module::*` is accepted in the `kain-mcp` blade context, but it is not a
  universal drop-in for every lane. The `smoketest/native-ui/episode-two`
  lane still fails `kain check` for unrelated native-ui stdlib resolution
  (`native_ui_node_set_text`) and should be treated as a separate cleanup task.

Validation:

- `target/debug/kain.exe check blades/kain-mcp/src/main.kn`
- `target/debug/kain.exe run plan blades/kain-mcp`

# 2026-05-13 - `blades/kain-mcp` became the canonical repo MCP lane

The repo no longer treats the Kain-authored MCP server as a loose `MCP/server.kn`
experiment. The live MCP implementation now lives in the real blade
`blades/kain-mcp`, which means future agents can discover, run, build, and inspect
it through the same blade/run pipeline as the rest of the language examples.

What changed:

- Added `blades/kain-mcp/KAIN.toml` with real `[package]`, `[blade]`, `[run]`, `[build]`, and `[manifests]` sections so `kain run blades/kain-mcp` and `kain blades build blades/kain-mcp --json` become the canonical operator flow.
- Split the server into blade-owned modules under `blades/kain-mcp/src/` for runtime settings, tool registry loading, MCP protocol framing, filesystem tools, Kain operator tools, authoring/example tools, and the entry router.
- Moved tool metadata and runtime policy into `blades/kain-mcp/config/tools.json` and `blades/kain-mcp/config/runtime_policy.json` so the MCP surface is data-driven rather than hardcoded in one giant file.
- Pointed authoring guidance at `docs/examples/examples_manifest.json` and `docs/examples/validate_examples.py` instead of duplicating example truth inside the blade.
- Added `scripts/python/launch_kain_mcp.py` as the canonical repo launcher. It resolves `KAIN_MCP_KAIN_BIN`, falls back through repo debug/release builds and PATH, sets `KAIN_REPO_ROOT`, and prepends discovered Python install directories to PATH so repo-built `kain.exe` can find its matching `pythonXY.dll` on Windows.
- Updated root `mcp.json`, `codex.config.toml`, and `MCP/README.md` so the repo now advertises the blade launcher instead of the missing `tools/kain-flight-control/launcher.py` sidecar path.

Durable design note:

- Keep `blades/kain-mcp` as the real source of truth for repo-local MCP behavior. Root `MCP/` is now redirect-only docs, not a second implementation surface.
- Keep new MCP tools and runtime policy data-driven. Add schemas, handler ids, env keys, limits, and resolution order to the blade config JSON first instead of hardcoding new branches into the entrypoint.
- The blade is also a teaching example. Favor simple Kain syntax patterns that survive the current frontend: single-line helper signatures where needed, no parser-hostile inline conditionals inside argument lists, and no unnecessary `return` statements inside `-> Unit` helpers.

Validation:

- `target/debug/kain.exe run plan .\\blades\\kain-mcp`
- `target/debug/kain.exe check .\\blades\\kain-mcp\\src\\main.kn`

# 2026-05-12 - Native runtime commands became first-class CLI entrypoints

The runtime validation wrappers from the earlier pass are now exposed directly
through the typed `kain` / `kn` command surface, so future operators do not
have to remember the underlying script names before they can prove the native
runtime bundle.

What changed:

- Added a dedicated `runtime` command pack at `crates/kain-commands/commands/runtime.toml` and registered it in the built-in command-pack index.
- Added typed `RuntimeCommand` parsing in `crates/kain-commands/src/kain.rs` for `kain runtime build` and `kain runtime validate`, including aggregate validation skip flags.
- Added `crates/cli/src/runtime_tools.rs` as the thin execution host. It resolves the repo root from `KAIN_REPO_ROOT`, the current working tree, or the repo-built binary location, then forwards to the existing bash/PowerShell runtime wrappers instead of reimplementing runtime policy in Rust.
- Updated the registry and dynamic help tests so `kain commands list --bin kain` and `kain commands help --bin kain` now expose the `runtime` command family.
- Updated runtime/operator docs and metadata so `kain runtime build` / `kain runtime validate` are the preferred front door, while the bash/PowerShell scripts remain the underlying implementation truth.

Durable design note:

- Keep `kain runtime build` and `kain runtime validate` as thin operator entrypoints. They should discover the repo and delegate to the canonical wrapper scripts, not grow a second copy of native-runtime build logic inside `crates/cli`.
- The existence of first-class runtime commands still does not imply a separate shipped `kain_runtime.exe`. The owned runtime remains a manifest-driven source/object/archive bundle linked into generated native programs.

Validation:

- `cargo fmt -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-commands --target-dir target\\codex-kain-runtime-commands -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\\codex-kain-runtime-commands-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\\codex-kain-runtime-commands-cli`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime build --help`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime validate --help`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe commands list --bin kain`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe commands help --bin kain`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime validate --skip-cli-build --skip-runtime-build --skip-fixtures --skip-conformance`
- `target\\codex-kain-runtime-commands-cli\\debug\\kain.exe runtime build`

# 2026-05-12 - Native runtime validation entrypoints aligned across bash and PowerShell

The native runtime build pipeline was already present in code, but the operator surface around it was inconsistent enough to create false confusion about whether Kain had a real C runtime pipeline at all.

What changed:

- Added the missing aggregate validation entrypoint `runtime/validate_native_runtime.sh` so the command already referenced by metadata and `ARCHITECTURE.md` now exists for real.
- Added Windows operator wrappers at `runtime/compile_native_runtime.ps1`, `runtime/conformance/run_all.ps1`, and `runtime/validate_native_runtime.ps1`.
- Replaced the stale `runtime/fixtures/validate_all.ps1` implementation with a thin wrapper around the canonical `runtime/fixtures/validate_all.sh` lane so PowerShell no longer routes through an older Rust-target-only fixture script.
- Added `runtime/scripts/runtime_windows_shell_helpers.ps1` to keep bash discovery and Windows path translation in one place instead of duplicating wrapper logic.
- Added `runtime/NATIVE_RUNTIME_VALIDATION.md` to spell out the important build distinction: `cargo build -p cli` builds the compiler host, while `kain build ... -t llvm` and `kain build ... -t c` compile and link the manifest-driven native runtime bundle into the produced executable.
- Updated `ARCHITECTURE.md` so runtime validation commands now list both bash and PowerShell entrypoints, and so future agents do not assume the C runtime should be a separate shipped executable.

Durable design note:

- Do not create a separate `kain_runtime.exe` just to prove the runtime exists. The current architecture intentionally treats the owned native runtime as a manifest-driven object/archive bundle that gets linked into each generated native program.
- Keep the bash scripts as the canonical runtime validation logic for now, and keep PowerShell wrappers thin. That avoids two diverging implementations of the same runtime build policy.

# 2026-05-12 - kain-core Z3 proof pack landed and low-level memory layout math was hardened

`crates/kain-core` now has its own durable proof pack at `crates/kain-core/z3`. The pack is scoped at compiler/frontend arithmetic and indexing seams instead of the native C runtime: low-level memory layout lowering in `src/low_level_memory.rs`, signed `usize -> i64` literal conversions used by lowered helpers, diagnostics span/line-end math in `src/diagnostics.rs`, and parser slice/index guards in `src/parser.rs`.

What changed:

- Added the `kain.core.proofs` pack with lanes `memory`, `diagnostics`, `literals`, `parser`, `smoke`, and `full`.
- Hardened `crates/kain-core/src/low_level_memory.rs` so layout addition, multiplication, align-up steps, fallback array sizing, fallback tuple sizing, and lowered signed literal conversions now fail explicitly instead of silently wrapping.
- Added `DiagnosticCode::MemoryLayoutOverflow` (`KAIN-MEM-0004`) in `crates/kain-core/src/diagnostic_registry.rs` and routed layout overflow failures through a dedicated validation diagnostic with a concrete suggestion.
- Seeded durable proofs for checked layout addition, checked layout multiplication, align-up wrap prevention, tuple and array fallback sizing, signed literal bounds, diagnostics span/line-end bounds, and parser indexing/slicing preconditions.

Validation:

- `cargo check -p kain-core`
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="memory")` proved 5/5.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="diagnostics")` proved 3/3.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="literals")` proved 1/1.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="parser")` proved 3/3.
- `run_proof_pack(path="D:\Kain-Lang\crates\kain-core", lane="full")` proved 12/12.
- `run_workspace_proofs(project_root="D:\Kain-Lang", lane="smoke")` proved both repo packs for 32/32 total cases.

Current unrelated test status:

- `cargo test -p kain-core` still has five pre-existing failures outside this proof pass: `language_features::tests::default_profile_keeps_struct_literals_disabled`, `realtime_app_bundle::tests::emits_bundle_owned_camera_and_presentation_metadata_for_viewports`, `realtime_app_bundle::tests::emits_realtime_bundle_with_viewport_scene_binding`, `stdlib_tests::test_load_stdlib_graceful_degradation`, and `stdlib_tests::test_env_var_priority_over_filesystem`.

Durable workflow note:

- If a `kain-core` proof fails only on values larger than `18446744073709551615` or `9223372036854775807`, inspect the proof model before changing Rust code. This pack intentionally constrains `usize`-shaped arithmetic to `SIZE_MAX` and signed-literal success paths to `i64::MAX`; otherwise Z3 can invent values the ABI or helper never accepts.

# 2026-05-12 - Native core Z3 pack expanded across actor/net/process/entangle

The repo-local native proof pack at `runtime/native/src/core/z3` is no longer just a seed lane. It now carries curated durable proofs across four low-level runtime seams and validates the upgraded Z3 workflow end to end.

What changed:

- Added actor coverage for `kain_actor_try_receive(...)` so the non-blocking mailbox receive path has its own explicit count-underflow proof.
- Expanded native net coverage with request-body span arithmetic, request-body allocation arithmetic, and stored-response allocation arithmetic around `kain_native_net_parse_http_request(...)` and `kain_native_net_store_http_response(...)`.
- Added first-class process proofs for argument/environment capacity guards, capture-append bounded growth, UTF-8/wide buffer append arithmetic, and hex-encoding allocation bounds in `kain_native_process_system.c`.
- Added first-class entangle proofs for `kain_runtime_copy_entangle_text(...)` null-terminated copy sizing and `kain_runtime_entangle_register(...)` fixed-capacity registry growth.
- Added local matcher bundles in `templates/process-runtime.yaml` and `templates/entangle-runtime.yaml`, and refined the entangle template so extraction constrains values to a real 64-bit `size_t` domain instead of proving against impossible widths.
- Added focused manifest lanes in `z3.toml`: `net`, `process`, and `entangle`, while keeping `actor`, aggregate `native`, `full`, and workspace `smoke`.

Runtime hardening in the same pass:

- `runtime/native/src/core/kain_native_process_system.c` now uses explicit checked `size_t` helpers for buffer growth, allocation sizing, wide/UTF-8 append helpers, hex encoding, wide-string duplication, environment-block construction, and capture-length accumulation.

Validation:

- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="actor")` proved 7/7.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="native")` proved 13/13.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="process")` proved 6/6.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="entangle")` proved 2/2.
- `run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="full")` proved 20/20.
- `run_workspace_proofs(project_root="D:\\Kain-Lang", lane="smoke")` proved discovery plus execution for 20/20 cases.
- `extract_source_proof_cases(save=false)` confirmed pack-local template extraction for actor, process, and entangle sources.
- `bash runtime/conformance/process_runtime/run_tests.sh --verbose`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_process_system.c`

Durable workflow note:

- When a proof fails on values larger than `18446744073709551615`, check the proof model before assuming the C path is wrong. Several seams here needed explicit `size_t` domain constraints so Z3 would stop inventing values that the ABI cannot represent.

# 2026-05-12 - Native core Z3 proof pack seeded

The first durable repo-local Z3 proof pack now lives at `runtime/native/src/core/z3` and is named `kain.native.core.proofs`. This is the seed lane for solver-backed native runtime invariants, especially the Erlang-style actor substrate and low-level C arithmetic seams that are easy to regress by inspection alone.

What the pack owns now:

- Six actor proofs covering bounded mailbox send counts, receive-count underflow prevention, scheduler dequeue accounting, scheduler max-depth monotonicity, restart-limit arithmetic, and actor ID slot ranges that preserve `KAIN_ACTOR_ID_INVALID == 0`.
- Two native net proofs preserving the recent hardening work: non-negative `Content-Length` parsing before `size_t` conversion and checked append-buffer size addition.
- A local `templates/actor-runtime.yaml` matcher bundle that describes the first actor proof shapes for future source-to-proof extraction.
- Manifest lanes in `z3.toml`: `smoke`, `actor`, `native`, and `full`.

Validation:

- `uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane smoke` proved 8/8 cases.
- `run_proof_pack(path="D:\Kain-Lang\runtime\native\src\core", lane="actor")` proved 6/6 cases.
- `run_proof_pack(path="D:\Kain-Lang\runtime\native\src\core", lane="native")` proved 2/2 cases.

Design note:

- Keep generated report JSON out of commits; it is local validation output. Commit durable proof cases, manifests, templates, fixtures, and generated tests only when they are intentionally part of the proof surface.
- Next high-leverage step is to extend the template loader/source analyzer so pack-local `templates/*.yaml` can drive extraction automatically, then add deeper actor state-machine proofs for mailbox state transitions, supervisor restart windows, and scheduler fairness bounds.

# 2026-05-12 - Native net runtime hardened against Content-Length and append-size wrap

The native HTTP lane in `runtime/native/src/core/kain_native_net_system.c` now rejects malformed or negative `Content-Length` headers before they ever reach a `size_t` cast, and the shared byte-append helper now uses overflow-checked `size_t` growth instead of raw `length + byte_count + 1` arithmetic.

What changed:

- Added `kain_native_net_size_add_overflow(...)` for local `size_t` addition checks and used it in request-body bounds checks, response-body allocation, and the shared append-buffer growth path.
- Added `kain_native_net_parse_content_length_header(...)` so `Content-Length` parsing is strict: it skips leading whitespace, rejects signed values, rejects junk suffixes, and rejects values that exceed `SIZE_MAX`.
- Hardened `kain_native_http_server_pump(...)` so malformed or overflowing `Content-Length` values fail with `KAIN_NATIVE_NET_PARSE_ERROR` instead of silently wrapping through request-length math.
- Added a native conformance regression in `runtime/conformance/net_runtime/test_native_net_system_kernel.c` that sends `Content-Length: -1` and asserts the request is rejected with parse diagnostics.

Why this matters:

- Before this pass, a header like `Content-Length: -1` could flow through `atoll(...)` into an unsigned `size_t`, wrap to `SIZE_MAX`, and then bypass a `header_length + body_length <= length` guard because unsigned addition wrapped modulo `2^N`.
- The same file also had a latent append-buffer overflow hazard in `needed = *length + byte_count + 1u`; the new helper makes that arithmetic explicit and checkable.

Validation:

- `cargo test -p kain-net --target-dir target\\codex-z3-net-fix`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`

# 2026-05-12 - Command manifests split into packs with dynamic registry help

`crates/kain-commands` now uses an indexed command-pack layout instead of a
mega `kain.toml` plus separate `blade.toml`. The build script reads
`crates/kain-commands/commands/index.toml`, validates each top-level pack file,
and generates built-in pack plus command definitions. The pack files stay flat
under `crates/kain-commands/commands/` so a future agent can scan `core.toml`,
`build.toml`, `run.toml`, `blade.toml`, `import.toml`, `unreal.toml`,
`registry.toml`, and the smaller domain packs directly.

The Unreal side is intentionally visible again: `unreal.toml` owns the current
UE5-facing executable entries (`gpu-artifacts` and `inject`), while the build
pack keeps UE5 build targeting tagged on `build` through the existing flags.

New registry affordances:

- `kain commands packs` / `--json` lists command packs.
- `kain commands help --bin kain|kn|blade` renders a dynamic Clap help tree from
  the registry.
- Registry text output includes `pack=` and `tags=`.
- Runtime command manifests may now provide `tags` and `args` for richer future
  dynamic help.

Validation for this pass:

- `cargo fmt -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-commands --target-dir target\codex-kain-command-packs -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\codex-kain-command-packs-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-command-packs-cli`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands packs`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands packs --json`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands list --bin kain`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin kain`
- `target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin blade`
- `target\codex-kain-command-packs-cli\debug\kn.exe commands list --bin kn`
- `target\codex-kain-command-packs-cli\debug\blade.exe --help`
- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Admin\.agents\skills\kain-command-platform`

Additional broad validation attempted:

- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p cli --target-dir target\codex-kain-command-packs-cli-test -- --nocapture`

That broader CLI suite still fails outside the command-platform slice:
`selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout` keeps
the known indentation assertion failure, and
`import_c::tests::test_import_with_target` currently returns an error while
asserting `result.is_ok()`. Command-pack tests, CLI check/build, and executable
registry smokes are green.

Recommended next step:

- Move execution from typed Clap-first toward the hybrid command host:
  dynamic Clap can already render and resolve registry entries, but built-in
  handler execution still flows through the typed routers. The next major step
  is a handler dispatch table that can execute registry-resolved built-ins and
  runtime blade handlers from one path.

# 2026-05-12 - Unified kain-run pipeline landed

Kain now has `crates/kain-run` as the explicit immediate-execution crate behind `kain run`, `kain run dev`, `kain run plan`, `kain watch`, `kain blades run`, and standalone `blade run`. This moved the old birth-era run behavior out of the CLI and into a reusable pipeline shaped like the other first-class Kain systems (`kain-fs`, `kain-process`, `kain-actor`, `kain-build`).

The new run crate owns:

- `RunRequest`, `RunPlan`, `RunUnit`, `RunAdapter`, `RunReport`, and JSONL run events.
- Target inference for Kain source, C, Cargo, Fabric, Node, and Bun.
- Blade and workspace resolution through `crates/kain-blades`.
- `[run]` manifest metadata: `entry`, `blade`, `target`, `args`, `env`, `cwd`, and `watch`.
- Hidden cached C execution through Clang with outputs under `.kain/cache/run/c`.
- Cargo run execution with isolated target dirs under `.kain/cache/run/cargo`.
- Run reports under `.kain/reports/run` and watcher polling through `kain-fs`.
- Process-backed report metadata using `kain-process::ProcessSpec`.

`crates/kain-commands` now exposes the new command surface for `run`, `run dev`, `run plan`, `watch`, `blades run`, and standalone `blade run`; `crates/cli/src/run.rs` is only the CLI print/exit wrapper. `crates/kain-core/src/types.rs` also registers stdlib registry globals in the type environment so raw stdlib bridge names such as `kain_input_reset` are visible during source checking and runtime compilation.

Validation for this pass:

- `cargo fmt -p kain-run -p blade -p kain-commands -p cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-run -p blade -p kain-commands --target-dir target\codex-kain-run -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo test -p kain-core type_env_registers_stdlib_registry_bridge_globals --target-dir target\codex-kain-run -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p kain-run -p kain-commands -p cli --target-dir target\codex-kain-run`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-run`
- `target\codex-kain-run\debug\kain.exe run plan docs\examples\00_hello_and_cli.kn --json`
- `target\codex-kain-run\debug\kain.exe run docs\examples\00_hello_and_cli.kn`
- `target\codex-kain-run\debug\kain.exe run target\codex-kain-run-smoke\hello.c --target c -- smoke-arg`
- `target\codex-kain-run\debug\kain.exe watch docs\examples\00_hello_and_cli.kn --dry-run`
- `target\codex-kain-run\debug\blade.exe run --help`
- `target\codex-kain-run\debug\kain.exe commands list --bin blade`

Current limits and next recommended step:

- Kain interpreter and Fabric adapters execute through their existing host functions; runtime args are meaningful for process-backed adapters first.
- The dev watcher is intentionally polling-based through `kain-fs` v1. A future pass can add native notify acceleration behind the same run-plan contract.
- `--trace` and `--keep-artifacts` are part of the request/report surface, but deeper adapter-specific trace payloads should be added as the native run pipeline grows.
- The next high-leverage pass is to add richer adapter-specific run reports and native notify watching without changing the CLI surface again.

# 2026-05-12 - Kain command platform crate landed

Kain now has `crates/kain-commands` as the command brain for `kain`, `kn`, and standalone `blade`. The crate owns built-in command manifests under `crates/kain-commands/commands/`, typed Clap routers under `crates/kain-commands/src/`, shared argument structs, launcher helpers, registry serialization, conflict detection, and a first runtime `[[commands]]` contribution loader/fallback. The workspace `Cargo.toml` now includes the crate and `crates/cli` depends on it.

The ownership split is now deliberate:

- `crates/kain-commands` owns command shape, metadata, aliases, bin exposure, registry views, and runtime contribution resolution.
- `crates/cli` is the host binary/execution shell: parse, dispatch, print, set exit codes, and call domain crates.
- Domain crates such as `kain-driver`, `kain-build`, `blade`, `kain-check`, `kain-test`, `kain-repair`, `kain-repl`, `kain-omni`, and `kain-codebase` still own actual behavior.

`kain commands list/export` now exposes the registry for `kain`, `kn`, and `blade`, with `--runtime` merging workspace-discovered runtime command manifests through the blade resolver. Runtime command fallback can recognize contributed paths, but dynamic handler execution is intentionally not implemented yet; matched runtime commands fail clearly until a real handler bridge is added. Built-ins win conflicts and duplicate runtime paths are rejected.

Validation for this pass:

- `cargo fmt -p kain-commands -p cli`
- `cargo test -p kain-commands --target-dir target\codex-kain-commands -- --nocapture`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo check -p cli --target-dir target\codex-kain-commands-cli`
- `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1 cargo build -p cli --target-dir target\codex-kain-commands-cli`
- `target\codex-kain-commands-cli\debug\kain.exe --help`
- `target\codex-kain-commands-cli\debug\kn.exe --help`
- `target\codex-kain-commands-cli\debug\blade.exe --help`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kain`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kn`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin blade`
- `target\codex-kain-commands-cli\debug\kain.exe commands list --bin kain --runtime`
- `target\codex-kain-commands-cli\debug\kain.exe commands export --bin blade`
- `python C:\Users\Admin\.codex\skills\.system\skill-creator\scripts\quick_validate.py C:\Users\Admin\.agents\skills\kain-command-platform`

Broader `cargo test -p cli --target-dir target\codex-kain-commands-cli -- --nocapture` now compiles the moved-router modules, but still fails in runtime-heavy pre-existing lanes: several tests hit `Unknown identifier 'kain_input_reset'`, and `selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout` still fails its indentation assertion. The router-specific compile issue found during that run was fixed by moving `PathBuf` imports into the affected test modules.

Recommended next step:

- Decide whether phase 2 should generate more of the Clap shape from the TOML manifests or keep typed Clap as the ergonomic parser layer, then add the dynamic runtime handler bridge for `handler = "blade:<id>:<command>"` contributions.

# 2026-05-12 - Native TCP and HTTP substrate landed

Kain now has a first-class network lane instead of relying on tiny interpreter-only `http_get`/`http_post_json` helpers or raw legacy `socket_*` functions. `crates/kain-net` owns the portable contract for TCP endpoints, HTTP request/response specs, headers, route specs, handles, lifecycle state, and typed errors. LLVM/direct-C builds load `stdlib/native/net.kn`, backed by `runtime/native/include/kain_native_net_system.h` and `runtime/native/src/core/kain_native_net_system.c`.

The native ABI is handle-driven and primitive-friendly so current LLVM/direct-C lowering can use it without aggregate ABI work. The v1 flow is TCP connect/listen/accept/read/write plus HTTP request/response handles, HTTP client send, local HTTP server listen/pump, actor route registration, request inspection, response writes, local URL helpers, reset, and diagnostics.

`io.net` in the service table now points at the owned native net function table instead of the older vendor/libuv placeholder. `kain_native_runtime_init/shutdown` reset the net registry so open sockets, listeners, request handles, and response handles are cleaned up between native runs. The lean and broad native runtime manifests both include the net source; Windows linking now includes `ws2_32` and `winhttp`.

Validation targets added for this lane:

- `cargo test -p kain-net`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_net_tcp_http_and_actor_route_primitives -- --exact`
- `cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_net_symbols_as_declarations -- --exact`
- `bash runtime/conformance/net_runtime/run_tests.sh --verbose`
- `target/debug/kain.exe build runtime/fixtures/native_net_http/main.kn --target llvm --output runtime/fixtures/native_net_http/generated/native_net_http.ll` then run the generated executable

Current known limits:

- HTTP server support is HTTP/1.1 with request-line/header parsing and `Content-Length` bodies. Server TLS, chunked request bodies, WebSockets, HTTP/2, and HTTP/3 are out of v1.
- HTTPS client support is Windows-first through WinHTTP. Plain HTTP client support uses the runtime TCP path.
- Actor routes currently dispatch a native actor message payload containing the incoming request handle and request metadata, while manual polling/response remains the deterministic fixture path. Rich Kain actor handler ergonomics should be layered above this ABI rather than baked into the socket kernel.
- Entangle is intentionally not part of the net ABI. Use it later for replicated state, distributed actor sessions, or cluster coordination above the transport.

Recommended next step:

- Add a Kain-authored HTTP server convenience layer above `stdlib/native/net.kn` that maps route patterns to actor handlers and response helpers, then add UDP/DNS only after the HTTP/TCP ergonomics are stable.

# 2026-05-12 - Native child-process and PTY substrate landed

Kain now has a first-class process lane instead of only ad hoc host-side command helpers. `crates/kain-process` owns the portable contract for process specs, stdio modes, cwd/env overrides, process/PTY handles, lifecycle state, and captured output. LLVM/direct-C builds load `stdlib/native/process.kn`, backed by `runtime/native/include/kain_native_process_system.h` and `runtime/native/src/core/kain_native_process_system.c`.

The native ABI is intentionally handle-driven and primitive-friendly so current LLVM/direct-C lowering can use it without aggregate ABI tricks. The flow is:

- create a process spec
- add argv entries
- set cwd/env/stdin/stdout/stderr policy
- spawn a normal child or PTY child
- poll/wait for exit
- write stdin or PTY input
- read/capture stdout, stderr, or PTY output
- inspect last-status diagnostics

Windows is the first real implementation. Normal child-process spawn uses explicit `CreateProcessW` plus inherit/pipe/null stdio wiring, cwd overrides, merged environment blocks, capture buffers, and output draining. PTY spawn uses ConPTY through `STARTUPINFOEX` plus inherited std handles so console APIs and standard-stream writes both route into the same transport. Non-Windows hosts keep the ABI surface but return explicit unsupported diagnostics instead of pretending parity exists.

The core runtime/profile updates in this pass:

- `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml` now include `kain_native_process_system.c`.
- `runtime/native/include/kain_runtime_native_stdlib.h` now exports the process ABI header.
- `kain_native_runtime_init/shutdown` reset the process registry so native fixtures start clean and shutdown kills live children before teardown.
- `io.process` in the service table now points at the owned native process function table instead of the older vendor/libuv stub.

Validation targets added for this lane:

- `cargo test -p kain-process`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_process_and_pty_primitives -- --exact`
- `cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_process_symbols_as_declarations -- --exact`
- `bash runtime/conformance/process_runtime/run_tests.sh --verbose`
- `target/debug/kain.exe build runtime/fixtures/native_process_stdio/main.kn --target llvm --output runtime/fixtures/native_process_stdio/generated/native_process_stdio.ll` then run the generated `.exe`

Current known limit:

- The ConPTY lane now proves PTY spawn/capture/resize and can write into an interactive PTY session, but the strongest deterministic proof today is still PTY capture from a self-contained command plus explicit API exercise for interactive writes. If future work needs richer terminal semantics, keep it in this substrate or a dedicated terminal layer above it; do not fall back to shell-specific host helpers.

Recommended next step:

- Add a first-class `kain-net` contract/subsystem using the same pattern: portable crate, `stdlib/native/*.kn` wrapper, native C ABI floor, codegen proof, conformance runner, and one focused LLVM fixture that proves a real TCP or HTTP roundtrip.

# 2026-05-12 - Native UI gained generic authored state cells

The raw native UI ABI now has generic per-node state cells: `kain_native_ui_node_set_state_i64/f64/string`, `kain_native_ui_node_state_i64/f64/string`, and `kain_native_ui_state_count`. This is deliberately substrate, not a component system. The runtime stores keyed values for authored nodes and marks nodes dirty, but it does not know what a button, tetrahedron, Kerr-field hit tester, shader surface, or product control means.

`stdlib/native/ui.kn` exposes thin state wrappers plus system-shaped helpers for booleans, toggles, counters, references, and arbitrary `shape.*`, `hit.*`, `draw.*`, and `resource.*` payload conventions. The stdlib still does not define baked buttons, panels, or product UI. Apps and Kain libraries can build any catalog or stranger UI model they want on top of these cells.

Validation targets updated for this pass:

- `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` covers raw state set/get/fallback/count.
- `runtime/conformance/ui_runtime/test_native_ui_system_host_services.c` covers state preservation through stable-key identity and live `win32-gl` acceptance.
- `runtime/fixtures/native_ui_stdlib_layer/main.kn` proves Kain-authored state payload helpers through LLVM.
- `smoketest/native-ui/pilot/main.kn` carries arbitrary command/viewport shape, hit, draw, and resource payloads into the live screenshot smoke.

Recommended next step:

- Build the real Kain-authored reconciler/state graph on top of these cells, including hot-reload retention policy and authored custom hit/layout callbacks. Keep rect hit testing as the v1 host prefilter only; do not make rects the semantic ceiling.

# 2026-05-12 - Kain-authored native UI stdlib layer started

`stdlib/native/ui.kn` now has a first real authored UI layer above the raw native UI ABI. The helpers are deliberately system-shaped rather than catalog-shaped: session/frame setup, stable keyed reconciliation, rect/layout math, split/inset/center helpers, style color/metric/padding/spacing helpers, inherited color resolution, texture hex upload convenience, render helpers for boxes/text/resources, and event helpers for authored pointer state. There are still no baked runtime buttons, panels, or product components.

The raw C kernel gained two generic node-state flags, `hovered` and `pressed`, so Kain-authored interaction helpers can store common pointer state without turning the runtime into a widget system. `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` now covers those flags.

`runtime/fixtures/native_ui_stdlib_layer/main.kn` is the new fast proof fixture. It runs on the headless `software` backend and validates stdlib reconciliation, layout, style inheritance, rendering metadata, event draining, focus, and pointer state. The live `smoketest/native-ui/pilot` now uses the stdlib layer for session setup, reconciliation, layout, styles, rendering, and authored hover state while still producing a Win32/GL screenshot.

Validation:

- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm`
- `target\codex-native-ui-win32\debug\kain.exe build runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm --output target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe`
- `target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe`
- `.\smoketest\native-ui\pilot\run.ps1`

Recommended next step:

- Build a Kain-authored reconciler/state graph that can preserve authored node state across hot reload, then layer optional app-code controls above these generic helpers rather than adding a stdlib catalog of prewritten widgets.

# 2026-05-12 - Raw native UI now has a live Win32/GL presenter and screenshotable LLVM smoke

The raw native UI ABI is no longer metadata-only on Windows. `runtime/native/src/ui/kain_native_ui_system.c` now delegates live presentation through an internal host adapter layer, with `runtime/native/src/ui/kain_native_ui_host_win32_gl.c` providing the first non-blocking `win32-gl` backend. The core session/node/resource/event kernel remains generic; the backend only owns window creation, GL presentation, Win32 message translation, clipboard/menu/dialog bridging, and screenshot capture. `software` remains the headless metadata backend.

Two ABI upgrades landed with the presenter:

- `draw_text` now requires an explicit font resource handle, so text rendering stays resource-shaped instead of depending on a hidden host default.
- UI resources now support generic byte upload plus a Kain-friendly hex helper. `stdlib/native/ui.kn` exposes `native_ui_resource_set_bytes_hex(...)` and `native_ui_texture_create_from_hex(...)`, letting a single Kain file author texture-backed UI without a host-owned image catalog.

`smoketest/native-ui/pilot` is now a real end-to-end proof, not just an LLVM link test. `main.kn` authors a compact UI system in one Kain file, attaches `win32-gl`, renders authored rect/text/resource commands, captures `outputs/pilot.bmp`, and exits `0`. `run.ps1` resolves a local `kain.exe`, runs `kain check`, builds LLVM to `outputs/pilot.exe`, scans `pilot.ll` for raw native UI ABI calls, runs the executable with screenshot env vars, and verifies the BMP artifact.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\codex-native-ui-win32 -- --nocapture`
- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo build -p cli --target-dir target\codex-native-ui-win32`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_single_file\main.kn --target llvm`
- `target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_runtime_systems\main.kn --target llvm`
- `.\smoketest\native-ui\pilot\run.ps1`

Recommended next step:

- Keep the raw ABI generic and build the Kain-authored layout/style/reconciliation layer above it in stdlib. Future platform work should add more adapters behind the same host boundary rather than widening the C layer into baked widgets or a host-owned component catalog.

# 2026-05-12 - Canonical Kain input semantics landed

Kain now has a first-class input semantics lane instead of treating input as scattered stdin/UI/native helper calls. `crates/kain-input` owns typed source provenance, events, data-driven action/axis bindings, frame reduction, text commits, first-class `agent.intent` events, and deterministic trace serialization/replay. `crates/kain-core` registers interpreter bridge builtins under `kain_input_*`, with root `stdlib/input.kn` exposing the public `input_*` helpers.

Native LLVM/direct-C builds now load `stdlib/native/input.kn`, backed by `runtime/native/include/kain_native_input_system.h` and `runtime/native/src/core/kain_native_input_system.c`. The native kernel exposes sessions, bindings, event injection, frame reduction, action/axis/text queries, agent intent injection, trace export/replay, and last-status diagnostics. `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml` include the input kernel, and `platform.input` service metadata now describes canonical Kain input sessions rather than only Win32 capture.

Design decisions:

- Keep input as stdlib/runtime capability, not parser syntax. No `input` keyword.
- Public Kain code should consume frames/actions/axes/text commits, while raw events stay available for inspection.
- `agent.intent` is first-class source provenance in v1, not a test-only synthetic event.
- Target adapters should translate raw Win32/web/UE/UI/CLI/agent events into `kain-input`; they should not define app-facing input policy.

Validation:

- `cargo test -p kain-input --target-dir target\\codex-kain-input`
- `cargo check -p kain-core --target-dir target\\codex-kain-input-core`
- `cargo test -p kain-core test_stdlib_builtin_functions_exist --target-dir target\\codex-kain-input-core -- --nocapture`
- `cargo test -p kain-sys-codegen native_input --target-dir target\\codex-kain-input-codegen -- --nocapture`
- `bash runtime/conformance/input_runtime/run_tests.sh --verbose`
- `cargo build -p cli --target-dir target\\codex-kain-input-cli`
- `target\\codex-kain-input-cli\\debug\\kain.exe build runtime\\fixtures\\native_input_actions\\main.kn -t llvm` then run `runtime\\fixtures\\native_input_actions\\main.exe`
- `target\\codex-kain-input-cli\\debug\\kain.exe build runtime\\fixtures\\native_input_actions\\main.kn -t c` then run `runtime\\fixtures\\native_input_actions\\main.exe`

Recommended next step:

- Add thin adapters for live Win32 window messages and UI runtime event handoff into `kain_native_input_*`, then add web DOM and UE5 Enhanced Input adapters that emit the same source/action schema.

# 2026-05-11 - Raw native UI ABI gained host services for Kain-authored UI systems

The raw native UI kernel now covers the first real "Kain can author the UI system" layer without introducing a host-side widget catalog. `runtime/native/include/kain_native_ui_system.h` and `runtime/native/src/ui/kain_native_ui_system.c` now expose generic host frame presentation metadata, stable node keys for hot reload, accessibility labels/roles, font/texture/canvas/shader resource handles, text measurement, draw-resource commands, clipboard, IME, drag/drop, menu, dialog, and hot reload generation APIs. `stdlib/native/ui.kn` wraps those APIs and adds only generic layout/stable-node helpers; it does not define buttons, panels, or product components.

Design decisions:

- Keep the runtime capability-shaped. The runtime owns handles, buffers, metadata, event/system services, and host presentation; Kain source or Kain stdlib code owns layout systems, style cascades, reconciliation, controls, and app-specific components.
- Stable keys are the reload bridge. Kain-authored code can rebuild one file, call `native_ui_node_find_by_stable_key`, and preserve/reuse existing nodes without the C runtime knowing what a "button" or "panel" means.
- `runtime/fixtures/native_ui_runtime_systems/main.kn` is the focused proof shape: one Kain file creates authored nodes/resources, drives host services, presents draw commands, and exits successfully through the LLVM native path.

Validation:

- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_ui_host_services_without_component_catalog --target-dir target\\codex-native-ui-host-services -- --nocapture`
- `cargo build -p cli --target-dir target\\codex-native-ui-host-services-cli`
- `target\\codex-native-ui-host-services-cli\\debug\\kain.exe check runtime\\fixtures\\native_ui_runtime_systems\\main.kn --target llvm`
- `target\\codex-native-ui-host-services-cli\\debug\\kain.exe build runtime\\fixtures\\native_ui_runtime_systems\\main.kn --target llvm --output target\\codex-native-ui-host-services\\native_ui_runtime_systems.exe`
- `target\\codex-native-ui-host-services\\native_ui_runtime_systems.exe`

Recommended next step:

- Attach `kain_native_ui_host_present` to a live pixel backend (Win32/Direct2D, Skia, wgpu, Qt, or another host) that consumes the existing draw/resource buffers, then build Kain-authored layout/style/reconciliation in stdlib above this ABI rather than widening the C layer into widgets.

# 2026-05-11 - Raw native graphics kernel exposes engine-building primitives to Kain

Kain now has a generic native graphics system kernel at the C ABI floor instead of relying on runtime-authored scenes or host-side primitive/default-scene behavior. `runtime/native/include/kain_native_graphics_system.h` and `runtime/native/src/core/kain_native_graphics_system.c` expose low-level sessions, backend target selection, truthful backend availability/status probes, SPIR-V shader module registration, authored buffer handles, mesh handles, pipeline handles, draw command recording, frame present bookkeeping, and diagnostics. `stdlib/native/graphics.kn` exposes thin `native_graphics_*` wrappers for LLVM/direct-C Kain source.

Design decisions:

- Keep this layer catalog-free. The runtime knows handles, backend target ids, SPIR-V byte counts, buffer metadata, mesh counts, pipelines, draw commands, and diagnostics; Kain source owns engine policy, scenes, primitive recipes, simulation loops, materials, cameras, and tools.
- Vulkan and DirectX 12 are first-class backend targets in the access layer, but direct command execution is reported as unavailable/degraded until a real backend executor is attached. Do not claim vendor-direct rendering based only on target selection.
- `runtime/fixtures/native_graphics_engine/main.kn` is the focused LLVM proof shape: one Kain file creates two different authored graphics submissions through the same raw kernel without runtime-provided geometry.
- The language-wide rule is now explicit in `ARCHITECTURE.md`: native/Rust/C code provides capabilities, ABI substrate, validation, diagnostics, and target integration; Kain authors behavior and systems.

Validation:

- `bash runtime/conformance/graphics_runtime/run_tests.sh --verbose`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_graphics_engine_primitives_without_scene_catalog --target-dir target\\codex-native-graphics-kernel -- --nocapture`
- Build and run `runtime/fixtures/native_graphics_engine/main.kn` through the LLVM native fixture path after rebuilding the CLI.

Recommended next step:

- Attach the raw graphics command buffer to a real Vulkan or DirectX 12 executor behind the same `kain_native_graphics_*` handles, then add backend-specific conformance that proves actual frame execution without widening the Kain-facing API.

# 2026-05-11 - Raw native UI C ABI makes single-file LLVM UI authoring possible

Kain now has a generic native UI system kernel at the C ABI floor instead of another host-authored component catalog. `runtime/native/include/kain_native_ui_system.h` and `runtime/native/src/ui/kain_native_ui_system.c` expose low-level sessions, arbitrary node kind strings, parent/rect/text/style/flag mutation, focus, hit testing, dirty tracking, event polling, and draw-command buffers. The source is included in both `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`, and `stdlib/native/ui.kn` exposes thin `native_ui_*` wrappers for LLVM/direct-C Kain source.

Design decisions:

- Keep this layer catalog-free. The runtime knows handles, strings, geometry, events, and commands; Kain source or `stdlib/native` owns higher-level buttons, panels, inspectors, tabs, and app-specific UI systems.
- `runtime/fixtures/native_ui_single_file/main.kn` is the current proof shape: one Kain file defines its own surface helper functions, creates arbitrary UI node kinds, draws, routes focus/events, hit-tests, and returns success through LLVM-compatible calls.
- `runtime/conformance/ui_runtime/test_native_ui_system_kernel.c` proves the raw C ABI without involving the older compiled-bundle overlay path.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_single_file_native_ui_primitives_without_component_catalog --target-dir target\\codex-native-ui-system -- --nocapture`
- `bash runtime/conformance/ui_runtime/run_tests.sh --verbose`

Recommended next step:

- Connect `kain_native_ui_system` to an actual Win32/bgfx or Qt host frame loop so `native_ui_draw_*` buffers can present pixels live, then add hot reload by rebuilding the single Kain file and replaying session state through stable node ids.

# 2026-05-11 - kain-ui-native archive and legacy feature were removed

Follow-up cleanup removed the `crates/kain-ui-native/src/archive` museum, the `legacy-egui` Cargo feature, and the optional egui/wgpu/font/image/nalgebra/kain-3D dependencies from `kain-ui-native`. The active crate should only carry `app.rs`, `session.rs`, `qt_host.rs`, `lib.rs`, and `main.rs`; old host implementations should be deleted, not archived in this crate.

Validation:

- `cargo fmt -p kain-ui-native`
- `cargo test -p kain-ui-native --target-dir target\\codex-kain-ui-native-slim`
- `cargo check -p kain-ui-native --target-dir target\\codex-kain-ui-native-slim-check`

# 2026-05-11 - Blade resolver crate import surface renamed to `blade`

The Blade workspace resolver package now imports as `blade`, so Rust call sites use `use blade::...` instead of `use kain_blades::...` or `use kain_blade::...`. The source folder remains `crates/kain-blades`, the workspace member path remains `crates/kain-blades`, and user/workspace folders remain plural (`blades/*`). CLI naming also remains plural where it refers to collections: `kain blades ...`; the standalone executable remains `blade`.

Design decision:

- Treat `blade` as the public Rust crate identity for Blade discovery/resolution APIs. Treat `crates/kain-blades` as only the repository folder name.
- Do not rename workspace folder conventions from `blades/*`; only the Rust crate/package identity changed.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p blade -p kain-build -p kain-core -p kain-c-ffi -p kain-crate-ffi -p kain-host -p kain-omni -p cli --target-dir target\codex-blade-singular`
- `cargo check --manifest-path labs\blades_workspace_smoke\crates\synthetic_reporter\Cargo.toml --target-dir target\codex-blade-singular-lab`

# 2026-05-11 - kain-ui-native became an authored UI host instead of a demo catalog

`crates/kain-ui-native` now follows the same ownership rule as `kain-3D`: Kain source owns UI structure and intent; Rust/native owns host launch, manifest projection, validation, and low-level rendering/diagnostics. The active non-egui path is split into `app.rs`, `session.rs`, and `qt_host.rs`; the old demo/catalog Qt path and legacy egui monolith were deleted from the crate after the follow-up cleanup.

Design decisions:

- Do not synthesize document/viewport/browser/shader/devtools placeholder panes when a bundle emits no authored UI.
- Do not add Rust-side UI catalogs, renderer switchboards, sample dashboards, or default widget layouts to `kain-ui-native`.
- `KainUiNativeSessionManifest` should carry authored surfaces plus the native projection generated from `UiBuildOutput`; the Qt host may render that projection generically, but it must not invent app content.
- Native C overlay fields are diagnostic-only (`diagnostic_title`, `diagnostic_subtitle`, `diagnostic_hint`) and compiled UI bundles take precedence over diagnostic labels.

Validation:

- `cargo fmt -p kain-ui-native`
- `cargo test -p kain-ui-native --target-dir target\\codex-kain-ui-native`
- `cargo check -p kain-ui-native --target-dir target\\codex-kain-ui-native-check`
- `cargo check -p kain-ui-native --features legacy-egui --target-dir target\\codex-kain-ui-native-legacy-check`

Recommended next step:

- Move richer native UI rendering behind authored Kain primitives and bundle metadata, then add a smoke that renders two visually different Kain-authored UIs through the same host to prove Rust is no longer deciding the layout.

# 2026-05-11 - Blade smoke workspace became the Singularity Atlas executable proof

`labs/blades_workspace_smoke` is now a full Blade workspace proof instead of a lightweight demo. The lab still exercises root workspace discovery, `apps/*`, `blades/*`, `crates/*`, C ABI, Rust crate, Kain, Fabric, GPU, and synthetic Cargo blades, but it now also builds and runs a real executable named `blade_singularity_atlas`.

What changed:

- The `gpu-compute` blade emits three Kain-authored shader artifacts: `gpu_step`, `nebula_field`, and `spectral_lattice`, with SPIR-V, HLSL, reflection JSON, and shader bundle outputs validated by the smoke runner.
- The synthetic Cargo blade now depends on `blade` and `kain-fs`, builds `blade_singularity_atlas`, discovers the Blade workspace graph, reads GPU artifacts through `kain-fs`, and renders an atlas report as SVG, PPM, JSON, and HTML under `outputs/singularity-atlas`.
- `scripts/run_blades_smoke.py` now executes the built binary from `.kain/build`, validates the atlas output, checks the expected compute keys, and still proves cache reuse and clean lab cache rebuilds.

Design decisions:

- Keep executable smoke artifacts produced by real Blade build tasks. The lab runner may validate and run them, but it should not become a replacement build system.
- Runtime admire/report outputs can live under `outputs/` when they are produced by the built executable; build artifacts, stamps, and build reports still belong under `.kain/build`, `.kain/cache/build`, and `.kain/reports/build`.
- Current GPU artifact generation accepts sample-based Float math in these smoke shaders; avoid unsupported `Float(index)`-style casts until the shader compiler surface explicitly supports them.

Validation:

- `cargo check --manifest-path labs\blades_workspace_smoke\crates\synthetic_reporter\Cargo.toml --target-dir target\codex-blade-atlas-check`
- `$env:KAIN_BIN=(Resolve-Path target\codex-fs-unified\debug\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\codex-fs-unified\debug\blade.exe).Path; python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --clean-cache`

# 2026-05-11 - kain-3D primitives moved to Kain-authored mesh ingestion

`crates/kain-3D` no longer carries a Rust-backed primitive catalog or procedural shape builders. Primitive support is now an authored mesh pipeline: Kain/source data owns the actual vertices, indices, normals, UVs, and primitive recipes; Rust validates and converts that data into `Geometry`, `Mesh`, scene metadata, and host/runtime values.

What changed:

- Replaced the old Rust shape-definition/default-library stack with `AuthoredPrimitive`, `AuthoredPrimitiveRegistry`, and validation errors in `crates/kain-3D/src/primitive.rs`.
- Removed Rust shape factories for box, plane, spheres, cylinder, cone, capsule, and torus. Generic mesh helpers such as `Geometry::indexed_triangle_mesh` remain because they do not encode product primitives.
- Replaced the Kain prelude's shape-specific native functions with `triangle_geometry(...)` / `mesh_geometry(...)` over explicit authored arrays, backed by the generic `__zen3d_triangle_geometry` runtime native.
- Updated `Scene` to register authored primitive registries without manufacturing default shape definitions.
- Updated smoke/test fixtures to use explicit fixture mesh data instead of the removed primitive factories.

Design decision:

- Going forward, do not add Rust-side primitive recipes to `kain-3D`. If Kain needs a cube, sphere, bevelled block, or generated modeling primitive, author that recipe in Kain/source assets and pass explicit mesh data through the generic pipeline.

Validation target:

- Run `cargo test -p kain-3d --target-dir target\\codex-kain-3d-authored-primitives-test` and `cargo check -p kain-3d --bins --lib --target-dir target\\codex-kain-3d-authored-primitives-check` after touching this lane.

# 2026-05-11 - Blade, Fabric, FFI, import, and codebase IO moved onto kain-fs

The Blade workspace pipeline and its adjacent import/FFI/workspace helpers now consume the shared `kain-fs` crate instead of carrying their own `std::fs` behavior.

What changed:

- Wired `kain-fs` into `kain-build`, `kain-blades`, `kain-check`, `kain-test`, `kain-omni`, `kain-host`, `kain-c-ffi`, `kain-crate-ffi`, `kain-import`, and `kain-codebase`.
- Migrated Blade build artifacts, cache stamps, report JSON/JSONL, safe clean, input hashing, C sidecar copying, GPU artifact writes, Fabric manifest/report IO, Omni staging, check/test source discovery, C/Rust FFI generated artifacts, importer source reads, and trusted-local codebase file helpers onto `kain-fs`.
- Kept raw `std::fs` out of the core Blade/build/check/test/host/omni/FFI/import/codebase lanes, except for literal type names and lower-level surfaces outside this pass such as UE/vendor/demo/runtime adapters.
- Fixed the host Fabric test helper to use `kain_fs::DirectoryEntry.file_name` and verified the full `/labs/blades_workspace_smoke` against rebuilt `kain.exe` and `blade.exe`.

Design decisions:

- `kain-fs` is now the expected filesystem owner for artifact-producing and workspace-scanning Kain crates, not only for in-language `fs_*` calls.
- Generated reports and artifacts should prefer `kain_fs::atomic_write_text` / `atomic_write_bytes` when replacing complete files; append-only event streams should use `append_text`.
- FFI/import crates map `FsError` into their existing crate-level error surfaces rather than leaking raw `std::io::Error` conversions from each call site.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p kain-codebase -p kain-import -p kain-c-ffi -p kain-crate-ffi -p kain-build -p kain-blades -p kain-check -p kain-test -p kain-omni -p kain-host --target-dir target\\codex-fs-unified`
- `cargo test -p kain-blades -p kain-build -p kain-check -p kain-test --target-dir target\\codex-fs-unified -- --nocapture`
- `cargo test -p kain-codebase -p kain-import --target-dir target\\codex-fs-unified -- --nocapture` (`kain-codebase` passed; `kain-import` still has 5 pre-existing transformer test failures unrelated to file IO)
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-crate-ffi --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-c-ffi --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\\codex-fs-unified -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-fs-unified`
- `$env:KAIN_BIN=(Resolve-Path target\\codex-fs-unified\\debug\\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\\codex-fs-unified\\debug\\blade.exe).Path; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py --clean-cache`

Current risks:

- Broad repo scans still find raw `std::fs` in CLI packagers, UI/demo apps, native/runtime adapters, vendor code, and UE-facing lanes. Those were intentionally left alone unless they were part of the core Blade/import/FFI/workspace unification.
- `kain-import` has unrelated C/Rust transformer unit failures in the current checkout; avoid treating those as filesystem regressions without checking the specific failing transformer assertions first.

Recommended next step:

- Continue migrating high-value artifact producers such as `kain-driver` native/Tauri app materialization and non-UE CLI import/packaging paths onto `kain-fs`, then add a small lint/check script that fails new raw `std::fs` use in the core Kain FS-owned lanes.

# 2026-05-11 - LLVM native semantic handles and intent runtime hooks landed

Kain's LLVM native lane now preserves the core semantic shapes that were previously erased for smoke-test convenience.

What changed:

- LLVM maps `Option<T>`, `Result<Ok, Err>`, and `Future<T>` to native tagged `i8*` handles instead of lowering them as plain payload types.
- Added native C facade constructors, tag checks, payload-copy helpers, ready-future creation, await payload extraction, async sleep future creation, and stdlib wrappers for runtime visibility.
- Wired `runtime/native/src/core/kain_runtime_async.c` into `runtime/native_core_runtime.toml` so lean LLVM file builds have the async substrate available.
- Added LLVM lowering for `Some`, `None`, `Ok`, `Err`, `is_some`, `is_none`, `is_ok`, `is_err`, `ok`, `unwrap`, `expect`, `unwrap_or`, `await`, `async`, and `?` for the native tagged path.
- Added native runtime hooks for patch begin/record/commit/undo visibility, entangle propagation records, converge mismatch recording, and orchestrate stage begin/end counters.
- Strengthened `converge` LLVM lowering so a fast lane emits alongside the spec lane, records verification status, returns the fast result on match, and falls back to spec on mismatch.
- Tightened frontend scalar compatibility so TypeScript-import scalar comparison leniency no longer makes ordinary return values, match arms, lets, or arguments type-compatible.
- Added `runtime/fixtures/native_option_result_future/main.kn` and expanded `runtime/fixtures/native_world_actor_intent/main.kn` to prove the native semantic/runtime counters through real LLVM builds.

Design decisions:

- The tagged C ABI is a pragmatic bridge: semantic handles stay visible across LLVM/native boundaries while payload extraction matures beyond scalar-heavy paths.
- `?` residual propagation in LLVM currently returns the existing native `Option`/`Result` handle from functions whose native return ABI is `i8*`.
- Intent runtime hooks are process-local observability and parity helpers, not durable crash-safe journals yet.
- Direct C was intentionally not expanded in this pass; it still trails LLVM for arrays, tuples, match, closures, ranges, fstrings, payload enums, generics, semantic options/results/futures, and typed actor lowering.

Validation:

- `cargo test -p kain-core --test semantic_typecheck_test --target-dir target\\codex-actor-runtime-cli -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-actor-runtime-cli -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-actor-runtime-cli`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_option_result_future\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_option_result_future\\main.kn -t llvm -o target\\codex-native-runtime-proofs\\native_option_result_future.ll`
- `target\\codex-native-runtime-proofs\\native_option_result_future.exe`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-native-runtime-proofs\\native_world_actor_intent.ll`
- `target\\codex-native-runtime-proofs\\native_world_actor_intent.exe`

Current risks:

- Tagged payload ownership is conservative and can leak nested RC-managed payloads; the next native ABI pass should add payload destructors or type-aware retain/release callbacks.
- Pattern payload binding and `unwrap` extraction in LLVM currently target scalar payloads first. Struct, tuple, slice, array, and nested semantic payloads need targeted fixtures before calling the lane complete.
- Ready futures are enough for `async { value }` and await payload proof, but a full scheduler/poll/waker/timer model still needs end-to-end Kain syntax and stdlib coverage.
- Patch undo/replay is a visibility hook, not semantic transaction rollback parity with the interpreter.

Recommended next step:

- Add a table-driven native semantic conformance suite that cross-runs interpreter and LLVM cases for every `Option`/`Result`/`Future` payload class, then deepen the C facade lifecycle model before broadening actor/future scheduling semantics.

# 2026-05-11 - Native actor ABI contract and C runtime hardening landed

Kain's native actor lane now has an executable ABI contract instead of relying on matching comments between Rust, LLVM IR, and C headers.

What changed:

- Expanded `crates/kain-actor/src/native.rs` into the canonical Rust-side native actor ABI descriptor: ABI version, actor ID width, invalid ID, mailbox defaults, ask/shutdown timing, supervision restart window, actor name/table/registry/scheduler capacities, monitor notification tag base, required C runtime symbols, required native stdlib actor symbols, and the native message/spawn-config layout.
- Added actor header parity tests in `kain-actor` that read `runtime/native/include/kain_runtime_actor.h` and `kain_runtime_native_stdlib.h`, so Rust model constants and C ABI symbols drift loudly.
- Added `KainActorAbiDescriptor`, `kain_actor_abi_descriptor`, and `kain_actor_abi_descriptor_is_compatible` to the native actor runtime.
- Added explicit `retain_user_data` ownership to `KainActorSpawnConfig` and `KainActorSpawnConfigStored`. Native C/C++ callers now default to plain borrowed `user_data`, while LLVM actor lowering sets `retain_user_data = 1` for Kain RC-managed actor state.
- Fixed native mailbox payload-size retention by storing `data_size` in `MessageNode`; `kain_actor_receive` and `kain_actor_try_receive` now return the original payload size.
- Hardened shutdown-before-first-run behavior: actors closed while still queued now finalize lifecycle side effects, including monitor notifications, supervisor observations, and link propagation when appropriate.
- Added `runtime/conformance/actor_runtime/test_actor_abi_contract.c` and wired it into the actor runtime conformance runner. The test covers ABI descriptor compatibility, spawn defaults, message size retention, registry, monitor notification tags, links, supervision snapshots, and scheduler stats.
- Exposed native actor constants through `runtime/native/include/kain_runtime_native_stdlib.h`, `runtime/native/src/core/kain_runtime_native_stdlib.c`, and `stdlib/native/actor.kn`, then updated `runtime/fixtures/native_world_actor_intent/main.kn` to prove them through LLVM and direct C.
- Updated LLVM actor spawn layout to include `retain_user_data` and made `crates/kain-sys-codegen` depend directly on `kain-actor` for actor ABI sizing.

Design decisions:

- `retain_user_data` is the ABI boundary between compiler-owned Kain RC state and arbitrary host/C/C++ pointers. Do not reintroduce unconditional `rc_retain`/`rc_release` on `user_data`.
- C actor ABI compatibility should be checked through `KainActorAbiDescriptor` and the `kain-actor` parity tests, not only by eyeballing struct comments.
- Native actor stdlib wrappers should expose stable constants where Kain source needs to reason about runtime behavior.

Validation:

- `cargo fmt -p kain-actor -p kain-sys-codegen`
- `cargo test -p kain-actor --target-dir target\\codex-actor-runtime`
- `cargo test -p kain-core --test actor_contract_test --target-dir target\\codex-actor-runtime-core`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\\codex-actor-runtime-core -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_generates_actor_spawn_and_send_message_paths --target-dir target\\codex-actor-runtime-codegen -- --nocapture`
- `bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose`
- `target\\codex-actor-runtime\\native_stdlib_bridge.exe`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-actor-runtime-cli`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target llvm`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-actor-runtime\\native_world_actor_intent.ll`
- `target\\codex-actor-runtime\\native_world_actor_intent.exe`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe check runtime\\fixtures\\native_world_actor_intent\\main.kn --target c`
- `target\\codex-actor-runtime-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t c -o target\\codex-actor-runtime\\native_world_actor_intent_c.c`
- `target\\codex-actor-runtime\\native_world_actor_intent_c.exe`

Current risks:

- Direct C actor lowering still uses the generic native actor facade rather than generated per-actor handler loops. The runtime substrate is much sturdier now, but specialized direct-C actor semantics remain a deeper compiler pass.
- The native actor conformance runner is Bash-first. It works under the current Windows Git Bash environment, but a PowerShell wrapper would make the Windows lane easier for future agents.

Recommended next step:

- Promote the actor conformance runner plus LLVM/direct-C native fixture into one repo-local smoke command, then add generated per-actor direct-C handler lowering on top of the now-explicit ABI contract.

# 2026-05-11 - Kain FS v2 added sandboxed virtual roots, streaming, watchers, and transactions

Kain's filesystem lane now has a real v2 substrate on top of the initial `kain-fs` crate work.

What changed:

- Added focused `crates/kain-fs` modules for scoped capabilities and virtual mounts (`capabilities.rs`), range/chunk streaming IO (`streaming.rs`), portable polling watchers (`watch.rs`), and best-effort transactional journals with rollback (`transaction.rs`).
- Extended `crates/kain-core/src/runtime.rs` with runtime-owned `FsSandbox`, watcher, and transaction registries plus globals for capability grants/revokes, `fs://` mount resolution, ranged text/byte IO, hex-encoded byte helpers, streaming copy, watcher polling/close, and transaction begin/write/append/remove/copy/move/commit/rollback.
- Registered the new filesystem-facing types and globals in `crates/kain-core/src/types.rs` and `crates/kain-core/src/stdlib.rs`, including `FsChunk`, `FsWatchEvent`, and `FsJournalEntry`.
- Expanded `stdlib/native/fs.kn` and the native C facade in `runtime/native/include/kain_runtime_native_stdlib.h` / `runtime/native/src/core/kain_runtime_native_stdlib.c` with ranged text reads, byte hex reads/writes, metadata text, newline-delimited directory/walk path listings, and streaming copy.
- Updated `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` and `runtime/fixtures/native_fs/main.kn` so direct C, LLVM, and the raw C facade prove the richer filesystem surface.
- Updated the local `kain-fs-pipeline` skill so future agents know the v2 source files, validation commands, and native ABI caveats.

Design decisions:

- `kain-fs` stays the semantic owner. `kain-core` owns process-local runtime handles and Kain-visible globals; `stdlib/native` and the C facade expose ABI-compatible native target wrappers.
- Scoped v2 interpreter helpers resolve through `FsSandbox` before touching the host filesystem. Existing v1 helpers are intentionally not all retrofitted yet, so future work should migrate old `fs_*` calls through the same resolver if virtual roots need universal coverage.
- Native byte arrays and rich records are encoded as lowercase hex, key-value metadata text, and newline-delimited path lists for now because the C ABI does not yet have a clean typed array/record/result story for these values.
- Watchers are portable polling watchers rather than OS notification backends. Transactions are process-local and best-effort rollback journals, not durable crash-safe multi-file commits yet.

Validation:

- `cargo test -p kain-fs --target-dir target\\codex-kain-fs-v2`
- `cargo test -p kain-core filesystem --target-dir target\\codex-kain-fs-v2-core`
- `cargo test -p kain-sys-codegen --test c_codegen_test --target-dir target\\codex-kain-fs-v2-codegen-c -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-kain-fs-v2-codegen-llvm -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-kain-fs-v2-cli`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-kain-fs-v2-native\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-kain-fs-v2-native\\native_stdlib_bridge.exe`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe check runtime\\fixtures\\native_fs\\main.kn --target c`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t c -o target\\codex-kain-fs-v2-native\\native_fs_c.c`
- `target\\codex-kain-fs-v2-native\\native_fs_c.exe`
- `target\\codex-kain-fs-v2-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t llvm -o target\\codex-kain-fs-v2-native\\native_fs.ll`
- `target\\codex-kain-fs-v2-native\\native_fs.exe`

Current risks:

- The v2 sandbox resolver is not yet universal across every older v1 interpreter `fs_*` helper.
- The native parity wrappers intentionally use text/hex encodings until native typed records/results/arrays mature.
- Watchers should eventually gain platform-native backends, and transactions should eventually gain durable crash-safe journaling if they become part of `patch` / `law` / `converge` workflows.
- The direct C backend still emits harmless extra-parentheses comparison warnings in generated C.

Recommended next step:

- Retrofit the older v1 interpreter `fs_*` helpers through `FsSandbox`, then add an explicit capability manifest model (`fs.read`, `fs.write`, `fs.project`, `fs.temp`, `fs.watch`, `fs.transaction`) so Kain programs can declare filesystem access instead of inheriting the runtime default.

# 2026-05-11 - Dedicated kain-fs crate and native filesystem pipeline landed

Kain now has a real filesystem substrate instead of scattered file/path helpers.

What changed:

- Added `crates/kain-fs` as a workspace crate for portable file operations, path helpers, metadata, directory entries, directory walks, temp paths, atomic writes, copy/move/remove operations, SHA-256 file hashes, and typed `FsError` values.
- Wired `crates/kain-core` to depend on `kain-fs` and expose first-class `fs_*` runtime globals. Strict variants raise runtime errors, while `fs_try_*` variants return structured `Result` values.
- Added typed filesystem registry data in `crates/kain-core/src/types.rs` and `crates/kain-core/src/stdlib.rs` so interpreter, type metadata, and native codegen see the same global function surface.
- Added `stdlib/native/fs.kn` plus native C facade functions in `runtime/native/include/kain_runtime_native_stdlib.h` and `runtime/native/src/core/kain_runtime_native_stdlib.c` so LLVM and direct C builds can perform real file operations without depending on the generic root stdlib.
- Extended `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` and added `runtime/fixtures/native_fs/main.kn` to prove temp directories, path joins, text writes/appends/reads, copy/move/atomic write, SHA-256 hashing, and recursive removal through native C and generated LLVM/direct-C executables.
- Tightened `crates/kain-sys-codegen` so the C backend lowers string equality through `strcmp`, and LLVM trusts explicit target-stdlib wrapper signatures instead of inferring wrong ABIs for Kain-defined native wrappers.

Design decisions:

- `kain-fs` owns portable semantics; `kain-core` owns how those semantics appear as Kain runtime globals; `stdlib/native` and the C facade own native target exposure.
- `fs_hash_file` is SHA-256 in both Rust and native C lanes. Do not replace one side with a faster non-cryptographic hash unless the API name and docs change together.
- Native target stdlib wrappers are ordinary Kain functions over a C ABI facade. LLVM must skip external declarations for stdlib functions that are defined by loaded target stdlib source, or native builds can produce duplicate declarations/definitions.
- `StdLib::new` return types matter for LLVM lowering. New native-callable filesystem helpers should not be left as `Any` when they return strings, integers, booleans, or units.

Validation:

- `cargo test -p kain-fs --target-dir target\\codex-kain-fs`
- `cargo test -p kain-core filesystem --target-dir target\\codex-kain-fs-core`
- `cargo test -p kain-sys-codegen --test c_codegen_test --target-dir target\\codex-kain-fs-codegen-c -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\\codex-kain-fs-codegen-llvm -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-kain-fs-cli`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-kain-fs-native\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-kain-fs-native\\native_stdlib_bridge.exe`
- `target\\codex-kain-fs-cli\\debug\\kain.exe check runtime\\fixtures\\native_fs\\main.kn --target c`
- `target\\codex-kain-fs-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t c -o target\\codex-kain-fs-native\\native_fs_c.c`
- `target\\codex-kain-fs-native\\native_fs_c.exe`
- `target\\codex-kain-fs-cli\\debug\\kain.exe build runtime\\fixtures\\native_fs\\main.kn -t llvm -o target\\codex-kain-fs-native\\native_fs.ll`
- `target\\codex-kain-fs-native\\native_fs.exe`

Current risks:

- The native facade currently exposes a useful v1 subset: text/path/temp/hash/copy/move/remove/status. The Rust crate already has richer metadata and directory-walk APIs that need native wrappers if Kain code should call them from LLVM/direct-C.
- Several complex filesystem values still flow as `Any` in the stdlib registry until Kain's typed record/result story is strengthened.
- The direct C backend still emits harmless extra-parentheses comparison warnings in generated C.

Recommended next step:

- Add a manifest-driven filesystem smoke under `smoketest/` or `runtime/fixtures` that runs the Rust interpreter, direct C, and LLVM filesystem lanes from one command, then expand native wrappers for directory listing and structured metadata.

# 2026-05-11 - Blade build system v1 landed in kain-build

Kain now has a real blade workspace build orchestrator instead of lab-local build scripts.

What changed:

- Added `crates/kain-build/src/workspace.rs` as the typed Blade build planner/executor. It discovers a blade workspace through `kain-blades`, builds a DAG, topologically orders tasks, stamps cacheable work, and emits JSON/JSONL build reports.
- `kain-build` now owns adapters for C shared libraries, Cargo manifests, GPU shader artifacts, Kain source checks, Fabric validation/runs, and explicit Node/Bun/custom tasks declared in `[[build.tasks]]`.
- Extended `KAIN.toml` blade metadata with `[build] artifact_root`, `cache_root`, `profile`, and `[[build.tasks]]`, and extended C FFI library metadata with `sources`.
- Added `kain blades build .` plus a standalone `blade build .` binary. Both support `--json`, `--dry-run`, `--clean`, `--profile`, `--target`, and `--include-vulkan`.
- Reworked `labs/blades_workspace_smoke` so its runner invokes `blade build . --json` instead of compiling the C sidecar itself. The smoke now proves cold builds, cache hits, C sidecar materialization, Cargo blades, GPU artifacts, CPU Fabric execution, GPU Fabric validation, and `kain blades/equip` inspection.
- Fixed the shared Node bridge process boundary on Windows by stripping `\\?\` verbatim prefixes before spawning Node. Fabric Node steps were otherwise able to fail before the bridge script could answer.

Design decisions:

- Build products are workspace-local and disposable: `.kain/build/<profile>/<target>/...` for canonical artifacts, `.kain/cache/build/stamps` for fingerprints, and `.kain/reports/build` for build reports/events.
- `kain-blades` still owns discovery and manifest resolution; `kain-build` owns build graph planning and execution. Callers should not rescan `blades/*`, `apps/*`, or `crates/*`, and labs should not carry custom build scripts for artifacts the build graph can own.
- Fabric GPU manifests are validated by default and only run when `--include-vulkan` is passed, because local machines may not have a working Vulkan compute runtime.
- Safe clean is intentionally narrow: `--clean` removes only workspace-local `.kain` artifact/cache/report roots.

Validation:

- `cargo check -p kain-build --target-dir target\\codex-blade-build`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo check -p cli --target-dir target\\codex-blade-build-cli`
- `cargo test -p kain-blades --target-dir target\\codex-blade-test-blades`
- `cargo test -p kain-build --target-dir target\\codex-blade-test-build`
- `cargo test -p kain-node process_portable_path_strips_windows_verbatim_prefix --target-dir target\\codex-blade-test-node`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blade-build-cli`
- `$env:KAIN_BIN=(Resolve-Path target\\codex-blade-build-cli\\debug\\kain.exe).Path; $env:BLADE_BIN=(Resolve-Path target\\codex-blade-build-cli\\debug\\blade.exe).Path; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`
- Same smoke with `--clean-cache`

Current risks:

- `kain-build` v1 is sequential. The DAG and cache fingerprints are ready for parallel scheduling, but execution currently stays simple and deterministic.
- Explicit `[[build.tasks]] depends_on` handling is intentionally conservative and needs a deeper pass before complex cross-blade user-authored dependency aliases become a public contract.
- JSON output can still be preceded by lower-layer compiler/runtime chatter. The lab smoke extracts the final JSON payload robustly, but a future CLI quiet mode would be cleaner.

Recommended next step:

- Add parallel task execution with a small scheduler and stable report ordering, then promote a blade-build CI lane that runs the clean-cache lab smoke from freshly built `kain` and `blade` binaries.

# 2026-05-11 - Dedicated kain-actor crate landed as actor-system foundation

Kain now has a real `crates/kain-actor` crate instead of keeping all actor-system vocabulary hidden inside `kain-core`.

What changed:

- Added `crates/kain-actor` to the workspace with focused modules for actor IDs, addresses/paths, messages, actor definitions, mailbox policy, lifecycle, supervision, scheduler policy, behavior contracts, registry snapshots, actor-system validation, runtime snapshots/events, and native ABI descriptors.
- Kept `kain-actor/src/lib.rs` as a public index only. Future actor work should extend the focused module that owns the concept instead of growing a giant lib file.
- Wired `kain-core` to consume `kain-actor` for `ActorId`, `ActorIdAllocator`, `MessageEnvelope<Value>`, default ask timeout, and typed actor contracts.
- `TypedActor` now carries `actor_contract: kain_actor::ActorDefinition`, built during typechecking from resolved actor state slots, handler message parameters, and actor method signatures.
- Actor contract validation now catches duplicate handler names, duplicate state slots, duplicate method names, invalid message/parameter shapes, and supervisor child mistakes through reusable `kain-actor` validators.
- Runtime-contract reflection now emits actor message names from the shared actor contract instead of leaving actor reflection empty.
- Added focused tests for the actor crate model and for `kain-core` actor contract construction/duplicate-handler rejection.

Design decisions:

- `kain-core` still owns actor syntax, AST, typechecking, and interpreter execution. `kain-actor` owns reusable actor-system model data that can be consumed by core, native runtime work, LLVM/direct-C lowering, IDE tooling, and future stdlib layers.
- The first crate pass is deliberately model/contract-heavy, not a replacement scheduler. That gives supervision, mailbox, behavior, registry, and native ABI work stable files to extend without destabilizing existing interpreter semantics.
- Actor IDs now reserve raw `0` as invalid so Rust actor model data stays aligned with the native C runtime ABI.

Validation:

- `cargo fmt -p kain-actor -p kain-core`
- `cargo test -p kain-actor --target-dir target\\codex-kain-actor`
- `cargo test -p kain-core --test actor_contract_test --target-dir target\\codex-kain-actor-core`
- `cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\\codex-kain-actor-core`
- `cargo test -p kain-core actor --target-dir target\\codex-kain-actor-core` was also attempted. The actor contract/runtime cases passed, but the broad filter failed on existing missing fixture `m:/Code/Factory/Example_GAS/test_targets.kn` in `test_target_actor_parser`.

Current risks:

- `kain-actor` is now the correct home for actor-system model expansion, but the interpreter still runs actor loops in `kain-core/src/runtime.rs`. Moving scheduling/mailbox execution behind reusable runtime traits should be a separate, careful pass.
- Direct C and LLVM actor lowering can consume the new native ABI descriptors, but generated specialized per-actor handler loops are still future work.
- There are unrelated dirty filesystem/blades/native-runtime changes in this checkout. Do not stage or revert them as part of actor work.

Recommended next step:

- Add a second pass that gives `kain-actor` executable mailbox/supervision runtime traits, then have `kain-core` delegate spawn/send/ask through those traits while native LLVM/C lowering consumes the same actor contract metadata.

# 2026-05-11 - Native stdlib and runtime facade landed for LLVM and direct C

Kain now has a target-scoped native stdlib profile and C ABI facade that let actor, entangle, patch, law, converge, orchestrate, world, timing, diagnostics, and runtime helpers compile through both `-t llvm` and `-t c`.

What changed:

- Added `stdlib/native` as the shared native target stdlib profile for LLVM and direct C, plus `stdlib/c` as the direct C bridge layer. `crates/kain-core/src/stdlib.rs` loads all matching profiles for a target, so C gets `native` then `c`, while LLVM gets `native` only.
- Added `runtime/native/include/kain_runtime_native_stdlib.h` and `runtime/native/src/core/kain_runtime_native_stdlib.c` as the narrow C ABI facade for native Kain stdlib calls. It wraps runtime init/shutdown, actor registry/spawn/send/scheduler helpers, entangle registry helpers, status/diagnostics, and timing.
- Added `runtime/native_core_runtime.toml` as the default lean native runtime manifest for normal LLVM/direct-C file builds. The broader `runtime/native_runtime.toml` remains the app/vendor manifest and now also includes the native stdlib facade source.
- Updated `crates/cli/src/main.rs` so native builds prefer `runtime/native_core_runtime.toml` before the broad manifest, and only stage the GPU runtime DLL when the LLVM artifact stage actually produced compute residency payloads.
- Updated `crates/cli/src/llvm_native_stage.rs` so shader artifact staging only runs for source that declares shader items, avoiding shader/GPU sidecar work for native stdlib-only actor/intent programs.
- Updated `crates/kain-sys-codegen/src/codegen_c.rs` so `@extern` functions become declarations only, `spawn`/`send` lower to the native actor facade, `main` emits a valid C `int`, unsigned integer casts map to C integer types, and direct C entangle metadata registers with the native runtime through a generated `__kain_register_entanglements()` thunk.
- Added `runtime/fixtures/native_world_actor_intent/main.kn` as the all-in-one native proof for `world`, `entangle`, `actor`, `patch`, `law`, `converge`, `orchestrate`, and the native stdlib facade.
- Added `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` to exercise the facade directly from C.

Design decisions:

- The native stdlib is target-scoped on purpose. Do not let LLVM/C native builds fall back to the root stdlib unless the target profile is absent; the generic root includes richer constructs that direct C does not yet own.
- `runtime/native_core_runtime.toml` is the safe default for ordinary language/native proof builds. Use the full `runtime/native_runtime.toml` when the task needs the broader app/UI/vendor runtime surface.
- Direct C now links against the same native runtime facade as LLVM for first-class actor and entangle behavior. It remains an experimental subset, but unsupported forms should fail explicitly rather than silently erasing core language declarations.
- The current actor facade spawn path uses a generic blocking actor bootstrap for named-payload mailbox traffic. It proves runtime wiring and send/spawn ABI, not compiler-generated per-actor handler specialization for direct C yet.

Validation:

- `cargo test -p kain-core stdlib --target-dir target\\codex-native-stdlib-core`
- `cargo test -p kain-entangle --target-dir target\\codex-native-entangle`
- `cargo test -p kain-sys-codegen c_backend --target-dir target\\codex-native-stdlib -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-native-stdlib-cli`
- `cargo test -p cli --lib "stage_llvm_native_artifacts_" --target-dir target\\codex-native-stdlib-cli-test -- --nocapture`
- `cargo test -p cli --bin kain native_runtime_manifest_candidates_prefer_core_runtime --target-dir target\\codex-native-stdlib-cli-bin-test -- --nocapture`
- `toolchain\\llvm\\bin\\clang.exe -c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-native-stdlib\\kain_runtime_native_stdlib.obj`
- `toolchain\\llvm\\bin\\clang.exe runtime\\conformance\\native_stdlib_bridge\\test_native_stdlib_bridge.c runtime\\native\\src\\core\\kain_runtime_core.c runtime\\native\\src\\core\\kain_runtime_version.c runtime\\native\\src\\core\\kain_runtime_diagnostics.c runtime\\native\\src\\core\\kain_runtime_actor.c runtime\\native\\src\\core\\kain_runtime_entangle.c runtime\\native\\src\\core\\kain_runtime_native_stdlib.c -Iruntime\\native\\include -o target\\codex-native-stdlib\\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32`
- `target\\codex-native-stdlib\\native_stdlib_bridge.exe`
- `target\\codex-native-stdlib-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t llvm -o target\\codex-native-stdlib\\native_world_actor_intent.ll`
- `target\\codex-native-stdlib\\native_world_actor_intent.exe`
- `target\\codex-native-stdlib-cli\\debug\\kain.exe build runtime\\fixtures\\native_world_actor_intent\\main.kn -t c -o target\\codex-native-stdlib\\native_world_actor_intent_c.c`
- `target\\codex-native-stdlib\\native_world_actor_intent_c.exe`

Current risks:

- The broad `runtime/native_runtime.toml` still carries the larger app/vendor surface. Prefer `runtime/native_core_runtime.toml` for core language proofing until the full app/vendor lane is refreshed end to end.
- Direct C actor lowering currently routes through the generic facade instead of emitting specialized actor handler loops. That is enough for spawn/send/link proofing and runtime smoke coverage, but generated direct-C actor semantics still need a deeper pass.
- The C backend still emits noisy but harmless comparison-parentheses warnings in some stdlib helper expressions.

Recommended next step:

- Promote `runtime/fixtures/native_world_actor_intent/main.kn` and `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c` into a single scripted smoke so future runtime/compiler changes prove LLVM, direct C, and the C facade together without hand-running each command.

# 2026-05-11 - Full blades workspace smoke landed under labs

Kain now has a repo-local smoke that exercises the blades system as a complete workspace instead of as isolated unit tests.

What changed:

- Added `labs/blades_workspace_smoke` with a root `KAIN.toml` workspace over `apps/*`, `blades/*`, and `crates/*`.
- Added an app blade (`signal-console`), a Kain utility blade (`signal-math`), a C ABI blade (`native-filter`), a Rust crate blade with Kain glue (`native-metrics`), a Cargo-only synthetic blade (`synthetic-reporter`), and a GPU metadata blade (`gpu-compute`).
- Added CPU Fabric execution through `blade = "..."` references for Python -> Kain -> C ABI -> Rust crate -> Node, plus a GPU Fabric manifest that validates blade-backed `gpu_compute`.
- Added `scripts/run_blades_smoke.py`, which builds the platform C shared library, checks blade list/graph/check/equip JSON, validates both Fabric manifests, runs the CPU Fabric pipeline, and emits GPU artifacts. It keeps lab-local `.kain` bridge caches by default and supports `--clean-cache` for cold-cache proofing.
- Updated `labs/README.md` and `ARCHITECTURE.md` so future agents can find the smoke and know the validation command.

Design decisions:

- The smoke is intentionally shaped like a real workspace, not a minimal fixture. It proves root workspace discovery, explicit blade metadata, synthetic Cargo discovery, graph edges, C/Rust FFI fallback through blades, and GPU compute metadata in one place.
- The default runner validates GPU by generating artifacts instead of dispatching Vulkan. Use `--include-vulkan` only on machines with a working Vulkan compute runtime.
- The C checksum returns a bounded signed `int64_t`; importing a raw `uint64_t` checksum into Kain `Int` can overflow at runtime.

Validation:

- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blades-smoke`
- `$env:KAIN_BIN='D:\\Kain-Lang\\target\\codex-blades-smoke\\debug\\kain.exe'; python labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`
- `python -m py_compile labs\\blades_workspace_smoke\\scripts\\run_blades_smoke.py`

Current risks:

- A stale `target\\debug\\kain.exe` may not have the `blades` subcommand. Set `KAIN_BIN` to a freshly built CLI when running the smoke from an isolated target dir.
- The generated C shared library, `.kain` bridge caches, and `outputs/` are ignored and disposable. `kain blades check` is expected to pass after the runner builds the C sidecar.
- The GPU Fabric manifest is validation-ready, but full Vulkan dispatch is opt-in because local machines may lack a working Vulkan compute runtime.

Recommended next step:

- Add this smoke to any future blades CI lane once the repo has a stable way to select a freshly built `kain` binary and a policy for local C/Rust FFI bridge cache reuse.

# 2026-05-11 - Kain check/test pipeline hardened into a Rust-inspired v1

The reusable source validation pipeline now has a sturdier first-class shape instead of being only a thin CLI addition.

What changed:

- `crates/kain-core/src/runtime.rs` now recursively executes `test` items nested inside typed modules, so module-scoped tests are not merely counted and silently skipped.
- `crates/kain-test` now reports `skipped` cases separately from `passed` and `failed`, parses `//@ ignore`, `//@ skip`, and `//@ known-bug` directives, and supports `run_ignored` so CLI `--ignored` can burn down known-bug inventory.
- `crates/kain-test` now reports the real execution lane for run/test modes (`run` for run-pass/run-fail, `test` for Kain test items) even when a target directive exists for check modes.
- `kain check -` now honors the documented stdin path and emits the same structured report shape as file/directory checks.
- `kain test` now exposes `--ignored`, prints skipped reasons, and keeps JSON reports explicit through `skipped` and `skip_reason`.
- Added `smoketest/kain-test` as a tiny directive suite covering check-pass, check-fail, run-fail, nested module tests, and ignored cases.
- Added `docs/cli/check-and-test.md` and refreshed the CLI, crate, feature, command-matrix, architecture docs around `kain-check` and `kain-test`.

Design decisions:

- Kain should borrow Rust compiletest's proven directive ideas, not its whole architecture. The source-of-truth crates are `kain-check` and `kain-test`; CLI remains a shell.
- Ignored/known-bug cases are success-neutral by default and only execute with `--ignored`, matching the workflow of keeping known gaps visible without breaking every local suite.
- Future snapshot, revision, target-conditional, and bless/update semantics should land inside `kain-test` before any ad hoc script gets to invent parallel suite semantics.

Validation:

- `rustfmt --edition 2021 crates\\kain-test\\src\\lib.rs crates\\kain-core\\src\\runtime.rs crates\\cli\\src\\main.rs`
- `cargo test -p kain-test -p kain-check --target-dir target\\codex-check-test`
- `cargo test -p kain-core run_tests --target-dir target\\codex-check-test`
- `cargo test -p kain-test --target-dir target\\codex-check-test`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-check-test`
- `target\\codex-check-test\\debug\\kain.exe check smoketest\\kain-test\\check_pass.kn`
- `"fn main() -> Int:`n    return 0`n" | target\\codex-check-test\\debug\\kain.exe check -`
- `target\\codex-check-test\\debug\\kain.exe test smoketest\\kain-test --json target\\codex-check-test\\kain-test-report.json`
- `target\\codex-check-test\\debug\\kain.exe test smoketest\\kain-test --ignored` was expected to fail because the ignored parser-bad fixture is intentionally executed under `--ignored`.

Current risks:

- There is still no snapshot comparison, revision matrix, target-conditional directive family, bless/update flow, or parallel scheduling. The crate boundary is ready for those, but v1 only proves directive modes and structured reports.
- Runtime test output is printed directly by `runtime::run_tests`; the harness does not capture stdout/stderr for snapshot-style assertions yet.
- Existing workspace warnings remain noisy during `cargo build -p cli`; they are pre-existing and not part of this pipeline pass.

Recommended next step:

- Add snapshot support to `kain-test`: normalized stdout/stderr/diagnostic artifacts, `--bless`, and sidecar `.stderr` / `.stdout` files, using Rust compiletest as the behavior reference while keeping the Kain-owned report schema.

# 2026-05-11 - Kain blades landed as the local crate-like workspace system

Kain now has a first-class `kain-blades` crate that makes the "blades" idea real across CLI, Fabric, module lookup, Rust crate FFI, and C ABI FFI.

What changed:

- Added `crates/kain-blades` as the typed discovery/resolution layer for local blade workspaces. It discovers default `blades/*`, `apps/*`, and `crates/*` roots, honors `[workspace] blades`, `blade_roots`, and `members` from `KAIN.toml`, parses `[blade]` metadata, and treats plain `Cargo.toml` packages as synthetic Rust blades.
- Added `kain blades list`, `kain blades graph`, `kain blades check`, and `kain equip <blade>` to the CLI, with text and JSON output.
- Committed the existing `kain-check` and `kain-test` crates as the reusable libraries behind the already-planned `kain check` and `kain test` CLI commands; their stale failure fixtures were updated to use syntax errors instead of type mismatch cases that the current frontend accepts.
- Wired blade module roots into `kain-core` filesystem module candidates so a blade can expose Kain modules without callers hardcoding folder paths.
- Wired blade fallback into `kain-crate-ffi` and `kain-c-ffi`, so Rust crate imports and C ABI library imports can resolve through the same blade graph.
- Extended Fabric schema/execution with `blade = "..."` support. Kain, Rust crate, C ABI, and GPU Fabric steps can now resolve entries/manifests/shaders/compute keys from a blade instead of repeating path fields.
- Fixed the pre-existing CLI exhaustiveness blocker by wiring `Commands::Check` and `Commands::Test` through the `kain-check` and `kain-test` crates, which allowed a full `cargo build -p cli` proof.

Design decisions:

- Blades are local-first and crate-like today, not a remote package manager yet. Future `sharpen` behavior should extend `kain-blades` rather than making a separate registry/update path.
- Rust crates and Kain blades are deliberately interchangeable at the folder-boundary level: a Cargo crate under `crates/*` can be equipped as a blade, and a `KAIN.toml` blade can point at Rust/C/Fabric/GPU artifacts.
- `kain-blades` is the one place that should know default blade patterns and manifest semantics. Callers should consume `ResolvedBlade`, module roots, Rust crate blade resolution, or C FFI library resolution instead of reimplementing scans.

Validation:

- `rustfmt --edition 2021 crates/kain-blades/src/lib.rs crates/cli/src/blades.rs crates/kain-core/src/module_resolution.rs crates/kain-crate-ffi/src/resolve.rs crates/kain-c-ffi/src/lib.rs crates/kain-omni/src/fabric.rs crates/kain-host/src/fabric.rs`
- `cargo test -p kain-blades --target-dir target\\codex-blades`
- `cargo test -p kain-core blade_module_roots_extend_filesystem_candidates --target-dir target\\codex-blades`
- `cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\\codex-blades`
- `cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\\codex-blades`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\\codex-blades`
- `target\\codex-blades\\debug\\kain.exe equip kain-core --json`
- `target\\codex-blades\\debug\\kain.exe blades list .`
- `cargo test -p kain-crate-ffi --target-dir target\\codex-blades`
- `cargo test -p kain-c-ffi --target-dir target\\codex-blades`
- `cargo test -p kain-check -p kain-test --target-dir target\\codex-blades`

Current risks:

- `cargo test -p kain-omni --target-dir target\\codex-blades` still has one non-blades failure in `tests::build_emits_rust_from_import_aware_entry` with `Unknown identifier 'helper'`; the focused Fabric blade-adjacent validation passed.
- `blades check` can report missing generated/shared-library artifacts for blades whose native sidecars have not been built yet.
- There is no remote registry, lockfile, install, or `sharpen` implementation yet. The current crate is the local graph and resolver foundation.

Recommended next step:

- Add a smoke blade under `blades/` with Kain, Rust crate, C ABI, Fabric, and GPU sections, then run `kain equip`, `kain fabric run`, and both FFI import paths against that one intentional fixture.

# 2026-05-11 - LLVM and direct C native intent backends refreshed

Kain's native backend path now handles the compiler-owned intent suite more honestly across LLVM, direct C output, and the native C runtime.

What changed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` now registers and emits `law` declarations as real LLVM callables, records parameter types for `patch`/`law`/`converge`/`orchestrate`, preserves orchestrate stage runtime comments, and emits an entangle registration function that calls the C runtime from `main`.
- `runtime/native/include/kain_runtime_entangle.h` and `runtime/native/src/core/kain_runtime_entangle.c` add a small fixed-capacity native entangle registry. `runtime/native_runtime.toml` now includes that source in the manifest-driven C runtime bundle.
- `crates/kain-sys-codegen/src/codegen_c.rs` now lowers worlds to C structs/static world instances, emits `patch`, `law`, `converge`, and `orchestrate` as callable functions, preserves entangles as a static metadata table, supports stage calls, and maps world parameters/fields through pointer-style C access.
- `crates/kain-core/src/stdlib.rs` now routes `CompileTarget::C` through `stdlib/c` before root fallback, keeping the experimental C backend away from the full generic stdlib unless the C profile is absent.
- The Rust backend bootstrap intrinsic tests were corrected after missing `CallArg.span` fields were restored; those intrinsics now assert the rendered intrinsic behavior instead of stale raw-call fallback text.

Design decisions:

- LLVM entangle support uses the typed entangle items already produced by `kain-core` and emits a narrow native registration ABI. Rich write barriers, cross-process propagation, and distributed conflict policy remain future runtime adapters.
- Direct C output keeps entangle metadata local instead of forcing every generated C file to link a runtime registration symbol. Runtime-linked registration is currently the LLVM lane's responsibility.
- The C backend remains an explicit subset, but it should now fail on truly unsupported expression/type forms rather than silently ignoring first-class intent declarations.

Validation:

- `cargo test -p kain-sys-codegen --target-dir target\\codex-llvm-refresh`
- `cargo test -p kain-core test_load_stdlib_for_target_uses_target_profile_order --target-dir target\\codex-llvm-refresh -- --nocapture`
- `cargo test -p kain-entangle --target-dir target\\codex-llvm-refresh`
- `cargo test -p cli --lib stage_llvm_native_artifacts_materializes_entangle_metadata --target-dir target\\codex-llvm-refresh -- --nocapture`
- `toolchain\\llvm\\bin\\clang.exe -c runtime\\native\\src\\core\\kain_runtime_entangle.c -Iruntime\\native\\include -o target\\codex-llvm-refresh\\kain_runtime_entangle.obj`

Current risks:

- Full `cargo test -p cli ...` still fails in this checkout because the pre-existing dirty CLI command enum has `Commands::Check` and `Commands::Test` variants that are not handled in `main.rs`. Use `cargo test -p cli --lib ...` for the native staging test until that unrelated CLI work is reconciled.
- The C backend does not yet implement every expression form, generic type, container ABI, or runtime registration path. It now covers the compiler-owned intent declarations, but deep C parity still needs focused backend work.
- Entangle alias canonicalization remains an interpreter/runtime risk from the earlier entangle pass: alias writes such as `let p = Physics; p.player_health -= 10` still need canonical path recovery.

Recommended next step:

- Add a native-link smoke that compiles a generated LLVM file with `kain_runtime_entangle.c` included from `native_runtime.toml`, then asserts the registry contains the emitted binding after `main` runs.

# 2026-05-11 - First-class entangle state coupling landed

Kain now has a v1 first-class `entangle` declaration for compiler-owned Topological State Coupling between stable state endpoints.

What changed:

- Added `crates/kain-entangle` as the shared semantic/runtime metadata crate. It owns `state.entangle`, `EntangleGraph`, endpoint ids, single-writer binding descriptors, duplicate endpoint checks, self-entanglement rejection, mirror lookup, and mirror-write denial.
- Added parser, AST, typechecker, formatter, interpreter, runtime contract, realtime app bundle, LSP, and UE5-codegen awareness for:
  - `entangle Physics.player_health <-> UI.health_display with single_writer`
- `crates/kain-core` now lowers entanglements into typed metadata, `RuntimeContractBundle.entanglements`, `RealtimeAppBundle.entanglements`, the reflection payload, and required capability/service-binding metadata.
- The interpreter registers entanglements during program setup, treats the left endpoint as the authority, propagates authority writes into the right mirror endpoint, and rejects direct mirror writes under the v1 `single_writer` policy.
- Docs now list entangle as the sixth compiler-owned intent family and describe the v1 syntax, capability, contract shape, interpreter semantics, and current limits.

Design decisions:

- `entangle` is a contextual top-level item keyword rather than a hard lexer keyword, matching other compiler-owned intent forms.
- V1 supports only stable dotted storage endpoints with at least two path segments. The typechecker resolves world state and struct-field paths through the existing value/type environment.
- V1 requires strict matching resolved storage types after shared-reference peeling. It intentionally does not use the looser assignment-compatibility rule.
- The left endpoint is authoritative and the right endpoint is the mirror. The policy is explicit as `with single_writer` so future policies can be added without reshaping the syntax.
- Backend/codegen crates currently treat entanglements as metadata-only unless they consume the runtime contract or realtime bundle.

Validation:

- `cargo test -p kain-entangle --target-dir target\codex-entangle`
- `cargo test -p kain-core entangle --target-dir target\codex-entangle -- --nocapture`
- `cargo test -p kain-core --test compiler_owned_intent_test --target-dir target\codex-entangle -- --nocapture`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-entangle`
- `git diff --check`

Current risks:

- Interpreter propagation keys off the authored assignment path. `Physics.player_health -= 10` propagates, but alias-based writes such as `let p = Physics; p.player_health -= 10` do not yet canonicalize back to the entangled endpoint.
- Native ABI, LLVM, C/C++/TS/WASM, and distributed side-channel lowering are not implemented yet. Those targets should consume the emitted `entanglements[]` metadata and `state.entangle` requirement.
- Only `single_writer` exists today. Multi-writer conflict policy, timestamp/vector-clock resolution, atomics, shared-memory rings, and cross-process transport are future work.

Recommended next step:

- Add backend lowering that consumes `RuntimeContractBundle.entanglements` and emits target-specific write barriers or adapter hooks, starting with the realtime/native UI path where the `state.entangle` service binding already makes the requirement visible.

# 2026-05-10 - Windows git index writes now have a repo-local safe-write escape hatch

This checkout can still hit a Windows-only git failure where index-mutating commands finish their work and then die on the final `.git/index.lock -> .git/index` swap with `fatal: unable to write new index file`.

What changed:

- Added `scripts/windows/git-safe-index.ps1`.
- The script copies the live `.git/index` to `.git/index.safe-write`, runs the requested git command with `GIT_INDEX_FILE` pointed at that temporary index, then stream-copies the resulting bytes back into the live `.git/index` in place instead of relying on the failing rename step.
- The script refuses to run if a real `.git/index.lock` exists so it does not silently stomp an active git writer.
- `ARCHITECTURE.md` now points future agents at the helper from `## Common Errors`.

Usage:

- `./scripts/windows/git-safe-index.ps1 add -A`
- `./scripts/windows/git-safe-index.ps1 rm -r --cached generated`
- If no arguments are passed, it defaults to `add -A`.

Current risks:

- This is a safe operator workaround for an external Windows file-handle/index-swap issue, not a root-cause fix inside the repo source itself.
- The helper only addresses index writes. If a separate environment issue later blocks branch ref updates during `commit` or remote-tracking ref updates during `push`, that still needs the same kind of manual repair or a future wrapper for refs.

Recommended next step:

- If this keeps recurring outside Codex too, capture the actual handle owner with Process Explorer or Sysinternals `handle.exe` and decide whether a broader `git-safe-commit.ps1` / `git-safe-push.ps1` wrapper is worth adding.

# 2026-05-10 - Full-power codebase bridge and Fabric Node handoff landed

Kain now treats local workspace control as an explicit trusted execution lane instead of a read-only inspection helper.

What changed:

- Added `crates/kain-codebase` as the trusted-local workspace authority layer. It discovers roots from `KAIN.toml`, `package.json`, `Cargo.toml`, `.git`, and explicit paths; scans, hashes, creates, writes, copies, moves, and deletes files/directories; round-trips JSON/TOML; and captures commands with structured stdout/stderr/status.
- Exposed `kain codebase inspect <path> --json` and `kain codebase run <cwd> -- <command> ...`. `codebase run` exits successfully when Kain captured the child process correctly, even if the child command itself returned nonzero; inspect the JSON `success` and `status` fields for the child result.
- Registered the new codebase APIs in host-backed Kain execution: `codebase_*`, `cargo_*`, `python_*`, `c_*`, and `ts_*` bridge functions now typecheck and dispatch in interpreted/test host lanes.
- Fixed the Node/Fabric raw bridge regression by keeping `@extern` declarations from shadowing native bridge functions, preserving raw Node module handles through raw import/call paths, adding CJS `js_require_raw`/`node_require`, and adding `node_package_run` for package-script execution from the Node bridge cwd.
- Fabric Kain steps now install Node runtime cwd/cache config from the Fabric workspace root, and Node Fabric steps receive upstream `fabric_inputs` with shared payload projections instead of an empty JS object.
- The GreebleFS image-converter Fabric pipeline now runs end to end with Python -> Kain -> C ABI -> Rust crate -> Node. The latest proof session reported Node `inputProjection = received`, upstream keys `kain_orchestrator,rust_analyzer`, a 64x64 shared-image chain, and a native shared-buffer snapshot.

Validation:

- `cargo test -p kain-codebase --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo test -p kain-node --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo test -p kain-host node_step_consumes_shared_inputs_via_fabric_inputs --target-dir target\codex-codebase-bridge -- --nocapture`
- `cargo build -p cli --target-dir target\codex-codebase-bridge`
- `target\codex-codebase-bridge\debug\kain.exe codebase inspect D:\GreebleFS --json`
- `target\codex-codebase-bridge\debug\kain.exe codebase run D:\GreebleFS -- bun --version`
- `target\codex-codebase-bridge\debug\kain.exe codebase run D:\GreebleFS -- cargo check --manifest-path src-tauri/Cargo.toml --lib`
- `D:\Kain-Lang\target\codex-codebase-bridge\debug\kain.exe fabric run -m KAIN.fabric.toml` from `D:\GreebleFS\usr\plugins-kain\kain-image-converter\plugin.runtime\fabric`

Current risks:

- Direct C calls in `kain-codebase` intentionally support a narrow scalar ABI surface today (`i64`/`f64`/`void` signatures). Rich C ABI reflection should build on `kain-c-ffi` instead of bloating this first workspace-control crate.
- GreebleFS `pnpm --version` is captured correctly but returns child status `1` because that repo is configured for Bun. Use Bun for GreebleFS package-manager smokes unless the package-manager policy changes.
- GreebleFS `cargo check --manifest-path src-tauri/Cargo.toml --lib` is captured correctly and passed in this session, but the build can still print Windows incremental-cache cleanup warnings when another process has target files locked.

Recommended next step:

- Add a higher-level Kain-authored smoke under `smoketest/fabric/` or GreebleFS `usr/plugins-kain` that calls the new `codebase_*` APIs from `.kn` directly, then runs package/Cargo/Python/C/TS operators from one trusted-local script.

# 2026-05-09 - TypeScript import ambient prelude is generated from TypeScript lib data

The TypeScript import pipeline now uses an embedded ambient manifest instead of hand-maintained Rust lists of JavaScript/DOM globals.

What changed:

- Added `tools/typescript_import/extract_ambient_manifest.py` and `tools/typescript_import/typescript_ambient_overrides.json`.
- The extractor reads `reference/TypeScript-main/src/lib/*.d.ts`, merges Kain-specific aliases/helpers from the JSON override file, and writes `crates/kain-import/src/typescript/data/typescript_ambient_manifest.json`.
- `crates/kain-import/src/typescript/ambient.rs` embeds that manifest and exposes lookup helpers for ambient value names and TypeScript utility-type fallbacks.
- `kain import-ts` now writes the global TS prelude from the manifest, not from hardcoded DOM/JS arrays in `crates/cli/src/import_typescript.rs`.
- Global runtime constructor aliases such as `Array -> ts_Array` and ecosystem helpers such as Node/test-runner globals live in data, so future additions should update the override JSON and regenerate the manifest.
- Generated `.kn` validation for the TypeScript importer now uses the TS backend instead of the interpreter target; interpreter validation is not representative for TS imports with external stubs.

Validation:

- `python tools\typescript_import\extract_ambient_manifest.py` generated a manifest with 1051 ambient value symbols and 2206 ambient type symbols.
- `cargo test -p kain-import ambient --target-dir target\codex-ts-import-manifest` passes.
- `cargo build -p cli --target-dir target\codex-ts-import-manifest` passes with pre-existing workspace warnings.
- A focused ambient smoke using `HTMLElement`, `URL`, `ImportMeta`, `Uint8Array`, `Blob`, `Proxy`, `Promise`, `console`, `window`, and `import.meta` imports, parses, validates, and compiles to TS.
- `target\codex-ts-import-manifest\debug\kain.exe import-ts D:\GreebleFS\src --flat --exclude vendor --output target\codex-ts-import-manifest\greeblefs_src_firstparty.kn --target ts` imported 650/650 first-party files after excluding 392 vendor files; generated `.kn` parse validation, generated `.kn` TS compile validation, and requested TS output compile all passed.
- After the destructured-param and high-arity `forEach` lowering fixes, PATH `kain import-ts D:\GreebleFS\src --flat --output D:\GreebleFS\src-kain\reflection\imports\greeblefs\greeblefs_src.kn --target ts --report-json D:\GreebleFS\src-kain\reflection\imports\greeblefs\greeblefs_src.import_report.json` emits both `greeblefs_src.kn` and `greeblefs_src.ts`; generated Kain validation and requested TS target compilation both pass.

Current risks:

- Import diagnostics remain high on large React projects because external module imports, JSX fallbacks, object spreads, and destructured props still lower through lossy stubs. Those are now reported as degradation diagnostics, not validation failures.
- Full `D:\GreebleFS\src` import still reports one source parse failure in `D:\GreebleFS\src\vendor\tiptap\extension-drag-handle\__tests__\edgeDetection.spec.ts` from SWC (`Expected(,, "[")`). The batch continues, writes the reflection artifacts, and compiles the generated Kain/TS outputs, but true 1042/1042 coverage needs a follow-up parser fallback or targeted handling for that test file's syntax.
- The embedded prelude is intentionally broad. A future optimization can make prelude emission usage-pruned while keeping this manifest as the source of truth.

Recommended next step:

- Add project-aware ambient discovery for `node_modules/@types` or configured `tsconfig` type roots so Node/Vitest/React ecosystem globals can be generated from package declarations instead of only from the stable override JSON.

# 2026-05-08 - Rust import printer now preserves expression-heavy Tauri command bodies

The Rust import pipeline no longer turns most expression bodies into `LOSSY LOWERING [class:unsupported_expr_lowering]` comments when generating `.kn` from already-lowered Rust AST.

What changed:

- Expanded `crates/cli/src/import_rust.rs` source emission for Kain AST expressions and statements instead of only printing literals/idents.
- The CLI printer now handles calls, method chains, fields, indexing, assignments, binary/unary ops, refs/derefs, casts, `await`, `?`, lambdas, arrays, tuples, structs, enum variants, `if`, `match`, loops, and unit `()`.
- Added a regression test for the GreebleFS-shaped Tauri preview helpers (`PathBuf::from`, `preview_streaming.policy().clone()`, `run_native_blocking_task(...).await?`, `BinaryResponse::new`, and `dirs::home_dir().map(...).ok_or_else(...)`).

Validation:

- `cargo check -p cli --target-dir target\codex-rust-import-check` passes with pre-existing warnings.
- `cargo test -p cli --target-dir target\codex-rust-import-check import_rust::tests::rust_import_printer_preserves_tauri_preview_expression_bodies -- --nocapture` passes.
- Re-importing `D:\GreebleFS\src-tauri\src\fs_commands.rs` into `generated\rust_import_validation\fs_commands.kn` produced 199 functions, 37 structs, 12 enums, zero `LOSSY LOWERING`, zero `unsupported_expr_lowering`, and an empty diagnostics class report.

Current risks:

- This repair is a printer expansion, not a full guarantee that every printed construct is accepted by every Kain backend. The importer can now preserve much more source shape, but backend/codegen support remains target-sensitive.
- The output may still contain Rust-shaped names normalized into Kain identifiers (for example `PathBuf__from`, `NativeTaskRequest__new_`), which is expected for this importer lane.

Recommended next step:

- Add a small CLI fixture under `crates/cli/tests/fixtures/import_rust` or a broader all-in-one smoke that imports a real Tauri command slice and asserts the generated report stays free of `unsupported_expr_lowering`.

# 2026-05-07 - Filesystem imports now dogfood sibling Kain modules

Kain now handles the import shape that blocked the first GreebleFS Kain control-plane split: `use module::item` can resolve against `module.kn` / `src/module.kn` when `module/item.kn` does not exist, and `use module::*` can expose top-level sibling module items during typechecking.

What changed:

- Added `crates/kain-core/src/module_resolution.rs` as the shared lookup helper for stdlib roots and authored filesystem module candidates.
- Updated the interpreter runtime import path so named filesystem imports can select one top-level item from a fallback module file and honor `as` aliases.
- Updated the typechecker to best-effort register symbols from cleanly parsed filesystem modules, while preserving the older `Unknown` fallback when imported modules are absent or not safe to register during typechecking.
- Added focused `kain-core` runtime tests for the GreebleFS-shaped imports: `use host_reflection::build_control_plane_catalog` and `use plugin_authoring::*`.
- Updated `docs/syntax-and-semantics/module-resolution.md` and the local `kain-engineer` import reference so future agents do not rediscover the old workaround.

Validation:

- `cargo test -p kain-core filesystem_ -- --nocapture` passes.
- `cargo build -p cli --target-dir target\codex-cli-build` passes; the alternate target dir avoids the local `target/debug` PyO3 artifact lock.
- `git diff --check -- crates\kain-core\src\module_resolution.rs crates\kain-core\src\lib.rs crates\kain-core\src\runtime.rs crates\kain-core\src\types.rs crates\kain-core\src\runtime_tests.rs` passes with line-ending warnings only.

Current risk:

- Filesystem module lookup is still rooted in the process current directory, not the source file's absolute parent. For nested scripts such as `src/server.kn`, launch from the project/runtime root or a directory where the expected `src/<module>.kn` exists until source-file-relative roots are added.
- Plain `cargo build -p cli` in the default `target/debug` directory is blocked on this machine by a locked PyO3 artifact (`target/debug/deps/libpyo3_build_config-9afde652236a6978.rlib`). Use a separate `--target-dir` for validation until that Windows file handle clears, then refresh `target/debug/kain.exe`.

Recommended next step:

- After the CLI binary rebuilds, simplify the GreebleFS control-plane `server.kn` back into real sibling imports instead of keeping it self-contained, then add a Kain CLI smoke that runs that split module layout.

# 2026-04-18 - Tauri desktop adapter landed as a first-class native-ui host lane

The repo now has a real Tauri 2 desktop host path for Kain-authored UI instead of forcing every native-ui flow through the Qt launcher.

What changed:

- `crates/kain-ui` and `crates/kain-core` now recognize `UiHostBackendKind::Tauri`, including authored `host_backend="tauri"` and `host_backend="webview"` aliases.
- `crates/kain-ui-tauri` now owns the generated Tauri host lane: plugin/capability/permission presets, bridge-manifest construction, merged reflection metadata, hybrid frontend bridge JS, and generated `src-tauri/*` project files.
- `crates/kain-driver` now has a dedicated Tauri bundle/materialization path that combines native runtime-contract truth with hybrid frontend artifacts and emits a generated Tauri app root with `frontend/`, `generated/`, `config/`, `state/`, and `src-tauri/`.
- `crates/cli/src/native_ui_build.rs` now exposes `NativeUiHostKind::{Qt,Tauri}` plus typed Tauri config, and `crates/cli/src/native_ui_dev.rs` now abstracts launch targets so the same dev loop can launch either a packaged Qt executable or `cargo run --manifest-path src-tauri/Cargo.toml`.
- Hot-reload metadata for generated Tauri apps now preserves the resolved custom bundle identifier instead of silently falling back to a derived default, and new tests pin both the Tauri alias parsing path and the generated bundle-id propagation.

Validation:

- `cargo test -p kain-ui tauri_aliases`
- `cargo test -p kain-core tauri_aliases`
- `cargo test -p kain-ui-tauri`
- `cargo test -p kain-driver --features tauri tauri_bundle_materialization_writes_bridge_and_frontend_assets`
- `cargo test -p cli --features tauri native_ui_build::tests::native_ui_build_materializes_tauri_project_without_binary -- --exact`
- `cargo test -p cli --features tauri native_ui_dev::tests::reload_decision_hot_reloads_runtime_sidecar_changes -- --exact`

Important behavior notes:

- Tauri remains a host/package lane under `build native-ui` and `native-ui dev`; there is still no `CompileTarget::Tauri`.
- The generated Tauri app consumes existing compiler-owned truth: native runtime bundle/contract/realtime metadata plus hybrid JS/TS/WASM output. Keep those bundle families authoritative instead of inventing Tauri-local semantics.
- In this checkout `cargo fmt --all` is still blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`, so file-scoped `rustfmt` is the safe formatting fallback when only the Tauri lane is being touched.

Current risk:

- The generated Rust host bridge is intentionally broad but still generic. Future work should harden real typed command handlers and add richer plugin-specific round-trip tests once there are Kain-authored apps depending on those namespaces.
- Full workspace validation for `kain-driver --features tauri` still includes unrelated pre-existing driver test failures outside the Tauri lane, so use the Tauri-focused test filters above when validating this subsystem.

Recommended next step:

- Add a smoketest app under `smoketest/UI/` that is materialized and launched through `--host tauri`, then validate one real plugin namespace such as dialog/fs/store end to end against the generated bridge.

- New Kain 3D pass (2026-04-17): `SceneCatalog::picker_entries()` now orders canonical scenes semantically, keeping the default scene first, then ranking remaining canonicals by scene role and scene scale before appending aliases. This makes native scene browsers and inspectors surface showcase/environment scenes more intentionally instead of only following raw name order.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d picker_entries_prioritize_default_then_semantic_canonicals_then_aliases -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalogEntry::picker_label()` now includes the authored `viewport_summary` alongside the resolved scene name and composition labels, so native scene browsers can show the scene's launch/context cue instead of hiding it in the struct.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_entries_surface_picker_ready_metadata -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalogEntry` now carries `scene_focus` alongside role/scale/profile/density/stage, so native scene browsers get the dominant composition cue without re-deriving it from `SceneCompositionSummary`.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_entries_surface_picker_ready_metadata -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `material_atrium_smoke` now embeds `SceneCatalog::summary()` data in the structured smoke JSON, including default scene, canonical scene count, alias count, total scene names, and picker entry count. The header copy also now calls out catalog coverage so the smoke reports scene-browser context without re-deriving it in downstream tooling.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\bin\material_atrium_smoke.rs` completed cleanly, but `cargo test -p kain-3d catalog_summary_reports_canonical_and_alias_counts -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalog::picker_entries()` now emits a picker-ordered scene list with the default scene first, followed by canonical scenes and then aliases. This gives native scene browsers and inspectors a direct, data-driven ordering instead of making each host re-sort the catalog itself.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d picker_entries_prioritize_default_scene_before_aliases -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCompositionSummary` now exposes a structured `scene_focus` cue (`geometry-led`, `instance-led`, `material-led`, `lighting-led`, `environment-led`, `anomaly-led`) and `FrameDiagnostics` carries it through the CPU/WGPU frame path. `material_atrium_smoke` now preserves the cue in its JSON payload, so scene tooling can tell what dominates a composition instead of only reading size and density.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs crates\kain-3D\src\renderer.rs crates\kain-3D\src\bin\material_atrium_smoke.rs` completed cleanly, but `cargo test -p kain-3d scene::tests::scene_focus_label_tracks_scene_dominant_authoring_signal -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): `SceneCatalog` now exposes a structured `summary()` with canonical scene count, alias count, and default scene name. This gives native tooling a cheap, stable way to present catalog coverage without re-deriving totals from map sizes in multiple places.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs` completed cleanly, but `cargo test -p kain-3d catalog_summary_reports_canonical_and_alias_counts -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-17): extracted the scene-composition-to-frame-diagnostics mapping into `SceneCompositionSummary::populate_frame_diagnostics(...)` and switched both CPU and WGPU renderers to call it. This removes duplicated diagnostics wiring, keeps `FrameDiagnostics` fields aligned across backends, and gives future 3D tooling a single place to extend when new summary fields should surface in native frame logs.
- Validation note: `rustfmt --edition 2021 crates\kain-3D\src\scene.rs crates\kain-3D\src\renderer.rs crates\kain-3D\src\wgpu_renderer.rs` completed cleanly, but `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` is still blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries `scene_density` alongside the existing role/scale/profile/camera-fit diagnostics, and both the CPU and WGPU renderers populate it from `SceneCompositionSummary::density_label()`. This keeps the dense/sparse/balanced cue available to native inspectors without forcing them to re-derive it from the brief label.
- Validation note: `cargo test -p kain-3d renderer::tests::default_camera_auto_frames_off_center_scene -- --nocapture` was still blocked by the repo-local Windows GNU toolchain, not by the 3D change. `x86_64-w64-mingw32-gcc` failed while linking build scripts because `lld` could not find `-lgcc_eh` and `-lgcc`.
- New selfhost bootstrap pass (2026-04-16): collapsed `src/core/parser.kn` to a bootstrap-safe `parse_source(...)` stub and rewrote `src/core/lexer.kn` to a field-access-free bootstrap surface. This removed the owned `--emit-llvm-only` blocker `Unknown field 'kind'`, which was coming from the bootstrap token seam rather than the LLVM backend itself.
- Validation note: the exact command `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; $env:PYO3_PYTHON='C:\Users\ephemara\AppData\Local\Programs\Python\Python312\python.exe'; cargo run -q -p cli --bin kain -- selfhost bootstrap --manifest-path src/KAIN.toml --emit-llvm-only` now fails later with `let binding expected Result<Value, KainError>, found Result<Value, Unknown>`, narrowed to the bootstrap `Result::Ok(...)` coercion path in `src/core/runtime.kn`.
- Operator note: when this automation reads the bootstrap report in parallel with the command, `bootstrap_report.md/json` can lag one run behind the live stderr/stdout failure. Use the direct command output as the source of truth for the freshest blocker.

- New backend pass (2026-04-16): Kain now has a first-class experimental `c` compile target wired through `kain-core`, `kain-driver`, `kain-sys-codegen`, CLI native artifact staging, and `kain selfhost bootstrap --backend c`. The C lane reuses the raw-native runtime contract/bundle path and native link flow instead of pretending C is just another alias for LLVM.
- The new C backend is intentionally an honest subset today. It covers the target plumbing plus an initial emitter for structs, unit enums, functions, basic statements, casts, pointer/ref syntax, struct literals, and `print`/`println` helpers, while failing explicitly on unsupported semantic surface such as generic/function types from the full stdlib and many richer expression forms.
- Validation note: `cargo check -p kain-core -p kain-c-ffi -p kain-sys-codegen -p kain-driver -p cli` is green here only with `PYO3_USE_ABI3_FORWARD_COMPATIBILITY=1` because the local Python is 3.14 while repo PyO3 is pinned below that. A direct `target/debug/kain.exe -c ... -t c` smoke now reaches the C backend and reports backend-specific unsupported-type errors instead of rejecting the target, so the current blocker is C semantic coverage rather than CLI wiring.
- New Kain 3D pass (2026-04-16): renderer frame diagnostics now expose an explicit `camera_fit_ratio` string alongside the existing framing hint, and the `material_atrium_smoke` JSON payload preserves it. This gives scene tooling a sharper read on how tightly a scene is framed without recomputing the fit math downstream, and it keeps CPU/WGPU 3D diagnostics aligned on the same framing signal.
- Validation note: `cargo test -p kain-3d renderer::tests::render_scene_autoframes_off_center_geometry_and_tracks_diagnostics -- --nocapture` was blocked by the repo-local Windows GNU toolchain, not by the 3D code. `x86_64-w64-mingw32-gcc` could not resolve `-lgcc_eh` and `-lgcc` while linking build scripts. `rustfmt --edition 2021 crates\\kain-3D\\src\\renderer.rs crates\\kain-3D\\src\\wgpu_renderer.rs crates\\kain-3D\\src\\bin\\material_atrium_smoke.rs` completed cleanly.
- New selfhost bootstrap pass (2026-04-16): the owned `--emit-llvm-only` lane now gets past the previous parser-hostile support modules in `src/core/span.kn`, `src/core/error.kn`, `src/core/diagnostic.kn`, and `src/core/effects.kn` by collapsing those files to declaration-only bootstrap-safe surfaces. The latest validated command is `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo run -q -p cli --bin kain -- selfhost bootstrap --manifest-path src/KAIN.toml --emit-llvm-only`, and it now fails later with `Unknown identifier 'tokenize_source'` at `<input>:922:16`, which maps to the lexer/kainc bootstrap seam rather than the old impl/match parser failures.

- New Kain 3D direction update (2026-04-16): the next wave should pivot away from smoke/report polish and into core 3D power features. Treat SPIR-V compilation strength as a major asset, then build outward into renderer architecture, scene/runtime systems, GPU compute, and other high-leverage capabilities that move Kain toward UE5-class power instead of demo-only output.
- New Kain 3D pass (2026-04-16): `SceneCatalog` now exposes picker-ready catalog entries with canonical/alias resolution plus scene role, scale, profile, density, and composition-stage metadata. That gives native tooling a single structured list for scene browsers and inspectors instead of forcing each host to re-derive labels from names.
- New Kain 3D pass (2026-04-16): `SceneCatalog` now exposes canonical scene names and alias-inclusive scene names directly, which lets future tooling build real scene pickers and inspectors without hardcoding the catalog. This is a small but high-leverage step toward more discoverable 3D composition and runtime tooling.
- New Kain 3D pass (2026-04-16): the CPU and WGPU renderers now both reuse `SceneCompositionSummary::framing_hint_label()` for `FrameDiagnostics.framing_hint`, removing duplicate fit-ratio logic so the two presentation paths stay aligned when composition heuristics evolve. This keeps renderer diagnostics consistent across backends with a very small code change.
- Validation attempt: `cargo test -p kain-3d scene::tests::scene_role_label_tracks_scene_complexity_signals -- --nocapture` still failed in this checkout because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc` during build-script linking.

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries a `framing_hint` string (`tight-fit` / `balanced-fit` / `loose-fit`) derived from the scene bounds radius and the framed camera distance, and `material_atrium_smoke` persists that hint in the runtime-matrix JSON. This gives native tooling a quick-read signal for whether a frame is tightly composed or has deliberate breathing room, without recomputing camera fit heuristics downstream.
- Validation attempt: `cargo test -p kain-3d default_camera_auto_frames_off_center_scene -- --nocapture` still fails here before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.

- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now exposes a structured `diagnostics()` helper, and `material_atrium_smoke` uses it when writing the runtime-matrix JSON. That makes the smoke report and any future scene inspectors consume one canonical scene-composition shape instead of hand-rebuilding the same labels and counts in multiple places.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_uses_view_aspect_ratio_for_fit_distance -- --nocapture` still fails here before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries structured scene-composition cues (`scene_role`, `scene_scale`, and `scene_profile`) alongside the existing flat summary string, so renderer output can be queried without parsing one concatenated label. This is a tooling-focused uplift for native inspectors and scene browsers.
- Validation attempt: `cargo test -p kain-3d --lib` could not finish here because the repo-local Windows GNU toolchain still fails during build-script linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-16): `SceneBounds` now exposes a coarse composition profile (`linear` / `planar` / `stacked` / `volumetric`), and `SceneCompositionSummary::brief_label()` surfaces that profile alongside the existing scale, aspect, and density cues. This makes scene diagnostics better at telling native tooling whether a scene is a corridor, a flat stage, or a fuller volumetric composition at a glance.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now also emits a coarse scene-role cue (`study` / `lookdev` / `showcase` / `environment` / `anomaly`), giving native tooling a one-word read on whether a composition is a small study, a presentation set, an FX-heavy environment, or a black-hole-style special case. The role cue is folded into the brief label so smoke logs and inspectors get the classification for free.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_profile_label_distinguishes_flat_and_volumetric_scenes -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- Validation attempt for the new role cue: `cargo test -p kain-3d scene::tests::scene_role_label_tracks_scene_complexity_signals -- --nocapture` hit the same repo-local Windows GNU linker gap while building build-script dependencies, not a scene-logic failure.

- New Kain 3D pass (2026-04-16): software rendering now distinguishes visible vs. fully culled instances in `FrameDiagnostics`, so tooling can see when an authored object was completely clipped/backfaced instead of only inferring success from the final image. Added a regression test that pushes a triangle behind the camera and expects it to land in `culled_instances`.
- Validation attempt: `cargo test -p kain-3d renderer::tests -- --nocapture` still hits the repo-local Windows GNU linker gap before the test binary can link, because `x86_64-w64-mingw32-gcc` cannot resolve `-lgcc_eh` and `-lgcc`.

- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now includes an explicit scene-scale cue (`miniature` / `room-scale` / `studio-scale` / `world-scale`), and the `material_atrium_smoke` JSON payload now carries that scale as structured metadata. This gives 3D tooling one more quick-read signal for composition quality without re-deriving bounds heuristics downstream.
- Validation attempt: `cargo test -p kain-3d scene::tests::scene_scale_label_tracks_bounds_radius -- --nocapture` and `rustfmt --edition 2021 --check crates\\kain-3D\\src\\scene.rs crates\\kain-3D\\src\\lib.rs crates\\kain-3D\\src\\bin\\material_atrium_smoke.rs` both hit repo-local/environment issues before a clean green could be proven. The test run failed at link time because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`; the rustfmt check also surfaced pre-existing formatting differences elsewhere in `crates/kain-3D` and `crates/kain-ui-native` plus trailing whitespace in `crates/ue5-shaders/src/validation.rs`.

- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits a structured `diagnostics.composition` payload alongside the existing brief label, including summary counts, framing distance, viewport aspect ratio, and bounds span/center data. This makes the 3D smoke report much easier for tooling to consume without re-deriving scene structure from screenshots or renderer internals.
- Validation attempt: `cargo check -p kain-3D --bin material_atrium_smoke` still fails in this repo-local Windows GNU toolchain before the crate can finish compiling because build-script linking cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now counts directional and point lights in addition to meshes/materials/instances/animations/emitters/terrain, and the brief scene label surfaces those light counts when present. This makes dense lookdev or lighting-heavy scenes read more truthfully in renderer diagnostics and keeps the density cue aligned with actual authored scene complexity.
- Validation attempt: `cargo test -p kain-3d composition_summary_density_label_tracks_authoring_scale -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot find `-lgcc_eh` and `-lgcc`.
- The Kain 3D pipeline is a live fleet initiative now, and its steering should stay spec-first.
- The intended build path is native, GPU-aware 3D capability that can grow toward DCC-class tools like ZBrush, Substance Painter, and UE5-style workflows.
- Use Codex CLI through the coding-agent skill for pipeline tasks unless the user asks for another harness.
- If Codex reports a usage-limit error, verify the actual CLI output before assuming any seat-switch workaround.
- The user wants frequent updates while the pipeline is active, especially when branches, specs, or heartbeat behavior change.
- Kaino should keep the heartbeat/operator guidance current in this workspace so future passes stay aligned.
- New Kain 3D pass (2026-04-16): the WGPU renderer now preserves the same frame diagnostics as the software renderer, including scene name, viewport summary, composition summary, camera source, and catalog resolution metadata for scene renders. This closes a tooling gap where GPU-backed 3D frames were less self-describing than CPU-backed frames.
- Validation attempt: `cargo test -p kain-3d wgpu_renderer::tests::aligns_readback_rows_to_wgpu_requirement -- --nocapture` failed before reaching the 3D test because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now carries a coarse scene-density cue (`sparse` / `balanced` / `dense`) based on authored meshes, instances, emitters, and terrain surfaces. This makes scene diagnostics better at signaling when a composition is small enough for quick iteration versus crowded enough to need more careful framing or tooling.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` and `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` both failed before the tests could run because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now spells out viewport shape as `portrait` / `square` / `landscape` instead of only raw aspect ratio, and the 3D scene tests now cover that banding helper. This makes renderer diagnostics easier to scan during scene-composition work without changing the underlying framing math.
- Validation attempt: `cargo test -p kain-3d scene::tests -- --nocapture` still hits the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the `kain-3d` test binary can link.
- 2026-04-15 bootstrap update: `kain selfhost bootstrap` now exists as the owned hand-written lane entrypoint, `src/KAIN.toml` is the manifest contract, `src/build_selfhost.sh` is just a wrapper, and the bootstrap report machinery now emits JSON/Markdown under `src/.selfhost/reports/`.
- The bootstrap harness is partially green: `--combine-only` passes and writes the combined source artifact, but `--emit-llvm-only` currently hard-fails inside the owned `src/core` source set with parser errors concentrated in `runtime.kn` and `types.kn`. The immediate blocker is language/source compatibility, not the CLI wrapper or report plumbing.
- Added a 3D platform uplift in `crates/kain-3D`: primitive libraries now export richer scene metadata (`definition_count`, `definition_ids`, and startup primitive display name) when registered into an authoring scene, which makes the library more self-describing for tooling and runtime composition.
- Added `SceneDescription::composition_summary(...)` plus a shared bounds helper in `crates/kain-3D`, so tooling can ask a scene for counts and framing data in one pass instead of re-deriving it ad hoc.
- Validation was blocked by the local Windows GNU toolchain, not by the change itself. `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while linking build scripts because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): tightened default scene framing in `crates/kain-3D` so the auto-camera distance now scales with field of view instead of using a fixed radius multiplier. Added a regression test for the new framing helper to prove tighter FOVs push the camera farther back. Validation hit a repo-env Windows GNU linker gap, not a code failure: `cargo test -p kain-3d framed_camera_distance_scales_with_field_of_view` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-17): the template viewport contract now exposes explicit `composition_policy` and `framing_policy` fields, and the scene-spine validator checks that those policy tokens stay present in `viewport_runtime.kn`. This keeps the documented launch/framing policy aligned with the authored 3D runtime contract instead of letting it drift back into implicit renderer behavior.
- New Kain 3D pass (2026-04-14): scene bounds now include particle emitters, not just meshes/terrain/black holes, so auto-framing keeps volumetric FX inside the camera composition. Added a regression test proving an emitter-only scene still produces bounds and a framed camera pose. Validation was blocked by the same local Windows GNU linker gap, not by the scene logic: `cargo test -p kain-3d scene::tests` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now has a human-readable `brief_label()`/`Display` form, so 3D tooling and logs can describe a scene's composition without reformatting counts ad hoc. Added a regression assertion that `to_string()` matches the brief label. Validation was again blocked by the local Windows GNU linker gap, not the code change: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framed camera placement now scales its framing direction with the scene's horizontal and vertical extents instead of always biasing toward a fixed diagonal offset, and a new regression test covers tall-scene framing so vertical compositions stay above the scene center. This should behave better on wide or asymmetrical 3D compositions while keeping the same bounds-driven camera target. Validation hit the same repo-local Windows GNU linker gap before the test binary could build: `cargo test -p kain-3d scene::tests -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a span() helper and `SceneCompositionSummary::brief_label()` includes the full XYZ span alongside radius. This makes scene logs and tooling more spatially descriptive without re-deriving extents at each call site. Added a regression assertion that the label includes span text and that `span()` equals `half_extents * 2.0`.
- New Kain 3D pass (2026-04-14): auto-framing now respects per-view instance transform overrides through `SceneDescription::bounds_with_overrides(...)` and `framed_camera_pose_with_overrides(...)`, and the software renderer uses that override-aware camera when no explicit view camera is supplied. Added a regression test proving the frame target follows an overridden material_atrium node. Validation is still blocked locally by the Windows GNU linker gap (`-lgcc_eh` / `-lgcc` missing from `x86_64-w64-mingw32-gcc`).
- New Kain 3D pass (2026-04-14): hardened zero-length vector handling in the 3D math/render path by adding `Vec3::normalized_or(...)` and using it for particle emitter axes, orbit rotation, and basis construction in the CPU and WGPU renderers. This prevents zero-axis scene data from producing brittle normalization behavior and keeps particle/orbit math stable. Added regression tests for zero-axis particle emitters and zero-axis rotation. Validation is still blocked by the repo-local Windows GNU linker gap, and `cargo fmt --all` is currently blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- New Kain 3D pass (2026-04-14): added explicit scene resolution metadata to `SceneCatalog` via `resolve_scene(...)`, so tools can distinguish exact hits, aliases, and default fallbacks instead of treating every lookup as a plain `scene(...)` fetch. The `material_atrium_smoke` report now records requested vs resolved scene names plus the resolution kind, which makes smoke output much more useful for alias/debug triage. Validation is still blocked by the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the test binary can link.
- New Kain 3D pass (2026-04-14): auto-framed camera poses now compute near/far clip planes from scene bounds, which should reduce clipping in large or shallow compositions while preserving the bounds-driven framing target. Also cleaned up a stray syntax brace in `crates/kain-3D/src/scene.rs` that `rustfmt` surfaced during validation. Validation remains blocked by the same local Windows GNU linker gap, so `cargo test -p kain-3d scene::tests::framed_camera_clip_planes_expand_with_bounds -- --nocapture` could not link because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now includes an explicit `framed_camera_distance` derived from the scene bounds and camera FOV, and the brief label reports that fit distance alongside bounds. This gives 3D tooling a direct framing cue instead of forcing it to recompute camera fit from the raw summary. Validation on the focused `scene_bounds_and_framed_camera_follow_scene_composition` test is still blocked by the local Windows GNU linker gap (`-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the software renderer now forwards scene/tooling metadata through `FrameDiagnostics` (`scene_name`, `viewport_summary`, and a brief `composition_summary`), so hosts can label 3D frames without re-deriving context from pixels. Added a regression assertion that the framed-camera smoke scene reports those fields. Validation was blocked by the same local Windows GNU linker gap, because `cargo test -p kain-3d` could not link build scripts while `x86_64-w64-mingw32-gcc` lacked `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framing now takes viewport aspect ratio into account in `crates/kain-3D`, and both the software and WGPU renderers pass their actual aspect ratio into the scene camera fit. This should reduce clipping on wide or tall viewports without changing authored scene meaning. Added a regression test that wide viewports demand a farther camera fit than square ones. Validation is pending, but the repo-local Windows GNU linker gap has been the recurring blocker for `cargo test -p kain-3d` on this machine (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the `material_atrium_smoke` report now serializes each tile's frame diagnostics (`camera_source`, scene name, viewport summary, composition summary, and visible/culled instance lists), so tooling can inspect the actual framing decision instead of inferring it from screenshots alone. This is a tooling uplift that makes the 3D smoke output more self-describing for future debugging and scene-composition work.
- New Kain 3D pass (2026-04-14): scene composition summaries are now aspect-ratio aware in `crates/kain-3D`, so renderer diagnostics report a framing distance that matches the actual viewport instead of assuming a square view. The software renderer now feeds its real aspect ratio into the summary path, which makes frame metadata and logs more trustworthy for wide native viewports. Added a regression test for the new aspect-aware summary helper. Validation was blocked by the same local Windows GNU linker gap before the test binary could finish linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): `templates/3D/src-kain/stdlib/three_d_runtime/viewport_runtime.kn` now carries explicit `composition_policy` and `framing_policy` fields on `ViewportDescriptor`, with the default profile bound to `scene_summary_driven_and_launch_preset_bound` and `bounds_fov_and_aspect_ratio_fit`. This makes viewport launch contracts line up with the scene-summary/framing work already landing in `crates/kain-3D`, and the template README now calls out the policy explicitly for future authors.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a dominant-axis label, and `SceneCompositionSummary::brief_label()` appends a simple wide/tall/deep cue next to the span, so tooling can read scene proportions faster from logs and frame metadata. This is a small but practical authoring/tooling improvement for 3D composition debugging. Validation hit the same environment blocker as other local runs: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed during dependency linking because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now carries the viewport aspect ratio and includes it in `brief_label()`, so frame diagnostics can report the actual render shape alongside bounds and camera fit instead of leaving aspect implicit. Added a regression assertion that the summary label includes `aspect 1.00:1` for the default path. Validation pending.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::density_label()` now accounts for materials, animations, and black-hole presence in addition to meshes, instances, emitters, and terrain, so the sparse/balanced/dense cue better reflects actual scene complexity. The regression test now covers material/animation-heavy balanced scenes and black-hole-heavy dense scenes. Validation was blocked by the same local Windows GNU linker gap before the focused test binary could link: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): `crates/kain-3D` now carries catalog resolution metadata through `FrameDiagnostics` for catalog renders, so frame logs can distinguish exact scene hits from aliases and default fallbacks instead of dropping that context after resolution. The software renderer also now preserves that metadata on the returned frame, which makes alias/default debugging easier for tooling and smoke reports. Validation hit the same local Windows GNU linker gap before the focused test binary could finish linking: `cargo test -p kain-3d renderer::tests -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): auto-framed camera placement now uses an aspect-aware framing direction helper in `crates/kain-3D`, so the camera bias adapts more predictably to wide vs. tall compositions instead of using one hardcoded diagonal. Added a regression test for the direction helper. Validation was blocked by the repo-local Windows GNU linker gap when trying to run `cargo test -p kain-3d scene::tests`, and repo-wide `cargo fmt --all --check` is still blocked by trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- Superseded Kain 3D primitive note (2026-04-16): the old Rust-authored primitive catalog metadata was removed on 2026-05-11. Future primitive work should use the Kain-authored mesh ingestion registry instead of reviving catalog-policy metadata.
- New Kain 3D pass (2026-04-16): the `material_atrium_smoke` report now preserves catalog-resolution diagnostics in its JSON payload (`requested_name`, `resolved_name`, and resolution kind), so smoke consumers can distinguish exact, alias, and default scene resolution without re-parsing renderer internals. Validation of the crate still hits the local Windows GNU linker gap before the test binary can link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): fixed the WGPU renderer's camera-resolution plumbing by passing `RenderResolution` into the internal camera resolver, so the GPU 3D path can auto-frame scenes using the actual viewport size instead of a missing local variable. The WGPU frame diagnostics now also mirror the CPU renderer's structured composition cues (`scene_role`, `scene_scale`, `scene_profile`, and `framing_hint`), so GPU-backed frames are just as self-describing for scene tooling. The repo-local Windows GNU toolchain still blocks full `cargo check` / `cargo test` validation here (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`), so the next best follow-up is to run the same crate checks in a host with a working Windows GNU or compatible toolchain.
- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits structured scene-composition tags in its JSON payload (`scene_role`, `scene_profile`, `scene_density`) instead of only relying on the human-readable brief label. This makes the smoke report easier for inspectors and downstream automation to query without parsing a concatenated string. Validation still hit the repo-local Windows GNU linker gap before `cargo test -p kain-3d` could link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
# 2026-04-15 - Ouroboros now has an explicit owned bootstrap/native control-plane contract

The durable selfhost direction is now split cleanly into two lanes under the same Ouroboros control plane: the existing Rust mirror/reference lane and the hand-written owned bootstrap/native lane. The Rust mirror lane remains useful as donor, oracle, and repair infrastructure, but the hand-written lane is now the explicit promotion target for real selfhost.

What changed:

- Updated `ouroboros/docs/selfhost/pipeline_manifest.json`
  - Added `owned-bootstrap`, `owned-native`, and `owned-ouroboros` lanes beside the existing `phase2-*` lanes.
  - Added default path contracts for `src/KAIN.toml`, `src/.selfhost/`, the native runtime manifest, the runtime build script, and the first owned artifact outputs.
  - Recorded consumes/produces, success criteria, and validation commands for each owned lane so the control plane can track the hand-written bootstrap path without inventing a second planner.
- Updated `ouroboros/docs/selfhost/ouroboros-v2-selfhost-pipeline.md`
  - Reframed the selfhost docs around two lanes instead of only the Rust mirror lane.
  - Added owned-lane gates for manifest/runtime resolution, owned compiler emission, native self-build, and ouroboros parity.
- Updated `ARCHITECTURE.md`
  - Replaced the old mirror-only selfhost description with an explicit two-lane model.
  - Made `src/KAIN.toml` the canonical hand-written compiler contract and `runtime/native_runtime.toml` the canonical native runtime contract.
  - Recorded the bootstrap boundary: Rust may remain the thin host for manifest/filesystem/process/reporting work during bootstrap, but it should not stay the permanent owner of parser/typechecker/lowering/codegen once the hand-written lane is alive.
  - Added new operator notes for `kain selfhost bootstrap` and for false-green prevention under `src/.selfhost/`.

Design decisions:

- Kept the C runtime as the canonical native runtime substrate for the owned selfhost lane instead of trying to invent a runtime-free or Rust-hosted definition of native execution.
- Treated the aggregate bootstrap source under `src/.selfhost/phase0/combined/` as an explicit temporary compatibility bridge, not as the end-state module system.
- Chose to model the owned lane in the same Ouroboros manifest as the Rust mirror lane so future agents can compare, validate, and promote both lanes from one data-driven control plane.

Current risks:

- The docs now describe the owned bootstrap lane as the canonical direction, but the implementation still has to keep the emitted artifact set and the manifest fields in sync with those docs.
- The owned manifest and runtime manifest are now separate contracts by design. If either of them drifts from the CLI/bootstrap implementation, operators will get a structurally correct story and an incorrect tool.
- The owned lane will be vulnerable to false greens unless the CLI treats missing fresh artifacts as hard failures even when stale outputs remain under `src/.selfhost/`.

Recommended next step:

- Land and validate `kain selfhost bootstrap` so the owned control-plane entries are exercised by real commands, then add a strict parity check for the expected `src/.selfhost/` artifact family once the first end-to-end native self-build is green.

# 2026-04-14 - Three.js Node FFI lab grew into a sculpt suite with a Rust WASM core

The existing browser proof under `labs/threejs_node_ffi_space_lab/` is no longer only a free-fly sphere scene. It now acts as a small sculpting suite with a manifest-driven universal viewport and a local Rust `wasm32-unknown-unknown` brush kernel.

What changed:

- Added manifest registries for sculpt tools, universal viewport profiles, and the Rust WASM build pipeline.
- Added a local crate under `labs/threejs_node_ffi_space_lab/wasm/sculpt_core/` that exports raw brush deformation over vertex buffers.
- Extended `helpers/space_lab_runtime.mjs` so `npm run build` also compiles the Rust crate, copies `outputs/wasm/sculpt_core.wasm`, and serves `.wasm` with the correct MIME type.
- Split the browser client into clearer ownership layers: runtime model parsing, universal viewport control, WASM bridge, and scene/app shell wiring.
- Replaced the original free-fly-only scene with a universal viewport shell that supports sculpt, orbit, and fly modes over one floating orb in a large Three.js space.

Validation:

- `rustup target add wasm32-unknown-unknown`
- `npm run build:wasm` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `curl -I http://127.0.0.1:4192/wasm/sculpt_core.wasm`

Important behavior notes:

- The sculpt core is intentionally narrow. It mutates vertex positions only; raycasts, UI, normals, and camera policy stay in the browser/Three.js lane.
- The current localhost server for this lab must be restarted after runtime changes or it can keep serving stale MIME behavior for `.wasm`.
- The host-backed Kain JavaScript bridge issue is still unresolved in this checkout, so the validated execution path remains the Node helper commands rather than `kain run`.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so the lab can be executed end-to-end from `src/main.kn`, then decide whether this browser-side sculpt proof should stay a lab or graduate into a broader app archetype.

# 2026-04-14 - Node FFI Three.js space lab landed under labs

The repo now has a minimal browser-side proof under
`labs/threejs_node_ffi_space_lab/` that shows Kain can orchestrate a Node-owned
Three.js app and serve it on localhost without going through the native-ui lane.

What changed:

- Added `labs/threejs_node_ffi_space_lab/` with a manifest-driven app config,
  scene registry, Node runtime helper, browser client, and Kain entrypoint.
- The lab uses `std::javascript::bridge` from `src/main.kn` to call
  `helpers/space_lab_runtime.mjs`, which bundles the browser client with
  `esbuild`, emits `outputs/index.html`, and serves the generated files over a
  local Node HTTP server.
- The browser client is intentionally small and purpose-built: a giant star
  field, a beacon ring, a floating emissive sphere, and pointer-lock free-fly
  movement so the lane proves real Three.js interactivity instead of a static
  canvas.
- Added lab-local docs plus root-level `labs/README.md` and `ARCHITECTURE.md`
  updates so future agents can find the proof surface quickly.

Validation:

- `npm install` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `cargo run -q -p cli --bin kain -- fabric validate --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`

Important behavior notes:

- The live localhost proof is validated through the Node/browser lane, not the
  native-ui or `kain-3D` renderer lane. That distinction matters when debugging
  runtime regressions.
- Scene scale, lighting, server port, and movement tuning live in JSON
  manifests. Future tweaks should stay data-driven rather than drifting into
  hardcoded client constants.
- The Kain-facing entrypoints (`src/main.kn` and `KAIN.fabric.toml`) are wired
  in place, but this checkout currently fails Kain execution with unknown
  `js_import` / `js_bridge_import` identifiers before the Node helper runtime
  is reached.

Current risk:

- The proof still depends on local Node package installation in the lab root,
  so a clean checkout needs `npm install` before browser bundling or serving can
  succeed.
- The host-backed Kain JavaScript bridge registration appears to be drifting
  from the checkout's authored examples, which means the lab currently proves
  the Node + Three.js runtime path more strongly than the Kain execution path.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so `src/main.kn`
  and `kain fabric run --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`
  can execute successfully, then keep the reusable Node-side browser bundling
  and localhost helper path as a template for future web/Three.js labs.

# 2026-04-13 - native-ui dev loop tightened, Chronos native proof added, and TS effect hooks lower into native semantics

The repo now has a real native desktop iteration lane centered on
`kain native-ui dev`, plus a first Chronos-scale proof app that exercises the
same packaged runtime/realtime/shader sidecar path instead of relying on an
imported TS shell.

What changed:

- Added and validated the native desktop dev loop around
  `crates/cli/src/native_ui_dev.rs`. The loop materializes once, launches the
  packaged child, watches the authored app root recursively, ignores generated
  project/artifact trees plus common editor temp files, debounces save bursts,
  and classifies each rebuild as `Noop`, `HotReloadInProcess`, or
  `RestartProcess`.
- Repaired the native-ui reload-coordinator tests so they reflect the live
  executable-path compatibility rule instead of stale assumptions.
- Added the first native Chronos proof under `labs/chronos_native/`, authored
  directly in Kain with compiler-owned `world` state, docked native UI, tabbed
  control panels, `viewport3d`, shader sidecars, and packaged runtime snapshot
  output from one `main.kn`.
- Tightened the TypeScript importer so recognized React effect hooks
  (`useEffect`, `useLayoutEffect`, `useInsertionEffect`) lower into reactive
  component methods instead of surviving as raw hook calls in emitted Kain.
- The importer's degradation/report path is now the truth source for whether a
  generated `.kn` output is honest: parse/compile validation failures are part
  of degradation, and strict mode can fail the import while still writing the
  JSON report.

Validation:

- `cargo test -q -p kain-import test_component_hooks_lower_to_reactive_methods -- --nocapture`
- `cargo test -q -p cli native_ui_dev -- --nocapture`
- `cargo run -q -p cli --bin kain -- build native-ui labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`
- `timeout 20 cargo run -q -p cli --bin kain -- native-ui dev labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`

Important behavior notes:

- The Chronos native lab proves the packaging/dev loop shape even in this
  environment where the launched child exits through `/usr/local/bin/qmlscene`
  with status `134`. The dev loop itself still materializes, launches, prints
  the executable path, and keeps watching the app root.
- The native-ui packaging/typecheck lane is still stricter than the direct GPU
  artifact lane for at least some compute expressions. The current Chronos
  proof therefore keeps a simplified compute kernel instead of a full
  dispatch-indexed particle step.
- Dependency arrays from imported React effects are still preserved only as
  importer diagnostics, not as a complete reactive scheduler model.

Current risk:

- Native Chronos is now a real proof surface, but the current Qt host/runtime
  environment can still fail after packaging succeeds, which means desktop-loop
  validation remains split between CLI/materialization proof and live GUI-host
  proof.
- The compute authoring seam still needs reconciliation between direct
  `gpu-artifacts` acceptance and `build native-ui` acceptance before this lane
  can claim full descriptor parity for dispatch-indexed simulation code.

Recommended next step:

- Reconcile the native-ui packaging/typecheck lane with the direct GPU artifact
  lane for compute dispatch indexing, then upgrade `labs/chronos_native` from
  the simplified kernel to a real particle-step implementation and revalidate it
  in a GUI-capable environment.

# 2026-04-14 - full parity spec package for KSculpt and KPainter

The repo now has a full spec package under `.specs/ksculpt-kpainter-parity/`
plus steering docs under `.specs/steering/` that define the execution program
for taking Kain to native KSculpt and KPainter parity.

What changed:

- Added a full spec package with `requirements.md`, `design.md`, `tasks.md`,
  `validation.md`, and `decisions.md` for the parity program.
- Added steering for repo-wide standards, git workflow, and DCC native-authoring
  rules so future implementation agents have durable guardrails.
- Locked the parity destination to `apps/kain-fabric-dcc-suite` as the flagship
  native DCC app instead of spreading parity work across multiple equal app
  surfaces.
- Locked the sculpt baseline to `.reference/sculpting/*` and the painter
  baseline to `.reference/graphos/*` plus the current Kain painter scaffolds,
  because the repo does not contain a single dedicated `paint/` reference tree.
- Structured the program around:
  1. native authoring and hot-reload foundation,
  2. shared DCC session, workbench, and asset contracts,
  3. KSculpt parity vertical slices,
  4. KPainter parity vertical slices,
  5. parity harness and importer honesty.

Important behavior notes:
# New Kain 3D pass (2026-04-16): `SceneCompositionDiagnostics` now carries a structured `framing_hint` (`tight-fit` / `balanced-fit` / `loose-fit`) derived from the summary's bounds radius and framed camera distance, and `material_atrium_smoke` now includes that hint in the structured scene-composition JSON. This keeps the runtime matrix easier to scan without re-deriving camera-fit heuristics in downstream tooling.
# Validation attempt: pending in this pass, because the local Windows GNU toolchain has been the recurring blocker for `kain-3D` test linkage.

# New Kain 3D pass (2026-04-16): `material_atrium_smoke` now also threads the scene composition stage through the structured smoke JSON (`composition_stage`) at both the per-tile diagnostics layer and the shared composition payload. That gives native tooling one more stable field for distinguishing staged-line / staged-plane / staged-stack / staged-volume scenes without parsing the brief label.
# Validation attempt: `cargo test -p kain-3d scene_composition_payload_includes_stage_metadata --bin material_atrium_smoke -- --nocapture` could not finish here because the repo-local Windows GNU toolchain still fails while linking build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

# New Kain 3D pass (2026-04-17): `SceneCompositionSummary::brief_label()` now leads with the structured composition cues (`composition_stage`, role, scale, profile, focus, density) before raw counts, so scene browsers and logs can skim shape first and inventory second. This is a small design-quality uplift for tooling that already consumes the summary string.
# Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_uses_view_aspect_ratio_for_fit_distance -- --nocapture` still hits the same repo-local Windows GNU linker gap before the test binary can finish building.

# 2026-05-07 - Windows rebuild/install restored and Kain 3D build drift repaired

Windows setup was restored from `D:\Kain-Lang` using the root installer with LLVM 21 and Python 3.11:

- `py install_kain.py --clang-path C:\LLVM-21\bin\clang.exe --python-path C:\Users\Admin\AppData\Local\Programs\Python\Python311\python.exe`
- The installer bundled LLVM tools into `toolchain/llvm/bin`, built release `kain.exe` / `kn.exe`, copied both into `C:\Users\Admin\.cargo\bin`, and wrote `generated/kain-env.ps1`.
- Future PowerShell sessions should dot-source `. .\generated\kain-env.ps1` before local validation so `KAIN_STDLIB_PATH`, `KAIN_RUNTIME_C_PATH`, `KAIN_RUNTIME_MANIFEST_PATH`, `KAIN_CLANG_PATH`, and `PYO3_PYTHON` match the installed binary.

What changed:

- Repaired `crates/kain-3D` workspace build drift by re-exporting `SceneResolution`, `SceneResolutionKind`, and `SceneCatalogSummary`, adding `Vec3::normalized_or` to match the existing `Vec2` fallback-normalization API, and making catalog entry composition diagnostics sample time explicitly at `0.0`.
- Promoted `camera_fit_ratio` into `SceneCompositionDiagnostics` so `material_atrium_smoke` can serialize the same composition payload truth that frame diagnostics already carry.
- Updated the `material_atrium_smoke` composition payload test to the current live scene metadata: `world-scale`, `volumetric`, `staged-volume`, `instance-led`, and `dense`.

Validation:

- `cargo build --workspace` passes under `. .\generated\kain-env.ps1`.
- `kain doctor` and `kn doctor` resolve the installed cargo-bin launchers, repo stdlib, runtime C file, runtime manifest, and bundled LLVM clang.
- `py docs\examples\validate_examples.py --kain C:\Users\Admin\.cargo\bin\kain.exe` validates all 12 docs examples.
- `cargo test -p kain-3d scene_composition_payload_includes_stage_metadata -- --nocapture` passes.
- `cargo test -p kain-3d catalog_scene_render_diagnostics_include_resolution_context -- --nocapture` passes.

Current risks:

- Full `cargo test -p kain-3d -- --nocapture` now compiles but still has 13 stale assertion failures around primitive counts and scene/camera composition expectations. The live build and targeted smoke surfaces are healthy; the broader 3D test suite needs a focused expectation refresh.
- Root `cargo fmt` is still blocked by pre-existing trailing whitespace in `crates/ue5-shaders/src/validation.rs`; format only touched files or clean that file first before expecting repo-wide fmt to run.

# 2026-05-11 - Kain 3D hardcoded demo cleanup

`crates/kain-3D` no longer owns built-in showcase/demo scenes. `SceneCatalog` is now explicit data: callers construct it with authored `SceneDescription` values and optional aliases, while `SceneCatalog::empty()` is the honest no-scene host fallback. The old embedded catalog, terrain/black-hole special cases, and demo-specific frame diagnostics were removed so Kain source, realtime bundles, or assets own scene identity.

The Win32 native viewport now carries one neutral `default_viewport` fallback profile and a generic fallback draw path. Raw native labs that need a fallback profile should set `KAIN_NATIVE_SCENE_PROFILE=default_viewport`; authored viewport scenes should still travel through Kain UI/runtime bundle data such as `geometry_fixture`.

The 3D smoke binary is now `generic_scene_smoke` and the package disables Cargo auto-bin discovery so the legacy demo-named local file is not part of the crate surface. The local filesystem ACL prevented deleting/renaming that old file in place, so future cleanup may need an elevated shell to physically remove it from this checkout; the intended repo path is `crates/kain-3D/src/bin/generic_scene_smoke.rs`.

Validation:

- `cargo check -p kain-3d --bins --lib --target-dir target\codex-kain-3d-clean-check` passes.
- `cargo test -p kain-3d --target-dir target\codex-kain-3d-clean-test` passes: 27 lib tests, 2 smoke-bin tests, 0 doc tests.
- `cargo check --bins` exposed a separate `kain-fs::canonicalize_path` return-type drift; `kain-c-ffi`, `kain-crate-ffi`, and `kain-codebase` now convert the returned `String` into `PathBuf` at PathBuf-owning call sites.
