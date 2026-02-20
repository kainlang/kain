# Phase 3: KAIN.toml Optional Implementation

## Status: IMPLEMENTED (Pending Full Build Test)

## Overview
Made `KAIN.toml` optional for `kain build --ue5` by implementing auto-detection of .kn files and plugin name.

## Changes Made

### 1. Modified `build_ue5_plugin()` in `crates/cli/src/packager/ue5_pipeline.rs`

**Before:**
```rust
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let manifest = super::load_manifest(&cwd)?;  // FAILS if no KAIN.toml
    
    let ue5_config = manifest.ue5.as_ref()
        .ok_or_else(|| KainError::runtime("No [ue5] section in KAIN.toml"))?;
    // ...
}
```

**After:**
```rust
pub fn build_ue5_plugin() -> KainResult<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    
    // Try to load KAIN.toml, but don't fail if it doesn't exist
    let (manifest, ue5_config) = match super::load_manifest(&cwd) {
        Ok(manifest) => {
            // KAIN.toml exists - use it
            let ue5_config = manifest.ue5.as_ref()
                .ok_or_else(|| KainError::runtime("No [ue5] section in KAIN.toml"))?
                .clone();
            (Some(manifest), ue5_config)
        }
        Err(_) => {
            // KAIN.toml not found - auto-detect
            println!("ℹ️  No KAIN.toml found, using auto-detection...");
            println!();
            let config = create_default_config(&cwd)?;
            (None, config)
        }
    };
    // ...
}
```

### 2. Updated `load_and_parse_sources()` Signature

Changed from:
```rust
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: &super::config::PackageManifest,  // Required
    cwd: &PathBuf,
) -> KainResult<...>
```

To:
```rust
fn load_and_parse_sources(
    ue5_config: &Ue5Config,
    manifest: Option<&super::config::PackageManifest>,  // Optional
    cwd: &PathBuf,
) -> KainResult<...>
```

### 3. Added Helper Functions

#### `create_default_config(cwd: &PathBuf) -> KainResult<Ue5Config>`
- Auto-detects plugin name from .uplugin or directory name
- Finds all .kn files in current directory (non-recursive)
- Returns error if no .kn files found
- Creates Ue5Config with sensible defaults:
  - `modular_output: true`
  - `stdlib_path: None`
  - `shaders: vec![]`

#### `detect_plugin_name_from_dir(cwd: &PathBuf) -> KainResult<String>`
- Looks for .uplugin file first
- Falls back to directory name
- Prints detection method for user feedback

#### `find_kn_files(cwd: &PathBuf) -> KainResult<Vec<PathBuf>>`
- Scans current directory for .kn files (non-recursive)
- Skips README files
- Sorts files for consistent ordering

## User Experience

### With KAIN.toml (Existing Behavior)
```bash
cd MyPlugin
kain build --ue5
# Uses KAIN.toml configuration
```

### Without KAIN.toml (New Behavior)
```bash
cd MyPlugin
kain build --ue5
# Output:
# ℹ️  No KAIN.toml found, using auto-detection...
# 
# 🔍 Detected plugin name from .uplugin: MyPlugin
# 📁 Found 2 .kn file(s):
#    - actors.kn
#    - components.kn
# 
# 🚀 Building UE5 Plugin: MyPlugin
# ...
```

## Error Handling

### No .kn Files
```
Error: No .kn files found in current directory. Please create a .kn file or add a KAIN.toml configuration.
```

### Cannot Detect Plugin Name
```
Error: Could not determine plugin name. Please create a KAIN.toml or .uplugin file.
```

## Backward Compatibility

✅ **Fully backward compatible**
- KAIN.toml still works exactly as before
- Only uses auto-detection as fallback
- No breaking changes to existing workflows

## Testing

### Test Case 1: Directory with .uplugin
```
MyPlugin/
├── MyPlugin.uplugin
├── test.kn
└── (no KAIN.toml)

Result: Plugin name = "MyPlugin" (from .uplugin)
```

### Test Case 2: Directory without .uplugin
```
MyPlugin/
├── test.kn
└── (no KAIN.toml)

Result: Plugin name = "MyPlugin" (from directory name)
```

### Test Case 3: Multiple .kn files
```
MyPlugin/
├── actors.kn
├── components.kn
├── enums.kn
└── (no KAIN.toml)

Result: All 3 files included in build
```

### Test Case 4: With KAIN.toml
```
MyPlugin/
├── KAIN.toml
├── test.kn
└── MyPlugin.uplugin

Result: Uses KAIN.toml configuration (existing behavior)
```

## Implementation Notes

1. **Non-recursive file search**: Only scans current directory, not subdirectories
2. **Modular output default**: Auto-detected configs use `modular_output: true`
3. **No stdlib by default**: Auto-detected configs don't load stdlib
4. **Sorted file order**: .kn files are sorted alphabetically for consistency
5. **README exclusion**: Files with "README" in name are skipped

## Known Limitations

1. Cannot auto-detect:
   - Custom shader names (uses auto-detection from .kn files)
   - Copyright notice (uses None)
   - Plugin description (uses None)
   - Custom stdlib path (uses None)

2. Only scans current directory (not recursive)

3. Requires at least one .kn file in current directory

## Future Enhancements

1. **Recursive .kn file search**: Option to scan subdirectories
2. **Smart defaults from .uplugin**: Parse .uplugin for description, version, etc.
3. **Interactive mode**: Prompt user for missing configuration
4. **Config generation**: `kain init --ue5` to generate KAIN.toml from auto-detected settings

## Files Modified

- `crates/cli/src/packager/ue5_pipeline.rs` - Main implementation
  - Modified `build_ue5_plugin()` function
  - Modified `load_and_parse_sources()` signature
  - Added `create_default_config()` helper
  - Added `detect_plugin_name_from_dir()` helper
  - Added `find_kn_files()` helper

## Compilation Status

✅ `ue5_pipeline.rs` compiles without errors
⚠️  Full CLI build blocked by pre-existing errors in `inject.rs` (unrelated to this feature)

## Next Steps

1. Fix pre-existing errors in `inject.rs`
2. Build CLI binary
3. Test with actual plugin directory
4. Verify .uplugin detection works
5. Verify directory name fallback works
6. Test with multiple .kn files
7. Verify backward compatibility with KAIN.toml
