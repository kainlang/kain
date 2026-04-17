# Element Architecture

Element is a Kain-authored language experiment that lives under `/home/ephemara/Dev/Kain/element`.

## Current Shape

- `src/element_kernel.kn` is the shared chemistry/semantics kernel for all element modules.
- `src/oxygen.kn` defines the oxygen physical profile plus the audited ownership-sink projection.
- `src/nitrogen.kn` defines the nitrogen physical profile plus the audited branch-coordinator projection.

## Design Rule

Each element gets its own `.kn` file and owns its own structural validation plus semantic interpretation.

The shared kernel exists only for durable, cross-element contracts:

- node shape
- bond roles
- ownership/thread categories
- diagnostics
- generic validation helpers
- physical profiles
- semantic projections
- profile-to-projection audit logic

## Current Semantics

- `O` is modeled as a two-bond ownership sink derived from a chemistry profile with group `16`, six valence electrons, two typical covalent bonds, two lone pairs, electronegativity `3.44`, and common oxidation states `-1/-2`.
  Its projection audit enforces that the ownership-sink meaning is justified by high electronegativity, negative oxidation posture, and two-bond covalency.
- `N` is modeled as a three-bond branch coordinator derived from a chemistry profile with group `15`, five valence electrons, three typical covalent bonds, one lone pair, electronegativity `3.04`, and oxidation states spanning `-3` through `+5`.
  Its projection audit enforces that the branch-coordinator meaning is justified by three public bonds, one hidden lone-pair slot, and a broad oxidation range.

## Audit Rule

Every serious element file should be split into two layers:

- `physical_profile()`: periodic-table facts and bond-capacity facts
- `semantic_projection()`: the language meaning projected from those facts

The module validator should audit that the semantic projection is mechanically compatible with the physical profile before validating any concrete AST node.

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
- Do not hardcode semantic rules without a physical-profile audit. If the semantic projection cannot be defended from chemistry, the element file is not finished yet.
- Keep memory/ownership/thread behavior data-driven through the shared contract struct instead of scattering hardcoded checks with no shared schema.
