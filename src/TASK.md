# Source Rewrite Task Board

This file is the operator handoff for source ownership work under `src/`.

## Global Rules

- All hand-owned work goes under `src/<folder>`.
- Only `src/core` is an active hand-owned source lane right now.
- `src/.rustimport/reference` is the moved donor corpus from the older Rust import lane.
- `src/.rustimport/phase2` is the canonical live selfhost mirror root.
- `src/.rustimport/*` is reference-only. Do not edit it.
- `src/.legacy` is donor/reference-only. Do not edit it.
- Keep the language name as `Kain`. Do not introduce a rename campaign.
- Keep folder names aligned with the bootstrap/runtime shape where practical.
- Do not pull in UE5 surfaces for this wave.
- Do not create new owned `src/` lanes until `src/core` is materially stable.
- Prefer complete, owned Kain files over placeholders, but keep scope bounded to the assigned lane.
- If a lane depends on another unfinished lane, define the clean boundary and keep moving instead of blocking on unrelated work.

## Current Folder Plan

- `src/core`
- `src/.rustimport/reference`
- `src/.rustimport/phase2`
- `src/.legacy`

## Deferred Owned Lanes

These are future ownership targets only. Keep the folders absent for now.

- `src/driver`
- `src/sys-codegen`
- `src/interop`
- `src/c-ffi`
- `src/crate-ffi`
- `src/3d`
- `src/gpu-runtime`
- `src/host`

## Important Note About UI

For this wave, UI is deferred.

- Do not create `src/ui` as an active rewrite lane yet.
- Do not create `src/ui-native` as an active rewrite lane yet.
- Finish the language/runtime/native execution layers first.
- Bring UI back only after `src/core` is materially stable and the next owned lanes are intentionally opened.

## Active Assignment

### Agent Alpha

- Folder: `src/core`
- Task: Own the foundational language core.
- Deliver: `ast`, `span`, `diagnostic`, `error`, `lexer`, `parser`, `effects`, `types`, `comptime`, `runtime`, `stdlib`, `low_level_abi`, `low_level_memory`, `low_level_memory_metadata`, `kainc`.
- Goal: Make `src/core` the canonical owned semantic center.

## Future Lanes

- `src/driver`, `src/sys-codegen`, `src/interop`, `src/c-ffi`, `src/crate-ffi`, `src/3d`, `src/gpu-runtime`, and `src/host` remain deferred.
- Do not create those folders during this wave.
- When they are activated later, they must still treat `src/.rustimport/*` and `src/.legacy` as no-edit reference surfaces.

## Suggested Prompt Format

Use this pattern when delegating to another agent:

`You are Agent Alpha. Own src/core only. Do not edit src/.rustimport or src/.legacy. Your job is to translate the assigned lane into hand-owned Kain code, keep the name as Kain, skip UE5, and preserve clean boundaries with the other src folders.`
