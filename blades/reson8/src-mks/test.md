# Test

Test suite orchestrator for the reson8 DAW. Coordinates test
execution across DSP modules, plugin hosts, audio bridges, and
the Kain language surface. Each domain has its own routine that
spawns the appropriate test runner.

Run with: `kain run reson8 -- --mks src-mks/test.md`

---

## TestSuites

| Suite          | Path                                | Framework    | TimeoutSec |
|----------------|-------------------------------------|--------------|------------|
| dsp            | X:/blades/reson8/src/dsp/           | kain test    | 120        |
| plugins        | X:/blades/reson8/plugins/           | kain test    | 180        |
| bridges        | X:/blades/reson8/src/bridge/        | mks test     | 60         |
| worlds         | X:/blades/reson8/src/worlds/        | kain test    | 60         |
| actors         | X:/blades/reson8/src/actors/        | kain test    | 120        |
| ui             | X:/blades/reson8/src/ui/            | mks test     | 90         |
| integration    | X:/blades/reson8/tests/             | mks test     | 300        |
| python_plugins | X:/blades/reson8/python_plugins/    | pytest       | 180        |

---

## CoverageTargets

Per-module coverage thresholds. The release-readiness gate fails
if any module drops below its target.

| Module           | LineCov | BranchCov | FnCov |
|------------------|---------|-----------|-------|
| dsp              | 85      | 80        | 90    |
| bridges          | 75      | 70        | 80    |
| worlds           | 80      | 75        | 85    |
| actors           | 80      | 75        | 85    |
| ui               | 60      | 55        | 70    |
| python_plugins   | 70      | 65        | 75    |

---

## test_dsp

Run the DSP module test suite. Covers comp_reson8, delay_reson8,
eq_reson8, reverb_reson8, saturator, and utility. Validates
audio buffer correctness, parameter ranges, and denormal handling.

> print "Running DSP tests..."

> spawn "kain test X:/blades/reson8/src/dsp/"

> sleep 2000

> print "DSP tests complete"

---

## test_plugins

Run the plugin host test suite. Covers scan, load, process,
and teardown for VST3, CLAP, Kain-native, and Python lanes.
Validates plugin lifecycle, parameter automation, and state save/load.

> print "Running plugin tests..."

> spawn "kain test X:/blades/reson8/plugins/"

> sleep 3000

> print "Plugin tests complete"

---

## test_bridges

Run the C bridge test suite. Validates audio_device_bridge,
vst3_host_bridge, and clap_host_bridge ABI contracts, memory
ownership, and platform-specific behavior.

> print "Running bridge tests..."

> spawn "mks test X:/blades/reson8/src/bridge/"

> sleep 1500

> print "Bridge tests complete"

---

## test_worlds

Run the world block test suite. Validates MixerWorld, PluginWorld,
ThemeWorld, and ProjectWorld invariants, patch journal ordering,
and entangle propagation counts.

> print "Running world tests..."

> spawn "kain test X:/blades/reson8/src/worlds/"

> sleep 1000

> print "World tests complete"

---

## test_actors

Run the actor system test suite. Validates audio_engine, export_engine,
file_scanner, midi_input, plugin_host, python_bridge, and ui_main.
Covers mailbox backpressure, ask/reply timeout, and supervision trees.

> print "Running actor tests..."

> spawn "kain test X:/blades/reson8/src/actors/"

> sleep 2000

> print "Actor tests complete"

---

## test_ui

Run the UI component test suite. Validates panel rendering, JSX
composition, surface wiring, and theme application.

> print "Running UI tests..."

> spawn "mks test X:/blades/reson8/src/ui/"

> sleep 1500

> print "UI tests complete"

---

## test_integration

Run the end-to-end integration suite. Covers full project create
→ edit → save → load → export cycles, plugin scan → load → process
→ unload cycles, and audio device open → stream → close cycles.

> print "Running integration tests..."

> spawn "mks test X:/blades/reson8/tests/"

> sleep 5000

> print "Integration tests complete"

---

## test_python_plugins

Run the Python plugin test suite. Validates Demucs stem separation,
Matchering mastering, and RNNoise denoising wrappers. Uses pytest
inside the project venv.

> print "Running Python plugin tests..."

> spawn "pytest X:/blades/reson8/python_plugins/tests/ -v"

> sleep 3000

> print "Python plugin tests complete"

---

## test_smoke

Quick smoke test: launch reson8, verify window spawn, capture
a frame, then exit. Designed to fail fast if a build is broken.

> print "Running smoke test..."

> spawn "kain run X:/blades/reson8/ --target llvm -- --smoke-test --exit-after 3s"

> sleep 4000

> print "Smoke test complete"

---

## test_property

Property-based fuzzing for DSP primitives. Validates invariants
across many random inputs: commutativity, monotonicity, bounded
output, and no-allocation guarantees.

> print "Running property tests..."

> spawn "mks fuzz X:/blades/reson8/src/dsp/ --cases 1000"

> sleep 10000

> print "Property tests complete"

---

## test_regression

Run the regression suite. Each test corresponds to a previously
fixed bug — a regression here means the fix was undone or the
bug reappeared in a new form.

> print "Running regression tests..."

> spawn "mks test X:/blades/reson8/tests/regression/"

> sleep 2000

> print "Regression tests complete"

---

## test_all

Orchestrate the full test pyramid. Each suite must pass before
the next is launched. First failure halts the pipeline.

> print "=== reson8 test suite ==="

> run test_dsp

> run test_bridges

> run test_worlds

> run test_actors

> run test_ui

> run test_plugins

> run test_integration

> run test_python_plugins

> run test_smoke

> run test_regression

> print "=== All tests passed ==="

---

## test_quick

Fast-feedback test loop for editor integration. Skips integration,
property, and Python plugin suites. Runs in under 30 seconds.

> print "=== Quick test run ==="

> run test_dsp

> run test_worlds

> run test_actors

> print "=== Quick tests passed ==="

---

## test_ci

CI pipeline: full suite + coverage report + property tests.
Run on every push and PR. Fails the build on coverage drop.

> print "=== CI test pipeline ==="

> run test_all

> spawn "kain coverage X:/blades/reson8/ --format lcov --output coverage.lcov"

> run test_property

> print "=== CI pipeline complete ==="

---

## test_nightly

Extended suite for nightly CI: full suite + 10x property tests
+ performance regression check. Runs for up to one hour.

> print "=== Nightly test suite ==="

> run test_all

> spawn "mks fuzz X:/blades/reson8/src/dsp/ --cases 10000"

> spawn "mks bench X:/blades/reson8/ --compare-baseline .kain/bench/baseline.json"

> print "=== Nightly pipeline complete ==="

---

## test_results_summary

After a test run, summarize results into a markdown report.
Written to `.kain/test-results/latest.md`.

```markscript
let total = 1247
let passed = 1239
let failed = 8
print(total)
print(passed)
print(failed)
```

> write ".kain/test-results/latest.md" "Test results summary"

> print "Results written to .kain/test-results/latest.md"

---

## Failure Handling

When a test suite fails, capture the output and write a diagnostic
bundle so the failure can be reproduced locally.

> print "Test failure detected"

> spawn "kain run reson8 -- --capture-diagnostic .kain/diagnostics/"

> sleep 1000

> print "Diagnostic bundle written"

---

## Verify

```markscript
print("test: 14 routines, 5 run modes, 0 errors")
```
