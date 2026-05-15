# Blade Authoring Patterns

Load this reference when creating, extending, or debugging a Kain blade workspace.

## Examples To Inspect

- `blades/kain-example`: canonical native LLVM proving ground. Read `KAIN.toml`, `src/main.kn`, `src/ui.kn`, and `run-ui.ps1` before making native executable or UI work.
- `blades/kain-fmt`: tiny library blade with simple exported helpers.
- `blades/kain-json`: library blade depending on `kain-fmt`.
- `blades/kain-mcp`: data-driven app blade with `config/*.json`, multiple source modules, and several sibling blade dependencies.
- `blades/kain-example/src/episode_layout.kn`: compact data-like layout functions for native UI coordinates.
- `blades/kain-example/src/episode_theme.kn`: style functions that keep UI color policy out of main control flow.
- `blades/kain-example/src/episode_graphics.kn`: native graphics helper shape for buffers, SPIR-V modules, pipelines, and draw submission.
- `blades/kain-labs`: reference-driven native GPU lab workspace. Use its KQuantum app as the example for recreating a `reference/*.tsx` design with data-driven mode config, compact native UI modules, native graphics, SPIR-V kernels, Z3 dispatch proofs, `spirv-val`, and a root executable proof.

## Minimal Runnable Manifest

```toml
[package]
name = "my-blade"
version = "0.1.0"
description = "Short, concrete description."

[blade]
name = "my-blade"
entry = "src/main.kn"
source_roots = ["src"]
module_roots = ["src"]
build_targets = ["llvm"]

[run]
entry = "src/main.kn"
target = "llvm"

[build]
entry = "src/main.kn"
artifact_root = ".kain/out"
cache_root = ".kain/cache/build"
profile = "debug"

[[build.tasks]]
id = "check-llvm"
kind = "check"
entry = "src/main.kn"
target = "llvm"
inputs = ["src/main.kn"]
```

For library-style blades, set `kind = "kain_library"` under `[blade]`, usually target `kain`, and keep a small `src/main.kn` smoke that exercises exported helpers.

## Dependencies

Prefer sibling blades before local rewrites:

```toml
[[blade.dependencies]]
name = "kain-json"
kind = "kain"
```

Common first picks:

- `kain-fmt`: string formatting helpers.
- `kain-log`: structured status/log output.
- `kain-fsx`: filesystem convenience.
- `kain-config`: config loading and policy shaping.
- `kain-json`: JSON text/object helpers.
- `kain-process-kit`: process and command wrappers.
- `kain-http`: HTTP helpers.
- `kain-actor-kit`: actor support helpers.
- `kain-interop-kit`: interop payload helpers.

## Source Shape

Prefer this source layout for nontrivial blades:

- `src/main.kn`: app entrypoint and high-level orchestration only.
- `src/layout.kn`: UI dimensions, slots, and coordinate policy.
- `src/theme.kn`: visual style policy.
- `src/state.kn`: world, entangle, actor, or app state helpers.
- `src/runtime.kn`: native runtime, process, net, filesystem, or graphics integration.
- `src/<feature>.kn`: focused behavior modules named after domain concepts.
- `config/*.json` or `data/*.toml`: tables, routes, command surfaces, labels, modes, or feature policy.

Keep Kain expressive:

- Use `world` and `entangle` for mirrored state instead of manually copying every value.
- Use `actor` when the behavior is message-driven or long-running.
- Use `component` for authored UI semantics.
- Use top-level constants and helper functions for shared policy.
- Use named modules instead of dumping unrelated logic into `main`.
- Use native UI and native graphics wrappers directly when building real app surfaces.

## Reference Folder Rule

If a blade has `reference/`, treat it as the design/spec corpus:

- Read relevant text, JSON, TOML, `.kn`, screenshots, and mockups before writing source.
- Mirror naming, screen hierarchy, colors, spacing, copy, behavior, and data shape as closely as current Kain allows.
- If the reference uses an unsupported capability, patch the missing language/runtime path when that is the real bottleneck.
- Do not replace a referenced UI with a prose explanation or generic dashboard.

## Blade-Local Executable Loop

Use the bundled script from repo root. It prefers Bazel, runs the direct `bazel-bin/crates/cli/kain.exe` artifact, places the `.exe` in the blade root, and moves compiler sidecars into the blade's `.kain/out/<exe-name>/` folder:

```powershell
.\.agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1 -Entry blades\my-blade\src\main.kn -OutputName my-blade.exe -Run
```

Equivalent manual loop:

```powershell
bazel build //:kain --config=dev
$bazelBin = (bazel info bazel-bin --config=dev | Select-Object -Last 1).Trim()
$kainBin = Join-Path $bazelBin "crates\cli\kain.exe"
& $kainBin check blades\my-blade\src\main.kn --target llvm
& $kainBin blades\my-blade\src\main.kn -t llvm -o blades\my-blade\my-blade.exe
.\blades\my-blade\my-blade.exe
```

Use `-BazelConfig release` for hotter compiler builds. Use `-CompilerBuild auto` only when the host may not have Bazel; `auto` can fall back to existing Cargo artifacts. Do not prefer PATH launcher shims for scripted compile proof because forwarding Kain's own `-o` flag through wrappers can be ambiguous. Use `-OutputPlacement repo-root` only when the user explicitly asks for a repo-root executable.

If the compiler writes `.ll` next to the requested output, validate it:

```powershell
toolchain\llvm\bin\llvm-as.exe blades\my-blade\my-blade.ll -o blades\my-blade\.kain\out\my-blade\my-blade.bc
```

After validation, move generated `.ll`, `.bc`, `.pdb`, `.ilk`, `*.runtime_contract.json`, and `*.realtime_app.json` into `blades/<blade>/.kain/out/<exe-name>/`. The `.exe` should stay directly in `blades/<blade>/` for zero-hunt manual testing.

## UI Proof Loop

For native UI:

- Use `blades/kain-example/run-ui.ps1` as the proven script pattern.
- Compile to `blades/<blade>/<blade>.exe`, but keep screenshot/profile artifacts under `blades/<blade>/.kain/run/`.
- Set `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH` to a BMP path under `blades/<blade>/.kain/run/`.
- Set `KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES` for noninteractive validation.
- Run the executable, assert exit code 0, assert screenshot exists, and assert it is larger than 1024 bytes.
- Use the screenshot tool or image viewer if visual quality matters.
- Run `samply --help`; if recording works locally, capture a profile for interactive/runtime-heavy apps.

## GPU And SPIR-V

Use GPU only when it materially helps. When authoring shader/compute code:

- Keep shader math inside the currently proven compiler surface unless expanding the backend is part of the task.
- Build native executable proof and shader artifact proof.
- Write generated shader artifacts under `blades/<blade>/.kain/gpu/<kernel-name>/`, never repo-root `target/<blade>/`.
- Use the `kain-spirv-codegen-validation` skill if backend behavior changes.
- Run focused GPU tests and Z3 proof lanes for storage layout, vector constructor, local-size, or index math.
- Validate emitted SPIR-V with the repo's `spirv-val`-backed test path when possible.
- If `spirv-val` reports duplicate decorations across a multi-entry shader module, inspect module-scoped type/decorator caching in `crates/gpu/src/codegen_spirv.rs` before working around it in blade source.

## Root-Cause Compile Debugging

Treat failures as signals:

- Parser/typechecker errors belong in `crates/kain-core` unless the source is simply wrong.
- Undefined native stdlib calls usually mean a missing or mismatched wrapper in `stdlib/native` or a runtime ABI export.
- LLVM verifier errors belong in `crates/kain-sys-codegen`; inspect the `.ll` and add a backend regression.
- Native link/runtime errors usually belong in `runtime/native` manifests, C ABI sources, or bootstrap/link settings.
- Blade import/discovery errors belong in `crates/kain-blades` or `crates/kain-build`, not in ad hoc path hacks.
- Buffer, capacity, index, pointer, ABI, allocator, state-machine, ownership, graphics, UI runtime, net, process, and GPU math must get Z3 proof coverage.
