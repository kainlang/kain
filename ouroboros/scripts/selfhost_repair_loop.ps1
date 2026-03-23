param(
    [string]$Phase2Root = "M:\Code\OuroborosV2\out\selfhost\phase2",
    [string]$ToolRoot = "M:\Code\OuroborosV2\tools\selfhost_repair",
    [switch]$FullWorkspace
)

$ErrorActionPreference = "Stop"

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
    & powershell -NoProfile -ExecutionPolicy Bypass -File "M:\Code\OuroborosV2\scripts\selfhost_stage2_core_check.ps1"
    exit $LASTEXITCODE
}
