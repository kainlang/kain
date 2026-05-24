# Package Capsules

Kain packages now travel as amalgamate capsules and install into the Kain-owned
package store. This gives Kain a Python-like "install once, use from anywhere"
operator flow without making hidden machine state the only dependency model.

## Mental Model

- `use ...` stays the Kain module and package import surface.
- `import ...` is intentionally left free for future foreign ecosystems such as
  Python.
- `kain publish` turns a package, blade, or project root into a portable source
  capsule.
- `kain install` installs a capsule into the global Kain package store.
- `kain add` installs or reuses a package, records the dependency in
  `KAIN.toml`, and pins it in `KAIN.lock`.

The public package format is the same capsule family already used by
`kain amalgamate`: source-first `.kn` capsules with optional `artifacts` and
`evidence` companions in the same capsule set.

## Commands

Publish a local package root as a source capsule:

```powershell
kain publish blades\kaintana
```

Install a capsule or a local package root into the global store:

```powershell
kain install blades\kaintana
kain install .kain\publish\kaintana-0.3.0.kn
```

Add a package to the current project and pin it:

```powershell
kain add kaintana
kain add .\vendor\kaintana
kain add .kain\publish\kaintana-0.3.0.kn
```

You can override the version with `--version` and point `kain add` at a
specific project root or `KAIN.toml` path with `--manifest`.

## Package Store Layout

Installed packages live under the Kain home package root:

```text
~/.kain/packages/
  kaintana/
    package-index.json
    versions/
      0.3.0/
        source.kn
        artifacts.kn
        evidence.kn
        package-install.json
        workspace/
          KAIN.toml
          src/
            kaintana.kn
```

Key files:

- `package-index.json` records the installed versions and the active version.
- `versions/<version>/workspace/` is the materialized capsule workspace used by
  module resolution.
- `package-install.json` records the installed capsule digest and companion
  capsule metadata.

Use `kain doctor` to see the active Kain home and package store roots.

## Project Truth

Project-local dependency truth lives in two places:

- `KAIN.toml` records declared dependencies under `[blade].dependencies`.
- `KAIN.lock` pins the exact installed version and digest.

Example `KAIN.lock`:

```toml
schema = 1

[[packages]]
name = "kaintana"
version = "0.3.0"
source = "kain_home"
digest = "sha256:..."
kind = "blade"
capsule_set = "kaintana"
```

The lockfile is what keeps the Python-like package experience from collapsing
into ambient machine-state chaos.

## Resolver Order

When Kain resolves `use some_package`, it now searches in this order:

1. importer-relative and local filesystem candidates
2. local workspace and blade module roots
3. installed package workspace roots declared by `KAIN.lock` or
   `[blade].dependencies`
4. ambient installed package module roots from `~/.kain/packages` as the final
   scratch-mode fallback

That order keeps real projects explicit and reproducible while still letting a
small script or scratch file benefit from globally installed finished packages.

## Capsule Companions

If a source capsule has sibling companion capsules in the same capsule set,
`kain install` stages them next to the installed source capsule and records them
in the installed package metadata. The installed module resolver uses the
materialized `workspace/` tree for source imports, while artifact and evidence
companions remain available as part of the installed package set for future
tooling and runtime consumers.

## Current Scope

The current package lane is intentionally local-first:

- local package root to capsule via `kain publish`
- local package root or source capsule to global store via `kain install`
- project pinning via `kain add`

Remote registries, auto-fetch, and foreign package ecosystems are future work.
The core install graph, lockfile, package-store layout, and resolver semantics
are the shipped foundation.
