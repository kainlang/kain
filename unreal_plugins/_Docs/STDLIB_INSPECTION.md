# KAIN UE5 Stdlib — Inspection & Recommendations
**Date:** February 22, 2026  
**Location:** `m:\Code\Kain\stdlib\ue5\`  
**Status:** Early stage — 3 of 12 documented files exist

---

## Current State

### What Exists

| File | Lines | Status | Quality |
|---|---|---|---|
| `common.kn` | 27 | ✅ Exists | Skeleton — 3 `@extern` functions, rest commented out |
| `math.kn` | 55 | ✅ Exists | Solid — 11 `@extern` math/vector/interp functions |
| `gameplay.kn` | 149 | ✅ Exists | Best file — 20 pure KAIN functions, fully implemented |
| `README.md` | 270 | ✅ Exists | Documents 12 files, only 3 exist |
| `actor.kn` | — | ❌ Missing | Referenced in README |
| `world.kn` | — | ❌ Missing | Referenced in README |
| `components.kn` | — | ❌ Missing | Referenced in README |
| `utilities.kn` | — | ❌ Missing | Referenced in README |
| `patterns.kn` | — | ❌ Missing | Referenced in README |
| `shaders.kn` | — | ❌ Missing | Referenced in README — **highest priority** |
| `skeletal_mesh.kn` | — | ❌ Missing | Referenced in README |
| `materials.kn` | — | ❌ Missing | Referenced in README |
| `particles.kn` | — | ❌ Missing | Referenced in README |

**9 of 12 files are missing.** The README is a spec document, not a reflection of reality yet.

---

## Code Quality Analysis

### `gameplay.kn` — The Gold Standard
This is the best file in the stdlib and shows exactly what the stdlib should be. Pure KAIN functions with real logic, no boilerplate, immediately useful:

```kain
@blueprint
fn determine_loot_rarity(luck_stat: Float) -> LootRarity:
    let roll = random() * 100.0 + luck_stat
    if roll >= 95.0:
        return LootRarity::Mythic
    if roll >= 85.0:
        return LootRarity::Legendary
    ...
```

This is 8 lines of KAIN that would be 40+ lines of C++ with Blueprint exposure. **This is the ratio you want everywhere.**

**Issues found:**
- Uses `var` keyword (line 71, 72) — invalid KAIN, should be `let`
- Uses `&&` instead of `and` (line 75) — invalid KAIN boolean operator
- `Array<T>.len()` method call — verify this is supported in the type system
- `BuffType` and `LootRarity` and `InventorySlot` are referenced but not defined in any stdlib file — they need to be in `patterns.kn` (missing)

### `math.kn` — Correct but Thin
All `@extern` declarations mapping to UE5 C++ functions. Correct pattern for engine bindings. But only covers the basics — missing the functions that actually save time:

```kain
# What's there
@extern fn Lerp(a: Float, b: Float, alpha: Float) -> Float
@extern fn VLerp(a: Vec3, b: Vec3, alpha: Float) -> Vec3

# What's missing that every plugin needs
@extern fn Clamp(value: Float, min: Float, max: Float) -> Float
@extern fn Abs(value: Float) -> Float
@extern fn Sign(value: Float) -> Float
@extern fn Floor(value: Float) -> Float
@extern fn Ceil(value: Float) -> Float
@extern fn Round(value: Float) -> Float
@extern fn Pow(base: Float, exp: Float) -> Float
@extern fn Sqrt(value: Float) -> Float
@extern fn Sin(value: Float) -> Float
@extern fn Cos(value: Float) -> Float
@extern fn Atan2(y: Float, x: Float) -> Float
@extern fn RandomFloat() -> Float
@extern fn RandomFloatInRange(min: Float, max: Float) -> Float
@extern fn RandomIntInRange(min: Int, max: Int) -> Int
```

### `common.kn` — Almost Empty
3 functions, rest commented out with `// TODO: Attribute type definitions (not yet implemented in parser)`. The attribute system (`@property`, `@function` with metadata) is the most valuable thing in this file and it's not there yet. This is the edge case you mentioned.

---

## The Architecture — Why It's Brilliant

The data-driven approach is the right call. Here's what makes it powerful:

**No backend touches needed** because:
1. `@extern fn` declarations tell the codegen "this maps to a real C++ function" — the name resolves at link time
2. Pure KAIN functions (`@blueprint fn`) compile through the normal pipeline — they become `UFUNCTION(BlueprintCallable)` automatically
3. The stdlib is just KAIN source that gets prepended/merged before compilation — zero special handling

This means **anyone can extend the stdlib** without touching Rust. That's the right architecture for a language that wants a community.

---

## What's Missing That Hurts the Most Right Now

Ranked by how often you'd reach for it in a real plugin:

### 1. `shaders.kn` — Biggest Gap
Every shader plugin reimplements the same noise functions, PBR math, and color utilities. The Materialize plugin had `perlin_noise`, `fbm`, `fresnel_schlick` etc. written inline. If those were stdlib, `shaders.kn` would be 30% shorter.

The shader stdlib is unique because it needs to target USF, not C++. Functions marked `@shader_fn` would inline into shader bodies rather than generating C++ calls.

```kain
# What shaders.kn should look like
@shader_fn
fn fresnel_schlick(cos_theta: Float, f0: Vec3) -> Vec3:
    return f0 + (vec3(1.0, 1.0, 1.0) - f0) * pow(1.0 - cos_theta, 5.0)

@shader_fn
fn distribution_ggx(n: Vec3, h: Vec3, roughness: Float) -> Float:
    let a = roughness * roughness
    let a2 = a * a
    let n_dot_h = max(dot(n, h), 0.0)
    let denom = n_dot_h * n_dot_h * (a2 - 1.0) + 1.0
    return a2 / (3.14159 * denom * denom)

@shader_fn
fn fbm(uv: Vec2, octaves: Int) -> Float:
    let value = 0.0
    let amplitude = 0.5
    let frequency = 1.0
    let i = 0
    while i < octaves:
        value = value + amplitude * noise(uv * frequency)
        amplitude = amplitude * 0.5
        frequency = frequency * 2.0
        i = i + 1
    return value
```

### 2. `patterns.kn` — Type Definitions for gameplay.kn
`gameplay.kn` references `BuffType`, `LootRarity`, `InventorySlot`, `HealthComponent` etc. but none of these are defined anywhere in the stdlib. The file currently can't compile standalone. `patterns.kn` needs to define these as KAIN structs/enums.

```kain
enum LootRarity:
    Common
    Uncommon
    Rare
    Epic
    Legendary
    Mythic
    LootRarity_MAX

enum BuffType:
    Damage
    Defense
    Speed
    Healing
    BuffType_MAX

struct InventorySlot:
    item_id: Int
    quantity: Int
    max_stack: Int
```

### 3. `actor.kn` + `world.kn` — The Daily Drivers
These are the functions you call in literally every actor. `GetActorLocation`, `SetActorLocation`, `SpawnActor`, `GetWorldTimeSeconds` — currently only 2 of these exist (in `common.kn`). Every plugin has to redeclare them or rely on the codegen knowing about them implicitly.

### 4. `utilities.kn` — The Remap/Clamp Gap
`remap`, `smooth_step`, `lerp_color`, `distance_2d` — these come up constantly and are currently written inline in every plugin that needs them.

---

## Bugs to Fix Before the Overnight Run

These are in the existing stdlib files and will cause parse errors if any plugin imports them:

| File | Line | Issue | Fix |
|---|---|---|---|
| `gameplay.kn` | 71-72 | `var total_weight`, `var i` | Change to `let` |
| `gameplay.kn` | 75 | `&&` operator | Change to `and` |
| `gameplay.kn` | 75 | `item_weights.len()` | Verify Array method syntax |

---

## Recommendations — Priority Order

### Immediate (before overnight run)

**1. Fix the 3 syntax bugs in `gameplay.kn`**  
`var` → `let`, `&&` → `and`. Takes 2 minutes.

**2. Create `patterns.kn`**  
Define the structs/enums that `gameplay.kn` depends on. Without this, `gameplay.kn` is a floating file with undefined types.

### High Value (next session)

**3. Write `shaders.kn`**  
PBR functions, noise functions, color grading, UV utilities. Mark with `@shader_fn` so the codegen knows to inline rather than call. This is the single highest-leverage stdlib file for the Factory plugins — Materialize, VoxelForgePro, KainFlow, Cosmos all need these.

**4. Write `actor.kn` + `world.kn`**  
Complete the `@extern` bindings for the 20 most common UE5 actor/world functions. These are pure declarations — fast to write, high daily value.

**5. Write `utilities.kn`**  
Pure KAIN math helpers: `remap`, `smooth_step`, `lerp_color`, `random_range`, `weighted_random`. Same pattern as `gameplay.kn` — pure functions, no `@extern` needed.

### Architecture Improvements

**6. `@shader_fn` annotation**  
Distinguish shader-inlined functions from C++ Blueprint functions. Currently there's no way to write a stdlib function that targets USF specifically. This is the edge case you mentioned — needs one small parser addition and a codegen dispatch check.

**7. Namespace the stdlib**  
Right now all stdlib functions are global. As the stdlib grows to 150+ functions, name collisions become a real risk. Consider `math::lerp`, `gameplay::apply_damage` etc. — or at minimum a convention like `kn_lerp` for stdlib functions.

**8. `@deprecated` annotation**  
When you improve a stdlib function, you need a way to warn users who are calling the old version. One annotation, one compiler warning.

---

## The Real Ratio Opportunity

Going back to the 1:5 vs 1:3 question from earlier — the stdlib is **the entire answer**.

Right now Materialize is 2:1 lines because shader functions like `fresnel_schlick`, `perlin_noise`, `fbm` are written inline. If those move to `shaders.kn`:
- Materialize `shaders.kn` shrinks by ~300 lines
- Every other shader plugin gets them for free
- The ratio for new plugins starts at 1:4+ immediately

The stdlib doesn't improve the ratio of the *compiler output* — it improves the ratio of *what you have to write*. That's the right place to attack it.

**Target:** 12 stdlib files fully implemented → new plugins start at 1:6 to 1:8 lines with zero extra work.

---

## Summary

| Metric | Current | Target |
|---|---|---|
| Files implemented | 3 / 12 | 12 / 12 |
| Total stdlib lines | 231 | ~1,500-2,000 |
| Functions available | ~23 | 150+ |
| Shader functions | 0 | 40+ |
| Syntax bugs | 3 | 0 |
| Missing type definitions | ~8 types | 0 |

The foundation is correct. The `@extern` + pure KAIN function split is the right architecture. `gameplay.kn` proves the pattern works. The stdlib just needs to be filled out — it's a writing task, not an architecture task.
