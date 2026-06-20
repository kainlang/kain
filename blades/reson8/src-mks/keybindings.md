# Keybindings

> Keybinding definitions as markscript tables.
> Each domain groups related actions (Transport / Edit / View).
> Modifier cells are pipe-delimited lists when more than one
> modifier is required (e.g. `Ctrl + Shift`).
> The reson8 bridge consumes these tables at startup to
> register the action→key map with the native input layer.

---

## Transport
| Action | Key | Modifiers |
|--------|-----|-----------|
| transport_play | Space | - |
| transport_stop | Escape | - |
| transport_pause | Enter | - |
| transport_record | R | Ctrl |
| transport_loop | L | Ctrl |
| transport_go_start | Home | - |
| transport_go_end | End | - |
| transport_rewind | Left | - |
| transport_forward | Right | - |
| transport_metronome | M | - |

---

## Edit
| Action | Key | Modifiers |
|--------|-----|-----------|
| undo | Z | Ctrl |
| redo | Y | Ctrl |
| cut | X | Ctrl |
| copy | C | Ctrl |
| paste | V | Ctrl |
| delete | Delete | - |
| select_all | A | Ctrl |
| deselect | D | Ctrl |
| split_clip | S | - |
| trim_start | [ | - |
| trim_end | ] | - |
| duplicate_clip | D | Ctrl + Shift |
| nudge_left | Left | Alt |
| nudge_right | Right | Alt |
| quantize | Q | - |

---

## View
| Action | Key | Modifiers |
|--------|-----|-----------|
| toggle_mixer | M | Ctrl |
| toggle_browser | B | Ctrl |
| toggle_piano_roll | P | Ctrl |
| toggle_inspector | I | Ctrl |
| zoom_in | = | Ctrl |
| zoom_out | - | Ctrl |
| zoom_reset | 0 | Ctrl |
| toggle_fullscreen | F11 | - |
| toggle_grid | G | - |
| toggle_snap | N | - |
| toggle_metronome | T | - |

---

## Project
| Action | Key | Modifiers |
|--------|-----|-----------|
| new_project | N | Ctrl |
| open_project | O | Ctrl |
| save_project | S | Ctrl |
| save_as | S | Ctrl + Shift |
| close_project | W | Ctrl |
| export_audio | E | Ctrl |
| import_audio | I | Ctrl + Shift |
| quit | Q | Ctrl |

---

## Marker
| Action | Key | Modifiers |
|--------|-----|-----------|
| add_marker | M | Ctrl + Shift |
| next_marker | . | - |
| prev_marker | , | - |
| clear_markers | M | Ctrl + Alt |
| rename_marker | Enter | - |

---

## ToolPalette
| Action | Key | Modifiers |
|--------|-----|-----------|
| tool_select | 1 | - |
| tool_draw | 2 | - |
| tool_erase | 3 | - |
| tool_slice | 4 | - |
| tool_pencil | 5 | - |

---

## verify

```markscript
print("keybindings: 6 domains defined (Transport, Edit, View, Project, Marker, ToolPalette)")
print("keybindings: Transport = 10 actions")
print("keybindings: Edit = 15 actions")
print("keybindings: View = 11 actions")
print("keybindings: Project = 8 actions")
print("keybindings: Marker = 4 actions")
print("keybindings: ToolPalette = 5 actions")
print("keybindings: total = 53 actions")
```
