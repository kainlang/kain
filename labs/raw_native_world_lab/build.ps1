param(
    [string]$Entry = "src/main.kn",
    [string]$OutputLl = "raw_native_world_lab.ll",
    [switch]$SkipCliBuild
)

$labRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $labRoot "..\..")).Path
$cliPath = Join-Path $repoRoot "target\debug\kain.exe"
$entryPath = Join-Path $labRoot $Entry
$outputPath = Join-Path $labRoot $OutputLl

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

Push-Location $repoRoot
try {
    & $cliPath build $entryPath --target llvm -o $outputPath
} finally {
    Pop-Location
}
