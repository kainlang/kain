---
inclusion: always
---

# KAIN Language Patterns for UE5 Plugin Development

## Essential Patterns

### DataTable Pattern (CSV Import)
```kn
@datatable
struct ItemData:
    id: Int
    name: String
    description: String
    icon_path: String
    value: Int
    weight: Float
    max_stack: Int
    rarity: ItemRarity
```
**Generates:** `FItemData : public FTableRowBase`  
**Use:** CSV import, data-driven design, easy iteration

### Component Pattern (Reusable Systems)
```kn
@component
struct HealthComponent:
    @replicated
    current: Float
    
    @replicated
    max: Float
    
    @transient
    regen_rate: Float
    
    @savegame
    is_invulnerable: Bool
```
**Generates:** `UHealthComponent : public UActorComponent`  
**Use:** Modular systems, composition over inheritance

### Actor Pattern (Networked Entities)
```kn
actor GameMode:
    state score: Int = 0
    state time_remaining: Float = 300.0
    
    on Server_StartMatch():
        score = 0
        time_remaining = 300.0
        Multicast_AnnounceStart()
    
    on Server_AddScore(points: Int):
        score = score + points
        Client_UpdateScore(score)
    
    on Client_UpdateScore(new_score: Int):
        println("Score: {new_score}")
    
    on Multicast_AnnounceStart():
        println("Match started!")
```
**Generates:** `AGameMode : public AActor` with RPCs  
**Use:** Game logic, networked gameplay, authoritative server

### Blueprint Function Pattern (Utility Library)
```kn
@blueprint
fn calculate_damage(base: Float, multiplier: Float, armor: Float) -> Float:
    let raw = base * multiplier
    let mitigated = raw * (1.0 - armor / 100.0)
    return max(mitigated, 0.0)

@blueprint
fn get_rarity_color(rarity: ItemRarity) -> Vec3:
    match rarity:
        ItemRarity::Common => vec3(1.0, 1.0, 1.0)
        ItemRarity::Uncommon => vec3(0.0, 1.0, 0.0)
        ItemRarity::Rare => vec3(0.0, 0.5, 1.0)
        ItemRarity::Epic => vec3(0.6, 0.0, 1.0)
        ItemRarity::Legendary => vec3(1.0, 0.5, 0.0)
        _ => vec3(0.5, 0.5, 0.5)
```
**Generates:** `UKainFunctionLibrary` static methods  
**Use:** Blueprint-callable utilities, math helpers, conversions

### Enum Pattern (Type-Safe Constants)
```kn
enum ItemRarity:
    Common
    Uncommon
    Rare
    Epic
    Legendary
    Mythic

enum ItemType:
    Weapon
    Armor
    Consumable
    Material
    Quest
    Currency

enum EquipSlot:
    Head
    Chest
    Legs
    Feet
    Hands
    MainHand
    OffHand
    Ring1
    Ring2
    Amulet
```
**Generates:** `UENUM(BlueprintType)` with display names  
**Use:** Type-safe categories, Blueprint dropdowns, serialization

## Advanced Patterns

### Inventory System Pattern
```kn
@datatable
struct ItemDefinition:
    id: Int
    name: String
    type: ItemType
    rarity: ItemRarity
    max_stack: Int
    value: Int
    weight: Float

@component
struct InventoryComponent:
    @replicated
    items: Array<ItemStack>
    
    @replicated
    capacity: Int
    
    @savegame
    gold: Int

struct ItemStack:
    item_id: Int
    quantity: Int
    durability: Float

@blueprint
fn can_add_item(inv: InventoryComponent, item_id: Int, quantity: Int) -> Bool:
    // Logic here
    return true

@blueprint
fn add_item(inv: InventoryComponent, item_id: Int, quantity: Int) -> Bool:
    // Logic here
    return true
```

### Combat System Pattern
```kn
@component
struct CombatComponent:
    @replicated
    health: Float
    
    @replicated
    max_health: Float
    
    @replicated
    armor: Float
    
    @transient
    is_attacking: Bool
    
    @transient
    combo_count: Int

actor Weapon:
    state damage: Float = 10.0
    state attack_speed: Float = 1.0
    state range: Float = 100.0
    
    on Server_Attack(target: Actor):
        // Apply damage
        Multicast_PlayAttackEffect()
    
    on Multicast_PlayAttackEffect():
        // Visual/audio feedback
        println("Attack effect!")

@blueprint
fn calculate_damage(base: Float, armor: Float, crit: Bool) -> Float:
    var damage = base * (1.0 - armor / 100.0)
    if crit:
        damage = damage * 2.0
    return damage
```

### Quest System Pattern
```kn
@datatable
struct QuestDefinition:
    id: Int
    name: String
    description: String
    objectives: Array<String>
    rewards: Array<QuestReward>
    required_level: Int

struct QuestReward:
    type: RewardType
    item_id: Int
    quantity: Int
    experience: Int
    gold: Int

enum QuestStatus:
    NotStarted
    InProgress
    Completed
    Failed

@component
struct QuestComponent:
    @replicated
    active_quests: Array<ActiveQuest>
    
    @savegame
    completed_quests: Array<Int>

struct ActiveQuest:
    quest_id: Int
    status: QuestStatus
    progress: Array<Int>
```

### Dialogue System Pattern
```kn
@datatable
struct DialogueNode:
    id: Int
    speaker: String
    text: String
    choices: Array<DialogueChoice>
    conditions: Array<String>

struct DialogueChoice:
    text: String
    next_node_id: Int
    required_item: Int
    required_quest: Int

@component
struct DialogueComponent:
    @replicated
    current_node: Int
    
    @transient
    available_choices: Array<DialogueChoice>

actor NPC:
    state dialogue_tree_id: Int = 0
    
    on Server_StartDialogue(player: Actor):
        Client_ShowDialogue(player, dialogue_tree_id)
    
    on Client_ShowDialogue(player: Actor, tree_id: Int):
        // Show UI
        println("Dialogue started")
```

## Shader Patterns

### Basic Material Shader
```kn
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform intensity: Float @1
    uniform albedo_map: Sampler2D @2
    
    let tex_color = sample(albedo_map, uv).rgb
    let final_color = tex_color * base_color * intensity
    return vec4(final_color, 1.0)
```

### Permutation Shader (Quality Levels)
```kn
shader fragment OptimizedEffect(uv: Vec2) -> Vec4:
    uniform CFG_HIGH_QUALITY: Float @0
    uniform CFG_MOBILE: Float @1
    uniform ENABLE_SHADOWS: Float @2
    
    uniform base_color: Vec3 @3
    uniform albedo_map: Sampler2D @4
    
    var color = sample(albedo_map, uv).rgb * base_color
    
    if CFG_HIGH_QUALITY:
        // Expensive calculations
        color = color * 1.2
    elif CFG_MOBILE:
        // Cheap calculations
        color = color * 0.8
    
    if ENABLE_SHADOWS:
        // Shadow calculations
        color = color * 0.9
    
    return vec4(color, 1.0)
```

### Surface Shader (Material System)
```kn
shader surface PBRMaterial(uv: Vec2) -> SurfaceOutput:
    uniform roughness: Float @0
    uniform metallic: Float @1
    uniform albedo_map: Sampler2D @2
    uniform normal_map: Sampler2D @3
    
    var out: SurfaceOutput
    out.base_color = sample(albedo_map, uv).rgb
    out.roughness = roughness
    out.metallic = metallic
    out.normal = sample(normal_map, uv).rgb
    out.emissive = vec3(0, 0, 0)
    out.opacity = 1.0
    return out
```

## Naming Conventions

### Actors
- `actor Player` → `APlayer`
- `actor GameMode` → `AGameMode`
- `actor ItemPickup` → `AItemPickup`

### Structs
- `struct Point` → `FPoint`
- `struct ItemData` → `FItemData`
- `struct QuestReward` → `FQuestReward`

### Enums
- `enum Rarity` → `ERarity`
- `enum ItemType` → `EItemType`
- `enum QuestStatus` → `EQuestStatus`

### Components
- `@component Health` → `UHealthComponent`
- `@component Inventory` → `UInventoryComponent`
- `@component Combat` → `UCombatComponent`

### RPCs
- `Server_*` → Server RPC (Reliable)
- `Client_*` → Client RPC (Reliable)
- `Multicast_*` → Multicast RPC (Reliable)

## Attribute Reference

### Struct Attributes
- `@datatable` - Makes struct inherit from FTableRowBase (CSV import)
- `@component` - Generates UActorComponent class

### Field Attributes
- `@replicated` - UPROPERTY(Replicated)
- `@savegame` - UPROPERTY(SaveGame)
- `@transient` - UPROPERTY(Transient)
- `@editdefaults` - UPROPERTY(EditDefaultsOnly)
- `@visibleonly` - UPROPERTY(VisibleAnywhere)

### Function Attributes
- `@blueprint` - Makes function Blueprint-callable in UBlueprintFunctionLibrary

### Shader Attributes
- Permutation uniforms: `CFG_*` or `ENABLE_*` prefix

## Common Mistakes to Avoid

### ❌ Don't: Manual prefixing
```kn
struct FItemData:  // Wrong - compiler adds F prefix
    id: Int
```

### ✅ Do: Let compiler handle prefixes
```kn
struct ItemData:  // Correct - becomes FItemData
    id: Int
```

### ❌ Don't: Forget attributes
```kn
struct ItemData:  // Missing @datatable
    id: Int
```

### ✅ Do: Use attributes
```kn
@datatable
struct ItemData:  // Correct - CSV import ready
    id: Int
```

### ❌ Don't: Wrong RPC naming
```kn
actor GameMode:
    on StartMatch():  // Won't be an RPC
        println("Starting")
```

### ✅ Do: Use RPC naming convention
```kn
actor GameMode:
    on Server_StartMatch():  // Correct - Server RPC
        println("Starting")
```

### ❌ Don't: Forget uniform bindings
```kn
shader fragment Test(uv: Vec2) -> Vec4:
    uniform color: Vec3  // Missing @N binding
    return vec4(color, 1.0)
```

### ✅ Do: Specify bindings
```kn
shader fragment Test(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0  // Correct
    return vec4(color, 1.0)
```

## Performance Tips

1. **Use permutations** for quality levels (zero runtime cost)
2. **Mark transient data** with `@transient` (don't replicate/save)
3. **Batch operations** in Blueprint functions (fewer calls)
4. **Use components** for modular systems (better cache locality)
5. **Leverage DataTables** for data-driven design (easy iteration)

## Testing Checklist

- [ ] Compiles without errors
- [ ] Blueprint integration works
- [ ] Networking replicates correctly
- [ ] Save/load persists data
- [ ] Shaders render correctly
- [ ] No runtime errors in UE5
- [ ] Performance is acceptable
- [ ] Documentation is clear

## Quick Reference

```kn
// DataTable
@datatable
struct Data: ...

// Component
@component
struct Comp: ...

// Actor with RPCs
actor Name:
    on Server_Method(): ...
    on Client_Method(): ...
    on Multicast_Method(): ...

// Blueprint function
@blueprint
fn utility(...) -> Type: ...

// Enum
enum Type:
    Variant1
    Variant2

// Shader
shader fragment Name(uv: Vec2) -> Vec4:
    uniform param: Type @N
    return vec4(...)

// Permutation
uniform CFG_FEATURE: Float @N
if CFG_FEATURE:
    // Compile-time branch
```

## Plugin Structure Template

```kn
// Enums first
enum ItemRarity: Common, Rare, Epic, Legendary
enum ItemType: Weapon, Armor, Consumable

// DataTables
@datatable
struct ItemData:
    id: Int
    name: String
    type: ItemType
    rarity: ItemRarity

// Components
@component
struct InventoryComponent:
    @replicated
    items: Array<ItemStack>
    capacity: Int

// Actors
actor ItemPickup:
    state item_id: Int
    on Server_Pickup(player: Actor): ...

// Blueprint utilities
@blueprint
fn calculate_value(base: Int, rarity: ItemRarity) -> Int: ...
```

This structure ensures clean, organized, production-ready plugins.
