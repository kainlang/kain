---
name: kain-blade-workspace
description: Use when creating, extending, compiling, or debugging a runnable Kain blade workspace or Kain-authored app/file under D:\Kain-Lang, especially when the task asks for a new blade, a desired `.kn` file, reference-driven recreation from a `reference/` folder, native LLVM executable output in the blade root, local `.kain` build artifacts, native UI, graphics, GPU/SPIR-V shader work, or fixing Kain compiler/runtime/bootstrap blockers exposed by blade compilation.
---

# Kain Blade Workspace

Use this skill to turn a requested Kain idea into a real blade workspace with the runnable executable in that blade's root folder and all generated artifacts under that blade's `.kain/` directory. Pair it with `kain-engineer` for Kain language details. Use `kain-blades-system` only when changing the blade resolver/build system itself. Use `kain-spirv-codegen-validation`, `kain-ui-native-pipeline`, `kain-3d-pipeline`, or native runtime skills when the bottleneck enters those systems.

## Required Workflow

1. Read `ARCHITECTURE.md` and `MEMORY.md` before changing code.
2. Inspect `blades/kain-example/KAIN.toml`, `blades/kain-example/src/main.kn`, and `blades/kain-example/src/ui.kn` as the canonical native LLVM proof blade.
3. Inspect sibling library blades before reimplementing helpers: `kain-fmt`, `kain-log`, `kain-fsx`, `kain-config`, `kain-process-kit`, `kain-http`, `kain-actor-kit`, `kain-interop-kit`, and `kain-json`.
4. If the requested blade already exists, preserve its identity and work inside it. If it has a `reference/` folder, read the relevant reference files first and recreate the requested Kain file from those references as faithfully as possible, including UI/layout/style/interaction details.
5. If the blade does not exist, create `blades/<blade-name>/KAIN.toml`, `src/main.kn`, and focused helper modules under `src/`. Prefer manifest data and local config files over hardcoded routes, modes, large string tables, or file lists.
6. Author expressive Kain. Use modules, top-level constants, named helper functions, `world`, `entangle`, `actor`, `component`, native UI, native graphics, filesystem, process, net, and ownership features when they fit. Do not collapse everything into "function and let soup."
7. Keep current parser reality in mind: exported helper signatures should stay on one line unless you are actively fixing the frontend import-scan path. Multiline `pub fn` signatures in blade helper modules are still a live footgun in this checkout.
8. Compile the selected entry to the blade root, not the repo root: `blades/<blade-name>/<blade-name>.exe` unless the user explicitly asks for a shared artifact elsewhere. Keep `.ll`, `.bc`, `.pdb`, `.ilk`, runtime contract JSON, realtime app JSON, SPIR-V artifacts, screenshots, profiles, and scratch outputs under the blade's `.kain/`. Use `scripts/compile_kain_blade_to_root.ps1` when possible; it defaults to a direct Bazel-built `//:kain` compiler artifact, runs Kain from the discovered blade root so nested blades resolve local modules, and avoids stale Cargo binaries or launcher shims.
9. Run the executable from the blade root. For UI/interactivity, capture a screenshot under the blade's `.kain/` directory and verify it is non-empty. If `samply` can record on the host, store the profile under `.kain/`; if it cannot, record that limitation.
10. If compilation fails because Kain, the runtime, bootstrap, stdlib, native UI, graphics, GPU, or build lane is missing real capability, patch the root cause instead of giving up or dumbing the Kain file down. Add the smallest durable regression proof/test for the owning subsystem.
11. Update `ARCHITECTURE.md`, `MEMORY.md`, and any relevant Kain skill when the work adds a meaningful pipeline, subsystem behavior, recurring gotcha, or validation lane.

## Reference Loading

Open `references/blade-authoring-patterns.md` when creating or reshaping a blade. It summarizes manifest patterns, composition rules, compile commands, UI/GPU validation expectations, and the examples to inspect.

For reference-driven tasks:

- Read every relevant file under `<blade>/reference/` before authoring.
- If references include images, screenshots, or mockups, view them with the image/screenshot tools and reproduce the visual hierarchy in Kain native UI rather than describing it in prose.
- Treat the reference as the source of truth unless it conflicts with current Kain compiler/runtime reality. If it conflicts, implement the closest faithful version and patch the language/runtime blocker when practical.

## Compile Proof

Default native LLVM loop from repo root:

```powershell
bazel build //:kain --config=dev
$bazelBin = (bazel info bazel-bin --config=dev | Select-Object -Last 1).Trim()
$kainBin = Join-Path $bazelBin "crates\cli\kain.exe"
& $kainBin check blades\<blade-name>\src\main.kn --target llvm
& $kainBin blades\<blade-name>\src\main.kn -t llvm -o blades\<blade-name>\<blade-name>.exe
.\blades\<blade-name>\<blade-name>.exe
```

Prefer explicit blade-root executable names such as `blades/<blade-name>/<blade-name>.exe` or the user-requested app name inside the blade root. Do not scatter blade build outputs into repo-root `target/` or repo-root `.kain/` unless the user explicitly asks for a shared workspace artifact. Keep generated `.ll`, `.bc`, `.pdb`, `.ilk`, runtime JSON, screenshots, profiles, shader artifacts, and scratch outputs under `blades/<blade-name>/.kain/`. Use `--config=release` / `-BazelConfig release` when compiler performance or benchmark-quality native tuning matters.

For UI blades, mirror the `blades/kain-example/run-ui.ps1` proof style: check, compile, verify LLVM IR when useful, run with `KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH`, set an auto-exit frame budget for noninteractive proof, and assert the screenshot exists and has real size.

For GPU/SPIR-V work, compile the Kain native executable and also validate shader artifacts. Use the GPU/SPIR-V skill when backend code changes are needed; shader math is not proved until the relevant Z3 lane and `spirv-val`-backed tests accept it.

## Native C FFI / Vulkan Blade Pattern

When a blade uses `[c_ffi]` to call a blade-local C bridge, keep the bridge source under `blades/<blade>/native/`, compile objects or shared sidecars into `blades/<blade>/.kain/native/`, and point `KAIN.toml [[c_ffi.libraries]].shared_lib` at that blade-local artifact. Prebuild scripts should compile/validate shaders into `.kain/gpu/<lane>/`, compile the C object into `.kain/native/`, then call `scripts/compile_kain_blade_to_root.ps1` for the Kain executable.

Prefer numeric handles/status/counters and report files across Kain/C boundaries. Do not expose C-owned `const char*` return functions through a Kain `[c_ffi]` header unless ownership has been explicitly modeled; Kain should own Kain strings, C should write textual diagnostics to `.kain/run/*.txt`.

For Vulkan or D3D-style bridges, dogfood the actual window path from Kain, then validate:

- `spirv-val --target-env vulkan1.3` for every shader artifact used by the window.
- A direct C harness under `.kain/native/` when the failure might be C/Vulkan rather than Kain.
- Z3 for fixed-array bounds, shader byte counts, draw-counter overflow, and cleanup-after-partial-init invariants.
- A non-empty screenshot/report under `.kain/run/` and a blade-root exe left ready to launch.

`blades/vulkain` is the current reference for the minimal raw package pattern: keep the Kain surface tiny, compile bridge/shader artifacts under `.kain/`, and let consuming blades own higher-level intent. On Windows, if the bridge is a blade-local DLL, stage that DLL beside the blade-root exe for direct testing even though link-time should prefer the sibling `.lib`.

## Failure Policy

Do not stop at "Kain cannot compile this." Triage the failure into one of these buckets:

- Authoring mistake: fix the Kain file while preserving the requested behavior.
- Missing stdlib/native wrapper: add the wrapper in the correct stdlib/native or blade helper layer.
- Compiler frontend/typechecker gap: patch `crates/kain-core` with focused tests.
- LLVM/direct-C/backend gap: patch `crates/kain-sys-codegen`, inspect emitted `.ll`, and validate with `llvm-as`.
- Runtime/native ABI gap: patch `runtime/native`, use Z3 for memory/index/state math, and run native conformance or direct C smokes.
- Blade/build resolver gap: use `kain-blades-system` and keep discovery/build behavior in `crates/kain-blades` or `crates/kain-build`.
- GPU/SPIR-V gap: use solver-backed proofs plus `spirv-val` and focused GPU tests.

For low-level runtime, allocation, index, buffer, ABI, state-machine, ownership, UI runtime, graphics, networking, process, or GPU math, use Z3 MCP. The proof target is `unsat`, not "seems fine."

## Quality Bar

Create compact, real software. UI should be killer-no-filler: dense, intentional, minimal panes, no bloated boxes, no placeholder explainware. Kain source should show the language off honestly: named concepts, cohesive modules, reusable library blades, data-driven manifests, local `.kain/` artifacts, and a built executable the user can run immediately from the blade root.
