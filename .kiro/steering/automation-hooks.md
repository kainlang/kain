---
inclusion: always
---

# KAIN Automation & Hook System

## Overview

The KAIN pipeline includes 12+ automated hooks that provide continuous quality assurance, instant feedback, and self-healing capabilities. These hooks run automatically during development or can be triggered manually for deep analysis. In this codebase we have unlimited API tokens and unlimited power. Don`t worry about wasting tokens etc.

## Hook Categories

### 🔄 Auto-Trigger Hooks (Continuous Validation)

These hooks fire automatically when specific files are edited, providing instant feedback:

#### 1. Type System Consistency Checker
**Triggers:** When `types.rs` or editor codegen files are modified  
**Purpose:** Validates map_type() consistency across all codegen crates  
**Checks:**
- map_type() implementations match between ue5 and ue5-editor crates
- Pointer detection covers all UObject types
- EngineKnowledge lookups used instead of hardcoded lists
- Type conversions (Vec3→FVector, Vec4→FLinearColor) are consistent

#### 2. Oracle Coverage Checker
**Triggers:** When `oracle.rs` or `validation.rs` are modified  
**Purpose:** Ensures validation rules are comprehensive  
**Checks:**
- All UE5 naming collisions detected
- All shader validation rules present
- RPC naming conventions validated
- Component/Actor state validation complete
- No gaps in semantic checks

#### 3. Metadata Schema Validator
**Triggers:** When any `unreal/metadata/*.json` file is modified  
**Purpose:** Validates JSON against schemas  
**Checks:**
- JSON syntax is valid
- All required fields present
- Types match schema definitions
- No duplicate keys

#### 4. Test Fixture Regenerator
**Triggers:** When codegen files are modified  
**Purpose:** Auto-updates test fixtures to match new codegen output  
**Actions:**
- Identifies affected test fixtures
- Regenerates fixtures with --update-snapshots
- Reports which fixtures were updated

#### 5. Documentation Sync Enforcer
**Triggers:** When core codegen files are modified  
**Purpose:** Keeps documentation in sync with implementation  
**Checks:**
- docs/AGENT_HANDOFF.md reflects architecture changes
- .kiro/steering/*.md reflects pattern changes
- Inline Rust doc comments updated for API changes
- Crate README files updated for feature changes

#### 6. EngineKnowledge Propagator
**Triggers:** When `engine_knowledge.json` or `engine_knowledge.rs` are modified  
**Purpose:** Validates EngineKnowledge changes propagate correctly  
**Checks:**
- JSON is valid and schema-compliant
- Rust loader handles new fields
- All codegen crates can access new data
- Tests pass with new knowledge

#### 7. Naming Convention Enforcer
**Triggers:** When codegen files are modified  
**Purpose:** Catches inline prefix logic that bypasses naming.rs  
**Checks:**
- No `format!("A{}", name)` or similar inline logic
- All prefixing uses naming.rs functions
- Reports violations immediately

#### 8. Dependency Graph Validator
**Triggers:** When `module_graph*.json` files are modified  
**Purpose:** Detects circular dependencies  
**Actions:**
- Runs `validate_module_graph.py`
- Reports circular dependencies
- Validates module graph structure

#### 9. Auto Compile & Test
**Triggers:** When any Rust file in `crates/**/*.rs` is modified  
**Purpose:** Catches compilation errors immediately  
**Actions:**
- Runs `cargo check --all-targets`
- Runs `cargo test --package kain-core --package ue5 --package ue5-editor --package ue5-shaders --lib`
- Reports errors with file:line references

---

### 🎯 Manual Trigger Hooks (On-Demand Power Tools)

These hooks are triggered manually when you need deep analysis or testing:

#### 10. UE5 Integration Tester
**Trigger:** Manual (command palette or UI)  
**Purpose:** Full UE5 build test with actual compilation  
**Actions:**
- Builds SlateTest4 plugin with `kain build --ue5`
- Verifies all generated C++ files are syntactically correct
- Checks for common UE5 errors (missing includes, wrong macros, pointer issues)
- Reports codegen bugs with file:line references

#### 11. Metadata Auto-Expander
**Trigger:** Manual (command palette or UI)  
**Purpose:** Scans for missing UE5 types and expands metadata  
**Actions:**
- Scans all codegen files for hardcoded UE5 type references
- Cross-references with engine_knowledge.json, widget_registry.json, shader_knowledge.json
- Identifies missing types
- Runs appropriate expansion scripts to add them
- Reports what was added

#### 12. Performance Regression Detector
**Trigger:** Manual (command palette or UI)  
**Purpose:** Runs benchmarks to detect performance regressions  
**Actions:**
- Runs `cargo bench --package kain-core --package ue5 --package ue5-editor`
- Compares results to baseline
- Reports slowdowns

#### 13. Parallel Task Agent
**Trigger:** Manual (command palette or UI)  
**Purpose:** Splits large tasks across multiple subagents for parallel execution  
**Actions:**
- Analyzes task for independent operations
- Delegates subtasks to general-task-execution subagents
- Coordinates results after completion
- Max 5 parallel agents to avoid overload

#### 14. Optimize, Enhance & Debug
**Trigger:** Manual (command palette or UI)  
**Purpose:** Reviews completed work for improvements  
**Actions:**
- Identifies performance improvements
- Suggests 2-3 new features
- Looks for bugs and edge cases
- Fixes critical issues immediately

---

## Metadata Expansion Scripts

These Python scripts automatically expand the UE5 knowledge base when new types are referenced:

### expand_engine_knowledge.py
**Purpose:** Adds engine types, constructors, includes  
**Adds:**
- Type definitions (UStaticMeshComponent, FVector, etc.)
- Constructor formats (vec3(x,y,z) → FVector(x,y,z))
- Include paths (#include "Components/StaticMeshComponent.h")
- Property format strings (ImportText/ExportText)

### expand_widget_registry.py
**Purpose:** Adds Slate widget types and properties  
**Adds:**
- Widget class names (SButton, STextBlock, etc.)
- Widget properties (Text, OnClicked, etc.)
- Widget hierarchy (parent-child relationships)

### expand_shader_knowledge.py
**Purpose:** Adds shader types and parameters  
**Adds:**
- Shader stage types (Fragment, Compute, Vertex)
- Uniform types (Float, Vec3, Sampler2D)
- Permutation rules (CFG_*, ENABLE_*)

### expand_uht_rules.py
**Purpose:** Adds UHT macro generation rules  
**Adds:**
- UCLASS specifiers
- UPROPERTY specifiers
- UFUNCTION specifiers
- Replication rules

### validate_module_graph.py
**Purpose:** Validates module dependency graphs  
**Checks:**
- No circular dependencies
- All modules have valid dependencies
- Module graph is acyclic

---

## Hook Usage Patterns

### For LLMs

**During Development:**
1. Edit codegen file → Auto-compile hook runs → Instant feedback
2. Modify metadata → Schema validator runs → Catches errors immediately
3. Change types → Type consistency checker runs → Validates across crates

**For Deep Analysis:**
1. Trigger "UE5 Integration Tester" → Full build test
2. Trigger "Metadata Auto-Expander" → Scan for missing types
3. Trigger "Performance Regression Detector" → Check for slowdowns

**For Large Tasks:**
1. Trigger "Parallel Task Agent" → Split into subtasks
2. Multiple subagents work simultaneously
3. Coordinate results when complete

### For Humans

**Continuous Feedback:**
- Save a file → Hooks run automatically
- See errors immediately in terminal
- Fix and save again → Instant validation

**Manual Testing:**
- Open command palette (Ctrl+Shift+P)
- Search for hook name (e.g., "UE5 Integration Tester")
- Click to run
- Review results

---

## Hook Configuration

All hooks are stored in `.kiro/hooks/*.kiro.hook` as JSON files:

```json
{
  "enabled": true,
  "name": "Hook Name",
  "description": "What the hook does",
  "version": "1",
  "when": {
    "type": "fileEdited",  // or userTriggered, preToolUse, postToolUse
    "patterns": ["crates/**/*.rs"]  // file patterns to watch
  },
  "then": {
    "type": "askAgent",  // or runCommand
    "prompt": "What to do when triggered"
  }
}
```

### Hook Event Types
- `fileEdited` - Triggers when matching files are saved
- `fileCreated` - Triggers when matching files are created
- `fileDeleted` - Triggers when matching files are deleted
- `userTriggered` - Triggers manually via command palette
- `promptSubmit` - Triggers on every user message (use sparingly!)
- `agentStop` - Triggers when agent completes work (use sparingly!)
- `preToolUse` - Triggers before a tool is executed
- `postToolUse` - Triggers after a tool is executed

### Hook Action Types
- `askAgent` - Sends a prompt to the agent
- `runCommand` - Executes a shell command

---

## Best Practices

### ✅ Do:
- Use `fileEdited` for continuous validation
- Use `userTriggered` for expensive operations
- Keep hook prompts focused and specific
- Use `preToolUse` for access control only
- Test hooks after creation

### ❌ Don't:
- Use `promptSubmit` for heavy operations (token waste)
- Use `agentStop` for recursive operations (infinite loops)
- Create hooks that block all write operations (lockout risk)
- Forget to set timeouts on `runCommand` hooks
- Create circular dependencies between hooks

---

## Troubleshooting

### Hook Not Firing
- Check `"enabled": true` in hook file
- Verify file patterns match edited files
- Check hook event type matches action

### Hook Blocking Operations
- Disable hook temporarily: set `"enabled": false`
- Check for `preToolUse` hooks on broad categories
- Look for circular dependencies

### Hook Errors
- Check command syntax for `runCommand` hooks
- Verify prompt clarity for `askAgent` hooks
- Check timeout settings (default 60s)

---

## Future Enhancements

### Planned Hooks
- **Incremental Build Hook** - Only rebuild changed files
- **Hot Reload Hook** - Auto-reload UE5 plugin on changes
- **Marketplace Validator** - Check plugin meets marketplace requirements
- **Documentation Generator** - Auto-generate API docs from code
- **Changelog Generator** - Auto-update CHANGELOG.md from commits

### Planned Automation
- **Auto-fix Common Errors** - Automatically fix known error patterns
- **Smart Suggestions** - Context-aware code suggestions
- **Dependency Auto-Install** - Auto-add missing crate dependencies
- **Test Auto-Generation** - Generate tests from function signatures

---

## Summary

The hook system provides:
- **Instant Feedback** - Errors caught during file save
- **Comprehensive Coverage** - 12+ validation layers
- **Self-Healing** - Metadata auto-expands when gaps detected
- **Zero Manual Work** - All quality checks automated
- **Production Confidence** - If hooks pass, code is production-ready

This automation is critical for the LLM-first development philosophy - it ensures that LLM-generated code is production-ready without manual intervention.
