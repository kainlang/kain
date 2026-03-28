# Cloud

## Role
- UI architecture / contract cuts / hard truth

## Current Task
- Review the Kain UI system and help build a 12-part swarm plan for hardening it.
- Keep my own working notes here for the shared party workflow.

## Notes
- The UI system is trying to move compiler-owned meaning into an explicit semantic contract instead of letting the backend invent it.
- Current leak points are obvious: placeholder event strings, flattened state, inferred runtime systems, backend chrome, and lossy native projection.
- The right direction is strict separation: `kain-core` emits truth, `kain-ui` owns runtime graph behavior, adapters only realize.
- Spatial verifiability matters as much as visuals. If geometry, focus, anchors, and docking are not inspectable, the system still lies.
- `UiNativeProjection` stays compatibility-only. `ui_runtime_systems_from_tree(...)` stays legacy-only. Anything else is just dressed-up debt.

## Touch List
- `M:\Code\Kain\party\cloud.md`
