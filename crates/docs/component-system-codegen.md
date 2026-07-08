# Kain Component & JSX LLVM Codegen System

**Date:** 2026-07-05  
**Source files covered:**
- `X:\crates\sys-codegen\src\codegen_llvm\component.rs` — 2,004 lines, the main codegen file
- `X:\crates\sys-codegen\src\codegen_llvm\mod.rs` — ~22,000 lines, the LLVM generator host
- `X:\crates\sys-codegen\src\codegen_llvm\chunk-10-component_calls.tsv` — 231-line catalog of every vtable call
- `X:\runtime\native\include\component_surface.h` — the C-side ABI contract (150 lines)

---

## 1. Architecture Overview

The component codegen system emits **textual LLVM IR** that drives UI rendering through a **trait vtable** (`KainComponentSurface`). All element creation, attribute setting, state persistence, and frame lifecycle operations go through **indirect vtable calls** — never direct `abi_ui_*` function calls. This keeps the compiler backend-agnostic: it doesn't know whether the surface is `native_ui`, `web`, `viewport3d`, `headless`, or `tui`.

```
┌──────────────────────────────────────────────────────────────┐
│  Kain source                               Compiler          │
│  ┌─────────────────────────────┐            (sys-codegen)     │
│  │ component Counter:           │                             │
│  │   state count: i64 = 0      │   ──►  LLVM IR              │
│  │   fn render(self):          │         vtable calls        │
│  │     <button on_click={...}>  │                             │
│  │       "Count: {self.count}" │                             │
│  └─────────────────────────────┘                             │
│                                         │                    │
│                                         ▼                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  KainComponentSurface vtable (24 slots)                  │ │
│  │  slot 0:  session_create                                │ │
│  │  slot 2:  element_begin                                 │ │
│  │  slot 4:  element_set_text                              │ │
│  │  slot 5:  element_set_attr_i64                          │ │
│  │  slot 6:  element_set_attr_f64                          │ │
│  │  slot 7:  element_set_attr_string                       │ │
│  │  slot 8-9:   state_get/set_i64                          │ │
│  │  slot 10-12: begin_frame/end_frame/present              │ │
│  │  slot 14: should_close                                  │ │
│  │  slot 15: window_open                                   │ │
│  │  slot 16: host_pump                                     │ │
│  │  slot 17: session_attach_platform                       │ │
│  │  slot 18: get_gpu_extension                             │ │
│  │  slot 19-22: state_get/set_f64, state_get/set_string    │ │
│  │  slot 23: element_set_callback                          │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                         │                    │
│                                         ▼                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │  Runtime backends (C, `X:\runtime\native\`)              │ │
│  │  native_ui_surface  →  ui_system.c + ui_host_adapter.c  │ │
│  │  (future: web, vulkan, d3d12, tui...)                   │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### Vtable type declaration

The vtable is declared as an opaque struct with 24 uniform `i8*` slots. The actual function pointer type is resolved per call via `bitcast`:

```llvm
; component.rs line 158 — declare_surface_trait_types()
%KainComponentSurface = type {
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*,
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*,
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*
}
%KainGpuSurfaceExtension = type { i8*, i8* }
%KainComponentCallback = type void (i64, i64, i8*)*
```

External declarations emitted once per module (`component.rs` lines 161–176):
- `declare %KainComponentSurface* @kain_component_surface_resolve(i8*)` — surface registry lookup
- `declare void @kain_runtime_panic(i8*)` — fatal error handler
- `declare double @__kain_frame_delta_ms()` — high-resolution frame timer

---

## 2. `compile_surface_frame_loop` — The Surface Frame Loop

**Location:** `component.rs` lines 404–828  
**Called from:** `compile_world_initializer` in `mod.rs` line 16504, once per world surface declaration.

### Signature

```rust
pub(crate) fn compile_surface_frame_loop(
    &mut self,
    world_name: &str,
    surface_kind: &str,   // e.g. "native_ui", "shader_canvas", ...
    root_component_name: &str,
    width: i64,
    height: i64,
) -> KainResult<()>
```

### Step-by-step LLVM IR emission

#### Step 1: Resolve Surface

```llvm
; component.rs lines 425–442
%s = call %KainComponentSurface* @kain_component_surface_resolve(i8* %surface_kind_str)
%is_null = icmp eq %KainComponentSurface* %s, null
br i1 %is_null, label %null_block, label %init_block
```

If NULL → `kain_runtime_panic` + `unreachable`. The surface kind string (e.g. `"native_ui"`) is resolved at runtime through the registry populated by `kain_component_surface_register`.

#### Step 2: Create Session (vtable slot 0)

```llvm
; component.rs lines 457–466
%sid = call i64 %session_create_fn(i8* %world_name, i64 %width, i64 %height)
    ; → i64 session_id (negative = error, checked with icmp slt)
```

Session name = world name, dimensions come from component declaration or default to 1280×720.

#### Step 3: Attach Platform (vtable slot 17)

```llvm
; component.rs lines 493–501
%handle = alloca [8 x i8], align 8
call void @llvm.memset.p0i8.i64(i8* %handle, i8 0, i64 8, i1 false)
call void %attach_platform_fn(i64 %sid, i8* %handle)
```

An 8-byte, zero-initialized platform handle is stack-allocated. Backends that own their own window (Vulkan, D3D12) create it here when `hwnd` is NULL.

#### Step 4: window_open (vtable slot 15)

```llvm
; component.rs lines 508–518
%window_ok = call i64 %window_open_fn(i64 %sid, i8* %title, i64 %w, i64 %h)
```

Flags the session as open. The OS window was created by `native_ui_session_create` which auto-attaches the winit host adapter (`RegisterClassA` + `CreateWindowExA`).

#### Step 5: Slot 18 Probe — GPU vs Component path

```llvm
; component.rs lines 526–543
%ext = call i8* %get_gpu_extension_fn(i64 %sid)
%has_gpu = icmp ne i8* %ext, null
br i1 %has_gpu, label %gpu_init_block, label %component_init_block
```

This is the **runtime dispatch point** — no compile-time string matching. LLVM LTO constant-folds this branch at `-O2` because the vtable is a link-time constant global. Zero runtime cost when the backend is known at link time.

**GPU path** (lines 548–710): bitcasts the extension to `KainGpuSurfaceExtension*`, calls `load_shader` with embedded SPIR-V hex, then runs a frame loop with `begin_frame` → `set_uniform` (time, resolution, mouse) → `end_frame` → `present` → `should_close`.

**Component path** (lines 715–812): registers component-internal pulses/resonates, then runs the component frame loop.

#### Step 6: Component Frame Loop

```llvm
; component.rs lines 754–812
component_frame_loop:
  ; host_pump — process OS message queue (PeekMessage/TranslateMessage/DispatchMessageA on Win32)
  %_ = call i64 %host_pump_fn(i64 %sid)        ; slot 16

  ; begin_frame
  %delta = call double @__kain_frame_delta_ms()
  call void %begin_frame_fn(i64 %sid, double %delta) ; slot 10

  ; Render root component — calls ComponentName_render with parent_id=0 (root)
  call void @Counter_render(%KainComponentSurface* %s, i64 %sid, i64 0)

  ; end_frame + present
  call void %end_frame_fn(i64 %sid)            ; slot 11
  call void %present_fn(i64 %sid)              ; slot 12

  ; should_close — 0 = keep running, non-zero = close
  %close = call i64 %should_close_fn(i64 %sid) ; slot 14
  %keep = icmp eq i64 %close, 0
  br i1 %keep, label %component_frame_loop, label %shutdown

shutdown:
  call void %session_destroy_fn(i64 %sid)      ; slot 1
  ret void
```

### Multiple surfaces per world

Each world can declare multiple surfaces (`world.ast.surfaces`). The `compile_world_initializer` function in `mod.rs` (line 16487) iterates all surfaces and emits one `__kain_world_surface_loop_{world_name}` function per surface. Each function name is pushed to `pending_frame_loops` and called from `main()`. Each surface gets its own independent session.

### Vtable call mechanism

Every call goes through `emit_vtable_call` (`component.rs` lines 1580–1634):

```llvm
; 1. GEP into the vtable at the given offset
%gep = getelementptr inbounds %KainComponentSurface, %KainComponentSurface* %surf, i32 0, i32 %offset
; 2. Bitcast from i8** to the real function-pointer-pointer type
%cast = bitcast i8** %gep to %fn_ptr_ptr_ty*
; 3. Load the function pointer
%fn = load %fn_ptr_ty, %fn_ptr_ptr_ty* %cast
; 4. Call
%result = call %ret_ty %fn(%args)
```

The helper `emit_vtable_call_void` delegates to `emit_vtable_call` and discards the (empty) result register.

---

## 3. `compile_component_render` — How Component Render Functions Are Compiled

**Location:** `component.rs` lines 182–356  
**Called from:** `compile_component` in `mod.rs` line 15199 (which delegates to `compile_component_render`)

### Generated function signature

Every component emits a function like:

```llvm
define void @Counter_render(%KainComponentSurface* %arg0, i64 %arg1, i64 %arg2, i64 %arg3) {
  ; arg0 = surface pointer (threaded through all render calls)
  ; arg1 = session_id
  ; arg2 = parent_id
  ; arg3..N = props (in declaration order, typed)
```

When a component has multiple props, the signature expands. For example, `component Button(text: String, variant: String, disabled: bool)` becomes:

```llvm
define void @Button_render(%KainComponentSurface* %arg0, i64 %arg1, i64 %arg2, i8* %arg3, i8* %arg4, i64 %arg5)
```

### Compilation steps

1. **Declare surface trait types** — one-time guard (`surface_trait_declared` flag), emits the `%KainComponentSurface` struct, registry, panic, frame delta, and callback type declarations.

2. **Reset generator state** — clears registers, locals, scopes, allocas, string pools, etc. (`component.rs` lines 188–209).

3. **Emit stub pulse/resonate handlers** — `define internal void @__kain_pulse_fire_{name}() { ret void }`. These are replaced by top-level pulse/resonate codegen in `mod.rs`, but component-inline pulses need a stub for the registration call reference.

4. **Define the render function** — params include surface, session_id, parent_id, and all props.

5. **Store props in stack allocas** — each prop gets an `alloca` + `store` + `locals` entry (`component.rs` lines 263–275).

6. **Compile state initialization** — calls `compile_component_state_init` which reads persistent state via vtable slots 8/19/21 (`state_get_i64/f64/string`), detects first-frame via sentinel values (i64=-1, f64=NaN, string=null), and PHI-merges initial values with stored values.

7. **Emit pulse/resonate registration** — one-time guard using a sentinel state key `"{name}:__pulses_init"`. Registers `kain_machine_pulse_start` and `abi_resonate_register` on first render only.

8. **Set component context** — `current_component_name`, `current_component_methods`, `current_component_session`, `current_component_parent` are set so JSX codegen can resolve component methods and access session/parent registers.

9. **Compile JSX body** — calls `compile_jsx_to_surface(surface_reg, session_reg, parent_reg, &component.ast.body, ...)` which recursively walks the JSX tree and emits vtable calls.

10. **Write-back state** — persists current state values at the end of render via `state_set_i64/f64/string` (vtable slots 9/20/22), so mutations survive across frames.

11. **Clear context + return** — clears component context fields, emits `ret void`.

### Component methods as inline code

Component methods are NOT compiled as standalone LLVM functions. Instead, `try_inline_component_method` (see §6) compiles method bodies inline at each call site within the component's render function. This is necessary because methods access component state and locals that only exist within the render frame.

---

## 4. JSX Attribute Routing and `map_jsx_attr_to_surface_key`

**Location:** `component.rs` lines 54–103

### The `AttrMapping` struct

```rust
struct AttrMapping {
    vtable_offset: u32,     // which vtable slot
    fn_ptr_ty: &'static str, // LLVM function pointer type
    style_key: &'static str, // canonical style key passed to backend
}
```

### Complete attribute → vtable slot mapping

#### Numeric attributes → slot 6 (`element_set_attr_f64`)

| JSX Attribute | `style_key` | Notes |
|:---|:---|:---|
| `padding`, `pad` | `"padding"` | `pad` is shorthand alias |
| `spacing`, `gap` | `"spacing"` | `gap` is shorthand alias |
| `corner_radius`, `radius` | `"corner_radius"` (for `corner_radius`), `"radius"` (for `radius`) | separate keys |
| `font_size` | `"font_size"` | |
| `opacity` | `"opacity"` | |
| `border`, `border_width`, `stroke_width` | `"border_width"` | three aliases, one key |
| `width` | `"width"` | |
| `height` | `"height"` | |
| `min` | `"min"` | slider range |
| `max` | `"max"` | slider range |
| `step` | `"step"` | slider increment |

All f64 attrs share fn_ptr_ty: `"void (i64, i64, i8*, double)*"` — `(session_id, element_id, style_key, value)`.

#### String attributes → slot 7 (`element_set_attr_string`)

| JSX Attribute | `style_key` | Notes |
|:---|:---|:---|
| `background`, `fill` | `"fill_color"` | semantic aliases |
| `border_color`, `stroke` | `"border_color"` | `stroke` is SVG-aligned alias |
| `color`, `ink_color` | `"ink_color"` | semantic canonical |
| `title` | `"title"` | |
| `variant` | `"variant"` | e.g. "primary", "secondary" |
| `role` | `"role"` | accessibility |
| `align` | `"align"` | |
| `font_family` | `"font_family"` | |
| `distribution` | `"layout.distribution"` | compound key for layout engine |
| `axis` | `"axis"` | |
| `placeholder` | `"placeholder"` | input placeholder |
| `tooltip` | `"tooltip"` | |

All string attrs share fn_ptr_ty: `"void (i64, i64, i8*, i8*)*"`.

#### Integer attributes → slot 5 (`element_set_attr_i64`)

| JSX Attribute | `style_key` | Notes |
|:---|:---|:---|
| `direction` | `"layout.direction"` | compound key; string→int: "vertical"/"column"→1, "horizontal"/"row"→0 |
| `disabled` | `"disabled"` | boolean as i64 |
| `checked` | `"checked"` | |
| `selected` | `"selected"` | |
| `tab_index` | `"tab_index"` | |
| `weight` | `"weight"` | layout flex weight |

#### Text attribute → slot 4 (`element_set_text`)

| JSX Attribute | Notes |
|:---|:---|
| `value` | Bypasses key-based attribute setting; emits `element_set_text` directly |

#### Unknown attributes → fallback (slot 7)

```rust
_ => AttrMapping {
    vtable_offset: OFF_ELEMENT_SET_ATTR_STRING,
    fn_ptr_ty: "void (i64, i64, i8*, i8*)*",
    style_key: ""  // empty → use raw attr name as key
}
```

When `style_key` is empty and the attr isn't `"value"`, `compile_jsx_attr` uses the raw attribute name as the key (component.rs line 1447–1448). This enables forward compatibility — new backends can interpret any attribute name without compiler changes.

### pad/gap fix

Lines 58 and 60 of `component.rs`:

```rust
"pad" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, ..., style_key: "padding" },
"gap" => AttrMapping { vtable_offset: OFF_ELEMENT_SET_ATTR_F64, ..., style_key: "spacing" },
```

These are shorthand aliases. `pad` maps to the canonical `"padding"` style key, and `gap` maps to `"spacing"`. This means the backend sees the canonical key regardless of which alias was used in JSX.

### Type coercion in attribute compilation

`compile_jsx_attr` (lines 1429–1571) handles type coercion:
- `i1` (bool) → `zext i1 %val to i64` for i64 attrs, or `sitofp` + double literal for f64 attrs
- `i64` → `sitofp i64 %val to double` when the attribute expects f64
- `Bool(false)` → no-op (no vtable call emitted)
- `Bool(true)` → emits `1`/`1.0`/`"true"` depending on attribute type
- `Callback` → early-return to `compile_jsx_callback` (slot 23)

---

## 5. Stable Key Generation

**Location:** `component.rs` lines 1695–1735

### Purpose

Stable keys enable the backend to re-identify elements across frames. Without stable keys, the backend would need to diff the entire tree every frame. With stable keys, elements that maintain the same position in the tree get the same key and can retain internal state (focus, scroll position, animation progress).

### Format

```
ComponentName:path:parent_id:sibling_index
```

### LLVM IR emission

```llvm
; component.rs lines 1702–1734
; 1. Emit path prefix and separators as static string literals
; 2. Build step by step via runtime str_concat:

%step1 = call i8* @str_concat(i8* %prefix, i8* %colon)
        ; "Counter:button" + ":" → "Counter:button:"

%parent_str = call i8* @to_string(i64 %parent_id)
        ; 0 → "0"

%step2 = call i8* @str_concat(i8* %step1, i8* %parent_str)
        ; "Counter:button:" + "0" → "Counter:button:0"

%result = call i8* @str_concat(i8* %step2, i8* %si_str)
        ; "Counter:button:0" + ":2" → "Counter:button:0:2"
```

### Usage across JSX constructs

| JSX Node | Path prefix | Stable key |
|:---|:---|:---|
| `<tag>` element | `"{Component}:{tag}"` | `Counter:button:0:2` |
| Text node | `"{Component}:text"` | `Counter:text:0:0` |
| Expression `{expr}` | `"{Component}:text"` | `Counter:text:0:1` |
| `for` loop body | `"{Component}:{tag}"` with `child_parent = parent_reg + idx` | `Counter:item:42:0` |

For `for` loops, `child_parent` is `parent_reg + idx` (line 1284), encoding the iteration index directly into the parent ID used for children's stable keys. This ensures each iteration's children have unique, deterministic keys.

For `if`/`else` branches, each branch consumes a distinct `sibling_index` value (lines 1343–1345), so the `then` block and `else` block get different sibling indices even though only one renders.

---

## 6. Component Method Resolution: `try_inline_component_method`

**Location:** `component.rs` lines 933–1020

### Purpose

Component methods (defined inside `component { ... }` blocks) are NOT compiled as standalone LLVM functions. They must be **inlined at each call site** because they access component state locals that only exist within the render function's stack frame.

### Detection

The function matches two AST patterns:

```rust
// component.rs lines 942–964
Expr::Call { callee, args, .. } => {
    // MethodCall: method_name(args...)
    if let Expr::Ident(name, _) = callee.as_ref() { ... }
}
Expr::MethodCall { receiver, method, args, .. } => {
    // MethodCall: self.method_name(args...)
    if let Expr::Ident(receiver_name, _) = receiver.as_ref() {
        if receiver_name == "self" { ... }
    }
}
```

### `current_component_methods` context

The component's AST methods are stored in `self.current_component_methods` during render function compilation (set at line 291):

```rust
self.current_component_methods = Some(component.ast.methods.clone());
```

This is cleared at line 348 after JSX compilation completes. The context is only valid within a component's render function.

### Inlining process

1. **Find the method** — look up `method_name` in `current_component_methods` (line 972)
2. **Verify arg count** — `call_args.len() != method.params.len()` → compile error (line 978)
3. **Push scope** — new scope for method parameters (line 991)
4. **Bind args to params** — each argument is compiled and stored in a stack alloca, registered in `self.locals` under the parameter name (lines 994–1007)
5. **Compile method body** — `compile_block_with_result(&method.body)` runs inline (line 1010)
6. **Pop scope** — clean up parameter locals (lines 1013–1017)
7. **Return** — `Ok(Some((val, ty)))` on success, `Ok(None)` if not a component method

### Known limitations

1. **No `super.method()` or chained method calls** — only `self.method()` and bare `method()` patterns are detected. `self.child.method()` falls through to normal expression compilation.

2. **No method overloading/overriding** — methods are matched by name only, not by parameter types. If two methods share a name, the first one found wins.

3. **Method visibility** — all component methods are treated as available. There's no `pub`/`private` distinction on component methods.

4. **Cross-component method calls** are not resolved — `other_component.method()` is not a component method call (the receiver isn't `self`).

5. **No return type checking** — the return type is whatever `compile_block_with_result` infers. Mismatches between caller expectations and actual return type may only surface at LLVM verification time.

---

## 7. Callback Compilation: `compile_jsx_callback`

**Location:** `component.rs` lines 1379–1426

### When it's called

`compile_jsx_attr` (line 1437) detects `JSXAttrValue::Callback(_, _)` and routes it to `compile_jsx_callback` before the normal attribute mapping. The `Callback` arm in the main attribute match is `unreachable!()`.

### Generated LLVM IR

```llvm
; component.rs lines 1392–1424
; 1. Emit event kind string (e.g. "click", "change", "toggle")
%event = ; static string literal

; 2. Compile the handler expression to get a function pointer
%handler_val = ; result of compile_expr(fn_expr)

; 3. Bitcast to the canonical callback type
%callback = bitcast %handler_ty %handler_val to %KainComponentCallback
           ; %KainComponentCallback = void (i64, i64, i8*)*

; 4. Bitcast to void* for storage in the vtable
%callback_i8 = bitcast %KainComponentCallback %callback to i8*

; 5. element_set_callback (vtable slot 23)
;    void (i64, i64, i8*, void*)*
call void %set_callback_fn(i64 %sid, i64 %element_id, i8* %event_name, i8* %callback_i8)
```

### Callback signature

The C runtime callback type (`component_surface.h` line 127) is:

```c
void (*element_set_callback)(int64_t session_id, int64_t element_id,
                              const char* event_name, void* callback_fn);
```

The callback function itself is expected to have signature `void (i64, i64, i8*)*` — receiving `session_id`, `element_id`, and an opaque `event_data` pointer. The backend invokes the callback when the event fires.

### How callbacks are authored in Kain

```kain
component Counter:
    state count: i64 = 0

    fn render(self):
        <button on_click={self.increment}>
            "Count: {self.count}"

    fn increment(self):
        self.count = self.count + 1
```

The `on_click={self.increment}` compiles to:
1. `event_name` = `"click"` (the `on_` prefix is stripped, the rest is the event kind)
2. `fn_expr` = the expression `self.increment`, which `compile_expr` resolves to the function pointer for `increment`

The function pointer for `increment` must be available at the JSX call site. Currently, component methods are only inlined (see §6), so a callback referencing a component method would need to produce a function pointer — this area has known limitations with closures and method references.

---

## 8. Component Call Compilation: `compile_jsx_component_call`

**Location:** `component.rs` lines 1121–1213

When JSX encounters `<ChildComponent prop="val" />`, it compiles to a direct function call:

```llvm
; First, declare the child component's render function if not already defined
declare void @ChildComponent_render(%KainComponentSurface*, i64, i64, i8*, ...)

; Then call it
call void @ChildComponent_render(%KainComponentSurface* %arg0, i64 %arg1, i64 %arg2, i8* %prop_val, ...)
```

### Prop resolution

Props are passed in **declaration order** from the child component's definition, not the order they appear in JSX. Missing props get zero/empty defaults via `zero_value_for_ty`:

```rust
// component.rs line 1168
let zero = self.zero_value_for_ty(_prop_ty);
```

### Children after component calls

Children of a component call are compiled as **siblings under the same parent** after the component's render call completes (lines 1202–1210). This ensures tree ordering across component boundaries:

```kain
<Parent>
    <Child>  <!-- renders some elements under parent -->
        <!-- these children go after Child_render(), under the same parent -->
```

---

## 9. State Persistence

**Location:** `component.rs` lines 1741–1916 (`compile_component_state_init`) and lines 306–343 (write-back)

### Three type lanes

| State Type | Get Slot | Set Slot | Sentinel (first frame) |
|:---|:---|:---|:---|
| `i64` | 8 (`state_get_i64`) | 9 (`state_set_i64`) | `-1` (was `0`, changed to fix collision with valid zero) |
| `f64` | 19 (`state_get_f64`) | 20 (`state_set_f64`) | `NaN` (`fcmp uno` against NaN bit pattern) |
| `String` | 21 (`state_get_string`) | 22 (`state_set_string`) | `null` pointer |

### First-frame detection and PHI merge

For each state field, the codegen emits:

```llvm
; component.rs lines 1761–1840
; 1. Get stored value from surface
%stored = call double %state_get_f64_fn(i64 %sid, i8* %key_str)  ; slot 19

; 2. Check sentinel
%is_first = fcmp uno double %stored, 0x7FF8000000000000          ; NaN check

; 3. Branch
br i1 %is_first, label %init_block, label %load_block

init_block:
  ; Compile initial value expression, store via state_set
  call void %state_set_f64_fn(i64 %sid, i8* %key_str, double %init_val)  ; slot 20
  br label %load_block

load_block:
  ; PHI merge: on first frame use init_val; on subsequent frames use stored_val
  %phi = phi double [ %init_val, %init_block ], [ %stored, %entry ]
  store double %phi, double* %state_addr
```

### Write-back at end of render

After the JSX body is compiled, the current values in the state allocas are written back to the surface (lines 309–343). This is what makes `self.count = self.count + 1` work — the mutated value is persisted via `state_set_i64`.

---

## 10. JSX Control Flow: `compile_jsx_for` and `compile_jsx_if`

### `compile_jsx_for` (lines 1216–1312)

```llvm
; 1. Evaluate iterable → runtime array handle (i8*)
%iter = call i8* @runtime_array_...

; 2. Get length
%len = call i64 @runtime_array_len(i8* %iter)

; 3. Loop setup: alloca idx = 0, br to header
loop_header:
  %idx = load i64, i64* %idx_ptr
  %done = icmp sge i64 %idx, %len
  br i1 %done, label %loop_done, label %loop_body

loop_body:
  %item = call i8* @runtime_array_get(i8* %iter, i64 %idx)
  store i8* %item, i8** %item_addr
  ; child_parent = parent_reg + idx (encodes iteration in stable keys)
  %child_parent = add i64 %parent_reg, %idx
  ; ... compile body under child_parent ...
  %next_idx = add i64 %idx, 1
  store i64 %next_idx, i64* %idx_ptr
  br label %loop_header

loop_done:
```

### `compile_jsx_if` (lines 1315–1374)

Standard LLVM if-else with branch merging. Each branch consumes a distinct `sibling_index` value so stable keys don't collide between branches:

```llvm
%is_true = icmp ne i64 %cond, 0
br i1 %is_true, label %then_block, label %else_block

then_block:
  ; compile then_branch with sibling_index 3
  br label %done_block

else_block:
  ; compile else_branch with sibling_index 4
  br label %done_block

done_block:
```

---

## 11. Integration Points in `mod.rs`

### Type registration (mod.rs line 13525)

During `compile_typed_items`, `TypedItem::Component` populates `self.component_defs`:

```rust
TypedItem::Component(component) => {
    let mut props = Vec::new();
    for prop in &component.ast.props {
        props.push((prop.name.clone(), self.map_type(res_ty)));
    }
    self.component_defs.insert(component.ast.name.clone(), props.clone());
    self.functions.insert(format!("{}_render", component.ast.name), "void".to_string());
}
```

This is used by `compile_jsx_component_call` to look up prop types and emit forward declarations for cross-module component renders.

### Shader SPIR-V collection (mod.rs line 14429)

Before compiling items, `collect_shader_spirv_hexes` walks world surface declarations and compiles shader surfaces (kind `"shader_canvas"`) to SPIR-V hex via `gpu::compile_fragment_to_spirv_hex`. The hex is embedded as a global:

```llvm
@__kain_spirv_MyShader = private unnamed_addr constant [N x i8] c"hex...", align 8
```

This is referenced by the GPU path in `compile_surface_frame_loop` at line 576.

### World initializer (mod.rs line 16487)

For each world surface declaration, `compile_world_initializer` calls `compile_surface_frame_loop` and records the function name in `pending_frame_loops` for calling from `main()`.

---

## 12. Vtable Slot Reference

Full 24-slot layout matching `component_surface.h`:

| Slot | Offset Constant | Function | Signature |
|:---|:---|:---|:---|
| 0 | `OFF_SESSION_CREATE` | `session_create` | `i64 (i8*, i64, i64)*` |
| 1 | `OFF_SESSION_DESTROY` | `session_destroy` | `void (i64)*` |
| 2 | `OFF_ELEMENT_BEGIN` | `element_begin` | `i64 (i64, i64, i8*, i8*)*` |
| 3 | `OFF_ELEMENT_END` | `element_end` | `void (i64, i64)*` |
| 4 | `OFF_ELEMENT_SET_TEXT` | `element_set_text` | `void (i64, i64, i8*)*` |
| 5 | `OFF_ELEMENT_SET_ATTR_I64` | `element_set_attr_i64` | `void (i64, i64, i8*, i64)*` |
| 6 | `OFF_ELEMENT_SET_ATTR_F64` | `element_set_attr_f64` | `void (i64, i64, i8*, double)*` |
| 7 | `OFF_ELEMENT_SET_ATTR_STRING` | `element_set_attr_string` | `void (i64, i64, i8*, i8*)*` |
| 8 | `OFF_STATE_GET_I64` | `state_get_i64` | `i64 (i64, i8*)*` |
| 9 | `OFF_STATE_SET_I64` | `state_set_i64` | `void (i64, i8*, i64)*` |
| 10 | `OFF_BEGIN_FRAME` | `begin_frame` | `void (i64, double)*` |
| 11 | `OFF_END_FRAME` | `end_frame` | `void (i64)*` |
| 12 | `OFF_PRESENT` | `present` | `void (i64)*` |
| 13 | `OFF_POLL_EVENT` | `poll_event` | `i64 (i64, void*, i64)*` |
| 14 | `OFF_SHOULD_CLOSE` | `should_close` | `i64 (i64)*` |
| 15 | `OFF_WINDOW_OPEN` | `window_open` | `i64 (i64, i8*, i64, i64)*` |
| 16 | `OFF_HOST_PUMP` | `host_pump` | `i64 (i64)*` |
| 17 | `OFF_SESSION_ATTACH_PLATFORM` | `session_attach_platform` | `void (i64, i8*)*` |
| 18 | `OFF_GET_GPU_EXTENSION` | `get_gpu_extension` | `i8* (i64)*` |
| 19 | `OFF_STATE_GET_F64` | `state_get_f64` | `double (i64, i8*)*` |
| 20 | `OFF_STATE_SET_F64` | `state_set_f64` | `void (i64, i8*, double)*` |
| 21 | `OFF_STATE_GET_STRING` | `state_get_string` | `i8* (i64, i8*)*` |
| 22 | `OFF_STATE_SET_STRING` | `state_set_string` | `void (i64, i8*, i8*)*` |
| 23 | `OFF_ELEMENT_SET_CALLBACK` | `element_set_callback` | `void (i64, i64, i8*, i8*)*` |

---

## 13. Key Design Decisions

1. **Indirect vtable calls only** — No `abi_ui_*` direct calls. Every surface operation goes through `getelementptr` → `bitcast` → `load` → `call`. This enables drop-in backend substitution: swap `native_ui` for `web` without recompiling.

2. **Uniform `i8*` vtable slots** — The vtable is declared as 24× `i8*` rather than 24 distinct function pointer types. Bitcast resolves the actual type per call site. This avoids LLVM type system friction with function pointer subtyping.

3. **Runtime GPU dispatch (slot 18)** — The GPU vs component path is decided at runtime by probing `get_gpu_extension`, not by compile-time string matching. LLVM LTO constant-folds this when the backend is known at link time.

4. **Component methods are inlined** — Methods are not compiled as standalone functions because they access frame-local state. They are compiled inline at each call site via `try_inline_component_method`.

5. **State sentinels for first-frame detection** — Each type lane uses a distinct sentinel: i64=-1, f64=NaN, string=null. The i64 sentinel was changed from 0 to -1 (component.rs line 1813 comment) to fix a bug where valid state value 0 was treated as first-frame.

6. **Stable keys for element identity** — Every element gets a deterministic stable key built from component name, path, parent ID, and sibling index. This enables stateful backends to re-identify elements across frames.

7. **One-time pulse/resonate registration** — Uses a state key sentinel (`"{name}:__pulses_init"`) to guard against re-registering every frame, which would reset timers.
