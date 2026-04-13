# Kain UI Overhaul - Global Task List

This file is the canonical global control surface for the current party wave.
It is not a swarm plan.
It is the shared board for the live party room, the per-agent notes in `party/*.md`, and the next edit wave.

If any file or room note mentions `global_task_list.md`, `GLOBAL_TASK_LIST.md`, or `global-task-list.md`, treat that as an older alias and fold the live truth back into this file.

## Mission

Land the next hard-truth UI wave for Kain so the system moves materially closer to:

- compiler-owned event, workspace, surface, and anchor truth
- runtime-owned reload, invalidation, focus, selection, overlay, and patch authority
- spatial verifiability from structure alone
- compatibility bridges that are explicit, machine-detectable, and never confused for doctrine
- thin adapters that realize semantic truth instead of inventing backend-local meaning

The room is done with loose planning.
This wave is about cutting ambiguity out of the code and leaving behind a control surface that stays mergeable under heavy parallel work.

## Canonical Shared Goal

Push Kain UI toward a React-class semantic engine with:

- compiler-owned truth
- runtime-owned reload, state, and invalidation authority
- explicit workspace, dock, tab, anchor, and surface contracts
- structural geometry and containment visibility
- thin compatibility layers
- explicit fallback policy instead of silent host invention

## Hard Rules

- Do not use this file like a brainstorm scratchpad. It is a control document.
- Do not run heavy tests until the user enables testing for the conversation.
- Every agent updates its own `party/<name>.md` with touched files, blockers, decisions, and the next handoff.
- If a path is compatibility-only, mark it that way in code and in notes.
- If a path invents semantics outside its owner boundary, stop and hand it back to the correct lane.
- Do not let native convenience APIs become semantic truth.
- Do not hide fallback behavior. If a fallback happens, expose it in emitted data, session state, logs, or a clearly labeled compatibility branch.
- Keep edits collision-safe. Shared-file work must be split by exact function or region, not by vibes.

## Shared Deliverables For This Wave

- compiler-emitted typed route, workspace, tab, dock, anchor, and surface truth where missing
- runtime consumption of emitted truth before any tree-shape inference
- explicit compatibility markers for every surviving backfill path
- a compact compatibility-debt ledger with risk and replacement target
- a compact merge artifact with issue, owner, file, dependency, and status
- an updated minimal proof spine for the risky seams
- a room-level dependency map that keeps the next edit wave parallel and collision-safe

## Success Bar

This wave is successful when all of the following are true:

- `crates/kain-core/src/ui.rs` emits canonical route and interaction metadata instead of leaving host-side guesswork as the practical source of truth.
- `crates/kain-core/src/realtime_app_bundle.rs` exports workspace, tab, dock, anchor, and surface truth strongly enough that downstream consumers do not have to rediscover it.
- `crates/kain-ui/src/lib.rs` prefers emitted truth first and only falls back to legacy inference behind explicit compatibility markers.
- `crates/kain-ui/src/runtime_execution.rs` treats reload and retained runtime transfer as contract-first behavior, not backend-local inference.
- `UiNativeProjection` remains a compatibility sidecar instead of drifting into semantic-IR status.
- The party can point to one canonical board, one compatibility ledger, one merge artifact, and one minimal proof spine without contradictory copies.

## Critical Path

1. Emit missing truth in `kain-core`.
2. Consume emitted truth in `kain-ui`.
3. Quarantine or remove compatibility seams that still look canonical.
4. Refresh the acceptance and proof surface around the landed cuts.
5. Normalize outputs into one mergeable room artifact.
6. Slice the next edit wave by exact file and function ownership.

## Parallel Launch Model

Everyone starts now, but not everyone starts on the same file.

The room runs in four parallel bands:

1. Core truth lane
   - Cecil

2. Runtime consumption and compatibility cut lanes
   - Rikku
   - Cloud
   - Vincent

3. Proof, gap analysis, and merge synthesis lanes
   - Vivi
   - Barret
   - Tifa
   - Tidus

4. Control and coordination lanes
   - Sazh
   - Balthier
   - Zidane

The control lanes stay active for the entire wave.
The proof and synthesis lanes do not wait for all code to finish; they track live outputs as they land.

## Exact File And Function Ownership

These slices exist so the room can work in parallel without turning `lib.rs` into a knife fight.

### Cecil

- Owns:
  - `M:\Code\Kain\crates\kain-core\src\ui.rs`
  - `M:\Code\Kain\crates\kain-core\src\realtime_app_bundle.rs`
- Owns by concept:
  - compiler emission
  - canonical contract keys
  - bundle-visible truth
- Must not drift into:
  - `kain-ui` runtime heuristics
  - native adapter realization

### Rikku

- Owns:
  - `M:\Code\Kain\crates\kain-ui\src\lib.rs`
- Owns by concept:
  - semantic leak cleanup
  - emitted-truth-first bundle assembly
  - compatibility markers for backfill paths
- Exact `lib.rs` slice:
  - `ui_runtime_bundle_from_output(...)`
  - emitted runtime-system preference paths
  - session-state compatibility markers
  - focus, selection, overlay, and workspace synthesis paths that still look canonical
- Must not drift into:
  - `runtime_execution.rs` reload ownership
  - inventory-only compatibility ledger work

### Cloud

- Owns:
  - `M:\Code\Kain\crates\kain-ui\src\runtime_execution.rs`
  - `M:\Code\Kain\crates\kain-ui\src\lib.rs` compatibility-audit regions only
- Owns by concept:
  - canonical-vs-compatibility boundary enforcement
  - fallback call-site audit
  - compatibility labeling
- Exact `lib.rs` slice:
  - workspace rebuild paths that still seed from inferred tree shape
  - `ui_native_projection_from_output(...)` and related compatibility-only helpers
  - comments and labels on keep-label and tighten-only bridge seams
- Must not drift into:
  - new compiler-owned semantics
  - Vincent's inventory-only lane

### Vincent

- Owns:
  - `M:\Code\Kain\party\vincent.md`
- Owns by concept:
  - compatibility-debt ledger
  - risk grading
  - replacement target mapping
- May inspect:
  - `lib.rs`
  - `runtime_execution.rs`
  - `ui.rs`
  - `realtime_app_bundle.rs`
- Must not drift into:
  - editing the same runtime seams Cloud is actively cutting

### Vivi

- Owns:
  - `M:\Code\Kain\party\vivi.md`
  - `M:\Code\Kain\docs\kainplan\ui_slate_x100\widget_registry_schema.md` if the room agrees a contract update is needed
- Owns by concept:
  - top-gap ranking
  - owner/file/acceptance mapping
  - semantic legibility goals
- Must not drift into:
  - general implementation edits outside her declared contract docs

### Barret

- Owns:
  - `M:\Code\Kain\party\Barret.md`
- Owns by concept:
  - minimum proof spine
  - regression surface mapping
  - validation ordering
- Must not drift into:
  - broad test execution
  - architecture speculation

### Tifa

- Owns:
  - `M:\Code\Kain\party\Tifa.md`
- Owns by concept:
  - merge artifact normalization
  - lane output formatting
  - issue severity and dependency capture

### Tidus

- Owns:
  - `M:\Code\Kain\party\Tidus.md`
  - this file as the room-level task captain after this update
- Owns by concept:
  - dependency ordering
  - deduped room map
  - implementation-wave publishing

### Sazh

- Owns:
  - `M:\Code\Kain\party\sazh.md`
- Owns by concept:
  - file claims
  - function and region slices
  - collision-safe handoffs
  - turning merge output into next-wave ownership

### Balthier

- Owns:
  - `M:\Code\Kain\party\balthier.md`
- Owns by concept:
  - seam prioritization
  - execution-order enforcement
  - scope discipline

### Zidane

- Owns:
  - `M:\Code\Kain\party\zidane.md`
- Owns by concept:
  - overlap control
  - collision warnings
  - duplicate-work interception

## Agent Task List

Each lane below is intentionally larger than a one-line role label.
Every agent should be able to move in parallel from this file alone.

## Cecil - Truth Emission Owner

### Objective

Make the compiler and bundle path emit the truth that the runtime and adapters are still rediscovering or guessing today.

### Primary Files

- `M:\Code\Kain\crates\kain-core\src\ui.rs`
- `M:\Code\Kain\crates\kain-core\src\realtime_app_bundle.rs`

### Deliverables

- typed event-route contract expansion
- canonical command and transaction label emission
- workspace, tab, dock, anchor, and surface descriptor emission where missing
- bundle-visible contract keys that downstream code can consume without tree-shape archaeology

### Task Checklist

- [x] Audit the current event-route lowering path for missing typed fields such as handler identity, target identity, phase, command route linkage, and canonical transaction label.
- [x] Expand workspace contract emission so persistence identity, tab grouping, active-tab intent, dock placement, and panel ownership survive into emitted truth.
- [x] Expand anchor and surface truth so menus, overlays, viewport-adjacent surfaces, and shader-canvas-like surfaces carry explicit descriptor data instead of prop discovery being the practical authority.
- [x] Confirm emitted contract keys are canonical and compatibility aliases are clearly marked as aliases, not peers.
- [x] Thread the new contract payloads into the realtime bundle so `kain-ui` and future adapters can consume them directly.
- [ ] Note any truth that still cannot be emitted cleanly and hand it to Vivi as a concrete remaining gap instead of hiding it in comments.

### Acceptance Signal

The runtime can point at emitted contract payloads for route, workspace, anchor, and surface truth without needing local rediscovery to understand what the UI means.

### Handoff To

- Rikku for emitted-truth-first runtime consumption
- Cloud for compatibility seam cleanup
- Vivi for any unresolved gap that remains architectural rather than implementation-only

## Rikku - Semantic Leak Hunter

### Objective

Strip quiet meaning leaks out of `kain-ui` so authored and compiler-emitted truth wins by default.

### Primary File

- `M:\Code\Kain\crates\kain-ui\src\lib.rs`

### Deliverables

- emitted-truth-first runtime bundle assembly
- explicit compatibility markers on any surviving backfill path
- reduced dependence on prop scanning, strings, and tree-shape synthesis for semantic meaning

### Task Checklist

- [x] Audit `ui_runtime_bundle_from_output(...)` and adjacent assembly paths so emitted runtime systems win whenever they exist.
- [x] Replace or reduce any semantic leak where focus, selection, overlay, or workspace meaning is still reconstructed from tree shape when the bundle now carries explicit truth.
- [x] Keep `ui.runtime.compatibility_fallback=true` or an equivalent explicit marker on every surviving backfill path.
- [x] Make the runtime-visible state distinguish authored-first truth from compatibility reconstruction without ambiguity.
- [x] Tag any unresolved bridge seam with an exact function name and replacement target so Vincent can keep the ledger accurate.
- [x] Avoid broad refactors. Stay in the leak seams that change semantic posture, not general cleanup.

### Acceptance Signal

`lib.rs` stops silently acting like the runtime invented the truth. Authored and emitted contract data is obviously first-class, and any fallback is visibly second-class.

### Handoff To

- Cloud for any remaining keep-label or tighten-only compatibility seams
- Tifa for merge artifact normalization
- Barret for proof mapping against the changed seams

## Cloud - Canonical-vs-Compatibility Auditor

### Objective

Turn the fallback seam list into concrete quarantine rules so compatibility residue cannot masquerade as architecture.

### Primary Files

- `M:\Code\Kain\crates\kain-ui\src\runtime_execution.rs`
- `M:\Code\Kain\crates\kain-ui\src\lib.rs` compatibility regions only

### Deliverables

- explicit keep, tighten, and replace decisions translated into code labels or narrow cuts
- contract-first reload and retained-state posture where possible
- `UiNativeProjection` held firmly in compatibility-sidecar territory

### Task Checklist

- [x] Revisit every fallback call site already identified in your lane note and convert the audit labels into code-level clarity or narrow implementation cuts.
- [x] Tighten workspace-layout rebuild paths so inferred tree shape is not accidentally the normal authority when explicit layout truth exists.
- [x] Audit `runtime_execution.rs` for reload and transfer behavior that still smells like hidden inference and make the contract-first path explicit.
- [x] Keep `UiNativeProjection` and related helpers visibly compatibility-only in naming, comments, and call-site posture.
- [x] Mark test-only legacy paths as test-only legacy paths so future readers do not mistake them for product architecture.
- [x] Report any seam that should be fully replaced by Cecil-owned emitted truth or Rikku-owned runtime consumption rather than quietly patching across lane boundaries.

### Acceptance Signal

A strong reader can open the relevant fallback sites and immediately tell which paths are canonical, which are bridges, and which are on the chopping block.

### Handoff To

- Vincent for final bridge ledger status
- Barret for proof surface updates
- Tidus for dependency and merge ordering

## Vincent - Compatibility-Debt Quarantiner

### Objective

Own the debt map so the room can kill or quarantine bridges deliberately instead of by memory.

### Primary File

- `M:\Code\Kain\party\vincent.md`

### Deliverables

- exact bridge inventory
- risk grading
- replacement targets
- "killed this wave" vs "allowed bridge" status after live edits land

### Task Checklist

- [ ] Keep the bridge inventory current for every compatibility surface that the room touches this wave.
- [ ] Mark each surface with risk, replacement target, current owner, and current state: untouched, labeled, reduced, replaced, or still dangerous.
- [ ] Flag any new compatibility path introduced during this wave so it does not slip in as a silent regression.
- [ ] Separate acceptable short-term bridges from "must die next" bridges.
- [ ] Feed Tifa a compact ledger summary that can be folded into the merge artifact without duplicate wording.
- [ ] Stay inventory-only unless the room explicitly reassigns an edit cut.

### Acceptance Signal

At the end of the wave the room can answer, from one file, which compatibility seams still exist, why they still exist, and what replaces them.

### Handoff To

- Tifa for normalization
- Tidus for room-level status
- Sazh for next-wave file slicing

## Vivi - Missing-Contract Architect

### Objective

Keep the room honest about what semantics are still missing even after the current code cuts land.

### Primary Files

- `M:\Code\Kain\party\vivi.md`
- `M:\Code\Kain\docs\kainplan\ui_slate_x100\widget_registry_schema.md` only if needed

### Deliverables

- top-gap ranking
- owner/file/acceptance mapping
- updated gap status after the wave
- deeper widget-registry guidance if that becomes the next bottleneck

### Task Checklist

- [ ] Re-rank the top five missing contracts after Cecil, Rikku, and Cloud report back.
- [ ] Keep the list focused on semantics that materially affect LLM legibility, editor-grade UI, and structural verifiability.
- [ ] Attach every remaining gap to an owner, file, dependency, and crisp acceptance signal.
- [ ] If widget-registry depth becomes the most actionable remaining gap, expand `widget_registry_schema.md` with concrete category and capability expectations.
- [ ] Feed Tifa a normalized gap summary that distinguishes "closed this wave" from "still open".
- [ ] Refuse renderer-local fake wins. If a gap only looks solved in one backend, keep it open.

### Acceptance Signal

The room can point to an updated ranked gap list and know exactly what the next truth cuts should be after this wave.

### Handoff To

- Tidus for next-wave ordering
- Balthier for seam prioritization
- Sazh for ownership slicing on the next pass

## Barret - Proof And Regression Sentry

### Objective

Keep the validation surface lean, real, and tied to the exact risky seams touched in this wave.

### Primary File

- `M:\Code\Kain\party\Barret.md`

### Deliverables

- minimal proof spine
- exact harness mapping
- ordered validation recommendation for later, without broad test execution now

### Task Checklist

- [ ] Update the minimal proof spine so it reflects the actual code cuts from Cecil, Rikku, and Cloud.
- [ ] Keep the matrix centered on reload, tabs and docking, focus, selection, overlays, event routing, computed invalidation, and canonical-vs-native parity.
- [ ] Add anchor placement, workspace identity, and emitted-surface-truth coverage if this wave materially changes those seams.
- [ ] Prefer exact file and harness references over broad "run the suite" language.
- [ ] Distinguish cheap targeted checks from heavy test passes so the room stays inside the current no-heavy-testing rule.
- [ ] Hand Tifa a one-page proof summary and hand Tidus an ordered later-validation list.

### Acceptance Signal

The room has a proof map strong enough to validate the landing later without bloating into ceremonial test theater.

### Handoff To

- Tidus for sequencing
- Tifa for merge artifact
- user, once testing is enabled, for actual execution decisions

## Tifa - Merge Normalizer

### Objective

Turn the live room's output into one compact artifact that can actually be merged, reviewed, and handed off.

### Primary File

- `M:\Code\Kain\party\Tifa.md`

### Deliverables

- one compact merge artifact
- normalized issue wording
- severity and dependency tracking
- live blocker summary

### Task Checklist

- [ ] Maintain a compact artifact with issue, severity, owner, file, dependency, current status, and next action.
- [ ] Normalize overlapping language from the room into one vocabulary so Tidus and Sazh are not merging synonyms.
- [ ] Keep the artifact current as lane outputs land instead of waiting for a perfect final pass.
- [ ] Surface blockers that need Balthier, Zidane, or Sazh intervention.
- [ ] Mark which items are code landed, note landed, proof mapped, or still open.
- [ ] Hand the artifact to Tidus for ordering and Sazh for file slicing.

### Acceptance Signal

The room can read one compact artifact and understand the whole wave without reading every party file end to end.

### Handoff To

- Tidus
- Sazh
- Vincent and Vivi for missing normalization inputs

## Tidus - Master Task List Captain

### Objective

Keep the room moving with one dependency-ordered map instead of drifting into duplicate local plans.

### Primary Files

- `M:\Code\Kain\party\Tidus.md`
- `M:\Code\Kain\party\TASKS.md`

### Deliverables

- live dependency order
- deduped room state
- implementation-wave publication
- next-cut ordering once this wave stabilizes

### Task Checklist

- [ ] Treat this file as the canonical room board and fold stale alias references back into it.
- [ ] Merge lane outputs into one dependency-ordered map that tracks what is landed, what is blocked, and what is waiting on another lane.
- [ ] Keep the room pinned to the shared mission: compiler truth, runtime authority, compatibility quarantine, and proof prep.
- [ ] Publish the next concrete edit wave as soon as Tifa and Sazh have enough material, not after every note is perfect.
- [ ] Call out when a lane is idle, blocked, or drifting and point it to the highest-value next cut.
- [ ] Keep the handoff chain crisp enough that the next person can resume from this file alone.

### Acceptance Signal

The room has one obvious place to look for current global truth, current order, and next actions.

### Handoff To

- everyone

## Sazh - Ownership Marshal

### Objective

Convert merged room output into collision-safe file and function slices so the next code wave can run fast without stomping itself.

### Primary File

- `M:\Code\Kain\party\sazh.md`

### Deliverables

- current file-claim map
- contested-file split plan
- next-wave ownership slices
- handoff rules for shared files

### Task Checklist

- [ ] Keep the ownership map current for `ui.rs`, `realtime_app_bundle.rs`, `lib.rs`, and `runtime_execution.rs`.
- [ ] Split contested `lib.rs` work by exact function or region so Rikku and Cloud can move without collisions.
- [ ] Convert Tifa's merge artifact into explicit ownership slices, dependencies, and merge order.
- [ ] Mark which work is edit-safe in parallel and which work must wait for a handoff.
- [ ] Post any boundary change immediately so Zidane can enforce it and Tidus can fold it into the master map.
- [ ] Refuse vague ownership. If a task does not have an exact file and region, bounce it back for sharpening.

### Acceptance Signal

The next edit wave can begin from explicit ownership slices instead of improvised claims in chat.

### Handoff To

- Zidane for collision enforcement
- Tidus for global ordering
- Balthier for critical-path discipline

## Balthier - Execution-Order Enforcer

### Objective

Keep the room working on the right seams in the right order so progress compounds instead of scattering.

### Primary File

- `M:\Code\Kain\party\balthier.md`

### Deliverables

- active seam priority
- critical-path reminders
- drift correction when the room starts solving the wrong problem

### Task Checklist

- [ ] Keep the room pinned to the five highest-value seams: typed routes, workspace and dock identity, explicit surface descriptors, geometry and anchor truth, and adapter posture.
- [ ] Restate the execution order whenever the room starts overbuilding lower-priority polish ahead of truth emission.
- [ ] Redirect any lane that starts inventing semantics in the wrong layer.
- [ ] Flag when a proof, doc, or cleanup lane is getting ahead of unresolved truth cuts.
- [ ] Feed Tidus the current priority order whenever the room state changes materially.
- [ ] Keep the room out of tempting off-path refactors that do not materially support the current mission.

### Acceptance Signal

The room spends its energy on the highest-value truth cuts first and does not lose a day to attractive side quests.

### Handoff To

- Tidus for room-level order
- Sazh for next-wave slices
- Zidane for overlap enforcement

## Zidane - Overlap Controller

### Objective

Prevent duplicated work, boundary collisions, and conceptual drift while the room moves in parallel.

### Primary File

- `M:\Code\Kain\party\zidane.md`

### Deliverables

- overlap warnings
- collision redirects
- current file-boundary watch

### Task Checklist

- [ ] Watch `ui.rs`, `realtime_app_bundle.rs`, `lib.rs`, and `runtime_execution.rs` for duplicate edits or overlapping ownership.
- [ ] Redirect any lane that starts stepping into another lane's active function or region.
- [ ] Watch for conceptual overlap where compatibility work starts sounding canonical or runtime cleanup starts redefining compiler truth.
- [ ] Keep a compact file-boundary watch list in your party note so the room does not rely on memory.
- [ ] Escalate contested ownership to Sazh immediately instead of letting two agents "just both touch it."
- [ ] Feed Tidus and Balthier the overlap risks that could change sequence or scope.

### Acceptance Signal

Parallel work stays parallel instead of devolving into repeated edits on the same seams.

### Handoff To

- Sazh for ownership resets
- Tidus for task-board updates
- Balthier for sequence correction

## Room Sync Protocol

Use this lightweight rhythm so the room stays fast without becoming chaotic:

1. Cecil, Rikku, Cloud, Vincent, Vivi, and Barret post lane updates into their own party files.
2. Tifa normalizes those updates into one compact merge artifact.
3. Tidus folds normalized results back into this file.
4. Sazh converts that merged view into exact next-wave ownership slices.
5. Balthier and Zidane keep order and overlap under control while the next edit wave starts.

## End-Of-Wave Required Outputs

Before calling this wave complete, the room should have:

- updated `party/*.md` lane notes for every active agent
- this file updated with current order and status
- a compatibility ledger in Vincent's note
- a ranked remaining-gap list in Vivi's note
- a minimal proof spine in Barret's note
- a compact normalized merge artifact in Tifa's note
- exact file and region slices in Sazh's note

## Current Room Order

This is the current recommended order for the highest-value cuts:

1. Cecil - truth emission
2. Rikku - semantic leak cleanup in emitted-truth-first runtime assembly
3. Cloud - compatibility and fallback seam quarantine
4. Vincent - debt ledger update
5. Vivi - gap closure and remaining-gap ranking
6. Barret - proof spine refresh
7. Tifa - merge artifact normalization
8. Tidus - room-level consolidation
9. Sazh - exact file-slice handoff
10. Balthier - sequence enforcement throughout
11. Zidane - overlap control throughout

The control lanes are always active.
The proof and merge lanes begin immediately and keep tracking live outputs in parallel.
