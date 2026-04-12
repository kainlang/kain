# Kain Piano Lab

This lab is a Linux-native `kain build native-ui` proof that keeps the piano semantics in Kain and the audio engine in a small C bridge.

What it does:

- opens a native Kain UI window with one octave of keys
- plays generated note WAVs through `miniaudio`
- records timing into a loop buffer in the C runtime
- replays the captured loop on demand

Run:

```bash
./build.sh
./run.sh
```

Controls:

- `Record Loop` starts a fresh capture
- `Stop Recording` freezes the captured loop
- `Play Loop` replays the capture on repeat
- `Stop Loop` halts playback
- `Clear Loop` resets the loop buffer

Implementation notes:

- `native/piano_audio.c` builds `native/libpiano_audio.so`
- note samples are cached under `native/piano_cache/`
- the native-ui launcher materializes into `native-app/`
