# SM64 Fast3D Smoke

This smoke is an isolated Fast3D-style adapter lane for Kain research work.

Why it exists:
- it proves a display-list-driven native viewer path without polluting `crates/kain-3D`
- it keeps the command-stream, matrix stack, texture sampling, and combiner logic in a dedicated smoke crate
- it gives us a concrete target format that a future `sm64_all.kn` extractor can emit into
- the smoke folder is now a consumer of the backend crate `crates/kain-fast3d-runtime` instead of owning the adapter runtime itself

What it does today:
- loads a manifest-driven Fast3D scene description from `scene_manifest.json`
- can extract a real Mario face display-list subset from `actors/mario/model.inc.c` into `scene_manifest_title_face.json`
- resolves texture and display-list segment bindings through a small segment registry
- interprets display-list commands including matrix push/pop, vertex loads, textured triangles, and display-list calls
- compiles a small RDP-style combiner profile into the raster path
- maps a small but useful subset of SM64 combine modes (`G_CC_BLENDRGBFADEA`, `G_CC_SHADEFADEA`, `G_CC_SHADE`) into the smoke combiner contract
- launches a native viewer executable with keyboard orbit/zoom controls
- supports a headless snapshot mode for validation

Primary files:
- `scene_manifest.json`
- `scene_manifest_title_face.json`
- `crates/kain-fast3d-runtime/src/lib.rs`
- `crates/kain-fast3d-runtime/src/model.rs`
- `crates/kain-fast3d-runtime/src/runtime.rs`
- `crates/kain-fast3d-runtime/src/rasterizer.rs`
- `crates/kain-fast3d-runtime/src/extractor.rs`
- `crates/kain-fast3d-runtime/src/viewer.rs`
- `sm64_import_profile.render_us.json`
- `refresh_sm64_import.ps1`

Run it:

```bat
build_visual_exe.bat
launch_visual_exe.bat
```

The release launcher now builds the workspace crate `kain-fast3d-runtime` and copies its native executable into `outputs/`.

Generate a snapshot without opening the window:

```bat
capture_snapshot.bat
```

Extract and view the SM64 title-face scene:

```bat
extract_sm64_title_face.bat
launch_title_face_visual_exe.bat
```

Capture the current title-face scene directly to a PNG:

```bat
capture_title_face_snapshot.bat
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

Current title-face reality:
- the title-face smoke now uses real extracted Mario face geometry, lights, and display-list structure from `actors/mario/model.inc.c`
- the provided external SM64 checkout still does not include the original extracted title-screen texture blobs or a baserom, so the background card plus some facial textures are generated fallback assets rather than pixel-perfect Nintendo originals
- the current composition intentionally favors a truthful compiled extraction proof over exact title-screen parity; the next step for higher fidelity is wider texture/segment extraction, not moving N64-specific behavior into shared `kain-3D`

Native-hosting direction:
- the backend adapter now lives in `crates/kain-fast3d-runtime`, which exposes the CLI/runtime entrypoint that the smoke scripts call
- the crate supports an env-driven default manifest through `KAIN_FAST3D_MANIFEST`, which is the first clean step toward letting future native launchers host the adapter without the smoke folder being the runtime owner

Where Fabric fits cleanly:
- keep display-list extraction and frame rendering in this isolated adapter lane
- add Fabric later as an optional simulation lane for effects like water, cloth, or deformers after geometry and segment extraction are stable
- treat Fabric outputs as host-side buffers or textures consumed by the adapter, instead of making the base Fast3D path depend on simulation
