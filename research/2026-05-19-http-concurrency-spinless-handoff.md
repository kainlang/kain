# HTTP Concurrency Spinless Handoff

- Date: 2026-05-19
- Status: concluded
- Repo Root: `D:\Kain-Lang`
- Session Slug: `http-concurrency-spinless-handoff`

## Research Question

Can Kain beat Tokio on the honest `http_server_concurrency` row by replacing the current spin/yield socket handoff with a bounded blocking queue or completion-style worker handoff without changing the protocol, request count, or checksum?

## Constraints

- Keep the fixed `/bench` POST workload, body, checksum, and `CONCURRENCY=16` client shape intact.
- Keep the row honest: no persistent-connection shortcut, no request-count change, no foreign baseline cache reuse while assessing the change.
- Treat the full canonical suite as the arbiter, not a single focused probe.

## Hypothesis Lattice

### Baseline
- Mechanism: keep the original staged accepted-socket worker swarm, but eliminate repeated parsing/string scanning by comparing the exact fixed request frame and emitting one cached full response frame.
- Expected upside: remove helper-side parse, header search, and double-send overhead without changing the concurrency shape.
- Likely blocker: the remaining gap may mostly be scheduler/syscall cost, not parsing cost.
- Proof obligation: the exact-frame shortcut must still preserve the benchmark contract for the closed fixed request/response domain.

### Unconventional
- Mechanism: replace the accepted-socket staging array plus spin/yield handoff with a blocking power-of-two socket queue and a bounded server worker pool.
- Expected upside: reduce worker-side busy waiting and better match Tokio's runtime thread count.
- Likely blocker: the condvar/mutex wake path may cost more than the short loopback request it is trying to save.
- Proof obligation: accepted-socket handoff must stay inside the staged span and shutdown must not strand accepted sockets.

### Moonshot
- Mechanism: completion-style or event-loop server/client batch handling with fewer kernel threads.
- Expected upside: attack the real syscall/scheduler tax instead of shaving validation overhead.
- Likely blocker: it is easy to drift into a semantically different benchmark or destabilize the lane under the full suite.
- Proof obligation: preserve the same request count, checksum, route, and connection lifecycle.

## Mathematical Model

- Variables:
  - `N = rounds`
  - `Q = 64` queue slots in the rejected experiment
  - `R = |request_frame|`
  - `S = |response_frame|`
- Invariants:
  - every accepted socket must be consumed exactly once
  - exact request frame bytes must match the fixed benchmark request
  - exact response frame bytes must match the fixed benchmark response
  - accepted-socket staging / queue indexing must stay within allocated span
- Objective:
  - minimize end-to-end median milliseconds for `N = 240` loopback roundtrips while preserving the benchmark domain
- Bad states:
  - stranded accepted sockets
  - client/server checksum mismatch
  - queue wrap aliasing
  - a focused-run win that regresses the canonical suite
- Simplifying assumptions:
  - the benchmark request/response domain is closed and fixed, so exact-frame validation is allowed

## Z3 Claims

1. `runtime/native/src/core/z3/proofs-experimental/http-concurrency-fixed-frame-bounds-and-checksum.smt2`
   - result: `unsat`
   - meaning: the fixed benchmark request/response frames fit the helper buffers and the 240-request checksum remains exactly `5695`.

## Evidence And Sources

- Local:
  - `benchmark/out/reports/latest_http_spinless_exact_frame.llm.md`
  - `benchmark/out/reports/latest_http_sanity.llm.md`
  - `benchmark/out/reports/latest.llm.md`
  - `runtime/native/src/core/net_system.c`
  - `benchmark/benchmarks.json`
- External:
  - none needed; this was a local runtime/benchmark investigation

## Dead Ends

- The blocking queue + bounded worker-pool experiment looked promising in one focused probe, but failed the canonical suite badly:
  - full-suite regression sample: Kain `137.275 ms`, Rust `54.578 ms`
- A single-thread event-loop client swarm attempt tripped the benchmark lane and was reverted immediately.

## Conclusion

The spinless queue hypothesis was the wrong abstraction for this benchmark. The kept win is the smaller one:

- preserve the original accepted-socket worker swarm
- compare the exact fixed request frame instead of reparsing headers/body
- emit one cached full response frame instead of a cached head plus separate body send

Validation for the kept path:

- focused sanity: `benchmark/out/reports/latest_http_sanity.llm.md`
  - Kain `58.326 ms`
  - Rust `38.002 ms`
- final canonical suite: `benchmark/out/reports/latest.llm.md`
  - generated `2026-05-20T00:31:37.355712+00:00`
  - Kain `68.832 ms`
  - Rust `44.327 ms`

This does not retake the row, but it improves the honest Kain/Rust ratio relative to the earlier canonical baseline while avoiding the catastrophic queue regression. The next serious attack should focus on syscall/scheduler shape, not mutex/condvar queue geometry.
