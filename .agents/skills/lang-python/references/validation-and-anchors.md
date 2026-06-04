# Python Validation And Anchors

Load this for proof commands, source anchors, handoff routing, and anti-patterns.

## Focused Rust Proofs

Use focused tests before broad workspace runs:

```powershell
cargo test -p kain-core parses_python_import_items --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_import_supports_local_sibling_from_imports --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_import_supports_local_dotted_module_alias_calls --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_bridge_exec_scope_persists_between_calls --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_imported_host_object_calls_accept_named_kwargs --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_callable_host_objects_accept_keyword_only_args --target-dir X:\target\python-skill -- --nocapture
cargo test -p kain-python python_host_object_call_errors_name_the_symbol --target-dir X:\target\python-skill -- --nocapture
```

Run Windows Cargo tests sequentially if linker output races on the same test
binary. Use a roomy target dir when `Z:\_b` is full.

## Authored Kain Proofs

```powershell
kain check smoke.kn --target llvm
kain build smoke.kn --target llvm
kain run smoke.kn --target llvm
```

Use real on-disk `.kn` plus `.py` fixtures for local resolution claims.

For current benchmark truth:

```powershell
python benchmark/run.py --case python_interop --languages kain
python benchmark/run.py --case python_stdlib_fused --languages kain
```

Use `test-bench` if the claim is cost, throughput, region-cache behavior, or
materialization speed.

## Live Proof Surfaces

- `benchmark/cases_v2/python_interop.kn`: raw import, shared buffer/image/tensor, region cache, and GPU tensor cases.
- `benchmark/cases_v2/python_stdlib_fused.kn`: stdlib breadth, JSON/path, and `asyncio` cases.
- `benchmark/build.kn` and `benchmark/cases_v2/.telemetryrouter/router.kn`: benchmark wiring.
- `blades/python/library/2_pygame.kn`: direct Python package window proof.
- `blades/python/library/3_pygame_shader.kn`: Kain-authored shader/control plane with Python window.
- `blades/python/library/4_flet.kn`: Flet facade where Kain owns architecture.
- `blades/python/library/5_pyglet.kn`: small visible-window event-pump proof.
- `blades/python/library/6_py_shader3.kn`: Python-window graphics/control proof.
- `mcp/semantic_search/src/mcp_server.kn`: acceptable tiny `python_exec` bootstrap for decorator-heavy FastMCP.
- `smoketest/src/stdlib/interop_lane.kn`: stdlib shared-contract pressure.

## Source Anchors

- `crates/core/src/ast.rs`
- `crates/core/src/parser.rs`
- `crates/core/src/types.rs`
- `crates/core/src/runtime.rs`
- `crates/python/src/lib.rs`
- `stdlib/python.kn`
- `stdlib/interop.kn`
- `stdlib/STDLIB_MAP.llm.md`
- `runtime/native/src/core/python_runtime.c`
- `runtime/native/src/core/python_runtime_region.c`
- `runtime/native/src/core/python_runtime_buffers.c`
- `runtime/native/src/core/python_runtime_async.c`
- `runtime/native/src/core/python_runtime_gpu.c`
- `runtime/native/include/host_bridge.h`
- `runtime/native/src/core/host_bridge.c`

## Validation Ladder

1. Pick the boundary shape: import, facade, sibling `.py`, tiny bootstrap, bridge helper, region cache, async callback, materialization, or GPU adoption.
2. Prove import and kwargs first with a tiny real case.
3. If local resolution matters, use real sibling files.
4. If the result should stay foreign, prove host-object member/call behavior.
5. If Kain should own data, materialize and inspect ownership metadata.
6. If zero-copy sync matters, mutate one side and prove visibility on the other.
7. If async/evented behavior matters, prove lifecycle, await/pump, cancellation, and teardown.
8. If the feature is visual/tooling-facing, prove a real visible window or tool/server surface.
9. If the claim is performance, use benchmark reports.
10. If native memory math is involved, use Z3 or the owning runtime proof lane.

## Handoff Rules

- `bootstrap-core`: parser/import syntax, AST, type env, interpreter call dispatch, named kwargs lowering.
- `runtime-core`: embedded runtime, host handles, native startup/shutdown, host bridge registry.
- `runtime-stdlib`: public stdlib helper mismatch.
- `lang-c-abi`: C/Rust/DLL/platform package/native wheel bridge.
- `lang-semantics`: worlds, actors, ownership, effects, converge, shaders, app semantics.
- `lang-stdlib`: public root `std.*` surface design.
- `test-crash-forensics`: native executable crash or hang.

## Anti-Patterns

- Giant string-built Python apps in `python_exec`.
- Raw bridge calls throughout app code instead of a facade.
- `use python::...` fantasy syntax.
- Pretending dynamic Python modules are static Kain modules.
- Forgetting aliases for awkward or reserved Python names.
- Materializing rich Python objects too early.
- Keeping host objects forever when Kain ownership is required.
- JSON glue for data that has buffer/image/tensor lanes.
- Blaming authored Kain when the runtime/import/diagnostic substrate is wrong.
