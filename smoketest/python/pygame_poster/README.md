# Pygame Poster Smoke

This smoke test proves that Kain can drive `pygame` through the Python bridge, generate raster art, pull image data back into Kain, mutate it, and save final artifacts.

Primary wrapper surface:

- `use std::python::bridge`
- `use std::python::pygame`
- `use std::dcc::image`

Run:

```powershell
run_all.bat
cargo run -q -p cli -- smoketest/python/pygame_poster/smoke.kn -t test
cargo run -q -p cli -- smoketest/python/pygame_poster/smoke.kn -t interpret
```

Artifacts:

- `outputs/pygame_poster.png`
- `outputs/pygame_poster_mutated.png`
- `outputs/pygame_poster_preview.ppm`
- `outputs/pygame_report.txt`
