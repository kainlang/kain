# Imports, Modules, and Stdlib

## The Most Important Syntax Clarification

Do not write `use::std`.

The current parser expects:

- `use std::foo`
- `use std/foo`
- `use foo::bar as baz`
- `use foo::*`

The `use` keyword comes first, then the path. The path segments are separated by `::` or `/`.

## Canonical Import Forms

Both of these parse today:

```kain
use compiler::lexer
use compiler::parser
```

```kain
use compiler/lexer
use compiler/parser
```

Alias and glob forms also parse:

```kain
use std::io as io
use compiler::*
```

## Recommended Style for New Code

Use `::` in new hand-written code unless you are intentionally matching an older file's style.

Why:

- it is easier to scan
- it makes alias and glob forms clearer
- it aligns with how most people expect namespaced imports to read

Know that slash-style imports are still common in older bootstrap files under `M:\Code\Kain\bootstrap`.

## What the Parser Actually Supports

`M:\Code\Kain\crates\kain-core\src\parser.rs` accepts:

- a leading identifier after `use`
- repeated separators of `::` or `/`
- glob imports via trailing `*`
- aliasing via `as`

This means these are all intentional, not accidents:

- `use foo::bar::baz`
- `use foo/bar/baz`
- `use foo::*`
- `use foo::bar as baz`

## Module Resolution Model

At runtime, `Use.path` segments are joined with `/` before lookup. That means both separator styles converge to the same filesystem-oriented resolution model.

For non-stdlib modules, the runtime checks paths in this order:

1. `./<path>.kn`
2. `src/<path>.kn`
3. `src/core/<first-segment>.kn`
4. `<path>.kn`
5. legacy `<path>.god`
6. fallback first-segment module files: `<first-segment>.kn`, `src/<first-segment>.kn`, then legacy `<first-segment>.god`

Examples:

- `use compiler::lexer`
  - may resolve to `compiler/lexer.kn`
  - or `src/compiler/lexer.kn`
- `use foo/bar`
  - may resolve to `foo/bar.kn`
- `use host_reflection::build_control_plane_catalog`
  - first tries `host_reflection/build_control_plane_catalog.kn`
  - then can fall back to `host_reflection.kn` or `src/host_reflection.kn` and register only `build_control_plane_catalog`
- `use plugin_authoring::*`
  - can load `plugin_authoring.kn` or `src/plugin_authoring.kn` and expose the module's top-level items to both runtime and best-effort typechecking

Legacy `.god` support still exists for compatibility, but new files should be `.kn`.

The shared resolver lives in `crates/kain-core/src/module_resolution.rs`. The typechecker mirrors filesystem imports when a module file parses cleanly, and falls back to the older `Unknown` imported-symbol behavior when it cannot safely register the module during typechecking.

## Stdlib Resolution

Stdlib imports are special-cased.

These map to stdlib module lookup:

- `use std::option`
- `use std/option`
- `use stdlib::option`
- `use stdlib/option`

The runtime strips the `std/` or `stdlib/` prefix and looks for `<module>.kn` inside stdlib roots.

If the path is exactly `stdlib`, it is treated as already loaded.

The stdlib resolver also supports a small flat-root alias table for root-preferred authored modules that still want nested import spelling. For example, `use std::graphics::shared` resolves to the root file `stdlib/graphics_shared.kn`.

## Canonical Native Stdlib Domains

For new native LLVM/direct-C Kain code, prefer the public root domain imports:

```kain
use std::actor
use std::fs
use std::http
use std::ui
```

Do not write `use std::native::foo` for ordinary authored code. The root `stdlib/` folder is now the canonical backend and authored surface for LLVM/direct-C too, and `std::native::*` is only a compatibility alias for older files.

Current root-domain mirrors include:

- `std::actor`
- `std::collections`
- `std::diagnostics`
- `std::fs`
- `std::graphics`
- `std::http`
- `std::http2`
- `std::input`
- `std::intent`
- `std::net`
- `std::process`
- `std::result`
- `std::runtime`
- `std::time`
- `std::tls`
- `std::ui`

Prefer clean domain names where the module exposes them, for example `actor_spawn`, `runtime_init`, `result_ok`, `now_millis`, `fs_temp_file`, `graphics_session_create`, and `ui_session_create`. Use `native_*` names only when deliberately testing ABI-compatibility wrappers.

## Stdlib Search Roots

The current stdlib loader in `M:\Code\Kain\crates\kain-core\src\stdlib.rs` uses this priority:

1. `KAIN_STDLIB_PATH`
2. walk upward from the executable location looking for `stdlib`
3. walk upward from the current working directory looking for `stdlib`

Practical guidance:

- if you are in `M:\Code\Kain`, the repo `stdlib\` directory is usually found automatically
- if you are running from another workspace or a copied binary, `KAIN_STDLIB_PATH` is the clean override
- do not hardcode stdlib paths in code when an env var or repo-relative lookup should be used

## Stdlib Profiles

Kain supports target-specific stdlib profile loading.

Important facts:

- most targets use the root stdlib profile
- LLVM uses the root profile directly, and direct C loads root first and then `c`
- UE5-like targets prefer `ue5` and then fall back to root
- `ks` shares the JS/root stdlib profile
- `KAIN_STDLIB_PROFILE` can override the profile order with a comma-separated list

Examples:

```powershell
$env:KAIN_STDLIB_PATH = "M:\Code\Kain\stdlib"
$env:KAIN_STDLIB_PROFILE = "ue5,root"
```

Use this only when you need to force a specific stdlib layout.

## `mod` and Module Boundaries

Kain also has real module items:

```kain
mod math:
    fn add(a: Int, b: Int) -> Int with Pure:
        a + b
```

This matters because importers use module items heavily:

- directory imports default to wrapping each source file in `mod <name>:`
- `--flat` disables that wrapping and merges everything into the top level

If you want to preserve original source-file boundaries during import, keep the default modular mode.

## Practical Advice for Agents

- Prefer `use std::foo` over `use std/foo` in new code, but recognize both.
- If import resolution is failing, check current working directory, `src\`, and `KAIN_STDLIB_PATH` before rewriting code.
- When changing import semantics, update both parser and runtime expectations. Syntax support without resolution support is a trap.
- Do not paste stdlib code into source files. `compile()` prepends stdlib automatically.
- If a file tree depends on stable per-file grouping, do not reach for `--flat` unless you really want a single merged scope.

## Fast Diagnostics

When imports are failing:

1. Confirm the exact written syntax is `use std::foo`, not `use::std`.
2. Check whether the target path should be stdlib-backed or filesystem-backed.
3. Check for `.kn` versus legacy `.god`.
4. Check whether the file exists under the current directory or under `src\`.
5. Check `KAIN_STDLIB_PATH` if the failure is stdlib-related.
