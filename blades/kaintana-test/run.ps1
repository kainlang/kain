param(
    [switch]$NoRun,
    [ValidateSet("desktop", "vulkan", "all")]
    [string]$Backend = "all",
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$kaintanaDesktopBuild = Join-Path $repoRoot "blades\kaintana\build-desktop.ps1"
$vulkainBuild = Join-Path $repoRoot "blades\vulkain\build-vulkain.ps1"
$vulkainNativeBridge = Join-Path $repoRoot "blades\vulkain\.kain\native\vulkain_bridge.dll"
$rootBridge = Join-Path $bladeRoot "vulkain_bridge.dll"
$rootExe = Join-Path $bladeRoot "kaintana-test.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$runOut = Join-Path $bladeRoot ".kain\run"

& $kaintanaDesktopBuild
& $vulkainBuild

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

if (Test-Path $vulkainNativeBridge) {
    Copy-Item -LiteralPath $vulkainNativeBridge -Destination $rootBridge -Force
}

function Invoke-KaintanaRun([string]$SelectedBackend) {
    $entry = if ($SelectedBackend -eq "vulkan") {
        Join-Path $bladeRoot "entrypoints\vulkan.kn"
    } else {
        Join-Path $bladeRoot "src\main.kn"
    }

    if ($KainBin) {
        & $compileScript -Entry $entry -OutputName "kaintana-test.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $KainBin -CompilerBuild auto
    } else {
        & $compileScript -Entry $entry -OutputName "kaintana-test.exe" -BazelConfig $BazelConfig -VerifyLlvm
    }

    if (Test-Path $vulkainNativeBridge) {
        Copy-Item -LiteralPath $vulkainNativeBridge -Destination $rootBridge -Force
    }

    if (!$NoRun) {
        & $rootExe
    }
}

Push-Location $bladeRoot
try {
    if ($Backend -eq "desktop" -or $Backend -eq "all") {
        Invoke-KaintanaRun "desktop"
    }
    if ($Backend -eq "vulkan" -or $Backend -eq "all") {
        Invoke-KaintanaRun "vulkan"
    }
} finally {
    Pop-Location
}
