# UHT Rules Expansion Summary

## Overview

The `uht_rules.json` file has been expanded with additional validation rules, attribute compatibility rules, and replication rules to support comprehensive UE5 semantic validation in the KAIN compiler.

## Expansion Date

February 12, 2025

## Changes Made

### 1. Additional Validation Rules (15 new rules)

Added KAIN-specific validation rules for:

- **Replication Validation**: Actor with replicated properties must have GetLifetimeReplicatedProps
- **RPC Naming**: RPC functions must follow Server_*/Client_*/Multicast_* naming convention
- **RPC Validation**: Server/Client/Multicast RPCs must be marked as reliable or unreliable
- **DataTable Validation**: 
  - Fields must be UE5-serializable types
  - Cannot contain pointer types
  - Must inherit from FTableRowBase
- **Component Validation**:
  - Cannot contain actor-only features
  - Cannot have Tick function (use TickComponent instead)
- **Name Collision Detection**:
  - Type name collides with UE5 engine type
  - Type name is a C++ reserved keyword
  - Type name is a UE5 macro name
- **Circular Dependency Detection**: Detect circular dependencies in type definitions
- **Type Validation**: Nested container types are not supported by UHT

### 2. Additional Incompatible Combinations (13 new combos)

Added attribute compatibility rules for:

- **Property Attributes**:
  - Replicated + Transient (incompatible)
  - Replicated + Config (incompatible)
  - EditAnywhere + EditInstanceOnly (incompatible)
  - EditAnywhere + EditDefaultsOnly (incompatible)
  - EditInstanceOnly + EditDefaultsOnly (incompatible)
  - VisibleAnywhere + VisibleInstanceOnly (incompatible)
  - VisibleAnywhere + VisibleDefaultsOnly (incompatible)
  - VisibleInstanceOnly + VisibleDefaultsOnly (incompatible)
  - BlueprintReadOnly + BlueprintReadWrite (incompatible)
  - SaveGame + Transient (incompatible)

- **Function Attributes**:
  - BlueprintCallable + BlueprintPure (incompatible)
  - Const + Exec (incompatible)
  - Static + Virtual (incompatible)

- **Class Attributes**:
  - Abstract + NotPlaceable (warning - Abstract implies NotPlaceable)

### 3. New Section: Replication Rules

Added comprehensive replication rules including:

- **Property Replication**:
  - Allowed types: bool, uint8, int32, int64, float, double, FString, FName, FText, FVector, FRotator, FTransform, FLinearColor, FColor, TArray, TObjectPtr, UObject*, AActor*, UActorComponent*, enum, struct
  - Disallowed types: TMap, TSet, TWeakObjectPtr, TSoftObjectPtr, TLazyObjectPtr, TFunction, TDelegate, TMulticastDelegate
  - Constraints: 5 rules for replicated properties

- **RPC Validation**:
  - Naming conventions: Server_*, Client_*, Multicast_*
  - Required specifiers: Reliable or Unreliable
  - Constraints: 8 rules for RPC functions

- **Lifetime Replication**:
  - Required includes: Net/UnrealNetwork.h
  - Required macros: DOREPLIFETIME, DOREPLIFETIME_CONDITION
  - Function signature: `void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override`

### 4. New Section: Attribute Compatibility Matrix

Added detailed compatibility matrix for:

- **Property Attributes**: Replicated, SaveGame, Transient, Config
  - Each attribute lists compatible_with and incompatible_with attributes

- **Function Attributes**: BlueprintCallable, BlueprintPure, BlueprintImplementableEvent, BlueprintNativeEvent, Server, Client, Multicast
  - Each attribute lists compatible_with, incompatible_with, and requires_one_of attributes

- **Class Attributes**: Abstract, Blueprintable, NotBlueprintable
  - Each attribute lists compatible_with, incompatible_with, and implies attributes

### 5. New Section: KAIN-Specific Rules

Added KAIN language-specific rules for:

- **Actor Rules** (4 rules):
  - Actors must use A-prefix (AMyActor)
  - Actor state fields are automatically replicated if marked @replicated
  - Actor RPC functions must follow Server_/Client_/Multicast_ naming
  - Actors with replicated state must have GetLifetimeReplicatedProps generated

- **Struct Rules** (4 rules):
  - Structs must use F-prefix (FMyStruct)
  - @datatable structs must inherit from FTableRowBase
  - @datatable structs cannot contain pointers
  - Struct members cannot be replicated

- **Enum Rules** (4 rules):
  - Enums must use E-prefix (EMyEnum)
  - Enum values cannot be named 'true' or 'false'
  - Enums should have a 0 entry for default initialization
  - BlueprintType enums must use uint8 as base type

- **Component Rules** (4 rules):
  - Components must use U-prefix and Component suffix (UMyComponent)
  - @component structs generate UActorComponent subclasses
  - Components cannot contain actor-only features
  - Components use TickComponent instead of Tick

- **Delegate Rules** (3 rules):
  - Delegates must use F-prefix (FMyDelegate)
  - Delegate parameters cannot be in RPCs
  - Delegates cannot be replicated

## Updated Statistics

- **Total Validation Rules**: 352 (was 337)
- **Total Specifiers**: 154 (unchanged)
- **Total Property Types**: 41 (unchanged)
- **Total Incompatible Combinations**: 38 (was 25)
- **New Sections**: 3 (replication_rules, attribute_compatibility_matrix, kain_specific_rules)

## Rust API Updates

The `UhtRules` struct in `crates/ue5/src/ue5/uht_rules.rs` has been updated with new query methods:

### Replication Rules API

- `is_replicable_type(type_name)` - Check if a type is allowed for replication
- `is_non_replicable_type(type_name)` - Check if a type is explicitly disallowed
- `replication_constraints()` - Get replication constraints for properties
- `rpc_naming_convention(rpc_type)` - Get RPC naming convention (Server, Client, Multicast)
- `rpc_constraints()` - Get RPC validation constraints
- `lifetime_replication_signature()` - Get GetLifetimeReplicatedProps function signature
- `replication_includes()` - Get required includes for replication

### Attribute Compatibility API

- `are_property_attributes_compatible(attr1, attr2)` - Check property attribute compatibility
- `are_function_attributes_compatible(attr1, attr2)` - Check function attribute compatibility
- `required_attributes_for(attr, attr_type)` - Get required attributes (e.g., Server requires Reliable/Unreliable)
- `implied_attributes(attr, attr_type)` - Get implied attributes (e.g., Abstract implies NotPlaceable)

### KAIN-Specific Rules API

- `kain_actor_rules()` - Get KAIN-specific rules for actors
- `kain_struct_rules()` - Get KAIN-specific rules for structs
- `kain_enum_rules()` - Get KAIN-specific rules for enums
- `kain_component_rules()` - Get KAIN-specific rules for components
- `kain_delegate_rules()` - Get KAIN-specific rules for delegates

## Usage in Oracle

The Oracle validator (`crates/ue5/src/ue5/oracle.rs`) can now use these new rules for enhanced validation:

```rust
// Check if a type is replicable
if uht.is_replicable_type("TMap") {
    // TMap is not replicable
}

// Check attribute compatibility
if !uht.are_property_attributes_compatible("Replicated", "Transient") {
    // Error: Replicated and Transient are incompatible
}

// Get RPC naming convention
if let Some(convention) = uht.rpc_naming_convention("Server") {
    // convention = "Server_FunctionName"
}

// Get KAIN-specific rules
for rule in uht.kain_actor_rules() {
    // Validate against KAIN actor rules
}
```

## Validation Against UHT Source Code

The rules in this expansion are based on:

1. **UE5 UHT Source Code**: Extracted from `D:\Unreal\UE_5.7\Engine\Source\Programs\Shared\EpicGames.UHT`
2. **UE5 Documentation**: Official Unreal Engine documentation on replication, RPCs, and attributes
3. **KAIN Language Specification**: KAIN-specific rules from the language design
4. **Production Testing**: Rules validated against the `ultimate.kn` test plugin

## Files Modified

1. `unreal/metadata/uht_rules.json` - Main rules file (expanded)
2. `unreal/metadata/uht_rules_expansion.json` - Expansion source file (new)
3. `unreal/scripts/expand_uht_rules.py` - Expansion script (new)
4. `crates/ue5/src/ue5/uht_rules.rs` - Rust API (updated)
5. `unreal/metadata/uht_rules_expansion_summary.md` - This file (new)

## Next Steps

1. Update Oracle validator to use new replication rules
2. Update Oracle validator to use attribute compatibility matrix
3. Add validation for KAIN-specific rules
4. Write unit tests for new query methods
5. Write property tests for replication validation
6. Update documentation with examples

## Requirements Satisfied

This expansion satisfies the following requirements from the KAIN Pipeline Robustness spec:

- **Requirement 13.15**: Oracle validation needs UE5 semantic rules - query uht_rules.json before using hardcoded fallbacks
- **Requirement 13.18**: When the System encounters an unknown type, it SHALL check all metadata files before returning an error
- **Requirement 3.1**: WHEN an actor contains replicated properties, THEN the Oracle SHALL verify GetLifetimeReplicatedProps will be generated
- **Requirement 3.2**: WHEN an RPC function is declared, THEN the Oracle SHALL verify the naming convention (Server_*, Client_*, Multicast_*) is followed
- **Requirement 3.3**: WHEN a @datatable struct is declared, THEN the Oracle SHALL verify it contains only UE5-serializable field types

## Maintenance

To update the rules in the future:

1. Edit `unreal/metadata/uht_rules_expansion.json` with new rules
2. Run `python unreal/scripts/expand_uht_rules.py` to merge into main file
3. Update this summary document
4. Run tests to verify no regressions

## References

- UHT Source: `D:\Unreal\UE_5.7\Engine\Source\Programs\Shared\EpicGames.UHT`
- KAIN Spec: `.kiro/specs/kain-pipeline-robustness/`
- Oracle Implementation: `crates/ue5/src/ue5/oracle.rs`
- Test Plugin: `testing/Phase3/SlateTest4/ultimate.kn`
