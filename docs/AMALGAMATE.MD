# Amalgamate -- Kain's Package Manager & Portable Capsule System

> **One file. All the code. Forever.**

Amalgamate is Kain's **capsule pipeline** -- a first-class package distribution and code portability system. It packs any number of Kain modules (2 or 2,594) into a single portable `.kn` capsule file. Drop that file into any project. Import whatever you need. The compiler resolves everything directly from the capsule. No unpack. No install. No network. No lockfile. No version solver.

Amalgamate turns Kain code into a **durable digital asset**. Every `pub fn` ever written is one `kain amalgamate` away from being importable by every other Kain project. Forever.

______________________________________________________________________

## Table of Contents

1. [The Three Big Problems Amalgamate Solves](#1-the-three-big-problems-amalgamate-solves)
1. [The Capsule Format](#2-the-capsule-format)
1. [Storage Modes: Editable vs Archive](#3-storage-modes-editable-vs-archive)
1. [Content Policies: Five Capsule Flavors](#4-content-policies-five-capsule-flavors)
1. [The Companion Capsule System](#5-the-companion-capsule-system)
1. [CLI Reference -- All Commands & Flags](#6-cli-reference----all-commands--flags)
1. [build.kn Integration](#7-buildkn-integration)
1. [The Package System: add / install / publish](#8-the-package-system-add--install--publish)
1. [Materialization: How Capsules Become Imports](#9-materialization-how-capsules-become-imports)
1. [Capsule Augmentation: Reshape & Merge](#10-capsule-augmentation-reshape--merge)
1. [Proven Scale: 2,594 Modules, 15 MB, 3 Seconds](#11-proven-scale-2594-modules-15-mb-3-seconds)
1. [Common Workflows](#12-common-workflows)
1. [Capsule Format On-Disk Structure](#13-capsule-format-on-disk-structure)
1. [CRATE MAP: Where Everything Lives](#14-crate-map-where-everything-lives)

______________________________________________________________________

## 1. The Three Big Problems Amalgamate Solves

### Problem 1: Code Distribution Is Permanent Pain

Every language ecosystem reinvents dependency distribution: npm has `node_modules` + lockfiles, Rust has crates.io + `Cargo.lock`, Python has PyPI + `requirements.txt`. They all share the same failure modes -- network dependency, version solver explosion, lockfile drift, and the "works on my machine" problem.

Amalgamate's answer: **pack the source into a single file that IS the distribution.** No metadata server. No version solver. No network. The capsule file IS the package.

### Problem 2: Monorepo Code Reuse Is Friction

You built a killer `collision.kn` in one blade. You want to use it in another blade. Today you copy files or reorganize directories or set up workspaces. Tomorrow you run `kain amalgamate src/ -o collision.kn`, drop the file into your second blade, and `use collision`.

### Problem 3: Reproducible Builds Need Source Anchoring

You updated a dependency three months ago. What exact source did you use? With capsules, the `sha256` digest in the capsule metadata IS the provenance. Store the capsule in your repo. Tag the digest. Forever reproducible.

______________________________________________________________________

## 2. The Capsule Format

A capsule is a **valid Kain source file**. The Kain parser sees comment blocks as comments, so a capsule `.kn` file passes `kain check` as a valid (empty) source document. This means capsules are first-class import targets -- the module resolution system reads capsule files natively.

### Capsule Anatomy

```
// -- KAIN CAPSULE v2 ----------------------------------------------------------
// name:       my-package
// kind:       blade
// storage:    editable
// contents:   source
// digest:     sha256:abc123...
// capsule:    my-package
// entry:      src/main.kn
// structure:  47 files | 12 modules
// ---------------------------------------------------------------------------
// -- PUBLIC INTERFACE DIRECTORY -----------------------------------------------
// [functions]
//   main, vec3_add, sha256, json_parse
// [types]
//   Vec3, Quat
// ...
// ---------------------------------------------------------------------------

//!kain-capsule
// schema = 2
// kind = "blade"
// ...
//!end-kain-capsule

//!kain-file
// path = "src/main.kn"
// ...
//!kain-file-content
// fn main() -> Int:
//     return 0
//!end-kain-file

// More //!kain-file blocks...
```

### Capsule Structure (Top to Bottom)

1. **Header (optional, configurable):** Human-readable preview showing name, kind, digest, file count, module count, and a public symbol directory. Configured via `--header` (rich, minimal, or off).

1. **Metadata Block:** TOML-encoded metadata behind `//!kain-capsule` / `//!end-kain-capsule` sentinels. Contains schema version, kind, storage mode, content policy, digest, creation timestamp, file/module counts, capsule_set name, entry points, authors, tags, and custom metadata.

1. **Content Blocks:**

   - **Editable storage:** Each file is a `//!kain-file` block with inline content between `//!kain-file-content` and `//!end-kain-file`. Text files are stored as-is. Binary files are base64-encoded.
   - **Archive storage:** A single `//!kain-capsule-payload` block containing a zstd-compressed (or raw) base64 blob: a JSON archive with all files, their paths, sizes, SHA-256 digests, and base64-encoded contents.

### Schema Versions

| Version | Capabilities |
|---------|-------------|
| v1 | Initial format: editable file blocks, minimal metadata |
| v2 (current) | Archive mode, capsule_set companion discovery, content policies, zstd compression, custom metadata, symbol preview indexing |

______________________________________________________________________

## 3. Storage Modes: Editable vs Archive

### Editable Storage (`--archive` NOT set)

Each file in the capsule is written as a discrete `//!kain-file` block. Text files are human-readable inline. Binary files are base64-encoded inline.

**Use when:** You want to read the capsule contents in a text editor, track changes with git diff, or use the capsule as a development-time dependency.

**Trade-off:** Larger on disk (no compression). Binary files are expanded ~33% by base64.

### Archive Storage (`--archive`)

All files are serialized into a single JSON archive, then zstd-compressed (level 19) and base64-encoded into a single payload block. Every file carries a SHA-256 digest for integrity verification on unpack.

**Use when:** You want maximum compression for distribution, CI artifacts, or long-term archival. Payload digest is verified against metadata on every read.

**Trade-off:** Not human-readable. Must be unpacked to inspect individual files.

______________________________________________________________________

## 4. Content Policies: Five Capsule Flavors

The `--contents` flag controls WHICH files from the input tree get included:

| Policy | What's Included | Use Case |
|--------|----------------|----------|
| **`source`** | `.kn` files, `.c`/`.h` files, config files, markdown -- everything except assets/artifacts/evidence | Distributable source package |
| **`snapshot`** | Everything -- the raw directory snapshot | Complete archival backup |
| **`assets`** | Images, fonts, audio, video, 3D models, ZIP archives, PDFs, and any non-UTF-8 binary | Game asset bundles, design files |
| **`artifacts`** | `.exe`, `.dll`, `.so`, `.lib`, `.obj`, `.wasm`, `.spv`, `.ptx`, `.pdb`, `.runtime_contract.json`, and generated runtime objects | Compile output distribution, CI build artifacts |
| **`evidence`** | Telemetry, benchmark reports, attrition results, proof attestations, certify gate outputs | Reproducible evidence for audits |

### How Content Classification Works

When packing a directory, Amalgamate reads the manifest (`build.kn` / `KAIN.toml`) to understand what's source, what's build output, and what's evidence:

- **Source classification:** Files in `module_roots`, `source_roots`, `search_roots`, entry points
- **Artifact classification:** Files in `artifact_root`, `cache_root`, files matching known artifact extensions (`.exe`, `.dll`, `.obj`, `.spv`, `.ptx`, etc.)
- **Evidence classification:** Task outputs from test/proof/benchmark/attrition/certify tasks, files in `telemetry/full/` directories
- **Asset classification:** Files with known media extensions (`.png`, `.jpg`, `.ttf`, `.mp3`, `.glb`, etc.) or any non-UTF-8 binary file

### Smart Task Output Detection

Amalgamate automatically classifies build task outputs based on `build.kn` task kind:

| Task Kind | Output Classified As |
|-----------|---------------------|
| `test`, `proof`, `benchmark`, `bench`, `attrition`, `certify`, `check` | Evidence |
| `exec`, `build`, `native_executable`, `amalgamate` | Artifacts |
| Any task with `telemetry` channel containing "evidence" words | Evidence |

______________________________________________________________________

## 5. The Companion Capsule System

A single project can produce THREE capsule files that share a `capsule_set` name:

```
my-package.kn              -- Source capsule
my-package.artifacts.kn    -- Artifacts companion capsule
my-package.evidence.kn     -- Evidence companion capsule
```

When you **materialize** (unpack + import) the source capsule, the system automatically discovers sibling capsules with the same `capsule_set` name and merges their contents into the materialized workspace. This gives you:

- **Source capsule stands alone** for code reuse
- **Artifacts capsule** carries the compiled `.exe`/`.dll`/`.spv` outputs alongside
- **Evidence capsule** carries the telemetry and proof results

### Companion Naming Convention

Given output capsule path `my-package.kn`:

- Artifacts companion: `my-package.artifacts.kn`
- Evidence companion: `my-package.evidence.kn`

When re-packing source, existing companion capsules with matching `capsule_set` are automatically excluded from the source capsule (they're companion capsules, not source files).

### Companion Merge Rules

- Files from companion capsules are merged into the materialized workspace
- If two companions contain the same file path, the bytes MUST match (hash collision check)
- Companions are applied in priority order: source > assets > artifacts > evidence
- The merged workspace is the full project tree -- source code, binaries, and evidence all in one directory

______________________________________________________________________

## 6. CLI Reference -- All Commands & Flags

### 6.1 `kain amalgamate` -- Pack a Capsule

**Alias:** `kain a`

```
kain amalgamate [OPTIONS] <INPUT> -o <OUTPUT>
```

Pack a file or directory into a portable Kain capsule.

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | PathBuf | Required | File or directory to pack |
| `-o` / `--output` | PathBuf | Required | Output capsule `.kn` path |
| `--name` | String | Auto | Capsule display name (default: folder name or manifest name) |
| `--version` | String | Auto | Version label (default: from KAIN.toml / Cargo.toml) |
| `--author` | Repeatable String | -- | Author field (repeatable) |
| `--note` | Repeatable String | -- | Free-form note (repeatable) |
| `--tag` | Repeatable String | -- | Tag for categorization (repeatable) |
| `--meta` | Repeatable `KEY=VALUE` | -- | Arbitrary key=value metadata (repeatable) |
| `--contents` | String | `"source"` | Content policy: `source`, `snapshot`, `assets`, `artifacts`, `evidence` |
| `--capsule-set` | String | Auto | Sibling capsule set name for companion discovery |
| `--archive` | bool | false | Store as compressed archive payload instead of editable blocks |
| `--header` | String | `"rich"` | Header rendering: `minimal`, `rich`, `off` |
| `--preview-symbols` | usize | 40 | Max preview symbols shown in header |
| `--compression` | String | `"zstd"` | Archive payload compression: `zstd` or `none` |
| `--api-index` | String | `"auto"` | Public API symbol index: `auto` or `off` |
| `--module-index` | String | `"auto"` | Module count in preview: `auto` or `off` |

**Examples:**

```powershell
# Pack a source capsule
kain amalgamate src/ -o mylib.kn

# Pack with metadata
kain amalgamate . -o mylib.kn --name "My Library" --version "1.2.0" --tag "math" --tag "3d"

# Pack as compressed archive
kain amalgamate src/ -o mylib.kn --archive --compression zstd

# Pack only artifacts (build outputs)
kain amalgamate . -o mylib.artifacts.kn --contents artifacts

# Pack with capsule_set for companion discovery
kain amalgamate src/ -o mylib.kn --capsule-set "mylib"

# Minimal header for smaller file
kain amalgamate src/ -o mylib.kn --header minimal

# Disable symbol preview (faster pack)
kain amalgamate src/ -o mylib.kn --api-index off --module-index off
```

### 6.2 `kain amalgamate inspect` -- Inspect a Capsule

```
kain amalgamate inspect [--json] <INPUT>
```

Read capsule metadata and print its structure, public interface, and file inventory.

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | PathBuf | Required | Capsule `.kn` file path |
| `--json` | bool | false | Emit structured JSON output |

**Example output (text mode):**

```
KAIN capsule
  name: mylib
  version: 1.2.0
  kind: blade
  digest: sha256:abc123...
  storage: editable
  contents: source
  structure: 47 files | 12 modules
  tags: math, 3d

PUBLIC INTERFACE DIRECTORY
[functions]
  main, vec3_add, vec3_normalize, sha256, json_parse
[types]
  Vec3, Quat, Matrix4x4

FILES
  src/main.kn (2048 bytes)
  src/math.kn (4096 bytes)
  ...
```

**JSON output (`--json`):**

```json
{
  "metadata": {
    "schema": 2,
    "kind": "blade",
    "storage": "editable",
    "contents": "source",
    "digest": "sha256:abc123...",
    "file_count": 47,
    "module_count": 12,
    ...
  },
  "preview": {
    "module_count": 12,
    "total_symbols": 89,
    "sections": [
      {"title": "functions", "symbols": ["main", "vec3_add", ...]},
      {"title": "types", "symbols": ["Vec3", "Quat"]}
    ]
  },
  "files": [
    {"path": "src/main.kn", "size_bytes": 2048, "sha256": "sha256:..."}
  ]
}
```

### 6.3 `kain amalgamate unpack` -- Unpack a Capsule

```
kain amalgamate unpack [-o OUTPUT] <INPUT>
```

Extract all files from a capsule into a directory tree.

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | PathBuf | Required | Capsule `.kn` file path |
| `-o` / `--output` | PathBuf | `<capsule>.unpacked/` | Output directory |

**Examples:**

```powershell
# Unpack to default directory (mylib.unpacked/)
kain amalgamate unpack mylib.kn

# Unpack to specific directory
kain amalgamate unpack mylib.kn -o ./vendor/mylib/
```

______________________________________________________________________

## 7. build.kn Integration

### 7.1 Auto-Emit Capsules from Build Pipeline

Amalgamate tasks can be wired directly into the `build.kn` build graph:

```kain
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    // Create a capsule task ~ runs when its dependencies are satisfied
    let capsule = amalgamate_capsule("my-package")
        .path(".")                                // input directory
        .output("$root/.kain/capsules/mylib.kn")  // output capsule path
        .name("mylib")
        .version("1.2.0")
        .storage("editable")
        .contents("source")
        .capsule_set("mylib")
        .header("rich")
        .preview_symbols(40)
        .tag("portable")
        .note("Auto-generated from build pipeline")
        .after(cert)                              // run after certify

    return build_graph().task(capsule)
```

### 7.2 Capsule Task Constructor API

Two equivalent constructors produce the same `"amalgamate"` task kind:

```kain
amalgamate_capsule("task-id")   // explicit name
capsule_task("task-id")         // alias ~ same thing
```

### 7.3 Capsule Task Methods

| Method | CLI Equivalent | Description |
|--------|---------------|-------------|
| `.path(PATH)` | `<INPUT>` | Input directory or file |
| `.output(PATH)` | `-o` / `--output` | Output capsule path |
| `.name(NAME)` | `--name` | Display name |
| `.version(VER)` | `--version` | Version label |
| `.contents(TYPE)` | `--contents` | Content policy |
| `.storage(MODE)` | -- | `"editable"` or `"archive"` |
| `.capsule_set(NAME)` | `--capsule-set` | Companion set name |
| `.header(MODE)` | `--header` | `"rich"`, `"minimal"`, or `"off"` |
| `.compression(MODE)` | `--compression` | `"zstd"` or `"none"` |
| `.preview_symbols(N)` | `--preview-symbols` | Max preview symbols |
| `.archive(bool)` | `--archive` | Enable archive mode |
| `.editable()` | -- | Force editable mode |
| `.tag(TAG)` | `--tag` | Tag (repeatable via chaining) |
| `.note(NOTE)` | `--note` | Note (repeatable) |
| `.author(AUTHOR)` | `--author` | Author (repeatable) |
| `.meta(KEY, VALUE)` | `--meta` | Custom metadata (repeatable) |
| `.after(TASK_ID)` | -- | Depends on another task |

### 7.4 Capsule Set Auto-Emission Pattern

```kain
fn build(ctx: BuildContext) -> BuildGraph:
    // Source capsule
    let source_capsule = amalgamate_capsule("pack-source")
        .path("src/")
        .output("$root/.kain/capsules/app.kn")
        .contents("source")
        .capsule_set("app")

    // Artifacts companion (auto-discovered by materialize)
    let artifact_capsule = amalgamate_capsule("pack-artifacts")
        .path(".")
        .output("$root/.kain/capsules/app.artifacts.kn")
        .contents("artifacts")
        .capsule_set("app")
        .after(source_capsule)

    return build_graph()
        .task(source_capsule)
        .task(artifact_capsule)
```

______________________________________________________________________

## 8. The Package System: add / install / publish

Amalgamate capsules are the **backend storage format** for Kain's package management. The `kain add`, `kain install`, and `kain publish` commands are the user-facing package workflow.

### 8.1 `kain publish` -- Create Distributable Capsules

```
kain publish [OPTIONS] <INPUT> [-o OUTPUT]
```

Packages a Kain project into one or more capsules, ready for distribution or installation.

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | PathBuf | Required | Source directory |
| `-o` / `--output` | PathBuf | Auto | Output capsule path |
| `--name` | String | Auto | Package name |
| `--version` | String | Auto | Package version |
| `--artifacts` | bool | false | Also emit artifacts companion capsule |
| `--evidence` | bool | false | Also emit evidence companion capsule |
| `--archive` | bool | false | Use archive storage mode |

**Example:**

```powershell
# Publish a package with all three companion capsules
kain publish . --name "my-lib" --version "1.0.0" --artifacts --evidence --archive
```

**Output:**

```
Published my-lib v1.0.0
  source:    .kain/capsules/my-lib.kn
  artifacts: .kain/capsules/my-lib.artifacts.kn
  evidence:  .kain/capsules/my-lib.evidence.kn
```

### 8.2 `kain add` -- Add a Package Dependency

```
kain add <PACKAGE>
```

Records a capsule-backed dependency in the project's `KAIN.lock` lockfile. The `<PACKAGE>` argument can be:

- A package name (resolved from `~/.kain/packages/`)
- A local directory path
- A capsule `.kn` file path

**Example:**

```powershell
kain add ./path/to/mylib.kn
kain add ~/.kain/packages/mylib/1.0.0/source.kn
```

### 8.3 `kain install` -- Install a Package

```
kain install <PACKAGE>
```

Installs a package into `~/.kain/packages/<name>/<version>/`, storing:

- `source.kn` -- source capsule
- `artifacts.kn` -- artifacts capsule (if available)
- `evidence.kn` -- evidence capsule (if available)
- `workspace/` -- materialized workspace directory

### 8.4 Package Storage Layout

```
~/.kain/packages/
  mylib/
    index.json                    # InstalledPackageIndex
    versions/
      0.1.0/
        metadata.json             # InstalledPackageMetadata
        source.kn                 # Source capsule
        artifacts.kn              # Artifacts companion capsule
        evidence.kn               # Evidence companion capsule
        workspace/                # Materialized source tree
          src/
            main.kn
            lib.kn
          build.kn
```

### 8.5 KAIN.lock -- The Lockfile

```toml
schema = 1

[[packages]]
name = "mylib"
version = "1.0.0"
source = "local"
digest = "sha256:abc123..."
kind = "blade"
capsule_set = "mylib"
```

The lockfile pins exact capsule digests -- the same digest in the capsule metadata block. No version range resolution. No network fetches. The digest IS the truth.

______________________________________________________________________

## 9. Materialization: How Capsules Become Imports

Materialization is the bridge between a capsule file and the compiler's module resolution system.

### 9.1 The Materialization Pipeline

```
capsule .kn file
    -> read metadata block (TOML behind //!kain-capsule)
    -> read content blocks (editable or archive mode)
    -> discover companion capsules (same capsule_set, sibling .kn files)
    -> merge companions (source + artifacts + evidence)
    -> compute state hash (primary path + digests + companions)
    -> check cache (.kain/cache/amalgamate/<state-hash>/)
    -> unpack to workspace/ subdirectory (if cache miss)
    -> return MaterializedCapsule { workspace_root }
    -> compiler resolves modules from workspace_root
```

### 9.2 Automatic Materialization

The compiler automatically materializes capsules when they appear as:

- An input path to `kain check`/`kain build`/`kain run`
- A dependency in `KAIN.lock`
- A `build.kn` entry point

The materialization root is `.kain/cache/amalgamate/` in the working directory.

### 9.3 Caching

Materialization is **content-addressed**. The state hash is computed from:

- The primary capsule path
- The primary capsule digest
- All companion capsule digests
- The companion capsule set names

If the state hash matches an existing cache directory, materialization skips unpack -- the workspace is already there.

### 9.4 Capsule Relativity

Capsule-relative paths use forward slashes (`/`) as separators, regardless of platform. This ensures portability across Windows, Linux, and macOS:

```
src/main.kn          ->  ./src/main.kn (Windows)
lib/core/math.kn     ->  ./lib/core/math.kn (all platforms)
```

______________________________________________________________________

## 10. Capsule Augmentation: Reshape & Merge

The `kain amalgamate reshape` command (in development) will provide:

- **Strip:** Remove files matching a pattern from a capsule
- **Extract:** Pull a subset of files into a new capsule
- **Merge:** Combine two or more capsules into one
- **Rebase:** Change capsule root path prefix

These operations work without unpacking -- they operate on the capsule's internal JSON archive directly.

______________________________________________________________________

## 11. Proven Scale: 2,594 Modules, 15 MB, 3 Seconds

The Amalgamate system has been battle-tested at extreme scale:

| Property | Value |
|----------|-------|
| Largest pack (tested) | 2,594 files into one capsule |
| Capsule size | 15 MB (316,135 lines) |
| Pack time | 3 seconds |
| Public symbols preserved | 3,211+ |
| Module imports from capsule | 8 modules, 938 items typechecked |
| Typecheck result | 0 errors |
| LLVM build result | PASS |

**The import scaling is flat:** 6 modules or 2,594 modules -- the compiler doesn't care. Module resolution is the same. Typechecking is the same. Import syntax is the same.

______________________________________________________________________

## 12. Common Workflows

### Workflow 1: Share a Library

```powershell
# In the library project:
cd my-math-lib/
kain amalgamate src/ -o mathlib.kn --name "MathLib" --version "1.0.0"

# In the consumer project:
cp ../my-math-lib/mathlib.kn ./vendor/
# In consumer's main.kn:
use mathlib
```

### Workflow 2: CI Artifact Bundle

```powershell
# Build everything
kain build src/ --target llvm

# Pack source + artifacts as companion capsules
kain amalgamate . -o release/app.kn --contents source --capsule-set "app"
kain amalgamate . -o release/app.artifacts.kn --contents artifacts --capsule-set "app"

# Ship the capsule pair
# Consumer drops both files and runs:
kain check app.kn    # auto-materializes + merges companions
```

### Workflow 3: Archive an Entire Project

```powershell
# Snapshot everything -- every file, every artifact, every telemetry log
kain amalgamate . -o archived.kn --contents snapshot --archive
```

### Workflow 4: Build Pipeline Auto-Capsule

```kain
// In build.kn:
fn build(ctx: BuildContext) -> BuildGraph:
    let build_task = native_executable("app")
        .entry("src/main.kn")
        .output("$root/.kain/out/app.exe")

    let capsule = amalgamate_capsule("dist")
        .path(".")
        .output("$root/.kain/capsules/app.kn")
        .contents("source")
        .capsule_set("app")
        .tag("release")
        .after(build_task)

    return build_graph().task(build_task).task(capsule)
```

Now `kain run` auto-emits the capsule alongside the binary.

______________________________________________________________________

## 13. Capsule Format On-Disk Structure

### Sentinel Markers

| Sentinel | Purpose |
|----------|---------|
| `//!kain-capsule` | Start of metadata block |
| `//!end-kain-capsule` | End of metadata block |
| `//!kain-capsule-payload` | Start of archive payload (base64) |
| `//!end-kain-capsule-payload` | End of archive payload |
| `//!kain-file` | Start of editable file block |
| `//!kain-file-content` | Content separation marker within a file block |
| `//!end-kain-file` | End of editable file block |

### Metadata Fields (TOML format)

```toml
schema = 2
kind = "blade"
storage = "editable"
contents = "source"
compression = "none"
digest = "sha256:abc123..."
created_at = "2026-06-22T12:00:00+00:00"
file_count = 47
module_count = 12
preview_symbol_limit = 40
api_index = "auto"
module_index = "auto"
directories = ["src", "src/lib", "include"]
name = "mylib"
capsule_set = "mylib"
version = "1.2.0"
root_label = "mylib"
entry = "src/main.kn"
manifest = "build.kn"
authors = ["Alice", "Bob"]
notes = ["First release"]
tags = ["math", "3d"]

[meta]
license = "MIT"
homepage = "https://example.com"
```

### Editable File Block Format

```
//!kain-file
// path = "src/main.kn"
// kind = "text"
// encoding = "utf8"
// bytes = 2048
// sha256 = "sha256:def456..."
// trailing_newline = true
//!kain-file-content
// fn main() -> Int:
//     return 0
//!end-kain-file
```

For binary files, `kind = "binary"`, `encoding = "base64"`, and the content is base64-encoded chunks.

### Archive Payload Format

The archive payload is a base64-encoded, optionally zstd-compressed JSON document:

```json
{
  "schema": 2,
  "format": "kain-capsule-archive-json-v1",
  "root_label": "mylib",
  "directories": ["src", "src/lib"],
  "files": [
    {
      "path": "src/main.kn",
      "size_bytes": 2048,
      "sha256": "sha256:def456...",
      "bytes_base64": "Zm4gbWFpbigpIC0+..."
    }
  ],
  "preview": {
    "module_count": 12,
    "total_symbols": 89,
    "sections": [...]
  }
}
```

______________________________________________________________________

## 14. CRATE MAP: Where Everything Lives

| Component | Location | Language |
|-----------|----------|----------|
| **Core amalgamate library** | `crates/amalgamate/src/lib.rs` (2,770 lines) | Rust |
| **CLI dispatch** | `crates/cli/src/amalgamate.rs` | Rust |
| **CLI argument parsing** | `crates/commands/src/kain.rs` (§Amalgamate) | Rust |
| **Command metadata** | `crates/commands/commands/amalgamate.toml` | TOML |
| **Package store (add/install/publish)** | `crates/cli/src/packages.rs` | Rust |
| **Package store logic** | `crates/blades/src/package_store.rs` | Rust |
| **build.kn capsule task** | `crates/blades/src/lib.rs` (§BUILD_TASK_CONSTRUCTORS) | Rust |
| **Build task execution** | `crates/build/src/workspace.rs` (§Amalgamate) | Rust |
| **Materialization cache** | `.kain/cache/amalgamate/` | Runtime dir |
| **Package store** | `~/.kain/packages/` | Runtime dir |
| **Examples & proofs** | `blades/edge_cases/amalgamate/` | Kain |
| **build.kn stdlib** | `stdlib/build.kn` (§BUILD_KIND_AMALGAMATE) | Kain |

______________________________________________________________________

*Document generated 2026-06-22 from source analysis of `crates/amalgamate/`, `crates/cli/`, `crates/commands/`, `crates/blades/`, `crates/build/`, and `blades/edge_cases/amalgamate/`.*
