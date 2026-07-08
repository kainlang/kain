# Todo App — Build Results Report

## File
`X:\runtime\native\src\ui_v2\stdlib\kaintana\examples\todo_app.kn`

## Results

### `kain check` — ✅ PASS
```
Total: 1, Passed: 1, Failed: 0
```
Typecheck passes cleanly with 0 errors. 972 items checked. All required LLVM capabilities confirmed:
- compiler.typed-program, ui.components, ui.runtime-bundle, world.native-ui
- memory.ownership, memory.raw-ops, memory.raw-pointers
- runtime.contract.bundle

### `kain build --target llvm` — ❌ FAIL
```
Error: Undefined: on_add
```
LLVM codegen fails because component methods referenced in JSX (`on_click={on_add}`, `value={summary_text()}`) can't be resolved at module level. The methods are defined inside the component struct but the LLVM IR lowering expects module-level function symbols.

### `kain run --target llvm` — ❌ FAIL
Same root cause — "Undefined: on_add" codegen error during LLVM native compilation.

## Root Cause

The Kain LLVM codegen cannot resolve component instance methods when they are referenced from JSX attributes (`on_click`, `value`, `checked`). The typechecker accepts method references and Self_-parameter methods in JSX, but the LLVM IR emitter looks for them as global symbols rather than struct methods.

**Affects ALL Kaintana examples** that use component methods in JSX:
- `counter.kn` — uses `on_click={on_increment}` and `value={display_text()}`
- `dashboard.kn` — uses `on_click={on_refresh}`
- `scrollable_list.kn` — no methods in JSX (pure static display)
- `todo_app.kn` — uses `on_click={on_add}`, `on_click={on_remove_N}` etc.

The `scrollable_list.kn` example works because it has zero component methods called from JSX — it's a pure static display.

## What the Todo App Demonstrates

Despite the codegen limitation, the file is architecturally complete and demonstrates:

| Feature | How |
|---------|-----|
|| **World + surface** | `TodoWorld` with `surface kaintana => TodoApp` |
| **Component state** | `_items`, `_done`, `_input` strings |
| **Pipe-delimited storage** | Items stored as `"text1|text2|text3"`, done as `"0|1|0"` |
| **String helpers** | 8 `pub fn` helpers: seg_count, seg_get, seg_set, seg_append, seg_remove, seg_build, flip_done, completed_for |
| **TextInput** | For adding new items |
| **Checkbox** | Per-item toggle (8 slots, 0–7) |
| **ProgressBar** | Completion ratio display |
| **ScrollView** | Wrapping the item list |
| **Summary count** | "Completed: X / Y" label |
| **Divider** | Visual separator |
| **Button (Add)** | Adds input text as new item |
| **Button (Remove)** | ✕ button per item to delete |
| **Null backend** | Registered for headless testing |
| **No import collisions** | Avoids `layout.kn` and `theme.kn` to bypass pre-existing pub-const collisions |

## Workaround for LLVM codegen

Component methods in JSX require the LLVM codegen to support struct-method-to-global-symbol lowering. Until this is implemented, the workaround is to:
1. Use top-level `pub fn` callbacks that communicate via session state persistence (`kt_put`/`kt_get`)
2. Store all mutable state in session state instead of component state
3. Pass the session ID via a `world` field

This is a documented compiler limitation that affects all Kaintana component demos.
