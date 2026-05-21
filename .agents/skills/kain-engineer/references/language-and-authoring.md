# Language and Authoring Guide

## Authoring Model

Kain source is normally authored in `.kn` files.

The frontend in `M:\Code\Kain\crates\kain-core` is:

- indentation-aware
- multi-error tolerant up to a capped error count
- strongly target-aware through effects, low-level diagnostics, and backend capability checks

Do not treat Kain as "just syntax that later becomes something else". The frontend already encodes important semantics about purity, async behavior, actors, components, memory, and target compatibility.

## Common Top-Level Items

You will regularly encounter or author:

- `fn`
- `const`
- `struct`
- `enum`
- `trait`
- `impl`
- `type`
- `actor`
- `component`
- `shader`
- `mod`
- `use`
- `comptime`
- `test`

If you are about to invent a new top-level construct or syntax, inspect `parser.rs` first.

## High-Signal Language Features

### Functions and effects

Effects are part of the function signature. Common examples include `Pure`, `Async`, `IO`, `Unsafe`, and `Reactive`.

```kain
fn factorial(n: Int) -> Int with Pure:
    match n:
        0 => 1
        _ => n * factorial(n - 1)
```

```kain
async fn fetch_user(id: Int) -> String with Async, IO:
    http_get("/api/user/" + to_string(id))
```

Guidance:

- Keep pure logic isolated from I/O wrappers when possible.
- Be explicit with effects instead of hiding side effects inside helper layers.

### Structs, enums, traits, impls

```kain
struct Player:
    name: String
    hp: Int

enum Action:
    Idle
    Move(Int, Int)

trait Drawable:
    fn draw(self) -> Unit
```

Kain supports rich enum and pattern workflows. Traits and impls are important for target-agnostic domain modeling.

### Pattern matching

Pattern matching is a core idiom, not an edge feature.

```kain
fn handle(action: Action) -> String with Pure:
    match action:
        Action::Idle => "idle"
        Action::Move(x, y) => "move"
```

### Generics

Kain uses generics across functions, structs, traits, and impls. If you are changing generic behavior, inspect monomorphization and type-checking code instead of assuming a backend-specific issue.

### Components and JSX-like rendering

Components are first-class items, parsed directly by the frontend.

```kain
component Counter(initial: Int) -> UI with Reactive:
    state count: Int = initial
    render:
        <div>{count}</div>
```

Important details:

- components are not just macros
- `state` is a contextual keyword inside component bodies
- direct JSX in render positions is real parser behavior

### Actors

Actor-style concurrency is part of the language model. Use actors when the design really wants mailbox-style ownership and message passing rather than shared mutable state.

### Shaders

Shaders are first-class items and can flow to `spirv`, `hlsl`, `usf`, and related artifact generators.

### `comptime`

Compile-time execution exists and matters. If a change touches compile-time behavior, verify against frontend and type system code, not only backend output.

## Authoring Patterns That Age Well

- Keep target-neutral domain logic separate from thin target-specific adapters.
- Use explicit modules instead of one giant file once a subsystem grows.
- Prefer data-driven tables, registries, and manifests over hardcoded mappings.
- When adding importer or backend support, preserve stable naming and deterministic output.
- Use dedicated low-level constructs only where needed. Do not leak pointer-heavy code into broadly portable modules unless that is truly the goal.

## Reserved Names and Imported Code

Kain reserves more names than a small hobby language usually would because the parser and downstream targets must stay compatible with:

- Kain syntax
- HLSL and shader namespaces
- C++ and UE5 macro-heavy ecosystems

This matters when importing foreign code:

- name collisions are common
- identifier sanitization is expected
- stable renaming matters for repeatable imports

If an imported identifier suddenly changes shape, check the importer's identifier registry before blaming the parser.

## What Not to Guess

Do not guess the exact syntax of rarely used features if you have not verified them in source or tests. In particular:

- test item syntax
- obscure attribute forms
- low-level memory sugar
- macro-like or self-host-only constructs

When unsure, inspect:

- `M:\Code\Kain\crates\kain-core\src\parser.rs`
- `M:\Code\Kain\crates\kain-core\KAIN_FEATURES_PART1.md`
- `M:\Code\Kain\crates\kain-core\KAIN_FEATURES_PART2.md`

## Practical Validation

For ordinary authoring changes:

```powershell
kain run path\to\file.kn
kain build path\to\file.kn --target ks
kain build path\to\file.kn --target rust
```

Pick at least one cheap target and one realistic target for the subsystem you are changing.
