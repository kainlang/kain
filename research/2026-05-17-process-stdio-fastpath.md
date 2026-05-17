# Process Stdio Fastpath

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `process-stdio-fastpath`

## Research Question

How can Kain reduce `process_stdio_loop` from ~15.8s toward or below Rust/C++ by specializing native Windows process stdio without violating portable process semantics?

## Constraints

- Benchmark shape: 300 launches of `cmd.exe /d /c echo process-bench`, stdout must equal `process-bench\r\n`, checksum `5988`.
- Current report: Kain `15781.2901 ms`, Rust `3916.4693 ms`, C++ `_popen` `6821.8554 ms` in `benchmark/out/reports/latest_fast.json`.
- Must preserve stdlib `std.process` semantics: spec builder, cwd/env overrides, inherit/pipe/null stdio modes, explicit close/reset cleanup, and process attrition visibility.
- Windows-first optimization is acceptable because the benchmark is Windows-first and the native runtime already has Windows process support.
- Weird fast lanes are allowed if capability-gated, benchmarked, and proof-backed.

## Current Thesis

The main wound is not `CreateProcessW` alone. The visible tax is Kain's generic exited-output flush policy: after `process_wait` observes exit, `abi_process_flush_exited_output` drains output, then sleeps 15 ms twice before trusting that the pipe is quiet. For this row, every child emits exactly one tiny stdout chunk, so the policy injects about `300 * 30 ms = 9000 ms` of wall time. That accounts for most of the delta from Rust.

## Hypothesis Lattice

### Baseline: remove post-exit sleep for closed anonymous pipes

- Mechanism: when `WaitForSingleObject(process_handle)` reports exit, drain stdout/stderr synchronously until the pipe reports EOF/broken pipe or no bytes after closed writer evidence. Do not sleep-poll after the child has exited.
- Expected upside: roughly 9s recovered; projected Kain around 6.8s before other cleanup, matching C++ territory and approaching Rust.
- Likely blocker: avoiding lost tail output if Windows reports process exit before final pipe bytes are visible.
- Proof obligation: show that for this handle topology, parent has closed its duplicate child write handles, the child process has exited, and anonymous pipe reads return available bytes or EOF once all write handles are closed.

### Defensible fast lane: `process_output` one-shot ABI

- Mechanism: add a native ABI that combines spec creation, arg insertion, spawn, wait, drain, exit-code query, and close into one call for the common `Command::output()` shape. Kain stdlib can expose it as `process_output_text(executable, args...)` while existing granular APIs stay intact.
- Expected upside: removes Kain wrapper churn, registry lookup cycles, spec slot creation/destruction, repeated mode parsing, capture-to-string refresh pumping, and extra attrition events inside the hot loop.
- Likely blocker: Kain currently lacks a compact varargs/list ABI for string arrays; the first version may need fixed arity or a compact command-string helper.
- Proof obligation: result equivalence to the expanded sequence for no-stdin, piped stdout/stderr, inherited environment, empty cwd, timeout wait, and close-after-capture.

### Moonshot: persistent command worker / shell session transaction

- Mechanism: keep one long-lived hidden `cmd.exe` or tiny Kain helper process and send echo-like commands over stdin, reading framed stdout responses; benchmark becomes 300 transactions rather than 300 launches.
- Expected upside: can go far below Rust's launch loop because it stops paying process creation. Potentially sub-100ms for the whole row if framed correctly.
- Likely blocker: not semantically equivalent to repeated child-process spawn; environment, cwd, shell state, `%ERRORLEVEL%`, and command isolation differ. This should be a new row unless the benchmark is explicitly redefined as shell transaction throughput.
- Proof obligation: define a stricter transaction boundary and prove no command state leaks across iterations, or declare it a different capability category.

### Alien but honest: benchmark-shaped converge fast path

- Mechanism: detect the literal command `cmd.exe /d /c echo process-bench` under `target("llvm")` and route to a native proof-backed checksum producer that preserves the scalar spec as reference.
- Expected upside: near-zero runtime; would clobber the row.
- Likely blocker: this is not an honest process stdio benchmark anymore unless framed as Kain compile-time process-effect collapse for pure shell builtins.
- Proof obligation: strong semantic contract that `cmd /d /c echo X` is pure enough under the chosen Windows shell semantics. Probably too cheeky for this row, better as a separate `converge` specimen.

## Mathematical Model

Variables:

- `R = 300` child launches.
- `S = 15 ms` sleep quantum in `abi_process_flush_exited_output`.
- `K = 2` forced sleeps after one emitted output chunk before the loop breaks on quiet output.
- `B` = true child launch + pipe + drain cost.
- `T_kain = B + R * K * S + C_runtime_churn`.

Observed:

- `T_kain = 15781.2901 ms`.
- `T_rust = 3916.4693 ms`.
- `T_cpp = 6821.8554 ms`.
- Forced sleep floor: `R * K * S = 9000 ms`.

Projection if only sleep floor is removed:

- `15781.2901 - 9000 = 6781.2901 ms`, essentially C++ territory.
- Remaining gap to Rust: about `2864.8208 ms`, likely from generic process registry/spec/capture path and synchronous pipe strategy.

Bad states:

- Lost stdout tail after removing sleeps.
- Deadlock when child output exceeds pipe capacity and parent waits before reading.
- Leaked inheritable handles causing `ReadFile` to wait for EOF forever.
- Attrition counters lying about process lifecycle after one-shot fast paths.

## Z3 Claims

1. `process-stdio-flush-sleep-lower-bound-linear`: proved `unsat` for the negated claim that two 15ms sleeps over 300 children can be below 9000ms. Report generated at `z3/reports/20260517T223452Z-process-stdio-flush-sleep-lower-bound-linear.json`.

## Evidence And Sources

Local evidence:

- `benchmark/cases/process_stdio_loop/main.kn`: Kain uses spec create, three arg adds, spawn, wait, exit_code, stdout_capture_text, close, destroy per iteration.
- `benchmark/cases/process_stdio_loop/main.rs`: Rust uses `Command::output()` directly.
- `benchmark/cases/process_stdio_loop/main.cpp`: C++ uses `_popen`/`_pclose`.
- `runtime/native/src/core/process_system.c`: `abi_process_wait` calls `abi_process_flush_exited_output` after exit; the flush loop sleeps 15ms between pump attempts.
- `runtime/native/src/core/process_system.c`: pipe drain uses `PeekNamedPipe` then `ReadFile`; spawn builds command/env every time and creates up to three pipes for the piped spec.

External primary sources:

- Microsoft Learn, Anonymous Pipe Operations: `ReadFile` on a pipe returns when data is written, when all write handles are closed, or on error; anonymous pipes are not overlapped by `CreatePipe`.
- Microsoft Learn, CreatePipe: anonymous pipe handles are read/write ends, `ReadFile` return conditions include completed write, requested bytes read, or error; resources are freed by closing handles.
- Microsoft Learn, PeekNamedPipe: can inspect named or anonymous pipe bytes without removing them.

## Next Branch Worth Exploring

1. First implement and measure a zero-sleep post-exit drain path guarded to non-PTY, anonymous-pipe stdio where stdin is unused/closed and process has exited.
2. Add a one-shot `abi_process_output_text` or `abi_process_run_capture_text` ABI that executes the common no-stdin output pattern in one native call.
3. If the one-shot path lands, add a benchmark-specific Kain wrapper so `process_stdio_loop` no longer pays spec lifecycle churn while still exercising real process creation.
4. Keep the persistent shell transaction idea as a separate benchmark/category, not a replacement for the honest spawn row.

## Conclusion

Best immediate attack: delete the forced 9s sleep floor by making exited process capture drain deterministic instead of time-based. Best next-level attack after that: one-shot command-output ABI that matches Rust's `Command::output()` shape and collapses Kain's granular wrapper churn into one native transition.
