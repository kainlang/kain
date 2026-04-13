# KAIN Inject Command

## Overview

The `inject` command allows you to surgically add new KAIN files to existing UE5 plugins without overwriting the entire plugin structure. This is useful for:

- Adding new actors, components, or structs to an existing plugin
- Incrementally building plugins file-by-file
- Collaborating on plugins without conflicts
- Testing individual features in isolation

## Usage

```bash
# Basic usage (auto-detect plugin)
kain inject --ue5 NewActor.kn

# Specify plugin directory
kain inject --ue5 NewActor.kn --plugin-dir path/to/plugin

# Specify plugin name
kain inject --ue5 NewActor.kn --plugin MyPlugin

# Inject multiple files
kain inject --ue5 Actor1.kn Actor2.kn Component1.kn

# Force overwrite existing files
kain inject --ue5 NewActor.kn --force

# Dry run (preview without writing)
kain inject --ue5 NewActor.kn --dry-run
```

## How It Works

1. **Detect Plugin**: Searches for `.uplugin` file in current directory or parents
2. **Detect Layout**: Determines if plugin uses single-module or split-module layout
3. **Parse Files**: Parses and type-checks input `.kn` files
4. **Validate**: Runs Oracle validation to ensure code quality
5. **Generate**: Creates modular `.h` and `.cpp` files for each item
6. **Check Conflicts**: Verifies no existing files will be overwritten (unless `--force`)
7. **Write Files**: Writes generated files to appropriate directories
8. **Update Headers**: Appends includes to master header

## Plugin Detection

The inject command automatically detects:

- **Plugin Directory**: Searches up to 5 levels for `.uplugin` file
- **Plugin Name**: Extracted from `.uplugin` filename
- **Layout Mode**: Single-module vs split-module (runtime + editor)

You can override detection with:
- `--plugin-dir <path>` - Explicit plugin directory
- `--plugin <name>` - Explicit plugin name

## File Placement

Generated files are placed according to the plugin's layout:

### Single-Module Layout
```
MyPlugin/
├── Source/
│   ├── Public/
│   │   ├── MyActor.h
│   │   └── MyComponent.h
│   └── Private/
│       ├── MyActor.cpp
│       └── MyComponent.cpp
```

### Split-Module Layout
```
MyPlugin/
├── Source/
│   ├── MyPlugin/          # Runtime module
│   │   ├── Public/
│   │   │   └── MyActor.h
│   │   └── Private/
│   │       └── MyActor.cpp
│   └── MyPluginEditor/    # Editor module
│       ├── Public/
│       │   └── MySlateWidget.h
│       └── Private/
│           └── MySlateWidget.cpp
```

## Conflict Detection

By default, inject will **refuse to overwrite** existing files:

```bash
$ kain inject --ue5 MyActor.kn
❌ File conflicts detected:
   - MyActor.h
   - MyActor.cpp

Use --force to overwrite existing files.
```

Use `--force` to overwrite:

```bash
$ kain inject --ue5 MyActor.kn --force
⚠️  2 file(s) will be overwritten (--force enabled)
   - MyActor.h
   - MyActor.cpp
✅ Injection complete!
```

## Dry Run Mode

Preview what would be generated without writing files:

```bash
$ kain inject --ue5 MyActor.kn --dry-run
🔍 DRY RUN - Files that would be generated:
   - AMyActor.h
   - AMyActor.cpp
   [DRY RUN] Would update master header: Source/MyPlugin/Public/MyPlugin.h
      + #include "AMyActor.h"
```

## Master Header Updates

The inject command automatically updates the plugin's master header:

```cpp
// Before injection
#pragma once
#include "CoreMinimal.h"
#include "ExistingActor.h"

// After injection
#pragma once
#include "CoreMinimal.h"
#include "ExistingActor.h"
#include "AMyActor.h"  // ← Added by inject
```

## Supported Items

The inject command supports all KAIN language features:

- **Actors**: `actor MyActor`
- **Components**: `@component struct MyComponent`
- **Structs**: `struct MyData`, `@datatable struct ItemData`
- **Enums**: `enum MyEnum`
- **Delegates**: `type MyDelegate = delegate(...)`
- **Slate Widgets**: `@slate struct MyWidget`
- **Details Panels**: `@details struct MyDetails`
- **Viewports**: `@viewport struct MyViewport`
- **Toolbars**: `@toolbar struct MyToolbar`
- **Asset Editors**: `@asset_editor struct MyEditor`
- **Editor Modules**: `@editor_module struct MyModule`

## Limitations

1. **No .uplugin/.Build.cs Updates**: Inject does not modify plugin metadata or build files
2. **No Shader Registration**: Shaders must be added via full `kain build --ue5`
3. **No Module Creation**: Cannot create new modules, only add to existing ones
4. **No Dependency Resolution**: Does not update module dependencies in .Build.cs

## Examples

### Add Actor to Existing Plugin

```bash
cd MyPlugin
kain inject --ue5 NewEnemy.kn
```

### Add Multiple Files

```bash
kain inject --ue5 HealthComponent.kn DamageComponent.kn InventoryComponent.kn
```

### Add to Specific Plugin

```bash
kain inject --ue5 MyActor.kn --plugin-dir ../OtherPlugin --plugin OtherPlugin
```

### Preview Changes

```bash
kain inject --ue5 MyActor.kn --dry-run
```

### Force Overwrite

```bash
kain inject --ue5 MyActor.kn --force
```

## Workflow Integration

### Incremental Development

```bash
# Start with minimal plugin
kain build --ue5  # Creates plugin structure

# Add features incrementally
kain inject --ue5 Feature1.kn
kain inject --ue5 Feature2.kn
kain inject --ue5 Feature3.kn

# Rebuild when needed
kain build --ue5
```

### Team Collaboration

```bash
# Developer A adds actor
kain inject --ue5 PlayerActor.kn

# Developer B adds component (no conflict!)
kain inject --ue5 InventoryComponent.kn

# Both changes coexist without overwriting
```

### Testing Individual Features

```bash
# Test new actor in isolation
kain inject --ue5 TestActor.kn --dry-run  # Preview
kain inject --ue5 TestActor.kn            # Add
# Test in UE5
kain inject --ue5 TestActor.kn --force    # Update after changes
```

## Comparison with Build Command

| Feature | `kain build --ue5` | `kain inject --ue5` |
|---------|-------------------|---------------------|
| Creates plugin structure | ✅ | ❌ |
| Generates .uplugin | ✅ | ❌ |
| Generates .Build.cs | ✅ | ❌ |
| Compiles shaders | ✅ | ❌ |
| Adds individual files | ❌ | ✅ |
| Non-destructive | ❌ | ✅ |
| Conflict detection | ❌ | ✅ |
| Dry run mode | ❌ | ✅ |

## Best Practices

1. **Use `build` for initial setup**: Create plugin structure with `kain build --ue5`
2. **Use `inject` for additions**: Add new files with `kain inject --ue5`
3. **Preview with `--dry-run`**: Always check what will be generated
4. **Avoid `--force` unless necessary**: Prevents accidental overwrites
5. **Commit before injecting**: Use version control to track changes
6. **Rebuild periodically**: Run `kain build --ue5` to regenerate metadata

## Troubleshooting

### "Could not find plugin directory"

```bash
# Solution 1: Run from plugin directory
cd MyPlugin
kain inject --ue5 MyActor.kn

# Solution 2: Specify plugin directory
kain inject --ue5 MyActor.kn --plugin-dir path/to/MyPlugin
```

### "File conflicts detected"

```bash
# Solution 1: Use different name
kain inject --ue5 MyActor2.kn

# Solution 2: Force overwrite
kain inject --ue5 MyActor.kn --force
```

### "Invalid plugin layout"

```bash
# Plugin must have Source/Public and Source/Private directories
# Run kain build --ue5 first to create structure
```

## Future Enhancements

- [ ] Auto-update .Build.cs dependencies
- [ ] Support shader injection
- [ ] Support material graph injection
- [ ] Interactive conflict resolution
- [ ] Batch injection from directory
- [ ] Rollback/undo support
- [ ] Injection history tracking

## See Also

- [SURGICAL_INJECTION_MODE.md](SURGICAL_INJECTION_MODE.md) - Design document
- [AGENT_HANDOFF.md](AGENT_HANDOFF.md) - Architecture overview
- [PACKAGER_ARCHITECTURE.md](../crates/cli/PACKAGER_ARCHITECTURE.md) - Build system details
