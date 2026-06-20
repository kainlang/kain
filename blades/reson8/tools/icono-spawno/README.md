# icono-spawno — reson8's Procedural Icon Creator

A Kain CLI tool that generates SVG icons programmatically. No external
dependencies, no raster conversion needed — clean resolution-independent
SVG icons for the reson8 DAW and any other Kain project.

**~50 icons across 9 categories**, designed on a 24x24 normalized grid
and scalable to any pixel size.

## Quick Start

```bash
# Generate a single icon
kain run icono-spawno -- --name play --size 256 --output icons/

# Generate all transport icons
kain run icono-spawno -- --category transport --size 256 --output icons/

# Generate the full suite
kain run icono-spawno -- --all --size 512 --output X:/blades/reson8/resources/icons/

# Custom color
kain run icono-spawno -- --name reson8_logo --color "#60a5fa" --size 1024 --output logos/
```

## Usage

```
kain run icono-spawno -- [OPTIONS]
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `--name <name>` | Generate a single icon by name | — |
| `--category <cat>` | Generate all icons in a category | — |
| `--all` | Generate the full icon suite (~50 icons) | — |
| `--size <N>` | Icon size in pixels | 256 |
| `--color <hex>` | Fill color (hex) | #e94560 |
| `--stroke <hex>` | Stroke color (hex) | #ffffff |
| `--stroke-width <N>` | Stroke width in grid units | 0.4 |
| `--bg <hex>` | Background color (hex) | transparent |
| `--output <dir>` | Output directory | icons/ |
| `--quiet` | Suppress progress output | — |
| `--help` | Show help message | — |

### Categories

| Category | Icons |
|----------|-------|
| `transport` | play, stop, pause, record, loop, skip_forward, skip_back |
| `mixer` | volume_high, volume_low, volume_mute, pan, fx, eq, compressor, reverb |
| `edit` | cut, copy, paste, undo, redo, delete, trim, split |
| `navigation` | folder, file, save, open, new, export |
| `tool` | zoom_in, zoom_out, selection, pencil, crosshair, grid |
| `status` | check, warning, error, info |
| `settings` | settings, search, menu, close |
| `midi` | midi, piano, note |
| `general` | reson8_logo, plugin, theme, python |

## Icon Design

### Grid System

All icons are designed on a **24x24 normalized grid** and scaled to the
target pixel size. This makes them resolution-independent — generate at
64, 256, 1024, or any other size, and they'll look identical.

Padding: ~2-4 grid units on each side depending on the icon shape.
Content area: roughly 20x20 units centered.

### Monochrome-First

Icons use a single fill color by default (reson8 accent red: `#e94560`)
with optional white stroke. They can be generated in any color, making
them suitable for:

- Light and dark themes
- Active/hover states (vary the color)
- Disabled states (use muted tones)
- Glass/frosted backgrounds

### Glass-Compatible

Since reson8 uses glass/liquid UI surfaces, icons use clean geometric
shapes with consistent stroke weight. No fine details that would get
lost on frosted glass backgrounds.

## Project Structure

```
tools/icono-spawno/
├── build.kn              # Build authority
├── README.md             # This file
└── src/
    ├── main.kn           # CLI entry point
    ├── icon_lib.kn       # Icon definitions (~50 icons)
    └── svg_primitives.kn # Low-level SVG element generators
```

### Architecture

```
main.kn (CLI dispatch)
    │
    ├── icon_lib.kn (icon definitions + batch generation)
    │       │
    │       └── svg_primitives.kn (SVG element builders)
    │
    └── std::fs, std::os_path (filesystem)
```

All modules are pure functions — no `world`, no `actor`, no state.
This is Layer 0 of the Kain decision ladder: plain `fn`, `struct`,
`let`, `while`, `if`, `return`.

## Examples

### Single Icon

```bash
kain run icono-spawno -- --name check --size 256 --output icons/
# → icons/check.svg
```

### Full Suite for reson8

```bash
kain run icono-spawno -- --all --size 512 --output X:/blades/reson8/resources/icons/
# → 48 .svg files
```

### Dark Theme Icons

```bash
kain run icono-spawno -- --category transport --color "#94a3b8" --stroke "#1e293b" --bg "#0f172a" --output dark_icons/
```

### Large Logo

```bash
kain run icono-spawno -- --name reson8_logo --size 2048 --color "#e94560" --bg "#1a1a2e" --output logos/
```

## Design Notes

- **SVG only** — no raster conversion needed. SVG works at any DPI.
- **No external dependencies** — pure string construction.
- **Coordinates in 0-1 range** — scaled by target size for resolution
  independence.
- **Consistent visual weight** — all icons use the same stroke width
  and level of detail.
- **Category system** — icons are grouped for batch generation.
- **Icon registry** — `icon_all()` returns all icon definitions with
  names and categories for programmatic use.

## Building

```bash
# Typecheck
kain check tools/icono-spawno/ --json

# Build native executable
kain build tools/icono-spawno/ --target llvm

# Run directly (no build step needed)
kain run tools/icono-spawno/ -- --all --size 256
```

## License

Part of the reson8 DAW project. See root LICENSE.
