$ErrorActionPreference = "Stop"

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\..")).Path

Set-Location $RepoRoot

powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/materialize-session-state.ps1"
powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/process-command-queue.ps1"
powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/materialize-shell.ps1"
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/materialize-session-state.ps1"
powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/process-command-queue.ps1"
powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-dcc-suite/scripts/materialize-shell.ps1"
cargo run -p cli --bin kain -- build native-ui apps/kain-fabric-dcc-suite/generated/main.generated.kn --bundle-only --app-name kain-fabric-dcc-suite --window-title "Kain Fabric DCC Suite" -o apps/kain-fabric-dcc-suite/native-app
