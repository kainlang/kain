# GameplayTags Examples — Real-World Patterns from Lyra and NinjaGAS

> **Practical examples of tag usage in production GAS implementations**

---

## Table of Contents

1. [Lyra Tag Definitions](#lyra-tag-definitions)
2. [NinjaGAS Tag Definitions](#ninjagas-tag-definitions)
3. [Ability Tag Usage](#ability-tag-usage)
4. [Effect Tag Usage](#effect-tag-usage)
5. [Tag Queries](#tag-queries)
6. [Tag Events](#tag-events)
7. [Tag-Based Ability Activation](#tag-based-ability-activation)
8. [Tag-Based Effect Application](#tag-based-effect-application)
9. [Complex Tag Patterns](#complex-tag-patterns)

---

## Lyra Tag Definitions

### Native Tags (LyraGameplayTags.h)

**Ability activation failure tags:**
```cpp
namespace LyraGameplayTags
{
    // Ability activation failures
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_IsDead, 
        "Ability.ActivateFail.IsDead", 
        "Ability failed to activate because its owner is dead."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_Cooldown, 
        "Ability.ActivateFail.Cooldown", 
        "Ability failed to activate because it is on cool down."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_Cost, 
        "Ability.ActivateFail.Cost", 
        "Ability failed to activate because it did not pass the cost checks."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_TagsBlocked, 
        "Ability.ActivateFail.TagsBlocked", 
        "Ability failed to activate because tags are blocking it."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_TagsMissing, 
        "Ability.ActivateFail.TagsMissing", 
        "Ability failed to activate because tags are missing."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_Networking, 
        "Ability.ActivateFail.Networking", 
        "Ability failed to activate because it did not pass the network checks."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_ActivateFail_ActivationGroup, 
        "Ability.ActivateFail.ActivationGroup", 
        "Ability failed to activate because of its activation group."
    );
}
```

**Ability behavior tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Ability_Behavior_SurvivesDeath, 
        "Ability.Behavior.SurvivesDeath", 
        "An ability with this type tag should not be canceled due to death."
    );
}
```

**Input tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InputTag_Move, 
        "InputTag.Move", 
        "Move input."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InputTag_Look_Mouse, 
        "InputTag.Look.Mouse", 
        "Look (mouse) input."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InputTag_Look_Stick, 
        "InputTag.Look.Stick", 
        "Look (stick) input."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InputTag_Crouch, 
        "InputTag.Crouch", 
        "Crouch input."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InputTag_AutoRun, 
        "InputTag.AutoRun", 
        "Auto-run input."
    );
}
```

**Initialization state tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InitState_Spawned, 
        "InitState.Spawned", 
        "1: Actor/component has initially spawned and can be extended"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InitState_DataAvailable, 
        "InitState.DataAvailable", 
        "2: All required data has been loaded/replicated and is ready for initialization"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InitState_DataInitialized, 
        "InitState.DataInitialized", 
        "3: The available data has been initialized for this actor/component, but it is not ready for full gameplay"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        InitState_GameplayReady, 
        "InitState.GameplayReady", 
        "4: The actor/component is fully ready for active gameplay"
    );
}
```

**Gameplay event tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        GameplayEvent_Death, 
        "GameplayEvent.Death", 
        "Event that fires on death. This event only fires on the server."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        GameplayEvent_Reset, 
        "GameplayEvent.Reset", 
        "Event that fires once a player reset is executed."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        GameplayEvent_RequestReset, 
        "GameplayEvent.RequestReset", 
        "Event to request a player's pawn to be instantly replaced with a new one at a valid spawn location."
    );
}
```

**SetByCaller tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        SetByCaller_Damage, 
        "SetByCaller.Damage", 
        "SetByCaller tag used by damage gameplay effects."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        SetByCaller_Heal, 
        "SetByCaller.Heal", 
        "SetByCaller tag used by healing gameplay effects."
    );
}
```

**Status tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Status_Crouching, 
        "Status.Crouching", 
        "Target is crouching."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Status_AutoRunning, 
        "Status.AutoRunning", 
        "Target is auto-running."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Status_Death, 
        "Status.Death", 
        "Target has the death status."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Status_Death_Dying, 
        "Status.Death.Dying", 
        "Target has begun the death process."
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Status_Death_Dead, 
        "Status.Death.Dead", 
        "Target has finished the death process."
    );
}
```

**Movement mode tags:**
```cpp
namespace LyraGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Movement_Mode_Walking, 
        "Movement.Mode.Walking", 
        "Default Character movement tag"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Movement_Mode_NavWalking, 
        "Movement.Mode.NavWalking", 
        "Default Character movement tag"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Movement_Mode_Falling, 
        "Movement.Mode.Falling", 
        "Default Character movement tag"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Movement_Mode_Swimming, 
        "Movement.Mode.Swimming", 
        "Default Character movement tag"
    );
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(
        Movement_Mode_Flying, 
        "Movement.Mode.Flying", 
        "Default Character movement tag"
    );
    
    // Movement mode mapping
    const TMap<uint8, FGameplayTag> MovementModeTagMap =
    {
        { MOVE_Walking, Movement_Mode_Walking },
        { MOVE_NavWalking, Movement_Mode_NavWalking },
        { MOVE_Falling, Movement_Mode_Falling },
        { MOVE_Swimming, Movement_Mode_Swimming },
        { MOVE_Flying, Movement_Mode_Flying },
    };
}
```

### Lyra .ini Tags (ShooterCore plugin)

**From `ShooterCoreTags.ini`:**
```ini
[/Script/GameplayTags.GameplayTagsList]
; Weapon tags
GameplayTagList=(Tag="Ability.ActivateFail.MagazineFull",DevComment="Cannot reload with full magazine")
GameplayTagList=(Tag="Ability.ActivateFail.NoSpareAmmo",DevComment="No ammo to reload")

; Movement events
GameplayTagList=(Tag="Event.Movement.ADS",DevComment="Aim down sights event")
GameplayTagList=(Tag="Event.Movement.Dash",DevComment="Dash movement event")

; Weapon types
GameplayTagList=(Tag="Weapon.Type.Rifle",DevComment="Rifle weapon type")
GameplayTagList=(Tag="Weapon.Type.Pistol",DevComment="Pistol weapon type")
GameplayTagList=(Tag="Weapon.Type.Shotgun",DevComment="Shotgun weapon type")

; Damage types
GameplayTagList=(Tag="Damage.Type.Point",DevComment="Point damage")
GameplayTagList=(Tag="Damage.Type.Radial",DevComment="Radial/splash damage")

; Gameplay cues
GameplayTagList=(Tag="GameplayCue.Weapon.Rifle.Fire",DevComment="Rifle fire cue")
GameplayTagList=(Tag="GameplayCue.Weapon.Impact",DevComment="Weapon impact cue")
```

---

## NinjaGAS Tag Definitions

### Native Tags (NinjaGASTags.h)

```cpp
// Passive ability tag
UE_DEFINE_GAMEPLAY_TAG_COMMENT(
    Tag_GAS_Ability_Passive, 
    "Ability.Passive", 
    "If present, activates the ability as soon as the avatar is set."
);

// Initial cooldown tag
UE_DEFINE_GAMEPLAY_TAG_COMMENT(
    Tag_GAS_Ability_InitialCooldown, 
    "Ability.InitialCooldown", 
    "If present, applies the cooldown Gameplay Effect as soon as the avatar is set."
);

// Activation failure tags
UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_BlockedByTags, 
    "Activation.Fail.BlockedByTags"
);

UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_CantAffordCost, 
    "Activation.Fail.CantAffordCost"
);

UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_IsDead, 
    "Activation.Fail.IsDead"
);

UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_MissingTags, 
    "Activation.Fail.MissingTags"
);

UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_Networking, 
    "Activation.Fail.Networking"
);

UE_DEFINE_GAMEPLAY_TAG(
    Tag_GAS_Activation_Fail_OnCooldown, 
    "Activation.Fail.OnCooldown"
);
```

---

## Ability Tag Usage

### Example 1: Melee Attack Ability

```cpp
UCLASS()
class UMeleeAttackAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UMeleeAttackAbility()
    {
        // What this ability IS
        AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Melee")));
        
        // Tags granted while active
        ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Attacking")));
        ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Busy")));
        
        // Must be alive to use
        ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive")));
        
        // Cannot use while stunned, silenced, or dead
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.CC.Stunned")));
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.CC.Silenced")));
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Death")));
        
        // Blocks other abilities while active
        BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack")));
        BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Defend")));
        
        // Cancels channeled abilities
        CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Channeled")));
    }
};
```

### Example 2: Heal Ability with Complex Requirements

```cpp
UCLASS()
class UHealAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UHealAbility()
    {
        // Ability identity
        AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Utility.Heal")));
        
        // Casting state
        ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Casting")));
        
        // Source requirements (caster)
        ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive")));
        ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.CanCast")));
        
        // Source blocks (caster)
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.CC.Silenced")));
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.OutOfMana")));
        
        // Target requirements
        TargetRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive")));
        TargetRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Injured")));
        
        // Target blocks
        TargetBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.FullHealth")));
        TargetBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Immune.Healing")));
    }
};
```

### Example 3: Passive Ability (NinjaGAS Pattern)

```cpp
UCLASS()
class UPassiveRegenAbility : public UGameplayAbility
{
    GENERATED_BODY()
    
public:
    UPassiveRegenAbility()
    {
        // Mark as passive - auto-activates on avatar set
        AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Passive")));
        AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Utility.Regen")));
        
        // Always active
        ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Regenerating")));
        
        // Only works while alive
        ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive")));
        
        // Disabled while in combat
        ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.InCombat")));
    }
};
```

---

## Effect Tag Usage

### Example 1: Damage Effect

```cpp
UCLASS()
class UDamageEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UDamageEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;
        
        // Asset tags (what this effect IS)
        UAssetTagsGameplayEffectComponent* AssetTagsComp = 
            CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Damage"))
        );
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Physical"))
        );
        
        // Application requirements
        UTargetTagRequirementsGameplayEffectComponent* RequirementsComp = 
            CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("Requirements"));
        
        // Target must be alive
        RequirementsComp->ApplicationTagRequirements.RequireTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Alive"))
        );
        
        // Target cannot be immune
        RequirementsComp->ApplicationTagRequirements.IgnoreTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Immune.Physical"))
        );
        RequirementsComp->ApplicationTagRequirements.IgnoreTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Invulnerable"))
        );
    }
};
```


### Example 2: Burn DOT Effect

```cpp
UCLASS()
class UBurnEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UBurnEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        DurationMagnitude = FScalableFloat(5.0f);
        Period = 1.0f;  // Tick every second
        
        // Asset tags
        UAssetTagsGameplayEffectComponent* AssetTagsComp = 
            CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Fire"))
        );
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Type.DOT"))
        );
        
        // Granted tags (applied to target)
        UTargetTagsGameplayEffectComponent* TargetTagsComp = 
            CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
        TargetTagsComp->InheritableGrantedTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Debuff.Burning"))
        );
        TargetTagsComp->InheritableGrantedTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.DOT"))
        );
        
        // Application requirements
        UTargetTagRequirementsGameplayEffectComponent* RequirementsComp = 
            CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("Requirements"));
        RequirementsComp->ApplicationTagRequirements.RequireTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Alive"))
        );
        RequirementsComp->ApplicationTagRequirements.IgnoreTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Immune.Fire"))
        );
        
        // Remove if target becomes immune
        RequirementsComp->RemovalTagRequirements.RequireTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Immune.Fire"))
        );
    }
};
```

### Example 3: Stun Effect with Ability Blocking

```cpp
UCLASS()
class UStunEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UStunEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        DurationMagnitude = FScalableFloat(3.0f);
        
        // Asset tags
        UAssetTagsGameplayEffectComponent* AssetTagsComp = 
            CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.CC.Stun"))
        );
        
        // Granted tags
        UTargetTagsGameplayEffectComponent* TargetTagsComp = 
            CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
        TargetTagsComp->InheritableGrantedTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.CC.Stunned"))
        );
        
        // Block abilities
        UBlockAbilityTagsGameplayEffectComponent* BlockComp = 
            CreateDefaultSubobject<UBlockAbilityTagsGameplayEffectComponent>(TEXT("BlockAbilities"));
        BlockComp->InheritableBlockedAbilityTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Ability.Attack"))
        );
        BlockComp->InheritableBlockedAbilityTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Ability.Defend"))
        );
        BlockComp->InheritableBlockedAbilityTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Ability.Utility"))
        );
        
        // Cancel abilities
        UCancelAbilityTagsGameplayEffectComponent* CancelComp = 
            CreateDefaultSubobject<UCancelAbilityTagsGameplayEffectComponent>(TEXT("CancelAbilities"));
        CancelComp->CancelAbilitiesWithTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Ability.Channeled"))
        );
        
        // Application requirements
        UTargetTagRequirementsGameplayEffectComponent* RequirementsComp = 
            CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("Requirements"));
        RequirementsComp->ApplicationTagRequirements.RequireTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Alive"))
        );
        RequirementsComp->ApplicationTagRequirements.IgnoreTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Immune.CC"))
        );
    }
};
```

### Example 4: Immunity Effect

```cpp
UCLASS()
class UFireImmunityEffect : public UGameplayEffect
{
    GENERATED_BODY()
    
public:
    UFireImmunityEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Infinite;
        
        // Asset tags
        UAssetTagsGameplayEffectComponent* AssetTagsComp = 
            CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
        AssetTagsComp->InheritableAssetTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Immunity"))
        );
        
        // Granted tags
        UTargetTagsGameplayEffectComponent* TargetTagsComp = 
            CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
        TargetTagsComp->InheritableGrantedTagsContainer.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Immune.Fire"))
        );
        
        // Immunity component
        UImmunityGameplayEffectComponent* ImmunityComp = 
            CreateDefaultSubobject<UImmunityGameplayEffectComponent>(TEXT("Immunity"));
        ImmunityComp->ImmunityTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Fire"))
        );
        ImmunityComp->ImmunityTags.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Effect.CC.Burn"))
        );
        
        // Remove fire effects when applied
        URemoveOtherGameplayEffectComponent* RemoveComp = 
            CreateDefaultSubobject<URemoveOtherGameplayEffectComponent>(TEXT("RemoveEffects"));
        RemoveComp->RemoveGameplayEffectQuery = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
            FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Fire")))
        );
    }
};
```

---

## Tag Queries

### Example 1: Buffed OR Empowered, NOT Stunned

```cpp
bool CanActivateAbility(UAbilitySystemComponent* ASC)
{
    // Build buff tags
    FGameplayTagContainer BuffTags;
    BuffTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Buff.Strength")));
    BuffTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Buff.Empowered")));
    
    // Build blocked tags
    FGameplayTagContainer BlockedTags;
    BlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.CC.Stunned")));
    
    // Build query: (HasAny[Buffed, Empowered]) AND (NOT Stunned)
    FGameplayTagQuery Query = 
        FGameplayTagQuery::MakeQuery_MatchAnyTags(BuffTags)
            .And(FGameplayTagQuery::MakeQuery_MatchNoTags(BlockedTags));
    
    // Get owner tags
    FGameplayTagContainer OwnerTags;
    ASC->GetOwnedGameplayTags(OwnerTags);
    
    // Evaluate query
    return Query.Matches(OwnerTags);
}
```

### Example 2: Complex Nested Query

```cpp
bool CanApplyEffect(UAbilitySystemComponent* ASC)
{
    // Must have ALL state tags
    FGameplayTagContainer StateTags;
    StateTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive")));
    StateTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Conscious")));
    
    // Must have ANY vulnerability tag
    FGameplayTagContainer VulnerabilityTags;
    VulnerabilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Vulnerable.Fire")));
    VulnerabilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Vulnerable.Magic")));
    
    // Must NOT have ANY immunity tag
    FGameplayTagContainer ImmunityTags;
    ImmunityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Immune.Fire")));
    ImmunityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Immune.Magic")));
    
    // Build query: (HasAll[State]) AND (HasAny[Vulnerability]) AND (HasNone[Immunity])
    FGameplayTagQuery Query = 
        FGameplayTagQuery::MakeQuery_MatchAllTags(StateTags)
            .And(FGameplayTagQuery::MakeQuery_MatchAnyTags(VulnerabilityTags))
            .And(FGameplayTagQuery::MakeQuery_MatchNoTags(ImmunityTags));
    
    FGameplayTagContainer OwnerTags;
    ASC->GetOwnedGameplayTags(OwnerTags);
    
    return Query.Matches(OwnerTags);
}
```

### Example 3: Effect Query for Removal

```cpp
void RemoveFireEffects(UAbilitySystemComponent* ASC)
{
    // Build query for fire effects
    FGameplayTagContainer FireTags;
    FireTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Fire")));
    FireTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.CC.Burn")));
    
    // Create effect query
    FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FireTags);
    
    // Remove all matching effects
    ASC->RemoveActiveEffects(Query);
}
```

---

## Tag Events

### Example 1: Listen for Stun State

```cpp
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Listen for stun tag changes
        FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName("Status.CC.Stunned"));
        ASC->RegisterGameplayTagEvent(StunnedTag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &AMyCharacter::OnStunnedChanged);
    }
}

void AMyCharacter::OnStunnedChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Became stunned
        CancelAllAbilities();
        PlayAnimation(StunnedAnimation);
        DisableInput();
    }
    else
    {
        // No longer stunned
        PlayAnimation(IdleAnimation);
        EnableInput();
    }
}
```

### Example 2: Track Buff Count

```cpp
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Listen for any buff tag changes
        FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(FName("Status.Buff"));
        ASC->RegisterGameplayTagEvent(BuffTag, EGameplayTagEventType::AnyCountChange)
            .AddUObject(this, &AMyCharacter::OnBuffCountChanged);
    }
}

void AMyCharacter::OnBuffCountChanged(const FGameplayTag Tag, int32 NewCount)
{
    // Update UI with buff count
    if (UBuffWidget* BuffWidget = GetBuffWidget())
    {
        BuffWidget->SetBuffCount(NewCount);
    }
    
    // Visual feedback
    if (NewCount > 0)
    {
        ShowBuffParticles();
    }
    else
    {
        HideBuffParticles();
    }
}
```

### Example 3: Death State Machine

```cpp
void AMyCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Listen for death state changes
        FGameplayTag DyingTag = FGameplayTag::RequestGameplayTag(FName("Status.Death.Dying"));
        FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("Status.Death.Dead"));
        
        ASC->RegisterGameplayTagEvent(DyingTag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &AMyCharacter::OnDyingChanged);
        
        ASC->RegisterGameplayTagEvent(DeadTag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &AMyCharacter::OnDeadChanged);
    }
}

void AMyCharacter::OnDyingChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Started dying
        PlayAnimation(DeathAnimation);
        DisableInput();
        CancelAllAbilities();
    }
}

void AMyCharacter::OnDeadChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        // Fully dead
        SetActorEnableCollision(false);
        SetLifeSpan(5.0f);  // Destroy after 5 seconds
    }
}
```

---

## Tag-Based Ability Activation

### Example 1: Check Activation Requirements

```cpp
bool UMyGameplayAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    
    // Check if dead
    if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Status.Death"))))
    {
        if (OptionalRelevantTags)
        {
            OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.ActivateFail.IsDead")));
        }
        return false;
    }
    
    // Check if stunned
    if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Status.CC.Stunned"))))
    {
        if (OptionalRelevantTags)
        {
            OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.ActivateFail.TagsBlocked")));
        }
        return false;
    }
    
    // Check if has required buff
    FGameplayTagContainer RequiredBuffs;
    RequiredBuffs.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Buff.Empowered")));
    
    if (!ASC->HasAnyMatchingGameplayTags(RequiredBuffs))
    {
        if (OptionalRelevantTags)
        {
            OptionalRelevantTags->AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.ActivateFail.TagsMissing")));
        }
        return false;
    }
    
    return true;
}
```

### Example 2: Activate Abilities by Tag

```cpp
void AMyCharacter::ActivateAttackAbilities()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Build tag container for attack abilities
        FGameplayTagContainer AttackTags;
        AttackTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack")));
        
        // Try to activate all attack abilities
        ASC->TryActivateAbilitiesByTag(AttackTags);
    }
}

void AMyCharacter::CancelDefenseAbilities()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Build tag container for defense abilities
        FGameplayTagContainer DefenseTags;
        DefenseTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Defend")));
        
        // Cancel all defense abilities
        ASC->CancelAbilities(&DefenseTags);
    }
}
```

---

## Tag-Based Effect Application

### Example 1: Apply Damage with Tag Requirements

```cpp
void UMyDamageAbility::ApplyDamage(AActor* Target, float DamageAmount)
{
    if (!Target)
    {
        return;
    }
    
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC)
    {
        return;
    }
    
    // Check if target is alive
    if (!TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Status.Alive"))))
    {
        return;
    }
    
    // Check if target is immune
    if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Status.Immune.Physical"))))
    {
        return;
    }
    
    // Apply damage effect
    FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    
    FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
        DamageEffectClass,
        GetAbilityLevel(),
        EffectContext
    );
    
    if (SpecHandle.IsValid())
    {
        // Set damage magnitude
        SpecHandle.Data->SetSetByCallerMagnitude(
            FGameplayTag::RequestGameplayTag(FName("SetByCaller.Damage")),
            DamageAmount
        );
        
        // Apply effect
        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
}
```

### Example 2: Remove Effects by Tags

```cpp
void UMyAbility::RemoveDebuffs(AActor* Target)
{
    if (!Target)
    {
        return;
    }
    
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
    if (!TargetASC)
    {
        return;
    }
    
    // Build debuff tags
    FGameplayTagContainer DebuffTags;
    DebuffTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Status.Debuff")));
    
    // Remove all debuff effects
    int32 RemovedCount = TargetASC->RemoveActiveEffectsWithGrantedTags(DebuffTags);
    
    UE_LOG(LogTemp, Log, TEXT("Removed %d debuff effects"), RemovedCount);
}
```

---

## Complex Tag Patterns

### Pattern 1: Lyra Death System

```cpp
// Death state machine using hierarchical tags
void ALyraCharacter::HandleDeath()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Add dying tag
        ASC->AddLooseGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("Status.Death.Dying"))
        );
        
        // Cancel all abilities except those that survive death
        FGameplayTagContainer SurvivesDeathTag;
        SurvivesDeathTag.AddTag(
            FGameplayTag::RequestGameplayTag(FName("Ability.Behavior.SurvivesDeath"))
        );
        
        ASC->CancelAbilities(nullptr, &SurvivesDeathTag);
        
        // Play death animation
        PlayDeathAnimation();
        
        // After animation, add dead tag
        FTimerHandle DeadTimer;
        GetWorld()->GetTimerManager().SetTimer(DeadTimer, [this, ASC]()
        {
            ASC->RemoveLooseGameplayTag(
                FGameplayTag::RequestGameplayTag(FName("Status.Death.Dying"))
            );
            ASC->AddLooseGameplayTag(
                FGameplayTag::RequestGameplayTag(FName("Status.Death.Dead"))
            );
        }, DeathAnimationDuration, false);
    }
}
```

### Pattern 2: Movement Mode Tracking

```cpp
// Lyra pattern: Map movement modes to tags
void ALyraCharacter::OnMovementModeChanged(
    EMovementMode PrevMovementMode,
    uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Remove previous movement mode tag
        if (const FGameplayTag* PrevTag = LyraGameplayTags::MovementModeTagMap.Find(PrevMovementMode))
        {
            ASC->RemoveLooseGameplayTag(*PrevTag);
        }
        
        // Add new movement mode tag
        if (const FGameplayTag* NewTag = LyraGameplayTags::MovementModeTagMap.Find(GetCharacterMovement()->MovementMode))
        {
            ASC->AddLooseGameplayTag(*NewTag);
        }
    }
}
```

### Pattern 3: Initialization State Machine

```cpp
// Lyra pattern: Track initialization state with tags
void ALyraCharacter::InitializeAbilitySystem()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // 1. Spawned
        ASC->AddLooseGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("InitState.Spawned"))
        );
        
        // 2. Data available
        LoadCharacterData();
        ASC->AddLooseGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("InitState.DataAvailable"))
        );
        
        // 3. Data initialized
        InitializeAttributes();
        GrantAbilities();
        ASC->AddLooseGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("InitState.DataInitialized"))
        );
        
        // 4. Gameplay ready
        ASC->AddLooseGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("InitState.GameplayReady"))
        );
    }
}
```

### Pattern 4: Combo System with Tags

```cpp
// Track combo state with tags
void UComboAbility::ActivateAbility()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        // Check current combo state
        if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.First"))))
        {
            // Execute second combo
            ExecuteSecondCombo();
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.First")));
            ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.Second")));
        }
        else if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.Second"))))
        {
            // Execute third combo
            ExecuteThirdCombo();
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.Second")));
            ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.Third")));
        }
        else
        {
            // Execute first combo
            ExecuteFirstCombo();
            ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State.First")));
        }
        
        // Reset combo after timeout
        FTimerHandle ComboResetTimer;
        GetWorld()->GetTimerManager().SetTimer(ComboResetTimer, [ASC]()
        {
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Combo.State")));
        }, ComboResetTime, false);
    }
}
```

---

## Summary

**Key patterns from Lyra and NinjaGAS:**

1. **Hierarchical organization** — Use clear hierarchies (Ability.Attack.Melee, Status.CC.Stunned)
2. **Native tags for core systems** — Use UE_DEFINE_GAMEPLAY_TAG for frequently accessed tags
3. **.ini files for content** — Use GameplayTags.ini for designer-friendly tags
4. **Tag events over polling** — Use RegisterGameplayTagEvent instead of checking every frame
5. **State machines with tags** — Track complex state (death, initialization, combo) with tags
6. **Activation failure tags** — Provide clear feedback on why abilities fail
7. **Passive ability pattern** — Use "Ability.Passive" tag to auto-activate abilities
8. **Movement mode mapping** — Map engine enums to tags for GAS integration
9. **Effect queries** — Use tag queries to find and remove specific effects
10. **Immunity patterns** — Use granted tags + immunity component for damage immunity

**For KAIN implementation:**
- Support both native and .ini tag generation
- Provide clear syntax for tag hierarchies
- Generate tag event handlers from decorators
- Support complex tag queries with readable syntax
- Integrate seamlessly with ability/effect definitions

---

**References:**
- LyraGAS: `LyraGame/LyraGameplayTags.h/cpp`
- NinjaGAS: `NinjaGAS/Public/NinjaGASTags.h/cpp`
- ShooterCore: `Config/Tags/ShooterCoreTags.ini`
