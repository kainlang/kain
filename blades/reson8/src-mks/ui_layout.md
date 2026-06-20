# UILayout

> UI layout configuration as markscript tables.
> The `DefaultLayout` table is the canonical panel-dock map.
> Named routines (e.g. `MixingLayout`, `ArrangingLayout`)
> apply that layout at runtime through DAW-bridge intents
> registered in the 78-handler IVT (`set_panel_visible`,
> `set_panel_width`, `set_panel_dock`, `set_panel_height`).
>
> Boolean cells (`true`/`false`) are coerced to `Int 1/0`
> per the markscript table type system; `Width` and `Height`
> columns are `Int` when numeric, `String` when `-` is used
> to denote "fill remaining space".

---

## DefaultLayout
| Panel | Visible | DockSide | Width | Height |
|-------|---------|----------|-------|--------|
| mixer | true | right | 320 | - |
| browser | true | left | 240 | - |
| piano_roll | false | bottom | - | 200 |
| inspector | true | left | 260 | - |
| transport | true | top | - | 48 |
| status_bar | true | bottom | - | 24 |

---

## MixingLayout
> print "Applying mixing layout..."
> set_panel_visible mixer true
> set_panel_width mixer 420
> set_panel_visible browser false
> set_panel_dock mixer center
> print "Mixing layout applied"

### mixer_focus
| Panel | Visible | DockSide | Width | Height |
|-------|---------|----------|-------|--------|
| mixer | true | center | 420 | - |
| browser | false | left | - | - |
| piano_roll | false | bottom | - | - |
| inspector | false | left | - | - |
| transport | true | top | - | 48 |
| status_bar | true | bottom | - | 24 |

---

## ArrangingLayout
> print "Applying arranging layout..."
> set_panel_visible mixer false
> set_panel_visible browser true
> set_panel_visible piano_roll true
> set_panel_height piano_roll 400
> print "Arranging layout applied"

### arrange_focus
| Panel | Visible | DockSide | Width | Height |
|-------|---------|----------|-------|--------|
| mixer | false | right | - | - |
| browser | true | left | 320 | - |
| piano_roll | true | bottom | - | 400 |
| inspector | true | left | 260 | - |
| transport | true | top | - | 48 |
| status_bar | true | bottom | - | 24 |

---

## PerformanceLayout
> print "Applying performance layout..."
> set_panel_visible mixer true
> set_panel_visible piano_roll false
> set_panel_visible browser false
> set_panel_width mixer 800
> print "Performance layout applied"

### perf_focus
| Panel | Visible | DockSide | Width | Height |
|-------|---------|----------|-------|--------|
| mixer | true | center | 800 | - |
| browser | false | left | - | - |
| piano_roll | false | bottom | - | - |
| inspector | false | left | - | - |
| transport | true | top | - | 96 |
| status_bar | true | bottom | - | 24 |

---

## RecordingLayout
> print "Applying recording layout..."
> set_panel_visible mixer true
> set_panel_visible browser true
> set_panel_visible inspector true
> set_panel_dock inspector right
> print "Recording layout applied"

---

## verify

```markscript
print("ui_layout: 5 named layouts defined (default, mixing, arranging, performance, recording)")
print("ui_layout: DefaultLayout panel map = 6 rows (mixer, browser, piano_roll, inspector, transport, status_bar)")
print("ui_layout: dock sides = right / left / bottom / top / center")
```
