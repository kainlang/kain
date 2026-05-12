param(
    [switch]$Release
)

$ErrorActionPreference = "Stop"

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SmokeRoot = (Resolve-Path $SmokeRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $SmokeRoot "..\..\..")).Path

Set-Location $RepoRoot

$Arguments = @(
    "run",
    "-p",
    "cli",
    "--bin",
    "kain",
    "--",
    "build",
    "native-ui",
    "smoketest/UI/kinetic_ui_atlas/showcase.kn",
    "--app-name",
    "kinetic-ui-atlas",
    "--window-title",
    "Kinetic UI Atlas",
    "-o",
    "smoketest/UI/kinetic_ui_atlas/native-app"
)

if ($Release) {
    $Arguments += "--release"
}

cargo @Arguments
