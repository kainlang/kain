# BuildConfig
# @schema "schemas/build_config.md"
#
# This file IS the build configuration for the self-host compiler.
# It is consumed by orchestrator.kn at runtime via markscript table
# queries. The orchestrator reads the Metadata table to populate its
# BuildConfig struct, then executes pipeline routines from buildex.md.
#
# Column keys are defined in build.kn (KEY_NAME, KEY_TARGET, etc.)
# and must match exactly. Values are the authoritative config defaults.

## Metadata

| Key | Value |
|-----|-------|
| name | kainc |
| target | llvm |
| profile | debug |
| optimize | false |
| lto | none |
| entry | main.kn |
| source_root | src/ |
| deps | std::runtime, std::machine, std::text, std::markscript, std::fs, std::collections, std::fmt, std::io, std::process |
| output | kainc |
| runtime | kain_runtime |
| linker | clang |
| linker_flags | -lkain_runtime |
| cc | clang |
| cc_flags | -c -std=c11 |
| test_root | spec/ |
| doc_root | docs/ |

## Release

| Key | Value |
|-----|-------|
| name | kainc |
| target | llvm |
| profile | release |
| optimize | true |
| lto | thin |
| entry | main.kn |
| source_root | src/ |
| deps | std::runtime, std::machine, std::text, std::markscript, std::fs, std::collections, std::fmt, std::io, std::process |
| output | kainc |
| runtime | kain_runtime |
| linker | clang |
| linker_flags | -lkain_runtime -flto=thin |
| cc | clang |
| cc_flags | -c -std=c11 -O2 |
| test_root | spec/ |
| doc_root | docs/ |
