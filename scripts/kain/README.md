# KAIN Utility Scripts

This folder contains executable KAIN source files for repo-local filesystem
automation.

These are normal `.kn` programs, not shell wrappers. Run them with `kain run`
or `kn`.

## Runtime Surface

The scripts use the real interpreter builtins for filesystem and environment
work:

- `read_file(path)` and `write_file(path, content)` for text files
- `read_dir(path)` for direct child paths, returned in sorted order
- `create_dir_all(path)` for parent directories
- `copy_file(src, dest)` and `remove_file(path)` for file moves
- `file_exists(path)`, `path_is_file(path)`, and `path_is_dir(path)` for
  checks
- `path_join(base, child)`, `path_parent(path)`, `path_file_name(path)`,
  `path_extension(path)`, and `path_stem(path)` for path manipulation
- `env(name)` for configuration, returning an empty string when the variable is
  missing

## Scripts

- `append_text_to_file.kn` appends text to a file, creating parent directories
  when needed.
- `organize_by_extension.kn` groups files in a directory into extension-based
  folders. It defaults to dry-run mode and only mutates files when
  `KAIN_ORGANIZE_APPLY` is truthy.
- `actor/` contains actor-system demos that fan out work to worker actors and
  use the real `spawn` / `send` message surface.

## Usage

Append text:

```bash
KAIN_APPEND_TARGET=notes.txt \
KAIN_APPEND_TEXT="Hello from KAIN" \
kain run scripts/kain/append_text_to_file.kn
```

Organize a folder:

```bash
KAIN_ORGANIZE_APPLY=1 \
KAIN_ORGANIZE_ROOT="$PWD" \
kain run scripts/kain/organize_by_extension.kn
```

Actor demos:

```bash
KAIN_JOB_ROOT="$PWD" \
KAIN_JOB_RETRY_TOKEN=README \
kain run scripts/kain/actor/folder_job_runner.kn

KAIN_INDEX_ROOT="$PWD" \
kain run scripts/kain/actor/file_indexer.kn
```

## Safety Defaults

- `append_text_to_file.kn` defaults to `notes.txt` in the current directory if
  `KAIN_APPEND_TARGET` is blank or unset.
- `organize_by_extension.kn` is preview-only unless `KAIN_ORGANIZE_APPLY` is
  truthy.
- The organizer only touches direct child files of the target root. It does not
  recurse.
- The actor demos also stay on direct child files only so their message flow is
  easy to follow in the terminal.
