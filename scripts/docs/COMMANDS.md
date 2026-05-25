# KAIN Commands - LLM Quick Reference

> Ultra-simple command guide optimized for AI agents. No fluff, just what you need.

## Repo Build Rule

For repo/compiler/runtime work, use Bazel first:

```bash
bazel build //:kain --config=dev
bazel build //:kn --config=dev
bazel build //:blade --config=dev
bazel test //:crate_tests --config=dev
bazel test //:developer_smoke_tests --config=dev
```

Do not default to `cargo build`, `cargo run`, or `cargo test` for normal agent workflows in this repo.

---

## 🎯 The Three Ways to Use KAIN

### 1. Single File → C++ (Quick & Dirty)
```bash
kain MyActor.kn -t ue5
```
**What it does:** Compiles one .kn file to UE5 C++ (header + source)  
**Output:** `AMyActor.h` and `AMyActor.cpp` in current directory  
**Use when:** Testing, prototyping, or generating standalone files  
**Metadata:** ✅ Full (auto-loads from KAIN_ROOT or walks up directories)  
**Validation:** ✅ Oracle runs  

---

### 2. Full Plugin Build (The Main Event)
```bash
cd MyPlugin
kain build --ue5
```
**What it does:** Builds complete UE5 plugin from all .kn files  
**Output:** Complete plugin structure (Source/, Shaders/, .uplugin, .Build.cs)  
**Use when:** Building production plugins  
**KAIN.toml:** Optional (auto-detects if missing)  
**Metadata:** ✅ Full  
**Validation:** ✅ Oracle runs  

**With KAIN.toml:**
```toml
[package]
name = "MyPlugin"
version = "1.0.0"

[build]
entry = ["main.kn", "actors.kn", "components.kn"]
```

**Without KAIN.toml:**
- Auto-finds .uplugin file
- Auto-scans for all .kn files in current directory
- Just works™

---

### 3. Surgical Injection (Add to Existing Plugin)
```bash
cd ExistingPlugin
kain inject --ue5 NewComponent.kn
```
**What it does:** Adds new files to existing plugin WITHOUT overwriting  
**Output:** New .h/.cpp files in Source/Public/ and Source/Private/  
**Use when:** Adding features to existing plugins  
**Safety:** ✅ Conflict detection (aborts if files exist)  
**Metadata:** ✅ Full  
**Validation:** ✅ Oracle runs  

**Flags:**
- `--dry-run` - Preview what would be generated (no file writes)
- `--force` - Overwrite existing files (use with caution)
- `--plugin-dir <path>` - Specify plugin directory explicitly
- `--plugin <name>` - Specify plugin name explicitly

---

## 📋 Command Cheat Sheet

| Command | What It Does | Output Location | Overwrites? |
|---------|-------------|-----------------|-------------|
| `kain file.kn -t ue5` | Single file → C++ | Current dir | Yes |
| `kain build --ue5` | Full plugin build | PluginName/ | Yes |
| `kain inject --ue5 file.kn` | Add to existing | Existing plugin | No (unless --force) |
| `kain inject --ue5 file.kn --dry-run` | Preview injection | None (preview only) | No |
| `kain inject --ue5 file.kn --force` | Force overwrite | Existing plugin | Yes |

---

## 🔥 Common Workflows

### Workflow 1: Start New Plugin
```bash
# Create directory
mkdir MyPlugin
cd MyPlugin

# Write .kn files
echo "actor Player: ..." > player.kn
echo "struct Item: ..." > items.kn

# Build plugin (no KAIN.toml needed!)
kain build --ue5

# Output: MyPlugin/Source/, MyPlugin/Shaders/, MyPlugin.uplugin
```

### Workflow 2: Add Feature to Existing Plugin
```bash
# Navigate to plugin
cd MyExistingPlugin

# Write new feature
echo "@component struct Health: ..." > health.kn

# Inject (safe - won't overwrite)
kain inject --ue5 health.kn

# Output: FHealth.h and FHealth.cpp added to Source/
# Master header updated automatically
```

### Workflow 3: Quick Test
```bash
# Test an actor quickly
echo "actor Test: state x: Int = 0" > test.kn
kain test.kn -t ue5

# Output: ATest.h and ATest.cpp in current directory
# Copy to plugin manually if needed
```

### Workflow 4: Preview Before Inject
```bash
# See what would be generated
kain inject --ue5 risky_change.kn --dry-run

# Review output, then commit
kain inject --ue5 risky_change.kn
```

---

## 🎨 What Gets Generated

### From `actor Player:`
- `APlayer.h` (header with UCLASS)
- `APlayer.cpp` (implementation)
- RPCs auto-configured (Server_*, Client_*, Multicast_*)
- Replication setup
- Tick function if needed

### From `@component struct Health:`
- `UHealthComponent.h` (UActorComponent subclass)
- `UHealthComponent.cpp` (implementation)
- @replicated fields → UPROPERTY(Replicated)
- @savegame fields → UPROPERTY(SaveGame)

### From `enum Rarity:`
- `ERarity` enum in header
- UENUM(BlueprintType) macro
- Display names for Blueprint

### From `@datatable struct Item:`
- `FItem` struct (FTableRowBase subclass)
- CSV import ready
- Blueprint accessible

### From `shader fragment MyShader:`
- `MyShader.usf` (HLSL file)
- `FMyShaderParameters` struct
- `IMPLEMENT_GLOBAL_SHADER` registration

### From `@slate struct MyWidget:`
- `SMyWidget.h` (SCompoundWidget subclass)
- SLATE_BEGIN_ARGS / SLATE_END_ARGS
- Construct() implementation

---

## 🚨 Error Handling

### "Plugin Source directory not found"
**Fix:** Run from plugin root (where .uplugin is) or use `--plugin-dir`

### "File conflicts detected"
**Fix:** Use `--force` to overwrite OR rename your .kn file

### "Could not find plugin directory"
**Fix:** Make sure .uplugin file exists OR use `--plugin-dir`

### "Type check failed"
**Fix:** Check your KAIN syntax (types, initializers, etc.)

### "Oracle validation failed"
**Fix:** Check UE5 naming conventions (RPC prefixes, state initializers, etc.)

---

## 💡 Pro Tips for LLMs

1. **Always use `inject` for existing plugins** - It's non-destructive by default
2. **Use `--dry-run` first** - Preview before committing
3. **No KAIN.toml needed** - Just put .kn files in directory and `kain build --ue5`
4. **Single file testing** - Use `-t ue5` for quick prototypes
5. **Conflict detection is your friend** - It prevents accidental overwrites
6. **Master header auto-updates** - No manual include management needed
7. **Oracle catches UE5 errors** - Trust the validation messages
8. **Modular output** - Each type gets its own .h/.cpp file

---

## 🎯 Decision Tree

```
Need to compile KAIN code?
│
├─ Testing single file?
│  └─ Use: kain file.kn -t ue5
│
├─ Building new plugin?
│  └─ Use: kain build --ue5
│
├─ Adding to existing plugin?
│  ├─ Want to preview first?
│  │  └─ Use: kain inject --ue5 file.kn --dry-run
│  │
│  └─ Ready to commit?
│     ├─ Safe mode (abort on conflicts)
│     │  └─ Use: kain inject --ue5 file.kn
│     │
│     └─ Force overwrite
│        └─ Use: kain inject --ue5 file.kn --force
│
└─ Not sure?
   └─ Use: kain inject --ue5 file.kn --dry-run
      (Preview is always safe)
```

---

## 📦 Output Structure

### Single File Mode (`-t ue5`)
```
current_dir/
├── AMyActor.h
└── AMyActor.cpp
```

### Full Build Mode (`build --ue5`)
```
MyPlugin/
├── MyPlugin.uplugin
├── Source/
│   ├── MyPlugin/
│   │   ├── Public/
│   │   │   ├── MyPlugin.h (master header)
│   │   │   ├── AMyActor.h
│   │   │   └── FMyStruct.h
│   │   ├── Private/
│   │   │   ├── MyPlugin.cpp
│   │   │   ├── AMyActor.cpp
│   │   │   └── FMyStruct.cpp
│   │   └── MyPlugin.Build.cs
│   └── MyPluginEditor/ (if editor items exist)
│       ├── Public/
│       ├── Private/
│       └── MyPluginEditor.Build.cs
└── Shaders/
    └── MyShader.usf
```

### Inject Mode (`inject --ue5`)
```
ExistingPlugin/
└── Source/
    ├── Public/
    │   ├── ExistingPlugin.h (updated with new includes)
    │   └── FNewComponent.h (NEW)
    └── Private/
        └── FNewComponent.cpp (NEW)
```

---

## 🔧 Environment Variables

- `KAIN_ROOT` - Override metadata directory location
- If not set, walks up from current directory to find `unreal/metadata/`

---

## ✅ Quick Validation

After any command, check:
1. ✅ No error messages
2. ✅ Files generated in expected locations
3. ✅ Master header updated (for inject mode)
4. ✅ No duplicate includes in master header

---

**That's it! Three commands, infinite power. Go build plugins. 🚀**
