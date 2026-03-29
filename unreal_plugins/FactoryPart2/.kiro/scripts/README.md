# Factory Part 2 Assembly Line Scripts

This directory contains Python scripts for automating the plugin assembly line workflow.

## Scripts Overview

| Script | Purpose | Input | Output |
|--------|---------|-------|--------|
| `generate_plugin_spec.py` | Generate specification for a single plugin | Plugin ID from catalog | Complete specification files |
| `generate_all_specs.py` | Generate specifications for all 50 plugins | Plugin catalog | 50 plugin specifications + summary |
| `orchestrate_implementation.py` | Orchestrate plugin implementation with subagents | Plugin name + directory | Completed plugin implementation |
| `validate_plugin.py` | Validate completed plugin against quality gates | Plugin directory | Validation report |

## Prerequisites

- Python 3.7+
- KAIN compiler in PATH (`kain` command available)
- Plugin catalog at `FactoryPart2/.kiro/specs/factory-part-2-plugin-assembly-line/plugin_catalog.md`
- Specification templates at `FactoryPart2/.kiro/specs/factory-part-2-plugin-assembly-line/plugin_spec_template/`

## Usage

### 1. Generate Specification for Single Plugin

```bash
cd FactoryPart2/.kiro/scripts
python generate_plugin_spec.py <plugin_id> [output_base_dir]
```

**Examples**:
```bash
# Generate spec for Cinema4DMograph (Plugin 1.1)
python generate_plugin_spec.py 1.1

# Generate spec with custom output directory
python generate_plugin_spec.py 1.1 /custom/output/path
```

**Output**:
```
FactoryPart2/plugins/Cinema4DMograph/.kiro/specs/cinema4dmograph/
├── requirements.md
├── design.md
├── tasks.md
├── feature_checklist.md
└── KAIN.toml
```

### 2. Generate Specifications for All Plugins

```bash
cd FactoryPart2/.kiro/scripts
python generate_all_specs.py [output_base_dir]
```

**Examples**:
```bash
# Generate all 50 plugin specifications
python generate_all_specs.py

# Generate with custom output directory
python generate_all_specs.py /custom/output/path
```

**Output**:
- 50 plugin specification directories
- `FactoryPart2/.kiro/specs/factory-part-2-plugin-assembly-line/batch_generation_summary.md`

**Summary Report Includes**:
- Overall statistics (success rate, total time, average time per plugin)
- Results by domain
- Detailed results table
- Failed plugins with error details

### 3. Orchestrate Plugin Implementation

```bash
cd FactoryPart2/.kiro/scripts
python orchestrate_implementation.py <plugin_name> <plugin_dir> [max_parallel]
```

**Examples**:
```bash
# Orchestrate Cinema4DMograph with 3 parallel subagents
python orchestrate_implementation.py Cinema4DMograph ../plugins/Cinema4DMograph 3

# Orchestrate with 2 parallel subagents
python orchestrate_implementation.py VoxelSculptPro ../plugins/VoxelSculptPro 2
```

**What It Does**:
1. Parses `tasks.md` from plugin specification
2. Extracts phases and tasks
3. Groups tasks into batches (max_parallel at a time)
4. Spawns subagents for each task
5. Monitors subagent progress
6. Reports completion status

**Output**:
- Orchestration summary with timing and completion stats
- Marker files in `.kiro/temp/` for subagent coordination

### 4. Validate Completed Plugin

```bash
cd FactoryPart2/.kiro/scripts
python validate_plugin.py <plugin_dir> [min_loc] [min_compression]
```

**Examples**:
```bash
# Validate with default thresholds (5000 LOC, 1:15 compression)
python validate_plugin.py ../plugins/Cinema4DMograph

# Validate with custom thresholds
python validate_plugin.py ../plugins/Cinema4DMograph 8000 20.0
```

**Quality Gates**:
1. ✅ **No TODOs**: Zero TODO/FIXME/XXX comments
2. ✅ **No Placeholders**: Zero {{...}} or PLACEHOLDER markers
3. ✅ **No Simplifications**: Zero simplify/stub/mock patterns
4. ✅ **LOC Count**: Minimum 5000 lines (configurable)
5. ✅ **Compression Ratio**: Minimum 1:15 (configurable)
6. ✅ **KAIN Compilation**: `kain build --ue5 --dry-run` succeeds
7. ✅ **UE5 Plugin Structure**: Valid .uplugin and required files

**Output**:
- Validation report with pass/fail status for each gate
- Violation details (file:line:content) for failed checks
- Exit code 0 (success) or 1 (failure)

## Workflow Example

### Pilot Plugin (Cinema4DMograph)

```bash
cd FactoryPart2/.kiro/scripts

# Step 1: Generate specification
python generate_plugin_spec.py 1.1
# Output: FactoryPart2/plugins/Cinema4DMograph/.kiro/specs/cinema4dmograph/

# Step 2: Review generated specification
# Manually review requirements.md, design.md, tasks.md

# Step 3: Orchestrate implementation
python orchestrate_implementation.py Cinema4DMograph ../plugins/Cinema4DMograph 3
# Spawns 3 subagents to work on tasks in parallel

# Step 4: Validate completed plugin
python validate_plugin.py ../plugins/Cinema4DMograph
# Checks all quality gates
```

### Full Production (All 50 Plugins)

```bash
cd FactoryPart2/.kiro/scripts

# Step 1: Generate all specifications
python generate_all_specs.py
# Output: 50 plugin specifications + summary report

# Step 2: Review summary report
cat ../specs/factory-part-2-plugin-assembly-line/batch_generation_summary.md

# Step 3: Orchestrate plugins in parallel (manual coordination)
# Terminal 1:
python orchestrate_implementation.py VoxelSculptPro ../plugins/VoxelSculptPro 3

# Terminal 2:
python orchestrate_implementation.py HoudiniProcGen ../plugins/HoudiniProcGen 3

# Terminal 3:
python orchestrate_implementation.py BlenderBridge ../plugins/BlenderBridge 3

# Step 4: Validate each completed plugin
for plugin in ../plugins/*/; do
    python validate_plugin.py "$plugin"
done
```

## Script Architecture

### generate_plugin_spec.py

**Classes**:
- `PluginSpecGenerator`: Main generator class

**Methods**:
- `parse_catalog()`: Parse plugin_catalog.md
- `load_template()`: Load template file
- `generate_requirements()`: Generate requirements.md with EARS patterns
- `generate_design()`: Generate design.md with correctness properties
- `generate_tasks()`: Generate tasks.md with phase structure
- `generate_feature_checklist()`: Generate feature_checklist.md
- `generate_kain_toml()`: Generate KAIN.toml
- `validate_specification()`: Validate generated spec against rules
- `generate_specification()`: Main entry point

### generate_all_specs.py

**Classes**:
- `BatchSpecGenerator`: Batch generator class

**Methods**:
- `generate_all_specifications()`: Process all plugins
- `generate_summary_report()`: Generate markdown summary
- `save_summary_report()`: Save summary to file

### orchestrate_implementation.py

**Classes**:
- `Task`: Represents a task from tasks.md
- `Phase`: Represents a phase containing tasks
- `ImplementationOrchestrator`: Main orchestrator class

**Methods**:
- `parse_tasks()`: Parse tasks.md
- `group_tasks_for_parallel_execution()`: Group tasks into batches
- `spawn_subagent()`: Spawn subagent for task
- `monitor_agents()`: Monitor active subagents
- `execute_phase()`: Execute all tasks in phase
- `orchestrate()`: Main entry point

### validate_plugin.py

**Classes**:
- `ValidationResult`: Result of a validation check
- `PluginValidator`: Main validator class

**Methods**:
- `find_kain_files()`: Find all .kn files
- `find_generated_files()`: Find all generated C++ files
- `count_lines()`: Count LOC in files
- `check_forbidden_patterns()`: Check for forbidden patterns
- `validate_no_todos()`: Validate zero TODOs
- `validate_no_placeholders()`: Validate zero placeholders
- `validate_no_simplifications()`: Validate zero simplifications
- `validate_loc_count()`: Validate minimum LOC
- `validate_compression_ratio()`: Validate compression ratio
- `validate_kain_compilation()`: Validate KAIN compilation
- `validate_ue5_plugin()`: Validate UE5 plugin structure
- `run_all_validations()`: Main entry point

## Error Handling

All scripts include comprehensive error handling:
- File not found errors
- Parse errors
- Validation errors
- Subprocess errors
- Timeout errors

Exit codes:
- `0`: Success
- `1`: Failure

## Extending the Scripts

### Adding New Validation Checks

Edit `validate_plugin.py`:

```python
def validate_custom_check(self) -> ValidationResult:
    """Validate custom requirement."""
    # Your validation logic here
    return ValidationResult(
        check_name="Custom Check",
        passed=True,
        message="Check passed"
    )

# Add to run_all_validations():
checks = [
    # ... existing checks ...
    self.validate_custom_check,
]
```

### Adding New Template Sections

Edit `generate_plugin_spec.py`:

```python
def generate_custom_section(self, plugin: Dict) -> str:
    """Generate custom section."""
    template = self.load_template('custom_template.md')
    # Populate template
    return content

# Add to generate_specification():
custom_content = self.generate_custom_section(plugin)
with open(output_dir / 'custom.md', 'w') as f:
    f.write(custom_content)
```

## Troubleshooting

### "Catalog not found" Error
- Ensure you're running from `FactoryPart2/.kiro/scripts/`
- Check that `plugin_catalog.md` exists in `../specs/factory-part-2-plugin-assembly-line/`

### "Template not found" Error
- Ensure templates exist in `../specs/factory-part-2-plugin-assembly-line/plugin_spec_template/`
- Check template filenames match expected names

### "kain command not found" Error
- Ensure KAIN compiler is in PATH
- Run `kain --version` to verify installation

### Validation Failures
- Check violation details in validation report
- Fix issues in KAIN source files
- Re-run validation

## Future Enhancements

1. **Kiro Subagent API Integration**: Replace marker file coordination with Kiro's native subagent API
2. **Coordination State**: Add `coordination_state.json` for multi-orchestrator coordination
3. **Live Dashboard**: Connect HTML dashboard to live data from file system
4. **Feature Independence Analysis**: Implement graph coloring for optimal parallelization
5. **Build Queue System**: Add build serialization to prevent file lock issues
6. **Progress Persistence**: Save orchestration state to resume after interruption
7. **Metrics Collection**: Track detailed metrics (time per task, LOC per hour, etc.)
8. **Automated Retry**: Retry failed tasks with exponential backoff

## Support

For issues or questions:
1. Check this README
2. Review `PHASE_4_COMPLETION_SUMMARY.md`
3. Check script source code comments
4. Review specification rules in `specification_rules.md`

## License

Part of the KAIN Factory Part 2 project.
