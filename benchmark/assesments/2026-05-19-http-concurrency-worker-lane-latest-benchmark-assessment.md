# 2026-05-19 HTTP concurrency worker lane assessment

## Why this target

Focused frontier reruns showed that the latest full-suite `http_server_concurrency` loss was real enough to matter and more actionable than the near-noise sim and string gaps:

- before patch, focused probe: Kain `65.367 ms`, Rust `44.252 ms`
- telemetry gap: Kain `3,671.561 req/s`, Rust `5,423.422 req/s`

The hot helper was accepting sockets concurrently but still handling them serially on the server thread, so Kain was carrying a self-inflicted runtime-shape tax.

## What changed

- `runtime/native/src/core/net_system.c`
  - split the benchmark-only server path into one accept thread plus concurrent server workers
  - staged accepted sockets through a worker-readable buffer
  - cached the fixed HTTP response head once per run
  - replaced extra helper-side readiness polling with blocking reads guarded by socket timeouts
- `benchmark/benchmarks.json`
  - updated the Kain language note so the row stays honest about the new worker-staged helper shape
- `runtime/native/src/core/z3/proofs-experimental/http-concurrency-accepted-socket-span-bounds.smt2`
  - captures the accepted-socket staging bound

## Result

Focused retake after the patch:

- `benchmark/latest_http_concurrency_worker_probe.md`
- `http_server_concurrency`: Kain `58.287 ms`, Rust `69.680 ms`
- telemetry: Kain `4,117.570 req/s`, Rust `3,444.322 req/s`

This is about a `10.8%` Kain median improvement against the pre-patch focused baseline and flips the row to a Kain win in the main probe.

Canonical full-suite rerun after the landed change:

- `benchmark/latest.md`
- generated `2026-05-19T13:32:42.949811+00:00`
- `http_server_concurrency`: Kain `61.651 ms`, Rust `38.586 ms`

This means the pass is a real improvement against the pre-pass canonical Kain `68.686 ms`, but it is not yet a durable full-suite row retake.

## Caveat

Short 3-run networking sanity still showed noise:

- `benchmark/latest_http_net_regression_sanity.md`
- `http_server_concurrency`: Kain `68.093 ms`, Rust `55.800 ms`

That run also had one large outlier on both sides, so the real arbiter is the canonical full-suite rerun rather than the 3-sample sanity slice.

Unrelated compute/runtime rows that looked worse in the final suite were retaken in isolation:

- `benchmark/latest_http_runtime_regression_sanity.md`
- `memory_stream`: Kain `9.956 ms`, Rust `10.213 ms`, C++ `10.234 ms`
- `ownership_memory`: Kain `11.135 ms`, Rust `11.772 ms`, C++ `11.599 ms`
- `crypto_block_cipher`: Kain `10.522 ms`, Rust `12.042 ms`, C++ `10.715 ms`
- `ffi_shared_call_stress`: Kain `52.265 ms`, Rust `52.642 ms`, C++ `52.706 ms`

So the HTTP runtime patch does not show evidence of broader compute regression; the remaining honest losses are still `http_server_concurrency` and `process_stdio_loop`.
