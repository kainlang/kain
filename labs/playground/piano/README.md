# Kain Piano Lab

This lab is a Linux-native LLVM pipeline proof that keeps the piano semantics in Kain and the audio engine in a small C bridge.

What it does:

- opens a native Kain UI window with one octave of keys
- plays generated note WAVs through `miniaudio`
- records timing into a loop buffer in the C runtime
- replays the captured loop on demand
- renders a piano-specific Kain UI with scoped theme variants for the keys, transport, and loop tape

Run:

```bash
./build.sh
./run.sh
```

`build.sh` compiles `native/piano_audio.c` into `native/libpiano_audio.so` and then builds `src/main.kn` through `kain build --target llvm`, which emits `generated/piano.ll`, `generated/piano`, `generated/piano.runtime_contract.json`, and `generated/piano.realtime_app.json`.

`run.sh` auto-detects the current Wayland/X11 desktop session when the shell does not inherit GUI env, exports the LLVM sidecar paths, and launches the linked binary from `generated/`.

Controls:

- `Record Loop` starts a fresh capture
- `Stop Recording` freezes the captured loop
- `Play Loop` replays the capture on repeat
- `Stop Loop` halts playback
- `Clear Loop` resets the loop buffer

Implementation notes:

- `native/piano_audio.c` builds `native/libpiano_audio.so`
- note samples are cached under `native/piano_cache/`
- the LLVM-linked app lands in `generated/` alongside the runtime sidecars
