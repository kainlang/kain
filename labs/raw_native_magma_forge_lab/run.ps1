param(
    [string]$ExeName = "raw_native_magma_forge_lab.exe",
    [string]$BundleName = "ui_bundle.json",
    [string]$RuntimeContractName = "",
    [switch]$CompatContract
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

$env:KAIN_RUNTIME_CONTRACT_STRICT = if ($CompatContract) { "0" } else { "1" }
Remove-Item Env:KAIN_NATIVE_WORLD_ASSET -ErrorAction SilentlyContinue
Write-Host ("Runtime contract mode: " + $(if ($CompatContract) { "compat" } else { "strict" }))
Write-Host "Launching magma forge fallback world with no external GLB asset."
Write-Host "Launching: $exePath"
Start-Process -FilePath $exePath | Out-Null
