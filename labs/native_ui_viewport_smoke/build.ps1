param(
    [switch]$SkipCliBuild,
    [switch]$Release
)

$labRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $labRoot "..\..")).Path
$cliPath = Join-Path $repoRoot "target\debug\kain.exe"
$entryPath = Join-Path $labRoot "src\main.kn"
$projectDir = Join-Path $labRoot "native_ui_viewport_smoke-native-ui"
$projectExePath = Join-Path $projectDir "native_ui_viewport_smoke.exe"
$rootExeName = "native_ui_viewport_smoke.exe"
$rootExePath = Join-Path $labRoot $rootExeName
$freshExePath = Join-Path $labRoot "native_ui_viewport_smoke.next.exe"
$versionedExePath = Join-Path $labRoot ("native_ui_viewport_smoke." + (Get-Date -Format "yyyyMMdd-HHmmss") + ".exe")

if (!(Test-Path $entryPath)) {
    throw "Kain entry file not found: $entryPath"
}

if (!(Test-Path $cliPath) -or !$SkipCliBuild) {
    Push-Location $repoRoot
    try {
        cargo build -p cli
    } finally {
        Pop-Location
    }
}

$buildArgs = @(
    "build",
    "native-ui",
    $entryPath,
    "--app-name", "native_ui_viewport_smoke",
    "--window-title", "Kain Native Viewport Lab",
    "--out", $projectDir
)

if ($Release) {
    $buildArgs += "--release"
}

Push-Location $repoRoot
try {
    & $cliPath @buildArgs
} finally {
    Pop-Location
}

if (!(Test-Path $projectExePath)) {
    $projectExePath = Get-ChildItem -Path $projectDir -Filter *.exe -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (!$projectExePath) {
    throw "Expected native UI executable was not produced inside: $projectDir"
}

try {
    Copy-Item $projectExePath $rootExePath -Force -ErrorAction Stop
    if (Test-Path $freshExePath) {
        Remove-Item $freshExePath -Force -ErrorAction SilentlyContinue
    }
    Write-Host "Visual smoke exe ready: $rootExePath"
} catch {
    try {
        Copy-Item $projectExePath $freshExePath -Force -ErrorAction Stop
        Write-Warning "Root smoke exe is locked; wrote fresh build to $freshExePath instead."
        Write-Host "Fresh smoke exe ready: $freshExePath"
    } catch {
        Copy-Item $projectExePath $versionedExePath -Force -ErrorAction Stop
        Write-Warning "Root and .next smoke exes are locked; wrote versioned build to $versionedExePath instead."
        Write-Host "Versioned smoke exe ready: $versionedExePath"
    }
}
