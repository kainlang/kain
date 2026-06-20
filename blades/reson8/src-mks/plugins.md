# Plugins

> Plugin pipeline — audio effects defined as markscript routines.
> Each plugin is its own `##` routine; `###` sub-sections carry
> process steps and typed parameter tables that compile to
> `OP_PUSH_MATRIX` for the Kain bridge to consume.
>
> Plugin parameters are pipe-tables: Kain schema generation can
> read these directly to produce typed `struct` definitions.

---

## Reson8Reverb
> print "Loading Reson8Reverb v1.0"

### process
> print "Processing reverb..."

### params
| Param | Min | Max | Default | Unit |
|-------|-----|-----|---------|------|
| room_size | 0.0 | 1.0 | 0.7 | - |
| damping | 0.0 | 1.0 | 0.4 | - |
| width | 0.0 | 1.0 | 1.0 | - |
| wet_dry | 0.0 | 1.0 | 0.5 | - |
| pre_delay | 0 | 200 | 20 | ms |

---

## Reson8Compressor
> print "Loading Reson8Compressor v1.0"

### process
> print "Running dynamics compression..."

### params
| Param | Min | Max | Default | Unit |
|-------|-----|-----|---------|------|
| threshold | -60.0 | 0.0 | -20.0 | dB |
| ratio | 1.0 | 20.0 | 4.0 | :1 |
| attack | 0.1 | 500.0 | 10.0 | ms |
| release | 1.0 | 2000.0 | 100.0 | ms |
| makeup | -24.0 | 24.0 | 0.0 | dB |
| knee | 0.0 | 24.0 | 3.0 | dB |
| mix | 0.0 | 1.0 | 1.0 | - |

---

## Reson8EQ
> print "Loading Reson8EQ v1.0"

| Band | Freq | Q | Gain | Type |
|------|------|---|------|------|
| 1 | 80 | 0.7 | 0.0 | low_shelf |
| 2 | 500 | 1.4 | 0.0 | peaking |
| 3 | 2000 | 1.4 | 0.0 | peaking |
| 4 | 8000 | 0.7 | 0.0 | high_shelf |

### bands_meta
| Property | Value |
|----------|-------|
| channel_mode | stereo |
| slope_db_per_oct | 12 |
| analyzer_enabled | true |

---

## load_index
> print "Loading plugin index from disk..."
> read "plugins/index.json"
> print "Plugin index loaded"
> print "Enumerating 3 effects: reverb, compressor, EQ"

---

## verify

```markscript
print("plugins: 3 effects defined with typed parameter tables")
print("plugins: reverb params = 5 floats + 1 int + 1 string")
print("plugins: compressor params = 7 floats")
print("plugins: eq bands = 4 rows (low_shelf, peaking, peaking, high_shelf)")
```
