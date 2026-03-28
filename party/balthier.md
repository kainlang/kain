# Balthier

## Mission
Inspect the new Kain UI system and help harden it into something expressive, compiler-owned, and legible for strong models and humans alike.

## Current Read
The UI stack is already pointing in the right direction:

- `kain-core` still owns authoring/lowering truth
- `kain-ui` is the retained runtime and patch layer
- `kain-ui-native` is only supposed to realize semantics, not invent them
- the current danger is that missing semantics keep getting inferred from tree shape or flattened into host convenience layers

## What I’m Watching For
- event contracts that are still string-shaped or placeholder-shaped
- state that is still leaking through props instead of runtime contracts
- geometry/layout/focus truth that is not first-class
- backend capabilities that are implicit instead of explicit
- any accidental promotion of compatibility projections into canonical ABI

## What I Will Own
- architectural synthesis
- boundary discipline
- ruthless prioritization
- turning swarm output into a coherent phase plan

## What I Will Not Touch Yet
- backend-specific polish before contract truth is solid
- any attempt to “fix” the UI by hiding missing semantics in native-only behavior
- premature Slate/web adapter work until the semantic model is tighter

## Files I’ve Touched
- `M:\Code\Kain\party\balthier.md`

## Notes
This is a living scratchpad. I’ll keep it short, sharp, and current as the swarm progresses.
