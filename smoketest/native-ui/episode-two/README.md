# Native UI Episode Two Smoke

Large raw native UI proof for the LLVM lane.

This smoke keeps everything authored in one Kain file while crossing UI, input, native graphics, actors, and entangle/intent runtime hooks. The UI uses generic state payloads for non-rect shape, authored hit math, shader/canvas draw metadata, and graphics resource references. There is no runtime component catalog.

Current live texture note: this smoke uses the same stable 2x2 RGBA upload path as `pilot/`. Larger stretched raw UI textures exposed a Win32/GL heap-corruption bug during development, so keep the visual complexity in authored geometry/state until the texture upload/presenter path is hardened.

Run:

```powershell
.\run.ps1
```

For a window you can close manually:

```powershell
.\run.ps1 -Interactive
```

Generated outputs live under `outputs/` and are ignored.
