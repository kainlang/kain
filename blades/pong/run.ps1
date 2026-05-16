param(
    [switch]$Interactive,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [int]$FrameBudget = 0
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$prebuildScript = Join-Path $bladeRoot "build-pong-window.ps1"
$entry = Join-Path $bladeRoot "src\main.kn"
$exePath = Join-Path $bladeRoot "pong.exe"
$runRoot = Join-Path $bladeRoot ".kain\run"
$screenshotPath = Join-Path $runRoot "pong.bmp"
$llvmPath = Join-Path $bladeRoot ".kain\out\pong\pong.ll"
$bcPath = Join-Path $bladeRoot ".kain\out\pong\pong.bc"
$reportPath = Join-Path $runRoot "pong_report.txt"
$windowReportPath = Join-Path $runRoot "pong_window_report.txt"
$configPath = Join-Path $bladeRoot "config\pong_demo.json"

New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
if (Test-Path $screenshotPath) {
    Remove-Item -LiteralPath $screenshotPath -Force
}

& $prebuildScript

& $compileScript -Entry $entry -OutputName "pong.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!(Test-Path $exePath)) {
    throw "Expected executable was not created: $exePath"
}

if (Test-Path $llvmPath) {
    & (Join-Path $repoRoot "toolchain\llvm\bin\llvm-as.exe") $llvmPath -o $bcPath
    if ($LASTEXITCODE -ne 0) {
        throw "llvm-as verification failed with exit code $LASTEXITCODE"
    }
}

$env:KAIN_PONG_CONFIG = $configPath
if ($FrameBudget -gt 0) {
    $env:KAIN_PONG_FRAME_BUDGET = [string]$FrameBudget
}
if (!$Interactive) {
    $env:PONG_WINDOW_SCREENSHOT_PATH = $screenshotPath
    $env:PONG_WINDOW_SCREENSHOT_FRAME = "168"
}

Push-Location $bladeRoot
try {
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "pong.exe exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
    Remove-Item Env:KAIN_PONG_CONFIG -ErrorAction SilentlyContinue
    Remove-Item Env:KAIN_PONG_FRAME_BUDGET -ErrorAction SilentlyContinue
    Remove-Item Env:PONG_WINDOW_SCREENSHOT_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:PONG_WINDOW_SCREENSHOT_FRAME -ErrorAction SilentlyContinue
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

Write-Host "[PASS] pong built and executed: $exePath"
Write-Host "[PASS] report: $reportPath"
Write-Host "[PASS] presenter report: $windowReportPath"
if (!$Interactive) {
    Write-Host "[PASS] screenshot: $screenshotPath"
}
