param(
    [string]$CliPath = ""
)

$labRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $labRoot "..\..")).Path
$generatedRoot = Join-Path $labRoot "generated"
$defaultCliPath = Join-Path $repoRoot "target\debug\kain.exe"

if ([string]::IsNullOrWhiteSpace($CliPath)) {
    $CliPath = $defaultCliPath
}

if (!(Test-Path $CliPath)) {
    $fallbackTargetDir = Join-Path $repoRoot "target\llvmzone-cli-build"
    $env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY = "1"
    Push-Location $repoRoot
    try {
        cargo build -p cli --target-dir $fallbackTargetDir
    } finally {
        Pop-Location
    }

    $CliPath = Join-Path $fallbackTargetDir "debug\kain.exe"
    if (!(Test-Path $CliPath)) {
        throw "Unable to locate a Kain CLI binary after fallback build."
    }
}

New-Item -ItemType Directory -Force -Path $generatedRoot | Out-Null

$appsRoot = Join-Path $labRoot "apps"
$apps = Get-ChildItem -Directory $appsRoot | Sort-Object Name

if ($apps.Count -lt 5) {
    throw "Expected five LLVM Zone apps under $appsRoot."
}

$builtExecutables = @()

foreach ($app in $apps) {
    $entry = Join-Path $app.FullName "src\main.kn"
    if (!(Test-Path $entry)) {
        throw "Kain entry file not found: $entry"
    }

    $outputLl = Join-Path $generatedRoot ($app.Name + ".ll")
    Write-Host "Building $($app.Name)..."
    & $CliPath build $entry --target llvm --output $outputLl
    if ($LASTEXITCODE -ne 0) {
        throw "LLVM build failed for $($app.Name)"
    }

    $exeCandidates = @(
        [System.IO.Path]::ChangeExtension($outputLl, ".exe"),
        [System.IO.Path]::ChangeExtension($outputLl, $null)
    )

    $exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exePath) {
        throw "Expected executable not found for $($app.Name)"
    }

    $builtExecutables += $exePath
    Write-Host "Ready: $exePath"
}

Write-Host ""
Write-Host "LLVM Zone build complete."
Write-Host "Executables:"
foreach ($exe in $builtExecutables) {
    Write-Host " - $exe"
}
