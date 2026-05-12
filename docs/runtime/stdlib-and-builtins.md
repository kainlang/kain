# Stdlib And Builtins

Kain has two related builtin layers:

1. the source-loaded stdlib registry in `crates/kain-core/src/stdlib.rs`
2. the runtime-native function registry in `crates/kain-core/src/runtime.rs`

They overlap, but they are not the same thing.

## Source Map

| Family | Source file | What it means |
| --- | --- | --- |
| Source-loaded stdlib registry | `crates/kain-core/src/stdlib.rs` | Helpers loaded from stdlib roots and profiles before execution |
| Runtime-native function registry | `crates/kain-core/src/runtime.rs` | Helpers registered directly on the interpreter/runtime surface |
| Loader and profile selection | `crates/kain-core/src/stdlib.rs`, `crates/kain-core/src/runtime.rs` | Search paths and profile resolution for the stdlib layer |

## Stdlib Loader

The stdlib loader searches in this order:

1. `KAIN_STDLIB_PATH`
2. a sibling `stdlib/` beside the compiler binary
3. a workspace `stdlib/`

It also respects `KAIN_STDLIB_PROFILE`, and otherwise uses target profiles
defined in `crates/kain-core/src/stdlib.rs`.

For `CompileTarget::Llvm` and `CompileTarget::C`, the practical surface is the
native profile in `stdlib/native/*`, not only the generic root registry.
Families such as `input_*`, `native_ui_*`, `native_graphics_*`, and now
`process_*` are source-loaded from that native profile and backed by
`runtime/native` ABI symbols.

## Source-Loaded Stdlib Functions

The base stdlib registry includes functions in these groups:

- I/O and filesystem: `print`, `println`, `stdout_write`, `read_line`,
  `stdin_read_exact`, `read_file`, `write_file`, `file_exists`, `env`
- Math: `abs`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`,
  `min`, `max`, `clamp`
- Vectors: `vec2`, `vec3`, `vec4`, `dot`, `cross`, `normalize`, `length`,
  `distance`, `mix`, `smoothstep`
- Collections: `len`, `push`, `pop`, `map`, `filter`, `reduce`, `range`
- Hash maps: `map_new`, `map_set`, `map_get`
- Strings: `split`, `join`, `trim`, `to_upper`, `to_lower`, `contains`,
  `replace`, `starts_with`, `ends_with`, `substring`, `char_at`, `ord`, `chr`
- Conversion: `to_string`, `to_int`, `to_float`
- Debug: `dbg`, `assert`, `panic`
- Time: `now`, `sleep`
- Actors and UI: `spawn`, `send`, `mount`
- Network: `socket_connect`, `socket_send`, `socket_recv`
- Native bridge helpers: `spawn_cube`, `spawn_native_viewport`,
  `spawn_native_sculpt_lab`, `native_config_string`, `native_config_int`,
  `native_config_float`, `native_config_flag`

The loader can also accept extension registrars, so the concrete stdlib surface
can grow without rewriting the loader.

## Runtime-Native Builtins

The interpreter-native registry in `runtime.rs` exposes the effective runtime
surface used during execution. Grouped by behavior:

- core values: `None`, `none`, `Some`, `bool`, `int`, `float`, `str`,
  `to_string`, `to_int`
- printing and diagnostics: `print`, `println`, `eprint`, `eprintln`, `dbg`,
  `assert`, `panic`
- math: `min`, `max`, `abs`, `sqrt`, `random`, `sin`, `cos`, `tan`
- collections: `len`, `first`, `last`, `push`, `range`, `reverse`, `sum`,
  `map`, `filter`, `reduce`, `foreach`
- strings and text: `split`, `join`, `trim`, `to_upper`, `upper`, `to_lower`,
  `lower`, `contains`, `starts_with`, `ends_with`, `replace`, `char_at`,
  `substring`
- type and variant helpers: `type_of`, `variant_of`, `variant_field`
- I/O and platform: `read_file`, `write_file`, `read_line`, `stdout_write`,
  `stdin_read_exact`, `file_exists`, `env`, `exit`, `time`, `sleep`, `now`
- HTTP and JSON: `http_get`, `http_post_json`, `json_parse`, `json_string`
- actor and async helpers: `spawn_task`, `block_on`, `poll_once`,
  `is_ready`, `is_pending`, `unwrap_ready`, `send`
- patch/runtime introspection: `patch_history`, `patch_collaboration_events`,
  `patch_undo_last`, `patch_replay_last`, `patch_replay`
- native config and host launch: `spawn_cube`, `spawn_native_viewport`,
  `spawn_native_sculpt_lab`, `native_config_string`, `native_config_int`,
  `native_config_float`, `native_config_flag`

## Duplicate Registrations

Several names are registered more than once in `runtime.rs`:

- `sleep`
- `first`
- `last`
- `int`
- `float`
- `str`
- `sqrt`

The last registration is the effective behavior because the registry is a
map. The docs should describe the effective surface, not the intermediate
implementation steps.

## Practical Rule

If you need to know whether a function is a source-loaded stdlib helper or a
runtime-native builtin, check which file registers it.
