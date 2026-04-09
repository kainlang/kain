# Kain Toolchain Folder Guide

This folder stores the vendored compiler toolchain required for Kain's native lanes.

## What Lives Here

- `llvm/` is the full LLVM toolchain drop used by the native build pipeline.

## Notes

- The LLVM binaries in `toolchain/llvm/bin/` are expected and should remain intact.
- The root `install_kain.py` bootstrapper is now the canonical way to repopulate this folder on fresh Linux, macOS, or Windows machines. It will either symlink or mirror the discovered host LLVM binaries into `toolchain/llvm/bin/` and write `toolchain/llvm/kain_bundle_manifest.json`.
- Update the toolchain as a full, versioned drop instead of piecemeal edits.
- Keep runtime outputs and app build artifacts out of this directory.
