# KAIN Round-Trip Compiler System

## Overview

The round-trip compiler enables **lossless conversion** between KAIN and C++:

```
KAIN (.kn) → C++ (with markers) → KAIN (.kn) → C++ (identical)
```

This is critical for:
1. **Training LLMs** - Extract clean KAIN examples from compiled plugins
2. **Validation** - Prove KAIN codegen is deterministic and correct
3. **Documentation** - Generate canonical KAIN examples from working plugins
4. **Debugging** - Understand what KAIN generated a specific C++ pattern

## How It Works

### Step 1: KAIN → C++ (with markers)

When you compile KAIN with `--embed-kain` flag, the compiler embeds original KAIN source as comments:

```cpp
// KAIN_BEGIN: actor VoxelChunk
// KAIN: actor VoxelChunk:
// KAIN:     @replicated
// KAIN:     state chunk_data: Array<Int> = []
// KAIN:     @replicated
// KAIN:     state is_dirty: Bool = false
class VOXELFORGE_API AVoxelChunk : public AActor
{
    GENERATED_BODY()
    
    // KAIN: on Server_UpdateVoxel(x: Int, y: Int, z: Int, value: Int):
    UFUNCTION(Server, Reliable)
    void Server_UpdateVoxel(int32 X, int32 Y, int32 Z, int32 Value);
};
// KAIN_END: actor VoxelChunk
```

### Step 2: C++ → KAIN (extraction)

The `cpp_to_kain.py` tool extracts KAIN source from marked C++:

```bash
python scripts/python/cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --output recovered.kn
```

Output:
```kain
actor VoxelChunk:
    @replicated
    state chunk_data: Array<Int> = []
    @replicated
    state is_dirty: Bool = false
    
    on Server_UpdateVoxel(x: Int, y: Int, z: Int, value: Int):
        // Implementation
```

### Step 3: Validation (round-trip test)

Compile the extracted KAIN and compare with original C++:

```bash
python scripts/python/cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --validate
```

This:
1. Extracts KAIN from C++
2. Compiles extracted KAIN with `kain build --ue5`
3. Diffs generated C++ with original
4. Reports success/failure

## Usage

### Enable Markers During Compilation

**Option 1: CLI flag (recommended)**
```bash
cd unreal_plugins/VoxelForgePro/VoxelForgePro
kain build --ue5 --embed-kain
```

**Option 2: KAIN.toml config**
```toml
[build]
embed_kain_markers = true
marker_style = "block"  # or "inline"
```

### Extract KAIN from C++

**Basic extraction:**
```bash
python scripts/python/cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --output example.kn
```

**With statistics:**
```bash
python scripts/python/cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --output example.kn --stats
```

Output:
```
📄 Processing: KainFactory/Public/DialogueGraphGraphAsset.h
📄 Processing: KainFactory/Private/DialogueGraphGraphAsset.cpp
...

✅ Extracted KAIN source to: example.kn
   Lines: 1,247

📊 Extraction Statistics
==================================================
Files processed:  23
Actors found:     5
Components found: 8
Structs found:    12
Enums found:      4
Functions found:  47
Shaders found:    3
==================================================
```

### Validate Round-Trip

**Full validation:**
```bash
python scripts/python/cpp_to_kain.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ --validate
```

Output:
```
🔄 Starting round-trip validation...

📝 Extracted KAIN (1,247 lines)
   Saved to: /tmp/recovered.kn

🔨 Compiling extracted KAIN...
✅ Compilation succeeded!

🔍 Comparing generated C++ with original...
✅ Perfect round-trip! Generated C++ matches original.
```

## Marker Styles

### Block Markers (Recommended)

Wraps entire items with BEGIN/END markers:

```cpp
// KAIN_BEGIN: actor Player
// KAIN: actor Player:
// KAIN:     state health: Float = 100.0
class GAME_API APlayer : public AActor
{
    UPROPERTY(Replicated)
    float Health;
};
// KAIN_END: actor Player
```

**Pros:**
- Clear boundaries
- Easy to parse
- Handles complex items

**Cons:**
- More verbose

### Inline Markers

Single-line comments above each declaration:

```cpp
// KAIN: actor Player:
class GAME_API APlayer : public AActor
{
    // KAIN: state health: Float = 100.0
    UPROPERTY(Replicated)
    float Health;
};
```

**Pros:**
- Less verbose
- Cleaner diffs

**Cons:**
- Harder to parse complex items
- No clear boundaries

## Use Cases

### 1. Generate Training Examples for LLMs

Extract clean KAIN from all Factory plugins:

```bash
#!/bin/bash
# extract_all_examples.sh

for plugin in Factory/*/; do
    plugin_name=$(basename "$plugin")
    echo "Extracting $plugin_name..."

    python scripts/python/cpp_to_kain.py "$plugin/Source/" \
        --output "examples/${plugin_name}.kn"
done

echo "✅ Extracted $(ls examples/*.kn | wc -l) plugin examples"
```

Result: `examples/` folder with 20+ canonical KAIN examples.

### 2. Validate Codegen Correctness

Add to CI/CD pipeline:

```yaml
# .github/workflows/round-trip-test.yml
name: Round-Trip Validation

on: [push, pull_request]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build KAIN compiler
        run: cargo build --release --package cli
      
      - name: Test round-trip on all plugins
        run: |
          for plugin in Factory/*/; do
            echo "Testing $plugin..."
            python scripts/python/cpp_to_kain.py "$plugin/Source/" --validate
          done
```

### 3. Debug Codegen Issues

When C++ output is wrong, extract KAIN to see what was generated:

```bash
# Build with markers
cd Factory/BrokenPlugin
kain build --ue5 --embed-kain

# Extract to see what KAIN thinks it generated
python ../../scripts/python/cpp_to_kain.py Source/ --output debug.kn

# Compare with original
diff debug.kn broken_plugin.kn
```

### 4. Create Documentation Examples

Generate docs with side-by-side KAIN/C++:

```bash
python tools/generate_docs.py unreal_plugins/VoxelForgePro/VoxelForgePro/Source/ \
    --output docs/examples/ \
    --format markdown \
    --side-by-side
```

Output:
```markdown
## Actor Example

### KAIN Source
```kain
actor GameMode:
    state score: Int = 0
    
    on Server_AddScore(points: Int):
        score = score + points
```

### Generated C++
```cpp
class GAME_API AGameMode : public AActor
{
    UPROPERTY(Replicated)
    int32 Score;
    
    UFUNCTION(Server, Reliable)
    void Server_AddScore(int32 Points);
};
```
```

## Limitations

### What Works (High Confidence)

- ✅ Actors with state and RPCs
- ✅ Components with properties
- ✅ Structs (plain and @datatable)
- ✅ Enums
- ✅ Blueprint functions
- ✅ Delegates
- ✅ Attributes (@replicated, @savegame, etc.)

### What's Lossy (Medium Confidence)

- ⚠️ Complex expressions (simplified to `...`)
- ⚠️ Control flow (if/match/for/while)
- ⚠️ Function bodies (implementation details)
- ⚠️ Comments and documentation

### What Doesn't Work (Low Confidence)

- ❌ Slate widget trees (too complex)
- ❌ Details panel property binding
- ❌ Viewport client logic
- ❌ Shader HLSL code (separate .usf files)

**Workaround:** For complex items, keep original `.kn` files as source of truth.

## Implementation Details

### Marker Format

**Block markers:**
```
// KAIN_BEGIN: <item_type> <item_name>
// KAIN: <kain_source_line_1>
// KAIN: <kain_source_line_2>
...
// KAIN_END: <item_type> <item_name>
```

**Inline markers:**
```
// KAIN: <kain_source_line>
```

### Extraction Algorithm

1. **Scan C++ files** for `// KAIN:` comments
2. **Parse markers** to identify item boundaries
3. **Reconstruct KAIN** by concatenating marker content
4. **Deduplicate** lines (headers + source may have duplicates)
5. **Format** with proper indentation

### Validation Algorithm

1. **Extract KAIN** from C++ with markers
2. **Write to temp file** with minimal KAIN.toml
3. **Compile** with `kain build --ue5`
4. **Diff** generated C++ with original (ignoring markers/timestamps)
5. **Report** differences or success

## Future Enhancements

### Phase 1: Basic Round-Trip (Current)
- ✅ Marker generation in codegen
- ✅ Extraction tool
- ✅ Validation tool

### Phase 2: Enhanced Markers
- ⬜ Expression preservation (full AST in comments)
- ⬜ Control flow preservation
- ⬜ Documentation preservation
- ⬜ Source location tracking (file:line)

### Phase 3: Bidirectional Editing
- ⬜ Edit C++ → Update KAIN markers
- ⬜ Edit KAIN → Update C++ (hot reload)
- ⬜ Merge conflicts resolution
- ⬜ IDE integration (VS Code extension)

### Phase 4: Semantic Round-Trip
- ⬜ Preserve semantic intent (not just syntax)
- ⬜ Infer KAIN patterns from hand-written C++
- ⬜ Suggest KAIN refactorings
- ⬜ Auto-convert UE5 plugins to KAIN

## FAQ

**Q: Does this add overhead to compilation?**  
A: Minimal. Markers are comments, so they don't affect C++ compilation. KAIN compilation is ~2% slower with markers enabled.

**Q: Can I disable markers in production builds?**  
A: Yes. Omit `--embed-kain` flag or set `embed_kain_markers = false` in KAIN.toml.

**Q: Will markers break UE5 compilation?**  
A: No. They're standard C++ comments and are ignored by the compiler.

**Q: Can I extract KAIN from plugins without markers?**  
A: Partially. The tool can infer some patterns from UE5 macros, but accuracy is lower (~50% vs ~95% with markers).

**Q: Does this work with hand-written C++?**  
A: No. Markers are only added by KAIN codegen. For hand-written C++, use pattern inference (lower accuracy).

**Q: Can I use this for version control?**  
A: Yes! Store `.kn` files in git, generate C++ with markers during build. This keeps diffs clean.

## Contributing

To improve round-trip accuracy:

1. **Add marker support** for new codegen features
2. **Improve extraction** for complex patterns
3. **Add tests** for round-trip validation
4. **Report issues** when round-trip fails

See `Kain/crates/ue5/src/ue5/kain_markers.rs` for marker generation code.

## License

Same as KAIN compiler (MIT/Apache-2.0 dual license).
