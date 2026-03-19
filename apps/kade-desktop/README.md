# Kade Desktop For Kain

This app is the first native Kain shell for the Kade product shape: chat, tools, providers, file operations, and agent control in one desktop executable-oriented workspace.

The project is data-driven first. The `config/` directory is the source of truth for the product shell:

- `app_manifest.json` defines runtime identity, required capabilities, and manifest wiring.
- `panels.json` defines the shell layout and panel responsibilities.
- `commands.json` defines user-facing actions and command routing.
- `providers.json` defines AI provider profiles and capability flags.
- `tools.json` defines tool contracts and permission boundaries.

The controller now ingests those manifests and emits:

- `state/runtime_snapshot.json` as the runtime contract
- `generated/main.generated.kn` as the manifest-backed Kain shell source used for native builds

`src/main.kn` is now only a thin bootstrap shell so the handwritten app source stops duplicating providers, commands, tools, and session wiring.

The app controller now lives in `controller/`. It loads the manifests, bootstraps runtime state, persists sessions, persists active provider selection, persists tool approvals, and emits `state/runtime_snapshot.json` for runtime consumption.

## Suggested Commands

```powershell
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop bootstrap
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop generate-shell
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop create-session --title "First native Kade session"
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop set-provider --provider openrouter
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop set-provider-profile --provider openrouter --json "{\"base_url\":\"https://openrouter.ai/api/v1\",\"model\":\"anthropic/claude-sonnet-4\"}"
cargo run -p kade-desktop-controller -- --app-root apps/kade-desktop approve-tool --tool read_file --decision allow
cargo run -p cli --bin kain -- run generated/main.generated.kn
cargo run -p cli --bin kain -- build native-ui generated/main.generated.kn --bundle-only --app-name kade-desktop --window-title "Kade Desktop" -o native-app
cargo run -p cli --bin kain -- build native-ui generated/main.generated.kn --release --app-name kade-desktop --window-title "Kade Desktop" -o native-app
```
