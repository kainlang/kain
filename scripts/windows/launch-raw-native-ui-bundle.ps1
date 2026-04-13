param(
    [string]$ExePath = "M:\Code\Kain\generated\_3d_cube_compiled_ui.exe",
    [string]$BundlePath = "M:\Code\Kain\_codex_smoke\out_luminous_v2\native-bundle-smoke-native-ui\generated\native_app_bundle.json"
)

if (!(Test-Path $ExePath)) {
    throw "Raw native executable not found: $ExePath"
}

if (!(Test-Path $BundlePath)) {
    throw "Compiled UI bundle not found: $BundlePath"
}

$env:KAIN_NATIVE_UI_BUNDLE = $BundlePath
Write-Host "Launching raw native Kain app with compiled UI bundle:"
Write-Host "  exe    = $ExePath"
Write-Host "  bundle = $BundlePath"
Start-Process -FilePath $ExePath | Out-Null
