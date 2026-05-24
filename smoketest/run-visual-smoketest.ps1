param(
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$smoketestRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $smoketestRoot "..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$prebuildScript = Join-Path $smoketestRoot "build-smoketest-visualizer-bridge.ps1"
$entry = Join-Path $smoketestRoot "src\main.kn"
$exePath = Join-Path $smoketestRoot "smoketest.exe"
$visualTelemetryRoot = Join-Path $smoketestRoot "telemetry\visual"
$uiReportPath = Join-Path $visualTelemetryRoot "notes\ui_dashboard.json"
$openglReportPath = Join-Path $visualTelemetryRoot "notes\opengl_album.json"
$windowReportPath = Join-Path $visualTelemetryRoot "notes\opengl_window_report.txt"

New-Item -ItemType Directory -Force -Path $visualTelemetryRoot | Out-Null

& $prebuildScript
& $compileScript -Entry $entry -OutputName "smoketest.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!(Test-Path $exePath)) {
    throw "Expected executable was not created: $exePath"
}

$env:KAIN_SMOKETEST_MODE = "visual"

Push-Location $smoketestRoot
try {
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "smoketest.exe exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
    Remove-Item Env:KAIN_SMOKETEST_MODE -ErrorAction SilentlyContinue
}

if (!(Test-Path $uiReportPath)) {
    throw "Expected UI dashboard note was not created: $uiReportPath"
}
if (!(Test-Path $openglReportPath)) {
    throw "Expected OpenGL album note was not created: $openglReportPath"
}
if (!(Test-Path $windowReportPath)) {
    throw "Expected OpenGL window report was not created: $windowReportPath"
}

Write-Host "[PASS] smoketest visual mode executed: $exePath"
Write-Host "[PASS] ui note: $uiReportPath"
Write-Host "[PASS] opengl note: $openglReportPath"
Write-Host "[PASS] window report: $windowReportPath"
