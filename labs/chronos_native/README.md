# Chronos Native Lab

This lab is the first native Kain authoring proof for the Chronos direction.

It keeps the source at the app root so `kain native-ui dev` watches the whole lab
tree instead of only a nested `src/` folder.

Suggested commands:

```bash
cargo run -p cli --bin kain -- native-ui dev labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"
```

```bash
cargo run -p cli --bin kain -- build native-ui labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"
```

What this lab proves:

- compiler-owned `world` state drives a desktop shell instead of imported React hooks
- `viewport3d` props, dock layout, tab groups, and theme tokens are authored directly in Kain
- fragment and compute shader artifacts ship from the same file, so shader-side hot reload has a real proof surface
- the dev loop can preserve meaningful app state such as the selected preset, active tab, and dock layout when reload compatibility holds

Validation status:

- `kain build native-ui labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"` passes in this checkout and materializes the runtime, realtime, manifest, snapshot, shader, and compute sidecars.
- `kain native-ui dev labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"` starts the dev loop, launches the packaged child, and watches the lab root, but the child currently exits in this environment because `/usr/local/bin/qmlscene` aborts.
