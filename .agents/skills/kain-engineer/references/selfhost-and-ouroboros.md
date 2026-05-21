# Self-Hosting and Ouroboros

## When to Use This

Use the self-host workflow only when you are doing advanced compiler-facing work such as:

- importer evolution for compiler crates
- compiler AST or type-system changes
- round-trip validation of Rust -> Kain -> Rust
- Ouroboros inventory and stage2 pipeline maintenance

For normal app or gameplay code, you usually do not need this.

## Core Idea

Kain does not just compile user programs. It also has a reflexive bootstrap path where Rust compiler source is imported into Kain and then round-tripped back into Rust.

That is the heart of the Project Ouroboros flow.

## Base Reflexive Import Flow

At the lower level, the idea is:

```powershell
kain import-rust .\crates\kain-core\src --output kain-core.kn --flat
kain build kain-core.kn --target rust
```

The dedicated `selfhost` command builds a stricter, inventory-driven version of that workflow.

## `kain selfhost`

The current advanced commands are:

```powershell
kain selfhost phase1
kain selfhost phase2
```

### Phase 1 flags

- `--inventory-dir`
- `--output-dir`
- `--emit-bundles`

### Phase 2 flags

- `--inventory-dir`
- `--output-dir`
- `--emit-bundles`
- `--emit-roundtrip-rust`
- `--assemble-stage2`
- `--build-stage2`

## Current Default Paths

With repo root `M:\Code\Kain`, `selfhost.rs` currently derives these defaults:

- inventory dir
  - `M:\Code\OuroborosV2\docs\selfhost\inventories`
- phase1 output dir
  - `M:\Code\OuroborosV2\out\selfhost`
- phase2 output dir
  - `M:\Code\OuroborosV2\out\selfhost\phase2`

If you run from a different location or repo shape, verify the derived paths in source before assuming them.

## Inventory Files It Expects

The workflow expects these inventory files:

- `macro_inventory.json`
- `module_map.json`
- `selfhost_allowlist.json`
- `trait_inventory.json`

These are loaded from the inventory directory and drive what gets processed, preserved, rejected, or reported.

## What Phase Output Looks Like

When bundles are emitted, the workflow writes:

- `<crate>.kn` bundles for processed crates
- phase reports:
  - `phase1_report.json`
  - `phase1_report.md`
  - `phase2_report.json`
  - `phase2_report.md`

When round-trip Rust is enabled in phase 2, it also writes:

- `<crate>.roundtrip.rs`

When stage2 assembly is enabled, it prepares:

- `stage2_workspace\`

When stage2 build is enabled, it also writes:

- `stage2_build.log`
- a stage2-built `kain` artifact if the build succeeds

## What the Workflow Is Actually Doing

At a high level:

1. Find the Kain repo root.
2. Load inventories and strict self-host options.
3. Select the crate slice from the module map.
4. Import those crates from Rust into Kain.
5. Emit `.kn` bundles when requested.
6. Optionally compile those bundles back into Rust round-trip files.
7. Optionally assemble a stage2 workspace.
8. Optionally build stage2 and record reports.

## Important Practical Notes

- The CLI disables `include_tests` in self-host import options by default.
- Round-trip Rust generation requires the CLI to have the `sys` feature. The default CLI feature set currently includes it, but still verify with `kain doctor` if behavior seems odd.
- Phase 2 can rewrite package versions with a `-selfhost.0` suffix for stage2 manifests.
- The workflow is inventory-driven. If a crate is unexpectedly skipped or rejected, inspect the inventory files before changing compiler code.

## Recommended Usage Pattern

### Phase 1

Use phase 1 when you are checking whether a compiler slice can import cleanly into Kain and emit sane bundles.

```powershell
kain selfhost phase1
```

### Phase 2

Use phase 2 when you need the stricter round-trip and stage2 view.

```powershell
kain selfhost phase2 --emit-roundtrip-rust --assemble-stage2 --build-stage2
```

## When Debugging Failures

If self-hosting fails:

1. Check the report JSON and Markdown before patching code.
2. Inspect the inventory inputs and crate slice selection.
3. Confirm whether the failure happened during:
   - Rust import
   - bundle rendering
   - round-trip Rust generation
   - stage2 workspace assembly
   - stage2 cargo build
4. Only then decide whether the bug belongs in importer logic, frontend logic, or stage2 manifest rewriting.

## Source Files to Open

- `M:\Code\Kain\crates\cli\src\selfhost.rs`
- `M:\Code\Kain\crates\kain-import\src\rust\*.rs`
- `M:\Code\Kain\crates\cli\src\import_rust.rs`
- `M:\Code\Kain\crates\kain-import\CRATE_REFERENCE.md`

Self-hosting is too important to work from vague memory. Verify the current behavior from source whenever a decision is non-trivial.
