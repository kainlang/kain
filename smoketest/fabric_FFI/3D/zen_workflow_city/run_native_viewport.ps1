param(
    [string]$AssetName = "workflow_city_mutated.glb"
)

$smokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$assetPath = Join-Path (Join-Path $smokeRoot "outputs") $AssetName
$labRoot = "M:/Code/Kain/labs/raw_native_world_lab"
$exePath = Join-Path $labRoot "raw_native_world_lab.exe"

if (!(Test-Path $assetPath)) {
    throw "Expected generated asset at $assetPath. Run run_all.bat first."
}

if (!(Test-Path $exePath)) {
    & (Join-Path $labRoot "build.ps1")
}

$env:KAIN_NATIVE_WORLD_ASSET = $assetPath
$env:KAIN_RUNTIME_CONTRACT_STRICT = "0"
Write-Host "Launching raw native viewport with generated asset: $assetPath"
Start-Process -FilePath $exePath | Out-Null
