param(
    [string]$Phase2Root = "",
    [string]$ToolRoot = "",
    [switch]$FullWorkspace
)

$ErrorActionPreference = "Stop"
$OuroborosRoot = Split-Path -Parent $PSScriptRoot
if (-not $Phase2Root) { $Phase2Root = Join-Path $OuroborosRoot "out\selfhost\phase2" }
if (-not $ToolRoot) { $ToolRoot = Join-Path $OuroborosRoot "tools\selfhost_repair" }

Push-Location $ToolRoot
try {
    python repair_runner.py repair --validation skip
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

if ($FullWorkspace) {
    Push-Location (Join-Path $Phase2Root "stage2_workspace")
    try {
        cargo check -p cli --bin kain
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}
else {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "selfhost_stage2_core_check.ps1")
    exit $LASTEXITCODE
}
