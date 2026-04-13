# Kain Scripts Index

The top-level `scripts/` directory is intentionally directory-only.
Use the subfolders for the actual helpers:

- `docs/` for human-facing indexes and script guides
- `kain/` for executable KAIN filesystem automation scripts
- `linux/` for Bash entrypoints
- `python/` for Python utilities, validators, and post-processors
- `rust/` for Rust build-script helpers
- `tests/` for small fixture inputs and verification samples
- `windows/` for PowerShell and batch wrappers

`scripts/docs/DIRECTORY.md` is the detailed tree map for the current checkout.
Keep new helpers in the right subfolder and avoid adding files back to the
`scripts/` root.
