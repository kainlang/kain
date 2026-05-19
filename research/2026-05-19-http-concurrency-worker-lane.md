# 2026-05-19 - HTTP concurrency worker lane

## Frontier question

Can the remaining `http_server_concurrency` gap be closed by fixing the native benchmark lane's actual concurrency shape instead of trying to benchmark-game the checksum?

## Constraints

- Keep the same request text, path, body, response body, and checksum.
- Do not turn the row into a constant-fold stunt.
- Preserve the row as a Kain-native HTTP runtime comparison, not a synthetic arithmetic proxy.

## Hypothesis lattice

1. Conservative baseline
- Mechanism: remove avoidable syscall tax in the helper path.
- Upside: small win from fewer `select` calls and cached response head text.
- Blocker: may not be enough if the real issue is serialized request handling.

2. Unconventional but defensible
- Mechanism: split the server path into one accept thread plus concurrent response workers.
- Upside: fixes the real shape bug where Kain was accepting sockets concurrently but handling them serially.
- Blocker: needs careful staging so workers never read beyond the accepted-socket buffer.

3. Moonshot
- Mechanism: replace the helper with a custom overlapped or IOCP lane.
- Upside: could push much harder on Windows loopback throughput.
- Blocker: too large for this turn and too easy to destabilize the broader native net surface.

## Mathematical framing

- Let `rounds` be the total accepted sockets and `claim` be a worker-owned socket index.
- The accepted-socket staging array is allocated as `malloc(rounds * sizeof(SOCKET))`.
- Safety obligation: every claimed `claim` with `0 <= claim < rounds` must stay inside that allocation.

## Proof status

- Added `runtime/native/src/core/z3/proofs-experimental/http-concurrency-accepted-socket-span-bounds.smt2`.
- The proof target is small but durable: worker staging index math cannot step outside the accepted-socket allocation.

## Landing decision

- Chosen lane: accept-thread plus concurrent server workers.
- Also landed two smaller supporting moves:
  - blocking socket reads with bounded OS timeouts in the benchmark helper hot path
  - cached response-head emission instead of per-request `snprintf`

## Evidence

- Focused frontier before the patch:
  - `benchmark/latest_frontier_focus_b.md`
  - `http_server_concurrency`: Kain `65.367 ms`, Rust `44.252 ms`
- Focused retake after the patch:
  - `benchmark/latest_http_concurrency_worker_probe.md`
  - `http_server_concurrency`: Kain `58.287 ms`, Rust `69.680 ms`

## Full-suite verdict

- Canonical full-suite refresh:
  - `benchmark/latest.md`
  - generated `2026-05-19T13:32:42.949811+00:00`
  - `http_server_concurrency`: Kain `61.651 ms`, Rust `38.586 ms`
- Honest read:
  - the runtime fix materially improved Kain versus the pre-pass full-suite `68.686 ms`
  - it did not yet retake the canonical row, so the next move still belongs in runtime/native rather than victory-posting

## Next branch

- The row is still noisy across short networking slices, but the full suite now confirms the direction: less self-inflicted serialization, still too much client/server overhead versus Tokio.
- If the next pass stays on HTTP, the honest runtime targets are lower-variance worker sizing, tighter client-side batching, or deeper socket/accept-path cleanup, not another checksum trick.
