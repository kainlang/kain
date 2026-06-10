# Kaintana Screenshots

This directory contains BMP screenshots captured by the Kaintana screenshot
utility (`src/screenshot.kn`).

## File Naming

Each screenshot is saved with the following naming convention:

```
<prefix>_YYYYMMDD_HHMMSS_<epoch_ms>.bmp
```

| Part | Meaning |
|------|---------|
| `prefix` | Configurable prefix (default `screenshot`) |
| `YYYYMMDD` | UTC year, month, day |
| `HHMMSS` | UTC hour, minute, second |
| `epoch_ms` | Millisecond epoch timestamp (guarantees uniqueness) |

Example:

```
screenshot_20260609_143022_1752000000123.bmp
```

## Auto-Capture Configuration

The screenshot utility supports configurable auto-capture:

- **Interval-based**: Capture every N frames by setting
  `auto_capture_interval`.  For example, `60` captures once per second at
  60 FPS.
- **Event-triggered**: Capture on named events (`"activation"`, `"frame"`,
  `"shutdown"`) by setting `capture_on_event`.
- **Manual**: Call `kaintana_screenshot_capture()` at any point after a
  frame has been committed.

### Example

```kain
let cfg = kaintana_screenshot_init("screenshots/")
cfg.auto_capture_interval = 60       // every 60 frames
cfg.prefix = "kaintana"              // custom prefix
let result = kaintana_screenshot_auto(ctx, cfg, frame_index)
```

## Viewing BMP Files

BMP is a standard Windows bitmap format.  You can open these files with:

- **Windows**: Paint, Photos, File Explorer (thumbnail preview), or any
  image viewer.
- **Cross-platform**: GIMP, ImageMagick (`magick display`), or your
  browser (drag-and-drop).

## Programmatic Use

The screenshot utility is a Layer 0 module (`fn`, `struct`) with zero
dependencies on `world` / `entangle` / `patch`.  Call it from any
Kaintana context after `kaintana_commit_frame()`.

```kain
use screenshot::*

let cfg = kaintana_screenshot_init("screenshots/")
let _ok = kaintana_screenshot_capture(ctx, cfg)
```

For frame-loop auto-capture:

```kain
use screenshot::*

let cfg = kaintana_screenshot_init("screenshots/")
cfg.auto_capture_interval = 60
// In your frame loop:
let _result = kaintana_screenshot_auto(ctx, cfg, frame_index)
```
