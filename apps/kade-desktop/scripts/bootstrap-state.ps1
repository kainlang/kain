$ErrorActionPreference = "Stop"
Set-Location (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")
cargo run -p kade-desktop-controller -- --app-root . bootstrap
cargo run -p kade-desktop-controller -- --app-root . generate-shell
