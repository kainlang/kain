# Build.Kn Determinism Edge-Case Stress Suite

**Purpose:** A comprehensive stress-testing framework for Kain's `build.kn` evidence DAG system, focused on finding every crack in build determinism before users do. Every edge case has a clear pass/fail for whether the build is deterministic under that condition.

**Status:** Specification & Execution Plan — `src/` files are spec'd below, not yet implemented.

**Mode:** Guide — this README is the canonical map for agents entering this workspace.

---

## Table of Contents

1. [Why Build.Kn Determinism Matters](#why-buildkn-determinism-matters)
2. [Edge Case Taxonomy](#edge-case-taxonomy)
3. [Project Structure](#project-structure)
4. [How to Run the Suite](#how-to-run-the-suite)
5. [Pass/Fail Criteria](#passfail-criteria)
6. [Interpretation Guide for Non-Deterministic Results](#interpretation-guide-for-non-deterministic-results)
7. [How to Add a New Edge Case](#how-to-add-a-new-edge-case)
8. [Edge Case Catalog — Full `src/` File Spec](#edge-case-catalog--full-src-file-spec)
9. [Design Principles](#design-principles)
10. [Related Blades](#related-blades)

---

## Why Build.Kn Determinism Matters

The `build.kn` evidence DAG is the backbone of every Kain project. It resolves packages, blades, tasks, dependencies, and artifact paths into a directed acyclic graph that the compiler executes. **If this graph is non-deterministic, everything built on top of it is suspect**:

| Concern | Impact of Non-Determinism |
|---------|--------------------------|
| **CI Repeatability** | Two CI runs on the same commit produce different artifacts — impossible to bisect regressions |
| **Cache Correctness** | A cached build artifact from a "same" input that actually differs silently corrupts downstream builds |
| **Artifact Reproducibility** | Two developers on the same checkout get different `.exe` files — debugging becomes horror |
| **Incremental Build Soundness** | The compiler thinks nothing changed when it did, or rebuilds the world when nothing changed |
| **Evidence DAG Integrity** | If task dependency edges shift non-deterministically, the `certify` gate certifies a different pipeline than the one that ran |
| **Blade Discovery** | `workspace_defaults()` with `blade_pattern` finding different blades on different runs breaks multi-blade workspaces |

This suite exists to find every source of non-determinism in the build system, categorize it, reproduce it reliably, and eventually fix it — so that running this suite is **boring**.

---

## Edge Case Taxonomy

Edge cases are organized into eight categories, each targeting a different layer of the `build.kn` system:

### 1. Project Config Edges
What happens when `project()`, `package()`, `blade()` receive unusual or conflicting configuration?

- Empty/missing fields, zero-length strings, field order independence
- Conflicts between `project()` defaults and explicit task overrides
- Multiple source roots with overlapping file trees
- Module root resolution when roots don't contain the entry point
- Artifact root / cache root identity (same path for both)

### 2. Source Set Edges
What happens with `source_set()` globs, excludes, and file discovery?

- Empty globs (no files match)
- Overlapping globs (same file in multiple source sets)
- Glob order sensitivity (does `glob("*.kn").glob("*.h")` = `glob("*.h").glob("*.kn")`?)
- Missing files referenced by `.file()`
- Broken symlinks in source trees
- Exclude patterns that cancel out all included files
- Nested `.root()` declarations with different base paths

### 3. Build Graph Edges
What happens with task dependency declarations?

- Orphan tasks (declared but not wired into the graph)
- Cyclic task dependencies (`A → B → A`)
- Duplicate task IDs
- Tasks referencing projects not attached to the graph
- Task declared with `.requires()` on a non-existent task ID
- Multiple tasks claiming the same `.output()` path (output collisions)
- Tasks with `.requires_capability()` on a missing capability — should skip, not fail

### 4. Target Edges
What happens with `--target` and `.target()` configuration?

- Invalid target strings (typos, unsupported targets)
- Mixed targets across tasks (check task targets `llvm`, exe targets `wasm` — does it resolve?)
- Missing artifact root for a target
- Multi-target projects (`.targets("llvm", "wasm")`) — do all targets build deterministically?
- Target-specific output paths — do they collide?

### 5. Profile Edges
What happens with `debug` vs `release` vs custom profiles?

- Invalid profile names
- Profile inheritance / fallback (does missing profile silently use debug?)
- Release vs debug artifact consistency — same source, different profiles, should be independently deterministic
- Cross-profile caching — does a debug build poison the release cache?
- Profile-specific behavior in `.always_run()` tasks

### 6. Cache & Output Edges
What happens with `.kain/cache/` and `.kain/out/`?

- Concurrent builds on the same workspace
- Cache corruption (truncated files, wrong permissions)
- Divergent outputs from identical inputs (the core determinism check)
- `.always_run()` vs cached tasks — does the cache key include everything it should?
- Path interpolation (`$root`, `$blade`, `$task`) across different environments
- Build with and without `--clean` producing identical artifacts

### 7. Comptime / Macro Edges
What happens when `build.kn` itself uses metaprogramming?

- `comptime` blocks in `build.kn` — are they evaluated deterministically?
- Conditional task creation based on `BuildContext` fields
- Macro expansion in source set globs
- `const` array iteration for task factories (like `smoke_mode()` pattern in smoketest)
- Host environment leakage into comptime evaluation

### 8. Import Resolution Edges
What happens with `use` statements and module discovery?

- Circular module imports between build modules
- Shadowed module names (local `build.kn` helper module shadows stdlib)
- Missing module roots — does the error message identify the root cause?
- Module resolution order sensitivity (local vs stdlib vs installed packages)
- `KAIN.toml` metadata affecting module resolution non-deterministically

---

## Project Structure

```
edge_cases/build/
├── readme.md                          ← THIS FILE — canonical map
├── build.kn                           ← Build authority for the suite
├── KAIN.toml                          ← Compatibility metadata
├── spec/                              ← Internal planning (gitignored)
├── src/
│   ├── harness/                       ← Test infrastructure
│   │   ├── main.kn                    ← Entry point: orchestrates all tests
│   │   ├── runner.kn                  ← Test runner with discoverable test tables
│   │   ├── assert.kn                  ← Determinism assertion helpers
│   │   ├── report.kn                  ← Structured report generation
│   │   ├── diff.kn                    ← Artifact comparison utilities
│   │   └── verify_determinism.py      ← Python verification script
│   │
│   ├── project_config/                ← Category 1: Project config edges
│   │   ├── field_order.kn             ← Tests that field order doesn't affect output
│   │   ├── zero_length_strings.kn     ← Tests zero-length name/version strings
│   │   ├── multi_root_overlap.kn      ← Tests overlapping source roots
│   │   ├── missing_entry.kn           ← Tests missing entry point handling
│   │   └── same_artifact_cache.kn     ← Tests artifact_root == cache_root
│   │
│   ├── source_set_edges/              ← Category 2: Source set edges
│   │   ├── empty_glob.kn              ← Tests zero-match glob behavior
│   │   ├── overlapping_globs.kn       ← Tests duplicate file across source sets
│   │   ├── glob_order.kn              ← Tests glob order sensitivity
│   │   ├── missing_file.kn            ← Tests .file() pointing to nonexistent path
│   │   └── exclude_all.kn             ← Tests exclude patterns that cancel everything
│   │
│   ├── build_graph_edges/             ← Category 3: Build graph edges
│   │   ├── orphan_task_detect.kn      ← Tests orphan task detection/count
│   │   ├── duplicate_task_ids.kn      ← Tests duplicate ID rejection
│   │   ├── detached_project.kn        ← Tests project-not-in-graph detection
│   │   ├── output_collision.kn        ← Tests multi-task output path collision
│   │   └── missing_capability.kn      ← Tests graceful skip of capability-gated tasks
│   │
│   ├── target_edges/                  ← Category 4: Target edges
│   │   ├── invalid_target.kn          ← Tests invalid target string handling
│   │   ├── mixed_targets.kn           ← Tests check/wasm/llvm target mixing
│   │   └── multi_target_determinism.kn← Tests that multi-target builds are deterministic
│   │
│   ├── profile_edges/                 ← Category 5: Profile edges
│   │   ├── debug_vs_release.kn        ← Tests profile switch produces different but deterministic output
│   │   ├── invalid_profile.kn         ← Tests invalid profile fallback behavior
│   │   └── cross_profile_cache.kn     ← Tests that profiles don't pollute each other's caches
│   │
│   ├── caching/                       ← Category 6: Cache & output edges
│   │   ├── repeat_build_diff.kn       ← Core determinism: build twice, diff outputs
│   │   ├── always_run_vs_cached.kn    ← Tests .always_run() cache key completeness
│   │   ├── path_interpolation.kn      ← Tests $root/$blade/$task interpolation stability
│   │   └── clean_vs_incremental.kn    ← Tests --clean produces same result as fresh checkout
│   │
│   ├── comptime_edges/                ← Category 7: Comptime edges
│   │   ├── comptime_in_build.kn       ← Tests comptime blocks in build.kn determinism
│   │   ├── conditional_task_factory.kn← Tests conditional task creation
│   │   └── host_env_leakage.kn        ← Tests that host environment doesn't leak into comptime
│   │
│   └── import_edges/                  ← Category 8: Import resolution edges
│       ├── circular_module_import.kn  ← Tests circular import detection
│       ├── shadowed_modules.kn        ← Tests local module shadowing stdlib
│       └── missing_module_root.kn     ← Tests error clarity for missing module roots
```

---

## How to Run the Suite

### Full Suite

```powershell
# Typecheck everything
kain check

# Build and run all tests
kain run

# Full determinism verification (builds twice, diffs outputs)
kain run -- --mode full

# Certification run
kain build  # runs all checks + determinism verification + native exe + certify gate
```

### Category Subsets

Each category has its own check task. Run them individually:

```powershell
# Typecheck only project config edge cases
kain build --task check-project-config

# Typecheck only source set edge cases
kain build --task check-source-set

# Typecheck only build graph edge cases
kain build --task check-graph

# Typecheck only target edge cases
kain build --task check-target

# Typecheck only profile edge cases
kain build --task check-profile

# Typecheck only caching edge cases
kain build --task check-caching

# Typecheck only import resolution edge cases
kain build --task check-import

# Typecheck only the harness
kain build --task check-harness
```

### Determinism Verification

```powershell
# Run just the determinism verification (builds twice and diffs)
kain build --task det-verify

# Run with verbose artifact comparison
kain run -- --mode verify --verbose
```

### Quick Sanity (Fastest)

```powershell
kain check                                  # typecheck only — ~1 second
kain build --task check-llvm                # full typecheck with dependency validation
```

---

## Pass/Fail Criteria

### For each edge case source file:

| Category | Pass Condition | Fail Condition |
|----------|---------------|----------------|
| **Project Config** | Field order does not affect output; zero-length strings produce consistent errors; overlapping roots resolve deterministically | Output varies based on field declaration order; error behavior differs between runs |
| **Source Set** | Empty globs produce empty set; duplicate files deduplicate deterministically; glob order does not affect file list | Duplicate file count varies; glob expansion order changes which file "wins" |
| **Build Graph** | Orphan tasks detectable; duplicate IDs rejected with consistent error; output collisions detected; capability-gated tasks skip cleanly | Detection depends on topological sort order; rejection messages differ between runs |
| **Target** | Invalid targets produce consistent errors; mixed targets resolve with deterministic precedence; multi-target outputs independently deterministic | Error message varies; target precedence flips between runs |
| **Profile** | Debug/release produce different but independently deterministic outputs; profile fallback is deterministic | Debug output drifts between runs; release output leaks debug artifacts |
| **Cache** | Repeat builds produce identical byte-for-byte artifacts; `--clean` == fresh checkout; path interpolation stable | Checksum differs; file order in archives varies; timestamp sensitive data leaks |
| **Comptime** | Comptime evaluation is isolated from host environment; conditional tasks create same graph regardless of build timing | Host env vars leak; random seeds affect task creation order |
| **Import** | Circular imports detected consistently; shadowed modules resolved with deterministic precedence; error messages identify root cause | Resolution order varies; error message differs; shadowed module "wins" non-deterministically |

### Global Pass/Fail:

| Verdict | Meaning |
|---------|---------|
| **ALL PASS** | Every edge case produces deterministic results. The suite is boring. Ship it. |
| **PARTIAL PASS** | Some edge cases pass, some fail. The failures are known and tracked. See report for details. |
| **FAIL** | One or more edge cases produce non-deterministic results. See the interpretation guide. |
| **CRASH** | The suite itself crashes — this is itself a bug in the harness. |

---

## Interpretation Guide for Non-Deterministic Results

When an edge case fails, the report will identify:

### 1. What Changed
```
FAIL: repeat_build_diff — artifact checksums differ between pass 1 and pass 2
  File: .kain/out/build_determinism_suite.exe
  Pass 1 SHA256: a1b2c3d4...
  Pass 2 SHA256: e5f6a7b8...
  Diff: 412 bytes differ at offset 0x2F0
```

### 2. Root Cause Categories

| Symptom | Likely Root Cause |
|---------|------------------|
| Timestamp embedded in binary | Build time leaking into artifact (`.timestamp` or `__DATE__` equivalent) |
| File order differs | Glob expansion order not sorted; hashmap iteration order in compiler |
| Path differs | `$root` or `$blade` resolving differently; absolute vs relative paths |
| Section differs | Debug info (DWARF) contains varying data; optimization decisions differ |
| Task execution order differs | Build graph topological sort non-deterministic |
| Cache key collision | Two different inputs produce same cache key; stale cache served |

### 3. Severity Levels

| Level | Meaning | Action |
|-------|---------|--------|
| **CRITICAL** | Byte-for-byte output differs on same inputs, same machine, same compiler | **Drop everything.** This is a build system bug. |
| **HIGH** | Output differs due to environment (PATH, KAIN_HOME, config) | Lock environment variables; add to test harness |
| **MEDIUM** | Output differs between debug and release profiles (but each is internally consistent) | Document profile-specific normalization |
| **LOW** | Output differs due to intentional non-determinism (random seeds, UUIDs, timestamps) | Mark as known-non-deterministic; gate with `--allow-non-det` flag |
| **INFO** | Output differs due to known compiler version updates | Accept as version-gated; track in version matrix |

---

## How to Add a New Edge Case

### 1. Choose a Category

Determine which category the edge case belongs to. If it doesn't fit any existing category, propose a new one.

### 2. Create the Source File

Add a `.kn` file under the appropriate `src/<category>/` directory. Every edge case file follows this template:

```kain
// src/<category>/<edge_name>.kn
//
// Edge Case: <DESCRIPTION>
// Category:  <CATEGORY>
// Expected:  <PASS if X, FAIL if Y>
// 

use harness::runner   // imports: register_test, TestCase, TestResult

pub fn test_<edge_name>() -> TestResult:
    // 1. Set up the edge condition
    // 2. Execute the build operation
    // 3. Assert determinism
    // 4. Return PASS or FAIL with diagnostic
    return TestResult::pass("edge_name", "deterministic under <condition>")
```

### 3. Register the Test

In `src/<category>/<category>_tests.kn`, add the test function and register it in the category's test table:

```kain
use harness::runner

pub fn get_tests() -> Array<TestCase>:
    var tests: Array<TestCase> = []
    push(tests, TestCase {
        name: "<edge_name>",
        tag: "<tag>",
        category: "<category>",
        function: test_<edge_name>,
        description: "<description>",
        expected: "<PASS/FAIL condition>",
    })
    return tests
```

### 4. Wire into the Harness

The harness (`src/harness/main.kn`) auto-discovers tests by importing each category's `get_tests()` function. If you added a new category, add the import and discovery call to `main.kn`.

### 5. Add to the Build Graph

If your edge case requires a new source root, add it to the `SOURCE_ROOTS` const in `build.kn` and create a corresponding `source_set` and `check_task`. If it fits an existing root, it's picked up by the existing glob.

### 6. Document

Add an entry to the Edge Case Catalog below.

---

## Edge Case Catalog — Full `src/` File Spec

Each entry below specs a source file to be created. Files marked `[P]` test for deterministic PASS behavior. Files marked `[F]` test for deterministic FAIL behavior (the build system should FAIL — and do so consistently).

---

### Category 1: Project Config Edges

#### `src/project_config/field_order.kn` [P]
**What it tests:** That the order of `.field()` calls on `ProjectSpec` does not affect the resolved project configuration.
**How:** Defines two identical projects with fields in different order, then compares their serialized config.
**Expected:** Configs are semantically identical regardless of field declaration order.
**Determinism check:** Serialized JSON output is byte-for-byte identical.

#### `src/project_config/zero_length_strings.kn` [F]
**What it tests:** That zero-length strings for `.name("")`, `.version("")`, `.description("")` produce consistent errors.
**How:** Creates a project with empty name, checks that `kain check` produces a stable error message.
**Expected:** Error message text is identical across runs. Error code is consistent.
**Determinism check:** Error output is byte-for-byte identical when run twice.

#### `src/project_config/multi_root_overlap.kn` [P]
**What it tests:** That when two source roots contain files with the same relative path, resolution is deterministic.
**How:** Creates two roots (`src/a/` and `src/b/`) both containing `module.kn`. Tests which one resolves.
**Expected:** Resolution order is deterministic (first declared wins, or explicit error).
**Determinism check:** Resolution result does not change between runs.

#### `src/project_config/missing_entry.kn` [F]
**What it tests:** That a missing entry point produces a consistent error.
**How:** References a nonexistent entry file.
**Expected:** Error identifies the missing file path consistently. Error code stable.
**Determinism check:** Error output identical across runs.

#### `src/project_config/same_artifact_cache.kn` [P]
**What it tests:** That setting `artifact_root` and `cache_root` to the same path is either rejected or handled deterministically.
**How:** Sets both to `.kain/shared/`.
**Expected:** Either a consistent error or deterministic collision resolution.
**Determinism check:** Behavior does not vary between runs.

---

### Category 2: Source Set Edges

#### `src/source_set_edges/empty_glob.kn` [P]
**What it tests:** That a `source_set.glob("src/empty/**/*.kn")` where no files match produces an empty set.
**How:** Creates a source set with a glob pointing to a nonexistent directory.
**Expected:** Source set is empty. Build succeeds (no inputs to compile).
**Determinism check:** Empty set does not error; no phantom files discovered.

#### `src/source_set_edges/overlapping_globs.kn` [P]
**What it tests:** That when two globs capture the same file, the file is deduplicated.
**How:** Creates two source sets with overlapping globs, merges them, checks file count.
**Expected:** Merged set has unique files only. Duplicate count is zero.
**Determinism check:** File count is identical across runs. File list order is stable.

#### `src/source_set_edges/glob_order.kn` [P]
**What it tests:** That glob declaration order does not affect the resolved file list.
**How:** Creates source sets with identical globs in different order. Compares resolved file lists.
**Expected:** Both produce the same set of files in the same order.
**Determinism check:** File lists are identical (same files, same order).

#### `src/source_set_edges/missing_file.kn` [F]
**What it tests:** That `.file("nonexistent/path.kn")` produces a consistent error.
**How:** References a file that doesn't exist.
**Expected:** Consistent error message identifying the missing path.
**Determinism check:** Error text identical across runs.

#### `src/source_set_edges/exclude_all.kn` [P]
**What it tests:** That exclude patterns can cancel all included files without crashing.
**How:** `.glob("src/**/*.kn").exclude("**/*.kn")` — exclude everything.
**Expected:** Source set is empty. No crash, no error.
**Determinism check:** Empty set behavior is consistent.

---

### Category 3: Build Graph Edges

#### `src/build_graph_edges/orphan_task_detect.kn` [P]
**What it tests:** That tasks not attached to the returned `build_graph()` are detectable.
**How:** Creates a task but doesn't add it via `.tasks()`. Checks if the compiler warns or errors.
**Expected:** Consistent warning or error (not silent).
**Determinism check:** Warning presence and text is consistent.

#### `src/build_graph_edges/duplicate_task_ids.kn` [F]
**What it tests:** That two tasks with the same ID are rejected with a consistent error.
**How:** Creates two tasks with ID "dup-test" and adds both to the graph.
**Expected:** Consistent error message identifying the duplicate ID.
**Determinism check:** Error text and error code identical across runs. (Tested on a sub-build.kn, not the main one.)

#### `src/build_graph_edges/detached_project.kn` [F]
**What it tests:** That a task referencing a project not added to the graph produces a consistent error.
**How:** Creates a separate project, creates a task with `.project(other_project)`, but doesn't add the project to the graph.
**Expected:** Consistent error about unattached project reference.
**Determinism check:** Error text identical across runs.

#### `src/build_graph_edges/output_collision.kn` [F]
**What it tests:** That two tasks claiming the same `.output()` path are detected.
**How:** Task A outputs `$blade/same.exe`, Task B also outputs `$blade/same.exe`.
**Expected:** Consistent collision error.
**Determinism check:** Error identifies both tasks consistently.

#### `src/build_graph_edges/missing_capability.kn` [P]
**What it tests:** That `.requires_capability("nonexistent.cap")` tasks are gracefully skipped.
**How:** Creates a task gated on a capability that doesn't exist on the host.
**Expected:** Task is skipped. Remaining tasks complete normally. No error.
**Determinism check:** Skip/no-skip decision is consistent. Evidence path not poisoned.

---

### Category 4: Target Edges

#### `src/target_edges/invalid_target.kn` [F]
**What it tests:** That `.target("not_a_real_target")` produces a consistent error.
**How:** Sets an invalid target string.
**Expected:** Consistent error message listing valid targets.
**Determinism check:** Error text identical across runs.

#### `src/target_edges/mixed_targets.kn` [P]
**What it tests:** That a project with multiple targets (llvm + wasm) produces deterministic outputs for each.
**How:** Defines a project with `.targets("llvm", "wasm")`, builds both, diffs llvm output across runs.
**Expected:** Each target's output is independently deterministic.
**Determinism check:** Both .exe and .wasm are byte-for-byte identical across runs.

#### `src/target_edges/multi_target_determinism.kn` [P]
**What it tests:** That the output file selection for multi-target builds is deterministic.
**How:** Builds with `--targets llvm,wasm,js`, checks that the same output files are produced each run.
**Expected:** Same file set, same contents per file.
**Determinism check:** File manifest identical across runs.

---

### Category 5: Profile Edges

#### `src/profile_edges/debug_vs_release.kn` [P]
**What it tests:** That debug and release profiles produce different but each-deterministic outputs.
**How:** Builds same source with debug, then release. Compares outputs across two debug builds and two release builds.
**Expected:** Debug1 == Debug2; Release1 == Release2; Debug1 != Release1.
**Determinism check:** Intra-profile determinism holds. Inter-profile difference is expected.

#### `src/profile_edges/invalid_profile.kn` [F]
**What it tests:** That `.profile("not_a_profile")` produces consistent fallback behavior.
**How:** Sets an invalid profile string.
**Expected:** Consistent fallback (either error or silent fallback to debug — but same every time).
**Determinism check:** Behavior is identical across runs.

#### `src/profile_edges/cross_profile_cache.kn` [P]
**What it tests:** That building with debug doesn't poison the release cache and vice versa.
**How:** Build debug → build release → rebuild debug. Checks that the second debug build isn't contaminated by release artifacts.
**Expected:** Each profile's cache is independent. Debug rebuild matches original debug build.
**Determinism check:** Cache keys include profile. No cross-contamination.

---

### Category 6: Cache & Output Edges

#### `src/caching/repeat_build_diff.kn` [P]
**What it tests:** The core determinism invariant: two builds from the same source produce identical artifacts.
**How:** Build pass 1 → save artifact checksums → clean → build pass 2 → compare checksums.
**Expected:** All artifacts are byte-for-byte identical.
**Determinism check:** SHA256 match on every output file.

#### `src/caching/always_run_vs_cached.kn` [P]
**What it tests:** That `.always_run()` tasks are re-executed but still produce identical output.
**How:** Creates an `.always_run()` task that writes to a file. Runs it twice.
**Expected:** Both runs produce identical output files (the task is re-executed, but deterministic).
**Determinism check:** Output files match.

#### `src/caching/path_interpolation.kn` [P]
**What it tests:** That `$root`, `$blade`, `$task` interpolate the same path regardless of run context.
**How:** Tasks that write their interpolated paths to a report file. Compare reports across runs.
**Expected:** All interpolated paths are identical.
**Determinism check:** Path report is byte-identical.

#### `src/caching/clean_vs_incremental.kn` [P]
**What it tests:** That a `--clean` build produces the same output as a fresh checkout build.
**How:** Build 1 (clean) → Build 2 (incremental, touching nothing) → Build 3 (clean again). Compare 1 vs 3.
**Expected:** Build 1 and Build 3 produce identical artifacts.
**Determinism check:** Clean build is idempotent.

---

### Category 7: Comptime Edges

#### `src/comptime_edges/comptime_in_build.kn` [P]
**What it tests:** That `comptime` blocks inside `build.kn` helper functions produce deterministic results.
**How:** Uses comptime to generate task IDs, count source files, or compute cache keys.
**Expected:** Comptime evaluation produces same results every compilation.
**Determinism check:** Generated values match across runs.

#### `src/comptime_edges/conditional_task_factory.kn` [P]
**What it tests:** That conditional task creation (e.g., `if ctx.lane == "release"`) produces a deterministic graph.
**How:** Defines a task factory function that branches on BuildContext fields. Evaluates with same context twice.
**Expected:** Same context → same graph.
**Determinism check:** Serialized graph is identical.

#### `src/comptime_edges/host_env_leakage.kn` [P]
**What it tests:** That host environment variables don't leak into build.kn comptime evaluation.
**How:** Sets different env vars before two builds. Checks if output differs.
**Expected:** Output is identical regardless of env vars (except documented KAIN_* config vars).
**Determinism check:** Artifacts match despite env var differences.

---

### Category 8: Import Resolution Edges

#### `src/import_edges/circular_module_import.kn` [F]
**What it tests:** That circular `use` between build helper modules is detected consistently.
**How:** Module A uses Module B; Module B uses Module A.
**Expected:** Consistent error identifying the cycle.
**Determinism check:** Error text and error code identical across runs.

#### `src/import_edges/shadowed_modules.kn` [P]
**What it tests:** That when a local module shadows a stdlib module, resolution is deterministic.
**How:** Creates a local `src/build.kn` helper that shadows `std::build`. Tests which one resolves.
**Expected:** Consistent resolution (either local wins or error — but same every time).
**Determinism check:** Resolution result does not vary between runs.

#### `src/import_edges/missing_module_root.kn` [F]
**What it tests:** That a missing module root produces a clear, deterministic error.
**How:** Uses `.module_root("nonexistent/dir")`.
**Expected:** Error identifies the missing directory. Same error text every run.
**Determinism check:** Error output identical across runs.

---

## Design Principles

1. **Determinism-first.** Every edge case has a clear pass/fail for whether the build is deterministic under that condition. "It works" is not enough — it must work the same way every time.

2. **Boring reliability.** The goal is for this suite to eventually be so solid that running it is boring. Zero surprises. Zero flakes. A green run should mean nothing interesting happened.

3. **Self-documenting failures.** When an edge case fails, the report must identify WHAT changed, WHY it matters, and HOW to reproduce it. No opaque "test_build_42 failed."

4. **Subset runnable.** Each category can be run independently. Debugging one edge case category should not require waiting for all others.

5. **Valid build authority.** The main `build.kn` is itself a valid, compilable build authority. Invalid edge cases (duplicate IDs, cycles) are tested via sub-build.kn files in `src/`.

6. **Zero flakes.** Every edge case must produce the same result every time on the same machine with the same compiler. If an edge case is inherently non-deterministic (timestamp embedding, RNG), it must be explicitly marked as known-non-deterministic.

7. **CI-ready.** The suite outputs structured reports (JSON) suitable for CI consumption. Exit code 0 = all deterministic. Exit code 1 = non-determinism found. Exit code 2 = harness error.

---

## Related Blades

| Blade | Relationship |
|-------|-------------|
| `blades/test/build-kn-system-smoke/` | The sibling integration test — stresses the evidence DAG with real tasks (cargo, GPU, Z3, fabric). Complements this suite's edge-case focus. |
| `blades/edge_cases/codegen_edge_gaps/` | LLVM codegen edge cases. Shares the harness pattern (cause/effect/spookymagic/diagnostics) and `spawn.kn` self-replicating cloner. |
| `blades/edge_cases/runtime/` | Debug template for rapid edge-case testing. Source of the 4-layer architecture pattern used by the harness. |
| `smoketest/` | Album-edition workspace. Reference for complex multi-root build.kn patterns and the `smoke_mode()` task factory idiom. |
| `docs/BUILD_PROJECTS.MD` | Canonical reference for the build.kn DSL. The source of truth for every builder method used in this suite. |
