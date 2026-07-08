# Kain Audio DSP Effects

Six Kain `.kn` files implementing audio DSP effects for JavaScript loading.

## Files

| File | Effect | Export | Description |
|------|--------|--------|-------------|
| `lowpass.kn` | Biquad Lowpass | `lowpass_process(samples, count, freq_hz, q, sr)` | RBJ resonant lowpass filter |
| `reverb.kn` | Schroeder Reverb | `reverb_process(samples, count, decay, mix, sr)` | 4-comb + 2-allpass reverb |
| `delay.kn` | Ping-pong Delay | `delay_process(samples, count, delay_ms, feedback, mix, sr)` | Stereo cross-feedback delay |
| `chorus.kn` | Chorus | `chorus_process(samples, count, rate_hz, depth_ms, mix, sr)` | LFO-modulated delay chorus |
| `tremolo.kn` | Tremolo | `tremolo_process(samples, count, rate_hz, depth, sr)` | Sine LFO amplitude modulation |
| `run_audio.kn` | Test Harness | `main()` | Generates 440Hz → lowpass → JSON |

## Building

All files typecheck and compile:

```bash
# Typecheck
kain check audio/*.kn

# Build test harness (executable)
kain build audio/run_audio.kn --target llvm

# Run test harness
./run_audio.exe
```

## Usage from JavaScript

Each `.kn` exports a process function that takes a `ptr<Float>` buffer, processes samples in-place, and returns an Int status code (0 = success).

```js
// For shared library loading (DLL/so/dylib)
const ffi = require('ffi-napi');
const lib = ffi.Library('lowpass.dll', {
    'lowpass_process': ['int', ['pointer', 'int', 'float', 'float', 'int']]
});
```

## Algorithm Details

**Lowpass**: RBJ Audio EQ Cookbook biquad lowpass via transposed direct form II.

**Reverb**: Classic Schroeder reverberator — 4 parallel comb filters at prime-ratio delays + 2 series allpass filters.

**Delay**: Stereo ping-pong delay — left channel writes to right delay line and vice versa.

**Chorus**: Modulated delay line with sine-wave LFO controlling read position.

**Tremolo**: Sine-wave LFO multiplied with input signal.
