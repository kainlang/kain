# UE 5.3–5.7 Versioning Upgrade

**Date:** 2026-02-19  
**Status:** ✅ Complete

---

## What Was Done

Extended the vendored `unreal_asset` library and KAIN's central version authority
(`KainEngineTarget`) to support Unreal Engine 5.3 through 5.7 with **correct,
real `ObjectVersionUE5` watermarks** extracted directly from local engine installs.

### Data Source (Ground Truth)

Watermarks read directly from `Engine/Source/Runtime/Core/Public/UObject/ObjectVersion.h`
in each locally installed engine version:

| UE Version | Source Install | Last `ObjectVersionUE5` Variant Added |
|---|---|---|
| 5.2 (baseline) | vendored | `DATA_RESOURCES` (= 1010) |
| **5.3** | — | _(none — 5.3 added zero new global variants)_ |
| **5.4** | `d:\Unreal\UE_5.4` | `PROPERTY_TAG_COMPLETE_TYPE_NAME` (= 1013) |
| **5.5** | `m:\UnrealEngine\UE\UE_5.5` | `ASSETREGISTRY_PACKAGEBUILDDEPENDENCIES` (= 1014) |
| **5.6** | `m:\UnrealEngine\UE\UE_5.6` | `OS_SUB_OBJECT_SHADOW_SERIALIZATION` (= 1018) |
| **5.7** | `d:\Unreal\UE_5.7` | `IMPORT_TYPE_HIERARCHIES` (= 1019) |

---

## Files Changed

### 1. `crates/unreal/unreal_asset_base/src/object_version.rs`
Added 9 new `ObjectVersionUE5` enum variants in order, with UE-version section headers:

**UE 5.5:**
- `SCRIPT_SERIALIZATION_OFFSET`
- `PROPERTY_TAG_EXTENSION_AND_OVERRIDABLE_SERIALIZATION`
- `PROPERTY_TAG_COMPLETE_TYPE_NAME`
- `ASSETREGISTRY_PACKAGEBUILDDEPENDENCIES`

**UE 5.6:**
- `METADATA_SERIALIZATION_OFFSET`
- `VERSE_CELLS`
- `PACKAGE_SAVED_HASH`
- `OS_SUB_OBJECT_SHADOW_SERIALIZATION`

**UE 5.7:**
- `IMPORT_TYPE_HIERARCHIES`

### 2. `crates/unreal/unreal_asset_base/src/engine_version.rs`
Updated `OBJECT_VERSION_TO_ENGINE_VERSION_UE5` lookup table with real watermarks:
```
VER_UE5_2  → AUTOMATIC_VERSION    (DATA_RESOURCES ceiling, same as before)
VER_UE5_3  → DATA_RESOURCES       (5.3 genuinely has the same watermark as 5.2)
VER_UE5_4  → PROPERTY_TAG_COMPLETE_TYPE_NAME
VER_UE5_5  → ASSETREGISTRY_PACKAGEBUILDDEPENDENCIES
VER_UE5_6  → OS_SUB_OBJECT_SHADOW_SERIALIZATION
VER_UE5_7  → IMPORT_TYPE_HIERARCHIES
```

### 3. `crates/ue5-asset-utils/src/engine_target.rs`
Updated `KainEngineTarget::as_serializer_version()` dispatch:
```rust
Ue5_0 → VER_UE5_0
Ue5_1 → VER_UE5_1
Ue5_2 → VER_UE5_2
Ue5_3 → VER_UE5_2   // special case: no new watermark
Ue5_4 → VER_UE5_4   // now unlocked ✅
Ue5_5 → VER_UE5_5   // now unlocked ✅
Ue5_6 → VER_UE5_6   // now unlocked ✅
Ue5_7 → VER_UE5_7   // now unlocked ✅
```

Also updated `serializer_ceiling()` and `is_above_serializer_ceiling()` to reflect
that only `Ue5_3` is now "above its ceiling" (the unique case where a version has
no new binary format).

---

## Proof of Success

```
cargo test -p ue5-asset-utils -p unreal_asset_base
```

```
test engine_target::tests::test_all_versions_map_to_valid_serializer_version ... ok
test engine_target::tests::test_ue5_0_maps_distinctly ... ok
test engine_target::tests::test_ue5_3_maps_to_ue5_2_format ... ok
test engine_target::tests::test_ue5_4_through_5_7_have_native_formats ... ok
test engine_target::tests::test_is_above_serializer_ceiling ... ok
test engine_target::tests::test_round_trip_str ... ok
test engine_target::tests::test_default_is_stable_version ... ok
test engine_target::tests::test_serializer_ceiling ... ok

test result: ok. 8 passed; 0 failed
```

Note: `cargo build --workspace` has 2 expected errors in `cli` due to in-progress
material system expansion (`is_dynamic` / `expose_parameters` fields). Unrelated
to versioning work.

---

## Compatibility Matrix (Post-Upgrade)

| Generated with | Loads in |
|---|---|
| `Ue5_2` / `Ue5_3` | UE 5.2, 5.3, 5.4, 5.5, 5.6, 5.7 |
| `Ue5_4` | UE 5.4, 5.5, 5.6, 5.7 |
| `Ue5_5` | UE 5.5, 5.6, 5.7 |
| `Ue5_6` | UE 5.6, 5.7 |
| `Ue5_7` | UE 5.7 only |

---

## Architecture Notes

### Clean Decoupling ✅
- All external code works with `KainEngineTarget` — zero uses of `EngineVersion` leak out
- `as_serializer_version()` is the **single firewall** between KAIN semantics and binary format
- Adding UE 5.8 in the future = 3 files, ~10 lines of code

### No Technical Debt Introduced
- `VER_UE5_3` through `VER_UE5_7` variants already existed in `engine_version.rs`
  from the previous session; only the UE5 watermark table was placeholder
- All placeholders have now been replaced with real values from engine source

### Future Upgrade Process (UE 5.8+)
1. Find `ObjectVersion.h` in the new engine install
2. Add new `ObjectVersionUE5` variants to `object_version.rs`
3. Add `VER_UE5_8` to `engine_version.rs` UE5 table with the new watermark
4. Add `Ue5_8` arm to `KainEngineTarget::as_serializer_version()`
5. `cargo test -p ue5-asset-utils`
