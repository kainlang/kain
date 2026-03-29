## 2026-03-29 - Runtime lane map made more explicit

- Expanded `config/surfaces.json` so the `runtime_lane_map` inspector now spells out each lane separately: Kain, Fabric, Python, GPU compute, C ABI, Rust crate, and Node bridge.
- The new copy makes the app’s ownership model read more like a real multi-runtime DCC control room and less like a compressed summary row.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so the generated shell and live bridge snapshot stayed aligned after the inspector update.
- Clean extension seam: if we want live health instead of static ownership, the next step is to feed bridge/runtime telemetry into the lane map without moving semantic ownership out of Kain.

## 2026-03-29 - Fabric run green again after projection seam cleanup

- The manifest run for `cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml` is green again.
- The last real blockers were in `src/sculpt_brush_projection.kn` and `src/topology_history_projection.kn`.
- `sculpt_brush_projection.kn` had parser damage from a stray duplicate tail plus an unsupported `get_as::<...>` style call and a `log::info` runtime reference; it was simplified into a valid projection step that writes the brush report and returns a string.
- `topology_history_projection.kn` was reading non-existent `dcc_suite_seed` fields for active/topology mesh documents; it now derives those values from the existing mesh contract document so the step stays within the current seed contract.
- The run now completes with all 18 Fabric steps succeeding, so the remaining work on this lane is no longer a fabric blocker but downstream polish if needed.
