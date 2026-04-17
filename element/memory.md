# Element Memory

## 2026-04-17

Started the Element prototype as raw Kain code under `/home/ephemara/Dev/Kain/element/src`.

Durable decisions:

- Element modules are one-file-per-element.
- A tiny shared kernel is allowed for reusable node, bond, ownership, thread, and diagnostic contracts.
- Oxygen is the first reducer/ownership-sink element.
- Nitrogen is the first control/branch-coordination element.
- Oxygen and Nitrogen now each split into `physical_profile()` and `semantic_projection()` layers.
- `element_kernel.kn` now audits whether a semantic projection is justified by the underlying chemistry before node validation runs.

Why this shape:

- It matches the repo's selfhost direction instead of introducing a separate Rust-first prototype.
- It keeps the 118-file architecture viable without copy-pasting structural types into every element file.
- It gives future elements a stable contract for valency, memory width, ownership pull, and thread policy.
- It forces Element semantics to stay tied to actual chemistry instead of drifting into arbitrary symbolic opcodes.

Chemistry facts currently encoded for the first two exemplars:

- Oxygen: atomic number `8`, group `16`, mass `15.999`, electronegativity `3.44`, typical covalent bonds `2`, lone pairs `2`, common oxidation states `-1/-2`.
- Nitrogen: atomic number `7`, group `15`, mass `14.007`, electronegativity `3.04`, typical covalent bonds `3`, lone pairs `1`, common oxidation states `5/4/3/2/-3`.

Next recommended step:

Add `hydrogen.kn` and `carbon.kn` using the same audited two-layer model, then build a tiny dispatcher that routes an element symbol to the corresponding `*_contract_audit()` and `*_validate()` functions.
