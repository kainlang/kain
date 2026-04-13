# KAIN Utility Scripts

This folder contains executable KAIN source files for repo-local filesystem
automation.

These are not shell wrappers. They are normal `.kn` programs that you run with
`kain run` or `kn`.

## What They Use

The scripts lean on the file and environment builtins that already exist in the
language:

- `std__env__current_dir`
- `std__env__var_`
- `std__fs__read_dir`
- `std__fs__read_to_string`
- `std__fs__create_dir_all`
- `std__fs__copy`
- `std__fs__remove_file`
- `std__fs__write`

## Scripts

- `append_text_to_file.kn` appends text to a file, creating parent directories
  when needed.
- `organize_by_extension.kn` groups files in a directory into extension-based
  folders. It defaults to dry-run mode and only mutates files when
  `KAIN_ORGANIZE_APPLY=1`.

## Usage

Because the language does not currently expose a dedicated CLI argument parser
for standalone script helpers, these utilities use environment variables for
configuration.

Append text:

```bash
KAIN_APPEND_TARGET=notes.txt \
KAIN_APPEND_TEXT="Hello from KAIN" \
kain run scripts/kain/append_text_to_file.kn
```

Organize the current directory:

```bash
KAIN_ORGANIZE_APPLY=1 \
KAIN_ORGANIZE_ROOT="$PWD" \
kain run scripts/kain/organize_by_extension.kn
```

## Safety Defaults

- `append_text_to_file.kn` defaults to `notes.txt` in the current directory if
  `KAIN_APPEND_TARGET` is not set.
- `organize_by_extension.kn` is preview-only unless `KAIN_ORGANIZE_APPLY` is
  truthy.
