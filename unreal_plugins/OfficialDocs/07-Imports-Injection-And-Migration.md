# 07 Imports Injection And Migration

Kain's UE5 value is strongest as a DSL and codegen system, but the surrounding import and injection features are a major adoption accelerator.

This matters if you are:

- migrating existing code
- bootstrapping a plugin from another language
- adding Kain into an existing Unreal codebase without a rewrite

## Non-Destructive Injection

`kain inject` is the cleanest bridge into an existing UE5 plugin.

Example:

```powershell
kain inject src/new_actor.kn --ue5 --plugin MyPlugin
```

What the current packager is designed to do:

- target an existing plugin
- validate the source through the UE5 pipeline
- generate files without forcing a full destructive rewrite
- preserve existing plugin code while adding generated content

## Import Lanes

Kain also ships import paths that can help seed UE5-oriented authoring:

- `kain import-rust`
- `kain import-ts`
- `kain import-c`
- `kain import-asm`

These should be treated as migration helpers and advanced workflows, not as the main UE5 headline.

## Recommended Migration Workflows

### Workflow A: Greenfield Plugin

1. Start with a fresh `KAIN.toml`
2. Author runtime and editor modules directly in Kain
3. Build through `kain build --ue5`

### Workflow B: Existing Plugin, Surgical Adoption

1. Keep the existing Unreal plugin
2. Add one or two new `.kn` files
3. Use `kain inject --ue5`
4. Expand from there

### Workflow C: Foreign Code Bootstrap

1. Import existing Rust, TS, or C code
2. Clean up the generated `.kn`
3. Split it into plugin domains
4. Move into the normal UE5 build path

## Recommended Boundaries

Do not promise that imported code will instantly become ideal UE5 plugin code.

A more honest and useful claim is:

- imports accelerate translation and bootstrapping
- Kain then becomes the maintained authoring layer for the UE5 plugin
