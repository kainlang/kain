# Erlang Hot Swap And How Kain Should Evolve It

## Thesis

OTP hot code loading is not one feature. It is a three-layer system:

1. VM-level atomic code visibility swap
2. Process-level suspend/change/resume protocol
3. Release-level orchestration across applications and nodes

Kain should copy the shape, not the exact mechanism. The goal is not "be Erlang with shaders"; the goal is a stronger semantic reload system for UI, worlds, actors, and GPU resources.

## What OTP Actually Does

### 1. VM code loading is transactional and atomic

`erts` uses a prepare/finish model for module loading.

- The prepare phase parses and readies code without disturbing the running VM.
- The finishing phase swaps visibility atomically.

The key design is in [CodeLoading.md](../reference/langs/otp-master/erts/emulator/internal_doc/CodeLoading.md):

- code access structures are replicated rather than mutated in place
- a staging copy is prepared
- one atomic switch updates the active code index
- thread progress is used to ensure safe visibility without paying heavy synchronization on the fast path

Important properties:

- running processes continue during prepare
- no process sees half-loaded code
- readers stay cheap
- old/current code generations coexist briefly

### 2. OTP process hot swap is really a migration protocol

The reloadable process path lives above the VM in `sys` and OTP behaviours.

In [sys.erl](../reference/langs/otp-master/lib/stdlib/src/sys.erl):

- `change_code/4,5` sends a system message
- the process must be suspended first
- `do_change_code(...)` calls `Mod:system_code_change(Misc, Module, Vsn, Extra)`
- suspended processes sit in a system-message loop until resumed

In [gen_server.erl](../reference/langs/otp-master/lib/stdlib/src/gen_server.erl):

- `system_code_change(...)` delegates to `Mod:code_change(OldVsn, State, Extra)`

This is the heart of OTP's value: the VM swaps code visibility, but live state evolution is delegated to process-aware migration hooks.

### 3. Old/current code coexist, purge is explicit, and purge truth is machine-checked

The public semantics are documented in [code.erl](../reference/langs/otp-master/lib/kernel/src/code.erl):

- a module can have current code and old code
- both may execute concurrently
- old exported functions are no longer globally callable
- loading a third instance purges the old code and can kill lingering processes

The low-level purge detection is in [beam_bif_load.c](../reference/langs/otp-master/erts/emulator/beam/beam_bif_load.c):

- `erts_check_process_code(...)` checks whether a process still points into old code
- `check_process_code(...)` inspects instruction pointer, stack continuation pointers, and stackdump references
- purge only proceeds when lingering references are gone, or the runtime kills them

This is stronger than folklore. OTP does not merely "swap code"; it actively proves whether execution still holds references into old code.

### 4. Release handling is a separate orchestration layer

The whole-system story lives in [release_handler.erl](../reference/langs/otp-master/lib/sasl/src/release_handler.erl):

- release packages contain `.rel`, `.boot`, and optionally `relup`
- `relup` drives upgrade/downgrade instructions
- releases move through `unpacked`, `current`, `permanent`, and `old`
- upgrade/install/restart policy belongs here, not in the VM loader

This separation is excellent and Kain should keep it.

## What Kain Should Steal Directly

### 1. Dual-generation execution model

Kain should adopt OTP's strongest mental model:

- generation `N` is live
- generation `N+1` is built and staged
- both can coexist during transition
- commit is atomic at a semantic boundary

For Kain, the boundary is not only "module code." It is:

- UI tree and derived signals
- `world` state
- `actor` state and mailbox policy
- GPU resource graph / pipeline generation
- platform package bindings when relevant

### 2. Reload must be compatibility-classified, not wishful

OTP is honest about when old code still exists and when purge fails. Kain should be even more explicit by classifying reloads into lanes such as:

- `noop`
- `presentation_swap`
- `structural_migrate`
- `quiesce_and_migrate`
- `frame_boundary_gpu_swap`
- `restart_with_restore`
- `hard_incompatible`

### 3. Processes need explicit quiesce and migration hooks

OTP's `code_change` / `system_code_change` hooks are the right instinct.

Kain's version should live behind `std::reload` and be semantic rather than purely module-oriented:

- `world` migration hooks
- `actor` quiesce hooks
- mailbox/event transfer policy
- GPU frame-boundary swap hooks

## Where Kain Should Surpass OTP

### 1. Kain can migrate richer state than Erlang

OTP mostly reasons in terms of processes, modules, and callbacks.

Kain can reason in terms of:

- structural world schemas
- actor message schemas
- entangled state regions
- patch journals
- UI signal graphs
- graphics resource generations

That means Kain can make reload decisions with compiler-emitted semantic contracts instead of relying mostly on runtime conventions.

### 2. Kain can make GPU reload first-class

OTP has no equivalent of:

- frame-boundary shader swap
- render-graph compatibility classification
- resource rebind plans
- mixed-generation frame prevention

This is one of the biggest chances to build something genuinely new.

### 3. Kain can make reload inspectable and certifiable

Today the repo already has the start of that:

- [stdlib/reload.kn](../stdlib/reload.kn)
- [ARCHITECTURE.md](../ARCHITECTURE.md)
- [MEMORY.md](../MEMORY.md)
- runtime conformance under [runtime/conformance/hot_reload](../runtime/conformance/hot_reload)

The next leap is making reload a certifiable runtime transition system with attrition, not a convenience feature.

## Kain Architecture Direction

### Layer 1: runtime/kernel

Own:

- generation staging
- atomic commit
- compatibility classification
- quiesce scheduling
- restart fallback
- resource generation bookkeeping

Do not own:

- app-specific migration policy
- engine-specific scene policy

### Layer 2: `std::reload`

Own the public authoring contract:

- reload generations
- policies
- migrate hooks
- quiesce hooks
- runtime reports

This should stay opt-in and package-first, but compiler/runtime-backed.

### Layer 3: frameworks and engines

Kaintana, Vulkain, future editors, and company engines should define:

- what a reloadable UI tree means
- what a reloadable scene means
- what a reloadable resource graph means
- what state is preserved, migrated, replayed, or reset

### Layer 4: operator / release lane

Kain needs its own equivalent of `release_handler` eventually:

- package/app graph upgrade plans
- build provenance
- node/process orchestration
- rollback
- restart vs live-migrate policy

But this should be later than the core reload kernel.

## Best Kain V2 Plan

1. Keep `std::reload` as the canonical author surface.
2. Add explicit compatibility classes to the manifest/snapshot contract.
3. Add `world` structural migration plus semantic migrate hooks.
4. Add `actor` quiesce, migrate, and mailbox transfer policy.
5. Add Vulkain frame-boundary shader/pipeline/resource participation.
6. Add reload reports that explain exactly why the runtime chose live reload or restart.
7. Add attrition certification for reload.

## Attrition Should Become The Truth Machine

Hot reload should get its own attrition boss lane.

Suggested cases:

- `reload_ui_generation_stress`
- `reload_world_structural_migrate`
- `reload_actor_quiesce_mailbox_preserve`
- `reload_actor_schema_break_restart`
- `reload_gpu_frame_boundary_swap`
- `reload_mixed_epoch_rejection`

Suggested sabotage toggles:

- corrupt schema fingerprint
- stale generation replay
- duplicate mailbox transfer
- skipped actor quiesce
- mixed GPU generation present
- dropped focused-node identity
- invalid resource rebind plan

Suggested invariants:

- reload generation is strictly monotonic
- no committed event is delivered twice
- no committed event is silently lost
- incompatible schemas never enter live-migrate lanes
- actor mailbox transfer is exact-once
- no mixed-generation GPU frame is presented
- reload/restart leaves runtime closure clean

## Final Position

Erlang solved "code replacement in a live VM" beautifully.

Kain should solve the larger problem:

semantic continuity under evolution of code, state, UI, actors, and GPU resources.

So the move is:

- copy OTP's layered design
- keep OTP's honesty about incompatibility
- replace module-centric upgrade logic with compiler-emitted semantic contracts
- extend the model to worlds, actors, UI graphs, and graphics pipelines
- certify it with attrition instead of trusting demos

That is how Kain becomes "post-Erlang hot reload" instead of just re-implementing it.
