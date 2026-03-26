$ErrorActionPreference = "Stop"

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\..")).Path

Set-Location $RepoRoot

powershell -ExecutionPolicy Bypass -File "apps/kain-fabric-modeler/scripts/materialize-shell.ps1"
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
cargo run -p cli --bin kain -- build native-ui apps/kain-fabric-modeler/generated/main.generated.kn --bundle-only --app-name kain-fabric-modeler --window-title "Kain Fabric Modeler" -o apps/kain-fabric-modeler/native-app
