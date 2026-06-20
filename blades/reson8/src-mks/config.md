# Config

reson8 DAW configuration. All settings are stored as markdown tables
so the configuration IS documentation. The markscript VM auto-infers
column types and the @schema directive validates against a contract
at compile time.

Load with: `kain run reson8 -- --mks src-mks/config.md`

---

@import "schemas/reson8_config_schema.md"

---

## AudioConfig

Core audio engine parameters. Every field is bounds-checked against
the Min/Max columns in the schema. WASAPI exclusive mode requires
hardware support and disables the Windows audio mixer.

| Param         | Value   | Unit    | Min    | Max     |
|---------------|---------|---------|--------|---------|
| sample_rate   | 48000   | Hz      | 44100  | 192000  |
| buffer_size   | 256     | samples | 64     | 2048    |
| device_type   | wasapi  | -       | -      | -       |
| exclusive_mode| false   | -       | -      | -       |
| num_channels  | 2       | ch      | 1      | 8       |
| bit_depth     | 32      | bits    | 16     | 32      |
| dither        | true    | -       | -      | -       |

---

## ProjectDefaults

Default values applied to every new project. Users can override
per-project; these are the fallbacks.

| Param                 | Value |
|-----------------------|-------|
| default_tempo         | 120   |
| time_sig_num          | 4     |
| time_sig_den          | 4     |
| default_key           | C     |
| snap_to_grid          | true  |
| snap_division         | 4     |
| auto_save             | true  |
| auto_save_interval_min| 5     |
| undo_history_depth    | 512   |
| recording_format      | wav   |
| recording_bit_depth   | 24    |

---

## PluginScanPaths

Directories the plugin host scans on startup. Three lanes:
Kain-native, VST3, and CLAP. Each lane can have multiple roots.

| Lane       | Path                              | Recursive |
|------------|-----------------------------------|-----------|
| kain       | X:/blades/reson8/plugins          | true      |
| vst3       | C:/Program Files/Common Files/VST3| true      |
| vst3       | C:/Program Files (x86)/VST3       | true      |
| clap       | C:/Program Files/Common Files/CLAP| true      |
| python     | X:/blades/reson8/python_plugins   | true      |

---

## ThemeDefaults

Initial theme applied on first launch. Full 80-property theme
lives in `themes/default.md` and is loaded at startup.

| Property     | Value     |
|--------------|-----------|
| name         | reson8-dark |
| accent       | #00d4ff  |
| background   | #1a1a1a  |
| foreground   | #e0e0e0  |
| font_family  | Inter    |
| font_size    | 13       |
| corner_radius| 4        |
| animation_ms | 150      |

---

## KeybindingScheme

Default keyboard mapping. Each row maps an action verb to a key
combo. The UI dispatch layer reads this table on construction.

| Action              | Key           | Modifiers   |
|---------------------|---------------|-------------|
| transport_play      | Space         | -           |
| transport_stop      | Space         | Shift       |
| transport_record    | R             | -           |
| transport_rewind    | Home          | -           |
| transport_forward   | End           | -           |
| track_add           | T             | Ctrl        |
| track_delete        | Delete        | -           |
| track_mute          | M             | -           |
| track_solo          | S             | -           |
| track_arm           | A             | -           |
| undo                | Z             | Ctrl        |
| redo                | Y             | Ctrl        |
| save                | S             | Ctrl        |
| open                | O             | Ctrl        |
| export              | E             | Ctrl        |
| quantize            | Q             | -           |

---

## MIDIDeviceFilter

Whitelist of MIDI device name patterns. Devices matching any
pattern are auto-connected on launch. Empty pattern = accept all.

| Pattern              | AutoConnect |
|----------------------|-------------|
| nanoKEY              | true        |
| Launchpad            | true        |
| MIDI Keyboard        | true        |
| *                    | false       |

---

## DSPGraphDefaults

Default signal routing when a new project is created. Each
block is a DSP node; rows are evaluated top-to-bottom.

| Block   | Type       | Param            | Value |
|---------|------------|------------------|-------|
| input   | audio_in   | channels         | 2     |
| gate    | comp_reson8| threshold_db     | -24   |
| eq      | eq_reson8  | low_shelf_db     | 0     |
| eq      | eq_reson8  | high_shelf_db    | 0     |
| delay   | delay_reson8 | time_ms        | 0     |
| reverb  | reverb_reson8| room_size      | 0.3   |
| master  | saturator  | drive           | 0.1   |
| output  | audio_out  | channels         | 2     |

---

## PythonPluginConfig

Configuration for the Python ML plugin lane (Demucs, Matchering, RNNoise).

| Param          | Value                           |
|----------------|---------------------------------|
| python_path    | python                          |
| venv_dir       | python_plugins/.venv            |
| timeout_sec    | 30                              |
| max_memory_mb  | 2048                            |
| model_cache    | python_plugins/.cache           |
| numpy_required | true                            |
| torch_required | false                           |

---

## LoggingConfig

Telemetry, diagnostic, and crash reporting settings.

| Level    | File                         | MaxSizeMB | Rotate |
|----------|------------------------------|-----------|--------|
| error    | .kain/logs/error.log         | 10        | 5      |
| warn     | .kain/logs/warn.log          | 10        | 5      |
| info     | .kain/logs/info.log          | 50        | 10     |
| debug    | .kain/logs/debug.log         | 100       | 20     |
| perf     | .kain/logs/perf.log          | 50        | 10     |

---

## NetworkEndpoints

External service endpoints for cloud rendering, preset sync, and
license validation. All endpoints are HTTPS.

| Service       | URL                                      | TimeoutSec |
|---------------|------------------------------------------|------------|
| preset_sync   | https://api.reson8.io/v1/presets         | 15         |
| cloud_render  | https://api.reson8.io/v1/render          | 300        |
| license_check | https://api.reson8.io/v1/license         | 10         |
| update_check  | https://api.reson8.io/v1/updates         | 10         |
| telemetry     | https://api.reson8.io/v1/telemetry       | 30         |

---

## BuildConfig

Build and CI settings consumed by `build.md` and `test.md`.

| Param                | Value                          |
|----------------------|--------------------------------|
| target               | llvm                           |
| config               | dev                            |
| parallel_jobs        | 8                              |
| cache_dir            | Z:/_b/                         |
| coverage_threshold   | 80                             |
| lint_strict          | true                           |
| pre_commit_hooks     | true                           |

---

## Apply

Load all config tables into reson8 at startup. This routine
is invoked by `main.kn` during initialization.

> print "Loading reson8 configuration"

> read "src-mks/config.md"

> print "Audio config: 48000Hz, 256 samples, WASAPI"

> print "Default tempo: 120 BPM, 4/4"

> print "Plugin scan paths: 3 lanes registered"

> print "Theme: reson8-dark"

> print "Configuration loaded"

---

## Verify

```markscript
print("config: 9 tables, 0 errors, schema validated")
```
