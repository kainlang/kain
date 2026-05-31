param(
    [switch]$NoRun,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\\..\\..")
$compileScript = Join-Path $repoRoot ".agents\\skills\\lang-projects\\scripts\\compile_kain_project_to_root.ps1"
$configPath = Join-Path $bladeRoot "config\\zender.runtime.json"

& (Join-Path $bladeRoot "build-zender-vulkan.ps1") -ConfigPath $configPath

$env:ZENDER_CONFIG = $configPath
$entry = Join-Path $bladeRoot "src\\main.kn"
if ($NoRun) {
    & $compileScript -Entry $entry -OutputName "zender.exe" -BazelConfig $BazelConfig -VerifyLlvm
} else {
    & $compileScript -Entry $entry -OutputName "zender.exe" -BazelConfig $BazelConfig -VerifyLlvm -Run
}
