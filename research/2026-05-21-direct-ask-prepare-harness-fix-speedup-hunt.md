# 2026-05-21 Direct Ask Prepare + Harness Fix Speedup Hunt

## Mission

Take the cleanest remaining benchmark frontier after the inline scheduler lock cut, land a real speedup without cheating, prove the unsafe actor token math, and leave the benchmark lane honest enough that the next agent can trust the full-suite result.

## Skills / lanes used

- `tool-z3-black-magic`
- `breakthrough-research-lab`
- `test-bench`
- `runtime-core`

## What changed

### 1. Compiler-lowered `ask` now arms a direct reply token without synthesizing a reply-port actor ref

- Added `kain_actor_reply_port_prepare_direct(KainActorRef* out_ref)` to the native actor ABI.
- Reused the TLS reply-port state and emitted a direct token with:
  - `actor_id = KAIN_ACTOR_ID_INVALID`
  - `execution_class = KAIN_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT`
  - `locality_class = KAIN_LOCALITY_CLASS_LOCAL`
  - a fresh non-zero generation counter on every re-arm
- Swapped LLVM ask lowering from:
  - `reply_port_new()`
  - `reply_port_actor_ref()`
- To the direct prepare lane:
  - `reply_port_prepare_direct()`

This cut the synthetic bind/unbind traffic that was still sitting on the hot actor ask path after the earlier inline mail and reply-handle work.

### 2. Proved stale direct reply tokens cannot survive re-arm

- Added SMT case:
  - `runtime/native/src/core/z3/proofs-experimental/actor-reply-port-direct-token-rearm-invalidates-stale-generation.smt2`
- Added durable proof entry:
  - `runtime/native/src/core/z3/proofs/actor-reply-port-direct-token-rearm-invalidates-stale-generation.yaml`
- Direct solver result: `unsat`
- Actor proof pack rerun: `17 proved, 0 counterexamples, 0 unknown, 0 errors`

### 3. Hardened the Windows benchmark hygiene lane

The previous full suite was polluted by two benchmark-harness lies:

- stale `generated/native_runtime/cache/.../*.obj.tmp` cleanup could fail with `Access is denied`
- successful builds could be marked failed when the executable appeared a moment late on Windows

Fixes:

- `crates/cli/src/main.rs`
  - extended stale-file delete retries on Windows
  - downgraded stale `*.obj.tmp` removal to best-effort so dead temp files do not abort a valid build
- `benchmark/run.py`
  - retry build commands on `access is denied` and `unable to remove stale runtime cache artifact`
  - wait briefly for generated executables to appear before declaring build failure

## Validation

- `cargo test -p kain-sys-codegen --test llvm_codegen_test actor_ask_reply --target-dir target/codex-direct-ask`
- `cargo test -p kain-actor --target-dir target/codex-direct-ask`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/actor.c runtime/conformance/actor_runtime/test_actor_abi_contract.c`
- `cargo check -p cli --target-dir target/codex-benchmark-hygiene`
- `python -m py_compile benchmark/run.py`
- `python benchmark/run.py --case pulse_teleport_decay_mesh,semantic_host_bridge_fusion,branch_dispatch,scalar_mix --languages kain,cpp --runs 1 --warmups 0 --baseline-mode off --latest-stem latest_benchmark_hygiene_probe --minimal-name benchmark_hygiene_probe`
- `python benchmark/run.py --baseline-mode auto --latest-stem latest_full_after_direct_ask_harness_fix --minimal-name full_after_direct_ask_harness_fix`

## Benchmark outcome

### Clean full suite

- Report: `benchmark/out/reports/latest_full_after_direct_ask_harness_fix.llm.md`
- JSON: `benchmark/out/reports/latest_full_after_direct_ask_harness_fix.json`
- Status: `PASS`

### Target delta vs `latest_full_after_inline_scheduler_cut`

- `actor_ownership_backpressure`
  - old Kain median: `459.963 ms`
  - new Kain median: `302.735 ms`
  - speedup: `1.52x`
- `semantic_fabric_relay`
  - old Kain median: `114.693 ms`
  - new Kain median: `89.066 ms`
  - speedup: `1.29x`
- `pulse_teleport_decay_mesh`
  - old Kain median: `109.894 ms`
  - new Kain median: `93.344 ms`
  - speedup: `1.18x`
- `semantic_host_bridge_fusion`
  - old Kain median: `1101.984 ms`
  - new Kain median: `1136.830 ms`
  - delta: slight regression / noise, still needs a real attack

## Honest frontier ranking from the clean suite

### Biggest Kain gaps vs fastest available competitor

1. `actor_ownership_backpressure`
   - Kain `302.735 ms`
   - C++ `18.340 ms`
   - gap: `16.51x`
2. `recursive_sum`
   - Kain `112.582 ms`
   - C++ `9.657 ms`
   - gap: `11.66x`
3. `semantic_fabric_relay`
   - Kain `89.066 ms`
   - C++ `10.658 ms`
   - gap: `8.36x`
4. `pulse_teleport_decay_mesh`
   - Kain `93.344 ms`
   - C++ `16.827 ms`
   - gap: `5.55x`
5. `semantic_host_bridge_fusion`
   - Kain `1136.830 ms`
   - C++ `855.081 ms`
   - gap: `1.33x`

### Highest-value next attack surfaces

#### A. Finish the actor ask fast lane

The direct-token prepare cut helped, but Kain is still spending too much time after the send:

- likely remaining tax:
  - per-ask wait/result bookkeeping
  - reply completion path still materializing more state than needed for same-turn local completion
  - extra ref liveness / generation checks in the hot path
- next move:
  - add a same-turn inline completion lane where a local inline ask that fully replies before suspension writes the result directly without routing through the full reply-port wait state

This is the best remaining semantic frontier because one fix can hit:

- `actor_ownership_backpressure`
- `semantic_fabric_relay`
- `pulse_teleport_decay_mesh`

#### B. Attack recursion lowering

`recursive_sum` is now the loudest implemented row, not a semantic proxy. That makes it a high-value compiler truth bug, not just a benchmark curiosity.

- likely causes:
  - recursive call ABI overhead
  - missed inlining / loopification for obviously closed self-recursion
  - avoidable stack traffic in LLVM lowering
- next move:
  - inspect the generated LLVM for `benchmark/cases/recursive_sum/main.kn`
  - prove whether the recursive shape is closed enough for loop lowering or a tail-recursive normalization pass

This is the cleanest path to a likely multi-x win outside the semantic-state actor cluster.

#### C. Trim host-bridge loop overhead

`semantic_host_bridge_fusion` did not benefit from the ask token work.

- likely tax:
  - per-iteration host bridge marshalling
  - repeated process/path/spec normalization
  - bridge string/handle materialization churn
- next move:
  - trace the Kain/native bridge call count per round and hoist invariant bridge state out of the inner loop

## Recommendation for the next agent

1. Stay on the actor fast lane first:
   - same-turn direct completion for local inline ask/reply
2. If that path stops yielding clean wins, pivot immediately to:
   - `recursive_sum` LLVM lowering
3. Keep using the clean benchmark stem:
   - `latest_full_after_direct_ask_harness_fix`

The benchmark harness is honest again. The next big win should come from either collapsing the rest of local ask/reply completion overhead or finally teaching Kain to lower simple self-recursion like an alien compiler instead of a polite one.
