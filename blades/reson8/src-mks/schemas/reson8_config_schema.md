# Reson8ConfigSchema

Schema definition for reson8 DAW configuration.
Validated at compile time when referenced via `@schema` or `@import`
from `config.md`. Defines required columns, value ranges, and
allowed value sets for each configuration table.

---

## AudioConfig

Audio engine parameters. All numeric fields have explicit min/max.
The device_type field is an enum with allowed values.

| Field          | Type    | Required | Min    | Max      | Allowed                |
|----------------|---------|----------|--------|----------|------------------------|
| sample_rate    | Int     | yes      | 44100  | 192000   | -                      |
| buffer_size    | Int     | yes      | 64     | 2048     | -                      |
| device_type    | String  | yes      | -      | -        | wasapi, directsound, asio, coreaudio, alsa, pulseaudio, jack |
| exclusive_mode | Int     | no       | 0      | 1        | -                      |
| num_channels   | Int     | yes      | 1      | 8        | -                      |
| bit_depth      | Int     | yes      | 16     | 32       | -                      |
| dither         | Int     | no       | 0      | 1        | -                      |

---

## ProjectDefaults

Project-level defaults applied to new sessions.

| Field                  | Type   | Required | Min  | Max  | Allowed |
|------------------------|--------|----------|------|------|---------|
| default_tempo          | Int    | yes      | 20   | 300  | -       |
| time_sig_num           | Int    | yes      | 1    | 16   | -       |
| time_sig_den           | Int    | yes      | 1    | 32   | -       |
| default_key            | String | yes      | -    | -    | C, D, E, F, G, A, B, C#, D#, F#, G#, A# |
| snap_to_grid           | Int    | no       | 0    | 1    | -       |
| snap_division          | Int    | no       | 1    | 64   | -       |
| auto_save              | Int    | no       | 0    | 1    | -       |
| auto_save_interval_min | Int    | no       | 1    | 60   | -       |
| undo_history_depth     | Int    | no       | 16   | 4096 | -       |
| recording_format       | String | no       | -    | -    | wav, flac, aiff, ogg  |
| recording_bit_depth    | Int    | no       | 16   | 32   | -       |

---

## PluginScanPaths

Directories the plugin host scans on startup.

| Field      | Type   | Required | Allowed                |
|------------|--------|----------|------------------------|
| Lane       | String | yes      | kain, vst3, clap, python |
| Path       | String | yes      | -                      |
| Recursive  | Int    | no       | 0, 1                   |

---

## ThemeDefaults

Initial theme properties.

| Field        | Type   | Required |
|--------------|--------|----------|
| name         | String | yes      |
| accent       | String | yes      |
| background   | String | yes      |
| foreground   | String | yes      |
| font_family  | String | no       |
| font_size    | Int    | no       |
| corner_radius| Int    | no       |
| animation_ms | Int    | no       |

---

## KeybindingScheme

Keyboard mapping table.

| Field      | Type   | Required |
|------------|--------|----------|
| Action     | String | yes      |
| Key        | String | yes      |
| Modifiers  | String | no       |

---

## MIDIDeviceFilter

MIDI device name patterns and auto-connect policy.

| Field        | Type   | Required | Allowed          |
|--------------|--------|----------|------------------|
| Pattern      | String | yes      | -                |
| AutoConnect  | Int    | no       | 0, 1             |

---

## DSPGraphDefaults

Default signal routing blocks.

| Field | Type   | Required | Allowed                                       |
|-------|--------|----------|-----------------------------------------------|
| Block | String | yes      | input, gate, eq, delay, reverb, master, output |
| Type  | String | yes      | -                                             |
| Param | String | yes      | -                                             |
| Value | String | yes      | -                                             |

---

## PythonPluginConfig

Python ML plugin lane configuration.

| Field          | Type   | Required | Allowed |
|----------------|--------|----------|---------|
| python_path    | String | yes      | -       |
| venv_dir       | String | no       | -       |
| timeout_sec    | Int    | no       | 1, 600  |
| max_memory_mb  | Int    | no       | 64, 16384 |
| model_cache    | String | no       | -       |
| numpy_required | Int    | no       | 0, 1    |
| torch_required | Int    | no       | 0, 1    |

---

## LoggingConfig

Logging level and rotation settings.

| Field     | Type   | Required | Allowed                                    |
|-----------|--------|----------|--------------------------------------------|
| Level     | String | yes      | error, warn, info, debug, perf             |
| File      | String | yes      | -                                          |
| MaxSizeMB | Int    | no       | 1, 1024                                    |
| Rotate    | Int    | no       | 0, 100                                     |

---

## NetworkEndpoints

External service endpoints.

| Field        | Type   | Required |
|--------------|--------|----------|
| Service      | String | yes      |
| URL          | String | yes      |
| TimeoutSec   | Int    | no       |

---

## BuildConfig

Build and CI settings.

| Field               | Type   | Required | Allowed              |
|---------------------|--------|----------|----------------------|
| target              | String | yes      | llvm, wasm, spirv, rust, cpp, js |
| config              | String | no       | dev, debug, release, speed |
| parallel_jobs       | Int    | no       | 1, 64                |
| cache_dir           | String | no       | -                    |
| coverage_threshold  | Int    | no       | 0, 100               |
| lint_strict         | Int    | no       | 0, 1                 |
| pre_commit_hooks    | Int    | no       | 0, 1                 |

---

## Constraints

Cross-table referential integrity:

1. Every track name in DSPGraphDefaults must reference a known DSP type
2. PluginScanPaths paths must be absolute or relative to the project root
3. LoggingConfig files must end in `.log`
4. NetworkEndpoints URLs must start with `https://`
5. KeybindingScheme keys must be valid key names
6. AudioConfig device_type must match the current platform

---

## Defaults

If a required field is missing, the validator falls back to:

| Field                  | Default |
|------------------------|---------|
| sample_rate            | 48000   |
| buffer_size            | 256     |
| default_tempo          | 120     |
| time_sig_num           | 4       |
| time_sig_den           | 4       |
| undo_history_depth     | 512     |
| coverage_threshold     | 80      |
| parallel_jobs          | 8       |
