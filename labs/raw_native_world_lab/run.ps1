param(
    [string]$ExeName = "raw_native_world_lab.exe",
    [string]$BundleName = "ui_bundle.json",
    [string]$RuntimeContractName = "",
    [string]$AssetName = ""
)

$labRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $labRoot $ExeName
$bundlePath = Join-Path $labRoot $BundleName
$runtimeContractFile = if ($RuntimeContractName) {
    $RuntimeContractName
} else {
    "{0}.runtime_contract.json" -f [System.IO.Path]::GetFileNameWithoutExtension($ExeName)
}
$runtimeContractPath = Join-Path $labRoot $runtimeContractFile
$assetsRoot = Join-Path $labRoot "assets"
$assetPath = $null

if (!(Test-Path $exePath)) {
    & (Join-Path $labRoot "build.ps1")
}

if (Test-Path $bundlePath) {
    $env:KAIN_NATIVE_UI_BUNDLE = $bundlePath
    Write-Host "Using compiled UI bundle: $bundlePath"
} else {
    Remove-Item Env:KAIN_NATIVE_UI_BUNDLE -ErrorAction SilentlyContinue
    Write-Host "No local UI bundle found. Launching raw viewport without compiled UI metadata."
}

if (Test-Path $runtimeContractPath) {
    $env:KAIN_RUNTIME_CONTRACT = $runtimeContractPath
    Write-Host "Using runtime contract: $runtimeContractPath"
} else {
    Remove-Item Env:KAIN_RUNTIME_CONTRACT -ErrorAction SilentlyContinue
    Write-Host "No explicit runtime contract env set. The native runtime will try the exe sidecar automatically."
}

if ($AssetName) {
    $assetPath = Join-Path $assetsRoot $AssetName
} else {
    $glbFiles = Get-ChildItem -Path $assetsRoot -Filter *.glb -File -ErrorAction SilentlyContinue | Sort-Object Name
    if ($glbFiles -and $glbFiles.Count -gt 0) {
        $assetPath = $glbFiles[0].FullName
    }
}

if ($assetPath -and (Test-Path $assetPath)) {
    $env:KAIN_NATIVE_WORLD_ASSET = $assetPath
    Write-Host "Using native world asset: $assetPath"
} else {
    Remove-Item Env:KAIN_NATIVE_WORLD_ASSET -ErrorAction SilentlyContinue
    Write-Host "No local .glb found. Launching fallback procedural world."
}

Write-Host "Launching: $exePath"
Start-Process -FilePath $exePath | Out-Null
