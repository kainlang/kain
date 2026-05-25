# Proof And Validation

Read this when runtime-core work touches arithmetic, memory, ABI layout, state transitions, service capacity, scheduler behavior, or any unsafe fast path. The default standard is proof first, then executable validation.

## Proof Mindset

Start by naming the property:

- "header plus payload cannot wrap before allocation"
- "mailbox receive count never underflows"
- "reply-port generation mismatch rejects stale replies"
- "scheduler dequeue preserves depth accounting"
- "shatter lane pointer stays inside the payload"
- "service descriptor copy writes the null terminator only inside capacity"
- "branchless selector is equivalent to the scalar branch"

Then choose the claim shape:

- bounds: `0 <= index < capacity`
- accounting: `new_depth = old_depth + enqueued - dequeued`
- monotonicity: `max_depth' >= max_depth`
- exclusivity: two states cannot both own a resource
- implication: if guard `G` holds, bad state `B` is impossible
- equivalence: fast path and scalar path return the same value
- reachability: forbidden state is unreachable in N steps
- collision freedom: known tokens map to unique signatures

For low-level C, model widths explicitly. Signed `int64_t`, unsigned `uint64_t`, `size_t`, and pointer-sized arithmetic are not interchangeable.

## Native Core Z3 Pack

Pack root:

```text
runtime/native/src/core/z3
```

Layout:

- `z3.toml`: pack name, source scope, solver defaults, lanes, reports.
- `proofs/*.yaml`: durable proof cases that future agents should rerun.
- `proofs-experimental/*.smt2`: exploratory SMT2 hunts, magic constants, candidate invariants, and solver sketches not yet promoted.
- `templates/*.yaml`: pack-local source extraction templates.
- `reports/`: generated proof reports. Treat as validation output.
- `generated/`: counterexample-to-test outputs or proof-derived artifacts.
- `assumptions/` and `cache/`: analysis cache and assumption material. Inspect when extraction behaves oddly; do not hand-edit casually.

Lane names from `z3.toml`:

- `smoke`: all YAML proof cases as a quick pack check.
- `full`: all proof artifacts.
- `actor`: `proofs/actor-*.yaml`.
- `memory`: `proofs/native-memory-*.yaml`.
- `ownership`: `proofs/native-ownership-*.yaml`.
- `entangle`: `proofs/native-entangle-*.yaml`.
- `machine`: `proofs/native-machine-*.yaml`.
- `services`: `proofs/native-services-*.yaml`.
- `stdlib`: `proofs/native-stdlib-*.yaml`.
- `net`, `process`, `graphics`, `realtime`, `native`: focused aggregate lanes.

## Commands

Batch runner:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane actor
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane memory
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane ownership
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane full
```

MCP flow:

```text
analyze_source_file -> suggest_proof_targets -> extract_source_proof_cases(save=true)
-> prove_or_witness/check_smt2/helper proof tool -> save_proof_case_to_pack
-> run_proof_pack -> code patch -> rerun proof pack
```

Useful MCP tools:

- `analyze_source_file`: inspect a C source seam and suggested findings.
- `suggest_proof_targets`: ask the proof engine where a file is mathematically dangerous.
- `extract_source_proof_cases(save=true)`: create candidate proof cases from annotated/source patterns.
- `list_templates(context_path=...)`: see local/builtin extraction templates.
- `check_smt2`: run one-off SMT2, useful for `proofs-experimental`.
- `find_counterexample`: ask for a witness to a suspected invariant failure.
- `prove_or_witness`: run a structured proof case.
- `save_proof_case_to_pack`: promote a durable proof into the pack.
- `run_proof_pack`: rerun a lane.
- `counterexample_to_test`: convert a useful witness into a regression test.

Helper proof tools:

- `size_add_ok`, `size_mul_ok`: allocation and buffer accounting.
- `ptr_offset_ok`: pointer element/byte/span offsets.
- `buffer_growth_ok`: append/reserve/growth invariants.
- `content_length_ok`: parsed HTTP/content-length boundaries.
- `range_check`: expression bounds.
- `signed_unsigned_cast_ok`: ABI cast boundaries.
- `bitvec_equiv`: branchless/SIMD/magic constant equivalence.
- `state_machine_check`: actor, async, compatibility, reload, ownership transitions.
- `optimize`: find constants or small tables.

## `proofs-experimental`

Use `proofs-experimental` for exploratory work that may be wrong, too narrow, or not yet connected to a source template.

Good uses:

- solver-discovered constants and tables
- de Bruijn index tables
- branchless selector equivalence sketches
- state-machine sketches before durable YAML promotion
- magic-token collision hunts
- bounded ring/index proofs while deciding the final C shape
- fast-path preconditions that may become code guards

Rules:

- Name files after the claim, not after the mood: `actor-scheduler-ring-mask-index-bounds.smt2`, not `new-idea.smt2`.
- Include comments for the C function/constant being modeled.
- Make the query ask for a violation. `unsat` means the bad state is impossible under assumptions.
- Keep assumptions explicit at the top.
- Promote durable results to `proofs/*.yaml` or a local template when they protect landed code.
- Delete or rename stale breadcrumbs if C comments point at a proof that no longer exists.

## Durable Proof Case Standard

Every durable proof should answer:

- What code seam is protected?
- What property is modeled?
- What assumptions are required?
- What is the bad state?
- What does `unsat` mean?
- Which lane reruns it?
- Which test/fixture/benchmark complements it?

Bad proof:

```text
"queue works"
```

Good proof:

```text
actor-scheduler-dequeue-preserves-depth-accounting:
Given 0 < old_depth <= capacity and one successful dequeue,
new_depth = old_depth - 1 and cannot underflow.
Violation query is unsat.
```

## File-To-Proof Matrix

| Runtime seam | Properties to prove |
| --- | --- |
| `actor.c` mailbox | send count within capacity, receive count never underflows, try-receive underflow impossible, node cache bounded. |
| `actor.c` scheduler | ring mask index bounds, enqueue/dequeue accounting, max depth monotonicity, no stale queued actor after shutdown. |
| `actor.c` reply ports | generation rearm invalidates stale replies, ref match requires equal generation, spin wait preserves fallback, inline payload capacity. |
| `actor.c` supervision | restart count stays within window limit, termination cleanup joins/clears before freeing. |
| `async.c` | task state transitions, timer handle bounds, cancellation idempotence, result cleanup ownership. |
| `memory.c` | header+payload no wrap, payload size no wrap, pointer-size offset no wrap, cache bytes/nodes bounded. |
| `ownership.c` | slot token within registry capacity, observer count no overflow, decay only idle heap, imported fake header cannot bypass registration, pointer-index probe bounds. |
| `services.c` | text copy fits before null write, alias canonicalization states, magic key signatures collision-free for catalog. |
| `contract.c` | required-service masks imply startup failure when missing, strict-mode behavior, diagnostic capacity. |
| `entangle.c` | register count within capacity, endpoint text copy fits. |
| `machine_stones.c` | capability token signatures unique, pulse missed beat bounded, shatter lane offset in payload, teleport handoff exclusive. |
| `simd.c` | vector lane equivalence to scalar fallback, reduction/factorization equivalence, tail handling bounds. |
| `converge.c` | selector cache bounds, first eligible lane behavior, mismatch counter monotonicity. |
| `json.c`, `wire.c` | parse cursor bounds, encoded length growth, null terminator and buffer ownership. |
| `stdlib_abi.c` | result/option/future handle state, patch journal capacity, fs builder growth, public wrapper ABI shapes. |
| `process_system.c`, `net_system.c` | handle index bounds, capture/body allocation growth, content-length nonnegative parse, request-slot reuse. |
| `realtime.c`, `reflection.c`, `compatibility.c` | fixed buffer copies, item/type selector equivalence, schema/token state machines, transition lattice. |

## Validation Ladders

Z3 proves invariants. Native tests prove integration. Benchmarks prove speed. Attrition proves long-run closure. Use the narrowest ladder that matches the risk, then climb for shared ABI.

Fast local smoke:

```powershell
kain runtime build
kain runtime validate --skip-cli-build
```

Runtime fixtures:

```powershell
./runtime/fixtures/validate_all.sh
powershell -ExecutionPolicy Bypass -File runtime\fixtures\validate_all.ps1
```

Runtime conformance:

```powershell
./runtime/conformance/run_all.sh
powershell -ExecutionPolicy Bypass -File runtime\conformance\run_all.ps1
```

Focused lanes:

```powershell
bash runtime/conformance/actor_runtime/run_tests.sh --verbose
bash runtime/conformance/async_runtime/run_tests.sh --verbose
bash runtime/conformance/diagnostics/run_tests.sh --verbose
bash runtime/conformance/host_bridge/run_tests.sh --verbose
bash runtime/conformance/platform_parity/run_tests.sh --verbose
bash runtime/conformance/reflection/run_tests.sh --verbose
```

Bazel:

```powershell
py -3 tools/bazel/sync_native_runtime_builds.py --check
bazel build //runtime:all
bazel test //runtime:native_runtime_tests
```

Aggregate:

```powershell
./runtime/validate_native_runtime.sh
powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1
```

Benchmark, when performance is claimed:

```powershell
python benchmark/run.py --case <case-name> --languages kain --runs 3 --warmups 1 --timeout 900
```

Attrition, when long-run lifecycle/teardown is claimed:

```powershell
python attrition/run.py --help
rg -n "actor|async|ownership|runtime|teardown|native" attrition
```

## Promotion Workflow

1. Inspect source and nearby proofs.
2. State the invariant in plain English.
3. Model the smallest dangerous seam with helper tools or SMT2.
4. Get `sat` witness or `unsat` proof.
5. If `sat`, patch code or strengthen guard, then rerun.
6. Save the durable proof or experimental SMT2.
7. Run the focused proof lane.
8. Run native conformance/fixture.
9. Run benchmark/attrition when the claim is speed or lifecycle.
10. Record the proof and validation in the final answer; update `MEMORY.md` only when it changes durable operator knowledge.

## Proof Failure Triage

If a proof fails:

- Do not weaken the claim first.
- Read the model/witness.
- Map witness values back to code units: bytes, elements, slots, generations, handles, ticks.
- Decide whether the code needs a guard, a type width change, a stronger invariant, or a corrected proof assumption.
- Add a regression test only after the witness describes a real reachable bug.

If a proof times out:

- Shrink bit width only if the property is width-parametric and you state that assumption.
- Split the claim by helper function or transition.
- Replace nonlinear arithmetic with bounded constants only when those constants are real runtime limits.
- Consider a pack-local template/plugin if extraction is repetitive.

## Closeout Evidence Template

```text
Proof:
- Modeled <property> for <file:function>.
- Assumptions: <bounds/widths/preconditions>.
- Result: unsat via <proof file or command>.

Validation:
- <command> -> PASS/FAIL/BLOCKED.

Residual risk:
- <platform, benchmark, or proof gap>.
```
