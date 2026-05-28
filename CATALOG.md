# Kain Keyword Catalog

Snapshot: 2026-05-28

This file is the quick "what words does Kain actually own right now?" sheet.
It is meant to stop the constant rediscovery loop.

## Counting Rule

This catalog counts authored Kain language words, not punctuation.

- Counted:
  - hard lexer keywords with dedicated `TokenKind` entries
  - contextual parser words that behave like keywords in specific syntax positions
  - textual operator aliases written as words
- Not counted:
  - symbolic operators such as `+`, `??`, `->`, `=>`
  - attribute tags such as `@material_graph`
  - helper/builtin names such as `ptr`, `ptr_mut`, `delegate`
  - the large HLSL/C++/UE5 compatibility blacklist in `RESERVED_KEYWORDS`

## Headline Count

- `57` hard lexer keywords
- `36` contextual or keyword-like parser words
- `2` textual operator aliases: `and`, `or`
- `95` practical authored Kain words to remember if you want the whole live surface

## Source Of Truth

- `crates/core/src/lexer.rs`
- `crates/core/src/parser.rs`
- `docs/syntax-and-semantics/syntax.md`

## 1. Hard Lexer Keywords (`57`)

These have dedicated token entries in `TokenKind`.

### Core Control And Binding

`fn`, `let`, `mut`, `var`, `const`, `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `await`, `in`, `with`, `as`

### Types, Modules, Visibility, Self

`type`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, `self`, `Self`

### Built-In Literals

`true`, `false`, `none`

### Semantic And Runtime Surface

`component`, `shader`, `actor`, `state`, `spawn`, `send`, `receive`, `emit`, `comptime`, `macro`, `vertex`, `fragment`, `collapse`, `observe`, `decay`, `share`, `fanout`, `test`

### Effect Words

`Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`, `Unsafe`

## 2. Contextual Or Keyword-Like Parser Words (`35`)

These are not all tokenized as dedicated keywords, but the parser treats them like language words in specific positions.

### Compiler-Owned Declarations

`patch`, `law`, `axiom`, `pulse`, `orchestrate`, `converge`, `world`, `entangle`, `shatter`, `teleport`

### Import, Clause, And Selector Words

`include`, `every`, `when`, `guarantee`, `fallback`, `spec`, `fast`, `verify`, `random`, `jitter`, `target`, `capability`, `from`, `to`, `via`

### Surface And Projection Words

`surface`, `native_ui`, `viewport3d`, `web`, `ue5`

### Shader, Component, Actor, And Ownership Context Words

`compute`, `uniform`, `render`, `on`, `weak`, `single_writer`

## 3. Textual Operator Aliases (`2`)

These are word forms for operators, not normal declaration keywords, but they still matter when remembering the textual surface.

- `and`
- `or`

## 4. Flat Master List

If you just want one big memory dump, this is the current authored Kain word surface counted above.

`fn`, `let`, `mut`, `var`, `const`, `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `await`, `in`, `with`, `as`, `type`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, `self`, `Self`, `true`, `false`, `none`, `component`, `shader`, `actor`, `state`, `spawn`, `send`, `receive`, `emit`, `comptime`, `macro`, `vertex`, `fragment`, `collapse`, `observe`, `decay`, `share`, `fanout`, `test`, `Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`, `Unsafe`, `patch`, `law`, `axiom`, `pulse`, `orchestrate`, `converge`, `world`, `entangle`, `shatter`, `teleport`, `include`, `every`, `when`, `guarantee`, `fallback`, `spec`, `fast`, `verify`, `random`, `jitter`, `target`, `capability`, `from`, `to`, `via`, `surface`, `native_ui`, `viewport3d`, `web`, `ue5`, `compute`, `uniform`, `render`, `on`, `weak`, `single_writer`, `and`, `or`

## 5. What This Catalog Deliberately Excludes

### Builtin And Stdlib Function Surfaces

CUDA device intrinsics are not counted as keywords because they are imported
function names under `std::cuda`, but they are compiler-owned during PTX
lowering. The current authored CUDA surface includes:

`cuda_lane_id`, `cuda_warp_id`, `cuda_active_mask`, `cuda_block_sync`,
`cuda_barrier_sync`, `cuda_warp_sync`, `cuda_ballot`, `cuda_warp_any`,
`cuda_warp_all`, `cuda_shfl_xor_u32`, `cuda_shfl_xor_f32`,
`cuda_warp_reduce_sum_u32`, `cuda_warp_reduce_sum_f32`,
`cuda_cp_async_commit_group`, `cuda_cp_async_wait_group_0`,
`cuda_require_tensor_cores`, `cuda_require_wgmma`

### Symbol-Only Surface

Kain also owns a large symbolic surface that is not included in the `94`:

`+`, `-`, `*`, `/`, `%`, `**`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`, `::`, `->`, `=>`, `@`, `??`, `?.`, `?`

### External Reserved Blacklist

`parser.rs` also reserves many foreign words so authored Kain does not collide with host/runtime/shader ecosystems.

Examples:

- HLSL-ish words such as `line`, `compile`, `pass`, `cbuffer`, `groupshared`, `packoffset`
- C++ words such as `class`, `virtual`, `template`, `switch`, `case`, `try`, `catch`
- UE-style macros such as `UCLASS`, `USTRUCT`, `UFUNCTION`, `UPROPERTY`

Those are real reserved identifiers in the parser, but they are not part of the headline Kain-authored keyword count above.

## 6. Practical Notes

- `lexer.rs` is the truth for hard keywords.
- `parser.rs` is where the contextual language words show up.
- The biggest "easy to forget" family is the contextual one: `patch`, `law`, `world`, `entangle`, `pulse`, `teleport`, `include`, `surface`, `compute`, `uniform`, `single_writer`, and friends.
