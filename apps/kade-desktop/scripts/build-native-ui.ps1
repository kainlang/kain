$ErrorActionPreference = "Stop"
Set-Location (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")
cargo run -p kade-desktop-controller -- --app-root . bootstrap | Out-Null
cargo run -p kade-desktop-controller -- --app-root . generate-shell | Out-Null
cargo run -p cli --bin kain -- build native-ui generated/main.generated.kn --bundle-only --app-name kade-desktop --window-title "Kade Desktop" -o native-app
