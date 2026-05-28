param(
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$entry = Join-Path $bladeRoot "src\main.kn"
$exePath = Join-Path $bladeRoot "convergence.exe"

& $compileScript -Entry $entry -OutputName "convergence.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!(Test-Path $exePath)) {
    throw "Expected executable was not created: $exePath"
}

Push-Location $bladeRoot
try {
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "convergence.exe exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

Write-Host "[PASS] convergence built and executed: $exePath"
