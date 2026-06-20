# Markscript in reson8 — Embedded Scripting Engine

reson8 embeds the full [MarkScript](MARKSCRIPT.MD) bytecode VM as its built-in scripting language. Your documentation IS your program. Markdown files are executable.

## What This Gives reson8

| Feature | How |
|---------|-----|
| **Plugin pipeline** | `.mks` / `.md` files as plugins — markdown-native DSP chains, automation scripts |
| **UI configuration** | Layout presets, docking configs, component visibility — all in markdown tables |
| **Keybindings** | Per-action keymaps stored as readable markdown, not binary blobs |
| **Theme overrides** | Color/font/spacing overrides in markdown — no Kain knowledge needed |
| **Automation** | Transport control, export orchestration, batch processing — all via intents |
| **Interactive console** | Built-in markscript REPL for live DAW control |
| **Config→Code generation** | `mks gen config.md --target kain` generates Kain structs from tables |

## Quick Start

```bash
# Run a markscript automation
kain run reson8 -- --mks scripts/export_session.md

# Load UI layout from markdown tables
# In reson8: Settings → Layout → Import from markscript...

# Create a markscript plugin
# plugins/my_effect.md:
#   # MyEffect
#   > apply_gain 0.75
#   > apply_reverb room=medium
```

## Architecture

```
reson8/
├── packages/markscript/          ← Amalgamated markscript compiler
│   ├── markscript.kn             ← Full VM (lexer, parser, VM, JIT, 78 handlers)
│   ├── std/                      ← 100+ markscript stdlib modules
│   └── docs/                     ← Language reference
│
├── src/bridge/markscript_bridge.kn  ← reson8 ↔ markscript integration
│   ├── mks_load_file()           ← Load + execute .md files
│   ├── mks_eval_intent()         ← One-shot intent dispatch
│   ├── mks_read_table()          ← Extract data tables
│   ├── mks_plugin_load/run()     ← Plugin pipeline
│   ├── mks_load_ui_config()      ← UI layout from markdown
│   ├── mks_load_keybindings()    ← Keymaps from markdown
│   ├── mks_load_theme_override() ← Theme overrides from markdown
│   ├── mks_automation_script()   ← Transport/export automation
│   ├── mks_console*()            ← Interactive REPL console
│   └── MarkscriptPluginActor     ← Async markscript actor
│
└── config/                       ← DAW config in markdown (future)
    ├── ui_layout.md              ← Panel docking config
    ├── keybindings.md            ← Action keymaps
    └── theme_overrides.md        ← Per-project theme tweaks
```

## Intent Categories Available (78 handlers)

| Category | Count | Examples |
|----------|-------|----------|
| File I/O | 8 | `read`, `write`, `exists`, `mkdir`, `readdir`, `stat`, `touch`, `chmod` |
| String | 9 | `concat`, `split`, `join`, `substr`, `replace`, `upper`, `lower`, `trim`, `contains` |
| Math | 8 | `sin`, `cos`, `sqrt`, `abs`, `min`, `max`, `clamp`, `random` |
| JSON | 2 | `parse`, `stringify` |
| Time | 2 | `time`, `sleep` |
| Process | 10 | `run`, `spawn`, `await`, `kill`, `exitcode`, `stdout`, `stderr`, `pipe`, `env`, `cwd` |
| UI | 8 | `click`, `key`, `focus`, `close`, `find`, `set`, `get`, `create` |
| Template | 1 | `template` |
| Core | 3 | `print`, `assert`, `import` |
| Random | 6 | `randint`, `randfloat`, `randrange`, `randfrange`, `maybe`, `diceroll` |

## Example: Markscript Plugin

```markdown
# Reson8Reverb

## init
> print "Loading Reson8Reverb v1.0"

## process
> read "audio_buffer"
> apply_reverb room_size=0.7 damping=0.4 width=1.0
> write "audio_buffer"

## params
| Param      | Min  | Max  | Default |
|------------|------|------|---------|
| room_size  | 0.0  | 1.0  | 0.7     |
| damping    | 0.0  | 1.0  | 0.4     |
| width      | 0.0  | 1.0  | 1.0     |
| wet_dry    | 0.0  | 1.0  | 0.5     |
```

## Example: UI Layout Config

```markdown
# UILayout

## Default
| Panel       | Visible | DockSide | Width | Height |
|-------------|---------|----------|-------|--------|
| mixer       | true    | right    | 320   | -      |
| browser     | true    | left     | 240   | -      |
| piano_roll  | false   | bottom   | -     | 200    |
| inspector   | true    | left     | 260   | -      |
| transport   | true    | top      | -     | 48     |

## MixingLayout
> set_panel_visible mixer true
> set_panel_width mixer 400
> set_panel_visible browser false
> set_panel_visible piano_roll false
```

## Example: Automation Script

```markdown
# BatchExport

## export_all_tracks
> read "project/tracks.json"
> parse tracks_data
> for_each track:
>     solo track.id
>     transport_play
>     sleep 3000
>     transport_stop
>     export "output/" + track.name + ".wav"
```

## Future: Markscript→Kain Code Generation

```bash
# Generate Kain world definitions from markscript config tables
mks gen config/server_config.md --target kain
# → outputs server_config.kn with Kain struct + loader

# Generate TypeScript types for web frontend
mks gen config/api_schema.md --target typescript
# → outputs api_schema.ts
```

---

*"Your documentation is your program. Your README IS the executable."*
