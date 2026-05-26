# Direct Ask Owner Inline Wait Assessment

- date: `2026-05-21`
- focus: `compiler-lowered direct ask reply-port prepare plus owner-thread inline completion readback`
- evidence:
  - focused baseline: `benchmark/out/reports/latest_actor_frontier_baseline.llm.md`
  - focused retake: `benchmark/out/reports/latest_actor_owner_inline_wait_combo_9.llm.md`
  - canonical full suite: `benchmark/out/reports/latest.llm.md`
  - proof lanes:
    - `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-token-rearm-invalidates-stale-generation.smt2`
    - `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-owner-inline-stale-direct-token-rejected.smt2`
  - proof reports:
    - `z3/reports/20260521T223537Z-actor-reply-port-direct-token-rearm-invalidates-stale-generation.json`
    - `z3/reports/20260521T223537Z-actor-reply-port-owner-inline-stale-direct-token-rejected.json`

## What changed

- `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - compiler-lowered direct asks now call `kain_actor_reply_port_prepare_direct(...)` instead of minting a synthetic actor-table reply ref through `kain_actor_reply_port_new()` plus `kain_actor_reply_port_actor_ref(...)`
- `runtime/native/src/core/actor.c`
  - added direct-token reply-port rearm with generation bump and invalid-actor direct refs
  - added the `owner_inline_ready` fast path so same-thread inline reply completion can be copied back by the owner thread without re-taking the reply-port lock or waking itself
- `runtime/native/include/actor.h` and actor ABI tests
  - exported and validated the new direct-prepare reply-port ABI

## Honest performance result

- Focused actor frontier retake:
  - `actor_ownership_backpressure`: Kain `456.236 ms` -> `309.923 ms`
  - `semantic_fabric_relay`: Kain `114.217 ms` -> `93.487 ms`
- Canonical full suite:
  - `actor_ownership_backpressure`: Kain `313.311 ms`, C++ `18.673 ms`
  - `semantic_fabric_relay`: Kain `91.346 ms`, C++ `11.272 ms`
  - `pulse_teleport_decay_mesh`: Kain `93.919 ms`, C++ `16.827 ms`

## What this really means

- The actor win is real and big enough to keep. This was not a micro-noise pass.
- The direct ask setup path and same-thread reply-port completion path were both still charging hot actor traffic after the earlier lock/snapshot cuts.
- The actor semantic rows are still nowhere near done. Even after this pass, `actor_ownership_backpressure` remains the loudest frontier in the full suite at `16.78x` behind C++.

## Current benchmark frontier

1. `actor_ownership_backpressure`
- Kain `313.311 ms`, C++ `18.673 ms`
- Highest-value honest runtime frontier in the fresh PASS suite.

2. `unicode_string_heavy`
- Kain `102.749 ms`, C++ `9.917 ms`
- Biggest non-proxy implemented loss. This is the best non-actor target if the next pass wants a different subsystem.

3. `semantic_fabric_relay`
- Kain `91.346 ms`, C++ `11.272 ms`
- Same actor/request substrate in a smaller semantic package.

4. `pulse_teleport_decay_mesh`
- Kain `93.919 ms`, C++ `16.827 ms`
- Third witness row for the same semantic actor/world ownership machinery.

5. `http_server_concurrency`
- Kain `73.373 ms`, Rust `50.901 ms`
- Worth revisiting only after the actor cluster or `unicode_string_heavy`.

## Full-suite hygiene note

- The first full rerun hit a stale Windows linker/permission failure on `benchmark/out/build/process_stdio_loop/kain/process_stdio_loop.exe`. Removing that generated executable and rerunning restored a clean canonical `PASS`.
- No new benchmark rows were needed. The live suite already contains several honest 2-10x frontier candidates.

## Recommendation for the next agent

1. Stay on the actor frontier and attack request-side ownership/dispatch after direct ask setup.
2. Keep `semantic_fabric_relay` and `pulse_teleport_decay_mesh` beside `actor_ownership_backpressure` so we do not overfit a single actor row.
3. If actor work stalls, pivot to `unicode_string_heavy` before inventing a new benchmark.
