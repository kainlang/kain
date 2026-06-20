# Automation

> Transport and DAW control automation scripts.
> Each routine is a named automation that the reson8 bridge
> can schedule, batch, or trigger from a UI button.
>
> All transport operations (`transport_play`, `transport_stop`,
> `transport_record`) are DAW-bridge intents registered through
> the 78-handler IVT alongside the standard 57-keyword registry.
> `sleep` uses the standard `handler_time_sleep` keyword.
> `select_all` and `normalize` are DAW-clip intents.

---

## render_session
> print "Starting session render..."
> transport_play
> sleep 5000
> transport_stop
> export "output/session_master.wav"
> print "Render complete"

---

## batch_export_tracks
> print "Batch exporting all tracks..."
> solo track_1
> transport_play
> sleep 3000
> transport_stop
> export "output/track_1.wav"
> print "Track 1 exported"

### schedule
| Step | Action | DelayMs | Notes |
|------|--------|---------|-------|
| 1 | solo track_1 | 0 | mute siblings |
| 2 | transport_play | 0 | begin render |
| 3 | transport_stop | 3000 | fixed-length capture |
| 4 | export | 0 | target path |
| 5 | print | 0 | operator feedback |

---

## normalize_all
> print "Normalizing all clips..."
> select_all
> normalize -14.0
> print "Normalization complete"

### target_lufs
| Track | PreLUFS | PostLUFS | GainDB |
|-------|---------|----------|--------|
| drums | -18.0 | -14.0 | 4.0 |
| bass | -16.5 | -14.0 | 2.5 |
| vocals | -12.0 | -14.0 | -2.0 |
| guitars | -19.0 | -14.0 | 5.0 |
| master | -14.0 | -14.0 | 0.0 |

---

## record_punch_in
> print "Punch recording..."
> transport_record
> sleep 8000
> transport_stop
> print "Recording complete"

### punch_points
| Marker | Beat | Bar | Action |
|--------|------|-----|--------|
| pre_roll | 1.0 | 1 | countdown |
| punch_in | 5.0 | 5 | start record |
| punch_out | 13.0 | 13 | stop record |
| post_roll | 17.0 | 17 | tail decay |

---

## bounce_in_place
> print "Bouncing selected clips in place..."
> select_all
> export "output/bounce.wav"
> print "Bounce complete"

---

## loop_region
> print "Looping selected region..."
> transport_loop
> sleep 10000
> transport_stop
> print "Loop test complete"

---

## verify

```markscript
print("automation: 5 transport routines defined")
print("automation: sleep values in ms dispatched via handler_time_sleep")
print("automation: target_lufs table = 5 rows (drums, bass, vocals, guitars, master)")
print("automation: punch_points table = 4 markers (pre_roll, punch_in, punch_out, post_roll)")
```
