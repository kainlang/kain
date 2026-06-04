# Kain Keyword Catalog

Snapshot: 2026-06-03

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

- `58` hard lexer keywords
- `41` contextual or keyword-like parser words
- `2` textual operator aliases: `and`, `or`
- `101` practical authored Kain words to remember if you want the whole live surface

## Source Of Truth

- `crates/core/src/lexer.rs`
- `crates/core/src/parser.rs`
- `docs/syntax-and-semantics/syntax.md`

## 1. Hard Lexer Keywords (`58`)

These have dedicated token entries in `TokenKind`.

### Core Control And Binding

`fn`, `let`, `mut`, `var`, `const`, `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `defer`, `await`, `in`, `with`, `as`

### Types, Modules, Visibility, Self

`type`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, `self`, `Self`

### Built-In Literals

`true`, `false`, `none`

### Semantic And Runtime Surface

`component`, `shader`, `actor`, `state`, `spawn`, `send`, `receive`, `emit`, `comptime`, `macro`, `vertex`, `fragment`, `collapse`, `observe`, `decay`, `share`, `fanout`, `test`

### Effect Words

`Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`, `Unsafe`

## 2. Contextual Or Keyword-Like Parser Words (`49`)

These are not all tokenized as dedicated keywords, but the parser treats them like language words in specific positions.

### Compiler-Owned Declarations

`patch`, `law`, `axiom`, `pulse`, `orchestrate`, `converge`, `world`, `entangle`, `shatter`, `teleport`

### Import, Clause, And Selector Words

`include`, `import`, `from`, `where`, `stage`, `after`, `deps`, `residency`, `transfer`, `guarded`, `by`, `requires`, `policy`, `every`, `when`, `guarantee`, `fallback`, `spec`, `fast`, `verify`, `random`, `jitter`, `target`, `capability`, `to`, `via`

`stage` is the contextual statement form inside orchestrated pipelines:
`stage result: gpu kernel(value) when capability("gpu.compute") residency device transfer host_to_device guarded by gpu_truth fallback degrade cpu_seed policy telemetry_prefer_gpu`.
It lowers through the normal stage-call expression shape while preserving typed
graph metadata for dependency validation, runtime contracts, realtime app
bundles, LLVM telemetry, and native `std::intent` counters. Graph clauses include
`after`, `deps [...]`, `residency host|shared|device`, `transfer none|host_to_device|device_to_host|shared_view`,
`guarded by <axiom>`, `fallback abort|<stage>|degrade <stage>`, `requires <law-stage>`,
and `policy static|telemetry_prefer_gpu|telemetry_prefer_cpu|telemetry_balance_latency`.

`include` now covers both local C header imports such as
`include native/foo.h as foo` and the registry-backed system-header lane such
as `include <stdio.h> as cstdio`, `include <math.h> as cmath`,
`include <sys/mman.h> as posix_mman`, or `include <vulkan/vulkan.h> as vk`.
The angle-bracket form resolves known C runtime, C runtime math, POSIX,
Windows SDK, and Vulkan SDK families through deterministic include roots plus
compiler-owned link policy declared in `crates/c-ffi/system_headers.toml`.

First-class Python `import ...` names bind live Python host objects. Normal
member/call syntax applies, and named Kain call args on those host objects lower
to Python kwargs, so authored code can write
`py_json.dumps(value, separators = [",", ":"])` without dropping into
`python_call_attr_kwargs`.

### Surface And Projection Words

`surface`, `native_ui`, `viewport3d`, `web`, `ue5`

### Shader, Component, Actor, And Ownership Context Words

`compute`, `uniform`, `workgroup`, `dispatch`, `render`, `on`, `weak`, `single_writer`

## 3. Textual Operator Aliases (`2`)

These are word forms for operators, not normal declaration keywords, but they still matter when remembering the textual surface.

- `and`
- `or`

## 4. Flat Master List

If you just want one big memory dump, this is the current authored Kain word surface counted above.

`fn`, `let`, `mut`, `var`, `const`, `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `defer`, `await`, `in`, `with`, `as`, `type`, `struct`, `enum`, `trait`, `impl`, `pub`, `mod`, `use`, `self`, `Self`, `true`, `false`, `none`, `component`, `shader`, `actor`, `state`, `spawn`, `send`, `receive`, `emit`, `comptime`, `macro`, `vertex`, `fragment`, `collapse`, `observe`, `decay`, `share`, `fanout`, `test`, `Pure`, `IO`, `async`, `Async`, `GPU`, `Reactive`, `Unsafe`, `patch`, `law`, `axiom`, `pulse`, `orchestrate`, `converge`, `world`, `entangle`, `shatter`, `teleport`, `include`, `import`, `from`, `where`, `stage`, `after`, `deps`, `residency`, `transfer`, `guarded`, `by`, `requires`, `policy`, `every`, `when`, `guarantee`, `fallback`, `spec`, `fast`, `verify`, `random`, `jitter`, `target`, `capability`, `to`, `via`, `surface`, `native_ui`, `viewport3d`, `web`, `ue5`, `compute`, `uniform`, `workgroup`, `dispatch`, `render`, `on`, `weak`, `single_writer`, `and`, `or`

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

Kain also owns a large symbolic surface that is not included in the `101`:

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
- The biggest "easy to forget" family is the contextual one: `patch`, `law`, `world`, `entangle`, `pulse`, `teleport`, `include`, `import`, `where`, `surface`, `compute`, `workgroup`, `dispatch`, `uniform`, `single_writer`, and friends.
