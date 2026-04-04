# 01 Getting Started

## Mental Model

Kain UE5 authoring is best understood like this:

- Kain is a higher-level authoring language for Unreal Engine 5
- the UE5 pipeline turns Kain constructs into Unreal-native outputs
- the goal is to remove repetitive UE5 boilerplate, not to replace UE5 itself

In practice, you still end up with normal UE5-shaped artifacts:

- `AActor` classes
- `UActorComponent` classes
- `UWorldSubsystem` classes
- `USTRUCT` and `UENUM`
- `UFUNCTION` and `UPROPERTY`
- Slate widgets and editor modules
- `Build.cs`, `.uplugin`, shader files, and generated assets

## Minimum Workflow

The smallest useful flow is:

1. Create a plugin folder with a `KAIN.toml`
2. Author one or more `.kn` files
3. Run `kain build --ue5`
4. Open the generated plugin in Unreal

## Minimal Example

### `src/main.kn`

```kain
enum DamageType:
    Physical
    Fire

@component
struct HealthComponent:
    @replicated
    current: Float

actor CombatDummy:
    @replicated
    state health: Float = 100.0

    on BeginPlay():
        println("CombatDummy spawned")

    @blueprint_callable
    fn TakeDamage(amount: Float):
        health = health - amount
        if health < 0.0:
            health = 0.0
```

### `KAIN.toml`

```toml
[package]
name = "MyPlugin"
version = "0.1.0"

[ue5]
plugin_name = "MyPlugin"
engine_version = "5.4"
category = "Gameplay"
description = "My first Kain UE5 plugin"

[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"
loading_phase = "Default"
source_globs = ["src/**/*.kn"]
```

### Build

```powershell
kain build --ue5
```

## What Gets Generated

Depending on the source and config, the UE5 packager can generate:

- runtime module C++ headers and source
- editor module C++ when editor constructs are present
- shader files and shader helper code
- material assets
- graph assets and graph runtime types
- `Build.cs`
- `.uplugin`
- asset registry side data

## The Validation Story

Before C++ generation, the UE5 pipeline runs a semantic validator called "The Oracle".

Its job is to catch Unreal-specific mistakes earlier than a full engine compile, including:

- RPC naming issues
- replication misuse
- Blueprint annotation conflicts
- engine type name collisions
- some UHT-oriented invalid states

That means Kain is not just text templating. It is doing UE5-aware validation as part of the authoring loop.

## First Files To Study In The Repo

If you want a single showcase that exercises many non-GAS features, start with:

- `unreal_plugins/Example_Comprehensive/src/main.kn`

If you want more focused examples, start with:

- `unreal_plugins/Example_Blueprint`
- `unreal_plugins/Example_Slate`
- `unreal_plugins/Example_Material`
- `unreal_plugins/Example_Shader`
- `unreal_plugins/Example_Graph`

## Good First Use Cases

Kain is strongest today when you are trying to reduce repetitive UE5 plugin authoring in areas like:

- gameplay systems
- plugin runtime modules
- editor tooling
- utility Blueprint libraries
- graph tools
- shader-heavy tools
- material-processing plugins

## What To Expect From The Language

Kain source is intentionally denser than the generated C++.

Local pipeline docs from the plugin compilation work report approximate compression in the range of:

- about `1:5` for simpler UE5 code
- up to about `1:8` for shader-heavy or Blueprint-heavy output

That is a feature, not a bug. The whole point is to author less and generate more.
