# Blueprint API vs C++ API - Quick Reference

## TL;DR

**Use Blueprint API only. Delete both HTML dumps after extraction.**

## Comparison

| Feature | Blueprint API | C++ API |
|---------|--------------|---------|
| **Files** | 35,503 HTML files | ~50,000+ HTML files |
| **Size** | ~2-3 GB | ~5-10 GB |
| **Types** | ~15,000 types | ~20,000+ types |
| **Coverage** | Blueprint-exposed only | ALL engine types |
| **For KAIN** | ✅ Perfect | ❌ Overkill |
| **Location** | `M:/Code/Research/OfficialDocs/BlueprintAPI/` | `M:/Code/Kain/unreal/UE_API/` |

## Why Blueprint API is Better for KAIN

### 1. It's What Users Will Reference
KAIN users write Blueprint-style code. They'll name their types after what they see in Blueprint documentation, not internal C++ classes.

**Example collisions Blueprint API catches:**
- `AActor` - User tries to create `actor Player` → collision
- `UActorComponent` - User tries to create `@component Health` → collision  
- `FVector` - User tries to create `struct Vector` → collision
- `ECollisionChannel` - User tries to create `enum CollisionChannel` → collision

### 2. C++ API Has Too Much Noise

The C++ API includes internal engine classes that no KAIN user will ever collide with:

**Examples of C++ API noise:**
- `FRenderThreadCommandQueue` - Internal rendering class
- `FAsyncPackageDesc2` - Internal package loading
- `FMallocBinned2` - Internal memory allocator
- `FD3D12CommandContext` - Internal DirectX 12 implementation

**No KAIN user will ever name their type `FRenderThreadCommandQueue`!**

### 3. Blueprint API is the "Public API"

Epic Games officially supports and documents the Blueprint API. These are the types they want developers to use. The C++ API includes:
- Deprecated classes
- Internal implementation details
- Platform-specific code
- Debug-only classes

### 4. Smaller = Faster Oracle Validation

- **Blueprint API**: 15,000 types → faster name collision checks
- **C++ API**: 20,000+ types → slower checks with no benefit

## What You Get from Blueprint API

### Types Extracted: 15,359
- **1,088 Actors** (AActor subclasses)
- **936 Components** (UActorComponent subclasses)
- **1,279 Structs** (FStruct types)
- **354 Enums** (EEnum types)
- **1,299 Interfaces** (IInterface types)
- **142 Objects** (UObject subclasses)
- **10,258 Unknown** (needs classification refinement)

### Functions Extracted: 20,144
- All Blueprint-callable functions
- With parameters and return types
- Grouped by 600+ categories

## Cleanup After Extraction

Once you've extracted the JSON, **delete both HTML dumps** to free up ~7-13 GB:

```bash
# Use the cleanup script
Kain/unreal/doc_extractor/cleanup_html_dumps.bat
```

This deletes:
1. `M:/Code/Research/OfficialDocs/BlueprintAPI/` (~2-3 GB)
2. `M:/Code/Kain/unreal/UE_API/` (~5-10 GB)

## What to Keep

Keep the extracted JSON files at:
```
Kain/unreal/extracted_docs/
├── blueprint_api_index.json          # Master index
├── types/                            # Type files (actors, structs, enums, etc.)
├── functions/                        # Function files
└── metadata/
    └── engine_knowledge_expansion_blueprint.json  # For Oracle merge
```

**Total size**: ~2.5 MB (vs ~7-13 GB of HTML!)

## Next Steps

1. ✅ Extract Blueprint API (done)
2. ✅ Verify JSON files look correct
3. ⏭️ Delete HTML dumps (run `cleanup_html_dumps.bat`)
4. ⏭️ Merge `engine_knowledge_expansion_blueprint.json` into Oracle
5. ⏭️ Test on Factory plugins

---

**Bottom line**: Blueprint API gives you everything you need. C++ API is overkill. Delete both HTML dumps after extraction to save ~10 GB of disk space!
