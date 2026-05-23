param(
    [switch]$Interactive,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [int]$FrameBudget = 180
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$prebuildScript = Join-Path $bladeRoot "build-neural-lattice-bridge.ps1"
$entry = Join-Path $bladeRoot "src\main.kn"
$exePath = Join-Path $bladeRoot "neural_lattice.exe"
$runRoot = Join-Path $bladeRoot ".kain\run"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$reportPath = Join-Path $runRoot "neural_lattice_report.txt"
$windowReportPath = Join-Path $runRoot "neural_lattice_window_report.txt"
$screenshotPath = Join-Path $runRoot "neural_lattice.bmp"

New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
if (Test-Path $screenshotPath) {
    Remove-Item -LiteralPath $screenshotPath -Force
}

& $prebuildScript

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
if (!$Interactive) {
    $env:NEURAL_LATTICE_SCREENSHOT_PATH = $screenshotPath
    if ($FrameBudget -gt 0) {
        $env:NEURAL_LATTICE_FRAME_BUDGET = [string]$FrameBudget
    }
}

& $compileScript -Entry $entry -OutputName "neural_lattice.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!(Test-Path $exePath)) {
    throw "Expected executable was not created: $exePath"
}

Push-Location $bladeRoot
try {
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "neural_lattice.exe exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
    Remove-Item Env:KAIN_RUNTIME_CACHE_DIR -ErrorAction SilentlyContinue
    Remove-Item Env:NEURAL_LATTICE_SCREENSHOT_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:NEURAL_LATTICE_FRAME_BUDGET -ErrorAction SilentlyContinue
}

if (!(Test-Path $reportPath)) {
    throw "Expected report was not created: $reportPath"
}

if (!(Test-Path $windowReportPath)) {
    throw "Expected presenter report was not created: $windowReportPath"
}

if (!$Interactive) {
    if (!(Test-Path $screenshotPath)) {
        throw "Expected screenshot was not created: $screenshotPath"
    }
    if ((Get-Item $screenshotPath).Length -le 1024) {
        throw "Screenshot was too small: $screenshotPath"
    }
}

Write-Host "[PASS] neural_lattice built and executed: $exePath"
Write-Host "[PASS] report: $reportPath"
Write-Host "[PASS] presenter report: $windowReportPath"
if (!$Interactive) {
    Write-Host "[PASS] screenshot: $screenshotPath"
}
