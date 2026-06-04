# FFmpeg Editor Gauntlet

This blade is an include-first C ABI stress test for FFmpeg. Kain owns the
editor state, timeline scoring, frame-memory ownership, fixture orchestration,
checksums, reporting, and optional presenter policy. C only owns the hard ABI
edges: FFmpeg contexts and a tiny Win32/GDI presenter.

## Requirements

Set the SDK root before building so the C-FFI metadata can resolve headers and
import libraries:

```powershell
$env:KAIN_PLATFORM_FFMPEG_SDK = "F:/Scoop/apps/ffmpeg-shared/current"
$env:PATH = "$env:KAIN_PLATFORM_FFMPEG_SDK/bin;$env:PATH"
```

## Commands

```powershell
kain check blades/c/ffmpeg --target llvm
kain build blades/c/ffmpeg --target llvm -o blades/c/ffmpeg/ffmpeg_editor_gauntlet.exe
blades/c/ffmpeg/ffmpeg_editor_gauntlet.exe .kain/fixtures/ffmpeg_gauntlet_testsrc.mp4 45
blades/c/ffmpeg/ffmpeg_editor_gauntlet.exe .kain/fixtures/ffmpeg_gauntlet_testsrc.mp4 90 --gui
```

The default run generates `.kain/fixtures/ffmpeg_gauntlet_testsrc.mp4` with the
installed `ffmpeg.exe` if it is missing, then decodes through the C ABI bridge.
