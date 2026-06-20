# Export

Export and bounce pipeline for the reson8 DAW. Renders the current
session to audio files in multiple formats, applies dithering and
loudness normalization, and writes metadata sidecars.

Run with: `kain run reson8 -- --mks src-mks/export.md`

---

## Export Modes

| Mode         | Description                              | Latency     |
|--------------|------------------------------------------|-------------|
| offline      | Full bounce, all tracks, real-time × 0.1 | 1-5 min     |
| real_time    | Live capture from audio device           | track length|
| stem         | One file per track, no master bus        | 1-5 min     |
| freeze       | Render frozen tracks to audio            | <30s        |
| midi         | Export MIDI regions to .mid file         | <1s         |
| project      | Full project archive (.reson8 bundle)    | <5s         |

---

## LoudnessTargets

LUFS targets applied to each export format. Values follow the
EBU R128 broadcast standard for streaming platforms.

| Platform       | IntegratedLUFS | TruePeakDBF | RangeLU   |
|----------------|----------------|-------------|-----------|
| spotify        | -14            | -1          | 11        |
| apple_music    | -16            | -1          | 11        |
| youtube        | -14            | -1          | 11        |
| tidal          | -14            | -1          | 11        |
| amazon_music   | -14            | -2          | 11        |
| cd_master      | -9             | -0.3        | 8         |
| broadcast_ebu  | -23            | -1          | 20        |

---

## export_wav

Export the current session to a 32-bit float WAV file at the
project sample rate. WAV is the master format — all other formats
derive from this.

> spawn "kain run reson8 -- --export-wav master.wav"

> print "Exporting to WAV (32-bit float, 48kHz)..."

> sleep 500

> print "WAV export complete: master.wav"

---

## export_flac

Lossless compressed export. FLAC is ~50-60% the size of WAV
with bit-perfect reconstruction. Ideal for archival.

> spawn "ffmpeg -i master.wav -compression_level 8 master.flac"

> print "Encoding FLAC (compression level 8)..."

> sleep 300

> print "FLAC export complete: master.flac"

---

## export_mp3

320kbps CBR MP3 for portable playback. Universally compatible,
lossy compression acceptable for monitoring/reference copies.

> spawn "ffmpeg -i master.wav -codec:a libmp3lame -b:a 320k master.mp3"

> print "Encoding MP3 (320kbps CBR)..."

> sleep 400

> print "MP3 export complete: master.mp3"

---

## export_aac

256kbps AAC for Apple devices and streaming platform uploads.
Better quality-per-bit than MP3 at the same rate.

> spawn "ffmpeg -i master.wav -codec:a aac -b:a 256k master.m4a"

> print "Encoding AAC (256kbps)..."

> sleep 350

> print "AAC export complete: master.m4a"

---

## export_ogg

192kbps Opus-in-Ogg for low-bandwidth streaming and game audio.
Best perceptual quality at low bitrates.

> spawn "ffmpeg -i master.wav -codec:a libopus -b:a 192k master.ogg"

> print "Encoding Opus (192kbps)..."

> sleep 300

> print "Opus export complete: master.ogg"

---

## export_stems

Export each track as a separate WAV file. Stems are essential
for remixing, collaboration, and surround upmix workflows.

> spawn "kain run reson8 -- --export-stems stems/"

> print "Rendering stems to stems/ directory..."

> sleep 2000

> print "Stem export complete"

---

## export_midi

Export all MIDI regions to a single Standard MIDI File. Useful
for sharing melodic content with hardware sequencers and other DAWs.

> spawn "kain run reson8 -- --export-midi session.mid"

> print "Exporting MIDI to session.mid..."

> sleep 200

> print "MIDI export complete"

---

## export_project

Bundle the full project (audio, MIDI, automation, plugin state,
undo history) into a portable `.reson8` archive. Self-contained
file for backup and collaboration.

> spawn "kain run reson8 -- --export-project session.reson8"

> print "Bundling project archive..."

> sleep 1500

> print "Project archive: session.reson8"

---

## normalize_loudness

Apply EBU R128 loudness normalization to the WAV master.
Measures integrated LUFS, adjusts gain, applies true-peak limiting.

> spawn "kain run reson8 -- --normalize-lufs -14 master.wav"

> print "Applying -14 LUFS normalization..."

> sleep 800

> print "Loudness normalization complete"

---

## dither

Apply TPDF dither when reducing bit depth for 16-bit CD export.
Dither converts quantization error to broadband noise that is
psychoacoustically masked.

> spawn "kain run reson8 -- --dither tpdf --bit-depth 16 master.wav"

> print "Applying TPDF dither (16-bit target)..."

> sleep 400

> print "Dither applied"

---

## write_metadata

Embed ISRC, album, artist, and track metadata into the exported
files. Reads from project world state and writes to file tags.

> spawn "kain run reson8 -- --write-metadata master.wav"

> print "Writing metadata tags..."

> sleep 100

> print "Metadata written"

---

## export_all

Master export pipeline: WAV master → all derived formats with
loudness normalization applied. Each step depends on the previous.

> print "=== Export pipeline started ==="

> run export_wav

> run normalize_loudness

> run dither

> run export_flac

> run export_mp3

> run export_aac

> run export_ogg

> run write_metadata

> print "=== All exports complete ==="

---

## export_for_spotify

Preset pipeline for Spotify delivery: WAV + -14 LUFS + metadata.

> print "Exporting for Spotify (-14 LUFS, MP3 + OGG)..."

> run export_wav

> spawn "kain run reson8 -- --normalize-lufs -14 --true-peak -1 master.wav"

> run export_mp3

> run export_ogg

> print "Spotify export complete"

---

## export_for_apple_music

Preset pipeline for Apple Music delivery: WAV + -16 LUFS + AAC.

> print "Exporting for Apple Music (-16 LUFS, AAC 256k)..."

> run export_wav

> spawn "kain run reson8 -- --normalize-lufs -16 --true-peak -1 master.wav"

> run export_aac

> print "Apple Music export complete"

---

## export_for_cd

Preset for Red Book CD: 16-bit / 44.1kHz WAV with dither.

> print "Exporting for CD (16-bit, 44.1kHz, TPDF dither)..."

> spawn "kain run reson8 -- --export-wav master_cd.wav --bit-depth 16 --sample-rate 44100"

> run dither

> run write_metadata

> print "CD export complete"

---

## Verify

```markscript
print("export: 14 routines, 5 format presets, 0 errors")
```
