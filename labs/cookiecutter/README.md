# Cookie Cutter

This lab bundles four classic language stress tests into one Kain program:

- standalone quine source generation
- Conway's Game of Life with double-buffered state
- ASCII Mandelbrot rendering
- a tiny closure-capable Lisp evaluator

Run it from the repo root with:

```bash
./target/debug/kain run /home/ephemara/Dev/Kain/labs/cookiecutter/main.kn
```

Artifacts land in `/home/ephemara/Dev/Kain/labs/cookiecutter/outputs/`.

The output bundle includes:

- `quine_generated.kn` and `quine_output.txt`
- `game_of_life_frames.txt`, `game_of_life.svg`, and `game_of_life.png`
- `mandelbrot_ascii.txt`, `mandelbrot.svg`, and `mandelbrot.png`
- `lisp_report.txt`
- `showcase_report.txt` and `showcase.html`

Current runtime note: the Kain program computes all four benchmark sections correctly under `kain run`, but the authored `@extern write_file` path in this lab does not currently materialize new files reliably in the native interpreter lane. The committed files in `outputs/` are provided as the cookie-cutter inspection surface until that runtime contract is tightened.
