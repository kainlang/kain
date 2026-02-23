# UE5 Documentation Extraction Pipeline

This pipeline extracts structured type and function data from scraped UE5 HTML documentation files and converts them into hierarchical JSON for KAIN Oracle integration.

## Why This Matters

The KAIN compiler's Oracle validation system needs to know about ALL UE5 engine types to prevent name collisions. Currently, `engine_knowledge.json` has ~500 types. The official UE5 docs contain **15,000+ types** in the Blueprint API.

This extraction pipeline will:
1. Parse 35,503 HTML files from official UE5 Blueprint API documentation
2. Extract all type names (AActor, UComponent, FStruct, EEnum, IInterface)
3. Extract function signatures with parameters and return types
4. Generate `engine_knowledge_expansion.json` ready for Oracle merge
5. Provide hierarchical JSON for fast LLM lookups

## Blueprint API vs C++ API

### Blueprint API (RECOMMENDED) ✅
- **35,503 HTML files** at `M:/Code/Research/OfficialDocs/BlueprintAPI/`
- **Blueprint-exposed types only** - What KAIN users will actually name their types after
- **~15,000 types** - The "public API" that Epic officially supports
- **Perfect for KAIN Oracle** - Catches all realistic name collisions

### C++ API (Optional, Not Recommended) ❌
- **~50,000+ HTML files** at `M:/Code/Kain/unreal/UE_API/`
- **ALL engine types** - Including internal implementation details
- **~20,000+ types** - Too much noise (internal classes users will never collide with)
- **Overkill for KAIN** - Adds complexity without benefit

**Recommendation**: Use Blueprint API only. It's what KAIN users will reference.

## Prerequisites

```bash
pip install beautifulsoup4 lxml
```

## Directory Structure

```
M:/Code/Research/OfficialDocs/BlueprintAPI/  # 35,503 Blueprint API HTML files (~2-3 GB)
M:/Code/Kain/unreal/doc_extractor/           # This extraction script
M:/Code/Kain/unreal/extracted_docs/          # Output directory (created by script)
```

## Quick Start (Easiest)

Just double-click the batch file in Windows Explorer:

```
Kain/unreal/doc_extractor/run_extraction_blueprint.bat
```

This will:
1. Process all 35,503 Blueprint API HTML files
2. Extract 15,000+ types and 20,000+ functions
3. Generate clean JSON files in `extracted_docs/`
4. Take ~2 minutes with 16 workers

## Manual Usage

### Step 1: Extract Blueprint API (Recommended)

```bash
cd M:/Code/Kain/unreal/doc_extractor

python extract_ue5_docs.py \
  --input M:/Code/Research/OfficialDocs/BlueprintAPI \
  --output ../extracted_docs \
  --workers 16 \
  --api blueprint
```

**Expected output:**
- `blueprint_api_index.json` - Master index with counts
- `types/actors.json` - All AActor types
- `types/components.json` - All UActorComponent types
- `types/structs.json` - All FStruct types
- `types/enums.json` - All EEnum types
- `types/interfaces.json` - All IInterface types
- `functions/by_category.json` - Functions grouped by category
- `functions/by_module.json` - Functions grouped by module
- `metadata/engine_knowledge_expansion_blueprint.json` - Ready for Oracle merge

### Step 2: Extract C++ API (Optional)

```bash
python extract_ue5_docs.py \
  --input M:/Code/Kain/unreal/UE_API \
  --output ../extracted_docs \
  --workers 16 \
  --api cpp
```

**Expected output:**
- `cpp_api_index.json` - Master index with counts
- Same hierarchical structure as Blueprint API
- `metadata/engine_knowledge_expansion_cpp.json` - Ready for Oracle merge

### Step 3: Dry Run (Count Files Only)

```bash
python extract_ue5_docs.py \
  --input M:/Code/Research/OfficialDocs/BlueprintAPI \
  --output ../extracted_docs \
  --dry-run
```

## Output Structure

```
Kain/unreal/extracted_docs/
├── blueprint_api_index.json          # Master index (Blueprint API)
├── cpp_api_index.json                # Master index (C++ API)
├── types/
│   ├── actors.json                   # All AActor types (~5-10MB)
│   ├── components.json               # All UActorComponent types
│   ├── structs.json                  # All USTRUCT types
│   ├── enums.json                    # All UENUM types
│   ├── interfaces.json               # All UInterface types
│   └── templates.json                # All template types (C++ only)
├── functions/
│   ├── by_category.json              # Grouped by category
│   └── by_module.json                # Grouped by module
└── metadata/
    ├── engine_knowledge_expansion_blueprint.json  # Ready for KAIN Oracle
    └── engine_knowledge_expansion_cpp.json        # Ready for KAIN Oracle
```

## Performance

- **Workers**: 16 parallel processes (adjust based on CPU cores)
- **Speed**: ~1000 files/second on modern hardware
- **Memory**: ~2GB peak (processes one file at a time)
- **Time**: ~2-3 minutes for 150,000 files

## What Gets Extracted

### Types (Classes, Structs, Enums, Interfaces)

```json
{
  "name": "AActor",
  "type_category": "Actor",
  "module": "Runtime/Engine",
  "category": "Gameplay",
  "description": "Actor is the base class for an Object that can be placed or spawned in a level.",
  "parent_class": "UObject",
  "ue_versions": ["4.26", "4.27", "5.0", "5.1", "5.2", "5.3"],
  "blueprint_type": true,
  "blueprint_spawnable": true,
  "meta_tags": {},
  "file_path": "Runtime/Engine/GameFramework/AActor/index.html"
}
```

### Functions

```json
{
  "name": "GetActorLocation",
  "category": "Transformation",
  "module": "Runtime/Engine",
  "description": "Returns the location of the RootComponent of this Actor",
  "parameters": [],
  "return_type": "Vector",
  "ue_versions": ["4.26", "4.27", "5.0", "5.1", "5.2", "5.3"],
  "blueprint_callable": true,
  "blueprint_pure": true,
  "meta_tags": {},
  "file_path": "BlueprintAPI/Transformation/GetActorLocation/index.html"
}
```

## Integration with KAIN Oracle

### Step 1: Review Extracted Data

```bash
# Check master index
cat Kain/unreal/extracted_docs/blueprint_api_index.json

# Check actors
cat Kain/unreal/extracted_docs/types/actors.json | jq '.[] | .name' | head -20

# Check structs
cat Kain/unreal/extracted_docs/types/structs.json | jq '.[] | .name' | head -20
```

### Step 2: Merge into engine_knowledge.json

```bash
# Backup current engine_knowledge.json
cp Kain/unreal/metadata/engine_knowledge.json Kain/unreal/metadata/engine_knowledge.json.bak

# Merge expansion files (manual or scripted)
# TODO: Create merge script
```

### Step 3: Update KAIN Oracle

The Oracle validation system in `Kain/crates/kain-core/src/oracle.rs` will automatically load the expanded `engine_knowledge.json` and use it for name collision detection.

## Troubleshooting

### Issue: "BeautifulSoup4 not installed"

```bash
pip install beautifulsoup4 lxml
```

### Issue: "Permission denied" or file locks

- Close any programs that might have HTML files open
- Run from a terminal with appropriate permissions

### Issue: Extraction is slow

- Increase `--workers` (try 16, 24, or 32 based on CPU cores)
- Use an SSD for input/output directories
- Disable antivirus scanning on the directories temporarily

### Issue: Out of memory

- Reduce `--workers` to 4 or 8
- Process Blueprint API and C++ API separately
- Close other applications

## Expected Results

### Blueprint API (150,000 files)

- **Types**: ~8,000-10,000 (AActor, UComponent, FStruct, EEnum, IInterface)
- **Functions**: ~15,000-20,000 (Blueprint nodes)
- **Processing time**: 2-3 minutes with 16 workers

### C++ API (if available)

- **Types**: ~15,000-20,000 (all engine classes)
- **Functions**: ~50,000+ (all engine functions)
- **Processing time**: 5-10 minutes with 16 workers

## Next Steps After Extraction

1. **Review extracted data** - Check `blueprint_api_index.json` for counts
2. **Validate type names** - Ensure proper A/U/F/E/I prefixes
3. **Merge into Oracle** - Update `engine_knowledge.json`
4. **Test Oracle** - Run KAIN compilation on Factory plugins
5. **Verify collision detection** - Ensure Oracle catches name collisions

## Why Hierarchical JSON?

### Advantages over One Giant File:

1. **Fast targeted lookups** - Load only what you need (e.g., just `actors.json`)
2. **LLM-friendly** - Smaller context windows, focused queries
3. **Maintainable** - Easy to update individual categories
4. **Git-friendly** - Smaller diffs when updating
5. **Parallel processing** - Multiple LLMs can query different files simultaneously
6. **Memory efficient** - Don't load 50MB of data when you only need 5MB

### Example LLM Query:

```python
# Load only actors for collision detection
with open('extracted_docs/types/actors.json') as f:
    actors = json.load(f)
    actor_names = [a['name'] for a in actors]
    
# Check if user's actor name collides
if user_actor_name in actor_names:
    print(f"ERROR: Actor '{user_actor_name}' collides with engine type")
```

## Files in This Directory

- `extract_ue5_docs.py` - Main extraction script (567 lines)
- `README.md` - This file
- `merge_engine_knowledge.py` - TODO: Script to merge expansion files into engine_knowledge.json

## Related Documentation

- `Kain/unreal/metadata/engine_knowledge.json` - Current Oracle type database (~500 types)
- `Kain/crates/kain-core/src/oracle.rs` - Oracle validation system
- `.kiro/specs/factory-plugin-compilation-failures/` - Bugfix spec that motivated this extraction


## Cleanup After Extraction

Once you've verified the extracted JSON is correct, you can delete the HTML dumps to free up **~7-13 GB** of disk space:

### Option 1: Use the Cleanup Script (Easiest)

Just double-click:
```
Kain/unreal/doc_extractor/cleanup_html_dumps.bat
```

This will delete:
- `M:/Code/Research/OfficialDocs/BlueprintAPI/` (~2-3 GB)
- `M:/Code/Kain/unreal/UE_API/` (~5-10 GB)

### Option 2: Manual Cleanup

```bash
# Delete Blueprint API HTML files
rmdir /s /q "M:\Code\Research\OfficialDocs\BlueprintAPI"

# Delete C++ API HTML files (if you have them)
rmdir /s /q "M:\Code\Kain\unreal\UE_API"
```

### Before You Delete - Verify Extraction

Make sure these files exist and look correct:

1. **Master index**: `Kain/unreal/extracted_docs/blueprint_api_index.json`
   - Should show ~15,000 types and ~20,000 functions

2. **Oracle expansion**: `Kain/unreal/extracted_docs/metadata/engine_knowledge_expansion_blueprint.json`
   - Should have arrays of type names ready for Oracle merge

3. **Type files**: `Kain/unreal/extracted_docs/types/*.json`
   - actors.json, components.json, structs.json, enums.json, etc.

Once verified, the HTML files are just taking up disk space and can be safely deleted!

## Files in This Directory

- `extract_ue5_docs.py` - Main extraction script (485 lines)
- `run_extraction_blueprint.bat` - Easy Blueprint API extraction
- `run_extraction_cpp.bat` - Optional C++ API extraction (not recommended)
- `run_dry_run.bat` - Count files without processing
- `cleanup_html_dumps.bat` - Delete HTML dumps after extraction
- `README.md` - This file
