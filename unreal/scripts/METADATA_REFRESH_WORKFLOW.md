# KAIN Metadata Refresh Workflow

## Overview

This document describes the complete workflow for refreshing KAIN's UE5 metadata files. The metadata-first architecture ensures all UE5 knowledge is loaded from JSON files, enabling zero-recompilation updates and multi-version support.

## Prerequisites

- Python 3.7+ installed and in PATH
- At least one UE5 installation (5.4, 5.5, 5.6, or 5.7)
- Write access to `unreal/metadata/` directory

## Quick Start

### Automated Refresh (Recommended)

**Windows:**

```bash
cd unreal/scripts
refresh_all_metadata.bat
```

**Linux/Mac:**

```bash
cd unreal/scripts
chmod +x refresh_all_metadata.sh
./refresh_all_metadata.sh
```

This runs the core extraction pipeline:

1. Scan UE5 installations for engine types
1. Extract module dependency graphs
1. Verify metadata completeness

### Full Refresh (All Metadata)

For a complete metadata refresh including all optional files:

**Windows:**

```bash
cd unreal/scripts
refresh_all_metadata_full.bat
```

**Linux/Mac:**

```bash
cd unreal/scripts
chmod +x refresh_all_metadata_full.sh
./refresh_all_metadata_full.sh
```

This runs the complete extraction pipeline:

1. Engine type scanning
1. Module dependency graphs
1. UHT validation rules
1. Shader knowledge
1. Editor attributes
1. Virtual function obligations
1. Metadata verification

## Configuration

### Step 1: Edit UE5 Installation Paths

Edit `ue5_paths_config.json` with your UE5 installation paths:

```json
{
  "installations": [
    {
      "version": "5.7",
      "paths": [
        "C:/Program Files/Epic Games/UE_5.7/Engine/Source",
        "D:/UE_5.7/Engine/Source",
        "M:/UnrealEngine/UE_5.7/Engine/Source"
      ],
      "enabled": true
    }
  ],
  "output_directory": "../metadata",
  "output_filename_template": "engine_{version}_scanned.json"
}
```

**Key points:**

- Add all possible installation paths for each version
- Scripts will try each path in order until one succeeds
- Set `"enabled": false` to skip a version
- First valid path found is used

### Step 2: Verify Configuration

```bash
python verify_config.py
```

This checks:

- Config file is valid JSON
- At least one enabled installation exists
- Paths are accessible
- Output directory is writable

## Extraction Scripts

### Core Scripts (Required)

#### 1. ue5_scanner.py

Scans UE5 headers for engine types.

**Usage:**

```bash
# All configured installations
python ue5_scanner.py --config ue5_paths_config.json

# Single installation
python ue5_scanner.py "D:/UE_5.7/Engine/Source" ../metadata/engine_5.7_scanned.json
```

**Output:**

- `engine_5.4_scanned.json`
- `engine_5.5_scanned.json`
- `engine_5.6_scanned.json`
- `engine_5.7_scanned.json`

**Extracts:**

- UCLASS types with inheritance
- USTRUCT types with fields
- UENUM types with values
- Include paths
- Constructor signatures

#### 2. module_graph_extractor.py

Extracts module dependency information from .Build.cs files.

**Usage:**

```bash
# All configured installations
python module_graph_extractor.py --config ue5_paths_config.json

# Single installation
python module_graph_extractor.py "D:/UE_5.7/Engine/Source" --engine-scan ../metadata/engine_5.7_scanned.json
```

**Output:**

- `module_graph_5.4.json`
- `module_graph_5.5.json`
- `module_graph_5.6.json`
- `module_graph_5.7.json`

**Extracts:**

- Module names and categories
- Public/private dependencies
- Transitive dependency closure
- Type → module mappings
- Header → module mappings

### Optional Scripts (Enhanced Validation)

#### 3. uht_extractor.py

Extracts UHT validation rules from UnrealHeaderTool source.

**Usage:**

```bash
python uht_extractor.py "D:/UE_5.7/Engine/Source/Programs/UnrealHeaderTool"
```

**Output:** `uht_rules.json`

**Extracts:**

- UPROPERTY validation rules
- UFUNCTION validation rules
- Replication rules
- Attribute compatibility rules

#### 4. shader_extractor.py

Extracts HLSL type information and shader validation rules.

**Usage:**

```bash
python shader_extractor.py "D:/UE_5.7/Engine/Shaders"
```

**Output:** `shader_knowledge.json`

**Extracts:**

- HLSL built-in types
- HLSL keywords
- Binding slot rules
- Permutation naming conventions

#### 5. editor_attributes_extractor.py

Extracts editor attribute definitions from Slate and PropertyEditor source.

**Usage:**

```bash
python editor_attributes_extractor.py "D:/UE_5.7/Engine/Source"
```

**Output:** `editor_attributes.json`

**Extracts:**

- @slider, @color_picker, @button definitions
- Property type mappings
- Widget composition rules

#### 6. virtual_obligations_extractor.py

Extracts virtual function requirements for UE5 classes.

**Usage:**

```bash
python virtual_obligations_extractor.py "D:/UE_5.7/Engine/Source"
```

**Output:** `virtual_obligations.json`

**Extracts:**

- Pure virtual functions that must be overridden
- Virtual functions with default implementations
- Interface requirements

### Verification Script

#### verify_scan.py

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
- Schema validation passes

## Output Files

All generated files are written to `unreal/metadata/`:

### Core Files (Auto-Generated)

- `engine_5.4_scanned.json` - UE5 5.4 type information
- `engine_5.5_scanned.json` - UE5 5.5 type information
- `engine_5.6_scanned.json` - UE5 5.6 type information
- `engine_5.7_scanned.json` - UE5 5.7 type information
- `module_graph_5.4.json` - UE5 5.4 module dependencies
- `module_graph_5.5.json` - UE5 5.5 module dependencies
- `module_graph_5.6.json` - UE5 5.6 module dependencies
- `module_graph_5.7.json` - UE5 5.7 module dependencies

### Curated Files (Manually Maintained)

- `engine_knowledge.json` - Curated engine type database
- `widget_registry.json` - Slate widget types
- `named_colors.json` - Named color definitions
- `constructor_formats.json` - Constructor format strings

### Optional Files (Enhanced Validation)

- `uht_rules.json` - UHT validation rules
- `shader_knowledge.json` - HLSL type information
- `editor_attributes.json` - Editor attribute definitions
- `virtual_obligations.json` - Virtual function requirements

## Workflow Steps

### 1. Initial Setup (First Time Only)

```bash
cd unreal/scripts

# Edit config with your UE5 paths
notepad ue5_paths_config.json  # Windows
nano ue5_paths_config.json     # Linux/Mac

# Verify config
python verify_config.py
```

### 2. Core Metadata Refresh

```bash
# Run automated refresh
refresh_all_metadata.bat  # Windows
./refresh_all_metadata.sh # Linux/Mac
```

This takes 2-5 minutes per UE5 version.

### 3. Optional: Full Metadata Refresh

```bash
# Run full refresh (includes optional files)
refresh_all_metadata_full.bat  # Windows
./refresh_all_metadata_full.sh # Linux/Mac
```

This takes 5-10 minutes per UE5 version.

### 4. Verify Output

```bash
# Check for errors
python verify_scan.py

# Review generated files
cd ../metadata
ls -lh  # Linux/Mac
dir     # Windows
```

### 5. Rebuild KAIN Compiler

```bash
cd ../../kain
cargo build --release --package cli
```

### 6. Test with Plugins

```bash
cd ../testing/Phase3/SlateTest4
kain build --ue5
```

## Troubleshooting

### "No valid UE5 installations found"

**Cause:** None of the configured paths exist or are accessible.

**Solution:**

1. Edit `ue5_paths_config.json`
1. Add your actual UE5 installation paths
1. Ensure paths point to `Engine/Source` directory
1. Run `python verify_config.py` to check

### "Python is not installed"

**Cause:** Python not in PATH or not installed.

**Solution:**

1. Install Python 3.7+ from python.org
1. During installation, check "Add Python to PATH"
1. Restart terminal/command prompt
1. Verify: `python --version`

### "Module graph extraction failed"

**Cause:** Engine scan files not found.

**Solution:**

1. Run `ue5_scanner.py` first
1. Ensure `engine_{version}_scanned.json` files exist
1. Then run `module_graph_extractor.py`

### "Permission denied" on Linux/Mac

**Cause:** Shell scripts not executable.

**Solution:**

```bash
chmod +x refresh_all_metadata.sh
chmod +x refresh_all_metadata_full.sh
```

### "JSON parsing error"

**Cause:** Malformed JSON in config or output files.

**Solution:**

1. Validate config: `python -m json.tool ue5_paths_config.json`
1. Delete corrupted output files
1. Re-run extraction scripts

### "Metadata verification failed"

**Cause:** Missing or incomplete metadata files.

**Solution:**

1. Check which files are missing in error output
1. Re-run specific extraction script
1. If persistent, check UE5 installation integrity

## Adding a New UE5 Version

### Step 1: Update Config

Edit `ue5_paths_config.json`:

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

### Step 2: Run Extraction

```bash
python ue5_scanner.py --config ue5_paths_config.json
python module_graph_extractor.py --config ue5_paths_config.json
```

### Step 3: Verify Output

```bash
python verify_scan.py
```

Check for:

- `engine_5.8_scanned.json`
- `module_graph_5.8.json`

### Step 4: Update KAIN Compiler

The compiler automatically detects new metadata files. Just rebuild:

```bash
cd ../../kain
cargo build --release --package cli
```

### Step 5: Test

```bash
cd ../testing/Phase3/SlateTest4
kain build --ue5
```

## Multi-Drive Support

The config file supports multiple drives per version:

```json
{
  "version": "5.7",
  "paths": [
    "C:/Program Files/Epic Games/UE_5.7/Engine/Source",
    "D:/UE_5.7/Engine/Source",
    "M:/UnrealEngine/UE_5.7/Engine/Source",
    "E:/Games/UE_5.7/Engine/Source"
  ]
}
```

**How it works:**

- Scripts try each path in order
- First valid path is used
- Remaining paths are ignored
- Useful for team environments with different setups

## Version Detection

The scripts automatically detect UE5 version from:

1. Config file `version` field
1. Path structure (`UE_5.7` → version 5.7)
1. Engine version header files

**Priority:** Config > Path > Header

## Best Practices

### 1. Version Control

**Commit generated metadata files to git:**

- Team members don't need to regenerate
- Ensures consistent builds
- Tracks metadata changes over time

```bash
git add unreal/metadata/*.json
git commit -m "Update UE5 metadata for version 5.7"
```

### 2. Regular Updates

**Refresh metadata when:**

- Upgrading to a new UE5 version
- Adding new engine modules to your project
- Encountering "unknown type" errors in KAIN
- UE5 hotfix/patch is installed

**Frequency:** Monthly or after UE5 updates

### 3. Validation

**Always run verification after extraction:**

```bash
python verify_scan.py
```

Catches issues early before they cause build failures.

### 4. Backup

**Keep backups of working metadata:**

```bash
cd unreal/metadata
tar -czf metadata_backup_$(date +%Y%m%d).tar.gz *.json  # Linux/Mac
# Or manually copy to backup folder on Windows
```

### 5. Documentation

**Document manual edits to curated files:**

When editing `engine_knowledge.json` manually:

1. Add a comment explaining the change
1. Include the date and reason
1. Commit with descriptive message

Example:

```json
{
  "types": {
    "CustomType": {
      "cpp_name": "UCustomType",
      "include": "CustomType.h",
      "comment": "Added 2026-02-12: Custom type for project XYZ"
    }
  }
}
```

## Integration with KAIN Compiler

### Metadata Loading

The KAIN compiler loads metadata during initialization:

```rust
// In crates/ue5/src/ue5/context.rs
pub struct Ue5Context {
    pub knowledge: Arc<EngineKnowledge>,      // From engine_knowledge.json
    pub module_graph: ModuleGraph,            // From module_graph.json
    pub uht_rules: UhtRules,                  // From uht_rules.json
    pub shader_knowledge: ShaderKnowledge,    // From shader_knowledge.json
    pub widget_registry: WidgetRegistry,      // From widget_registry.json
}
```

### Hot-Reload Support

The compiler supports hot-reloading metadata without recompilation:

```bash
# Watch for metadata changes
kain build --ue5 --watch-metadata
```

When metadata files change:

1. Compiler detects file modification
1. Reloads affected metadata
1. Validates new metadata
1. Continues build with updated knowledge

**Note:** Hot-reload is implemented in task 0.10.

## Continuous Integration

### GitHub Actions Example

```yaml
name: Refresh Metadata

on:
  schedule:
    - cron: '0 0 * * 0'  # Weekly on Sunday
  workflow_dispatch:

jobs:
  refresh:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Refresh Metadata
        run: |
          cd unreal/scripts
          python ue5_scanner.py --config ue5_paths_config.json
          python module_graph_extractor.py --config ue5_paths_config.json
          python verify_scan.py
      
      - name: Commit Changes
        run: |
          git config user.name "Metadata Bot"
          git config user.email "bot@example.com"
          git add unreal/metadata/*.json
          git commit -m "Auto-update UE5 metadata" || echo "No changes"
          git push
```

## Performance

### Extraction Times (Approximate)

| Script | Time per Version | Output Size |
|--------|------------------|-------------|
| ue5_scanner.py | 2-3 minutes | 5-10 MB |
| module_graph_extractor.py | 1-2 minutes | 2-5 MB |
| uht_extractor.py | 30 seconds | 500 KB |
| shader_extractor.py | 20 seconds | 200 KB |
| editor_attributes_extractor.py | 30 seconds | 300 KB |
| virtual_obligations_extractor.py | 1 minute | 1 MB |

**Total (core):** 3-5 minutes per version\
**Total (full):** 5-10 minutes per version

### Optimization Tips

1. **Disable unused versions** in config
1. **Run in parallel** for multiple versions (manual)
1. **Use SSD** for faster file I/O
1. **Increase Python memory** for large scans

## Support

### Getting Help

1. **Check README.md** for script usage
1. **Run verify_scan.py** to diagnose issues
1. **Check error messages** for specific guidance
1. **Review this workflow document** for troubleshooting

### Reporting Issues

When reporting metadata extraction issues, include:

1. UE5 version and installation path
1. Python version (`python --version`)
1. Config file contents
1. Full error message
1. Output of `verify_scan.py`

## Future Enhancements

### Planned Features

- [ ] Parallel extraction for multiple versions
- [ ] Incremental updates (only changed files)
- [ ] Metadata diffing tool
- [ ] Web UI for metadata browsing
- [ ] Automatic UE5 version detection
- [ ] Cloud-hosted metadata repository

### Contributing

To add new extraction scripts:

1. Support `--config` flag for multi-version
1. Use same config file format
1. Write output to `../metadata/`
1. Add error handling for missing paths
1. Update README.md with usage
1. Add to refresh scripts
1. Add verification checks

## License

These scripts are part of the KAIN compiler project. See the main LICENSE file for details.

______________________________________________________________________

**Last Updated:** 2026-02-12\
**Version:** 1.0\
**Maintainer:** KAIN Development Team
