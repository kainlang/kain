# Generated Outputs Guide

`generated/` is the disposable artifact bucket for Kain build outputs and validation payloads.

Use it to hold:

- compiled runtime or pipeline byproducts that are safe to delete and regenerate
- large proof outputs that do not belong in source trees
- transient logs from ad-hoc validation runs

Avoid keeping:

- committed binaries such as `.exe`, `.dll`, `.lib`, `.obj`, `.pdb`, `.ilk`, `.o`
- Cargo `target/` directories or `.kain` caches

If a log, screenshot, or validation report matters long-term, move it into `docs/validation/` or `docs/recent/` and link it from the relevant README.
