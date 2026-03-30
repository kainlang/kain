# MEMORY

## 2026-03-29 - doctor CLI UX gained profile-aware repair reporting

The `kain doctor` repair lane now feels more like a first-class operator command instead of a bare file fixer.

What changed:

- `crates/cli/src/repair.rs`
  - Added a `--profile` selector on the repair sub-surface with `safe` and `aggressive` presets.
  - The repair runner now receives the selected profile and passes it through to `kain-repair`.
  - Safe profile disables the higher-risk semantic rewrites; aggressive keeps the full default repair profile.
- `crates/cli/src/main.rs`
  - Doctor repair output now reports:
    - selected profile
    - repair mode
    - safe vs aggressive action class
    - fixes applied/suggested
    - remaining diagnostics
    - per-fix classification as safe/aggressive
  - Repair runs now return after printing the repair report instead of falling through to the normal doctor diagnostics.
- `crates/cli/src/lib.rs`
  - Updated the launcher shortcut hints to show the profile-aware repair surface.

Command surface now includes:

- `kain doctor`
- `kain doctor --repair <file>`
- `kain doctor --repair <file> --profile safe`
- `kain doctor --repair <file> --profile aggressive`
- `kain doctor --repair <file> --suggest`
- `kain doctor --repair <file> --dry-run`
- `kain doctor --repair <file> --write`

Notes:

- I did not run tests or `cargo check`, per instruction.
- The CLI surface stays backward-compatible; existing `--repair`, `--suggest`, `--dry-run`, and `--write` flows still work.
- The repair output now exposes the difference between conservative normalization and more invasive parser-recovery work so users can see what was actually attempted.

## 2026-03-29 - repair surface coherence pass

Stabilization pass on the auto-repair wiring:

- Added `crates/kain-repair::repair_source_with_profile(...)` so the CLI can pass a profile through instead of flattening everything to the default profile.
- Kept the CLI repair branch aligned with the existing `RepairMode` contract in `kain-repair` instead of inventing extra mode variants in the caller.
- Doctor repair output now clearly labels the selected profile and classifies the mode as safe or aggressive.
- This was a low-conflict cleanup pass: no tests, no cargo check.
