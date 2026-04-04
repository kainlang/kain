# 06 Blueprints GAS And Config

This document covers three adjacent systems that matter a lot for real UE5 plugin adoption:

- Blueprint generation
- Gameplay Ability System generation
- configuration and developer settings generation

## Blueprint Generation

The `ue5-blueprints` crate has a dual-path design:

- try binary `.uasset` generation first
- fall back to factory-based generation when binary generation is not supported for that blueprint

That makes the lane flexible instead of forcing one output strategy.

## What Blueprint Support Means In Practice

There are multiple Blueprint-related surfaces in the current stack:

### Blueprint-callable functions

```kain
@blueprint_callable
fn ApplyDamage(base_damage: Float) -> Float:
    return base_damage
```

### Blueprint-native events

```kain
@blueprint_event
fn OnExplode():
    println("explode")
```

### Blueprint function libraries

```kain
@blueprint
fn CalculateDamage(base: Float, multiplier: Float) -> Float:
    return base * multiplier
```

### Full Blueprint asset generation

The blueprint crate also models:

- component hierarchies
- class defaults
- event graphs
- engine-version-aware asset output

## Gameplay Ability System

The GAS lane is powerful, but you need to understand its maturity honestly.

Current status by phase:

- `@gameplay_tags`: production
- `@attribute_set struct`: production
- `@ability struct`: IR and codegen exist, but not fully wired into the main CLI pipeline
- `@ability_task struct`: same situation
- `@target_actor struct`: same situation
- `@effect struct`: IR and codegen exist, but not fully wired into the main CLI pipeline
- `@gameplay_cue struct`: same situation

## Example GAS Syntax

```kain
@gameplay_tags namespace Combat:
    Attack:
        Melee
        Ranged

@attribute_set struct CombatAttributes:
    @replicated(on_rep: true)
    health: Float = 100.0
```

## Config And Developer Settings

The `ue5-config` crate gives Kain a serious settings story for UE5 plugins.

It supports:

- generated developer settings classes
- `.ini` integration
- optional Blueprint accessors
- optional console variable wiring
- category-aware settings classes

### Example

```kain
@config(category: "Game", display_name: "Game Settings")
struct GameSettings:
    @setting(blueprint: true, min: 0.0, max: 1000.0)
    player_speed: Float = 300.0

    @setting(cvar: "game.DebugMode")
    debug_mode: Bool = false
```

## Current Config Limits

The current config docs call out a few limits worth keeping visible:

- nested config structs are not currently supported
- array config fields are not currently supported
