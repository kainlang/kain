param(
    [switch]$NoRun,
    [switch]$SkipShaderCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$runRoot = Join-Path $bladeRoot ".kain\run"

New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$prebuildScript = Join-Path $bladeRoot "build-vulkan-bridge.ps1"
if ($SkipShaderCompile) {
    & $prebuildScript -SkipShaderCompile
} else {
    & $prebuildScript
}

$env:KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH = Join-Path $runRoot "kquantum_ui.bmp"
$env:KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES = "8"

$entry = Join-Path $bladeRoot "src\main.kn"
if ($NoRun) {
    & $compileScript -Entry $entry -OutputName "kain-labs.exe" -BazelConfig $BazelConfig -VerifyLlvm
} else {
    & $compileScript -Entry $entry -OutputName "kain-labs.exe" -BazelConfig $BazelConfig -VerifyLlvm -Run
}
