# Apps

`apps/` holds first-class applications and app prototypes that exercise the full Kain stack. These are product-shaped workflows, not toy samples.

## How To Read It

Use the app README and source for the specific lane you care about, then map what it proves back to the language, runtime, CLI, or packaging guide page. The app folders are especially useful when you want to understand how Kain behaves as a product substrate rather than as a standalone language snippet.

## Key Apps

| App | What It Proves |
| --- | --- |
| `kade-desktop/` | Native desktop app lane and supporting assets |
| `kain-canvas-forge/` | Node-first painting and Three.js composition studio path |
| `kain-fabric-modeler/` | Fabric-first native 3D modeling workbench |
| `kain-fabric-dcc-suite/` | Broader Fabric-first DCC suite scaffold |
| `3D/` | 3D app and tooling lane |
| `ZenDAW/` | Audio/workstation-oriented app lane |

## Output Hygiene

App build outputs are disposable. Keep generated executables, caches, and local preview folders out of git.

## Why It Matters

These folders show how Kain is used when the language owns the product flow, not just the code snippet. They are the best place to see how manifests, runtime services, UI bundles, and target-specific packaging fit together in real apps.
