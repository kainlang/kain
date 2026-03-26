# SM64 Fast3D Smoke

This smoke is an isolated Fast3D-style adapter lane for Kain research work.

Why it exists:
- it proves a display-list-driven native viewer path without polluting `crates/kain-3D`
- it keeps the command-stream, matrix stack, texture sampling, and combiner logic in a dedicated smoke crate
- it gives us a concrete target format that a future `sm64_all.kn` extractor can emit into

What it does today:
- loads a manifest-driven Fast3D scene description from `scene_manifest.json`
- resolves texture and display-list segment bindings through a small segment registry
- interprets display-list commands including matrix push/pop, vertex loads, textured triangles, and display-list calls
- compiles a small RDP-style combiner profile into the raster path
- launches a native viewer executable with keyboard orbit/zoom controls
- supports a headless snapshot mode for validation

Primary files:
- `scene_manifest.json`
- `local_crate/src/model.rs`
- `local_crate/src/runtime.rs`
- `local_crate/src/rasterizer.rs`
- `local_crate/src/viewer.rs`
- `sm64_import_profile.render_us.json`
- `refresh_sm64_import.ps1`

Run it:

```bat
build_visual_exe.bat
launch_visual_exe.bat
```

Generate a snapshot without opening the window:

```bat
capture_snapshot.bat
```

Refresh the staged SM64 import from the live decomp tree:

```bat
refresh_sm64_import.bat
```

What the refresh lane does:
- uses the nested repo root at `M:\Code\Other\Research\sm64-master\sm64-master`
- imports a render-facing US/Fast3D-old subset into `generated/sm64_import_refresh_<tag>`
- keeps the import recipe data-driven in `sm64_import_profile.render_us.json`
- prints a compact summary of imported files and top failing groups after the run

Current import reality:
- normal `src/game`, `src/engine`, and related C imports are increasingly workable through `import-c`
- most `actors/*/geo.inc.c`, `actors/*/model.inc.c`, and `levels/**/model.inc.c` files still fail because their macro-expanded form is not normal C after preprocessing
- that means the clean next step for direct SM64 rendering is an adapter extractor for those inc-style display-list assets, not forcing N64 macro dialect deeper into the shared Kain pipeline

Where Fabric fits cleanly:
- keep display-list extraction and frame rendering in this isolated adapter lane
- add Fabric later as an optional simulation lane for effects like water, cloth, or deformers after geometry and segment extraction are stable
- treat Fabric outputs as host-side buffers or textures consumed by the adapter, instead of making the base Fast3D path depend on simulation
