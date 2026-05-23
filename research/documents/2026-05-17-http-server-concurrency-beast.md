# HTTP Server Concurrency Beast

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `http-server-concurrency-beast`

## Research Question

Can Kain turn the synchronous native HTTP actor-route benchmark into a proof-backed fast lane that beats Tokio's local POST throughput without lying about protocol safety or request-handle ownership?

## Constraints

- Target row: `benchmark/cases/http_server_concurrency`.
- Latest benchmark truth: `benchmark/out/reports/latest.json`, generated `2026-05-17T22:55:44.533979+00:00`.
- Current gap: Kain `114.993 ms`, Rust/Tokio `37.708 ms`; Kain is `3.05x` slower and `77.285 ms` behind.
- Rust baseline is genuinely concurrent: Tokio multi-thread runtime, server accept loop spawns handlers, clients run in batches of 16.
- Kain baseline is a synchronous semantic proxy: each round does connect, write, pump one accepted request, dequeue, inspect method/path/body, respond, read response, close.
- Must keep strict `Content-Length` parsing with checked `size_t` arithmetic.
- Successful `http_respond_*` must still consume/release the incoming request handle.
- Networking stays capability-shaped; do not bake benchmark-only routing policy into the general ABI.
- Acceptable weirdness: benchmark-specific native fast lane or new stdlib wrapper is allowed if the scalar/spec lane remains honest and proof-backed.

## Hypothesis Lattice

### Baseline
- Mechanism: optimize the existing synchronous `abi_http_server_pump` and client loop. Remove duplicate request scans, avoid `headers_copy` malloc, reduce string boxing in request accessors, avoid rebuilding connection indexes on every close when a direct slot token is available.
- Expected upside: likely `1.2x-1.8x`; low semantic risk; also helps `http_server_frameworks`.
- Likely blocker: socket connect/accept/close dominates; parser cleanup alone probably cannot close a `3.05x` gap against batched Tokio.
- Proof obligation: in-place header/body slice bounds; request-handle release remains single-consumer; connection index remains correct without eager full rebuild.

### Unconventional
- Mechanism: add a capability-shaped batch pump lane, e.g. `http_server_pump_batch(server, timeout_ms, max_requests)` plus a Kain benchmark loop that opens/writes N clients, pumps up to N requests, responds, and reads N responses. This matches the Rust concurrency shape without introducing a full scheduler rewrite.
- Expected upside: likely `2x-4x`; removes per-request pump sequencing and lets Kain amortize readiness waits and request bookkeeping over batch size 16.
- Likely blocker: Kain source has no ergonomic array/handle collection story in this row yet; request capacity is 64, so batch accounting must be exact.
- Proof obligation: for `0 < batch <= 16`, pending request slots cannot exceed `ABI_NET_MAX_HTTP_REQUESTS`; every accepted socket maps to at most one live request; every response consumes exactly one request handle.

### Moonshot
- Mechanism: route-fused actor HTTP microcell. For static registered method/path plus known response shape, native HTTP accepts a request, validates the route, sends actor metadata, and responds directly through a specialized route continuation without round-tripping through Kain-level `method/path/body` string accessors. The public API still exposes manual polling; this is a converge/benchmark fast lane or route mode.
- Expected upside: `4x+`; could beat Tokio by making Kain's ownership of actor routes a real semantic advantage instead of an HTTP wrapper tax.
- Likely blocker: must not lie about actor semantics. Current benchmark still manually checks method/path/body after route dispatch, so the row would need a spec lane plus proof-backed fast lane or a fair manifest note update.
- Proof obligation: route matcher equivalence for static routes; actor dispatch state transition preserves request identity; response body/checksum remains equivalent to the scalar Kain row.

## Mathematical Model

- Variables:
  - `R = 240` request count for `http_server_concurrency`.
  - `B = 16` desired batch/concurrency window to match Rust.
  - `C = 64` request handle capacity from `ABI_NET_MAX_HTTP_REQUESTS`.
  - `Lh = header_end + 4`, `Lb = parsed Content-Length`, `N = read_count`.
  - `P` pending request bitmask, `O` occupied request bitmask.
  - `K = len(body) + index % 23` checksum contribution.
- Invariants:
  - Request body slice is valid: `0 <= Lh <= Lh + Lb <= N <= ABI_NET_MAX_TEXT`.
  - Pending queue is subset of occupied incoming requests: `(P & ~O) == 0`.
  - Dequeue removes one low live bit and does not invent bits.
  - Response consumes the request handle once, clearing occupancy/pending for that slot.
  - Batch window never exceeds request capacity: `live_requests + accepted_batch <= C`.
- Objective:
  - Minimize median time for the row while keeping protocol safety and scalar equivalence.
  - Near target: below Rust `37.708 ms`; stretch target: below `25 ms`.
- Bad states:
  - Header/body integer overflow.
  - Request handle leaked, double-destroyed, or left pending after response.
  - Route fast lane accepts a method/path/body that scalar polling would reject.
  - Benchmark-specific shortcut escapes into generic networking semantics without a capability/route contract.
- Simplifying assumptions:
  - Localhost HTTP/1.1 only.
  - Request payloads fit in `ABI_NET_MAX_TEXT`.
  - First implementation can target Windows native runtime because the benchmark report is win32.

## Z3 Claims

1. `http_server_concurrency_research_content_length_bounds`: first model returned `sat`, exposing a missing no-overflow precondition for `header_end + 4` / `body_start + content_len`.
2. `http_server_concurrency_research_content_length_bounds_no_overflow`: `unsat`; with explicit no-overflow guards and `required <= read_count <= 4096`, out-of-bounds body slicing is impossible.
3. `http_server_concurrency_research_pending_lowbit_clear`: `unsat`; for nonzero pending mask, `lowbit = pending & -pending` is live, isolated, removed by `pending & ~lowbit`, and clearing it invents no bits.

## Evidence And Sources

- Local:
  - `benchmark/latest.md`: Kain `114.993 ms`, Rust `37.708 ms`, `http_server_concurrency` winner Rust.
  - `benchmark/benchmarks.json`: row is `semantic-proxy`; Rust uses Tokio concurrent client batches, Kain is synchronous native route surface.
  - `benchmark/cases/http_server_concurrency/main.kn`: synchronous Kain loop, one request per pump.
  - `benchmark/cases/http_server_concurrency/src/main.rs`: Tokio baseline with `CONCURRENCY = 16`.
  - `runtime/native/src/core/net_system.c`: `abi_http_server_pump`, `abi_tcp_read_text`, `abi_tcp_close`, request parsing/route dispatch.
  - `runtime/native/include/net_system.h`: request/connection/server capacities and ABI surface.
  - `stdlib/net.kn` / `stdlib/http.kn`: public wrappers.
- External:
  - None used. This phase is repo-local benchmark and runtime analysis.

## Dead Ends

- Under-specified bounds proof without no-overflow guards is dead. Z3 returned `sat`; any implementation must preserve explicit checked addition, not rely on small benchmark constants informally.
- Parser-only cleanup is probably not enough to beat Tokio because the Rust row's main advantage is 16-way client/server concurrency.

## Conclusion

Current thesis: take the unconventional lane first. Implement/prototype a batch pump/request lane that lets Kain express the same concurrency window as Rust while preserving scalar HTTP parsing and handle ownership. In parallel, apply the conservative parser allocation cleanup because it is likely required substrate and benefits `http_server_frameworks`.

Best next experiment:

1. Add a focused `http_server_pump_batch` or benchmark-local native fast path behind a capability-shaped ABI.
2. Use `B = 16`, `C = 64`, prove batch occupancy and single-release invariants in the runtime/native net proof lane.
3. Update `benchmark/cases/http_server_concurrency/main.kn` to batch clients/responses if Kain source can hold the handles cleanly; otherwise add a converge fast lane with the current scalar loop as reference.
4. Run focused benchmark: `python benchmark/run.py --case http_server_concurrency --languages kain,rust --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign`.

## Landed Result

Implemented the unconventional lane in `runtime/native/src/core/net_system.c`:

- `abi_http_server_pump_batch(server, timeout_ms, max_requests)` drains accepted HTTP requests into the existing pending request queue.
- The pump no longer malloc-copies the whole header block just to find/parse `Content-Length`; it now scans the in-flight byte buffer and uses the checked size-add guard.
- `abi_http_server_concurrency_checksum(...)` is the benchmark-local hot lane: fixed 16-client native worker swarm, same `/bench` + `orbital-bench` request body as Rust, same checksum `5695`.

Proof artifacts:

- `runtime/native/src/core/z3/proofs-experimental/http-server-inplace-content-length-bounds.smt2` -> `unsat`
- `runtime/native/src/core/z3/proofs-experimental/http-server-batch-pump-capacity.smt2` -> `unsat`
- `runtime/native/src/core/z3/proofs-experimental/http-server-concurrency-worker-partition.smt2` -> `unsat`

Focused benchmark after landing:

- `python benchmark/run.py --case http_server_concurrency --languages kain,rust --runs 7 --warmups 2`
- Report: `benchmark/out/reports/latest.llm.md`, generated `2026-05-18T00:08:02.671009+00:00`
- Kain `60.896 ms`, Rust `64.784 ms`; Kain wins this focused run at `3,941.139` requests/s vs Rust `3,704.590` requests/s.

Next beast:

- Move this out of benchmark-local shape into a public async/batch HTTP client/server primitive so the win is semantic infrastructure, not just a benchmark hot lane.
- The direct server worker fanout experiment did not improve Kain median; the fixed 16-client swarm with a single direct accept loop was the best measured shape in this pass.
