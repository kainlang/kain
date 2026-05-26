# Compiler And Build Feedback Handoff

This note captures the current state of Kain compiler/build feedback after a
real smoketest probe. The goal is to give the next agent an honest starting
point for improving progress reporting without inventing fake percentages or
phase text the lower layers cannot prove.

## What We Ran

### Canonical smoketest workspace build

From `X:\smoketest`:

```powershell
X:\target\debug\kain.exe build --color never
```

Observed terminal behavior:

- the terminal stayed quiet for roughly 62 seconds
- then emitted one final line:

```text
Build failed: command failed: blade build failed; report written to \\?\X:\smoketest\.kain\reports\build\session-1779744656905-19796.json
```

The corresponding report showed a single task:

- `kain-compile:smoketest:llvm`

And the failure payload was:

```text
Kain error: Codegen error at Span { start: 443493, end: 443516 }: while compiling 'gpu_zero_bytes': Method push not found on type i8*
```

The event stream file existed:

- `X:\smoketest\.kain\reports\build\session-1779744656905-19796.jsonl`

But only contained the final task result, not live user-facing terminal output.

### Directory-wide check pass

From `X:\`:

```powershell
X:\target\debug\kain.exe check X:\smoketest --target llvm --color never
```

Observed terminal behavior:

- the terminal stayed quiet for roughly 150 seconds
- then emitted one final summary:

```text
Check failed: 73/75 passed
```

- then printed the two failing file diagnostics:
  - `X:\smoketest\telemetry\python_bridge.kn`
  - `X:\smoketest\telemetry\run_smoketest_mode.kn`

Both failures were rooted in:

```text
Unknown identifier 'py_bridge_exec'
```

## Current Truth

Kain does not currently provide satisfying real-time compiler/build/check
feedback in the terminal for these lanes.

### `kain build`

Today the user mainly sees:

- silence while the build runs
- one final success/failure line
- optional post-hoc report inspection

There is no live terminal feedback like:

- `Planning build graph`
- `Compiling smoketest (llvm)`
- `Running source-tests`
- `Building root-executable`
- `Certifying smoketest.local`

There is also no task counter shown during execution.

### `kain check`

Today the user mainly sees:

- silence while discovery and checking run
- one final summary such as `73/75 passed`
- failing file diagnostics after the work is already done

There is no live file counter like:

- `Checking 12/75: src/stdlib/fs_lane.kn`

## Important Architectural Finding

This is not a case where the lower layers know nothing.

### Build lane already has structured task truth

`crates/kain-build/src/workspace.rs` already records structured build execution:

- `BladeBuildReport`
- `BuildTaskExecution`
- `report_path`
- `events_path`
- per-task `started_unix_ms`
- per-task `finished_unix_ms`
- per-task status such as `planned`, `cached`, `succeeded`, `failed`, `skipped`

Relevant implementation area:

- `execute_plan(...)`
- `execute_task(...)`

The build system already writes JSON and JSONL artifacts. The missing piece is
primarily that the CLI is not surfacing task progress live in the terminal while
the build is running.

### Check lane already knows the full file set

`crates/kain-check/src/lib.rs` already:

- discovers all `.kn` and `.ks` files first
- loops through them one by one
- builds a `CheckReport`

Relevant implementation area:

- `check_path(...)`
- `discover_kain_files(...)`

That means `kain check` is the easiest honest place to add:

- current file index
- total file count
- current file path

without guessing.

## Why `smoketest` Matters Here

`smoketest/build.kn` is a real album-style workspace build graph, not a toy
single-file compile. It is a strong dogfood surface for progress feedback
because it naturally wants:

- graph planning feedback
- task-level progress
- target/phase labeling
- eventual certify/evidence reporting

It also exposes an important nuance:

- the current build path collapsed into a single compile task
- so on the build lane, the most truthful progress unit may be the build task
  or compiler phase, not necessarily every source file

By contrast, `kain check` really is file-by-file, so file counters are a
natural fit there first.

## Best Implementation Order

### 1. Add live file progress to `kain check`

Recommended first because it is the cleanest honest win.

Desired shape:

- `Checking 1/75: src/main.kn`
- `Checking 2/75: src/semantics/types.kn`
- ...

Good place to start:

- add a progress callback or sink to `kain-check`
- have the CLI print lightweight live updates when not in `--json` mode

### 2. Add live task progress to `kain build`

Use the build graph truth that already exists.

Desired shape:

- `Planning build graph`
- `Task 1/8: check-llvm`
- `Task 2/8: check-spirv`
- `Task 3/8: source-tests`
- `Task 4/8: root-executable`

Or slightly richer:

- `Compiling smoketest (llvm)`
- `Checking src/gpu/fragment.kn (spirv)`
- `Running source-tests`
- `Building native executable`

This should come from real task metadata, not synthetic percent bars.

### 3. Add richer compiler phase text only where truthful

Possible later layer:

- `Parsing`
- `Typechecking`
- `Codegen`
- `Writing artifacts`
- `Collecting diagnostics`
- `Linking`
- `Running`

But only add these if the compiler/build lane can expose them honestly.
Do not hardcode fake phase text just because other toolchains do.

## Where The CLI Currently Prints Final Summaries

These are useful anchors for the next agent:

- `crates/cli/src/kain_launcher.rs`
  - `run_check_command(...)`
  - `print_kain_build_report(...)`
- `crates/cli/src/blades.rs`
  - `print_build_report(...)`

Right now those surfaces mostly print after the work returns. They are good
places to thread in a live progress reporter once the lower-layer callbacks or
event readers exist.

## Recommendation

Treat this as a polish feature with real UX value, not fluff.

The REPL now has immediate queued-run feedback, which makes the lack of
comparable terminal compiler feedback more noticeable. The clean move is to
unify that feel across:

- REPL run status
- `kain check`
- `kain build`
- eventually `kain run`

If only one lane is implemented first, pick `kain check`. It has the best
signal-to-effort ratio and creates the terminal language for the broader build
progress system that can follow.
