# Fabric Pipeline

Snapshot: April 12, 2026.

Fabric is Kain's local-first orchestration layer for polyglot execution. It
models a workspace, a set of capability requirements, a dependency-ordered set
of steps, and a report session that records the run.

The CLI entrypoints are `kain fabric init [path] --template local|polyglot`,
`kain fabric validate --manifest <path>`, and `kain fabric run --manifest
<path>`.

## Manifest Shape

The canonical manifest is `KAIN.fabric.toml`.

The manifest model in `src/.rustimport/reference/kain-omni/fabric.kn` includes:

- `version`
- `workspace`
  - `root`
  - `search_roots`
- `requires`
- `steps`
- `reports`
  - `directory`
  - `emit_jsonl_events`

## Step Shape

Each step is explicit and data-driven. The core fields are:

- `id`
- `runtime`
- `entry`
- `module`
- `crate_name`
- `manifest_path`
- `library`
- `shader_source`
- `compute_key`
- `depends_on`
- `requires`
- `outputs`

The runtime kind determines which fields are required:

- `Kain` requires `entry`
- `Python` and `Node` require `entry` or `module`
- `RustCrate` requires `crate_name` and `entry`
- `CAbi` requires `library` and `entry`
- `GpuCompute` requires `shader_source`

## Runtime Kinds

The runtime lane for a step is explicit:

- `Kain`
- `Python`
- `RustCrate`
- `CAbi`
- `Node`
- `GpuCompute`

Each runtime kind also carries an implied capability key, so validation can tell
readers what the step depends on before execution starts.

The current capability keys are:

| Runtime kind | Capability key |
| --- | --- |
| `Kain` | `runtime.kain` |
| `Python` | `runtime.python` |
| `RustCrate` | `runtime.rust-crate` |
| `CAbi` | `runtime.c-abi` |
| `Node` | `runtime.node` |
| `GpuCompute` | `runtime.gpu-compute` |

## Contract Kinds

Fabric steps can produce different contract families:

- `SharedBuffer`
- `SharedImage`
- `Value`
- `ComputePlan`

Those output kinds are what let Fabric carry data across runtimes without
flattening everything into one generic blob.

The implied capability keys for output contracts follow the same pattern:

- `contract.shared-buffer`
- `contract.shared-image`
- `contract.value`
- `contract.compute-plan`

## Validation

Fabric validates the manifest before execution.

The validation result records:

- manifest path
- step count
- runtime counts
- required capability set

That means the manifest can fail early for missing capabilities or malformed
step structure before any runtime adapter starts work.

## Execution Order

Fabric executes in topological order.

For each step it tracks:

- dependencies
- blocked dependency failures
- runtime adapter selection
- produced outputs
- step status

If a dependency has not succeeded, the step is marked blocked instead of being
silently skipped.

## Runtime Adapters

The executor currently wires these adapters:

- Kain
- Python
- Rust crate
- C ABI
- Node
- GPU compute

That makes Fabric a true multi-runtime session engine rather than a single host
launcher.

## Starter Templates

`fabric init` can scaffold one of two current templates:

- `local` creates a single `Kain` step with `src/main.kn` and a `session.local`
  requirement
- `polyglot` creates a five-step local-first chain:
  - `python_source`
  - `kain_orchestrator`
  - `native_filter`
  - `rust_analyzer`
  - `node_packager`

The polyglot template is the best illustration of why Fabric exists. It shows
how Kain can coordinate Python, Kain, C ABI, Rust crate, and Node lanes while
preserving typed outputs such as `Value`, `SharedImage`, and `SharedBuffer`.

## Reports And Events

Each session writes a session directory under the report root.

Typical outputs include:

- `report.json`
- `session.lock.json`
- optional `events.jsonl`

The session emits event records for session start, step execution, blocked
dependencies, and session completion, which makes the pipeline inspectable
after the fact.

## Source Files To Read Next

- `src/.rustimport/reference/kain-omni/fabric.kn`
- `src/.rustimport/reference/kain-host/fabric.kn`
- `src/.rustimport/reference/cli/fabric.kn`
- `smoketest/fabric/`

## Practical Rule

Use Fabric when the question is “how do I execute a dependency-ordered
multi-runtime session and keep the report artifacts?” Use Omni when the
question is “how do I stage and emit mixed-language source and target outputs?”
