param(
    [switch]$Software,
    [switch]$Trace,
    [switch]$Inspector
)

$labRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $labRoot "native_ui_viewport_smoke.exe"
$freshExePath = Join-Path $labRoot "native_ui_viewport_smoke.next.exe"
$latestVersionedExe = Get-ChildItem -Path $labRoot -Filter "native_ui_viewport_smoke.20*.exe" -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1

if (!(Test-Path $exePath) -and !(Test-Path $freshExePath) -and !$latestVersionedExe) {
    & (Join-Path $labRoot "build.ps1")
    $latestVersionedExe = Get-ChildItem -Path $labRoot -Filter "native_ui_viewport_smoke.20*.exe" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
}

$candidates = @()
if (Test-Path $exePath) { $candidates += Get-Item $exePath }
if (Test-Path $freshExePath) { $candidates += Get-Item $freshExePath }
if ($latestVersionedExe) { $candidates += $latestVersionedExe }

$launchExePath = $candidates |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1 -ExpandProperty FullName

$env:KAIN_UI_NATIVE_VIEWPORT_RENDERER = if ($Software) { "glow" } else { "wgpu" }
$env:KAIN_UI_NATIVE_SHOW_INSPECTOR = if ($Inspector) { "1" } else { "0" }
$env:KAIN_UI_NATIVE_VIEWPORT_MAX_AXIS = "720"
$env:KAIN_UI_NATIVE_VIEWPORT_INTERACTIVE_MS = "33"
$env:KAIN_UI_NATIVE_VIEWPORT_IDLE_MS = "90"
$env:KAIN_UI_NATIVE_VIEWPORT_STARTUP_MS = "120"

if ($Trace) {
    $env:KAIN_UI_NATIVE_TRACE = "1"
    Write-Host ("Trace log: " + [System.IO.Path]::GetTempPath() + "kain-ui-native-trace.log")
} else {
    Remove-Item Env:KAIN_UI_NATIVE_TRACE -ErrorAction SilentlyContinue
}

Write-Host ("Renderer preference: " + $env:KAIN_UI_NATIVE_VIEWPORT_RENDERER)
Write-Host ("Runtime inspector: " + $(if ($Inspector) { "on" } else { "off" }))
Write-Host ("Launching visual smoke viewport: " + $launchExePath)
Start-Process -FilePath $launchExePath | Out-Null
