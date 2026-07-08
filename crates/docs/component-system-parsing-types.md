# Kain Component & JSX System: Parsing and Typechecking

**Date:** 2026-07-05
**Status:** Comprehensive reference from AST through parser through typechecker
**Based on:** Live analysis of `crates/core/src/ast.rs`, `parser.rs`, `types.rs`, `lexer.rs`

---

## 1. AST Definitions

### 1.1 Top-level representation: `Item::Component`

Components are top-level items in the AST, on par with functions, worlds, actors, etc.
The `Item` enum at `crates/core/src/ast.rs:123` defines 29 variants. Component is variant #15:

```rust
// ast.rs:154
/// `component Name(props) -> UI with Reactive: jsx`
Component(Component),
```

The same `Item` enum includes `World(WorldDef)` (variant #8, line 140), `Fn(Function)` (variant #1, line 125), etc.

### 1.2 `Component` struct (`ast.rs:546`)

```rust
pub struct Component {
    pub name: String,
    pub props: Vec<Param>,
    pub state: Vec<StateDecl>,
    pub methods: Vec<Function>,
    pub effects: Vec<Effect>,
    pub pulses: Vec<PulseDef>,
    pub resonates: Vec<ResonateDef>,
    pub dimensions: Option<ComponentDimensions>,
    pub body: JSXNode,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}
```

| Field | Purpose |
|-------|---------|
| `name` | Component identifier, e.g. `MyPanel` |
| `props` | Parameters in parens: `(title: String, count: Int)`. Uses `Param` (same as function params) |
| `state` | Local mutable state: `state count: Int = 0` |
| `methods` | `fn`-declared methods on the component, used as event handlers |
| `effects` | Effect annotations from the component signature (e.g. `Reactive`, `IO`) |
| `pulses` | Inline `pulse` definitions in the component body |
| `resonates` | Inline `resonate` definitions in the component body |
| `dimensions` | Optional `width=<expr>[, height=<expr>]` overrides |
| `body` | The JSX render tree (required—parser errors if absent) |
| `visibility` | `pub` or private |
| `attributes` | Attribute annotations |

### 1.3 `ComponentDimensions` (`ast.rs:564`)

```rust
pub struct ComponentDimensions {
    pub width: Option<Expr>,
    pub height: Option<Expr>,
    pub span: Span,
}
```

Parsed from `width=<expr>[, height=<expr>]` in the component header. When `None`, the renderer uses platform defaults (e.g. 1024×768).

### 1.4 `StateDecl` (`ast.rs:571`)

```rust
pub struct StateDecl {
    pub name: String,
    pub ty: Type,
    pub initial: Expr,
    pub weak: bool,           // `weak state name: Type = value`
    pub attributes: Vec<Attribute>,
    pub span: Span,
}
```

Supports both `state count: Int = 0` and `weak state cache: Map = {}`.

### 1.5 `JSXNode` enum (`ast.rs:581`)

The JSX tree is a recursive enum with 6 variants:

```rust
pub enum JSXNode {
    /// `<tag attr="value">children</tag>` — lowercase-native element
    Element { tag: String, attributes: Vec<JSXAttribute>, children: Vec<JSXNode>, span: Span },
    /// `{expression}` — interpolation
    Expression(Box<Expr>),
    /// Plain text
    Text(String, Span),
    /// `<Component prop={value} />` — uppercase component call
    ComponentCall { name: String, props: Vec<JSXAttribute>, children: Vec<JSXNode>, span: Span },
    /// `for item in list: jsx` — loop
    For { binding: String, iter: Box<Expr>, body: Box<JSXNode>, span: Span },
    /// `if cond: jsx [else: jsx]` — conditional
    If { condition: Box<Expr>, then_branch: Box<JSXNode>, else_branch: Option<Box<JSXNode>>, span: Span },
    /// Fragment wrapper — `<></>` or `<Fragment></Fragment>`
    Fragment(Vec<JSXNode>, Span),
}
```

Key design decisions:

- **`Element` vs `ComponentCall`**: Distinguished by the first character of the tag. Uppercase → `ComponentCall`, lowercase → `Element`. This mirrors React's convention.
- **`Fragment`**: Created either via `<>children</>` or `<Fragment>children</Fragment>`. The tag name check in `finish_jsx_node` (line 7473) maps `"Fragment"` to `JSXNode::Fragment`.
- **`For`** and **`If`**: Control flow constructs usable inside JSX (only within `{...}` blocks). No standalone `for`/`if` outside braces in JSX.
- **`Text`**: Accumulated from keywords, identifiers, punctuation, and string literals that appear as direct children in JSX. Whitespace between tokens is collapsed to a single space.
- **`Expression`**: Any `{expr}` interpolation that isn't `if`/`for`.

### 1.6 `JSXAttribute` and `JSXAttrValue` (`ast.rs:618-633`)

```rust
pub struct JSXAttribute {
    pub name: String,
    pub value: JSXAttrValue,
    pub span: Span,
}

pub enum JSXAttrValue {
    String(String),                    // fill="#FF0000"
    Expr(Expr),                        // pad={16}, value={self.count}
    Bool(bool),                        // (reserved, not yet produced by parser)
    Callback(String, Box<Expr>),       // on_click={handler_fn} — event_kind + handler expr
}
```

**How attribute types are differentiated during parsing** (`parser.rs:7167-7195`):

1. If the value starts with `{` (LBrace), it's parsed as an expression.
   - If the attribute name is in `EVENT_CALLBACK_ATTRS`, the value is wrapped as `JSXAttrValue::Callback`.
   - Otherwise it's `JSXAttrValue::Expr`.
2. If the value is a string literal `"..."`, it becomes `JSXAttrValue::String`.
   - Unless the attribute is a callback name (e.g. `on_click="handler_name"`), in which case it's treated as `JSXAttrValue::Callback` with the string as an `Expr::Ident`.
3. `Bool` variant exists but is not currently produced by the parser.

### 1.7 `EVENT_CALLBACK_ATTRS` (`ast.rs:636-640`)

```rust
pub const EVENT_CALLBACK_ATTRS: &[&str] = &[
    "on_click", "on_change", "on_toggle", "on_focus", "on_blur",
    "on_mouseenter", "on_mouseleave", "on_submit", "on_cancel",
    "on_hover", "on_press", "on_release", "on_drag",
];
```

### 1.8 `Expr::JSX` (`ast.rs:2251`)

JSX can appear as an expression anywhere (not just in component bodies):

```rust
Expr::JSX(JSXNode, Span),
```

This allows patterns like `let node = <div>hello</div>` though this is primarily used internally (e.g., JSX inside `{...}` interpolation within JSX itself; see parser line 7170-7172).

---

## 2. Parser

### 2.1 Component parsing entry point

**File:** `crates/core/src/parser.rs`

On encountering `TokenKind::Component`, the parser dispatches to `parse_component_with_attrs` (line 996).

The overall flow at `parse_item`:
```rust
TokenKind::Component => self.parse_component_with_attrs(vis, attributes)
```

### 2.2 `parse_component` (`parser.rs:2610`)

**Full signature parsing:**
```
component Name(props) with Effects [width=expr, height=expr]:
    state count: Int = 0
    weak state cache: Map = {}
    fn handle_click(self) -> Void:
        ...
    pulse heartbeat every 1s:
        ...
    resonate World.field dampen 16ms:
        ...
    render:
        <div>...</div>
```

**Parsing steps:**

1. **Header** (lines 2612-2624): `component Name(props) with Effects [width=..., height=...]:`
   - Consumes `component` keyword
   - Parses the component name (identifier)
   - Parses parameters in parens via `parse_params()`
   - Parses effect annotations via `parse_effects()`
   - Parses optional dimension overrides via `parse_component_dimensions()`
   - Expects `:` then `Indent`

2. **Body** (lines 2627-2761): Within the indented block, parses items sequentially:
   - `fn` → method (parser calls `parse_function`)
   - `state` keyword → state declaration (line 2645)
   - `weak state` → weak state declaration (line 2676)
   - `pulse` as ident → inline pulse (line 2706)
   - `resonate` as ident → inline resonate (line 2711)
   - `render` as ident → JSX body (line 2716)
   - `<` (Lt) → implicit render → direct JSX (line 2748)
   - Any other ident → error (line 2739)

3. **Error if no body** (line 2767): `Component must have a render body (JSX element)`

### 2.3 `render` block parsing (`parser.rs:2716-2738`)

Three forms of `render` are accepted:

| Form | Example | Parser behavior |
|------|---------|-----------------|
| Braced | `render { <jsx> }` | Expects `{`, parses JSX element, expects `}` |
| Colon + indent | `render:\n    <jsx>` | Expects `:`, `Indent`, parses JSX element, expects `Dedent` |
| Inline (implicit) | `render <jsx>` | Parses JSX element directly |

Additionally, a bare `<` token (without `render`) triggers implicit render parsing at line 2748:
```rust
} else if self.check(TokenKind::Lt) {
    body = Some(self.parse_jsx_element()?);
}
```

### 2.4 JSX element parsing: `parse_jsx_element` (`parser.rs:7159`)

**Full flow:**

1. **Opening tag** (lines 7161-7164): Expect `<`, parse tag name, collect attributes until `>` or `/`.
2. **Attributes** (lines 7165-7201): Each attribute is `name=value`:
   - Name: parsed via `parse_jsx_attribute_name()` (identifiers or any alpha/underscore-starting token)
   - Value: `{expr}`, `"string"`, or error
   - If the attribute name matches `EVENT_CALLBACK_ATTRS`, wrap as `Callback`
3. **Self-closing** (lines 7203-7207): If `/` before `>`, finish immediately with no children.
4. **Children** (lines 7211-7381): Main loop reads children until `</`:
   - `{` → `parse_jsx_braced_child()` (Expression, If, For nodes)
   - `<` → recursive `parse_jsx_element()`
   - Text tokens → accumulated into `text_buffer`
   - Newlines, indents → treated as whitespace (collapsed to space)
   - Keywords (fn, let, if, etc.) → treated as literal text
   - Other tokens (numbers, punctuation) → treated as literal text
5. **Whitespace handling** (lines 7221-7235): Tracks `last_end` position of the previous token. If `current_span.start > last_end`, there's a gap → append a space to the text buffer. This handles `Count is: {count}` correctly.
6. **Closing tag** (lines 7390-7400): Expect `</`, parse closing tag name, verify match, expect `>`.
7. **Node classification** in `finish_jsx_node` (lines 7467-7493):
   - `"Fragment"` → `JSXNode::Fragment`
   - First char uppercase → `JSXNode::ComponentCall`
   - First char lowercase → `JSXNode::Element`

### 2.5 Braced children: `parse_jsx_braced_child` (`parser.rs:7495`)

Inside `{...}`, three constructs are recognized:

| Token | Result |
|-------|--------|
| `if` keyword | `JSXNode::If { condition, then_branch, else_branch? }` |
| `for` keyword | `JSXNode::For { binding, iter, body }` |
| Anything else | `JSXNode::Expression(expr)` |

The `for` syntax is: `{for item in list: <jsx>}` — note the colon separator, not braces.

For `if`, the syntax is: `{if cond: <jsx> else: <jsx>}` or `{if cond: <jsx>}`. The `else` branch is optional.

### 2.6 `parse_jsx_inline_node` (`parser.rs:7539`)

A helper for parsing a single JSX node (used in if/for branches):
- `<` → `parse_jsx_element()`
- `{` → `parse_jsx_braced_child()`
- Otherwise → `Expression(expr)`

### 2.7 Attribute value constraints

The current parser has **no explicit constraint** on single-line vs multi-line attributes. However, the JSX child loop treats newlines/indents as whitespace within the children area. Attribute values are parsed as either `{expr}` or `"string"` — neither of which spans lines naturally due to the lexer's handling.

### 2.8 World surface projection parsing: `parse_world_surface_projection` (`parser.rs:2392`)

```rust
fn parse_world_surface_projection(&mut self) -> KainResult<WorldSurfaceProjection> {
    self.expect_contextual_ident("surface")?;
    let kind = /* identifier — the surface backend name */;
    self.expect(TokenKind::FatArrow)?;    // =>
    let expr = self.parse_expr()?;        // component or component call
    Ok(WorldSurfaceProjection { kind, expr, span })
}
```

This parses:
```
surface native_ui => MyPanel
surface "custom_backend" => MyComponent
surface kaintana => Counter()
```

The `kind` is always a string. The expression can be an `Ident`, `String`, or `Call` expression.

### 2.9 `Expr::JSX` in expressions (`parser.rs:7170-7172`)

When parsing attributes like `content={<div>hello</div>}`, the parser checks for `<` after `{`:
```rust
let e = if self.check(TokenKind::Lt) {
    let jsx = self.parse_jsx_element()?;
    Expr::JSX(jsx, self.current_span())
} else {
    self.parse_expr()?
};
```

---

## 3. Typechecker

### 3.1 `TypedComponent` (`types.rs:226`)

```rust
pub struct TypedComponent {
    pub ast: Component,
    pub prop_types: HashMap<String, ResolvedType>,
    pub state_types: HashMap<String, ResolvedType>,
    pub pulse_types: Vec<TypedPulse>,
    pub resonate_types: Vec<TypedResonate>,
}
```

### 3.2 `check_component` (`types.rs:7943`)

The main component typechecking function:

1. **Resolve prop types** (lines 7944-7947): Each parameter type is resolved via `resolve_param_type()`.

2. **Build self type** (lines 7949-7958): A `ResolvedType::Struct(name, fields)` is constructed containing:
   - All prop fields (from step 1)
   - All state fields (from `c.state` declarations)
   This `self_ty` is what `self.` resolves to in JSX expressions and method bodies.

3. **Scope-based checking** (lines 7963-8014): Inside `env.with_scope`:
   - Props are defined in the environment
   - Methods are registered by name and signature
   - State initializers are type-checked against declared types
   - `self` is defined as the combined struct type
   - Each method is type-checked via `check_function_with_self(env, method, &self_ty)` (line 7991)
   - Inline pulses are type-checked
   - Inline resonates are type-checked
   - `check_jsx_semantics(env, &c.body, None)` validates the JSX render tree (line 8012)

4. **Return value** (lines 8016-8023): `TypedComponent` with resolved prop/state/pulse/resonate types.

### 3.3 `check_function_with_self` (`types.rs:5858`)

Called for each component method. Uses `function_signature(env, f, Some(self_ty))` which resolves the `self` parameter:

```rust
// types.rs:8262
(Type::Infer(_), Some(self_ty)) if param.name == "self" => Ok(self_ty.clone()),
```

This means the first parameter named `self` in a component method gets the component's struct type (`ResolvedType::Struct(ComponentName, {prop: type, state: type, ...})`).

Within the method scope, regular parameter type resolution proceeds, and `self.field` accesses resolve through `field_access_type` → `ResolvedType::Struct` field lookup.

### 3.4 `Self_` handling

Components do **not** use a `Self_` enum variant in the `Type` AST. Instead:
- The `self` parameter is detected by name (`param.name == "self"`) in `resolve_param_type` (line 8262)
- The `self` value is defined in the type environment as `ResolvedType::Struct(name, fields)` at line 7988
- Access to `self.field` works through the standard `Expr::Field` → `field_access_type` path
- `method_has_receiver_param` (line 3328) checks if the first param is named `"self"` or `"_self"`

### 3.5 `check_jsx_semantics` (`types.rs:12755`)

Validates the JSX tree recursively:

```rust
fn check_jsx_semantics(env, node, ctx) -> KainResult<()>
```

| Node variant | Validation |
|-------------|------------|
| `Element` / `ComponentCall` | For each attribute: validate `Expr` values via `infer_expr_type`, validate `Callback` values via `validate_jsx_event_callback`, skip `String`/`Bool`. Recurse into children. |
| `Expression(expr)` | Infer type of the expression |
| `Text(_, _)` | No validation needed |
| `For { binding, iter, body }` | Infer iter type, extract element type, define binding, recurse into body |
| `If { condition, then_branch, else_branch }` | Validate condition is boolean-compatible, recurse into branches |
| `Fragment(children, _)` | Recurse into each child |

**Important:** There is currently no special validation for `ComponentCall` vs `Element`. Both are treated identically — the typechecker simply infers types for attribute expressions and validates callbacks. It does **not** verify that a `ComponentCall` name matches a declared component, nor does it validate props against the component's parameter list. This is a known gap.

### 3.6 `validate_jsx_event_callback` (`types.rs:12697`)

Validates event handler callbacks:

1. **Known event kind** (line 12704): Checks against `KNOWN_JSX_EVENT_KINDS`:
   ```rust
   const KNOWN_JSX_EVENT_KINDS: &[&str] = &[
       "click", "change", "toggle", "focus", "blur",
       "mouseenter", "mouseleave", "submit", "cancel",
       "hover", "press", "release", "drag",
   ];
   ```

2. **Function type** (line 12717): The callback expression must resolve to `ResolvedType::Function { params, ret, .. }`.

3. **Return type** (line 12721): Must be `Unit`, `Never`, or `Unknown` (void-compatible).

4. **Parameter count** (line 12732): Accepts 0, 1, or 2 parameters (rejects 3+).

Example error messages:
- `Unknown JSX event 'swipe'. Known events: click, change, ...`
- `Event callback 'click' must be a function, got String`
- `Event callback 'change' must return Void (unit), found Int`
- `Event callback 'submit' takes too many parameters (expected 0-2, got 3)`

### 3.7 `Expr::JSX` type inference (`types.rs:9888`)

```rust
Expr::JSX(node, _) => {
    check_jsx_semantics(env, node, ctx)?;
    Ok(ResolvedType::Unit)
}
```

JSX expressions always type as `Unit` — they are statements, not values.

### 3.8 State type checking

In `check_component` (lines 7972-7983):
```rust
for state in &c.state {
    let state_ty = state_types.get(&state.name).cloned().unwrap_or(ResolvedType::Unknown);
    let initial_ty = infer_expr_type(env, &state.initial, None)?;
    ensure_type_compatible(env, &state_ty, &initial_ty, state.initial.span(), "component state initializer")?;
    env.define(state.name.clone(), state_ty);
}
```

The declared type must be compatible with the initializer expression type. Mismatch produces a type error with context "component state initializer".

### 3.9 `self.` access in JSX expressions

When the JSX body contains `{self.count}` or `{self.name}`, this works because:
1. `self` is defined in the component scope as `ResolvedType::Struct(ComponentName, fields)`
2. `self.count` desugars to `Expr::Field { object: Ident("self"), field: "count" }`
3. `infer_expr_type` processes `Expr::Field` → `field_access_type` → looks up `"count"` in the struct fields

The same mechanism handles `self.method_name(args)` via `Expr::MethodCall`.

---

## 4. World → Surface → Component Binding

### 4.1 AST representation

The `WorldSurfaceProjection` struct (`ast.rs:430`):
```rust
pub struct WorldSurfaceProjection {
    pub kind: String,   // "native_ui", "kaintana", "vulkan", etc.
    pub expr: Expr,     // The component: MyPanel or Counter()
    pub span: Span,
}
```

The `kind` field was **previously an enum** `WorldSurfaceKind` with variants like `NativeUi`, but it is **now a free-form `String`**. Any registered backend name is valid. This change happened to support pluggable renderer backends without compiler changes.

### 4.2 Parser validation

At `parse_world_surface_projection` (parser.rs:2392):
- The `surface` keyword is parsed contextually (not as a token kind)
- The `kind` must be an identifier — error `ParseInvalidWorldSurfaceKind` is emitted otherwise
- Help text guides: `Example: surface native_ui => MyPanel`
- Note: `Any registered backend name (e.g., 'kaintana', 'vulkan', 'd3d12') is valid.`

### 4.3 Typechecker validation: `check_world` (`types.rs:6683`)

```rust
fn check_world(env: &mut TypeEnv, world: &WorldDef) -> KainResult<TypedWorld> {
    // ...
    for surface in &world.surfaces {
        if !seen_surface_kinds.insert(surface.kind.clone()) {
            return Err(env.type_error(
                format!("world '{}' declares duplicate '{}' surface", world.name, surface.kind),
                surface.span,
            ));
        }
        check_world_surface_projection(env, surface)?;
    }
    // ...
}
```

**Key design decisions:**
- Worlds **without surfaces are valid** (line 6704): pure state authorities for benchmarks, CI, server-mode
- **Duplicate surface kinds** are rejected (lines 6711-6721)
- The `in_world` flag is set on the type environment during world checking (line 6685)

### 4.4 `check_world_surface_projection` (`types.rs:6975`)

```rust
fn check_world_surface_projection(env, surface) -> KainResult<()> {
    match &surface.expr {
        Expr::Ident(_, _) | Expr::String(_, _) => Ok(()),
        Expr::Call { callee, .. } => match callee.as_ref() {
            Expr::Ident(_, _) => Ok(()),
            other => Err(/* "expects a component identifier or call" */),
        },
        other => Err(/* "expects an identifier, string, or component call" */),
    }
}
```

This is **intentionally permissive**: it only validates that the RHS is a valid expression shape (identifier, string, or call). The runtime handles actual binding validation with better error messages than the compiler could produce at typecheck time.

### 4.5 Runtime resolution

When a surface is declared (e.g. `surface native_ui => MyPanel`), the codegen emits a frame loop for that surface. At runtime, the `surface.kind` string determines which backend:
- `"native_ui"` → Win32 app host (platform service `platform.app-host`)
- `"kaintana"` → New UI system in `runtime/native/src/ui_v2/`
- `"vulkan"`, `"d3d12"` → GPU renderer backends

For worlds with no surfaces, the codegen emits no frame loop and no window is created.

---

## 5. Error Diagnostics Summary

### Parser errors

| Context | Error Message |
|---------|--------------|
| Missing render body | `Component must have a render body (JSX element)` |
| Invalid body item | `Unexpected identifier 'X' in component. Valid keywords: 'state', 'weak', 'pulse', 'resonate', 'render', or 'fn' for methods` |
| Invalid token in body | `Unexpected token in component: X. Expected 'state', 'weak', 'pulse', 'resonate', 'render', 'fn', or JSX element` |
| Tag mismatch | `Expected closing tag </div>, found </span>` |
| Bad JSX child token | `Unexpected token in JSX child: X. Use strings or {} for text.` |
| Bad attribute value | `Expected attribute value` |
| Bad surface kind | `Expected a surface kind identifier after 'surface'` (with help text) |
| Missing `=>` | Parse error at `expect(TokenKind::FatArrow)` |
| Duplicate surface | `world 'X' declares duplicate 'Y' surface` |

### Typechecker errors

| Context | Error Message |
|---------|--------------|
| Unknown event | `Unknown JSX event 'X'. Known events: click, change, ...` |
| Non-function callback | `Event callback 'X' must be a function, got Y` |
| Bad callback return | `Event callback 'X' must return Void (unit), found Y` |
| Too many callback params | `Event callback 'X' takes too many parameters (expected 0-2, got N)` |
| State type mismatch | `component state initializer: expected X, found Y` |
| Bad surface expression | `world surface 'X' expects an identifier, string, or component call, found Y` |

### Relaxed checks for worlds

`check_world` does **not** validate that the surface expression actually refers to a declared component or that the component's props are compatible with the surface kind. The rationale (documented at line 6980): "The runtime handles shape mismatches with better error messages than the compiler can produce at the typechecking stage."

---

## 6. File Reference Map

| Concept | File | Lines |
|---------|------|-------|
| `Item::Component` variant | `ast.rs` | 154-155 |
| `Component` struct | `ast.rs` | 546-559 |
| `ComponentDimensions` | `ast.rs` | 564-568 |
| `StateDecl` | `ast.rs` | 571-578 |
| `JSXNode` enum | `ast.rs` | 581-616 |
| `JSXAttribute` | `ast.rs` | 619-623 |
| `JSXAttrValue` enum | `ast.rs` | 626-633 |
| `EVENT_CALLBACK_ATTRS` | `ast.rs` | 636-640 |
| `Expr::JSX` | `ast.rs` | 2251 |
| `Expr::Field` | `ast.rs` | 1906-1910 |
| `WorldDef` | `ast.rs` | 412-419 |
| `WorldSurfaceProjection` | `ast.rs` | 430-434 |
| `TokenKind::Component` | `lexer.rs` | 86 |
| `TokenKind::Fragment` | `lexer.rs` | 108 |
| `TokenKind::LtSlash` | `lexer.rs` | 309 |
| `parse_component` | `parser.rs` | 2610-2788 |
| `parse_component_with_attrs` | `parser.rs` | 2791+ |
| `parse_component_dimensions` | `parser.rs` | 2586-2607 |
| `parse_world` | `parser.rs` | 2267-2309 |
| `parse_world_surface_projection` | `parser.rs` | 2392-2425 |
| `parse_jsx` | `parser.rs` | 7147-7157 |
| `parse_jsx_element` | `parser.rs` | 7159-7401 |
| `parse_jsx_tag_name` | `parser.rs` | 7403-7426 |
| `parse_jsx_attribute_name` | `parser.rs` | 7428-7465 |
| `finish_jsx_node` | `parser.rs` | 7467-7493 |
| `parse_jsx_braced_child` | `parser.rs` | 7495-7537 |
| `parse_jsx_inline_node` | `parser.rs` | 7539-7547 |
| `TypedComponent` | `types.rs` | 226-235 |
| `check_component` | `types.rs` | 7943-8023 |
| `check_function_with_self` | `types.rs` | 5858-5893 |
| `resolve_param_type` | `types.rs` | 8256-8265 |
| `function_signature` | `types.rs` | 8234-8254 |
| `check_world` | `types.rs` | 6683-6728 |
| `check_world_surface_projection` | `types.rs` | 6975-7006 |
| `check_jsx_semantics` | `types.rs` | 12755-12827 |
| `validate_jsx_event_callback` | `types.rs` | 12697-12753 |
| `KNOWN_JSX_EVENT_KINDS` | `types.rs` | 12689-12693 |
| `field_access_type` | `types.rs` | 13339+ |
| `Expr::JSX` type inference | `types.rs` | 9888-9891 |
| `method_has_receiver_param` | `types.rs` | 3328-3333 |
| `TypeEnv.in_world` | `types.rs` | 1085 |
| `ensure_condition_type_compatible` | `types.rs` | 12975+ |

---

## 7. Known Gaps and Future Work

1. **ComponentCall validation**: The typechecker does not validate that `<MyComponent />` refers to a declared component or that its props match the component's parameter list. Both `Element` and `ComponentCall` are validated identically.

2. **Bool attribute values**: The `JSXAttrValue::Bool` variant exists but is not produced by the parser. Boolean attributes like `<input disabled />` are not supported.

3. **Surface-to-component type checking**: `check_world_surface_projection` only validates expression shape, not whether the component exists or its props are compatible.

4. **No prop type validation in JSX**: `validate_jsx_event_callback` validates callback shape but there is no validation that the prop name exists on the target component or element.

5. **Inline JSX expressions**: `Expr::JSX` always returns `Unit`. If someone writes `let x = <div/>`, it typechecks but may not have meaningful semantics.

6. **Whitespace collapsing in text**: The parser collapses whitespace gaps between tokens to a single space. This is heuristic and may produce unexpected results for carefully formatted text content.
