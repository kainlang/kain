# Kain Working Memory

This file is the running memory for major architectural moves in `M:\Code\Kain`.
It is not meant to be a raw changelog.
It should preserve:

- what we were trying to make true
- what the system understands now that it did not before
- what design bets we made on purpose
- what remains incomplete or dangerous
- what future work should preserve instead of accidentally undoing

## 2026-03-21 - Testing Lane Guide Was Made Explicit

The top-level `testing/` directory finally has a root README that explains how phases progress and which outputs stay disposable.

Key takeaways:

- treat `Intermediate/`, `_Builds/`, `Binaries/`, and compiled artifacts as disposable test outputs
- keep durable test results in `docs/validation/` or `docs/recent/`
- move probes from `Unsorted/` into the smallest stable phase once they are vetted

## 2026-03-21 - Pipeline Output Hygiene Was Re-centered

The pipeline lanes were accumulating compiled outputs in `generated/`, `labs/`, and `smoketest/`.

The working rule now:

- compiled artifacts (`.exe`, `.dll`, `.lib`, `.obj`, `.o`, `.pdb`, `.ilk`) stay disposable
- caches like `target/`, `.kain`, and `.kain-runtime` should be cleared after validation
- any log or validation proof worth keeping should live under `docs/validation/` or `docs/recent/`

## 2026-03-21 - Parent Ignore Globs Were Normalized

Repo-wide searches were getting noisy because the parent `M:\.gitignore` had malformed Windows-style backslash globs.

The fix was simple, but the lesson matters:

- `gitignore` syntax needs to stay portable and valid, even in parent workspace files
- a broken parent ignore file can make repo hygiene work look more broken than the tree actually is
- when search tooling starts warning on ignore parsing, fix the ignore file instead of normalizing the warning away

## 2026-03-21 - Docs Landing Pages Were Restored

The repo map and README had drifted ahead of the filesystem again: `docs/README.md` was missing even though the root docs navigation still expected it.

This pass restored the docs landing pages and tightened the doc anchors so future cleanup work has a real navigation layer to follow:

- `docs/README.md`
- `docs/crates/README.md`
- `docs/pipeline/README.md`

The important lesson is the same one that keeps repeating in this repo:

- if a folder is important enough to show up in the repo map, it needs a living README
- stale memory references should point at current doc anchors, not retired one-off audits
- pipeline docs should stay pinned to the canonical runtime contract and not float as invisible knowledge

## 2026-03-21 - Remaining Stale Root Docs Were Confirmed Safe To Remove

This pass checked the still-pending root markdown deletions against the active docs map and found no current references outside the repo memory itself.

That means these files can stay gone without breaking the current documentation surface:

- `CODEGEN_OPERATOR_AUDIT.md`
- `WILD_FEATURE_RECOMMENDATIONS.md`
- `docs/archive/cleanup.md`
- `docs/archive/EDITOR_PIPELINE_IMPROVEMENTS.md`
- `docs/crates/README.md`

The useful lesson here is that cleanup work should always be confirmed against the live repo maps before being treated as final. The repo-level docs can get ahead of the tree, but the tree must stay internally consistent.

Current run recorded at 2026-03-21T17:17:46.8323301Z.

## 2026-03-22 - C Runtime Pipeline Notes Were Promoted

The C runtime lane now has a dedicated pipeline doc under `docs/pipeline/` to keep
runtime bundle validation, outputs, and cleanup rules anchored in the docs index.

Key takeaways:

- runtime bundle validation should write temporary JSON into `generated/` instead of the repo root
- `graphics_runtime_smoke_*` bundles are disposable and should be removed after each run
- `target/` remains disposable and should be cleared after pipeline runs (some files may be locked)

Supporting updates:

- `crates/README.md` now points at the crates maintenance pipeline doc
- `ouroborosV2/README.md` is now the folder guide for the nested repo
- the stale root `graphics_runtime_smoke_env_bundle.realtime_app.json` artifact was removed

Current run recorded at 2026-03-22T00:19:52.9857184-04:00.

## 2026-03-21 - Stale Root Docs And Empty Placeholders Were Removed

This pass cleaned a small set of dead markdown artifacts that were no longer referenced by the active repo docs:

- `CODEGEN_OPERATOR_AUDIT.md`
- `WILD_FEATURE_RECOMMENDATIONS.md`
- `docs/archive/cleanup.md`
- `docs/archive/EDITOR_PIPELINE_IMPROVEMENTS.md`
- `docs/crates/README.md`

The useful lesson from this run is that repository searches can still be tripped up by parent ignore files outside the workspace. If `rg` starts failing on glob parsing, use `--no-ignore-parent` instead of assuming the repo itself is broken.

Current run recorded at 2026-03-21T12:53:13.2630386-04:00.

## 2026-03-21 - Kain Fabric Phase 1 Landed As A Real Manifest And Validation Surface

Today `Kain Fabric` stopped being only a product idea and became a real repo-visible entry point.

The important truth is narrow on purpose:
Fabric is not a distributed runtime yet.
It is not a cloud scheduler.
It is not a replacement for compiler-owned execution semantics.

What became real is the first honest layer:

- a canonical `KAIN.fabric.toml` manifest
- local-first Fabric templates
- typed runtime-step declarations for `kain`, `python`, `rust_crate`, `c_abi`, and `node`
- capability validation
- dependency-cycle and duplicate-id validation
- first-class CLI commands for `kain fabric init`, `kain fabric validate`, and `kain fabric run`

### What Changed In Practice

On the orchestration side, `crates/kain-omni` now owns a real Fabric manifest/validation path instead of leaving the concept as a doc-only plan.

That path includes:

- manifest schema/version truth
- local and polyglot starter templates
- runtime kind declarations
- contract-kind declarations
- local capability validation
- dependency graph validation

On the CLI side, `crates/cli` now exposes Fabric as a first-class command family instead of hiding it behind future-work docs.

The commands are intentionally split by honesty:

- `kain fabric init` scaffolds a workspace and starter manifest
- `kain fabric validate` parses and validates a Fabric manifest
- `kain fabric run` validates successfully and then explicitly reports that execution is not wired yet

That last point matters.
The run command is a truthful stub, not a fake implementation dressed up as a platform.

### Files That Became The First Fabric Spine

- `crates/kain-omni/src/fabric.rs`
- `crates/kain-omni/src/lib.rs`
- `crates/cli/src/fabric.rs`
- `crates/cli/src/lib.rs`
- `crates/cli/src/main.rs`

### Architectural Meaning

Three design bets became real today:

1. Fabric will grow out of existing manifest infrastructure, not beside it.
   `kain-omni` is now the home for Fabric manifest truth.

2. Fabric will be local-first before it is ambitious.
   The validator knows about local capabilities and local runtime kinds first.

3. Fabric will stay subordinate to compiler/runtime truth.
   It validates orchestration shape.
   It does not define what compute, UI, shader, or runtime semantics mean.

### Why This Matters

This is the first point where Kain can start moving from:

- "we have many bridges and many targets"

toward:

- "we have one typed entry point for heterogeneous software composition"

That is strategically important because it gives Kain a practical adoption wedge that does not require users to rewrite everything into Kain first.

### Validation That Passed

The focused validation loop for the first Fabric slice passed:

- `cargo fmt --package kain-omni --package cli`
- `cargo test -p kain-omni fabric -- --nocapture`
- `cargo test -p cli fabric -- --nocapture`
- `target/debug/kain.exe fabric --help`
- `target/debug/kain.exe fabric init --help`
- `target/debug/kain.exe fabric validate --help`

This does not mean the full workspace is globally clean.
It means the Fabric phase-1 slice compiles and validates inside the existing repo reality.

### What Is Still Incomplete

- No Fabric executor exists yet.
- `kain fabric run` does not execute steps.
- No session lock file exists yet.
- No event stream exists yet.
- No `kain-host` Fabric runtime exists yet.
- Python, Rust crate FFI, C ABI, and Node are declared runtime kinds, not executed Fabric adapters yet.
- No end-to-end `smoketest/fabric/*` proof exists yet.

### What Future Work Should Preserve

- Do not turn Fabric into a second semantics layer.
  It should orchestrate runtimes and contracts, not redefine them.

- Do not invent a Fabric-specific compute dialect.
  If compute plans, tensor metadata, or dispatch semantics already belong to compiler-owned bundles, Fabric should consume those outputs rather than replacing them.

- Do not move Fabric ownership into a new god crate if `kain-omni`, `kain-driver`, `kain-interop`, and `kain-host` can keep the boundaries clean.

- Do not claim remote/distributed execution until local session execution is undeniably real.

- Do not let `kain fabric run` become a fake success command.
  It should remain explicit about scaffolded versus implemented behavior.

### Next Serious Move

The next real step is Phase 2:

- add a local Fabric session model in `kain-host`
- make `kain fabric run` execute a Kain-only manifest first
- emit session events and a lock/report artifact
- then wire Python, Rust crate FFI, C ABI, and Node adapters one by one

If that path holds, Fabric stops being "manifest paperwork" and starts becoming a genuine local-first polyglot execution lane for Kain.

## 2026-03-21 - LLVM Native Packaging Stopped Being A Side Quest

This pass closed an important emotional gap in the pipeline.
We already had a real compute executor, a residency contract, and a raw-native viewport that wanted to consume them, but the normal LLVM/native build lane was still too casual about staging the runtime truth beside the executable.
That kind of gap is how strong systems quietly turn back into demos.

The main correction was architectural discipline:
we pulled the LLVM/native artifact staging logic out of the CLI monolith and turned it into a dedicated library module.
That sounds small, but it matters because `kn` still includes `main.rs` directly, so every extra ounce of packaging logic left in that file gets duplicated in the noisiest possible compilation path.
Moving the staging code into a real module gave the packaging lane a stable home and stopped the raw-native build contract from living as a brittle side effect.

### What The System Understands Now

The LLVM/native lane now treats these artifacts as a single runtime story, not a bag of unrelated files:

- runtime contract
- realtime app bundle
- compute residency manifest
- compute residency payload binaries
- shader bundle
- `kain_gpu_runtime.dll`

That means a raw-native build no longer has to rely on wishful thinking that the viewport will somehow discover the right compute-side assets later.
The executable lane now stages the files that the runtime actually needs in order to execute `primary_compute` as runtime truth.

### Why The Module Split Matters

There was a deeper lesson hiding here:
the raw-native packaging path is not just another helper.
It is the place where compiler intent, runtime contracts, SPIR-V assets, residency sidecars, and native executable layout all become one physical deployment shape.
That deserves a named seam.

We created `cli/src/llvm_native_stage.rs` specifically so this deployment logic can grow without dragging more complexity into `main.rs`.
This should be preserved.
If future work adds release-vs-debug DLL policy, richer sidecar manifests, or platform-specific staging rules, that logic belongs in the staging module first, not scattered back into the CLI entrypoint.

### Validation Outcome

The good news is that the new packaging seam validated cleanly:

- new CLI tests now prove LLVM/native staging for compute-bearing and UI-only sources
- the native UI packaging regression still passes with compute residency sidecars present
- the native runtime C smoke compile still passes
- full `cargo test` still fails only on the pre-existing external self-hosting fixture under `M:\Code\Other\kainselfhosting\...`

That is exactly the result we wanted.
This move changed the runtime deployment shape, but the workspace-wide failure signature did not get worse or shift in a suspicious way.

### Guardrails

- Do not move the raw-native artifact staging policy back into `main.rs`.
- Do not let the LLVM/native lane emit only the `.ll` and executable while quietly omitting the runtime-side compute assets.
- Do not treat the residency manifest as optional when `primary_compute` is part of the emitted truth.
- If future packaging lanes appear, they should reuse the same staging semantics instead of inventing a second compute deployment dialect.

## 2026-03-21 - Crates Guide Restored And Strategy Notes Indexed

The repo map had drifted ahead of the filesystem again: `crates/README.md` was missing even though the root map still treated it like a first-class navigation point.
I restored that guide, synced the root and crate-level maps, and added a small README for `docs/kainvsgiants/` so the strategy note folder is a deliberate doc surface instead of a loose one-off.

Lesson:

- If a folder is important enough to show up in the repo map, it is important enough to have a real README and stay in sync with the map.
- `kain-gpu-runtime` now needs to stay visible as a runtime executor crate, not buried as a side artifact.
- Stale audit dump docs should be retired in favor of a small, living folder guide.

## 2026-03-20 - Tensor-Stream Compute Lane Becomes Real Compiler/Runtime Memory

Today the work stopped being "Kain has compute shaders somewhere" and started becoming "Kain is learning how to describe a compute-native execution lane as compiler-owned truth."

The most important shift is semantic, not cosmetic:
we pushed the system from vague GPU capability toward a structured model where a compute shader can now carry authored intent about dispatch, tensor payloads, stream roles, and neural-node planning.
That matters because the long-term goal is not just to emit SPIR-V blobs.
The goal is for Kain to understand continuous dataflow at compile time and then hand native runtimes enough structure to execute that intent coherently.

### What Changed In Practice

On the compiler side, `kain-core` now supports explicit compute-plan metadata authored from shader `comptime` blocks.
The current convention is intentionally constrained and conservative:

```kn
let compute = (
    [dispatch_x, dispatch_y, dispatch_z],
    [
        ("binding", "element_type", ["shape", "dims"], "role", "contract")
    ],
    [
        ("node_key", "op", ["inputs"], ["outputs"], stateful)
    ]
)
```

That data is now parsed, validated, and threaded into emitted realtime bundles and runtime contracts.
When present, it overrides the older heuristic-only path for dispatch/tensor/node planning.
When absent, the legacy fallback still exists so the broader tree does not collapse.

On the native-runtime side, the raw-native lane now does more than validate `primary_compute`.
It has a real execution handoff/fallback state path:

- the graphics bundle is loaded explicitly in the raw-native viewport lane
- `primary_compute` is executed into a per-frame runtime state record
- that execution state is surfaced in overlay/debug information
- viewport rendering now reflects that compute state instead of pretending it does not exist

This is not yet true GPU compute dispatch.
It is a runtime-owned bridge between "metadata exists" and "execution semantics are visible and alive."
That bridge is important because it gives us a place to evolve dispatch, residency, scheduling, and future SPIR-V execution without falling back into one-off demo code.

### Architectural Meaning

Three ideas became more concrete today:

1. Compute is no longer just a shader stage.
It is being treated as a first-class execution domain with tensor, stream, and neural semantics.

2. The compiler is beginning to own dispatch intent.
Even though some fallback behavior remains, the direction is now explicit:
dispatch sizing and operator metadata should be authored and emitted, not guessed by hosts forever.

3. The raw-native runtime is no longer purely passive.
It now has a legitimate role in executing and surfacing compute plans rather than only rejecting malformed metadata.

### Design Bets We Made On Purpose

- We preferred `comptime`-block authored metadata over adding a wider public AST break for shader constructors.
  That let the feature land without detonating other crates that instantiate `Shader` directly.

- We treated tensor and stream semantics as data attached to resource bindings, not as hardcoded runtime assumptions.
  This keeps the door open for future backends, NPUs, CUDA, or other ML/runtime targets.

- We added a native execution fallback/handoff instead of pretending full GPU dispatch was already solved.
  This gives us a truthful intermediate substrate that can still drive viewport/runtime behavior.

### What Is Still Incomplete

- True native GPU compute execution is not wired yet.
  The runtime fallback executes a compute-state model, not actual SPIR-V dispatch.

- `workgroup_size` still has fallback behavior when authored metadata is absent.

- Tensor shapes are explicit only when authored.
  Otherwise they still fall back to inferred/simple defaults.

- Neural nodes are still a compiler-emitted operator plan, not a true runtime scheduler with residency, fusion, or dependency orchestration.

- Full workspace tests are still not globally green due to repo-level issues outside this slice.
  The notable blockers during validation were:
  an external missing fixture under `M:\Code\Other\kainselfhosting\...`
  and linker OOM pressure in large CLI test binaries on Windows.

### What Future Work Should Preserve

- Do not collapse tensor/stream/neural metadata back into anonymous compute bindings.
  The whole point is that Kain should progressively understand dataflow structure, not just pass through lower-level payloads.

- Do not move dispatch ownership back into host heuristics if authored metadata exists.

- Do not let raw-native, Rust-native, and future backends invent separate compute-plan dialects.
  The emitted bundle must stay the center of truth.

- If we add real SPIR-V/native compute dispatch next, it should consume the same `primary_compute` plan and enrich it, not replace it with a host-local shortcut.

### Next Serious Move

The next step is to connect this authored compute-plan lane to actual backend execution:

- compiler-owned dispatch/workgroup truth should become standard, not optional
- tensor shape metadata should map to real residency/buffer layouts
- `primary_compute` should dispatch through a real execution backend
- neural-node plans should graduate from descriptive metadata into runtime scheduling primitives

If this direction holds, Kain stops looking like "a language that can target GPU shaders" and starts looking more like "a language/runtime that understands heterogeneous dataflow as part of compilation itself."

## 2026-03-20 - Explicit Compute Plans Landed, Runtime Execution Stopped Being Purely Decorative

The follow-up move today was to stop pretending the compiler and runtime were "close enough" on compute intent.
We added an authored compute-plan path and then made the raw-native viewport consume executable compute state instead of treating `primary_compute` as a validation artifact.

### What Changed

On the compiler side, compute shaders can now carry an explicit authored plan through `comptime` data.
That plan gives the compiler an intentional source of truth for:

- dispatch size
- tensor binding metadata
- neural node planning

This is materially different from the earlier heuristic pass.
The heuristic path still exists for compatibility, but there is now a real authored lane that tells the compiler what the compute workload is supposed to mean.

On the native side, the raw-native viewport now loads the graphics bundle and drives a real per-frame compute execution state.
This is still a fallback execution substrate, not full SPIR-V/Vulkan dispatch in the C runtime, but it means:

- `primary_compute` is stepped every frame
- dispatch counts, tensor counts, stream counts, and neural-node counts now live as runtime state
- that execution state feeds overlay/debug output
- viewport rendering can respond to compute phase instead of acting like compute metadata is inert

### Why This Matters

Before this pass, the runtime could say "the compute plan is valid."
After this pass, the runtime can at least say "the compute lane is alive right now, here is the state it is producing, and the host is reacting to it."

That is still not the final end state, but it is a meaningful transition:
validation-only systems die in place.
Execution-visible systems become pressure points that force the backend story to mature.

### What Is Still Missing

The actual last leap is still ahead:

- full SPIR-V dispatch promoted from test-only Vulkan code into a reusable runtime service
- shared-buffer residency and binding moved from descriptive contracts into true backend resource ownership
- native runtime compute results feeding real scene buffers, materials, particles, terrain, or viewport surfaces instead of only debug/live-state channels

The repo already contains a strong clue for the next move:
`crates/gpu/tests/spirv_execute.rs` is not hypothetical.
It is a real Vulkan SPIR-V execution harness.
The correct direction is to promote that into a reusable runtime/backend service rather than rebuilding execution semantics from scratch in every host.

## 2026-03-20 - The Vulkan Executor Graduated Out Of Test-Only Space

This was the first pass where the SPIR-V execution story stopped living only inside a test and started becoming runtime infrastructure.

The important move was not just "we made another crate."
The important move was that the old `spirv_execute.rs` Vulkan path was promoted into a dedicated runtime-facing module with a C ABI surface, and the raw-native viewport was pointed at that direction instead of only carrying synthetic compute state.

### What Changed

We now have a dedicated `kain-gpu-runtime` crate.
That crate owns the Vulkan setup and SPIR-V dispatch logic that used to be trapped in the GPU test harness.
The old test still matters, but it is now proving a library instead of being the only place where compute execution really exists.

We also moved the residency sidecar from a loose compute metadata snapshot toward an actual bootstrap artifact:

- deterministic compute residency manifest
- per-binding payload files
- resolved descriptor/binding metadata that the runtime can consume

On top of that, `kain-interop` now has a concrete shared-buffer-to-GPU-binding adapter.
That means the `kain.shared.buffer` contract is no longer just conceptual in this lane.
It is beginning to function as the runtime-facing binding truth for compute execution.

Finally, the raw-native viewport now has a real ABI loading path toward the GPU runtime.
It is still early and not yet the final generalized host-bridge form, but the direction is correct:
the C lane is no longer forced to fake compute forever.

### Why This Matters

Before this pass, the best compute execution path in the repo was:

- real Vulkan dispatch in test code
- runtime metadata in production code
- synthetic execution state in the raw-native host

That split was not sustainable.

After this pass, the architecture is more coherent:

- Vulkan dispatch is becoming reusable runtime code
- residency is beginning to exist as a runtime bootstrap contract
- shared buffers have a descriptor-facing adapter
- raw-native is beginning to talk to a real compute executor

That is the first shape that can realistically grow into a serious heterogeneous runtime story.

### What Is Still Incomplete

- The C ABI is intentionally minimal and still path-oriented in places.
  It is enough to establish execution, but it is not yet the final "all buffer metadata passed explicitly as plain structs" design.

- The residency sidecar is now real enough to bootstrap compute bindings, but uniform/scalar policy is still thinner than the storage-buffer lane.

- The raw-native viewport can now prepare for real compute execution, but the packaging and native startup path still need a more complete production handoff for the runtime DLL in all lanes.

- Full workspace validation is still constrained by unrelated repo blockers and Windows linker pressure, so broad green status remains noisy.

### Guardrail

Do not let `kain-gpu-runtime` turn into a random dumping ground for GPU experiments.
It should stay the execution-side counterpart to compiler-owned SPIR-V bundles and residency contracts.
Its job is not to become "another graphics engine."
Its job is to make Kain-owned compute payloads executable as runtime truth.

### Guardrail

Do not let the fallback execution path become the final architecture.
It exists to keep the compute lane alive while we promote real backend dispatch into the runtime story.

## 2026-03-20 - Root Repo Map And Compute Docs Were Brought Back Into Sync

This run tightened the repo's documentation around the compute lane and the top-level layout.

### What Changed

- Added a top-level `repomap.md` so the root workspace has the same folder-guide treatment as `crates/`.
- Updated the README to describe authored compute metadata as compiler-owned truth, not a runtime heuristic.
- Documented the raw-native viewport bridge as a compute execution/state surface, not a full GPU dispatcher.

### Lesson

When a feature starts crossing compiler, runtime contract, and native viewport boundaries, the docs should call out the ownership split explicitly.
That keeps future changes from collapsing authored intent back into host-local inference.

## 2026-03-21 - Crates + App/Toolchain Guides Hardened

The docs layer now explicitly tracks the crates maintenance pipeline, and the repo has folder guides for `apps/` and `toolchain/`.

The intent is to keep the crate surface and tooling lanes data-driven and discoverable:

- `docs/pipeline/CRATES_PIPELINE.md` defines the update order for crates metadata.
- `apps/README.md` and `toolchain/README.md` keep app outputs and toolchain drops understandable.


## 2026-03-22 - Research + Report Lanes Re-homed

The top-level `Research/` and `reports/` folders were moved into the docs layer to keep the repo root focused on source and runtime lanes.

### What Changed

- Consolidated `Research/` into `docs/research/` with a new folder guide.
- Moved the latest report into `docs/recent/reports/` and kept the reports README as a sub-guide.
- Updated the docs index and repo map to reflect the new `docs/research/` lane.

### Cleanup Notes

- Removed cached `.kain` directories where possible; the cache inside `generated/_ue5_smoke_pokered/.kain` could not be deleted due to access locks.
- `target/` still appears locked by another process and needs a clean sweep when the build pipeline releases it.

## 2026-03-22 - Stale Native App Outputs Were Purged

A cleanup pass removed generated native-app outputs that had leaked into source-controlled lanes, including app and smoketest native UI build products.

Key takeaways:

- `apps/kade-desktop/native-app` and `native-app-preview` are disposable build outputs, not canonical sources.
- UI smoke `native-app` folders are build artifacts and should be cleared after validation runs.
- `target/` and `.kain` caches are still the primary cleanup targets; some directories may be locked during active builds and must be cleared once the processes exit.

Current run recorded at 2026-03-22T06:20:00-04:00.

## 2026-03-22 - Conformance Bin Cleanup Pass

Conformance harness binaries under `runtime/conformance/**/bin` are disposable artifacts and were cleared this run.
Testing lane intermediate build outputs were removed to keep the test tree clean.

Locked outputs remain under:
- `generated/_ue5_smoke_pokered/.kain`
- `smoketest/UI/website_clone_signalcraft/native-app/target`
- `target/` (repo root)

Current run recorded at 2026-03-22T04:19:26-04:00.
