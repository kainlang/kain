# Docs Example Suite

This folder is the runnable Kain example ladder for the current checkout.
The `.kn` files are the primary documentation. The markdown here exists to
index them, describe what each one proves, and point future agents at the
canonical validator.

## Canonical Validation Path

Run the validator from the repo root:

```bash
python3 docs/examples/validate_examples.py
```

Use the repo-local compiler binary if your PATH launcher drifted:

```bash
python3 docs/examples/validate_examples.py --kain ./target/debug/kain
```

Target a single file while iterating:

```bash
python3 docs/examples/validate_examples.py --only 11_ultimate_kain_pipeline.kn --keep-output
```

## Example Ladder

| File | What it proves | Default proof | Read next |
| --- | --- | --- | --- |
| `00_hello_and_cli.kn` | Smallest real `fn main`, `const`, interpolation, and CLI flow | `run`, `build -t rust/js/ts` | `01_types_structs_enums_patterns.kn` |
| `01_types_structs_enums_patterns.kn` | Type aliases, structs, enums, field access, and `match` | `run`, `build -t rust/js/ts` | `02_modules_traits_impls_and_comptime.kn` |
| `02_modules_traits_impls_and_comptime.kn` | `use`, inline `mod`, traits, impl blocks, methods, and `comptime` | `run`, `build -t rust/js/ts` | `03_collections_strings_filesystem.kn` |
| `03_collections_strings_filesystem.kn` | String handling plus the working filesystem/path helper lane | `run`, `build -t rust/js/ts` | `04_async_actors_and_gen_server.kn` |
| `04_async_actors_and_gen_server.kn` | Raw actors, `spawn`, `send`, `ask`, and the Kain-authored `gen_server` stdlib layer | `run`, `build -t rust/js/ts` | `05_components_ui_and_theme.kn` |
| `05_components_ui_and_theme.kn` | Components, slots, panels, text roles, themes, and token authoring | `build -t rust` | `06_shader_compute_and_gpu_artifacts.kn` |
| `06_shader_compute_and_gpu_artifacts.kn` | Fragment shaders, compute shaders, `-t hlsl`, and `gpu-artifacts` | `build -t hlsl`, `gpu-artifacts` | `07_low_level_memory_and_layout.kn` |
| `07_low_level_memory_and_layout.kn` | `ptr`, allocation, typed memory stores/loads, offsets, and layout helpers | `run`, `build -t rust/js/ts` | `08_world_patch_law_converge_and_local_orchestrate.kn` |
| `08_world_patch_law_converge_and_local_orchestrate.kn` | Locally runnable `world`, `patch`, `law`, `converge`, and `orchestrate` | `run`, `build -t rust/js/ts` | `09_ue5_authoring_gallery.kn` |
| `09_ue5_authoring_gallery.kn` | Current UE5-authored item surface that still proves on the Rust backend | `build -t rust` | `10_polyglot_bridge_pipeline.kn` |
| `10_polyglot_bridge_pipeline.kn` | Cross-runtime orchestration shape for `rust`, `python`, and `node` stages | `build -t rust/js/ts` | `11_ultimate_kain_pipeline.kn` |
| `11_ultimate_kain_pipeline.kn` | Capstone pipeline: data model, trait impl, actor, filesystem, UI, `world`, `patch`, `law`, `converge`, `orchestrate` | `run`, `build -t rust/js/ts` | `done` |

## Practical Rules

- Treat `examples_manifest.json` as the machine-readable source of truth for
  validation commands, coverage tags, and local-vs-gated expectations.
- Treat `validate_examples.py` as the canonical execution path. Do not hand-roll
  ad hoc commands unless you are debugging a failing example.
- Keep new examples brand new. Use `scripts/kain`, `smoketest`,
  `docs/kn_library`, and `src/core` as reference surfaces, not copy sources.

## Known Local Limits

- `09_ue5_authoring_gallery.kn` is validated on the Rust backend, not direct
  `-t ue5`, because the current checkout fails while loading `stdlib/ue5` with
  an unresolved `max`.
- `@target_actor` and `@ability_task` still exist as language work-in-progress
  surfaces, but they are not yet accepted by the general typechecker on the
  locally proven Rust lane.
- Polyglot `orchestrate` stages compile into Rust, JS, and TS outputs, but
  foreign runtime stages are not executed by the local interpreter during
  `kain run`. That is why `10_polyglot_bridge_pipeline.kn` is build-first.
