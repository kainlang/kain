# Component — The Kain UI Primitive

> *"Component is the language's answer to 'how do I build a screen?' — a first-class, compiler-owned, world-independent UI abstraction with typed props, local state, methods, JSX composition, GPU support, and 24-slot vtable ABI."*

**Last updated:** 2026-06-26 (24-slot vtable, pulse/resonate in components, f64/String state, event callbacks, dimensions)
**Based on:**
- `crates/core/src/ast.rs:564-635` — Component, StateDecl, JSXNode, JSXAttribute AST nodes
- `crates/core/src/parser.rs:2596-2811` — `parse_component()`, `parse_component_dimensions()`, pulse/resonate parsing
- `crates/core/src/types.rs` — `check_component()`, typed state fields, callback validation
- `crates/sys-codegen/src/codegen_llvm/component.rs` — vtable offsets 0-23, JSX→vtable lowering, state persistence, callback codegen
- `runtime/native/include/component_surface.h` — `KainComponentSurface` 24-slot vtable struct
- `runtime/native/src/ui/native_ui_surface.c` — GDI backend vtable implementation
- `runtime/native/src/ui/research/MASTER_DOC.md` — KUIF master plan, vision, architecture
- `docs/RULEBOOK.md` — Decision ladder, component's place at Layer UI
- `docs/KAIN_BY_EXAMPLE.md` — Canonical syntax reference

---

## 1. What Is a Component?

A **component** is a top-level declaration that defines a reusable UI widget with typed props, local state, methods, JSX render body, inline pulse/resonate clocks, event callbacks, and an optional explicit dimension override. It is Kain's native answer to React/SwiftUI components — the language-level primitive for building user interfaces.

```kain
component Counter(initial: Int, label: String):
    state count: Int = initial

    fn label_text(_self: Self_) -> String:
        return _self.label + ": " + str(_self.count)

    render <box>
        <text value={label_text()} />
    </box>
```

### Key Properties

| Property | Description |
|----------|-------------|
| **Self-contained** | Owns its render tree, state, methods, pulse/resonate clocks |
| **Composable** | Calls other components via uppercase JSX tags |
| **Compiler-owned rendering** | JSX lowers to vtable calls on `KainComponentSurface` (24-slot ABI) |
| **Free-standing** | Does NOT require a `world` to exist |
| **World-bindable** | Any world can project a component via `surface native_ui => Component` |
| **Multi-surface** | Same component renders on `native_ui`, `web`, `viewport3d`, `ue5` |
| **Stateful** | `i64`, `f64`, and `String` state fields persist across frames via vtable slots |
| **Temporal** | Inline `pulse` clocks and `resonate` tripwires inside component bodies |
| **Interactive** | Event callbacks (`on_click`, `on_change`, etc.) via vtable slot 23 |
| **GPU-capable** | `with GPU` + `shader` support via vtable slot 18 (`get_gpu_extension`) |

### The Full Contract

```kain
component Name(props...):
    state field: Type = initial
    state another: Type = initial
    fn method(_self: Self_) -> Return:
        // read/write _self.field, _self.props
        return value
    pulse tick every 16ms:
        _self.count = _self.count + 1
    resonate World.field dampen 0ms:
        _self.display = resonate_new_i64
    render <jsx>...</jsx>
```

| Section | Required | Description |
|---------|----------|-------------|
| `(props)` | No | Typed parameters received from JSX caller |
| `state` | No | Local mutable state (i64/f64/String), persists across frames |
| `fn` | No | Methods with `_self: Self_` to read/write state and props |
| `pulse` | No | Timed recurrence clock inside the component body |
| `resonate` | No | Reactive tripwire on world field changes |
| `effects` (`with ...`) | No | `Reactive`, `Unsafe`, `GPU`, `Pure` |
| `dimensions` (`width=... height=...`) | No | Override default window size |
| `render` | **Yes** | The JSX tree this component produces |

---

## 2. The Decision Ladder — Where Component Fits

Component sits at **Layer UI** — above the 7-layer semantic stack but alongside the "what kind of thing am I building?" gate:

```
"Am I crossing into C/OS?"       → include ... as ...
"Is this Python host code?"      → import ...
"Is this a GPU kernel?"          → shader compute
"Is this a UI component?"        → component          ← THIS
───────────────────────────────────────────────────
LAYER 7: "Concurrent state?"     → actor
         "Raw memory lifecycle?" → collapse/observe/decay
LAYER 6: "Capability assumption?"→ axiom / shatter / teleport
LAYER 5: "Timed recurrence?"     → pulse / resonate
LAYER 4: "Multi-stage pipeline?" → orchestrate
LAYER 3: "Spec + fast lanes?"    → converge
LAYER 2: "Journaled mutation?"   → patch / law
LAYER 1: "Global named state?"   → world / entangle
LAYER 0: "None of the above?"    → fn, struct, let
```

**Decision rule for UI:**

```
"Am I building UI?"
  ├── Is it a single reusable widget with its own state?       → component
  ├── Is it application state behind the UI?                   → world + surface => Component
  ├── Is it an interactive widget framework?                   → component + world + resonate + patch
  ├── Is it a layout / theme layer?                            → component (HStack/VStack/Grid)
  └── Is it computation that feeds the UI?                     → fn (called from component {expr})
```

### What Component IS

- A **reusable UI widget** — `component Button`, `component Dashboard`
- A **typed props receiver** — `component Card(title: String, body: String)`
- A **state holder** — `state hovered: Bool = false`
- A **composition unit** — `<Button />` calls another component
- A **render tree factory** — `render <box>...</box>`
- A **world surface** — `surface native_ui => Component` wires world to component
- A **first-class language declaration** — parsed as `ast::Component`, not a struct wrapper
- An **effect-capable** declaration — `with Reactive`, `with GPU`, `with Unsafe`
- A **pulse/resonate host** — inline temporal clocks and reactive tripwires
- A **pointer-aware container** — components can manage raw memory, GPU buffers

### What Component IS NOT

- **NOT a world** — components have no surfaces, no entangle, no patch, no law of their own
- **NOT an actor** — components have no mailbox, no `on` handlers, no `spawn`/`send`/`ask`
- **NOT a struct** — cannot be instantiated via `let x = ComponentName(...)`, only via `<ComponentName />` in JSX
- **NOT a function** — `render` is declarative, not imperative
- **NOT a framework** — `component` is the primitive; Kaintana is a framework built on top
- **NOT bound to one surface** — same component can appear on `native_ui`, `web`, `viewport3d`

---

## 3. Component Declaration — Full Syntax

### 3.1 The Header

```kain
component Name(prop_a: Type, prop_b: Type = default) with Effect1, Effect2 width=expr height=expr:
```

- **`component`** — keyword
- **`Name`** — PascalCase identifier (must not shadow builtin types like `Void`)
- **`(props)`** — typed parameter list with optional defaults
- **`with Effect1, Effect2`** — optional effect annotations
- **`width=expr, height=expr`** — optional explicit window dimensions (overrides platform defaults)

### 3.2 The Body

The component body contains zero or more of:
1. `state field: Type = initial` — mutable state fields (i64/f64/String)
2. `fn method(_self: Self_) -> Return:` — methods with self access
3. `pulse name every Nms [jitter Nms]:` — inline temporal clocks
4. `resonate World.field [dampen Nms]:` — inline reactive tripwires
5. `render <jsx>` — exactly one render block

### 3.3 Complete Example

```kain
component Slider(value: Float = 0.5, min: Float = 0.0, max: Float = 1.0, step: Float = 0.01)
        with Reactive width=400 height=80:

    state dragging: Bool = false
    state thumb_pos: Float = value

    fn compute_ratio(_self: Self_) -> Float:
        if _self.max <= _self.min:
            return 0.0
        return (_self.thumb_pos - _self.min) / (_self.max - _self.min)

    fn display_value(_self: Self_) -> String:
        let pct = compute_ratio() * 100.0
        return str(round(pct)) + "%"

    fn on_pointer_down(_self: Self_, x: Float):
        _self.dragging = true
        _self.thumb_pos = clamp(x, _self.min, _self.max)

    fn on_pointer_up(_self: Self_):
        _self.dragging = false

    pulse thumb_decay every 32ms:
        if _self.dragging == false:
            _self.thumb_pos = _self.thumb_pos * 0.95 + value * 0.05

    render <box>
        <text value={display_value()} />
    </box>
```

---

## 4. Props — Typed Parameters

Props are declared as a parenthesized parameter list after the component name. They are read-only inside the component — only the caller provides values via JSX attributes.

```kain
component Button(label: String, variant: String = "primary", disabled: Bool = false):
    render <box>
        <text value={label} />
    </box>
```

Callers pass props as JSX attributes:

```kain
<Button label="Save" variant="primary" disabled={false} />
<Button label="Delete" />                           -- uses defaults
```

### Prop Rules

| Rule | Example |
|------|---------|
| Props are typed | `label: String`, `count: Int`, `checked: Bool`, `value: Float` |
| Props can have defaults | `variant: String = "primary"` |
| Props can be compound types | `items: [String]`, `callback: () -> Void` |
| Props are read-only in component | Cannot do `_self.label = "new"` |
| String literal props don't need braces | `kind="primary"` → `String` |
| Expression props use braces | `count={initial + 1}` |
| Props with `{true}`/`{false}` are `Bool` | `disabled={true}` |

---

## 5. State — Persistent Mutable Fields

State is declared with the `state` keyword inside the component body. Initializers can reference props. State persists across frames via vtable slots — the compiler reads state at render start and writes it back at render end.

### 5.1 State Types

Every state field resolves to one of three persistence types:

| Kain Type | Vtable Slot (Read) | Vtable Slot (Write) | LLVM Type | Use Case |
|-----------|-------------------|---------------------|-----------|----------|
| `Int`, `Bool`, `ptr<T>` | Slot 8: `state_get_i64` | Slot 9: `state_set_i64` | `i64` | Counters, booleans, flags, pointer handles |
| `Float` | Slot 19: `state_get_f64` | Slot 20: `state_set_f64` | `double` | Slider values, opacity, animation progress |
| `String`, `Char` | Slot 21: `state_get_string` | Slot 22: `state_set_string` | `i8*` | Text input content, labels, formatted text |

The compiler auto-detects the state type from the Kain type annotation via `StateFieldType::from_resolved(ty)`:

```rust
fn from_resolved(ty: &ResolvedType) -> Self {
    match ty {
        ResolvedType::Float(_) => StateFieldType::F64,
        ResolvedType::String | ResolvedType::Char => StateFieldType::String,
        _ => StateFieldType::I64,
    }
}
```

### 5.2 State Declaration

```kain
component Counter(limit: Int):
    state count: Int = 0                    -- i64 persistence
    state label: String = "items"           -- String persistence (slot 21/22)
    state opacity: Float = 1.0              -- f64 persistence (slot 19/20)
    state last_update: Int = 0              -- i64 persistence
```

### 5.3 State Read/Write via Methods

State is read and written through `_self` in component methods:

```kain
fn increment(_self: Self_):
    _self.count = _self.count + 1           -- mutation
    _self.last_update = _self.count         -- write
    _self.opacity = 0.5                     -- f64 write
    _self.label = "updated"                 -- String write

fn count_str(_self: Self_) -> String:
    return str(_self.count)                 -- read
```

### 5.4 State Persistence Contract

On every render frame, the compiler:

1. **Reads** each state field from the session at render start (via vtable `state_get_*`)
2. **Compiles** the render body — methods can mutate state through `_self`
3. **Writes** each state field back at render end (via vtable `state_set_*`)

This means state mutations (`_self.count = _self.count + 1`) survive across frames automatically. No manual persistence code is needed.

### 5.5 State Rules

- Each state field is typed: `state name: Type = initial`
- Initializers can reference props: `state display: Int = initial + 1`
- State is mutable ONLY through `_self` in methods (never directly in JSX `{expr}`)
- State is NOT visible from outside the component
- `weak` state is actor-only — NOT available in components
- State persists for the component's lifetime (session-bound)
- f64 state uses NaN sentinel for "not found" fallback
- String state uses `""` for "not found" fallback (beware of sentinel-0 bug: empty string is also a valid value)

---

## 6. Methods — Functions with `_self: Self_`

Methods are `fn` declarations inside the component body that receive `_self: Self_` as their first parameter. `_self` gives access to props and state.

### 6.1 Basic Pattern

```kain
component Thermometer(temp: Float, unit: String):
    state is_celsius: Bool = unit == "C"
    state display_temp: Float = temp

    fn convert(_self: Self_):
        if _self.is_celsius:
            _self.display_temp = (_self.display_temp * 9.0 / 5.0) + 32.0
        else:
            _self.display_temp = (_self.display_temp - 32.0) * 5.0 / 9.0
        _self.is_celsius = _self.is_celsius == false

    fn temp_str(_self: Self_) -> String:
        if _self.is_celsius:
            return str(_self.display_temp) + " C"
        return str(_self.display_temp) + " F"

    render <text value={temp_str()} />
```

### 6.2 Method Rules

| Rule | Detail |
|------|--------|
| First parameter MUST be `_self: Self_` | Required to access state and props |
| Methods can return values | Used in JSX `{expr}` interpolation |
| Methods can mutate state | `_self.count = _self.count + 1` |
| Methods can call other methods | `let label = _self.label_text()` |
| Methods can contain any Kain control flow | Loops, conditionals, pattern matching |
| Methods can allocate raw memory | `_self.buffer = alloc_zeroed(n, "Int")` in `with Unsafe` components |
| Methods can use `collapse/observe/decay` | In `with Unsafe` components |

### 6.3 Key Constraint — `_self` NOT in JSX Scope

The most common compile error new Kain developers hit:

```kain
// ❌ Will NOT compile — _self is not in JSX expression scope
render <text value={_self.count} />

// ✅ Use a getter method
fn count_str(_self: Self_) -> String:
    return str(_self.count)
render <text value={count_str()} />
```

---

## 7. The Vtable — How Rendering Actually Works

The `KainComponentSurface` vtable is the ABI contract between the Kain compiler and any surface backend. The compiler emits calls through this vtable; the backend implements them. Neither side knows the other's internals.

### 7.1 24-Slot Vtable Layout

```c
// runtime/native/include/component_surface.h
typedef struct KainComponentSurface {
    // Slots 0-1: Session lifecycle
    int64_t (*session_create) (const char* name, int64_t width, int64_t height);  // 0
    void    (*session_destroy)(int64_t session_id);                                // 1

    // Slots 2-4: Element tree
    int64_t (*element_begin)  (int64_t sid, int64_t parent, const char* kind,
                                const char* stable_key);                           // 2
    void    (*element_end)    (int64_t sid, int64_t element_id);                   // 3
    void    (*element_set_text)(int64_t sid, int64_t elem, const char* text);      // 4

    // Slots 5-7: Style/attribute setters
    void    (*element_set_attr_i64)   (int64_t sid, int64_t elem,
                                        const char* key, int64_t value);           // 5
    void    (*element_set_attr_f64)   (int64_t sid, int64_t elem,
                                        const char* key, double value);            // 6
    void    (*element_set_attr_string)(int64_t sid, int64_t elem,
                                        const char* key, const char* value);       // 7

    // Slots 8-9: i64 State persistence
    int64_t (*state_get_i64)(int64_t sid, const char* key);                        // 8
    void    (*state_set_i64)(int64_t sid, const char* key, int64_t value);         // 9

    // Slots 10-14: Frame lifecycle + event pump + close
    void    (*begin_frame)(int64_t sid, double delta_ms);                          // 10
    void    (*end_frame)  (int64_t sid);                                           // 11
    void    (*present)    (int64_t sid);                                           // 12
    int64_t (*poll_event) (int64_t sid, void* out_event, int64_t max_size);       // 13
    int64_t (*should_close)(int64_t sid);                                          // 14

    // Slots 15-17: Window lifecycle + platform attachment
    int64_t (*window_open)(int64_t sid, const char* title,
                            int64_t w, int64_t h);                                 // 15
    int64_t (*host_pump)  (int64_t sid);                                           // 16
    void    (*session_attach_platform)(int64_t sid, void* platform_handle);        // 17

    // Slot 18: GPU extension discovery
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t sid);              // 18

    // Slots 19-20: f64 State persistence
    double  (*state_get_f64)(int64_t sid, const char* key);                        // 19
    void    (*state_set_f64)(int64_t sid, const char* key, double value);          // 20

    // Slots 21-22: String State persistence
    const char* (*state_get_string)(int64_t sid, const char* key);                 // 21
    void    (*state_set_string)(int64_t sid, const char* key, const char* value);  // 22

    // Slot 23: Event callback binding
    void    (*element_set_callback)(int64_t sid, int64_t elem,
                                     const char* event_name, void* callback_fn);   // 23
} KainComponentSurface;
// sizeof = 24 * sizeof(void*) = 192 bytes on x64
```

### 7.2 Rust Vtable Offsets (Must Match C Exactly)

```rust
// crates/sys-codegen/src/codegen_llvm/component.rs
const OFF_SESSION_CREATE:          u32 = 0;
const OFF_SESSION_DESTROY:         u32 = 1;
const OFF_ELEMENT_BEGIN:           u32 = 2;
const OFF_ELEMENT_END:             u32 = 3;
const OFF_ELEMENT_SET_TEXT:        u32 = 4;
const OFF_ELEMENT_SET_ATTR_I64:    u32 = 5;
const OFF_ELEMENT_SET_ATTR_F64:    u32 = 6;
const OFF_ELEMENT_SET_ATTR_STRING: u32 = 7;
const OFF_STATE_GET_I64:           u32 = 8;
const OFF_STATE_SET_I64:           u32 = 9;
const OFF_BEGIN_FRAME:             u32 = 10;
const OFF_END_FRAME:               u32 = 11;
const OFF_PRESENT:                 u32 = 12;
const OFF_POLL_EVENT:              u32 = 13;
const OFF_SHOULD_CLOSE:            u32 = 14;
const OFF_WINDOW_OPEN:             u32 = 15;
const OFF_HOST_PUMP:               u32 = 16;
const OFF_SESSION_ATTACH_PLATFORM: u32 = 17;
const OFF_GET_GPU_EXTENSION:       u32 = 18;
const OFF_STATE_GET_F64:           u32 = 19;
const OFF_STATE_SET_F64:           u32 = 20;
const OFF_STATE_GET_STRING:        u32 = 21;
const OFF_STATE_SET_STRING:        u32 = 22;
const OFF_ELEMENT_SET_CALLBACK:    u32 = 23;
```

### 7.3 The Frame Loop — End-to-End

```
Compiler emits:                    Backend (e.g., native_ui_surface.c):
  resolve("native_ui")        →    kain_component_surface_resolve("native_ui")
  session_create("W", w, h)   →    abi_ui_session_create + auto-attach winit host
  session_attach_platform(sid, handle) → store native window handle
  window_open(sid, title, w, h)→    abi_ui_window_open
  LOOP:
    host_pump(sid)             →    PeekMessageA message pump (Win32)
    begin_frame(sid, delta_ms) →    abi_ui_begin_frame
    Component_render(surface, sid, parent_id)
      → state_get_i64/f64/string  ← reads persistent state
      → element_begin("box", key)
      → element_set_attr_*        ← style attributes
      → element_set_text("hello")
      → element_set_callback      ← event handler binding
      → element_end
      → state_set_i64/f64/string  ← writes state back
    end_frame(sid)              →    abi_ui_end_frame
    present(sid)                →    GDI framebuffer → BitBlt to screen
    should_close(sid)           →    check close flag
```

### 7.4 Surface Resolution

The compiler resolves the surface backend at frame-loop init:

```llvm
; LLVM IR emitted by component.rs
%surface_name = alloca i8*
store i8* @"native_ui\00", i8** %surface_name
%surface = call %KainComponentSurface* @kain_component_surface_resolve(i8* %surface_name)
```

Backends register themselves at startup:

```c
// native_ui_surface.c
kain_component_surface_register("native_ui", &native_ui_surface);
```

### 7.5 Vtable Invariants

| Rule | Detail |
|------|--------|
| **Append only** | Never insert, reorder, or delete slots |
| **Sequential integers** | OFF_* constants must match C struct field order exactly |
| **LLVM struct matches C** | `%KainComponentSurface = type { i8* x 24 }` |
| **Cross-reference required** | After any vtable change, grep both `component_surface.h` and `component.rs` for slot count mismatch |
| **Verification** | `cargo test -p kain-sys-codegen llvm_lowers_native_ui_primitives` must pass |

---

## 8. JSX Attributes — Full Mapping Table

Every JSX attribute maps to a vtable slot and a C style key. The compiler resolves which vtable slot to call based on the attribute name and value type.

### 8.1 Numeric (f64) Attributes — Vtable Slot 6

| JSX Attribute | Style Key | Kain Type | Use Case |
|--------------|-----------|-----------|----------|
| `padding` | `padding` | `Float` | Padding around element |
| `spacing` | `spacing` | `Float` | Gap between children in stacks |
| `corner_radius` | `corner_radius` | `Float` | Rounded corner radius |
| `radius` | `radius` | `Float` | Circle radius |
| `font_size` | `font_size` | `Float` | Text font size in points |
| `opacity` | `opacity` | `Float` | Element opacity (0.0–1.0) |
| `border` / `border_width` | `border_width` | `Float` | Border thickness |
| `stroke_width` | `border_width` | `Float` | Stroke width (alias) |
| `width` | `width` | `Float` | Element width |
| `height` | `height` | `Float` | Element height |
| `min` | `min` | `Float` | Minimum value |
| `max` | `max` | `Float` | Maximum value |
| `step` | `step` | `Float` | Step increment |

### 8.2 String Attributes — Vtable Slot 7

| JSX Attribute | Style Key | Kain Type | Use Case |
|--------------|-----------|-----------|----------|
| `background` | `fill_color` | `String` | Background color hex/rgba |
| `fill` | `fill_color` | `String` | Fill color (alias) |
| `border_color` | `border_color` | `String` | Border color |
| `stroke` | `border_color` | `String` | Stroke color (alias) |
| `color` / `ink_color` | `ink_color` | `String` | Text / ink color |
| `title` | `title` | `String` | Panel/tooltip title |
| `variant` | `variant` | `String` | Style variant (e.g. "primary", "ghost") |
| `role` | `role` | `String` | Accessibility role |
| `align` | `align` | `String` | Text alignment ("left"/"center"/"right") |
| `font_family` | `font_family` | `String` | Font family name |
| `distribution` | `layout.distribution` | `String` | Stack distribution |
| `axis` | `axis` | `String` | Scroll axis |
| `placeholder` | `placeholder` | `String` | Placeholder text |
| `tooltip` | `tooltip` | `String` | Tooltip text |

### 8.3 Integer Attributes — Vtable Slot 5

| JSX Attribute | Style Key | Kain Type | Use Case |
|--------------|-----------|-----------|----------|
| `direction` | `layout.direction` | `Int` | Stack direction (0=vertical, 1=horizontal) |
| `disabled` | `disabled` | `Bool` | Disabled state |
| `checked` | `checked` | `Bool` | Checkbox/toggle state |
| `selected` | `selected` | `Bool` | Selected state |
| `tab_index` | `tab_index` | `Int` | Tab order index |
| `weight` | `weight` | `Int` | Font weight |

### 8.4 Text Attributes — Vtable Slot 4

| JSX Attribute | Kain Type | Use Case |
|--------------|-----------|----------|
| `value` | `String` | Text content (maps to `element_set_text`) |

### 8.5 Unknown Attributes

Unknown attributes fall through as string attributes with the attribute name itself as the style key:

```rust
_ => AttrMapping {
    vtable_offset: OFF_ELEMENT_SET_ATTR_STRING,
    fn_ptr_ty: "void (i64, i64, i8*, i8*)*",
    style_key: "",
}
```

The attribute name is used as the key; the style_key is the attribute name itself.

---

## 9. Event Callbacks — Vtable Slot 23

Event callbacks bind Kain functions or closures to UI events on elements. The compiler emits `element_set_callback` (vtable slot 23) with the event name and a function pointer.

### 9.1 Supported Event Attributes

| JSX Attribute | Event Kind | Callback Signature |
|--------------|-----------|-------------------|
| `on_click` | `click` | `() -> Void` or `(Event) -> Void` |
| `on_change` | `change` | `() -> Void` or `(Event) -> Void` |
| `on_toggle` | `toggle` | `() -> Void` |
| `on_focus` | `focus` | `() -> Void` |
| `on_blur` | `blur` | `() -> Void` |
| `on_mouseenter` | `mouseenter` | `() -> Void` |
| `on_mouseleave` | `mouseleave` | `() -> Void` |
| `on_submit` | `submit` | `() -> Void` |
| `on_cancel` | `cancel` | `() -> Void` |

### 9.2 How Callbacks Work

**Source (Kain):**
```kain
component Button(label: String, on_click: () -> Void):
    state hovered: Bool = false

    fn handle_hover_enter(_self: Self_):
        _self.hovered = true

    render <box
        on_click={on_click}
        on_mouseenter={handle_hover_enter}
    >
        <text value={label} />
    </box>
```

**LLVM IR (emitted by component.rs):**
```llvm
; For on_click={on_click} — the callback is a prop
%callback_fn = bitcast void (i64, i64, i8*)* %on_click_prop to void*
call void %element_set_callback_fn(i64 %session_id, i64 %element_id,
    i8* @"click\00", void* %callback_fn)
```

**Runtime dispatch (ui_system.c):**
```c
int64_t abi_ui_node_invoke_callback(int64_t session_id,
    int64_t node_id, const char* event_name) {
    // Look up the stored callback function pointer by event name
    int64_t callback_raw = abi_ui_node_state_i64(session_id, node_id,
        event_name, 0);
    if (callback_raw == 0) return 0;
    void (*callback)(void) = (void (*)(void))(uintptr_t)callback_raw;
    callback();           // Invoke the Kain function
    return 1;
}
```

### 9.3 Callback Binding Flow

```
Kain Source:                        Compiler:                        C Runtime:
  on_click={my_handler}        →    JSXAttrValue::Callback         →    element_set_callback(sid, elem,
                                    ("click", my_handler_expr)           "click", fn_ptr)
                                                                   →    store fn_ptr in node state
                                    compile_jsx_callback()
                                    emits: element_set_callback     →    on actual click event:
                                    vtable slot 23                       abi_ui_node_invoke_callback(sid,
                                                                         node_id, "click")
                                                                   →    calls fn_ptr
```

### 9.4 Callback Rules

- Callbacks are stored as `i64` blobs (function pointer cast to `int64_t`)
- The callback function pointer type is `void (i64, i64, i8*)*` — session_id, element_id, event_data
- Not a vtable slot itself — `abi_ui_node_invoke_callback` lives in `ui_system.c`
- Callbacks are idempotent — registering twice with the same event name overwrites

---

## 10. Pulse in Components

Components can declare inline `pulse` clocks that run on every tick. The compiler emits pulse registration code at render time and wires the clock body to mutate component state.

### 10.1 Syntax

```kain
component Spinner(size: Float, color: String):
    state angle: Float = 0.0

    pulse rotate every 16ms jitter 2ms:
        _self.angle = _self.angle + 0.1
        if _self.angle >= 6.283185:
            _self.angle = _self.angle - 6.283185

    render <circle radius={size} stroke={color} />
```

### 10.2 How Pulse Lowers

1. **Parser** detects `pulse name every Nms:` inside a component body → creates `PulseDef` AST node
2. **Typechecker** validates the pulse body can access `_self` (component methods/state)
3. **Codegen** emits pulse registration on first render:
   ```llvm
   ; Emitted once (guarded by one-time flag)
   call void @__kain_pulse_register(i8* %name, i64 %interval_ms, i64 %jitter_ms,
                                     void ()* @ComponentName_pulse_handler)
   ```
4. The pulse handler function reads/writes component state via `_self` just like a method

### 10.3 Pulse Locals in Component Context

Inside a component pulse body, the standard pulse locals are available alongside `_self`:

| Local | Type | Meaning |
|-------|------|---------|
| `_self` | `Self_` | Access to component state and props |
| `pulse_tick` | `Int` | Monotonic tick counter |
| `pulse_dt_ms` | `Float` | Actual elapsed time since last tick |
| `pulse_missed` | `Int` | Count of missed beats |

### 10.4 One-Time Registration Guard

The compiler emits a static boolean flag so pulse registration happens exactly once, on the first render frame. Subsequent frames skip registration (the runtime handles the repetition):

```rust
// component.rs — compile_component_render()
if !component.pulse_types.is_empty() || !component.resonate_types.is_empty() {
    self.emit_component_pulse_resonate_registration(component, &surface_reg, &session_reg)?;
}
```

---

## 11. Resonate in Components

Components can declare inline `resonate` tripwires that fire when a world field changes via patch. The compiler emits resonate registration and wires the handler body.

### 11.1 Syntax

```kain
world AppState:
    state slider_value: Float = 0.5
    state epoch: Int = 0
    surface native_ui => Slider

component Slider():
    state display_value: Float = 0.5

    resonate AppState.slider_value dampen 16ms:
        _self.display_value = resonate_new_f64   -- Float resonance

    render <text value={str(_self.display_value)} />
```

### 11.2 How Resonate Lowers

1. **Parser** detects `resonate World.field [dampen Nms]:` inside a component body → creates `ResonateDef` AST node
2. **Typechecker** validates the resonate target is a world field, validates no self-feedback loop
3. **Codegen** emits resonate registration on first render:
   ```llvm
   call void @__kain_resonate_register(i8* %world_name, i8* %field_name,
                                        i64 %dampen_ms,
                                        void ()* @ComponentName_resonate_handler)
   ```

### 11.3 Resonate Locals in Component Context

| Local | Type | Meaning |
|-------|------|---------|
| `_self` | `Self_` | Access to component state and props |
| `resonate_new_i64` | `Int` | New value (for i64 world fields) |
| `resonate_old_i64` | `Int` | Old value (for i64 world fields) |
| `resonate_new_f64` | `Float` | New value (for f64 world fields) |
| `resonate_old_f64` | `Float` | Old value (for f64 world fields) |
| `resonate_fired` | `Bool` | Whether this is the first fire after registration |

### 11.4 Anti-Self-Feedback Rule

A resonate handler **cannot** write to its own trigger field — the typechecker rejects this at compile time:

```kain
// ❌ COMPILE ERROR: resonate on counter writes to counter (self-loop)
resonate AppState.counter dampen 0ms:
    AppState.counter = AppState.counter + 1

// ✅ CORRECT: resonate on slider_value writes to display_value (different field)
resonate AppState.slider_value dampen 0ms:
    _self.display_value = resonate_new_f64
```

---

## 12. Component Dimensions

Components can specify explicit window dimensions, overriding the platform default (typically 1024x768 or 1280x720).

### 12.1 Syntax

```kain
component MyApp(width=1024, height=768):
    render <panel title="My App" />
```

Dimensions can reference expressions:

```kain
const APP_W: Int = 1280
const APP_H: Int = 720

component Dashboard(width=APP_W, height=APP_H):
    render <panel title="Dashboard" />
```

### 12.2 AST Representation

```rust
pub struct ComponentDimensions {
    pub width: Option<Expr>,
    pub height: Option<Expr>,
    pub span: Span,
}
```

### 12.3 Codegen Flow

When `Component.dimensions` is `Some`, the surface loop uses these values instead of defaults:

```llvm
; Instead of hardcoded 1280x720:
%width = call i64 @eval_dimension(i64 %dim_expr_result)
%height = call i64 @eval_dimension(i64 %dim_expr_result)
%sess_id = call i64 %session_create_fn(i8* %name, i64 %width, i64 %height)
```

When `None`, the renderer uses platform defaults (e.g., `1280x720` for shader surfaces, `1024x768` for native UI).

---

## 13. JSX Syntax Reference

### 13.1 Native Elements (Lowercase Tags)

Lowercase tags represent native UI elements routed through the vtable:

```kain
render <panel title="My Panel">
    <stack direction="vertical">
        <text value="Line 1" />
        <text value="Line 2" />
        <box>
            <circle radius={50.0} fill="#FF0000" />
        </box>
    </stack>
</panel>
```

| Native Element | Purpose | Key Attributes |
|---------------|---------|---------------|
| `<panel>` | Top-level window section | `title` |
| `<box>` | Generic container | `width`, `height`, `background`, `corner_radius` |
| `<stack>` | Layout container | `direction` (`"vertical"`/`"horizontal"`), `spacing`, `distribution` |
| `<text>` | Text label | `value`, `font_size`, `color`, `align`, `font_family`, `weight` |
| `<image>` | Image display | `src`, `width`, `height`, `fit` |
| `<input>` | Text input field | `value`, `placeholder` |
| `<button>` | Clickable button | `value`, `on_click` |

### 13.2 Component Calls (Uppercase Tags)

Any tag starting with an uppercase letter resolves to a component declaration in scope:

```kain
render <stack direction="horizontal">
    <Button label="Save" variant="primary" on_click={handle_save} />
    <Button label="Cancel" variant="secondary" />
    <Counter initial={0} label="Items" />
</stack>
```

### 13.3 Expression Interpolation — `{expr}`

Curly braces interpolate Kain expressions into JSX attributes:

```kain
render <text value={"Hello, " + name + "!"} />
render <Counter initial={count + 1} label="Items" />
render <box width={calc_width()} opacity={0.8} />
```

**Limitations:**
- `_self` is NOT in scope inside `{expr}` — use getter methods
- `if` conditionals in JSX can only use method calls returning `Bool` or simple identifiers
- Complex logic belongs in methods, not inline expressions

### 13.4 Control Flow — `for`, `if`/`elif`/`else`

```kain
render <stack direction="vertical">
    for item in items:
        <text value={item} />

    if has_selection():
        <text value={"Selected: " + get_selected()} />
    elif is_loading():
        <text value="Loading..." />
    else:
        <text value="Nothing selected" />
</stack>
```

- **`for item in list:`** — loop body is a single JSX node (use `<Fragment>` for multiples)
- **`if cond:` / `elif cond:` / `else:`** — each branch is a single JSX node

### 13.5 Fragments — Multiple Root Nodes

```kain
component PureComposition():
    render <Fragment>
        <text value="A" />
        <text value="B" />
        <text value="C" />
    </Fragment>
```

### 13.6 JSX Attribute Value Kinds

```rust
pub enum JSXAttrValue {
    String(String),           // name="value"
    Expr(Expr),               // name={expression}
    Bool,                     // name (shorthand for name={true})
    Callback(String, Expr),   // on_click={handler} (event_name, handler_expr)
}
```

---

## 14. World Surface Binding

A `world` declares a UI surface by naming a component. The world acts as the state authority; the component acts as the view.

### 14.1 Canonical Wiring

```kain
world AppState:
    state count: Int = 0
    state theme: String = "dark"
    state epoch: Int = 0
    surface native_ui => App

component App():
    render <panel title="Counter App">
        <text value="Hello, world!" />
    </panel>
```

### 14.2 Dual-World Authority + Mirror Pattern

```kain
// Authority: owns mutable state, has native_ui surface
world RenderAuthority:
    state frame: Int = 0
    surface native_ui => RenderPanel

// Mirror: receives state via entangle, has web surface
world RenderMirror:
    state frame_copy: Int = 0
    surface web => RenderPanel

// Compiler-owned reactive sync
entangle RenderAuthority.frame <-> RenderMirror.frame_copy with single_writer

// Same component rendered on two different backends
component RenderPanel():
    render <panel title="Render" />
```

### 14.3 Multi-Surface Worlds

```kain
world MultiSurfaceWorld:
    state value: Int = 0

    surface native_ui => App       -- Desktop window (Win32, working)
    surface web => App             -- Same component, web target (planned)
    surface viewport3d => "HUD"    -- 3D viewport key (planned)
    surface ue5 => App             -- Unreal Engine 5 Slate widget (planned)
```

### 14.4 How the Surface Loop Works

When the compiler encounters `surface native_ui => MyComponent`, it emits a full frame loop:

1. `kain_component_surface_resolve("native_ui")` → get vtable pointer
2. `session_create("WorldName", width, height)` → create rendering session
3. `session_attach_platform(sid, handle)` → attach native window handle
4. `window_open(sid, title, w, h)` → create OS window
5. **LOOP:**
   - `host_pump(sid)` → process OS events
   - `begin_frame(sid, delta_ms)` → start frame
   - `MyComponent_render(surface, sid, parent_id)` → emit JSX tree via vtable
   - `end_frame(sid)` → finish frame
   - `present(sid)` → display framebuffer
   - `should_close(sid)` → check exit flag
6. `session_destroy(sid)` → cleanup

### 14.5 Components WITHOUT Worlds

Components do NOT need a world to exist. A file can host many components with zero worlds:

```kain
// Pure component library — no world required
use std::runtime

pub component Button(label: String, variant: String = "primary"):
    state hovered: Bool = false
    render <box>
        <text value={label} />
    </box>

pub component Card(title: String, body: String):
    render <box>
        <text value={title} />
        <text value={body} />
    </box>
```

---

## 15. Theme Integration

Components access theme values through `theme.colors.*` and `theme.spacing.*` structs. The KUIF target architecture defines these in `stdlib/ui/theme.kn`:

### 15.1 Theme Structs

```kain
struct Color:
    r: Float
    g: Float
    b: Float
    a: Float

struct Spacing:
    xs: Int
    sm: Int
    md: Int
    lg: Int
    xl: Int

struct Theme:
    colors: ThemeColors
    spacing: Spacing
    fonts: ThemeFonts

struct ThemeColors:
    accent: Color
    background: Color
    surface: Color
    text_primary: Color
    text_secondary: Color
    button_normal: Color
    button_hover: Color
    button_pressed: Color
    button_disabled: Color
    -- ... 19 color slots total

const DEFAULT_THEME: Theme = Theme {
    colors: ThemeColors {
        accent: color_hex("#4A90D9"),
        background: color_hex("#1A1A2E"),
        -- ...
    },
    -- ...
}
```

### 15.2 Theme Usage in Components

```kain
component Button(label: String, variant: String = "primary", disabled: Bool = false):
    state hovered: Bool = false
    state pressed: Bool = false

    fn compute_fill(_self: Self_) -> Color:
        if _self.disabled:
            return theme.colors.button_disabled
        if _self.pressed:
            return theme.colors.button_pressed
        if _self.hovered:
            return theme.colors.button_hover
        return theme.colors.button_normal

    fn fill_str(_self: Self_) -> String:
        return color_to_hex(compute_fill())

    render <box
        background={fill_str()}
        padding={to_string(theme.spacing.md)}
        corner_radius={8.0}
    >
        <text value={label}
              font_size={14.0}
              color={color_to_hex(theme.colors.text_primary)} />
    </box>
```

---

## 16. Component Composition — Complete Examples

### 16.1 Simple Stateful Widget

```kain
component Toggle(label: String):
    state on: Bool = false

    fn flip(_self: Self_):
        _self.on = _self.on == false

    fn display(_self: Self_) -> String:
        if _self.on:
            return label + " [ON]"
        return label + " [OFF]"

    render <box on_click={flip}>
        <text value={display()} />
    </box>
```

### 16.2 Multi-Component Dashboard

```kain
component MetricCard(title: String, value: Float, unit: String):
    state expanded: Bool = false

    fn display_value(_self: Self_) -> String:
        return str(_self.value) + " " + _self.unit

    render <box>
        <text value={title} />
        <text value={display_value()} font_size={24.0} />
    </box>

component Dashboard():
    render <panel title="Dashboard">
        <stack direction="horizontal" spacing={16.0}>
            <MetricCard title="CPU" value={72.5} unit="%" />
            <MetricCard title="Memory" value={45.2} unit="GB" />
            <MetricCard title="Network" value={1.2} unit="Gbps" />
        </stack>
    </panel>
```

### 16.3 Full Application — World + Component + Pulse + Resonate + Callbacks

```kain
use std::runtime

// ── State Authority ──
world AppState:
    state count: Int = 0
    state slider_value: Float = 0.5
    state epoch: Int = 0
    surface native_ui => App

// ── Patch for journaled mutation ──
patch increment(world: AppState) -> Int:
    world.count = world.count + 1
    world.epoch = world.epoch + 1
    return world.count

// ── Law for invariant ──
law count_valid(c: Int) -> Bool:
    return c >= 0 and c < 1000000

// ── Main Component ──
component App(width=800, height=600) with Reactive:
    state display_count: Int = 0
    state display_slider: Float = 0.5

    fn count_str(_self: Self_) -> String:
        return "Count: " + str(_self.display_count)

    fn slider_str(_self: Self_) -> String:
        return "Slider: " + str(round(_self.display_slider * 100.0)) + "%"

    fn handle_click(_self: Self_):
        let _ = increment(AppState)

    // React to world count changes
    resonate AppState.count dampen 0ms:
        _self.display_count = resonate_new_i64

    // React to world slider changes
    resonate AppState.slider_value dampen 16ms:
        _self.display_slider = resonate_new_f64

    // Live clock for freshness indication
    pulse tick every 1000ms:
        _self.display_count = _self.display_count  -- just ensures re-render

    render <panel title="Full App — World + Component + Pulse + Resonate + Callbacks">
        <stack direction="vertical" spacing={12.0} padding={16.0}>
            <text value={count_str()} font_size={20.0} />
            <text value={slider_str()} font_size={16.0} />
            <box
                on_click={handle_click}
                background="#4A90D9"
                corner_radius={8.0}
                padding={12.0}
            >
                <text value="Click to Increment" color="#FFFFFF" font_size={16.0} />
            </box>
        </stack>
    </panel>
```

---

## 17. Effects — Behavior Annotations

```kain
component Widget() with Reactive:
    render <panel title="Interactive" />

component MemoryView(cell_count: Int) with Unsafe:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")
    render <text value="memory view" />

component RenderView() with GPU:
    render <panel title="GPU View" />

component PureWidget() with Pure:
    render <text value="deterministic" />
```

| Effect | Meaning |
|--------|---------|
| `Reactive` | Component handles events, callbacks, reactive updates |
| `Unsafe` | Component manages raw memory, pointers, FFI handles |
| `GPU` | Component interfaces with GPU resources, shaders |
| `Pure` | Deterministic — same props always produce same render |

---

## 18. Combining Components with the Semantic Stack

### Component + Patch

```kain
patch bump_counter(world: AppState) -> Int:
    world.count = world.count + 1
    world.epoch = world.epoch + 1
    return world.count
```

### Component + Entangle

```kain
entangle AppState.count <-> AppMirror.count_copy with single_writer
```

### Component + Shader

```kain
shader compute MyKernel(id: UVec3) -> Void workgroup(8, 1, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    let lane = src[id.x]
    dst[id.x] = lane + UInt(9)
    return

component ComputeView() with GPU:
    render <panel title="GPU Compute" />
```

### Component + Teleport

```kain
world TeleportSource:
    state data: Int = 42
    surface native_ui => TeleportPanel

world TeleportDest:
    state data_copy: Int = 0
    surface web => TeleportPanel

teleport source_data from TeleportSource to TeleportDest via data_bus

component TeleportPanel():
    render <panel title="Teleport View" />
```

### Component + Orchestrate

```kain
orchestrate RenderPipeline(input: Int) -> Int:
    stage compute: cpu mix_values(input)
    stage render_to_component: world {
        surface native_ui => App
    }
    after compute → render_to_component
    return compute
```

### Component + Actor

```kain
actor Worker:
    state result: Int = 0
    on Compute(reply_to: P, value: Int):
        self.result = value * 2
        send reply_to.Reply(value = self.result)

component ActorView():
    state last_result: Int = 0
    fn result_str(_self: Self_) -> String:
        return "Actor result: " + str(_self.last_result)
    render <text value={result_str()} />
```

---

## 19. Anti-Patterns

### ❌ Component as World Decoration Only

```kain
// Underutilized — component is just a title string
component App():
    render <panel title="My App" />
```

**Fix:** Compose, use state, use methods, use events.

### ❌ Never Composing Components

```kain
// Every file has exactly one component — no composition
component App():
    render <panel title="Tiny App"><text value="hello" /></panel>
```

**Fix:** Break UI into composable pieces — `<Button />`, `<Card />`, `<Layout />`.

### ❌ Putting Logic in JSX Instead of Methods

```kain
// ❌ Fragile, limited by JSX parser
render <text value={if a > b: "big" else: "small"} />
```

**Fix:** Move logic to methods.

### ❌ Mutating State Directly in JSX

```kain
// ❌ Side effects in render
render <box on_click={_self.count = _self.count + 1}>
```

**Fix:** Mutate in an explicit method, reference the method in JSX.

### ❌ Using `weak` State in Components

```kain
// ❌ Compiler error
component Bad():
    state weak cache: Int = 0
```

**Fix:** `weak` is actor-only. Use regular `state` in components.

### ❌ Writing Callbacks That Mutate World State Directly

```kain
// ❌ Bypasses patch journal, law checking
render <box on_click={|| AppState.count = AppState.count + 1}>
```

**Fix:** Use `patch` functions for world mutations.

### ❌ Self-Looping Resonate

```kain
// ❌ COMPILE ERROR — resonates cannot write to their own trigger field
resonate AppState.count dampen 0ms:
    AppState.count = AppState.count + 1
```

**Fix:** Write to a different field, or use the shadow pattern.

---

## 20. Source Truth — Where Everything Lives

### Language Truth (Rust Bootstrap)

| File | What |
|------|------|
| `crates/core/src/ast.rs:564-635` | `Component`, `StateDecl`, `ComponentDimensions`, `JSXNode`, `JSXAttribute`, `JSXAttrValue` AST nodes |
| `crates/core/src/parser.rs:2601` | `parse_component_dimensions()` |
| `crates/core/src/parser.rs:2625` | `parse_component()` — main component parser |
| `crates/core/src/parser.rs:2806` | `parse_component_with_attrs()` — attribute-gated entry |
| `crates/core/src/types.rs` | `check_component()` — typechecking, state type detection, callback validation |
| `crates/sys-codegen/src/codegen_llvm/component.rs` | Full vtable codegen — 24-slot struct, JSX→vtable lowering, state persistence, callback codegen, pulse/resonate registration |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | Surface loop emission, frame loop orchestration |

### Runtime Truth (C Substrate)

| File | What |
|------|------|
| `runtime/native/include/component_surface.h` | `KainComponentSurface` 24-slot vtable struct — THE ABI contract |
| `runtime/native/include/gpu_surface_extension.h` | `KainGpuSurfaceExtension` — slot 18 return type |
| `runtime/native/src/ui/native_ui_surface.c` | GDI backend vtable implementation (all 24 slots) |
| `runtime/native/src/ui/ui_system.c` | `abi_ui_node_invoke_callback()` — callback dispatch |
| `runtime/native/src/ui/ui_host_adapter.c` | Win32 window creation, GDI framebuffer |
| `runtime/native/src/core/component_surface.c` | Surface registry — maps names to vtable pointers |

### Authored Examples

| File | What |
|------|------|
| `blades/window_proof/src/main.kn` | **17-line minimal window** — Oracle-verified, world + surface + component |
| `blades/kain/component_fuzz/src/components.kn` | **40+ components** across 10 categories (naked, stateful, computational, recursive, fragments, pointer-laden, actor-aware, deeply nested, world-surface) |
| `blades/kain/component_minimal/src/app.kn` | **280+ line Win32 GDI app** with components, `include <windows.h> as win`, manual memory, full message pump |
| `blades/kaintana/` | Kaintana framework built on top of components |
| `benchmark/cases_v2/keyword_crucible.kn` | Component keyword exercised in benchmark |
| `stdlib/ui.kn` | Current 1,677-line bridge — being shrunk to re-export hub |

### Design Documents

| File | What |
|------|------|
| `runtime/native/src/ui/research/MASTER_DOC.md` | KUIF master plan — vision, architecture, 10-phase plan, vtable design |
| `runtime/native/src/ui/research/PHASE3_COMPILER_PIPELINE_FILES.md` | Exact Rust changes for Phase 3 compiler pipeline |
| `docs/RULEBOOK.md` | Decision ladder — component's place at Layer UI |
| `docs/KAIN_BY_EXAMPLE.md` | Canonical syntax for every Kain construct |
| `docs/WORLD.MD` | World surface declarations, world→component wiring |
| `docs/PATCH.MD` | Patches + component surfaces |

---

## 21. Quick Reference

### Keyword Syntax

```kain
component Name(prop_a: Type = default, prop_b: Type = default)
        with Effect1, Effect2 width=1024 height=768:

    state count: Int = 0
    state opacity: Float = 1.0
    state label: String = ""

    fn method(_self: Self_) -> Return:
        return value

    pulse tick every 16ms:
        _self.count = _self.count + 1

    resonate World.field dampen 0ms:
        _self.label = resonate_new_string

    render <jsx>...</jsx>
```

### JSX Quick Reference

| Syntax | Meaning |
|--------|---------|
| `<panel title="x">` | Native element with string attr |
| `<text value={expr} />` | Native element with expression attr |
| `<Button label="x" />` | Component call (uppercase = dispatch) |
| `{expression}` | Expression interpolation |
| `for item in list:` | Loop inside JSX |
| `if cond:` / `elif:` / `else:` | Conditional inside JSX |
| `<Fragment>...</Fragment>` | Multiple root nodes without wrapper |
| `on_click={handler}` | Event callback binding |
| `name` (alone) | Bool shorthand `name={true}` |

### Vtable Quick Reference

| Slot | Name | Purpose |
|------|------|---------|
| 0 | `session_create` | Create rendering session |
| 1 | `session_destroy` | Destroy session |
| 2 | `element_begin` | Start a UI element |
| 3 | `element_end` | End a UI element |
| 4 | `element_set_text` | Set element text content |
| 5 | `element_set_attr_i64` | Set integer attribute |
| 6 | `element_set_attr_f64` | Set float attribute |
| 7 | `element_set_attr_string` | Set string attribute |
| 8 | `state_get_i64` | Read i64 state |
| 9 | `state_set_i64` | Write i64 state |
| 10 | `begin_frame` | Start frame |
| 11 | `end_frame` | End frame |
| 12 | `present` | Display frame |
| 13 | `poll_event` | Poll input events |
| 14 | `should_close` | Check exit flag |
| 15 | `window_open` | Create OS window |
| 16 | `host_pump` | Process OS messages |
| 17 | `session_attach_platform` | Attach native window handle |
| 18 | `get_gpu_extension` | Get GPU backend extension |
| 19 | `state_get_f64` | Read f64 state |
| 20 | `state_set_f64` | Write f64 state |
| 21 | `state_get_string` | Read String state |
| 22 | `state_set_string` | Write String state |
| 23 | `element_set_callback` | Bind event callback |

### Compile Commands

```bash
kain check src/app.kn              # Typecheck components and JSX
kain build src/app.kn --target llvm  # Compile to LLVM → native
kain run src/app.kn --target llvm    # Compile + execute
```

---

*Component is the UI layer of Kain. It sits at Layer UI of the decision ladder — above the 7-layer semantic stack — as a first-class, compiler-owned, world-independent UI abstraction with typed props, local state (i64/f64/String), methods, inline pulse/resonate clocks, event callbacks, explicit dimensions, GPU support, JSX composition, and a 24-slot vtable ABI that decouples the compiler from any rendering backend. The more you compose, state-manage, and method-encapsulate in your components, the more the compiler can help you.*
