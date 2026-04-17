# Element Architecture

Element is a Kain-authored language experiment that lives under `/home/ephemara/Dev/Kain/element`.

## Current Shape

- `src/element_kernel.kn` is the shared semantic kernel for all element modules.
- `src/oxygen.kn` defines the binary reduction / ownership-sink behavior for oxygen.
- `src/nitrogen.kn` defines the triadic control / branch-coordination behavior for nitrogen.

## Design Rule

Each element gets its own `.kn` file and owns its own structural validation plus semantic interpretation.

The shared kernel exists only for durable, cross-element contracts:

- node shape
- bond roles
- ownership/thread categories
- diagnostics
- generic validation helpers

## Current Semantics

- `O` is modeled as a two-bond reducer.
  It consumes at least one owner, emits one stabilized result, and rejects mutable shared-state behavior.
- `N` is modeled as a three-bond coordinator.
  It binds `condition`, `true lane`, and `false lane`, emits one selected result, and uses an implicit lone-pair scratch slot as hidden control context.

## Intended Growth Path

Add one file per element under `src/`.

Each file should provide:

- an `*_contract()` function
- a `*_default_node()` function
- a `*_validate()` function
- a compact explanation of how that element lowers

## Common Errors

- Do not duplicate bond/diagnostic enums in each element file; extend `element_kernel.kn` instead.
- Do not turn every element into a generic arithmetic node. The point is that each file owns a distinct semantic identity derived from chemistry.
- Keep memory/ownership/thread behavior data-driven through the shared contract struct instead of scattering hardcoded checks with no shared schema.
