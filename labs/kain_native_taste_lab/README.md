# Kain Native Taste Lab

This lab is a smaller native-ui probe than `labs/chronos_native`.

It is meant to answer one practical question quickly:

Can the current native Kain desktop pipeline materialize, launch, watch, and
reload a stateful docked shell without dragging in the bigger DCC scaffolds?

What it covers:

- compiler-owned `world` state
- patch-driven button interactions
- dock layout plus tab groups
- `viewport3d` props
- fragment-shader canvas sidecars
- native-ui bundle materialization and dev-loop watching

Suggested commands:

```bash
cargo run -p cli --bin kain -- build native-ui labs/kain_native_taste_lab/main.kn --app-name kain-native-taste-lab --window-title "Kain Native Taste Lab"
```

```bash
cargo run -p cli --bin kain -- native-ui dev labs/kain_native_taste_lab/main.kn --app-name kain-native-taste-lab --window-title "Kain Native Taste Lab"
```

Expected behavior in this checkout:

- `build native-ui` should materialize the desktop bundle and sidecars cleanly.
- `native-ui dev` should start the watch loop and launch the packaged child.
- The child may still exit in this environment if the Qt/QML host path fails,
  which is a host/runtime issue rather than a lab-source issue.
