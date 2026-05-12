# Qt Plasma Runtime Lounge

This smoke proves the Qt-backed `kain-ui-native` host can launch a polished session, auto-capture a screenshot, and exit cleanly in a deterministic way.

It is intentionally metadata-driven: the shell is the real generated Qt host, while the document, viewport, devtools, and staged-browser lanes are represented through a curated runtime bundle built in the smoke app.

Run:

```bash
smoketest/UI/qt_plasma_runtime_lounge/run_smoke.sh
```

Outputs:

- `outputs/qt_plasma_runtime_lounge.png`
- `outputs/generated/Main.qml`
- `outputs/generated/session.json`
