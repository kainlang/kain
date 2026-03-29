# KNOWN ISSUE: Graph Editor Module Separation

**Issue**: Graph editor factory files are currently generated in the runtime module (`Source/NarrativeGraph/`) instead of a separate editor module (`Source/NarrativeGraphEditor/`).

**Impact**: The plugin will compile and work, but the editor-only code is mixed with runtime code. This is not ideal for packaging (editor code shouldn't ship with packaged games).

**Root Cause**: `Kain/crates/cli/src/packager/ue5_pipeline.rs` lines 236-248 write factory files to `layout.source_dir` instead of creating a separate editor module directory.

**Workaround** (Manual):
1. Create `Source/NarrativeGraphEditor/` directory
2. Move `*Factory.h` and `*Factory.cpp` files there
3. Create `NarrativeGraphEditor.Build.cs` with editor dependencies
4. Update `.uplugin` to include editor module

**Proper Fix** (TODO):
Update packager to:
1. Detect `@graph_editor` definitions
2. Create separate `{PluginName}Editor` module
3. Write factory files to editor module
4. Generate editor `.Build.cs` with proper dependencies
5. Update `.uplugin` with editor module entry

**Status**: Known limitation, does not prevent plugin from working. Editor module separation is a packaging optimization, not a functional requirement for development.

**Reference**: See `Factory/_Archive/TitanGraph/TitanGraph/Source/TitanGraphEditor/` for proper structure.
