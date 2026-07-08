# Kain Parser Error Reference

**Source:** `KAIN_ERROR_SPECS.md` (parse section, lines 1249–1550) + `crates/core/src/parser.rs` (11,623 lines, 195 error call sites)
**Spec-documented errors:** 20 (KAIN-PARSE-0001 through KAIN-PARSE-0020)
**Compiler-backed codes:** 9 (only KAIN-PARSE-0001 through 0009 have `DiagnosticCode` constants)
**Undocumented call sites:** ~188 (use `ParseGeneric` → KAIN-PARSE-0001)
**Generated:** 2026-06-27 · **Updated:** 2026-06-27 (compiler-source mining)
**Related docs:** `KEYWORDS.MD`, `COMPONENT.MD`, `RULEBOOK.md`

---

## Overview

These 20 diagnostics cover all lexer and parser phases of Kain compilation. Every code maps to a `KAIN-PARSE-NNNN` identifier consumed by the diagnostic registry, terminal renderer, JSON output, and `kain explain`. The parser handles Kain's Python-like significant-newline syntax, `:` block delimiters, contextual keywords (58 hard lexer + 51 contextual), component/JSX parsing, effect annotations, and all literal forms.

---

## Error Index

| Code | Title | Severity | Summary |
|------|-------|----------|---------|
| KAIN-PARSE-0001 | Parse Error | error | Generic fallback — the parser cannot understand the input |
| KAIN-PARSE-0002 | Expected Token | error | A required token (usually `:`) is missing at this position |
| KAIN-PARSE-0003 | Unexpected Token | error | A token appeared in a position where the grammar forbids it |
| KAIN-PARSE-0004 | Reserved Identifier | error | A user-defined name collides with a reserved word |
| KAIN-PARSE-0005 | Missing Delimiter Before Newline | error | Block header missing `:` before the body — the most common parse error |
| KAIN-PARSE-0006 | Invalid World Surface Kind | error | Unknown surface kind in a `world` block (must be native_ui, viewport3d, web, or ue5) |
| KAIN-PARSE-0007 | Expected Contextual Keyword | error | A contextual keyword was expected but not found in the current grammar slot |
| KAIN-PARSE-0008 | Unclosed Delimiter | error | A bracket, brace, or parenthesis was opened but never closed |
| KAIN-PARSE-0009 | Mismatched Delimiter | error | A closing delimiter did not match its opening counterpart |
| KAIN-PARSE-0010 | Invalid Numeric Literal | error | A numeric literal could not be parsed (malformed hex, binary, octal, float) |
| KAIN-PARSE-0011 | Invalid String Literal | error | A string literal is malformed or unescaped |
| KAIN-PARSE-0012 | Invalid Character Literal | error | A character literal contains more than one codepoint |
| KAIN-PARSE-0013 | Attribute Syntax Error | error | `@name` or `@name(args...)` attribute syntax is malformed |
| KAIN-PARSE-0014 | Effect Annotation Syntax Error | error | Effect annotation (`Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`) in an invalid position |
| KAIN-PARSE-0015 | Module Declaration Error | error | `mod` declaration is incomplete (missing name or body/path) |
| KAIN-PARSE-0016 | Use/Import Syntax Error | error | `use` import path is malformed (e.g., double `::`) |
| KAIN-PARSE-0017 | Visibility Modifier Error | error | `pub` stands alone without an attached declaration |
| KAIN-PARSE-0018 | Comptime Block Syntax Error | error | `comptime` block body could not be parsed |
| KAIN-PARSE-0019 | Macro Invocation Syntax Error | error | `macro` definition or invocation is malformed |
| KAIN-PARSE-0020 | Test Declaration Syntax Error | error | `test` block declaration is malformed |

---

## Error Details

### KAIN-PARSE-0001 — Parse Error
- **Severity:** error
- **Category:** parser/general
- **Help:**
  The parser encountered input it cannot understand. This is the generic fallback — most parse errors produce a more specific code.
- **See Also:** KAIN-PARSE-0002, KAIN-PARSE-0003

---

### KAIN-PARSE-0002 — Expected Token
- **Severity:** error
- **Category:** parser/expected-token
- **Help:**
  The parser expected a specific token at this position but found something else. Kain grammar is whitespace-sensitive around block delimiters and uses `:` after block headers.
  
  Fix: insert the missing token before continuing, or restructure the surrounding syntax so the expected token appears in a valid grammar slot.
- **Example Bad:**
  ```kain
  fn main {
      return
  }
  ```
- **Example Good:**
  ```kain
  fn main: {
      return
  }
  ```
- **See Also:** KAIN-PARSE-0005, KAIN-PARSE-0007

---

### KAIN-PARSE-0003 — Unexpected Token
- **Severity:** error
- **Category:** parser/unexpected-token
- **Help:**
  A token appeared in a position where the Kain grammar does not allow it. This often happens when a delimiter is missing, causing the parser to interpret the next construct incorrectly.
  
  Fix: remove the stray token or add the missing delimiter so the token lands in a valid grammar position.
- **Example Bad:**
  ```kain
  let x = 5
    let y = 10
  ```
- **Example Good:**
  ```kain
  let x = 5
  let y = 10
  ```
- **See Also:** KAIN-PARSE-0002

---

### KAIN-PARSE-0004 — Reserved Identifier
- **Severity:** error
- **Category:** parser/reserved-identifiers
- **Help:**
  The identifier you used collides with a word reserved by Kain, HLSL, C++, or an engine host runtime. Reserved identifiers cannot be used as user-defined names.
  
  Fix: rename the identifier. A common convention is to append an underscore or choose a domain-specific synonym.
- **See Also:** KAIN-PARSE-0007

---

### KAIN-PARSE-0005 — Missing Delimiter Before Newline
- **Severity:** error
- **Category:** parser/missing-delimiter-before-newline
- **Help:**
  Kain block headers (`fn`, `if`, `match`, `for`, `while`, `component`, `world`, etc.) require a `:` before the body. A newline appeared before the expected delimiter.
  
  Fix: insert `:` at the end of the header line, or keep the expression on one logical line.
- **Example Bad:**
  ```kain
  fn greet
      return "hi"
  ```
- **Example Good:**
  ```kain
  fn greet:
      return "hi"
  ```
- **Fixit:** `:`
- **See Also:** KAIN-PARSE-0002

---

### KAIN-PARSE-0006 — Invalid World Surface Kind
- **Severity:** error
- **Category:** world/surface-kind
- **Help:**
  `surface` declarations inside a `world` block must use one of the four built-in surface kinds: `native_ui`, `viewport3d`, `web`, or `ue5`.
  
  Fix: replace the unknown kind with a valid surface projection kind.
- **Example Bad:**
  ```kain
  surface desktop => MyPanel
  ```
- **Example Good:**
  ```kain
  surface native_ui => MyPanel
  ```
- **See Also:** KAIN-WORLD-0001

---

### KAIN-PARSE-0007 — Expected Contextual Keyword
- **Severity:** error
- **Category:** parser/contextual-keywords
- **Help:**
  Contextual keywords (`patch`, `law`, `axiom`, `pulse`, `orchestrate`, `converge`, `world`, `entangle`, `shatter`, `teleport`, `every`, `when`, `guarantee`, `fallback`, `spec`, `fast`, `verify`, `random`, `jitter`, `target`, `capability`, `from`, `to`, `via`, `surface`, `compute`, `uniform`, `render`, `on`, `weak`, `single_writer`) only become special in specific grammar slots.
  
  Fix: check whether a nearby identifier or missing delimiter shifted the parser out of the keyword slot.
- **See Also:** KAIN-PARSE-0002, KAIN-PARSE-0004

---

### KAIN-PARSE-0008 — Unclosed Delimiter
- **Severity:** error
- **Category:** parser/unclosed-delimiter
- **Help:**
  A bracket, brace, or parenthesis was opened but never closed. Kain tracks delimiter pairs through the lexer and requires all scopes to be explicitly terminated.
  
  Fix: add the matching closing delimiter at the appropriate nesting level.
- **Example Bad:**
  ```kain
  let x = (a + b
  ```
- **Example Good:**
  ```kain
  let x = (a + b)
  ```
- **See Also:** KAIN-PARSE-0002

---

### KAIN-PARSE-0009 — Mismatched Delimiter
- **Severity:** error
- **Category:** parser/mismatched-delimiter
- **Help:**
  A closing delimiter did not match its opening counterpart (e.g., `]` closing `{`). The lexer tracks delimiter pairs and detected a mismatch.
  
  Fix: align the closing delimiter with the expected opening pair.
- **Example Bad:**
  ```kain
  let arr = [1, 2, 3}
  ```
- **Example Good:**
  ```kain
  let arr = [1, 2, 3]
  ```
- **See Also:** KAIN-PARSE-0008

---

### KAIN-PARSE-0010 — Invalid Numeric Literal
- **Severity:** error
- **Category:** parser/numeric-literal
- **Help:**
  A numeric literal could not be parsed. Kain supports integers (decimal, hex `0x`, binary `0b`, octal `0o`), floats (with `.` or exponent), and type suffixes (`u8`, `i32`, `f32`, `f64`).
  
  Fix: ensure the literal conforms to the supported formats.
- **Example Bad:**
  ```kain
  let x = 0xZZZ
  ```
- **Example Good:**
  ```kain
  let x = 0xFF
  ```

---

### KAIN-PARSE-0011 — Invalid String Literal
- **Severity:** error
- **Category:** parser/string-literal
- **Help:**
  A string literal is malformed. Kain strings use double quotes with backslash escapes (`\n`, `\t`, `\"`, `\\`, `\u{XXXX}`).
  
  Fix: ensure proper escaping and that the string is terminated.
- **Example Bad:**
  ```kain
  let s = "hello
  world"
  ```
- **Example Good:**
  ```kain
  let s = "hello\nworld"
  ```

---

### KAIN-PARSE-0012 — Invalid Character Literal
- **Severity:** error
- **Category:** parser/char-literal
- **Help:**
  A character literal must contain exactly one codepoint (or one escape sequence) between single quotes.
  
  Fix: use a string literal for multi-character sequences.
- **Example Bad:**
  ```kain
  let c = 'ab'
  ```
- **Example Good:**
  ```kain
  let c = 'a'
  ```

---

### KAIN-PARSE-0013 — Attribute Syntax Error
- **Severity:** error
- **Category:** parser/attribute
- **Help:**
  Kain attributes use `@name` or `@name(args...)` syntax. The attribute could not be parsed — check the attribute name and argument list.
  
  Fix: ensure the attribute conforms to `@identifier` or `@identifier(arg, ...)` syntax.
- **Example Bad:**
  ```kain
  @ material_graph
  ```
- **Example Good:**
  ```kain
  @material_graph
  ```

---

### KAIN-PARSE-0014 — Effect Annotation Syntax Error
- **Severity:** error
- **Category:** parser/effect-annotation
- **Help:**
  Effect annotations (`Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`, `Unsafe`) must appear in specific positions on function signatures or type definitions. The parser could not interpret the annotation in this position.
  
  Fix: move the effect annotation to the correct position (before the function return type or on the type definition).
- **See Also:** KAIN-EFFECT-0001, KAIN-EFFECT-0002

---

### KAIN-PARSE-0015 — Module Declaration Error
- **Severity:** error
- **Category:** parser/module
- **Help:**
  `mod` declarations must be followed by an identifier and optionally a body block (`{ ... }`) or a file path string. The parser could not complete the module declaration.
  
  Fix: ensure the module has a valid name and a body or path.
- **Example Bad:**
  ```kain
  mod
  ```
- **Example Good:**
  ```kain
  mod graphics { ... }
  ```

---

### KAIN-PARSE-0016 — Use/Import Syntax Error
- **Severity:** error
- **Category:** parser/use-import
- **Help:**
  `use` declarations import symbols with path syntax (`use a::b::c` or `use a::{b, c}`). The import path could not be parsed.
  
  Fix: ensure the import path uses `::` separators and valid identifiers.
- **Example Bad:**
  ```kain
  use std..math
  ```
- **Example Good:**
  ```kain
  use std::math
  ```

---

### KAIN-PARSE-0017 — Visibility Modifier Error
- **Severity:** error
- **Category:** parser/visibility
- **Help:**
  `pub` must be followed by a declaration (`fn`, `struct`, `enum`, `type`, `mod`, `const`, etc.). It cannot stand alone.
  
  Fix: attach `pub` to a valid declaration.
- **Example Bad:**
  ```kain
  pub
  ```
- **Example Good:**
  ```kain
  pub fn greet: ...
  ```

---

### KAIN-PARSE-0018 — Comptime Block Syntax Error
- **Severity:** error
- **Category:** parser/comptime
- **Help:**
  `comptime` blocks must contain valid Kain expressions that can be evaluated at compile time. The block could not be parsed.
  
  Fix: ensure the comptime block body contains valid, evaluable expressions.
- **See Also:** KAIN-COMPTIME-0001

---

### KAIN-PARSE-0019 — Macro Invocation Syntax Error
- **Severity:** error
- **Category:** parser/macro
- **Help:**
  `macro` definitions and invocations use a specific syntax. The macro could not be parsed — check the macro name, parameter list, and body.
  
  Fix: macros take the form `macro name(params...): { body }`.
- **See Also:** KAIN-COMPTIME-0002

---

### KAIN-PARSE-0020 — Test Declaration Syntax Error
- **Severity:** error
- **Category:** parser/test
- **Help:**
  `test` blocks must define a named test with an optional body. The test declaration could not be parsed.
  
  Fix: `test "name": { body }` or `test "name" { body }`.
- **Example Bad:**
  ```kain
  test: {}
  ```
- **Example Good:**
  ```kain
  test "my test": { assert true }
  ```
- **See Also:** KAIN-TEST-0001

---

## Error Cross-Reference Graph

```
KAIN-PARSE-0001 (generic) ──► 0002, 0003
KAIN-PARSE-0002 (expected token) ──► 0005, 0007
    ├── 0003 (unexpected) → 0002
    ├── 0005 (missing `:`) → 0002
    │   └── fixit: ":"
    ├── 0007 (contextual keyword) → 0002, 0004
    └── 0008 (unclosed delim) → 0002
        └── 0009 (mismatched delim) → 0008
KAIN-PARSE-0004 (reserved id) ──► 0007
KAIN-PARSE-0006 (surface kind) ──► KAIN-WORLD-0001
KAIN-PARSE-0010 (numeric)
KAIN-PARSE-0011 (string)
KAIN-PARSE-0012 (char)
KAIN-PARSE-0013 (attribute)
KAIN-PARSE-0014 (effect annotation) ──► KAIN-EFFECT-0001, KAIN-EFFECT-0002
KAIN-PARSE-0015 (module)
KAIN-PARSE-0016 (use/import)
KAIN-PARSE-0017 (visibility)
KAIN-PARSE-0018 (comptime) ──► KAIN-COMPTIME-0001
KAIN-PARSE-0019 (macro) ──► KAIN-COMPTIME-0002
KAIN-PARSE-0020 (test) ──► KAIN-TEST-0001
```

### Central Hotspots

The most commonly linked errors:

| Code | Referenced By | Why |
|------|:---:|---|
| **KAIN-PARSE-0002** (Expected Token) | 0001, 0003, 0005, 0007, 0008 | The `:` block delimiter is the most common syntax error source |
| **KAIN-PARSE-0007** (Expected Contextual Keyword) | 0002, 0004 | 51 contextual keywords create many grammar-sensitive slots |
| **KAIN-PARSE-0005** (Missing `:` Before Newline) | 0002 | The #1 concrete fix — add `:` at end of header line |

---

## Parser Architecture Context

### Keywords (from KEYWORDS.MD)

The parser must handle **111 keywords**: 58 hard lexer tokens (always reserved) + 51 contextual keywords (only special in specific grammar slots). The overlap between contextual keywords and user-defined identifiers is a major source of confusion — KAIN-PARSE-0007 catches the cases where a keyword was expected but something else was found.

### Component/JSX Parsing (from COMPONENT.MD)

Component declarations are grammar-heavy and produce many parse errors. The parser handles:
- `component Name(props...):` header with `:` delimiter (→ KAIN-PARSE-0005 if missing)
- `state field: Type = initial` body items
- `fn method(_self: Self_):` method signatures (→ KAIN-PARSE-0002 if malformed)
- `pulse tick every 16ms:` timed recurrence (→ KAIN-PARSE-0007 for contextual keywords)
- `resonate World.field dampen 0ms:` reactive tripwires
- `render <jsx>...</jsx>` JSX body with nested components and expressions

### Literal Parsing

Kain supports:
- **Integers:** decimal, hex (`0x`), binary (`0b`), octal (`0o`) with type suffixes (`u8`–`u64`, `i8`–`i64`, `usize`, `isize`)
- **Floats:** decimal point or exponent, with `f32`/`f64` suffixes
- **Strings:** double-quoted with `\n`, `\t`, `\"`, `\\`, `\u{XXXX}` escapes
- **Chars:** single-quoted, exactly one codepoint or escape

Malformed literals trigger KAIN-PARSE-0010 (numeric), KAIN-PARSE-0011 (string), or KAIN-PARSE-0012 (char).

---

## Undocumented Parser Errors (from compiler source)

> **Source:** `crates/core/src/parser.rs` — 195 error call sites across ~70 parsing functions.
> **Generated:** 2026-06-27
> **Method:** Mined every `parser_error(`, `rich_parser_error(`, `rich_parser_error_with_code(`, `rich_parser_report(`, `parser_report_at_code(` call.

### Reality Check: The Spec Gap

**The KAIN_ERROR_SPECS.md defines 20 KAIN-PARSE codes, but `crates/error/src/code.rs` only defines 9 `DiagnosticCode` constants for parse errors (0001–0009).** Codes 0010–0020 exist only in the spec TOML — the compiler has no `DiagnosticCode` constant for them, so they can never be emitted.

| DiagnosticCode Constant | KAIN Code | Where Used in parser.rs |
|-------------------------|-----------|--------------------------|
| `ParseGeneric` | KAIN-PARSE-0001 | Default for `rich_parser_error()` and `parser_error()`. Used by ~188 call sites. |
| `ParseExpectedToken` | KAIN-PARSE-0002 | `peek_next_ident()` (L7703), `expect()` (L8083) — identifier and token expectations |
| `ParseUnexpectedToken` | KAIN-PARSE-0003 | **NOT USED** in parser.rs (defined but never referenced) |
| `ParseReservedIdentifier` | KAIN-PARSE-0004 | `validate_identifier()` (L477), `peek_next_ident()` (L7695) |
| `ParseMissingDelimiterBeforeNewline` | KAIN-PARSE-0005 | `expect()` (L8053) — when `:` is expected but newline/dedent found |
| `ParseInvalidWorldSurfaceKind` | KAIN-PARSE-0006 | `parse_world_surface_projection()` (L2420) |
| `ParseExpectedContextualKeyword` | KAIN-PARSE-0007 | `expect_contextual_ident()` (L8105) |
| `ParseUnclosedDelimiter` | KAIN-PARSE-0008 | **NOT USED** in parser.rs (defined but never referenced) |
| `ParseMismatchedDelimiter` | KAIN-PARSE-0009 | **NOT USED** in parser.rs (defined but never referenced) |

**Key findings:**
- Only **4 of 9** defined codes are actually emitted from parser.rs (0001, 0002, 0004, 0005, 0006, 0007 — 6 total used, but 0002/0004 at 2 sites each, 0006/0007 at 1)
- **85% of all parse errors** (~188/195 call sites) fall through to `ParseGeneric` (KAIN-PARSE-0001)
- Codes 0010–0020 have **zero compiler backing** — they're spec-only fiction
- `ParseUnclosedDelimiter` and `ParseMismatchedDelimiter` constants exist but are never called from the parser

---

### Undocumented Error Catalog (by parsing function)

Each entry shows: **Function** → **Line** → **Message** → **Code actually emitted**

---

#### `parse_use_path_segment` (L795)
```
"Expected import path segment, got <token>"  → KAIN-PARSE-0001 (ParseGeneric)
```
Triggered when a `use` statement has a malformed path segment (not an identifier).

#### `parse_item` (L1033)
```
"Expected item (fn, patch, law, axiom, converge, world, entangle, orchestrate, pulse,
 resonate, shatter struct, struct, enum, actor, component, shader, material, trait,
 impl, mod, use, include, import, const, test), found <token>"  → KAIN-PARSE-0001
```
The top-level item dispatcher. Everything that isn't a known declaration keyword triggers this.

#### `parse_impl` (L1274, L1308)
```
L1274: "Expected trait path before 'for' in impl block, found <type>"  → KAIN-PARSE-0001
L1308: "Expected 'fn' in impl block (impl blocks can only contain function definitions),
        found <token>"  → KAIN-PARSE-0001
```
`impl MyType for Trait { ... }` syntax violations. First error fires when the type before `for` isn't a named path; second when an `impl` body contains something other than `fn`.

#### `parse_include` (L1392, L1402)
```
L1392: "Expected C include target such as `nuklear`, `native/nuklear`,
        `\"../native/nuklear.h\"`, or `<vulkan/vulkan.h>`"  → KAIN-PARSE-0001
L1402: "bare include form does not accept a version; use angle brackets:
        `include <foo.h> 1.0 as bar`"  → KAIN-PARSE-0001
```
C header import parsing. `include <windows.h> as win` and `include "foo.h" as bar` forms.

#### `parse_version_string` (L1533)
```
"Expected version string (e.g., '1.0' or '2.1.3')"  → KAIN-PARSE-0001
```
Triggered when `include` version suffix is malformed.

#### `parse_c_system_include_target` (L1623)
```
"Expected '<' to start system include path"  → KAIN-PARSE-0001
```
Angle-bracket include syntax (`<vulkan/vulkan.h>`) requires proper match.

#### `parse_macro` (L1735)
```
"Unknown macro param kind"  → KAIN-PARSE-0001
```
Macro parameters must be `expr`, `type`, `ident`, `block`, or `token`.

#### `parse_axiom` (L1970, L1978)
```
L1970: "axiom blocks may only declare one fallback"  → KAIN-PARSE-0001
L1978: "axiom blocks expect 'when', 'guarantee', or 'fallback'"  → KAIN-PARSE-0001
```
Axiom blocks accept predicates, guarantees, and exactly one fallback.

#### `parse_axiom_predicate` (L2008)
```
"Unknown axiom predicate '<name>'; expected target(...), arch(...), or
 capability(...)"  → KAIN-PARSE-0001
```

#### `parse_pulse_duration` (L2059)
```
"Expected pulse duration (e.g., 16ms, 1s, 500us)"  → KAIN-PARSE-0001
```

#### `parse_pulse_budget` (L2099, L2107, L2118)
```
L2099: "Expected alloc, lock, or io in pulse budget"  → KAIN-PARSE-0001
L2107: "pulse budget values must be non-negative integers"  → KAIN-PARSE-0001
L2118: "Unknown pulse budget category"  → KAIN-PARSE-0001
```

#### `parse_converge` (L2216, L2230, L2242, L2248)
```
L2216: "converge blocks may only declare one spec lane"  → KAIN-PARSE-0001
L2230: "Expected 'spec', 'fast', or 'verify' inside converge block"  → KAIN-PARSE-0001
L2242: "converge blocks require exactly one spec lane"  → KAIN-PARSE-0001
L2248: "converge blocks require at least one fast lane"  → KAIN-PARSE-0001
```
Converge dispatch blocks require exactly one `spec` lane and at least one `fast` lane.

#### `parse_converge_selector` (L2349)
```
"Unknown converge selector"  → KAIN-PARSE-0001
```

#### `parse_converge_verify_random_count` (L2366)
```
"verify random(N) requires a positive integer N"  → KAIN-PARSE-0001
```

#### `parse_world` (L2291)
```
"world block must contain at least one surface projection"  → KAIN-PARSE-0001
```
Note: This is a different error from KAIN-WORLD-0001 (missing surface at type level). This fires during *parsing* when a world body is empty.

#### `parse_entangle` (L2456)
```
"entangle requires both 'from' and 'to' target components"  → KAIN-PARSE-0001
```

#### `parse_entangle_endpoint` (L2480)
```
"Entangle endpoint must be a dotted path like World.component.field"  → KAIN-PARSE-0001
```

#### `parse_resonate_endpoint` (L2499)
```
"Resonate endpoint must be a dotted path like World.component.field"  → KAIN-PARSE-0001
```

#### `parse_string_like_argument` (L2534)
```
"Expected string argument"  → KAIN-PARSE-0001
```
Generic fallback for constructs that expect a string identifier argument.

---

#### `parse_component` (L2713, L2755, L2767, L2783)
```
L2713: "Expected 'state' keyword after 'weak' in component
        (use 'weak state name: Type = value'), found <token>"  → KAIN-PARSE-0001
L2755: "Unexpected identifier '<name>' in component. Valid keywords:
        'state', 'weak', 'pulse', 'resonate', 'render', or 'fn' for methods"  → KAIN-PARSE-0001
L2767: "Unexpected token in component: <token>. Expected 'state', 'weak',
        'pulse', 'resonate', 'render', 'fn', or JSX element"  → KAIN-PARSE-0001
L2783: "Component must have a render body (JSX element)"  → KAIN-PARSE-0001
```
Component parsing is one of the most error-dense areas. These fire for missing state after `weak`, unknown identifiers in the component body, unexpected tokens, and missing render body.

#### `parse_component_with_attrs` (L2880, L2903, L2918)
```
L2880: "Unexpected identifier in component: <name>"  → KAIN-PARSE-0001
L2903: "Unexpected token in component: <token>"  → KAIN-PARSE-0001
L2918: "Component must have a render body"  → KAIN-PARSE-0001
```
Similar to `parse_component` but for attributed components (`@with GPU component MyWidget:`).

---

#### `parse_shader` (L2959, L2962, L3007, L3055, L3073)
```
L2959: "Expected one of: vertex, fragment, compute, surface, mesh, task, raygen,
        anyhit, closesthit, miss, intersection, callable"  → KAIN-PARSE-0001
L2962: "Expected a shader stage"  → KAIN-PARSE-0001
L3007: "Expected integer binding after '@' (e.g., '@0', '@1', '@2'),
        found <token>"  → KAIN-PARSE-0001
L3055: "shader must contain at least one uniform binding or statement"  → KAIN-PARSE-0001
L3073: "Expected 'uniform', statement, or dedent in shader body"  → KAIN-PARSE-0001
```
Shader parsing — stage keywords, uniform `@N` bindings, and body content.

---

#### `parse_actor_with_attrs` (L3433, L3442)
```
L3433: "Unexpected identifier '<name>' in actor. Valid keywords:
        'state', 'var', 'fn', or 'on' for message handlers"  → KAIN-PARSE-0001
L3442: "Expected 'state', 'var', 'fn', or 'on' in actor definition,
        found <token>"  → KAIN-PARSE-0001
```
Actor body parsing — only `state`, `var`, `fn`, or `on` are valid.

---

#### `parse_material_graph` / `parse_material_function` / `parse_node_type` (L3525–L3925)
```
L3525: "Expected 'fn' in material graph"  → KAIN-PARSE-0001
L3535: "material graph must define at least one function"  → KAIN-PARSE-0001
L3602: "Expected 'inputs', 'outputs', or 'properties'"  → KAIN-PARSE-0001
L3624: "Unknown material function kind"  → KAIN-PARSE-0001
L3657: "material function must contain a body"  → KAIN-PARSE-0001
L3731: "Expected 'inputs', 'outputs', or 'properties'"  → KAIN-PARSE-0001
L3749: "material function must have at least one node type"  → KAIN-PARSE-0001
L3771: "Expected 'graph' keyword after @graph_editor attribute"  → KAIN-PARSE-0001
L3781: "Expected 'graph' keyword after @graph_editor attribute, found <token>"  → KAIN-PARSE-0001
L3818: "Expected @node_type or @schema attribute in graph editor body"  → KAIN-PARSE-0001
L3849: "Expected 'inputs', 'outputs', or 'properties'"  → KAIN-PARSE-0001
L3856: "node type must have a name"  → KAIN-PARSE-0001
L3915: "Expected 'inputs', 'outputs', or 'properties'"  → KAIN-PARSE-0001
```
UE5 material graph and graph editor parsing. Very domain-specific.

#### `parse_graph_schema` (L4053)
```
"Graph schema requires at least one pin"  → KAIN-PARSE-0001
```

#### `parse_function_header_clauses` (L4274)
```
"Expected function header clause (dimensions, workgroup, effect, where)"  → KAIN-PARSE-0001
```

#### `parse_optional_workgroup_clause` (L4300)
```
"workgroup sizes must be positive"  → KAIN-PARSE-0001
```

#### `parse_static_u32_dimension` (L4323, L4335)
```
L4323: "Expected integer for dimension value"  → KAIN-PARSE-0001
L4335: "<label> must be between 1 and 4294967295"  → KAIN-PARSE-0001
```
Component `width=... height=...` dimension parsing.

#### `parse_type` (L4420)
```
"Expected array size integer"  → KAIN-PARSE-0001
```
Array type syntax: `[T; N]` requires a literal integer for N.

---

#### `parse_orchestrate_stage` (L4657)
```
"Unknown orchestrate stage kind '<name>'; expected kain, c, cpu, gpu, dispatch,
 converge, law, patch, world, python, rust, or node"  → KAIN-PARSE-0001
```

#### `parse_orchestrate_graph_metadata` (L4721, L4733, L4763, L4775)
```
L4721: "Unknown orchestrate residency '<name>'; expected host, shared, or device"  → KAIN-PARSE-0001
L4733: "Unknown orchestrate transfer '<name>'; expected none, host_to_device,
       device_to_host, or shared_view"  → KAIN-PARSE-0001
L4763: "Unknown orchestrate policy '<name>'; expected static, telemetry_prefer_gpu,
       telemetry_prefer_cpu, or telemetry_balance_latency"  → KAIN-PARSE-0001
L4775: "Expected orchestrate graph metadata such as after, deps, residency,
       transfer, guarded by, fallback, requires, or policy"  → KAIN-PARSE-0001
```

#### `parse_orchestrate_selector` (L4800)
```
"Expected orchestrate selector condition"  → KAIN-PARSE-0001
```

---

#### `parse_defer` (L4868)
```
"defer expects an expression payload in v1"  → KAIN-PARSE-0001
```
`defer` must be followed by an expression or block.

#### `parse_dispatch` (L4886)
```
"Expected dispatch call with named arguments"  → KAIN-PARSE-0001
```

#### `parse_subgroup_block` (L4931)
```
"subgroup blocks must contain at least one statement"  → KAIN-PARSE-0001
```

#### `parse_u32_literal` (L4974, L4978)
```
L4974: "Expected positive integer literal"  → KAIN-PARSE-0001
L4978: "Expected integer literal, got <token>"  → KAIN-PARSE-0001
```
Binding indices, workgroup sizes, and other u32-constrained positions.

---

#### `parse_unary` (L5311, L5335, L5345, L5351)
```
L5311: "Send requires named arguments"  → KAIN-PARSE-0001
L5335: "Send requires named arguments"  → KAIN-PARSE-0001
L5345: "Expected actor or component after send"  → KAIN-PARSE-0001
L5351: "Expected message call after send"  → KAIN-PARSE-0001
```
Actor message sending: `send actor.message(args)`.

#### `parse_scoped_ownership_expr` (L5407)
```
"Unknown scoped ownership keyword"  → KAIN-PARSE-0001
```
Collapse/observe/decay scoped expressions.

#### `parse_postfix` (L5471, L5634)
```
L5471: "Expected field name, method name, or indexing expression"  → KAIN-PARSE-0001
L5634: "Expected '[' or '.' for field/index access"  → KAIN-PARSE-0001
```
Dot-access and index-access postfix parsing.

---

#### `parse_primary` (L5710, L5796, L5970, L6102)
```
L5710: "Unclosed '{' in f-string"  → KAIN-PARSE-0001
L5796: "Struct literal syntax is not supported in KAIN. Found '<name> { ... }'.
        Use field-by-field assignment instead"  → KAIN-PARSE-0001
L5970: "Spawn requires named arguments"  → KAIN-PARSE-0001
L6102: "Unexpected token: <token>"  → KAIN-PARSE-0001
```
Primary expression parsing — f-strings, struct literals (rejected with the helpful field-by-field suggestion), spawn calls, and the catch-all "Unexpected token".

#### `parse_struct_literal_expr` (L6138, L6147)
```
L6138: "Struct update syntax only supports one '..base' expression"  → KAIN-PARSE-0001
L6147: "Struct update syntax requires an expression after '..'"  → KAIN-PARSE-0001
```
Rust-like struct update syntax (`Struct { field: val, ..base }`).

---

#### `parse_if_tail` (L7020)
```
"Expected if expression after else"  → KAIN-PARSE-0001
```
`else if` requires an `if` expression, not just `else { ... }`.

#### `parse_pattern` (L7151)
```
"Expected pattern for match arm"  → KAIN-PARSE-0001
```

---

#### `parse_jsx_element` / `parse_jsx_tag_name` / `parse_jsx_attribute_name` (L7209–L7470)
```
L7209: "Expected attribute value"  → KAIN-PARSE-0001
L7372: "JSX element name must start with an uppercase letter"  → KAIN-PARSE-0001
L7408: "Expected closing tag for <name>"  → KAIN-PARSE-0001
L7433: "Expected JSX tag name"  → KAIN-PARSE-0001
L7470: "Expected '=' in JSX attribute"  → KAIN-PARSE-0001
```
JSX parsing rules: uppercase component names, attribute `=` syntax, closing tag matching.

#### `make_incdec_expr` (L7768)
```
"++ and -- are not valid operators in Kain. Use x += 1 or x -= 1"  → KAIN-PARSE-0001
```
Kain rejects C-style increment/decrement in favor of compound assignment.

---

#### `expect` → KAIN-PARSE-0005 / KAIN-PARSE-0002
```
L8052–8093: Two specific branches:
  - When ':' is expected but newline/dedent found:
    "Missing ':' before line break"  → KAIN-PARSE-0005
    + fixit: insert ':' at header end
  - For all other expected tokens:
    "Expected <expected>, got <actual>"  → KAIN-PARSE-0002
```
This is the most frequently called error function. It fires for every `self.expect(TokenKind::Colon)?` and similar calls throughout the parser.

#### `expect_contextual_ident` → KAIN-PARSE-0007
```
"Expected contextual keyword '<expected>', got <actual>"  → KAIN-PARSE-0007
```

#### `validate_identifier` → KAIN-PARSE-0004
```
"Identifier '<name>' conflicts with a reserved keyword"  → KAIN-PARSE-0004
+ note: "Reserved identifiers include Kain keywords, shader keywords, C++ keywords,
         and common engine macros."
+ help: "Rename '<name>' to something descriptive like '<name>_value' or '<name>_slot'."
```

#### `peek_next_ident` → KAIN-PARSE-0004 / KAIN-PARSE-0002
```
L7695: "<keyword> is a reserved keyword and cannot be used as an identifier."
       → KAIN-PARSE-0004
L7703: "Expected identifier, got <token>"  → KAIN-PARSE-0002
L8860: "Expected identifier"  → KAIN-PARSE-0001
L8863: "Unexpected end of input"  → KAIN-PARSE-0001
```

---

#### `parse_graph_runtime` (L8158)
```
"Expected struct keyword after @graph_runtime"  → KAIN-PARSE-0001
```

#### `parse_node_data` (L8336)
```
"Expected @input_pin, @output_pin, @property, or 'fn' in node data body,
 found <token>. Node data defines the structure and behavior of graph nodes"  → KAIN-PARSE-0001
```

#### `parse_graph_instance` (L8460)
```
"Expected fn, delegate, or field in instance"  → KAIN-PARSE-0001
```

#### `parse_delegate_def` (L8489)
```
"Expected 'delegate' keyword"  → KAIN-PARSE-0001
```

#### `parse_state_machine` (L8621)
```
"Expected 'state' keyword in state machine"  → KAIN-PARSE-0001
```

#### `parse_state` (L8726, L8741)
```
L8726: "Unexpected method in state definition. Use @transition for transitions."  → KAIN-PARSE-0001
L8741: "Expected string literal for animation property"  → KAIN-PARSE-0001
```

#### `parse_transition` (L8819)
```
"Expected 'to' in state transition"  → KAIN-PARSE-0001
```

---

#### `parse_editor_module` (L8935)
```
"Expected @menu_entry, @toolbar_button, @toolbar_widget, or fn in editor module"  → KAIN-PARSE-0001
```
UE5 editor module parsing.

#### `parse_menu_entry` (L8968, L8996, L8999, L9019)
```
L8968: "Expected @menu_entry attribute"  → KAIN-PARSE-0001
L8996: "@menu_entry requires 'path' parameter"  → KAIN-PARSE-0001
L8999: "@menu_entry requires 'label' parameter"  → KAIN-PARSE-0001
L9019: "Expected function after @menu_entry"  → KAIN-PARSE-0001
```

#### `parse_toolbar_button` (L9032, L9060, L9066, L9086)
```
L9032: "Expected @toolbar_button attribute"  → KAIN-PARSE-0001
L9060: "@toolbar_button requires 'section' parameter"  → KAIN-PARSE-0001
L9066: "@toolbar_button requires 'icon' parameter"  → KAIN-PARSE-0001
L9086: "Expected function after @toolbar_button"  → KAIN-PARSE-0001
```

#### `parse_toolbar_widget` (L9102, L9144, L9151)
```
L9102: "Expected @toolbar_widget attribute"  → KAIN-PARSE-0001
L9144: "@toolbar_widget requires 'section' parameter"  → KAIN-PARSE-0001
L9151: "@toolbar_widget requires 'type' parameter"  → KAIN-PARSE-0001
```

#### `parse_gameplay_tags` (L9186, L9193)
```
L9186: "Expected @gameplay_tags attribute"  → KAIN-PARSE-0001
L9193: "Expected 'tags' field after @gameplay_tags"  → KAIN-PARSE-0001
```

#### `parse_tag_name` (L9332)
```
"Expected gameplay tag name, got <token>"  → KAIN-PARSE-0001
```

---

#### `parse_gameplay_ability` (L9426–L9714) — 27 error call sites

All mapped to **KAIN-PARSE-0001**. This is the most error-dense function in the entire parser.

```
L9426: "Expected string value for instancing policy"
L9432: "Expected 'policy' parameter in @instancing"
L9438: "Expected 'policy' parameter in @instancing"
L9459: "Expected string value for replication policy"
L9465: "Expected 'policy' parameter in @replication"
L9471: "Expected 'policy' parameter in @replication"
L9492: "Expected string value for net_execution policy"
L9498: "Expected 'policy' parameter in @net_execution"
L9504: "Expected 'policy' parameter in @net_execution"
L9523: "Expected 'tags' field after @ability_tags"
L9529: "Expected 'tags' field after @ability_tags"
L9544: "Expected 'required' field after @activation_required_tags"
L9550: "Expected 'required' field after @activation_required_tags"
L9565: "Expected 'blocked' field after @activation_blocked_tags"
L9571: "Expected 'blocked' field after @activation_blocked_tags"
L9586: "Expected 'required' field after @source_required_tags"
L9592: "Expected 'required' field after @source_required_tags"
L9607: "Expected 'blocked' field after @source_blocked_tags"
L9613: "Expected 'blocked' field after @source_blocked_tags"
L9628: "Expected 'required' field after @target_required_tags"
L9634: "Expected 'required' field after @target_required_tags"
L9653: "Expected 'blocked' field after @target_blocked_tags"
L9659: "Expected 'blocked' field after @target_blocked_tags"
L9678: "Expected 'tags' field after @cancel_ability_tags"
L9684: "Expected 'tags' field after @cancel_ability_tags"
L9702: "Expected 'tags' field after @activation_owned_tags"
L9714: "Expected 'tags' field after @activation_owned_tags"
```
These are UE5 Gameplay Ability System (GAS) attribute parsing errors — instancing, replication, net execution, and gameplay tag arrays.

#### `parse_string_array` (L9757)
```
"Expected string in array"  → KAIN-PARSE-0001
```

#### `parse_gameplay_effect` (L9862–L10116) — 15 error call sites

All mapped to **KAIN-PARSE-0001**. The second most error-dense function.

```
L9862: "Expected string value for duration type"
L9868: "Expected 'type' parameter in @duration"
L9891: "Expected numeric value for duration"
L9918: "Expected numeric value for period"
L9941: "Expected boolean value for execute_on_application"
L9967: "Expected string value for attribute"
L9987: "Expected string value for operation"
L10018: "Expected numeric value after minus"
L10024: "Expected numeric value for magnitude"
L10055: "Expected string value for stacking type"
L10072: "Expected numeric value for stacking limit"
L10089: "Expected numeric value for stack count"
L10095: "Expected numeric value for stack count"
L10110: "Expected numeric value for level"
L10116: "Expected numeric value for level"
```
UE5 GameplayEffect attribute parsing — duration policies, period, modifiers (attribute/operation/magnitude), stacking, and level overrides.

#### `parse_gameplay_cue` (L10312–L10455) — 7 error call sites

All mapped to **KAIN-PARSE-0001**.

```
L10312: "Expected field name"
L10325: "Expected string for tag"
L10336: "Invalid cue type '<type>'. Valid: Static, Actor"
L10348: "Expected string for type"
L10362: "Expected true or false"
L10441: "Expected fn, on, or state field"
L10455: "Gameplay cue must have 'tag' field"
```

#### `parse_ability_task` (L10659)
```
"Unexpected token in ability task: <token>"  → KAIN-PARSE-0001
```

#### `parse_target_actor` (L10744–L10995) — 8 error call sites

All mapped to **KAIN-PARSE-0001**.

```
L10744: "Invalid trace type '<type>'. Valid: Line, Sphere, Cone, Box, Cylinder"
L10751: "Expected string for trace_type"
L10766: "Expected number for max_range"
L10778: "Expected string for trace_channel"
L10790: "Expected string for reticle_class"
L10839: (filter subfield error)
L10925: "Unknown target actor field: <name>"
L10995: "Expected field name or fn"
```
UE5 TargetActor definitions — trace types, ranges, channels, reticles, and filters.

---

### Error Density by Subsystem

| Subsystem | Error Count | % of Total |
|-----------|:-----------:|:----------:|
| **UE5 Gameplay Ability System** (ability + effect + cue + tags + target_actor) | 60 | 30.8% |
| **UE5 Editor** (editor_module + menu_entry + toolbar_button + toolbar_widget) | 12 | 6.2% |
| **UE5 Graph** (graph_editor + graph_schema + node_type + node_data + graph_instance + graph_runtime) | 9 | 4.6% |
| **Component/JSX** (component + component_with_attrs + jsx_element + tag_name + attribute) | 10 | 5.1% |
| **Shader** (shader + workgroup) | 6 | 3.1% |
| **Orchestrate** (stage + metadata + selector) | 6 | 3.1% |
| **Actor** (actor + send/spawn) | 5 | 2.6% |
| **Axiom/Pulse/Converge** (axiom + axiom_predicate + pulse_duration + pulse_budget + converge + converge_selector + verify_random) | 12 | 6.2% |
| **Token-level** (expect + expect_contextual_ident + validate_identifier + peek_next_ident) | 8 | 4.1% |
| **Expression** (primary + unary + postfix + if_tail + pattern + struct_literal + incdec) | 13 | 6.7% |
| **Material** (material_graph + material_function) | 7 | 3.6% |
| **Other** (include, impl, world, entangle, resonate, state, macro, defer, dispatch, subgroup, use_path, etc.) | ~47 | 24.1% |

---

### What This Means for LLMs

When an LLM sees a Kain parse error in practice:

1. **~85% chance** it will be `KAIN-PARSE-0001` (ParseGeneric) with a custom message
2. The custom message is the **only** clue to what went wrong — the code number is unhelpful
3. Errors 0010–0020 (numeric literal, string literal, char literal, attribute, effect, module, use/import, visibility, comptime, macro, test) **cannot be emitted** by the compiler since they lack `DiagnosticCode` constants
4. Even errors 0003, 0008, 0009 have defined constants but are **never called** from the parser
5. The error message catalog above (~195 sites) is the **actual** surface an LLM should use for matching

### Spec vs. Reality

| What | Spec (KAIN_ERROR_SPECS.md) | Compiler (parser.rs) |
|------|---------------------------|---------------------|
| Total documented codes | 20 | 9 have constants |
| Codes actually emitted | — | 6 (0001, 0002, 0004, 0005, 0006, 0007) |
| Error call sites | — | 195 |
| Sites using specific codes | — | 7 (3.6%) |
| Sites using ParseGeneric | — | 188 (96.4%) |
| UE5-specific errors documented | 0 | ~81 (41.5% of all call sites) |
| JSX-specific errors documented | 0 | 5 |
| Component-specific errors documented | 0 | 7 |
| Material/graph editor errors documented | 0 | 16 |

**Conclusion:** The spec lags significantly behind the compiler. The compiler has a rich set of specific error messages but funnels almost all of them through `ParseGeneric`. This document bridges that gap for LLM training and tooling.
