---
name: lang-blades
description: Use when creating or extending a runnable Kain blade workspace, including `KAIN.toml`, blade-local `.kn` code, native bridge layout under `native/`, local artifacts under `.kain/`, and the authored compile-run loop for a real blade.
---

# Lang Blades

Use this skill when the unit of work is a blade, not just a loose `.kn` file.

## Fast Loop

```powershell
kain blades build . --json
kain blades run <blade>
blade build . --json
blade run <blade> --target auto -- <args>
```

If you want a blade-root executable proof:

```powershell
.\.agents\skills\lang-blades\scripts\compile_kain_blade_to_root.ps1 -Entry blades\<blade>\src\main.kn -OutputName <blade>.exe -Run
```

## Minimal Blade Shape

```toml
[package]
name = "my-blade"
version = "0.1.0"

[blade]
name = "my-blade"
entry = "src/main.kn"
source_roots = ["src"]
module_roots = ["src"]
build_targets = ["llvm"]
```

```kn
fn main() -> Int:
    println("blade ok")
    return 0
```

## What To Do

- Keep authored source in `src/`, native bridges in `native/`, and generated artifacts in blade-local `.kain/`.
- Prefer blade-local validation loops over repo-root artifact sprawl.
- If the blade needs a C bridge, keep the bridge owned by the blade or package that actually exposes it.
- Build blades that prove a capability, not placeholder folders that only compile.

## Hand Off When

- Use `tool-build-system` when blade discovery, Bazel sync, launcher behavior, or resolver/build internals are the real issue.
- Use `bootstrap-core`, `runtime-core`, `runtime-stdlib`, or `runtime-gpu` when the blade exposed an engine defect instead of an authored one.
- Co-trigger `lang-ui`, `lang-gpu`, `lang-actors`, `lang-stdlib`, or `lang-c-abi-ffi` when the blade centers one of those domains.
