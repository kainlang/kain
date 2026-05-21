# Kain Packages

`packages/` is the official first-party package workspace. `blades/` can stay the forge and proving ground; packages here are the ones intended to become stable ecosystem artifacts.

Registry nouns:

- `packages`: stable identity, slug, domain, package class, trust tier, public/private visibility, owner metadata.
- `package_versions`: immutable published versions plus manifest/compatibility data, including future `KAIN.toml` and amalgamate capsule metadata.
- `package_artifacts`: downloadable capsules, archives, native bundles, installers, or target-specific payloads.
- `package_dependencies`: version-scoped dependency edges with runtime/build/dev/platform/native kinds.
- `package_owners`: maintainer, publisher, and owner roles for each package.

Current official domains:

- `tools/`: compiler tools, CLIs, package utilities, diagnostics, and developer workflow packages.
- `graphics/`: rendering, GPU, shader, asset, and graphics-adjacent packages.
- `ui/`: Kaintana, native UI helpers, widgets, and UI runtime packages.
- `platform/`: OS, native ABI, C bridge, Vulkan/D3D/Metal-adapter, process, filesystem, and platform integration packages.

Package graduation rule:

1. Prove the idea in `blades/` or `benchmark/`.
2. Give it a `KAIN.toml` manifest and an honest artifact story.
3. Move or mirror the stable package into the right `packages/<domain>/` lane.
4. Publish it to the website registry as `package -> version -> artifact -> dependencies -> owners`.

Do not design package code around the website schema. The website registry records the package truth; the package itself is still owned by Kain source, `KAIN.toml`, native artifacts, and proof/benchmark evidence.
