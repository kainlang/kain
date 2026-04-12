# Doctor And Repair

`kain doctor` is the canonical diagnostics entrypoint for the toolchain.

## What `doctor` Prints

It reports:

- compiler version and build metadata
- git state
- host and target triple
- active binary path and launcher kind
- PATH matches for `kain` and `kn`
- resolved stdlib roots
- runtime C path
- runtime manifest path
- resolved clang when the sys lane is enabled
- supported targets
- enabled features

## Repair Flags

`kain doctor` accepts repair-oriented flags through `DoctorRepairArgs`:

- `--repair FILE`
- `--repair-tree DIR`
- `--profile safe|aggressive`
- `--suggest`
- `--dry-run`
- `--write`

## Repair Modes

The mode is selected in this order:

- `--suggest`
- `--dry-run`
- aggressive profile
- safe profile

Tree repair applies the same logic over every `.kn` file in the directory tree.

## Safe Vs Aggressive

- safe mode keeps to low-risk normalization
- aggressive mode enables parser-recovery rewrites and broader normalization

## Practical Rule

If the compiler or launcher feels inconsistent, start with `doctor` before
assuming the build or runtime is wrong.
