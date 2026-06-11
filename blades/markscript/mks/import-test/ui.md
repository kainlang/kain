# HexColorMixer — Interactive Color Tool

> Pure Kain std::ui — real text input, live color preview, preset swatches.
> Type hex codes (e.g. "FF8040") to see the color update in real time.
> Click preset swatches to pick common colors.
> No C interop. No blade dependencies. Just Kain.

---

## Window

| Property | Value |
|----------|-------|
| Title | Kain UI Template — Hex Color Mixer |
| Width | 820 |
| Height | 620 |
| Backend | winit |
| FontTitle | Segoe UI 22 |
| FontBody | Segoe UI 15 |
| FontMono | Consolas 16 |

---

## Layout

| Region | X | Y | W | H | Purpose |
|--------|---|---|---|---|---------|
| TitleBar | 0 | 0 | 820 | 46 | Project name and subtitle |
| ColorPreview | 20 | 90 | 280 | 280 | Live color swatch |
| RGBReadout | 20 | 398 | 280 | 24 | Hex + RGB display |
| HexInput | 330 | 118 | 340 | 42 | Type hex codes here |
| Presets | 330 | 210 | 440 | 140 | Clickable color swatches |
| Actions | 330 | 370 | 440 | 70 | Apply + Random buttons |
| History | 330 | 460 | 440 | 60 | Last 4 colors |
| Status | 20 | 590 | 780 | 18 | Status messages |

---

## Presets — Default Color Swatches

| Index | Label | R | G | B | Hex |
|-------|-------|---|---|---|-----|
| 0 | Red | 220 | 50 | 50 | DC3232 |
| 1 | Orange | 240 | 140 | 30 | F08C1E |
| 2 | Yellow | 240 | 220 | 30 | F0DC1E |
| 3 | Green | 50 | 200 | 60 | 32C83C |
| 4 | Cyan | 40 | 200 | 200 | 28C8C8 |
| 5 | Blue | 50 | 80 | 220 | 3250DC |
| 6 | Purple | 160 | 50 | 220 | A032DC |
| 7 | Pink | 240 | 80 | 160 | F050A0 |
| 8 | White | 245 | 245 | 245 | F5F5F5 |
| 9 | Gray | 128 | 128 | 128 | 808080 |
| 10 | Dark | 30 | 30 | 40 | 1E1E28 |
| 11 | Teal | 20 | 180 | 140 | 14B48C |

---

## Input Handling

| Event | KeyCode | Action |
|-------|---------|--------|
| MousePress | — | Focus hex input field |
| MousePress | — | Click preset swatch → apply color |
| MousePress | — | Click Apply button → parse hex |
| MousePress | — | Click Random button → generate color |
| KeyPress | 8 / 259 | Backspace — delete last hex digit |
| KeyPress | 13 | Enter — apply current hex |
| KeyPress | 27 | Escape — reset to default gray |
| KeyPress | 0-9 A-F a-f | Insert hex digit at cursor |

---

## State Machine

```markscript
let color_r = 128
let color_g = 128
let color_b = 128
let hex_input = "808080"
let hex_index = 6
let status = "Type a hex code (e.g. FF8040) or click a preset"
```

| State | Trigger | Transition |
|-------|---------|------------|
| Default | Startup | RGB(128,128,128), hex="808080" |
| InputFocused | Click hex field | status="Input field focused" |
| HexTyped | KeyPress (hex char) | Insert char at cursor, increment index |
| HexDeleted | KeyPress (backspace) | Remove char before cursor |
| ColorApplied | Click Apply or Enter | Parse hex → RGB, push to history |
| RandomGenerated | Click Random | Generate pseudo-random color |
| PresetSelected | Click swatch | Load preset RGB, update hex |
| Reset | Escape | Default gray, clear history focus |

---

## Module Map

| File | Purpose | Exports |
|------|---------|---------|
| src/main.kn | Entry point, event loop, rendering | main() |
| src/color.kn | RGB struct, hex parse/format, presets | Rgb, rgb_new, parse_hex_rgb, rgb_to_hex, preset_rgb |
| src/input.kn | Hex input state machine, validation | hex_insert, hex_delete, is_hex_char, build_display_hex |
| src/ui.kn | Reusable UI drawing helpers | render_button, render_filled_box, render_label |

---

## Invariants

| # | Invariant |
|---|----------|
| 1 | All rendering is pure Kain std::ui — no C, no interop, no blade deps |
| 2 | Hex input is always exactly 6 characters (or fewer during editing) |
| 3 | History ring is FIFO — newest color pushes oldest out |
| 4 | Presets are immutable — loaded from a compile-time const table |
| 5 | Every frame: pump → begin frame → render → poll events → present |
| 6 | Color preview always matches hex_input after Apply/Enter |
| 7 | Status bar always reflects the last user action |

---

> This file IS the UI specification. Every table is parsable data.
> Every intent maps to a Kain handler through the IVT.
> The documentation and the program are the same artifact.
