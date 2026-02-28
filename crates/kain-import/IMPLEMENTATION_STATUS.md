# Implementation Status

## Summary

`kain-import` is no longer just scaffolded. The C importer is live, CLI-wired, and usable for real transliteration work.

The old status of "architecture complete but blocked by 128 compile errors" is obsolete.

## Current State

### Working now

- `kain.exe import-c <file.c> --output <file.kn>`
- `kain.exe import-c <dir> --output <merged.kn>`
- directory import with per-file `mod` wrapping
- `--flat` merged output mode
- `--include` / `--exclude` filtering
- `--fail-fast`
- `--report-json`
- direct compile after import with `--target`
- generated KAIN compiling through downstream backends in real cases

### Recently hardened areas

- reserved-keyword sanitization with stable rename alignment
- address-of and dereference lowering
- module-aware typechecking for imported programs
- fixed-size local array lowering
- sequence-correct increment/decrement lowering
- recovery of anonymous typedef structs into named KAIN types

## Current Quality Bar

The importer is strong enough for:

- real C-to-KAIN experiments
- large subset imports
- backend smoke compilation
- iterative import on old engines, decompilations, and runtime code

The importer is not yet strong enough to claim:

- perfect C semantic parity
- full self-hosting fidelity
- full build-system/preprocessor compatibility for arbitrary projects

## Main Remaining Gaps

- pointer arithmetic fidelity
- deeper storage/layout semantics
- macro/build-configuration heavy projects
- broader regression coverage across large C codebases

## Recommended Status Label

Use this wording for the project right now:

> C importer is active and useful, with working end-to-end CLI and backend output, but still in semantics-hardening rather than full-parity mode.

## Primary Doc

For actual usage and workflow, see:

- [C_IMPORT_PIPELINE.md](./C_IMPORT_PIPELINE.md)
