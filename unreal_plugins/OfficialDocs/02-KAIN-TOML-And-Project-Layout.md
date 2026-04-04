# 02 KAIN TOML And Project Layout

## Why `KAIN.toml` Matters

For the UE5 lane, `KAIN.toml` is the project contract between your authored Kain source and the packager.

It tells the CLI:

- what the plugin is called
- which UE version to target
- which modules exist
- which `.kn` files belong to each module
- how modules depend on each other

## Canonical Shape

```toml
[package]
name = "MyPlugin"
version = "1.0.0"
authors = ["Your Team"]

[ue5]
plugin_name = "MyPlugin"
engine_version = "5.4"
category = "Gameplay"
description = "Example plugin"

[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"
loading_phase = "Default"
source_globs = ["src/runtime/**/*.kn"]

[[ue5.modules]]
name = "MyPluginEditor"
type = "Editor"
depends_on = ["MyPlugin"]
loading_phase = "PostEngineInit"
source_globs = ["src/editor/**/*.kn"]
```

## UE5 Fields

### `[package]`

Use this for project metadata:

- `name`
- `version`
- `authors`

### `[ue5]`

Use this for plugin-level Unreal settings:

- `plugin_name`
- `engine_version`
- `category`
- `description`

### `[[ue5.modules]]`

Use one entry per UE5 module.

Important fields:

- `name`
- `type`
- `loading_phase`
- `source_globs`
- `depends_on`

## Recommended Layouts

### Simple Runtime-Only Plugin

```text
MyPlugin/
├── KAIN.toml
└── src/
    └── main.kn
```

### Runtime + Editor Plugin

```text
MyPlugin/
├── KAIN.toml
└── src/
    ├── runtime/
    │   ├── actors.kn
    │   ├── components.kn
    │   └── systems.kn
    └── editor/
        ├── editor_module.kn
        ├── details.kn
        └── slate.kn
```

### Multi-File Domain Plugin

```text
Plugin/
├── KAIN.toml
└── src/
    ├── actors.kn
    ├── components.kn
    ├── subsystems.kn
    ├── materials.kn
    ├── shaders.kn
    └── editor.kn
```

## Module Types

Common module types you will use:

- `Runtime`
- `Editor`
- `Developer`
- `UncookedOnly`

The current packager validates duplicate names, unknown dependencies, and dependency cycles before codegen starts.

## Source Globs

`source_globs` is how you scale cleanly.

Examples:

```toml
source_globs = ["src/**/*.kn"]
source_globs = ["src/runtime/**/*.kn"]
source_globs = ["Kain/**/*.kn"]
```

## Multi-Module Pattern

The current examples show a strong recurring pattern:

- runtime module for generated gameplay/runtime code
- editor module for Slate, details customizations, and editor registration

Typical naming convention:

- `MyPlugin`
- `MyPluginEditor`

## Build Outputs

The UE5 packager can write into standard Unreal plugin shape:

- `Source/Public`
- `Source/Private`
- `Shaders`
- `Content`

It also generates supporting files like:

- `.uplugin`
- `Build.cs`
- registry side data when needed

## Best Practices

- keep runtime and editor source separated by module
- use many focused `.kn` files instead of one giant file when the plugin grows
- keep file names domain-oriented: `actors.kn`, `components.kn`, `materials.kn`, `editor_module.kn`
- avoid treating `KAIN.toml` as a dump file; keep it structural
