# Python Interop & LSP Dogfooding — Bug Report
**Date:** 2026-06-06  
**Session:** LSP VSIX packaging + Python smoke test dogfooding  
**Files:** `blades/lsp/pack_vsix.kn`, `blades/lsp/src/lsp_smoke_py.kn`, `blades/lsp/src/lsp_smoke_runner.py`

---

## Python Interop Bugs (compiler/runtime)

### 1. `python_eval` returns raw objects for complex types
**Severity:** High — blocks natural interop patterns  
**Repro:**
```kn
let val = python_eval("{'rc': 7}")
let rc = to_int(python_getattr_raw(val, "get").__call__("rc"))  // returns 0, not 7
```
- `python_eval("42")` + `to_int()` → 42 ✓ (primitives materialize)
- `python_eval("{'rc': 7}")` → raw HostObject ✗ (dicts/lists don't materialize)
- **Workaround:** Use `python_exec` to extract individual primitives into globals, then `python_eval` each global.
- **Likely location:** `crates/python/src/lib.rs` — `py_eval` native function, `py_to_value` vs `py_to_value_or_wrap_raw` path.

### 2. Tuple indexing on Python objects crashes (0xc0000005)
**Severity:** Critical — ACCESS_VIOLATION crash  
**Repro:**
```kn
let proc = sp.Popen(["cmd.exe", "/c", "echo", "hello"], stdin=sp.PIPE, stdout=sp.PIPE, stderr=sp.PIPE)
let comm = proc.communicate(timeout=15, input="x")
let stdout_bytes = comm[0]  // CRASH: 0xc0000005
```
- `proc.communicate()` itself succeeds (A,B,C all print)
- `comm[0]` on the returned Python tuple triggers the crash
- **Workaround:** Don't index Python tuples. Use `proc.stdout.read()` or `python_exec` + globals.
- **Likely location:** `crates/core/src/runtime.rs` — `[]` operator lowering for Python host objects, or `__getitem__` dispatch in the Python bridge.

### 3. `keyword_args_to_dict` doesn't accept HostObject dicts
**Severity:** Medium — blocks kwargs from `python_eval`  
**Repro:**
```kn
let kwargs = python_eval("{\"stdout\": -1, \"stderr\": -1}")
let result = py_call(sp, "run", [["cmd.exe", "/c", "echo", "hello"]], kwargs)
// kwargs silently dropped — stdout not captured
```
- `keyword_args_to_dict` in `crates/python/src/lib.rs:2287` only handles `Value::None`, `Value::Unit`, and `Value::Struct("PythonKwargs", ...)`.
- `Value::HostObject` (what `python_eval` returns for dicts) hits the `_ => Err(...)` branch.
- **Fix:** Add `Value::HostObject` case that extracts the underlying `PyDict`.
- **Workaround:** Use natural named-arg syntax (compiler lowers to `Value::Struct`), but `True`/`False` aren't available as Kain literals.

### 4. First-class `import` call syntax returns raw wrapped objects
**Severity:** High — natural syntax doesn't work for return values  
**Repro:**
```kn
import json as py_json
let text = to_string(py_json.dumps([1, 2, 3]))  // returns "2552000532251" (handle), not "[1,2,3]"
```
- Natural `alias.fn(args)` syntax works for side effects (resonate_py.kn calls `py_surface.play_note(...)` successfully).
- Return values are raw host objects — `to_string()` returns the handle/pointer, not the value.
- **Workaround:** Use `python_getattr_raw` + `python_call_raw` + `to_string()` pattern (proven in benchmarks).
- **Likely location:** `crates/core/src/runtime.rs` — the compiler lowering for first-class `import` call expressions uses `py_call_raw` (raw=true) instead of `py_call` (raw=false, materializes values).

### 5. Reserved keyword collisions with Python interop
**Severity:** Low — documented workarounds exist  
- `out` is reserved → use `output`
- `match` is reserved → use `matched`
- `type` is reserved → can't call `type(val)` directly; use `json.dumps()` roundtrip to check types
- `True`/`False` are not Kain literals → can't pass Python bool kwargs via natural syntax; use `python_call_raw` with int 1/0 instead

### 6. Module-level `var` not accessible from functions
**Severity:** Low — design limitation  
**Repro:**
```kn
var _fails: Int = 0
fn reset() -> Unit:
    _fails = 0  // error: '_fails' is not in scope
```
- Module-level `var` declarations are not visible inside function bodies.
- `const` works but is immutable.
- **Workaround:** Use local variables within `main()`, or use a `world` for shared mutable state.

---

## LSP Bugs (blades/lsp)

### 7. LSP exits rc=1 when sent >6 messages in batch
**Severity:** Medium — affects VS Code extension reliability  
**Repro:** Send 14 LSP messages (initialize → codeLens → shutdown → exit) in one `communicate()` call.
- First 6 messages get responses (id=1-5 + publishDiagnostics notification).
- Messages 7-14 (documentSymbol through exit) return no responses.
- LSP process exits with code 1.
- **Note:** The original test_lsp.py sent only 6 messages (initialize → hover → shutdown → exit) and passed with rc=0.

### 8. 5 LSP endpoints don't respond
**Severity:** Medium  
**Affected:** documentSymbol (id=6), formatting (id=7), diagnostic (id=8), codeAction (id=9), codeLens (id=11), shutdown (id=10)
- These may work individually but fail when batched after 5+ prior messages.
- Needs per-endpoint isolation testing.

### 9. Native executable build fails — missing service API symbols
**Severity:** Medium — blocks standalone .exe  
**Repro:** `kain build blades/lsp` fails at link stage.
```
lld-link: error: undefined symbol: kain_service_open_workspace
lld-link: error: undefined symbol: kain_service_check_document
... (14 total missing symbols)
```
- The LSP blade uses `std::kain` service API functions (`open_workspace`, `check_document`, `hover_at`, etc.).
- These are implemented in Rust crates (`crates/service-api`, `crates/service-bridge`) and are available in interpreter mode (`kain run`).
- The native compilation pipeline doesn't link the service API crate.
- **Workaround:** The VSIX bundles `kain.exe` and runs LSP via `kain run` (interpreter mode).

### 10. `lsp_tests.kn` had placeholder assertion
**Severity:** Trivial — fixed in this session  
```kn
assert(win_path == "incorrect", "should extract absolute Windows path")
// Fixed to:
assert(win_path == "C:/Users/zenta/project/main.kn", ...)
```

---

## Compiler / Toolchain Papercuts

### 11. `kain check` pulls in entire stdlib causing false "main collides" errors
- When checking `lsp_smoke_py.kn`, the type checker reports `function 'main' collides with an existing global from function` pointing to `stdlib/runtime.kn:1:1`.
- `runtime.kn` line 1 is `@extern` — doesn't contain `main`.
- Error may be triggered by certain patterns (large `python_exec` strings? specific import combinations?).

### 12. Multi-line `let` without parentheses doesn't work
```kn
let x = "a" +    // parse error
    "b"
// Must use:
let x = ("a" +
    "b")
```

### 13. `kain check --json` frequently times out (300s+)
- Plain `kain check` works but `--json` flag causes hangs.
- May be related to Python bridge initialization during check phase.

---

## Action Items

| Priority | Bug | Suggested Fix |
|----------|-----|---------------|
| P0 | Tuple indexing crash (#2) | Fix `__getitem__` dispatch for Python host objects in `crates/core/src/runtime.rs` |
| P0 | `python_eval` raw returns (#1) | Add materialization path for dict/list in `py_eval` native |
| P1 | Natural call raw returns (#4) | Switch first-class import lowering from `py_call_raw` to `py_call` |
| P1 | kwargs HostObject (#3) | Add `Value::HostObject` → PyDict extraction in `keyword_args_to_dict` |
| P2 | LSP rc=1 / missing endpoints (#7, #8) | Isolate each endpoint; fix batch processing robustness |
| P3 | Native link failure (#9) | Link `crates/service-api` into native executable builds |
| P3 | `kain check` false collisions (#11) | Investigate type checker symbol table for spurious collisions |
