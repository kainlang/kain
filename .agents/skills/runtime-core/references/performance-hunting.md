# Performance Hunting

Read this when the runtime-core task is about speed, reducing overhead, beating C++/Rust rows, or landing proof-backed "alien" C runtime fast paths.

## Rule Zero

Runtime performance work needs two kinds of truth:

- Solver truth: the fast path preserves the invariant or is equivalent to the scalar/reference path.
- Measurement truth: the relevant benchmark, conformance telemetry, or attrition lane shows the change helped and did not poison lifecycle behavior.

Proof without measurement can be unused cleverness. Measurement without proof can be a time bomb.

## Hunt Loop

1. Identify the pressure row or hot function.
2. Capture the current baseline: benchmark report, conformance timing, profiler clue, or runtime counter.
3. Find the semantic invariant that lets Kain do less work than a conventional runtime.
4. Model the dangerous math in Z3.
5. Build the smallest fast path with a scalar fallback and explicit guard.
6. Prove equivalence/bounds.
7. Run focused conformance/fixture.
8. Run benchmark or attrition.
9. Save proof/report and update durable notes if this changes future agent behavior.

## Current Hot Surfaces

Actor runtime:

- Occupancy-word actor table.
- Power-of-two scheduler ring and mask indexing.
- TLS-cached reply-port state.
- Generation-tagged `KainActorRef` stale reply rejection.
- Inline reply payload capacity.
- Mailbox node caching and bounded queue counters.
- Microcell turn scheduling and nonblocking poll/yield paths.

Memory and ownership:

- 16-byte `KainAllocHeader` with magic+slot token.
- Helper allocation cache with bucketed payload sizes.
- Pointer-size offset helpers with overflow checks.
- Ownership pointer-index hash/probe table.
- Occupancy-word free-slot discovery.
- de Bruijn low-bit index tables.
- Imported pointer registration guard.
- Ephemeral-local compiler lowering may erase runtime allocation calls when the trace is provably local and non-escaping.

Services and registry:

- Magic-prefix service key metadata.
- Case-insensitive canonicalization plus descriptor key length/state precheck.
- Alias table instead of scattered compatibility strings.
- Data-driven service catalog from manifest and native registration.

Machine stones and SIMD:

- CPU capability gates for AVX/AVX2/AVX-512.
- Scalar fallback plus target-attributed vector lanes.
- Shatter SoA lane buffers.
- Pulse scheduler counters.
- Teleport token telemetry.
- Converge selector/cache and mismatch telemetry.

Stdlib facade:

- Tagged `Option`, `Result`, and `Future` handles at the ABI boundary.
- Patch/law/entangle counters and status helpers.
- Native filesystem builder/copy/status paths.
- String/handle helpers where LLVM still needs compatibility payloads.

## Solver Weapons

Use `tool-z3-black-magic` with `runtime-core` when hunting:

- magic constants
- perfect or near-perfect key hashes
- de Bruijn tables
- branchless selectors
- bit-packing layouts
- cache/ring masks
- SIMD lane rewrites
- table-driven state classifiers
- small bounded schedulers
- collision-free token signatures

Useful MCP calls:

- `optimize`: search constants/tables.
- `bitvec_equiv`: prove branchless or vector expression equivalence.
- `range_check`: prove index/cache/table bounds.
- `state_machine_check`: prove transition lattice behavior.
- `find_counterexample`: break a proposed guard before coding it.
- `check_smt2`: quick experimental solver loop.

Save rough hunts under:

```text
runtime/native/src/core/z3/proofs-experimental
```

Promote durable claims to:

```text
runtime/native/src/core/z3/proofs
```

## Candidate Optimizations By Area

Actor scheduler:

- Replace linear actor-table scans with occupancy-word low-bit discovery.
- Keep scheduler queue capacity power-of-two and prove mask-index bounds.
- Avoid OS thread churn; prefer worker pool, microcell turn execution, and explicit overflow counters.
- Inline local ask/reply only when backlog, generation, and exclusive-turn preconditions are proven.
- Cache mailbox nodes or payload buffers only behind bounded capacity proofs.
- Measure with actor mailbox, ask/reply, supervision, and semantic singularity benchmarks.

Actor reply ports:

- Prefer generation-tagged refs over raw actor ids.
- Borrow inline reply payloads only when lifetime is bounded by wait completion.
- Keep spin waits small and fallback to condition/event waits.
- Prove stale reply rejection and spin fallback behavior.

Memory helpers:

- Cache helper allocations only for payload bands that are frequent enough to matter.
- Prove cache byte/node caps and header+payload accounting.
- Keep payload size in the header and invalidate magic when an allocation becomes cache-owned.
- Do not bypass ownership registration to save cycles; make registration faster.
- Use compiler-owned ephemeral-local lowering when possible instead of runtime heroics.

Ownership registry:

- Use hash/probe metadata and occupancy words for lookup/free-slot work.
- Prove pointer-index probe bounds, tombstone behavior, and slot-token capacity.
- Keep observer count overflow impossible under guards.
- Never let imported fake headers impersonate helper allocations.

Services:

- Replace repeated case-insensitive string scans with precomputed key length/state plus exact compare.
- Keep alias canonicalization table-driven.
- Prove magic-token collision freedom for the known catalog before replacing compares.
- Avoid making service lookup platform-dependent.

SIMD and converge:

- Keep scalar reference functions simple and obviously correct.
- Gate vector lanes by runtime CPU capability or compiler target attribute.
- Prove vector reduction/factorization equivalence and tail handling.
- Use benchmark cases that stress enough data to overcome dispatch overhead.
- Keep `converge_mismatch_count()` and equivalent native telemetry visible.

Reflection and runtime metadata:

- Use compact selectors for item/type kinds only after collision/equivalence proof.
- Avoid reparsing JSON when sidecar metadata can be cached or staged.
- Keep schema version checks explicit.

Net/process/input/ui domain helpers:

- Co-trigger `runtime-stdlib` when optimizing public domain behavior.
- Prove buffer growth, handle tables, and capture/body bounds.
- Keep unsupported-platform behavior honest and diagnostic-backed.

## Benchmark Contract

Before claiming a win:

- Name the benchmark row or fixture.
- Record the baseline command and result.
- Run the same command after the change.
- Report speedup/regression and variance if available.
- If the row is still slower than C++ or Rust, say what remains.
- If a proxy benchmark is used, label it as proxy and explain the missing final lane.

Good commands to start from:

```powershell
python benchmark/run.py --case actor_mailbox_erlang,ownership_memory,semantic_singularity_crucible --languages kain --runs 3 --warmups 1 --timeout 900
python benchmark/run.py --case simd_lane_mix,sim_nbody_gravity --languages kain --runs 3 --warmups 1 --timeout 900
```

Always check live benchmark truth first:

```powershell
Get-Content benchmark/latest.md
Get-Content benchmark/out/reports/latest.llm.md
```

## Attrition Contract

Use attrition when a fast path changes lifecycle, cleanup, long-run counters, actor/async/process handles, heap behavior, or teardown.

Look for:

- live count returns to zero
- peak counters are sane
- stale rejects are counted
- event ring does not overflow silently
- shutdown joins before cleanup
- ownership decay frees only valid idle heap regions
- async timers/tasks cannot leak after cancellation

Commands:

```powershell
python attrition/run.py --help
rg -n "actor|async|ownership|process|runtime|heap|teardown" attrition runtime/native
```

## Things Worth Trying

- Replace branch-heavy state classifiers with table-driven token states proved equivalent.
- Precompute service/reflection key metadata at registration time and compare integers before strings.
- Use de Bruijn low-bit index or compiler intrinsics for occupancy/free-slot scans, with portable fallback.
- Split hot/cold actor paths so diagnostics remain rich but successful local sends stay lean.
- Add cache-line aligned structs only when measured false sharing exists.
- Batch registry or service validation work during startup instead of repeating on every call.
- Let codegen erase runtime calls when proof says the value is stack-local, non-escaping, and balanced.
- Push benchmark pressure into authored Kain so the runtime fast path is dogfooded by real semantics.

## Things Not Worth Doing

- Hand-optimizing platform-specific C branches that should be provider data.
- Hiding a runtime regression by deleting Kain semantics from a benchmark.
- Replacing diagnostics with silent sentinels in public ABI calls.
- Adding a magic constant without a proof and a comment naming the proof.
- Adding a proof without running the executable lane that uses the code.
- Adding a benchmark-only fast path that codegen cannot reach.
- Baking a package/vendor/app policy into `runtime/native/src/core`.
- Widening public stdlib surface without updating the stdlib map.

## Code Comment Pattern

Use short proof breadcrumbs near non-obvious fast paths:

```c
/* Proof: runtime/native/src/core/z3/proofs-experimental/<claim>.smt2 */
```

or, for durable proofs:

```c
/* Proof: runtime/native/src/core/z3/proofs/<claim>.yaml */
```

Keep the comment honest. If the proof file moves or is promoted, update the breadcrumb.

## Final Report Pattern

```text
Performance:
- Baseline: <command/result>.
- New: <command/result>.
- Delta: <speedup/regression>.

Proof:
- <claim> -> unsat via <file/lane>.

Runtime validation:
- <conformance/fixture/attrition> -> PASS.
```
