# KAIN Extension System

## Overview

The KAIN Extension System allows you to dynamically add support for any UE5 plugin or module (like MetaHuman, Niagara, PCG, etc.) without modifying the core compiler. Extensions are automatically loaded at compile time and provide full type information, includes, and module dependencies.

## How It Works

1. **Scan** - Use `extension_scanner.py` to extract API metadata from any UE5 plugin
2. **Generate** - Creates a JSON file in this directory (`extensions/*.json`)
3. **Auto-Load** - The Rust backend automatically loads all extensions on startup
4. **Use** - KAIN code can now use types from the extension seamlessly

## Quick Start

### Scan a Plugin

```bash
# Scan MetaHuman plugin
python Kain/unreal/scripts/extension_scanner.py Research/ReferenceCode/MetaHuman --name metahuman

# Scan Niagara plugin
python Kain/unreal/scripts/extension_scanner.py "C:/Program Files/Epic Games/UE_5.4/Engine/Plugins/FX/Niagara" --name niagara

# Scan PCG plugin
python Kain/unreal/scripts/extension_scanner.py "C:/Program Files/Epic Games/UE_5.4/Engine/Plugins/Experimental/PCG" --name pcg
```

### Use in KAIN Code

Once an extension is loaded, you can use its types directly:

```kain
// MetaHuman extension loaded - use MetaHuman types
actor MyMetaHumanController:
    state metahuman_character: MetaHumanCharacter
    state wardrobe_item: MetaHumanWardrobeItem
    
    fn apply_clothing(item: MetaHumanWardrobeItem):
        metahuman_character.add_wardrobe_item(item)
        println("Clothing applied!")

// Niagara extension loaded - use Niagara types
@component
struct MyNiagaraEffect:
    @replicated
    niagara_component: NiagaraComponent
    
    @replicated
    niagara_system: NiagaraSystem
```

The compiler automatically:
- Resolves type names (MetaHumanCharacter → UMetaHumanCharacter)
- Adds correct includes (#include "MetaHumanCharacter.h")
- Adds module dependencies (MetaHumanCharacter module)
- Validates type usage against the extension API

## Extension File Format

Extensions follow the same schema as `engine_knowledge.json`:

```json
{
  "extension_name": "metahuman",
  "version": "1.0.0",
  "description": "MetaHuman API metadata",
  "generated_at": "2026-02-22 07:52:18",
  
  "classes": [
    {
      "name": "UMetaHumanCharacter",
      "parent": "UObject",
      "header": "MetaHumanCharacter.h",
      "module": "MetaHumanCharacter",
      "prefix": "U",
      "is_abstract": false,
      "is_blueprintable": true
    }
  ],
  
  "structs": [...],
  "enums": [...],
  "interfaces": [...],
  "components": [...],
  "subsystems": [...],
  
  "include_map": {
    "UMetaHumanCharacter": "MetaHumanCharacter.h"
  },
  
  "module_map": {
    "UMetaHumanCharacter": "MetaHumanCharacter"
  },
  
  "modules": {
    "MetaHumanCharacter": {
      "name": "MetaHumanCharacter",
      "public_dependencies": ["Core", "CoreUObject", "Engine"],
      "private_dependencies": []
    }
  }
}
```

## Backend Integration

The Rust backend automatically loads extensions in `Ue5Context::new()`:

```rust
// In crates/ue5/src/ue5/context.rs
let extensions_dir = std::path::Path::new("unreal/metadata/extensions");
if let Ok(count) = knowledge.load_extensions(extensions_dir) {
    if count > 0 {
        eprintln!("📦 Loaded {} extension(s)", count);
    }
}
```

Extensions are merged into the main `EngineKnowledge` database, so all codegen systems can access them:
- Type resolution (`map_type()`)
- Include generation (`get_include()`)
- Module dependencies (`get_module_for_type()`)
- Hierarchy checks (`is_child_of()`)

## Available Extensions

### metahuman.json (313.5 KB)
- **Classes**: 256
- **Structs**: 176
- **Enums**: 99
- **Interfaces**: 6
- **Components**: 8
- **Subsystems**: 2
- **Modules**: 53

**Key Types**:
- `UMetaHumanCharacter` - Main character class
- `UMetaHumanWardrobeItem` - Clothing asset
- `UMetaHumanOutfitPipeline` - Clothing processing pipeline
- `UChaosClothComponent` - Physics simulation
- `UChaosOutfitAsset` - Cloth physics asset

## Creating New Extensions

### 1. Find the Plugin

Locate the UE5 plugin you want to add support for:
- Engine plugins: `C:/Program Files/Epic Games/UE_5.4/Engine/Plugins/`
- Project plugins: `YourProject/Plugins/`
- Marketplace plugins: Usually in project plugins folder

### 2. Run the Scanner

```bash
python Kain/unreal/scripts/extension_scanner.py <plugin_path> --name <extension_name>
```

**Naming Convention**:
- Use lowercase, no spaces
- Use the plugin's official name
- Examples: `metahuman`, `niagara`, `pcg`, `enhancedinput`, `gameplayabilities`

### 3. Verify the Output

Check that the generated JSON file contains:
- Classes with correct parent relationships
- Structs with proper headers
- Enums with all values
- Module dependencies

### 4. Test in KAIN

Write a simple KAIN file using types from the extension:

```kain
actor TestExtension:
    state my_type: SomeExtensionType
    
    fn test():
        println("Extension loaded!")
```

Build with `kain build --ue5` and verify:
- No "unknown type" errors
- Correct includes generated
- Module dependencies added to .Build.cs

## Troubleshooting

### Extension Not Loading

**Problem**: Extension JSON exists but types aren't recognized

**Solution**:
1. Check the JSON is valid: `python -m json.tool extensions/myextension.json`
2. Verify the file is in `Kain/unreal/metadata/extensions/`
3. Check the console output for "Loaded X extension(s)" message
4. Rebuild the KAIN compiler: `cargo install --path crates/cli --force`

### Missing Types

**Problem**: Some types from the plugin aren't in the extension

**Solution**:
1. The scanner only extracts types with UCLASS/USTRUCT/UENUM macros
2. Check if the type is in a Private/ folder (not scanned by default)
3. Manually add the type to the JSON if needed

### Wrong Includes

**Problem**: Generated C++ has wrong include paths

**Solution**:
1. Check the `include_map` in the extension JSON
2. Verify the header paths are relative to Public/ or Classes/
3. Update the JSON manually if the scanner got it wrong

### Module Dependencies Missing

**Problem**: Build.cs doesn't include required modules

**Solution**:
1. Check the `modules` section in the extension JSON
2. Verify the Build.cs file was scanned correctly
3. Add missing dependencies manually to the JSON

## Best Practices

### 1. One Extension Per Plugin

Create separate extensions for each major plugin:
- ✅ `metahuman.json` for MetaHuman
- ✅ `niagara.json` for Niagara
- ✅ `pcg.json` for PCG
- ❌ Don't combine multiple plugins into one extension

### 2. Keep Extensions Updated

When the plugin updates:
1. Re-run the scanner on the new version
2. Compare the new JSON with the old one
3. Merge any manual changes you made

### 3. Document Custom Changes

If you manually edit an extension JSON:
```json
{
  "extension_name": "metahuman",
  "version": "1.0.0",
  "description": "MetaHuman API metadata (manually added XYZ type)",
  ...
}
```

### 4. Share Extensions

Extensions are portable! Share them with the community:
- Commit to the KAIN repo
- Share on Discord/forums
- Create a marketplace for extensions

## Advanced Usage

### Custom Type Mappings

Add custom type aliases in your extension:

```json
{
  "type_aliases": [
    {
      "kain_name": "MetaHuman",
      "ue5_name": "UMetaHumanCharacter",
      "header": "MetaHumanCharacter.h"
    }
  ]
}
```

Now you can use `MetaHuman` instead of `MetaHumanCharacter` in KAIN code.

### Extension Dependencies

If your extension depends on another extension:

```json
{
  "extension_name": "myextension",
  "dependencies": ["metahuman", "niagara"],
  ...
}
```

The compiler will ensure dependencies are loaded first.

### Conditional Loading

Load extensions only for specific projects:

```toml
# KAIN.toml
[extensions]
enabled = ["metahuman", "niagara"]
disabled = ["pcg"]
```

## Future Enhancements

- **Auto-Update**: Automatically check for plugin updates and re-scan
- **Extension Marketplace**: Browse and install community extensions
- **Validation**: Verify extensions against actual UE5 headers
- **Merging**: Combine multiple extensions intelligently
- **Versioning**: Support multiple versions of the same plugin

## Contributing

Want to add support for a new plugin?

1. Scan the plugin with `extension_scanner.py`
2. Test the extension with a sample KAIN file
3. Submit a PR with the JSON file
4. Add documentation to this README

## License

Extensions are metadata extracted from UE5 plugins. The metadata itself is factual information and not subject to copyright. However, the original plugins may have their own licenses.

---

**Questions?** Check the KAIN documentation or ask on Discord!
