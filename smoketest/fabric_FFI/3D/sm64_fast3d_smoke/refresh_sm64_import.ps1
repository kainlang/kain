param(
    [string]$ProfilePath = "$PSScriptRoot\sm64_import_profile.render_us.json",
    [string]$SourceRoot,
    [string]$OutputTag = "latest"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\.."))
}

function Resolve-ProfilePath([string]$pathValue) {
    if ([System.IO.Path]::IsPathRooted($pathValue)) {
        return $pathValue
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $pathValue))
}

function Resolve-ImportSummaryLine([pscustomobject]$report) {
    return "discovered=$($report.discovered_files) imported=$($report.imported_files) skipped=$($report.skipped_files) failed=$($report.failed_files.Count)"
}

function Get-TopFailureGroups([pscustomobject]$report) {
    return $report.failed_files |
        Group-Object {
            $filePath = $_.file
            if ($filePath -match "\\actors\\") { "actors" }
            elseif ($filePath -match "\\levels\\") { "levels" }
            elseif ($filePath -match "\\src\\game\\") { "src/game" }
            elseif ($filePath -match "\\src\\engine\\") { "src/engine" }
            elseif ($filePath -match "\\src\\buffers\\") { "src/buffers" }
            elseif ($filePath -match "\\include\\") { "include" }
            else { "other" }
        } |
        Sort-Object Count -Descending |
        Select-Object -First 6
}

$resolvedProfilePath = Resolve-ProfilePath $ProfilePath
$profile = Get-Content -Raw $resolvedProfilePath | ConvertFrom-Json

$repoRoot = Resolve-RepoRoot
$sourceRootValue = if ([string]::IsNullOrWhiteSpace($SourceRoot)) { $profile.source_root } else { $SourceRoot }
$resolvedSourceRoot = [System.IO.Path]::GetFullPath($sourceRootValue)
$kainExePath = Join-Path $repoRoot "target\debug\kain.exe"

if (-not (Test-Path $kainExePath)) {
    throw "Missing Kain CLI binary at $kainExePath. Build it first with cargo build -p cli."
}

if (-not (Test-Path $resolvedSourceRoot)) {
    throw "Missing SM64 source root at $resolvedSourceRoot."
}

$outputDir = Join-Path $repoRoot ("generated\sm64_import_refresh_{0}" -f $OutputTag)
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$outputStem = $profile.output_stem
$outputKnPath = Join-Path $outputDir ("{0}.kn" -f $outputStem)
$outputReportPath = Join-Path $outputDir ("{0}.import_report.json" -f $outputStem)

$importArgs = @(
    "import-c"
    $resolvedSourceRoot
    "--output"
    $outputKnPath
    "--report-json"
    $outputReportPath
    "--flat"
)

foreach ($filterValue in $profile.include_filters) {
    $importArgs += @("--include", $filterValue)
}

foreach ($filterValue in $profile.exclude_filters) {
    $importArgs += @("--exclude", $filterValue)
}

foreach ($includePathValue in $profile.include_paths) {
    $resolvedIncludePath = [System.IO.Path]::GetFullPath((Join-Path $resolvedSourceRoot $includePathValue))
    $importArgs += @("-I", $resolvedIncludePath)
}

foreach ($defineValue in $profile.defines) {
    $importArgs += @("-D", $defineValue)
}

Write-Host "Using profile: $resolvedProfilePath"
Write-Host "Source root:   $resolvedSourceRoot"
Write-Host "Output dir:    $outputDir"
Write-Host "Output stem:   $outputStem"
Write-Host ""

& $kainExePath @importArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path $outputReportPath)) {
    Write-Host ""
    Write-Host "Import completed without a failure report."
    exit 0
}

$report = Get-Content -Raw $outputReportPath | ConvertFrom-Json
$topFailureGroups = Get-TopFailureGroups $report

Write-Host ""
Write-Host "Import summary: $(Resolve-ImportSummaryLine $report)"
Write-Host "Top failing groups:"

foreach ($group in $topFailureGroups) {
    Write-Host ("  {0,5}  {1}" -f $group.Count, $group.Name)
}
