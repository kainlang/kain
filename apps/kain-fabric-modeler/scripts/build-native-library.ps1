$ErrorActionPreference = "Stop"

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$SourcePath = Join-Path $AppRoot "native/modeler_ops.c"
$OutputPath = Join-Path $AppRoot "native/modeler_ops.dll"

clang -shared -O2 -o $OutputPath $SourcePath

if ($LASTEXITCODE -ne 0) {
    throw "Failed to build $OutputPath."
}

Write-Host "Built $OutputPath"
