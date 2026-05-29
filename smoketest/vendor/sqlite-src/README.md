# SQLite Upstream Provenance

This smoketest lane was sourced from a real upstream SQLite clone instead of a
toy header snippet.

- Clone source: `https://github.com/sqlite/sqlite.git`
- Clone date: `2026-05-28`
- Upstream manifest UUID: see [`manifest.uuid`](./manifest.uuid)

The live dogfood payload that Kain compiles against is staged in:

- `smoketest/native/sqlite3.h`
- `smoketest/native/sqlite3.c`
- `smoketest/native/sqlite3ext.h`

We keep the native folder stable for the include-system smoke while this vendor
folder records the upstream source we cloned to mint the amalgamation.
