# L5 Temporal — pulse + resonate Implementation Tasks

**Date:** 2026-06-12
**Source spec:** L5_temporal.md
**Target files:** `src/parser.kn`, `src/types.kn`, `src/codegen.kn`, `src/runtime.kn`

---

## Summary

Implement pulse (timed recurrence) and resonate (reactive tripwire) in the self-host compiler. These are standalone L5 constructs with no mutual dependency but both depend on L1 world resolution for their bodies (pulse) and endpoints (resonate).

Current state: parser dispatches pulse/resonate as contextual items, emits AST nodes. Typechecker has `check_pulse_resonate_stub` that merges both and hardcodes ALL effects. Codegen has no pulse/resonate emission.

---

## Phase 1: Parser Completion

### P-PULSE-01 — Parse pulse duration (HIGH)
**File:** `parser.kn`
**Details:** Add `parse_pulse_duration(st: ParserState) -> ParseResult` helper. Reads an Int token followed by an ident token (unit). Validates unit is `ns`, `us`, `ms`, `s`, `tick`, or `ticks`. Packs into two data slots: value and unit string-table index. Returns parse result with the packed node. Error on missing int, missing unit, or invalid unit.

### P-PULSE-02 — Parse pulse interval + jitter (HIGH)
**File:** `parser.kn`
**Details:** In `parse_pulse_item`, after parsing name, expect contextual keyword `every`. Call `parse_pulse_duration` for interval. If next contextual keyword is `jitter`, call `parse_pulse_duration` again for jitter. Emit AST_ITEM_PULSE with data: `[body_idx, interval_value, interval_unit, jitter_value, jitter_unit, has_jitter_flag]`.

### P-RES-01 — Parse resonate endpoint (HIGH)
**File:** `parser.kn`
**Details:** Add `parse_resonate_endpoint(st: ParserState) -> ParseResult`. After `resonate`, parse dotted ident chain (minimum 2 segments). Reject single-segment paths. Pack segments into data array: `[count, seg0, seg1, ...]`.

### P-RES-02 — Parse resonate dampen (HIGH)
**File:** `parser.kn`
**Details:** In `parse_resonate_item`, after endpoint, if next contextual keyword is `dampen`, call `parse_pulse_duration` for dampen window. Generate synthetic name `"resonate__{world}__{field}"` from endpoint segments. Emit AST_ITEM_RESONATE with data: `[body_idx, dampen_value, dampen_unit, has_dampen_flag, endpoint_count, seg0, seg1, ...]`.

---

## Phase 2: Typechecker Implementation

### T-PULSE-01 — Dedicated check_pulse function (HIGH)
**File:** `types.kn`
**Details:** Replace the shared `check_pulse_resonate_stub` with `check_pulse(env, node, idx)`. Extract interval_value, interval_unit, jitter_value, jitter_unit from AST data. Validate interval > 0. Validate interval unit is valid (`is_valid_duration_unit`). Validate jitter > 0 if present. Reject zero and negative intervals.

### T-PULSE-02 — Pulse local injection (HIGH)
**File:** `types.kn`
**Details:** Before typechecking pulse body, inject three locals into the body scope:
- `pulse_tick: Int(I64)` — monotonic beat counter
- `pulse_dt_ms: Int(I64)` — actual elapsed time in ms
- `pulse_missed: Int(I64)` — missed beats count
Use `env.define_local(name, type)` for each. Register with `SyntaxScope` so they shadow any outer identifiers.

### T-PULSE-03 — Pulse effect auto-emission (HIGH)
**File:** `types.kn`
**Details:** The pulse body gets ALL effects auto-emitted: `pulse_body_effects()` = `EFF_PURE | EFF_IO | EFF_GPU | EFF_ASYNC | EFF_REACTIVE | EFF_UNSAFE | EFF_ALLOC | EFF_PANIC`. Set `item.effects = pulse_body_effects()` in the TypedItem.

### T-PULSE-04 — is_valid_duration_unit helper (MEDIUM)
**File:** `types.kn`
**Details:** Add `fn is_valid_duration_unit(unit: String) -> Bool`. Returns true for `"ns"`, `"us"`, `"ms"`, `"s"`, `"tick"`, `"ticks"`. Used by both pulse and resonate typechecking.

### T-RES-01 — Dedicated check_resonate function (HIGH)
**File:** `types.kn`
**Details:** Replace stub with `check_resonate(env, node, idx)`. Extract endpoint segments, dampen value/unit from AST data. Resolve the target `World.field`: validate world name exists in type env, validate field name exists on that world. Record field type for old/new value locals. Validate dampen >= 0 if present.

### T-RES-02 — Resonate local injection (HIGH)
**File:** `types.kn`
**Details:** Before typechecking resonate body, inject three locals:
- `resonate_old_i64: Int(I64)` — previous field value before store
- `resonate_new_i64: Int(I64)` — new field value after store
- `resonate_fired: Bool` — whether the handler actually fired (for dampener absorption)

### T-RES-03 — Self-feedback detection (CRITICAL)
**File:** `types.kn`
**Details:** After typechecking the resonate body, scan all mutation targets within the body. If any mutation writes to the resonate's own trigger field (same world + same field), emit error: `"resonate handler '{}' writes to its own trigger field '{}'"`. The scan must walk assignment statements (`AST_STMT_LET` with assignment to a world field path), `AST_EXPR_ASSIGN`, and any expression that calls a patch on the trigger world+field combination. Implementation: collect the `(world_name, field_name)` pair from the resonant endpoint during parsing, then in the body typecheck pass, track all `WorldName.field = expr` assignments and check against the trigger pair.

### T-RES-04 — Resonate effect auto-emission (HIGH)
**File:** `types.kn`
**Details:** Like pulse, resonate bodies get ALL effects: `pulse_body_effects()`. Set `item.effects` accordingly.

---

## Phase 3: Codegen Implementation

### C-PULSE-01 — duration_to_ns helper (HIGH)
**File:** `codegen.kn`
**Details:** Implement `fn duration_to_ns(value: Int, unit_encoding: Int) -> Int`. Unit encoding: 0=ns, 1=us, 2=ms, 3=s, 4=tick, 5=ticks. Conversion: ns → value, us → value * 1000, ms → value * 1000000, s → value * 1000000000, tick/ticks → value (platform-relative, 1ns default).

### C-PULSE-02 — Pulse body function emission (HIGH)
**File:** `codegen.kn`
**Details:** Emit `define void @__kain_pulse_body_{name}(i64 %pulse_tick_arg, i64 %pulse_dt_ms_arg, i64 %pulse_missed_arg)`. The body is compiled as a void function with three i64 params. Arguments are stored in alloca'd slots so internal references work.

### C-PULSE-03 — Pulse fire wrapper emission (HIGH)
**File:** `codegen.kn`
**Details:** Emit `define void @__kain_pulse_fire_{name}()`. Allocates three stack slots (i64). Calls `@kain_machine_pulse_snapshot(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>, i64* %tick_out, i64* %dt_out, i64* %missed_out)`. Loads returned values. Calls body function with loaded values.

### C-PULSE-04 — Pulse registration in entry preamble (HIGH)
**File:** `codegen.kn`
**Details:** In the main/setup entry preamble (after world init, before user code), emit `call i64 @kain_machine_pulse_start(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>, void ()* @__kain_pulse_fire_{name})` for each pulse. Token is a stable 64-bit hash of the pulse name.

### C-PULSE-05 — Pulse token hash (MEDIUM)
**File:** `codegen.kn`
**Details:** Implement `fn pulse_token(name: String) -> Int` as a stable hash (djb2 or FNV-1a) for slot identification.

### C-RES-01 — Resonate handler function emission (HIGH)
**File:** `codegen.kn`
**Details:** Emit `define void @__kain_resonate_{name}(i64 %resonate_old_i64, i64 %resonate_new_i64, i1 %resonate_fired)`. Body compiled as void function. Three parameters match the auto-injected locals.

### C-RES-02 — Resonate binding table (HIGH)
**File:** `codegen.kn`
**Details:** During codegen initialization, collect all resonate items. Build a lookup table mapping `(world_name, field_name)` → `ResonateBinding{target, dampen_ns, handler_name}`. This table is consulted during world field store lowering.

### C-RES-03 — World field store resonate guard (HIGH)
**File:** `codegen.kn`
**Details:** In world field store lowering, after each store instruction: emit call to `@abi_resonate_should_fire_i64(i8* %target_ptr, i64 <dampen_ns>, i64 %old_val, i64 %new_val)`. Branch on return value: if non-zero, call handler function, then call `@abi_resonate_exit(i8* %target_ptr)`. Old value must be captured before the store, new value after.

---

## Phase 4: Runtime Contract & Declares

### R-PULSE-01 — Pulse runtime declares (MEDIUM)
**File:** `runtime.kn`
**Details:** Add runtime function entries for pulse: `kain_machine_pulse_start`, `kain_machine_pulse_snapshot`, `kain_machine_pulse_stop_all`, `kain_machine_pulse_total_fire_count`. If already present, verify signatures match.

### R-RES-01 — Resonate ABI declares (MEDIUM)
**File:** `runtime.kn`
**Details:** Add extern declare entries for resonate ABI functions: `abi_resonate_should_fire_i64`, `abi_resonate_should_fire_f64`, `abi_resonate_exit`. These are declared as external LLVM functions in the generated module.

---

## Phase 5: Verification

### V-L5-01 — Pulse typecheck tests (MEDIUM)
**File:** `tests/` or inline `test fn`
**Details:** Add test cases: pulse minimal, pulse with jitter, pulse body locals, pulse negative interval (error), pulse zero interval (error), pulse bad unit (error), pulse no every keyword (error).

### V-L5-02 — Resonate typecheck tests (MEDIUM)
**File:** `tests/`
**Details:** Add test cases: resonate minimal, resonate with dampen, resonate no dampen, resonate bad target (single segment), resonate self-feedback (error), resonate shadow write (ok), resonate body locals.

### V-L5-03 — Pulse codegen tests (MEDIUM)
**File:** `codegen.kn` test output matching
**Details:** Verify pulse generates correct body function signature, fire wrapper with snapshot call, and entry preamble registration call.

### V-L5-04 — Resonate codegen tests (MEDIUM)
**File:** `codegen.kn` test output matching
**Details:** Verify resonate generates correct handler function signature, binding table, and store guard calls.
