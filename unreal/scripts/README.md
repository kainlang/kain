# UE5 Metadata Extraction Scripts

This directory contains Python scripts for extracting UE5 engine knowledge into JSON metadata files consumed by the KAIN compiler.

## Overview

The KAIN compiler uses a **metadata-first architecture** where all UE5 knowledge is loaded from JSON files rather than hardcoded. This enables:

- Zero-recompilation updates for new UE5 versions
- Multi-version UE5 support (5.4, 5.5, 5.6, 5.7)
- Multi-drive installation support (C:, D:, M:, etc.)
- Data-driven validation rules
- LLM-friendly knowledge base

## Configuration

### ue5_paths_config.json

This file defines all UE5 installation paths across multiple drives and versions:

```json
{
  "installations": [
    {
      "version": "5.4",
      "paths": [
        "C:/Program Files/Epic Games/UE_5.4/Engine/Source",
        "D:/UE_5.4/Engine/Source",
        "M:/UnrealEngine/UE_5.4/Engine/Source"
      ],
      "enabled": true
    }
  ],
  "output_directory": "../metadata",
  "output_filename_template": "engine_{version}_scanned.json"
}
```

**How it works:**
- The scripts try each path in order until they find a valid UE5 installation
- You can disable versions by setting `"enabled": false`
- Add your custom installation paths to the `paths` array
- The first valid path found for each version is used

## Scripts

### 1. ue5_scanner.py

Scans UE5 headers and extracts:
- Classes (UCLASS) with inheritance, functions, properties
- Structs (USTRUCT) with fields
- Enums (UENUM) with values
- Include paths and module mappings

**Usage:**
```bash
# Single installation
python ue5_scanner.py "D:/UE_5.7/Engine/Source" ../metadata/engine_5.7_scanned.json

# All configured installations
python ue5_scanner.py --config ue5_paths_config.json

# Legacy flat format (backward compat)
python ue5_scanner.py --legacy "D:/UE_5.7/Engine/Source" ../metadata/legacy_scan.json
```

**Output:** `engine_{version}_scanned.json` files

### 2. module_graph_extractor.py

Scans .Build.cs files and extracts:
- Module names and categories (Runtime, Editor, Developer, etc.)
- Public and private dependencies
- Transitive dependency closure
- Type → module mappings
- Header → module mappings
- API symbol → module mappings

**Usage:**
```bash
# Single installation
python module_graph_extractor.py "D:/UE_5.7/Engine/Source" --engine-scan ../metadata/engine_5.7_scanned.json

# All configured installations
python module_graph_extractor.py --config ue5_paths_config.json
```

**Output:** `module_graph_{version}.json` files

### 3. uht_extractor.py

Extracts Unreal Header Tool validation rules from UHT source code.

**Usage:**
```bash
python uht_extractor.py "D:/UE_5.7/Engine/Source/Programs/UnrealHeaderTool"
```

**Output:** `uht_rules.json`

### 4. shader_extractor.py

Extracts HLSL type information and shader validation rules.

**Usage:**
```bash
python shader_extractor.py "D:/UE_5.7/Engine/Shaders"
```

**Output:** `shader_knowledge.json`

### 5. editor_attributes_extractor.py

Extracts editor attribute definitions (@slider, @color_picker, etc.) from Slate and PropertyEditor source.

**Usage:**
```bash
python editor_attributes_extractor.py "D:/UE_5.7/Engine/Source"
```

**Output:** `editor_attributes.json`

### 6. virtual_obligations_extractor.py

Extracts virtual function requirements for UE5 classes (which virtuals must be overridden).

**Usage:**
```bash
python virtual_obligations_extractor.py "D:/UE_5.7/Engine/Source"
```

**Output:** `virtual_obligations.json`

### 7. verify_scan.py

Validates metadata completeness and consistency.

**Usage:**
```bash
python verify_scan.py
```

**Checks:**
- All required metadata files exist
- JSON is valid and parseable
- Cross-references are consistent
- No missing types or modules

### 8. corpus_extractor.py

Extracts code corpus from .kn files for analysis and testing.

**Usage:**
```bash
python corpus_extractor.py ../kn_library
```

**Output:** `kain_corpus.json`

## Quick Start

> **📖 For detailed workflow documentation, see [METADATA_REFRESH_WORKFLOW.md](METADATA_REFRESH_WORKFLOW.md)**

### Option 1: Automated Refresh (Recommended)

**Core metadata (fast, 3-5 minutes):**
```bash
cd unreal/scripts
refresh_all_metadata.bat        # Windows
./refresh_all_metadata.sh       # Linux/Mac
```

**Full metadata (complete, 5-10 minutes):**
```bash
cd unreal/scripts
refresh_all_metadata_full.bat   # Windows
./refresh_all_metadata_full.sh  # Linux/Mac
```

This will:
1. Scan all configured UE5 installations
2. Extract module dependency graphs
3. Extract optional metadata (UHT rules, shaders, etc.) - full only
4. Verify metadata completeness
5. Report any issues

### Option 2: Verify Configuration First

Before running extraction, verify your config:
```bash
cd unreal/scripts
python verify_config.py
```

This checks:
- Config file is valid JSON
- At least one UE5 installation is accessible
- Output directory is writable

### Option 3: Manual Extraction

1. **Edit config file:**
   ```bash
   cd unreal/scripts
   # Edit ue5_paths_config.json with your UE5 installation paths
   ```

2. **Run scanner:**
   ```bash
   python ue5_scanner.py --config ue5_paths_config.json
   ```

3. **Extract module graphs:**
   ```bash
   python module_graph_extractor.py --config ue5_paths_config.json
   ```

4. **Verify:**
   ```bash
   python verify_scan.py
   ```

## Output Files

All generated files are written to `unreal/metadata/`:

- `engine_5.4_scanned.json` - UE5 5.4 type information
- `engine_5.5_scanned.json` - UE5 5.5 type information
- `engine_5.6_scanned.json` - UE5 5.6 type information
- `engine_5.7_scanned.json` - UE5 5.7 type information
- `module_graph_5.4.json` - UE5 5.4 module dependencies
- `module_graph_5.5.json` - UE5 5.5 module dependencies
- `module_graph_5.6.json` - UE5 5.6 module dependencies
- `module_graph_5.7.json` - UE5 5.7 module dependencies
- `engine_knowledge.json` - Curated engine type database (manually maintained)
- `uht_rules.json` - UHT validation rules
- `shader_knowledge.json` - HLSL type information
- `widget_registry.json` - Slate widget types
- `editor_attributes.json` - Editor attribute definitions
- `virtual_obligations.json` - Virtual function requirements

## Troubleshooting

### "No valid UE5 installations found"

**Solution:** Edit `ue5_paths_config.json` and add your UE5 installation paths. The scripts will try each path in order.

### "Python is not installed"

**Solution:** Install Python 3.7+ from python.org and ensure it's in your PATH.

### "Module graph extraction failed"

**Solution:** Make sure you ran `ue5_scanner.py` first. The module graph extractor needs the engine scan files.

### "Permission denied" on Linux/Mac

**Solution:** Make the shell script executable:
```bash
chmod +x refresh_all_metadata.sh
```

## Adding a New UE5 Version

1. **Edit config:**
   ```json
   {
     "version": "5.8",
     "paths": [
       "C:/Program Files/Epic Games/UE_5.8/Engine/Source",
       "D:/UE_5.8/Engine/Source"
     ],
     "enabled": true
   }
   ```

2. **Run refresh:**
   ```bash
   refresh_all_metadata.bat  # or .sh on Linux/Mac
   ```

3. **Verify output:**
   - Check `unreal/metadata/engine_5.8_scanned.json`
   - Check `unreal/metadata/module_graph_5.8.json`

4. **Update KAIN compiler:**
   - Rebuild the compiler to load the new metadata
   - Test with your plugins

## Multi-Drive Support

The config file supports multiple drives per version:

```json
{
  "version": "5.7",
  "paths": [
    "C:/Program Files/Epic Games/UE_5.7/Engine/Source",  # Try C: first
    "D:/UE_5.7/Engine/Source",                           # Then D:
    "M:/UnrealEngine/UE_5.7/Engine/Source",              # Then M:
    "E:/Games/UE_5.7/Engine/Source"                      # Then E:
  ]
}
```

The scripts will use the **first valid path** found. This is useful when:
- UE5 is installed on different drives on different machines
- You have multiple team members with different setups
- You're using network drives or external storage

## Integration with KAIN Compiler

The KAIN compiler loads these metadata files during initialization:

```rust
// In crates/ue5/src/ue5/context.rs
pub struct Ue5Context {
    pub knowledge: Arc<EngineKnowledge>,      // From engine_knowledge.json
    pub module_graph: ModuleGraph,            // From module_graph.json
    pub uht_rules: UhtRules,                  // From uht_rules.json
    pub shader_knowledge: ShaderKnowledge,    // From shader_knowledge.json
    pub widget_registry: WidgetRegistry,      // From widget_registry.json
    // ...
}
```

When you update the metadata files, rebuild the compiler:

```bash
cd kain
cargo build --release --package cli
```

## Best Practices

1. **Version Control:** Commit the generated metadata files to git so team members don't need to regenerate them.

2. **Regular Updates:** Refresh metadata when:
   - Upgrading to a new UE5 version
   - Adding new engine modules to your project
   - Encountering "unknown type" errors in KAIN

3. **Validation:** Always run `verify_scan.py` after extraction to catch issues early.

4. **Backup:** Keep backups of working metadata files before regenerating.

5. **Documentation:** Document any manual edits to `engine_knowledge.json` (the curated database).

## Contributing

When adding new extraction scripts:

1. Support the `--config` flag for multi-version extraction
2. Use the same config file format (`ue5_paths_config.json`)
3. Write output to `../metadata/` directory
4. Add error handling for missing paths
5. Update this README with usage instructions
6. Add the script to `refresh_all_metadata.bat` and `.sh`

## License

These scripts are part of the KAIN compiler project. See the main LICENSE file for details.
