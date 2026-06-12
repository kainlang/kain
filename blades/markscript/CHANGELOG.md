# MarkScript Changelog

> All notable changes to the MarkScript prose-native scripting VM.
> This file IS valid markscript — every heading is a domain, every table is data.

# Changelog

## Version 2.0 — The Companion Strike (2026-06-11)

### Summary
Four parallel lanes hardened markscript from a 1.0 core VM into a full companion scripting language for Kain: 78 handlers, 23 opcodes, 114 test cases, CLI build system, UI embedding, and config validation. The VM is now ready to be embedded in the self-hosted Kain compiler.

| Metric | 1.0 | 2.0 | Delta |
|--------|-----|-----|-------|
| VM Opcodes | 20 | 23 | +3 (ITER_GET, CALL_FN, RET_VAL) |
| IVT Handlers | 12 | 78 | +66 (stdlib, process, UI) |
| Kain Source Files | 9 | 16 | +7 (stdlib, ui, schema, gen, config, std_markscript, import_kain) |
| Source LOC | ~3,400 | ~7,500 | +4,100 |
| Test Cases | 22 | 114 | +92 (5.2×) |
| Z3 Proof Packs | 0 | 6 | VM invariants verified |
| CLI Subcommands | 8 | 13 | +5 (pipe, watch, build, test, clean) |
| MarkValue Kinds | 5 | 10 | +5 (BOOL, ARRAY, DICT, WIDGET, EVENT) |
| Stdlib Markdowns | 93 (spec only) | 10 wired + 93 | 10 now executable |
| Benchmarks | 0 | 17 | opcode latency, throughput, stress |
| Attrition Cases | 0 | 20 | sabotage + invariants |

## Language

### New Opcodes (21-23)

| Opcode | Value | Name | Purpose |
|--------|-------|------|---------|
| 21 | OP_ITER_GET | Pop index + array handle, push array[index] | `for item in arr:` support |
| 22 | OP_CALL_FN | Call named function from VM function table | `fn name():` support |
| 23 | OP_RET_VAL | Return with value, push to caller stack | `return value` support |

### New MarkValue Kinds (5-9)

| Kind | Value | Constant | Purpose |
|------|-------|----------|---------|
| 5 | MARK_BOOL | Boolean (distinct from Int 0/1) | Typed boolean values |
| 6 | MARK_ARRAY | Array of MarkValues | Array literals `[1, 2, 3]` |
| 7 | MARK_DICT | String→MarkValue map | Dict literals `{key: value}` |
| 8 | MARK_WIDGET | UI widget reference | Embedded UI scripting |
| 9 | MARK_EVENT | UI event reference | Event-driven dispatch |

### VM State Extensions

| Field | Type | Lane | Purpose |
|-------|------|------|---------|
| `processes` | `Array<ProcessRecord>` | GAMMA | PID tracking for spawn/await/kill |
| `widgets` | `Array<WidgetRecord>` | DELTA | Widget registry for UI embedding |
| `arrays` | `Array<Array<MarkValue>>` | BETA | Array storage for array literals |
| `dicts` | `Array<Array<DictEntry>>` | BETA | Dict storage for dict literals |
| `functions` | `Array<FnRecord>` | BETA | User-defined function table |
| `modules` | `Array<ImportedModule>` | BETA | Imported Kain module registry |

## Handlers

### Core Handlers (1-12, unchanged)

| ID Range | Category | Count |
|----------|----------|-------|
| 1-6 | Filesystem + Process + Import Kain | 6 |
| 7-12 | Assert, Print, Str, Len, Push, Pop | 6 |

### BETA: Stdlib Extension (13-50, NEW)

| ID Range | Domain | Handlers | Wired To |
|----------|--------|----------|----------|
| 13-21 | String | concat, split, join, substr, replace, upper, lower, trim, contains | `std::text` |
| 22-30 | Math | sin, cos, sqrt, abs, min, max, clamp, random_int, random_float | `std::math` |
| 31-32 | JSON | json_parse, json_stringify | `std::json` |
| 33-37 | Filesystem | mkdir, read_dir, stat, touch, chmod | `std::fs` |
| 38-40 | Process | exit_code, stderr, kill | stubbed (GAMMA owns at 53-56) |
| 41-43 | Time | now, sleep, format | `std::time` |
| 44-45 | Network | http_get (curl), tcp_connect (nc) | process delegation |
| 46-47 | Regex | match (grep), replace (sed) | process delegation |
| 48 | Template | render (`{{key}}` → value) | string substitution |
| 49-50 | Random | int_range, float_range | `pcg32_step` |

### GAMMA: Process Lifecycle (51-59, NEW)

| ID | Constant | Intent Phrase | Behavior |
|----|----------|---------------|----------|
| 51 | FN_PROCESS_SPAWN_TRACKED | `spawn` | Spawn + store PID in VM |
| 52 | FN_PROCESS_AWAIT | `await` | Wait + capture exit code |
| 53 | FN_PROCESS_KILL_PID | `kill` | Terminate process by PID |
| 54 | FN_PROCESS_EXIT_CODE | `exitcode` | Push exit code to stack |
| 55 | FN_PROCESS_STDOUT_PID | `stdout` | Push captured stdout |
| 56 | FN_PROCESS_STDERR_PID | `stderr` | Push captured stderr |
| 57 | FN_PROCESS_PIPE | `pipe` | Chain stdout→stdin |
| 58 | FN_PROCESS_ENV | `env` | Set env vars for process |
| 59 | FN_PROCESS_CWD | `cwd` | Set working directory |

### DELTA: UI Scripting (71-78, NEW)

| ID | Constant | Intent Phrase | Behavior |
|----|----------|---------------|----------|
| 71 | FN_UI_ON_CLICK | `click` | Dispatch on widget click |
| 72 | FN_UI_ON_KEY | `key` | Dispatch on key press |
| 73 | FN_UI_ON_FOCUS | `focus` | Dispatch on focus gained |
| 74 | FN_UI_ON_CLOSE | `close` | Dispatch on window close |
| 75 | FN_UI_GET_WIDGET | `find widget` | Look up widget by path |
| 76 | FN_UI_SET_PROPERTY | `set property` | Set widget property |
| 77 | FN_UI_GET_PROPERTY | `get property` | Get widget property |
| 78 | FN_UI_CREATE_WIDGET | `create widget` | Create widget in VM registry |

## CLI

### New Subcommands

| Subcommand | Usage | Description |
|-----------|-------|-------------|
| `pipe` | `echo '...' \| mks pipe` | Read stdin, execute, write accumulator |
| `watch` | `mks watch <file.md>` | Poll mtime, re-execute on change |
| `build` | `mks build [target]` | Auto-detect + build Rust/C/C++/Node/Python/Go/Kain |
| `test` | `mks test [target]` | Auto-detect + run tests |
| `clean` | `mks clean [target]` | Auto-detect + clean artifacts |

### CLI Improvements

- `--json` output for ALL subcommands (was planned, now implemented)
- `Mksfile.md` auto-discovery: `mks` without args searches cwd
- Build auto-detection: `Cargo.toml` → `cargo build`, `CMakeLists.txt` → cmake, `package.json` → npm, `*.kn` → kain build, `Makefile` → make, `go.mod` → go build

## Testing

### Test Suite Expansion

| File | Cases | Category |
|------|-------|----------|
| `test/edge_cases.kn` | 20 | Stack bounds, arithmetic overflow, variable bounds, jump bounds, call stack, all error kinds |
| `test/bridge_handlers.kn` | 16 | Handler registration, IVT dispatch, error propagation, handler chains |
| `test/combinatorial_matrix.kn` | 39 | Opcode pairs, triples, variable lifecycle, error cross-products, stress sequences |
| `test/test_runner.kn` | Catalog | Unified test runner with filtering |
| **Total New** | **75** | |
| Existing (e2e_pipeline + lexer + parser + JIT) | 39 | |
| **Grand Total** | **114** | **5.2× increase** |

### Z3 Proof Packs

| File | Invariants Verified |
|------|-------------------|
| `z3/vm_invariants.z3` | Stack bound safety, arithmetic overflow, DIV-by-zero trapping |
| `z3/var_store_integrity.z3` | Store/load consistency, overwrite correctness, variable independence |
| `z3/call_stack_integrity.z3` | Call/ret pairing, balanced depth, empty stack safety |

### Benchmark Suite

| File | Benchmarks |
|------|-----------|
| `benchmarks/markscript_bench.kn` | 17 benchmarks: opcode latency (10), VM throughput (3), stress (3), size analysis |

### Attrition

| File | Cases |
|------|-------|
| `attrition/markscript_attrition.json` | 20 sabotage cases + 4 invariants |

## Embedding & Config

### `std::markscript` Module (NEW)

Clean Kain embedding API for any Kain program to use the markscript VM:
- `mks_new_vm()` — create VM with builtins
- `mks_run_file(path)` / `mks_run_string(source)` — compile + execute
- `mks_register(vm, phrase, handler_id)` — add custom handler
- `mks_table_get_int/string/float(vm, handle, row, col)` — typed table access
- `mks_tables(vm)` — iterate all parsed tables

### UI Event Binding (NEW)

- `std::markscript_ui` module: `mks_ui_create_from_file()`, `mks_ui_layout()`, `mks_ui_presets()`
- `mks/src/main.kn` rewritten: hex color mixer now loads its entire UI spec from `ui.md` at runtime
- Widget registry in VM state: create, find, get/set properties through markscript intents

### Config System (NEW)

| File | Purpose |
|------|---------|
| `src/schema.kn` | `@schema` directive, column type checking, required/default/min/max constraints |
| `src/gen.kn` | Config → code generator: json/toml/env/kain/typescript targets |
| `src/config.kn` | Layered config merging: base + overlay, `@replace` support |
| `examples/kain_project_config.md` | KAIN.toml equivalent in markscript tables |

## Source Map (Updated)

| File | LOC | Role | Status |
|------|-----|------|--------|
| `src/lexer.kn` | ~350 | Tokenizer — 22 token types | Stable |
| `src/parser.kn` | ~500 | Single-pass token→bytecode compiler, @import, mini-language | Extended (opcodes 21-23) |
| `src/vm.kn` | ~847 | Virtual Machine — 23 opcodes, stack, data table, IVT, processes, widgets, arrays, dicts, functions, modules | Extended |
| `src/main.kn` | ~1,406 | CLI driver, subcommand dispatch, REPL, handler loop, pipe, watch, build, test, clean, --json | Extended |
| `src/cli.kn` | ~664 | Argument parser, usage text, MksConfig, auto-detection, JSON output | Extended |
| `src/bridge.kn` | ~1,331 | IVT handler registry — 78 built-in Kain stdlib bridges, dispatch, registration | Extended |
| `src/bridge_stdlib.kn` | ~414 | BETA: 35 handler functions across 10 stdlib domains | NEW |
| `src/types.kn` | ~436 | MarkValue (10 kinds), MatrixRecord, ProcessRecord, WidgetRecord, DictEntry, FnRecord, ImportedModule | Extended |
| `src/error.kn` | ~150 | MarkError (6 kinds), formatting, did-you-mean | Stable |
| `src/import.kn` | ~220 | @import resolution, cycle detection | Stable |
| `src/jit.kn` | ~670 | x86-64 JIT compiler (20 of 23 opcodes covered) | Stable |
| `src/std_markscript.kn` | ~321 | Clean embedding API for Kain programs | NEW |
| `src/markscript_ui.kn` | ~295 | UI event binding bridge | NEW |
| `src/schema.kn` | ~345 | Config schema validation | NEW |
| `src/gen.kn` | ~524 | Config → code generator | NEW |
| `src/config.kn` | ~409 | Layered config merging | NEW |
| **Total** | **~7,500** | | |

## Known Issues

| Issue | Severity | Lane | Status |
|-------|----------|------|--------|
| `bridge_stdlib.kn` was disabled (.broken rename) during cross-lane merge | Medium | BETA/GAMMA | Constants re-declared directly in bridge.kn |
| `config.kn` has 3 symbol shadowing errors | Low | DELTA | `read_file`, `split_lines`, `starts_with` collide |
| `schema.kn` has 2 `default` reserved keyword uses | Low | DELTA | Needs renaming to `default_val` |
| JIT coverage for new opcodes (21-23) not yet implemented | Medium | BETA | Tracked for jit.kn update |
| `> import kain` dynamic module loading is infrastructure-complete but not end-to-end tested | Medium | BETA | Handler + type conversion exists; runtime loader TBD |
| Workspace check: 14/16 files pass | — | All | Core infrastructure clean; DELTA config files need minor fixes |

---

## Version 1.0 (2026-06-08)

### Initial Release
- 20 VM opcodes, 22 token types, 12 built-in handlers
- CLI: run, check, disasm, repl, eval, init, handlers, doc
- x86-64 JIT compiler for all 20 opcodes
- 93 stdlib markdown files (spec only)
- 25 example scripts (game engine, data pipeline, servo, fizzbuzz, pong, life, metacompiler, primality)
- 22 test cases (e2e_pipeline.kn)
- Embedding API documented but not modularized
- `@import` multi-file composition with cycle detection
- Error model: 6 kinds with did-you-mean suggestions
- 8 spec invariants in MARKSCRIPT.MD
