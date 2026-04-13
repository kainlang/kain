# KAIN Actor Demos

This folder contains executable KAIN programs that exercise the real actor
runtime. They are normal `.kn` files, not wrappers around shell or Python.

The scripts use the actual actor syntax in this repo:

- `spawn ActorName(field = value)`
- `send actor_ref.Message(field = value)`

The main function only boots the coordinator actor and then runs a short flush
loop so the background worker actors can finish before the process exits.

## Scripts

- `folder_job_runner.kn` scans a folder, spawns one worker actor per runnable
  text file, and retries once when the file name contains
  `KAIN_JOB_RETRY_TOKEN`.
- `file_indexer.kn` scans a folder, spawns one bucket actor per extension, and
  routes each direct child file to the matching bucket actor.

## Usage

Folder job runner:

```bash
KAIN_JOB_ROOT="$PWD" \
KAIN_JOB_RETRY_TOKEN=README.md \
kain run scripts/kain/actor/folder_job_runner.kn
```

File indexer:

```bash
KAIN_INDEX_ROOT="$PWD" \
kain run scripts/kain/actor/file_indexer.kn
```

## Notes

- Both demos only look at direct child files.
- The job runner keeps its retry path deterministic by retrying once when the
  configured token exactly matches the file name.
- The bucket indexer uses one actor per extension group so the terminal output
  shows the coordinator/worker split clearly.
