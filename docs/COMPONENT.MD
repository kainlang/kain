# Component — React-like UI with Typed Props and JSX

> *"Components are the UI layer of Kain. They own their render tree, their state, and their composition graph — independently of worlds, functions, or any other language construct. Every interactive surface in Kain ultimately flows through a component."*

**Last updated:** 2026-06-08
**Based on:** `crates/core/src/ast.rs:153,543-617`, `crates/core/src/parser.rs:59,429,762,996`, `stdlib/ui.kn`, `blades/kain/component_minimal/src/app.kn`, `blades/kain/component_fuzz/src/components.kn`, `RULEBOOK.md` (lines 90–295)

______________________________________________________________________

## Table of Contents

1. [What Is a Kain Component?](#1-what-is-a-kain-component)
1. [The Core Idea](#2-the-core-idea)
1. [Component Anatomy](#3-component-anatomy)
1. [JSX Syntax Reference](#4-jsx-syntax-reference)
1. [Tag Dispatch — Uppercase vs Lowercase](#5-tag-dispatch--uppercase-vs-lowercase)
1. [World + Component Wiring](#6-world--component-wiring)
1. [Component vs World vs Fn](#7-component-vs-world-vs-fn)
1. [Component vs Kaintana](#8-component-vs-kaintana)
1. [Component Patterns](#9-component-patterns)
1. [Surface Kinds](#10-surface-kinds)
1. [Under-Utilized Ideas](#11-under-utilized-ideas)
1. [Combining with Other Keywords](#12-combining-with-other-keywords)
1. [Validation and Error Diagnostics](#13-validation-and-error-diagnostics)
1. [Source Anchors](#14-source-anchors)
1. [Anti-Patterns](#15-anti-patterns)
1. [Quick Reference](#16-quick-reference)

______________________________________________________________________

## 1. What Is a Kain Component?

A **component** is a top-level declaration that defines a reusable UI widget with typed props, local state, methods, and a JSX render body. It is Kain's native answer to React components — the language-level primitive for building user interfaces.

```kn
component Counter(initial: Int, label: String):
    state count: Int = initial

    fn label_text(_self: Self_) -> String:
        return _self.label + ": " + str(_self.count)

    render <box>
        <text value={label_text()} />
    </box>
```

Every component is:

- **Self-contained** — owns its state, methods, and render tree
- **Composable** — calls other components via uppercase JSX tags
- **Renderable** — produces a JSX visual tree lowered to `std::ui` native nodes
- **Free-standing** — does NOT require a world to exist

### The Core Contract

```kn
component Name(props…):
    state field: Type = initial…
    fn method(_self: Self_) -> Return…
    render <jsx>…</jsx>
```

| Section | Required | Description |
|---------|----------|-------------|
| `(props)` | No | Typed parameters received from the JSX caller |
| `state` | No | Local mutable state, initializers can reference props |
| `fn` | No | Methods with `_self: Self_` to read/write state |
| `effects` | No | Effect annotations (`with Reactive`, `with Unsafe`) |
| `render` | **Yes** | The JSX tree this component produces |

______________________________________________________________________

## 2. The Core Idea

### What Component IS

- A **reusable UI widget** — `component Button`, `component Toolbar`, `component Dashboard`
- A **typed props receiver** — `component Card(title: String, body: String)`
- A **state holder** — `state expanded: Bool = false`
- A **composition unit** — `<Button />` calls another component
- A **render tree factory** — `render <box>...</box>`
- A **world surface** — `surface native_ui => Component` wires world to component
- A **first-class language declaration** — parsed as `ast::Component`, not a struct wrapper
- An **effect-capable** declaration — `with Reactive`, `with Unsafe`
- A **recursive structure** — components can call themselves via conditional rendering
- A **pointer-aware container** — components manage raw memory, GPU buffers, shared resources

### What Component IS NOT

- **NOT a world** — components have no surfaces, no entangle, no patch, no law
- **NOT an actor** — components have no mailbox, no `on` handlers, no `send`/`spawn`
- **NOT a struct** — you cannot `let x = ComponentName(...)`, only `<ComponentName />` in JSX
- **NOT tied to a function** — `render` is declarative, not imperative
- **NOT a framework** — component is the primitive; Kaintana is the framework
- **NOT bound to one surface** — the same component can appear on `native_ui`, `web`, and `viewport3d`

### Relationship View

```
┌─────────────────────────────────────────────────────────────┐
│                        Kain Program                         │
│                                                             │
│  ┌──────────┐   surface    ┌──────────────┐   composes     │
│  │  world   │ ──────────▶  │  component   │ ────────────▶  │
│  │          │              │              │    child comp  │
│  │  state   │             │  props       │                │
│  │  patch   │             │  state       │                │
│  │  law     │             │  methods     │                │
│  │  entangle│             │  render JSX  │                │
│  └──────────┘             └──────┬───────┘                │
│                                  │                         │
│                           lowers to                        │
│                                  │                         │
│                          ┌───────▼────────┐                │
│                          │   std::ui      │                │
│                          │  reconciler    │                │
│                          │  native nodes  │                │
│                          └────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

______________________________________________________________________

## 3. Component Anatomy

### 3.1 Props — Typed Parameters

Props are declared as a parenthesized parameter list after the component name. They are read-only inside the component — only the caller provides values via JSX attributes.

```kn
component Button(label: String, kind: String, disabled: Bool):
    render <box>
        <text value={label} />
    </box>
```

Callers pass props as JSX attributes:

```kn
<Button label="Save" kind="primary" disabled={false} />
```

**Prop rules:**

- Props are typed — `String`, `Int`, `Bool`, `[Int]`, etc.
- Props are read-only inside the component body
- Props can be referenced in state initializers and methods
- Props with `{expr}` syntax support arbitrary expressions
- Props with literal values (no braces) are treated as strings: `kind="primary"` → `kind: String = "primary"`
- Props with `{true}` / `{false}` are boolean: `disabled={true}`

### 3.2 State — Local Mutable Fields

State is declared with the `state` keyword inside the component body. Initializers can reference props.

```kn
component Toggle():
    state on: Bool = false

    fn flip(_self: Self_):
        _self.on = _self.on == false

    fn label(_self: Self_) -> String:
        if _self.on:
            return "ON"
        return "OFF"

    render <box>
        <text value={label()} />
    </box>
```

```kn
component BoundedCounter(limit: Int):
    state count: Int = 0

    fn bump(_self: Self_) -> Int:
        if _self.count < _self.limit:
            _self.count = _self.count + 1
        return _self.count

    render <stack direction="horizontal">
        <text value={count_str()} />
        <text value="/" />
        <text value={limit_str()} />
    </stack>
```

**State rules:**

- Each state field is typed: `state name: Type = initial`
- Initializers can reference props: `state display_temp: Int = temp`
- State is mutable only through `_self` in methods (never directly in JSX)
- State is NOT visible from outside the component
- `weak` state (from actors) is NOT available in components — it is actor-only
- State persists for the component's lifetime

### 3.3 Methods — Functions with `_self: Self_`

Methods are regular `fn` declarations that receive `_self: Self_` as their first parameter. `_self` gives access to both props and state.

```kn
component Thermometer(temp: Int, unit: String):
    state is_celsius: Bool = unit == "C"
    state display_temp: Int = temp
    state warning: Bool = temp > 100

    fn convert(_self: Self_):
        if _self.is_celsius:
            _self.display_temp = (_self.display_temp * 9 / 5) + 32
        else:
            _self.display_temp = (_self.display_temp - 32) * 5 / 9
        _self.is_celsius = _self.is_celsius == false

    fn status(_self: Self_) -> String:
        if _self.warning:
            return "HOT"
        return "normal"

    fn temp_str(_self: Self_) -> String:
        return str(_self.display_temp)

    render <stack direction="vertical">
        <text value={temp_str()} />
        <text value={status()} />
    </stack>
```

**Method rules:**

- The first parameter MUST be `_self: Self_` to access component state
- Methods can return values for use in JSX `{expr}` interpolation
- Methods can mutate state: `_self.count = _self.count + 1`
- Methods can call other methods
- Methods can contain loops, conditionals, and any Kain control flow
- Methods can allocate raw memory, use `collapse/observe/decay`, and manage pointers

**Fallback for complex rendering logic:**

When JSX expressions don't support the control flow you need (e.g., building a string from an array), use a method:

```kn
fn format_sequence(_self: Self_) -> String:
    var text = ""
    var i: Int = 0
    while i < len(_self.values):
        if i > 0:
            text = text + ", "
        text = text + str(_self.values[i])
        i = i + 1
    return text
```

Then call `{format_sequence()}` in JSX.

### 3.4 Effects — Behavior Annotations

Components can declare effects with `with`:

```kn
component Widget() with Reactive:
    render <panel title="Interactive" />

component MemoryView(cell_count: Int) with Unsafe:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")

    fn alloc(_self: Self_):
        _self.buffer = alloc_zeroed(_self.cell_count, "Int")

    render <text value="memory view" />
```

| Effect | Meaning |
|--------|---------|
| `Reactive` | Component subscribes to event handling / reactive updates |
| `Unsafe` | Component manages raw memory, pointers, FFI handles |
| `Pure` | Component is deterministic — same props → same render |
| `GPU` | Component interfaces with GPU resources |

### 3.5 Render Body — The JSX Tree

Every component **MUST** have exactly one `render` body containing JSX markup:

```kn
render <box>
    <text value="Hello, world!" />
</box>
```

The render body is the visual output of the component. It lowers to `std::ui` native nodes through the compiler's JSX codegen path.

______________________________________________________________________

## 4. JSX Syntax Reference

JSX is Kain's declarative UI syntax. It is embedded directly in `render` blocks and supports elements, component calls, expressions, control flow, and fragments.

### 4.1 Native Elements (Lowercase Tags)

Lowercase tags represent native UI elements routed to the `std::ui` renderer:

```kn
render <box>
    <panel title="My Panel">
        <stack direction="vertical">
            <text value="Line 1" />
            <text value="Line 2" />
        </stack>
    </panel>
</box>
```

| Native Element | Purpose |
|---------------|---------|
| `<panel>` | Top-level window or section panel |
| `<box>` | Generic container box |
| `<stack>` | Layout container with `direction` (`"vertical"` / `"horizontal"`) |
| `<text>` | Text label with `value` attribute |
| `<image>` | Image display |
| `<input>` | Text input field |
| `<button>` | Clickable button |

### 4.2 Component Calls (Uppercase Tags)

Any tag starting with an uppercase letter is resolved to a component declaration in scope:

```kn
render <stack direction="horizontal">
    <Button label="Save" kind="primary" />
    <Button label="Cancel" kind="secondary" />
    <Counter initial={0} label="Items" />
</stack>
```

**Props are passed as JSX attributes:**

- Literal strings: `label="Save"`
- Expressions: `initial={0}`, `items={my_array}`
- Boolean shorthand: treat `{true}` as `enabled={true}`

### 4.3 Expressions — `{expr}` Interpolation

Curly braces interpolate arbitrary Kain expressions into JSX attributes and text nodes:

```kn
component Greeting(name: String):
    render <text value={"Hello, " + name + "!"} />

component Calc(a: Int, b: Int):
    render <text value={str(a + b)} />
```

**In attributes:**

```kn
<text value={label_text()} />
<Counter initial={count + 1} />
<Box width={calc_width()} height={calc_height()} />
```

**Limitations** (from fuzz testing):

- `{expr}` can reference: prop names, method calls, literals, operators
- `_self` is NOT in scope inside JSX — use getter methods instead
- JSX `if` conditions can only use method calls returning `Bool` or simple identifiers — not `>`, `<`, `==` operators inline
- Complex logic belongs in methods called from `{expr}`, not inline

### 4.4 Control Flow — `for` and `if` Inside JSX

JSX supports native control flow with `for` and `if`/`else`:

```kn
render <stack direction="vertical">
    for item in items:
        <text value={item} />

    if selected_index >= 0:
        <text value={"Selected: " + items[selected_index]} />
    else:
        <text value="Nothing selected" />
</stack>
```

**`for item in list:` rules:**

- `item` binds to each element of `list`
- The loop body is a single JSX node (use `<Fragment>` for multiples)
- Loop variables have LIMITED scope in the body — pre-compute in methods when needed

**`if cond:` / `elif cond:` / `else:` rules:**

- Conditions must evaluate to `Bool`
- Each branch body is a single JSX node
- `elif` is supported (from the `else if` pattern)

```kn
render <box>
    if is_empty():
        <text value="(empty)" />
    elif has_items():
        <text value={"count: " + str(len(items))} />
    else:
        <text value="loading..." />
</box>
```

### 4.5 Fragments — Multiple Root Nodes

`<Fragment>` wraps multiple sibling nodes without creating a parent element in the DOM:

```kn
component Fragments():
    render <Fragment>
        <text value="A" />
        <text value="B" />
        <text value="C" />
    </Fragment>
```

Use fragments when a component needs to return multiple top-level nodes without an extra container.

### 4.6 JSX Attribute Types

| Syntax | Type | Example |
|--------|------|---------|
| `name="value"` | `String` literal | `label="Save"` |
| `name={expr}` | Expression (any type) | `count={items.len()}` |
| `name` (alone) | `Bool = true` | `disabled` (shorthand for `disabled={true}`) |

### 4.7 JSX Limitations (From Fuzz Testing)

The component fuzz blade (`blades/kain/component_fuzz/`) discovered these compiler limits:

1. **`_self` is NOT in scope inside JSX.** Use getter methods instead:

   ```kn
   // ❌ Will NOT compile
   render <text value={_self.count} />

   // ✅ Use a method
   fn count_str(_self: Self_) -> String:
       return str(_self.count)
   render <text value={count_str()} />
   ```

1. **JSX `for` loop variables do NOT bind in the loop body for complex types.** Pre-compute arrays in methods, render lists differently.

   ```kn
   // ❌ Limited binding
   for item in items:
       <Card title={item.title} />   // may not bind

   // ✅ Pre-compute in method
   fn render_all(_self: Self_) -> String:
       // build string manually
   render <text value={render_all()} />
   ```

1. **JSX `{/* */}` comments do NOT exist.** Use `//` comments outside the render body.

1. **JSX `if` conditions can only use method calls returning `Bool` or simple identifiers.** Expressions like `count > 0` are NOT valid inline:

   ```kn
   // ❌ Will NOT compile
   if count > 0:
       <text value="yes" />

   // ✅ Use a method
   fn has_items(_self: Self_) -> Bool:
       return _self.count > 0
   if has_items():
       <text value="yes" />
   ```

1. **`weak` state is actor-only**, not component. Components use regular `state`.

1. **Component names cannot shadow builtin types** (e.g., `Void`).

______________________________________________________________________

## 5. Tag Dispatch — Uppercase vs Lowercase

The **first letter** of a JSX tag determines the dispatch target:

```kn
╔═══════════════╤══════════════════════════════════════════════════╗
║  Tag Pattern  │  Dispatch                                        ║
╠═══════════════╪══════════════════════════════════════════════════╣
║  `<panel>`    │  Native UI element → `std::ui` renderer          ║
║  `<box>`      │  Native UI element                               ║
║  `<text>`     │  Native UI element                               ║
║  `<stack>`    │  Native UI element                               ║
║  `<image>`    │  Native UI element                               ║
║  `<input>`    │  Native UI element                               ║
║  `<button>`   │  Native UI element                               ║
╠═══════════════╪══════════════════════════════════════════════════╣
║  `<Button>`   │  Component call → `component Button(...)`        ║
║  `<Counter>`  │  Component call → `component Counter(...)`       ║
║  `<Dashboard>`│  Component call → `component Dashboard(...)`     ║
║  `<CardGrid>` │  Component call → `component CardGrid(...)`      ║
║  `<RecursiveTree>`│ Component call → `component RecursiveTree(...)`║
╚═══════════════╧══════════════════════════════════════════════════╝
```

**This is the entire dispatch mechanism.** There is no import statement for components — they are resolved by name at the file/module scope. Modules imported with `use module::*` bring their components into scope for JSX tag resolution.

```kn
use components::*   // brings all component names into JSX scope

component Zoo():
    render <box>
        <Atom />
        <Blank />
        <Toggle />
        <BoundedCounter limit={5} />
    </box>
```

### Design Rationale

The uppercase/lowercase distinction gives the reader immediate visual feedback:

- **Uppercase** = I am calling another component. I am composing UI.
- **Lowercase** = I am using a native primitive. I am at the leaf of the render tree.

This is familiar from React/JSX, but in Kain it is the ONLY resolution mechanism — there is no separate component registry or string-based lookup.

______________________________________________________________________

## 6. World + Component Wiring

### 6.1 The `surface` Declaration

A `world` declares a UI surface by naming a component:

```kn
world MyWorld:
    state signal: Int = 1
    surface native_ui => App
```

This tells the runtime: "Render the `App` component using this world's state." The world acts as the **state authority**; the component acts as the **view**.

```kn
component App():
    render <panel title="My App" />
```

### 6.2 Full World-to-UI Chain

```kn
// ── Authority World (owns state) ──
world RenderAuthority:
    state frame: Int = 0
    state particles: Int = 65536
    surface native_ui => RenderPanel

// ── Mirror World (receives state) ──
world RenderMirror:
    state frame_copy: Int = 0
    state particles_copy: Int = 65536
    surface web => RenderPanel

// ── Entangle: authority → mirror ──
entangle RenderAuthority.frame <-> RenderMirror.frame_copy with single_writer
entangle RenderAuthority.particles <-> RenderMirror.particles_copy with single_writer

// ── Shared Component (used by both worlds) ──
component RenderPanel():
    render <panel title="Render">
        <text value="Frame data flowing through world surfaces" />
    </panel>
```

### 6.3 Component IS NOT Tied to World

This is the most important fact about components. A single `.kn` file can host 10,000 components and zero worlds:

```kn
// ── Valid Kain: components with no world ──
component Atom():
    render <text value="." />

component Toggle():
    state on: Bool = false
    fn label(_self: Self_) -> String:
        if _self.on: return "ON"
        return "OFF"
    render <text value={label()} />

component Dashboard():
    render <box>
        <Atom />
        <Toggle />
        <text value="No world needed" />
    </box>
```

Components are **first-class UI declarations**, not world accessories. The `surface native_ui => ComponentName` syntax is ONE way to wire a world to a root component — it's not required for components to exist, and it's not the only way components get rendered (see Kaintana).

### 6.4 Multi-Surface Worlds

A world can project the same or different components to multiple surfaces:

```kn
world TelemetryWorld:
    state frame: Int = 0
    state best_distance: Int = 0

    surface native_ui => TelemetryPanel
    surface web => TelemetryPanel         // same component, different backend
    surface viewport3d => "RenderView"    // 3D viewport (string key, not component)
```

______________________________________________________________________

## 7. Component vs World vs Fn

| You want to… | Use | Because |
|---|---|---|
| Define a reusable UI widget with its own state | `component` | Owns render tree, state, methods |
| Track app-level state across frames | `world` + `surface native_ui => Component` | World is authority; component is view |
| Compose UI from smaller pieces | `component` calling `<OtherComponent />` | JSX composition — uppercase tags dispatch to components |
| Bind a world to a root UI view | `surface native_ui => ComponentName` on world | Canonical world→UI wiring |
| Do computation with no UI output | `fn` | Components are for rendering; functions are for logic |
| Render data from a world inside a component | Read `world.field` inside JSX `{expr}` | Components have read access to world state (via surface projection) |
| Handle live events, polling, input | `component` with `state` + Kaintana/UI bridge | Component state + event system = interactive UI |
| Build a whole UI framework | `component` + `world` + `resonate` + `patch` | See Kaintana — component is the view layer |
| Share UI state between distant parts of the tree | `world` + `entangle` | Pass world state through surfaces, not prop drilling |
| Animate over time | `pulse` + `component` | Pulse updates world fields; component re-renders |
| Talk to a GPU from a UI | `component` with `with GPU` + `shader` | Components can manage GPU resources |

### The Decision Ladder for UI

```
"Am I building UI?"
  ├── Is it a single, reusable widget with its own state?      → component
  ├── Is it application state behind the UI?                   → world + surface => Component
  ├── Is it an interactive widget framework?                   → component + world + resonate + patch
  ├── Is it a layout / theme / framework layer?                → Kaintana (built on component)
  └── Is it computation that feeds the UI?                    → fn (called from component {expr})
```

______________________________________________________________________

## 8. Component vs Kaintana

| Aspect | `component` (Language Primitive) | Kaintana (Framework) |
|--------|----------------------------------|----------------------|
| What it is | Built-in keyword producing a UI declaration | Authored blade package at `blades/kaintana/` |
| Analogy | Kain's `<div>` | Kaintana = Kain's React |
| Scope | Single widget definition | Themes, layout helpers, widget reconciliation, event routing, input handling, desktop adapters |
| Dependencies | None — pure language surface | Built on `component`, `world`, `std::ui`, `entangle`, `resonate`, `patch`, `law`, desktop bridge |
| Surface backends | `native_ui`, `web`, `viewport3d`, `ue5` | Vulkan/winit desktop adapter via Vulkain |
| State management | Local `state` only | Full world + entangle + patch system |
| Reconciliation | Compiler-owned JSX tree lowering | `ui_reconcile_node()`, stable keys, dirty tracking |
| When to use | Always — even inside Kaintana | When you need themes, layout, and interactive widgets |

You can use `component` without Kaintana (via `std::ui` directly). Kaintana gives you the full retained-mode widget framework on top of components.

______________________________________________________________________

## 9. Component Patterns

### 9.1 Simple Widget — Leaf Component

```kn
component Atom():
    render <text value="." />

component Blank():
    render <panel title="" />

component ExprOnly(value: Int):
    render <text value={value} />
```

### 9.2 Stateful Widget — Interactive Component

```kn
component TrafficLight():
    state phase: Int = 0

    fn advance(_self: Self_):
        _self.phase = (_self.phase + 1) % 3

    fn color(_self: Self_) -> String:
        if _self.phase == 0: return "RED"
        elif _self.phase == 1: return "YELLOW"
        return "GREEN"

    fn phase_label(_self: Self_) -> String:
        return "phase " + str(_self.phase)

    render <box>
        <text value={color()} />
        <text value={phase_label()} />
    </box>
```

### 9.3 Computational Component — Logic in Methods

```kn
component Factorial(n: Int):
    state result: Int = 1
    state computed: Bool = false

    fn compute(_self: Self_):
        if _self.computed: return
        var i: Int = 1
        var acc: Int = 1
        while i <= _self.n:
            acc = acc * i
            i = i + 1
        _self.result = acc
        _self.computed = true

    fn display(_self: Self_) -> String:
        return str(_self.n) + "! = " + str(_self.result)

    render <box>
        <text value={display()} />
    </box>
```

### 9.4 Composed Dashboard — Multi-Level Composition

```kn
component Card(title: String, body: String):
    render <box>
        <text value={title} />
        <text value={body} />
    </box>

component CardGrid(card_a: String, card_b: String, card_c: String):
    render <stack direction="vertical">
        <Card title={card_a} body={"body of " + card_a} />
        <Card title={card_b} body={"body of " + card_b} />
        <Card title={card_c} body={"body of " + card_c} />
    </stack>

component Dashboard():
    render <panel title="Dashboard">
        <stack direction="vertical">
            <Counter initial={0} label="Clicks" />
            <TrafficLight />
            <CardGrid card_a="A" card_b="B" card_c="C" />
        </stack>
    </panel>
```

### 9.5 Recursive Component — Self-Referencing Tree

```kn
component RecursiveTree(depth: Int, label: String):
    state expanded: Bool = false

    fn has_children(_self: Self_) -> Bool:
        return _self.depth > 0

    fn display_label(_self: Self_) -> String:
        return _self.label + " (depth " + str(_self.depth) + ")"

    fn child_label_l(_self: Self_) -> String:
        return _self.label + ".L"

    fn child_label_r(_self: Self_) -> String:
        return _self.label + ".R"

    fn child_depth(_self: Self_) -> Int:
        return _self.depth - 1

    render <stack direction="vertical">
        <text value={display_label()} />
        if has_children():
            <RecursiveTree depth={child_depth()} label={child_label_l()} />
            <RecursiveTree depth={child_depth()} label={child_label_r()} />
        else:
            <text value="LEAF" />
    </stack>
```

### 9.6 Pointer-Laden Component — Raw Memory Management

Components with `with Unsafe` can manage raw buffers:

```kn
component MemoryWidget(cell_count: Int) with Unsafe:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")
    state checksum: Int = 0

    fn alloc_buffer(_self: Self_) -> Int:
        _self.buffer = alloc_zeroed(_self.cell_count, "Int")
        collapse _self.buffer:
            var i: Int = 0
            while i < _self.cell_count:
                mem_store(ptr_offset(_self.buffer, i, "Int"), i * 31 + 7, "Int")
                i = i + 1
            0
        let obs = observe _self.buffer:
            var acc: Int = 0
            var j: Int = 0
            while j < _self.cell_count:
                acc = acc + mem_load(ptr_offset(_self.buffer, j, "Int"), "Int")
                j = j + 1
            acc
        _self.checksum = obs
        return obs

    fn display_checksum(_self: Self_) -> String:
        return "mem[" + str(_self.cell_count) + "] checksum: " + str(_self.checksum)

    render <box>
        <text value={display_checksum()} />
    </box>
```

### 9.7 World-Bound Component — Full Authority + View

```kn
// ── State Authority ──
world FuzzAuthority:
    state signal: Int = 42
    state epoch: Int = 0
    state fuzz_score: Int = 0
    surface native_ui => FuzzPanel

// ── Mirror ──
world FuzzMirror:
    state signal_copy: Int = 42
    state epoch_copy: Int = 0
    state fuzz_score_copy: Int = 0
    surface web => FuzzPanel

entangle FuzzAuthority.signal <-> FuzzMirror.signal_copy with single_writer
entangle FuzzAuthority.epoch <-> FuzzMirror.epoch_copy with single_writer

// ── Law for invariant ──
law fuzz_score_valid(v: Int) -> Bool:
    return v >= 0 and v < 1000000007

// ── Patch for mutation ──
patch fuzz_commit(authority: FuzzAuthority, value: Int) -> Int:
    authority.fuzz_score = value
    authority.epoch = authority.epoch + 1
    return authority.fuzz_score

// ── Pulse for live updates ──
pulse fuzz_beat every 100ms jitter 10ms:
    FuzzAuthority.signal = (FuzzAuthority.signal + pulse_tick * 7) % 1000000007

// ── Component as View ──
component FuzzPanel():
    render <panel title="Component Fuzz Dashboard" />
```

### 9.8 Deeply Nested Composition (10 Levels)

```kn
component Nest_L0():
    render <box><text value="L0" /></box>

component Nest_L1():
    render <box><Nest_L0 /><text value="L1" /></box>

component Nest_L2():
    render <box><Nest_L1 /><text value="L2" /></box>

// ... continues through L10 ...

component Nest_L10():
    render <panel title="L10 -- DEEPEST NEST">
        <Nest_L9 />
        <Atom />
        <Blank />
        <text value="bottom of the rabbit hole" />
    </panel>
```

### 9.9 Full Win32 GDI App (Real code from `component_minimal`)

From `blades/kain/component_minimal/src/app.kn` — a complete Windows application using components, Win32 GDI via `include <windows.h> as win`, and manual memory management:

```kn
component Counter(label: String):
    render <box><text value={label} /></box>

component Toggle(label: String):
    render <box><text value={label} /></box>

component Indicator():
    render <box><text value="GREEN" /></box>

component Dashboard():
    render <panel title="Dashboard">
        <stack direction="vertical">
            <box><text value="running" /></box>
            <Counter label="Clicks" />
            <Toggle label="Debug" />
            <Indicator />
        </stack>
    </panel>
```

The full file also includes `fill_rect()`, `draw_text()`, `fast_invsqrt()`, `create_window()`, and a Win32 message pump with `PeekMessageA`/`TranslateMessage`/`DispatchMessageA` via `@extern` — all in one file with components as the UI layer.

______________________________________________________________________

## 10. Surface Kinds

A world's `surface` declaration selects the rendering backend for the component:

| Surface Kind | Typical Backend | Use Case |
|-------------|-----------------|----------|
| `native_ui` | Desktop native (Win32/Vulkan/Kaintana desktop) | Main application window |
| `web` | WASM / web target | Browser-based UI |
| `viewport3d` | 3D viewport (via `crates/3d`) | In-world 3D UI, HUD, debug overlays |
| `ue5` | Unreal Engine 5 integration | Game engine editor panels |

```kn
world MultiSurfaceWorld:
    state value: Int = 0

    surface native_ui => App          // Desktop window
    surface web => App                // Same component, web target
    surface viewport3d => "HUD"       // 3D viewport (string key)
```

The **same component** can be rendered through multiple surface kinds — the component author doesn't need to know which backend renders it.

______________________________________________________________________

## 11. Under-Utilized Ideas

Components are used far below their potential in most examples. Here are patterns that exist in the codebase but are not common:

### 11.1 Components Without Worlds

Most examples pair one component with one world. But components are free-standing declarations. A library module can export 50 components with zero worlds:

```kn
// ui_kit.kn — pure component library, no worlds
pub component Button(label: String, kind: String):
    render <box><text value={label} /></box>

pub component Card(title: String, body: String):
    render <box>
        <text value={title} />
        <text value={body} />
    </box>

pub component Divider():
    render <box><text value="—" /></box>
```

### 11.2 Computational Components

Components can do real work in their methods — sieves, factorials, sequence generation — and render the result:

```kn
component PrimeSieve(limit: Int):
    state primes: Int = 0
    state last_prime: Int = 0

    fn sieve(_self: Self_):
        var n: Int = 2
        while n <= _self.limit:
            var is_prime: Bool = true
            var d: Int = 2
            while d * d <= n:
                if n % d == 0:
                    is_prime = false
                    break
                d = d + 1
            if is_prime:
                _self.primes = _self.primes + 1
                _self.last_prime = n
            n = n + 1

    fn summary(_self: Self_) -> String:
        return "primes <= " + str(_self.limit) + ": " + str(_self.primes)

    render <stack direction="vertical">
        <text value={summary()} />
        <text value={"last: " + str(_self.last_prime)} />
    </stack>
```

### 11.3 Recursive Components

Components can call themselves recursively. The `RecursiveTree` pattern (up to 10 levels deep) is verified working:

```kn
component LayoutInception(depth: Int):
    fn has_depth(_self: Self_) -> Bool:
        return _self.depth > 0

    fn level_label(_self: Self_) -> String:
        return "level " + str(_self.depth)

    render <stack direction="vertical">
        <text value={level_label()} />
        if has_depth():
            <LayoutInception depth={depth - 1} />
        else:
            <Atom />
    </stack>
```

### 11.4 Pointer-Managing Components

Components with `with Unsafe` can own raw memory, GPU buffers, and shared resources. This is used in the component fuzz suite but rarely in production code:

```kn
component MemoryWidget(cell_count: Int) with Unsafe:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")
    state checksum: Int = 0
    // uses collapse/observe/decay for buffer lifecycle
```

### 11.5 Fragment-Only Components

Components that return only `<Fragment>` nodes — useful for composition factories:

```kn
component PureComposition():
    render <Fragment>
        <Atom />
        <Blank />
        <ExprOnly value={42} />
        <Toggle />
    </Fragment>
```

### 11.6 Hot-Reload-Aware Components

The `std::ui` module exposes hot reload functions (`native_ui_hot_reload_begin`, `native_ui_hot_reload_commit`, etc.). Components can participate in hot-reload workflows when paired with Kaintana or a compatible runtime.

______________________________________________________________________

## 12. Combining with Other Keywords

### Component + World

The canonical pairing. World holds state authority; component renders the view:

```kn
world AppWorld:
    state counter: Int = 0
    surface native_ui => App

component App():
    render <panel title="Counter App" />
```

### Component + Patch

Patches mutate world state that surfaces to a component:

```kn
patch increment(world: AppWorld) -> Int:
    world.counter = world.counter + 1
    world.epoch = world.epoch + 1
    return world.counter
```

### Component + Entangle

Entangle propagates world state to mirror worlds that surface different component views:

```kn
entangle Authority.value <-> Mirror.value_copy with single_writer
// Authority → native_ui component
// Mirror → web component (same or different)
```

### Component + Resonate

Resonate reacts to world state changes and can trigger component re-renders:

```kn
resonate World.counter on_counter_change dampen 16ms:
    // counter changed — component re-renders via surface
```

### Component + Pulse

Pulse drives periodic world updates that flow into the component surface:

```kn
pulse tick every 16ms:
    World.counter = World.counter + 1
```

### Component + Shader

Components can manage GPU resources and work with shaders:

```kn
component RenderView() with GPU:
    // GPU resource management + shader dispatch
    render <panel title="GPU View" />
```

### Component + Teleport

Components are the typical surface for visualizing teleported data. The smoke test shows this pattern:

```kn
component SmokeTeleportPanel():
    render <panel title="Smoke Teleport" />

world SmokeTeleportAuthority:
    state signal: Int = 1
    surface web => SmokeTeleportPanel
```

### Component + Actor

Components can display data from actors and interact with them:

```kn
actor ComponentActor:
    state echo_count: Int = 0
    on Echo(reply_to: P, message: Int):
        self.echo_count = self.echo_count + 1
        send reply_to.Reply(value = message + self.echo_count)

component ActorWidget(signal: Int):
    state last_reply: Int = 0
    fn reply_str(_self: Self_) -> String:
        return str(_self.last_reply)
    render <box>
        <text value={"actor echo: " + reply_str()} />
    </box>
```

### Component + Orchestrate

Orchestrate pipelines can include stages that target component rendering surfaces:

```kn
orchestrate Pipeline:
    stage compute: cpu { ... }
    stage render: world { surface native_ui => App }
```

______________________________________________________________________

## 13. Validation and Error Diagnostics

### Compiler Checks

The parser and typechecker in `crates/core/` enforce:

| Check | Error Message |
|-------|--------------|
| Missing `render` body | `"component 'Name' must have a render body"` |
| Unknown prop type | Type error on JSX attribute type mismatch |
| Unresolved component | `"no component named 'Foo' in scope"` for uppercase tags |
| Self in JSX | `"_self is not in scope inside JSX"` (use methods) |
| weak in component | `"weak state is actor-only, not available in components"` |
| Shadowed builtin | `"component name 'Void' shadows builtin type"` |
| Duplicate component name | `"component 'Name' is defined multiple times"` |

### Surface Validation

Worlds require at least one surface:

```kn
= note: Worlds currently need at least one surface projection so they
        can materialize into a runtime-facing view.
= help: Add a line like `surface native_ui => MyPanel` inside the world body.
```

Duplicate surfaces are caught:

```kn
= error: world 'Name' declares duplicate 'native_ui' surface
```

### Typechecking JSX

The typechecker (`crates/core/src/types.rs`) validates:

- Attribute types match prop types
- Component exists in scope
- Condition expressions return `Bool`
- `for` loop iterables are iterable types
- Expression interpolation types are compatible

### Common Compilation Failures

| Symptom | Likely Cause |
|---------|-------------|
| `_self not in scope` | Referencing `_self` directly in JSX `{expr}` |
| `type mismatch` | JSX attribute type doesn't match prop type |
| `no component named X` | Uppercase tag with no matching declaration |
| `weak not available` | Using `state weak x: Type` in a component (actor-only) |
| `unexpected token` | Using `{/* comment */}` in JSX |
| `expected Bool` | Using comparison operators in JSX `if` condition |

______________________________________________________________________

## 14. Source Anchors

### Language Truth — Parser and AST

| File | Line(s) | What |
|------|---------|------|
| `crates/core/src/parser.rs` | 59 | `"component"` keyword registration |
| `crates/core/src/parser.rs` | 429 | `TokenKind::Component ⇒ "keyword 'component'"` |
| `crates/core/src/parser.rs` | 762 | Component token handler in token display |
| `crates/core/src/parser.rs` | 996 | `TokenKind::Component ⇒ self.parse_component_with_attrs(vis, attributes)` |
| `crates/core/src/ast.rs` | 153 | `/// component Name(props) -> UI with Reactive: jsx` |
| `crates/core/src/ast.rs` | 543–555 | `pub struct Component { name, props, state, methods, effects, body, visibility, attributes, span }` |
| `crates/core/src/ast.rs` | 557–564 | `pub struct StateDecl { name, ty, initial, weak, attributes, span }` |
| `crates/core/src/ast.rs` | 566–602 | `JSXNode` enum: `Element`, `Expression`, `Text`, `ComponentCall`, `For`, `If`, `Fragment` |
| `crates/core/src/ast.rs` | 604–617 | `JSXAttribute`, `JSXAttrValue` (String, Expr, Bool) |

### Typechecking

| File | What |
|------|------|
| `crates/core/src/types.rs` | Typechecking components, JSX attribute validation, prop type checking |

### Code Generation

| File | What |
|------|------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | LLVM codegen for component declarations and JSX trees |

### Runtime / Stdlib

| File | What |
|------|------|
| `stdlib/ui.kn` | `std::ui` module — 340+ public symbols, native UI session, node, resource, event, accessibility, DPI, drag/drop, IME, menu, dialog, hot-reload API |
| `runtime/native/include/ui.h` | Native ABI for UI rendering (session management, node tree, draw commands) |
| `runtime/native/src/core/ui_system.c` | Native UI system implementation |

### Real Examples

| File | What |
|------|------|
| `blades/kain/component_minimal/src/app.kn` | **GOLD MINE**: 280+ line Win32 GDI app with components, `include <windows.h> as win`, manual memory, `@extern`, full message pump |
| `blades/kain/component_fuzz/src/components.kn` | Component fuzz test — 40+ components across 10 categories (naked, stateful, computational, recursive, fragments, pointer-laden, actor-aware, deeply nested, world-surface, singularity) |
| `blades/kain/component_fuzz/src/main.kn` | Fuzz main entry — demonstrates that components live in JSX only |
| `blades/kain/component_shader/` | Component + shader integration example |
| `blades/kaintana/` | Kaintana framework built on top of components |
| `blades/templates/python/pygame/src/world.kn` | Template using world + component + Python interop |
| `blades/templates/python/ursina/src/world.kn` | Template using world + component + Ursina engine |

### Smoketests

| File | What |
|------|------|
| `smoketest/src/semantics/teleport.kn` | Component in smoke test context: `SmokeTeleportPanel` with world surfaces |
| `benchmark/cases_v2/keyword_crucible.kn` | Component keyword exercised alongside 108 others — `CrucibleView` component |

### Documentation

| File | What |
|------|------|
| `RULEBOOK.md` (lines 90–295) | Full component section with anatomy, JSX composition, control flow, decision table, anti-patterns, component vs Kaintana |
| `GLOSSARY.MD` | Glossary entries: `component`, `Kaintana`, `surface` |
| `WORLD.MD` | Surface declarations, world→component wiring |
| `ENTANGLE.MD` | Entangle + component surfaces pattern |
| `PATCH.MD` | Patches + component surfaces |
| `ACTOR.MD` | Actor + component integration examples |

______________________________________________________________________

## 15. Anti-Patterns

### ❌ Component as World Decoration Only

**Wrong:** Using component ONLY as a single-line `render <panel>` wrapper for world surfaces, never composing components, never using state or methods.

```kn
// Underutilized — component is just a title string
component App():
    render <panel title="My App" />
```

**Right:** Components as the full UI composition layer.

```kn
// Component as UI composition — the full model
component MetricCard(title: String, value: Int, unit: String):
    state expanded: Bool = false
    fn display_value(_self: Self_) -> String:
        return str(_self.value) + " " + _self.unit
    render <box>
        <text value={title} />
        <text value={display_value()} />
    </box>

component Dashboard(signal: Int, hot: Int):
    render <panel title="Neural Scope">
        <stack direction="horizontal">
            <MetricCard title="Signal" value={signal} unit="ms" />
            <MetricCard title="Hot Synapses" value={hot} unit="nodes" />
        </stack>
    </panel>
```

### ❌ Never Composing Components

**Wrong:** Every file has exactly one component. No nesting, no reuse.

```kn
// Each component is a silo — no composition
component App():
    render <panel title="Tiny App"><text value="hello" /></panel>
```

**Right:** Break UI into composable pieces.

```kn
// Composable widgets
component Button(label: String):
    render <box><text value={label} /></box>

component Toolbar():
    render <stack direction="horizontal">
        <Button label="New" />
        <Button label="Open" />
        <Button label="Save" />
    </stack>

component App():
    render <panel title="Editor">
        <Toolbar />
        <text value="Content area" />
    </panel>
```

### ❌ Treating Component as Tied to World

**Wrong:** Assuming every component must have a `world` with `surface native_ui => ComponentName`.

```kn
// Unnecessary coupling
world MustExist:
    state placeholder: Int = 0
    surface native_ui => MyComponent

component MyComponent():
    render <text value="Why do I need a world?" />
```

**Right:** Components exist independently. Only add a world when you need entangle/patch/law/cross-frame state.

```kn
// Free-standing component — no world needed
component MyComponent():
    render <text value="I exist on my own" />
```

### ❌ Putting Logic in JSX Instead of Methods

**Wrong:** Complex expressions or control flow inside JSX `{expr}`.

```kn
// ❌ Fragile, hard to test, limited by compiler
render <text value={if a > b: "big" else: "small"} />
```

**Right:** Move logic to methods.

```kn
// ✅ Testable, reusable, full Kain control flow
fn comparison(_self: Self_) -> String:
    if _self.a > _self.b:
        return "big"
    return "small"

render <text value={comparison()} />
```

### ❌ Mutating State Directly in JSX

**Wrong:** Trying to call state-mutating functions in JSX.

```kn
// ❌ Side effects in render — undefined behavior
render <box>
    <text value={increment()} />
</box>
```

**Right:** Keep mutations in explicit methods, called from events or pulses.

```kn
fn bump(_self: Self_):
    _self.count = _self.count + 1

// Called from event handlers, not from render
render <text value={count_str()} />
```

### ❌ Using `weak` in Components

**Wrong:** `state weak` is actor-only syntax.

```kn
// ❌ Compiler error
component Bad():
    state weak cache: Int = 0
```

**Right:** Use regular `state` in components. Weak references are for actor systems only.

______________________________________________________________________

## 16. Quick Reference

### Keyword Syntax

```kn
component Name(prop_a: Type, prop_b: Type):
    state field: Type = initial
    state another: Type = initial

    fn method(_self: Self_) -> ReturnType:
        // read/write _self.field, _self.prop_a
        return value

    render <jsx>…</jsx>
```

### JSX Quick Reference

| Syntax | Meaning |
|--------|---------|
| `<panel title="x">` | Native element, string attr |
| `<text value={expr} />` | Native element, expression attr |
| `<Button label="x" />` | Component call, uppercase = dispatch |
| `{expression}` | Expression interpolation |
| `for item in list:` | Loop inside JSX |
| `if cond:` | Conditional inside JSX |
| `<Fragment>...</Fragment>` | Multiple root nodes |

### Compile Commands

```bash
kain check src/app.kn              # Typecheck components and JSX
kain build src/app.kn              # Compile to LLVM
kain run src/app.kn                # Compile and execute
kain native src/app.kn --emit exe  # Produce native executable
```

### File Map

```
📁 stdlib/ui.kn                     → std::ui runtime (340+ symbols)
📁 blades/kain/component_minimal/   → Full Win32 GDI app with components
📁 blades/kain/component_fuzz/      → 40+ component stress tests
📁 blades/kain/component_shader/    → Component + shader integration
📁 blades/kaintana/                 → UI framework built on components
📁 crates/core/src/ast.rs           → Component/JSX AST definitions
📁 crates/core/src/parser.rs        → Component/JSX parsing
📁 crates/core/src/types.rs         → Component/JSX typechecking
📁 crates/sys-codegen/              → LLVM codegen for components
📁 benchmark/cases_v2/              → keyword_crucible.kn (component keyword)
📁 smoketest/src/semantics/         → Component in smoke test context
```

______________________________________________________________________

*Component is the UI layer of Kain. It is the language's answer to "how do I build a screen?" — a first-class, compiler-owned, world-independent UI abstraction with typed props, local state, methods, JSX composition, and multiple surface backends. The more you compose, state-manage, and method-encapsulate in your components, the more the compiler can help you.*
