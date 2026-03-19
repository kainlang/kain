# Kade Porting Map

## Kade subsystem -> Kain desktop target

- `Task` and task persistence -> native chat/session controller backed by actor and async runtime lanes
- `ProviderSettingsManager` and `api/providers` -> data-driven provider registry plus host/API bridge contracts
- `registerCommands` -> app command bus sourced from `config/commands.json`
- `ClineProvider` webview shell -> native UI shell in `src/main.kn`
- theme integration -> compiler-owned theme registry plus desktop theme manifests
- agent manager and group chat -> native multi-agent control surface once the base shell is stable

## Rules

- Keep product structure in manifests, not hardcoded controller branches.
- Keep provider contracts independent from UI layout.
- Keep tool permissions explicit and serializable.
- Keep runtime dependencies declared in `app_manifest.json`.
