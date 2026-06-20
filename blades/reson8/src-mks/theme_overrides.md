# ThemeOverrides

> Theme customization as markscript tables.
> Each `##` domain is one facet of the visual system
> (glass material, colors, typography, spacing).
> The reson8 bridge reads these tables on theme load
> and applies them on top of the active base theme.
>
> Float cells are inferred as `MARK_FLOAT` (e.g. `16.0`,
> `0.04`); boolean cells coerce to `Int 1/0` (`true` / `false`).
> Color strings like `#e94560` and `rgba(15,23,42,0.45)` are
> stored as `MARK_STRING` for the bridge to parse at runtime.

---

## GlassSettings
| Property | Value | Notes |
|----------|-------|-------|
| glass_blur_radius | 16.0 | Higher = more frosted |
| glass_blur_enabled | true | Master toggle |
| glass_noise_opacity | 0.04 | Grain texture amount |
| glass_refraction_strength | 0.02 | Subtle light bend |
| glass_tint_alpha | 0.35 | Backdrop darkening |
| glass_border_radius | 12.0 | Corner rounding |
| glass_shadow_blur | 24.0 | Drop shadow softness |
| glass_shadow_opacity | 0.18 | Drop shadow density |
| glass_highlight_intensity | 0.55 | Top edge specular |
| glass_animation_speed | 1.0 | Effect damping rate |

---

## ColorOverrides
| Property | Value |
|----------|-------|
| color_accent | #e94560 |
| color_accent_alt | #533483 |
| color_glass_bg | rgba(15,23,42,0.45) |
| color_glass_border | rgba(255,255,255,0.08) |
| color_text_primary | #f1f5f9 |
| color_text_secondary | #cbd5e1 |
| color_text_muted | #94a3b8 |
| color_surface | rgba(30,41,59,0.65) |
| color_surface_alt | rgba(51,65,85,0.55) |
| color_waveform | #38bdf8 |
| color_waveform_rec | #f87171 |
| color_grid | rgba(148,163,184,0.15) |
| color_selection | rgba(233,69,96,0.30) |
| color_clip_default | #facc15 |
| color_clip_selected | #fb923c |

---

## Typography
| Property | Value |
|----------|-------|
| font_family_ui | Inter |
| font_family_mono | JetBrains Mono |
| font_size_ui | 13.0 |
| font_size_small | 11.0 |
| font_size_large | 16.0 |
| font_weight_normal | 400 |
| font_weight_bold | 600 |
| line_height | 1.4 |
| letter_spacing | 0.0 |

---

## Spacing
| Property | Value |
|----------|-------|
| spacing_xs | 4.0 |
| spacing_sm | 8.0 |
| spacing_md | 16.0 |
| spacing_lg | 24.0 |
| spacing_xl | 32.0 |
| spacing_xxl | 48.0 |
| panel_padding | 12.0 |
| panel_gap | 8.0 |
| control_height | 32.0 |
| control_radius | 6.0 |

---

## Animation
| Property | Value |
|----------|-------|
| duration_fast | 80.0 |
| duration_normal | 160.0 |
| duration_slow | 320.0 |
| easing_default | ease-out |
| easing_bounce | spring |
| easing_pan | linear |
| fps_target | 60 |

---

## verify

```markscript
print("theme_overrides: 5 facets defined (GlassSettings, ColorOverrides, Typography, Spacing, Animation)")
print("theme_overrides: GlassSettings = 10 properties (Float / Bool)")
print("theme_overrides: ColorOverrides = 15 properties (hex + rgba strings)")
print("theme_overrides: Typography = 9 properties (font family + sizes + weights)")
print("theme_overrides: Spacing = 10 properties (4.0 to 48.0)")
print("theme_overrides: Animation = 7 properties (durations, easings, fps)")
```
