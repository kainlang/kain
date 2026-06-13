# ============================================================================
# test_ouroboros.ps1 — Test Phase 1 (source concatenation) of the ouroboros pipeline
# ============================================================================
# Verifies:
#  1. All 23 files in source_order exist
#  2. Combined output is non-empty
#  3. Reports any missing files
#  4. Prints per-file stats
#
# Usage: .\scripts\test_ouroboros.ps1
# ============================================================================

$ErrorActionPreference = "Continue"

$BladeRoot = Resolve-Path "$PSScriptRoot\.."
$SrcDir    = "$BladeRoot\src"
$CombinedDir = "$BladeRoot\combined"

# Source order from KAIN.toml [source_order]
$SourceOrder = @(
    "token.kn",
    "error.kn",
    "span.kn",
    "ast.kn",
    "build.kn",
    "lexer.kn",
    "builtins.kn",
    "runtime.kn",
    "llvm_ffi.kn",
    "jit_metal.kn",
    "jit_x86.kn",
    "jit_orc.kn",
    "jit_cache.kn",
    "jit.kn",
    "parser.kn",
    "types.kn",
    "effects.kn",
    "monomorphize.kn",
    "codegen.kn",
    "orchestrator.kn",
    "compiler.kn",
    "cli.kn",
    "main.kn"
)

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  Ouroboros Phase 1 Test - Source Concatenation" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

$missingCount = 0
$totalLines = 0
$totalBytes = 0
$fileResults = [System.Collections.ArrayList]::new()

foreach ($file in $SourceOrder) {
    $fullPath = Join-Path $SrcDir $file
    if (Test-Path $fullPath) {
        $content = Get-Content $fullPath -Raw
        $lines = ($content -split "`r?`n").Count
        $bytes = (Get-Item $fullPath).Length
        $totalLines += $lines
        $totalBytes += $bytes
        $sizeKB = [math]::Round($bytes / 1024, 1)
        $null = $fileResults.Add(@{ File = $file; Lines = $lines; SizeKB = $sizeKB; Status = "OK" })
    }
    else {
        $null = $fileResults.Add(@{ File = $file; Lines = 0; SizeKB = 0; Status = "MISSING" })
        $missingCount++
    }
}

# Print per-file report
Write-Host "Source Files:" -ForegroundColor Yellow
Write-Host ("  # | File                   | Lines    | Size(KB) | Status")
Write-Host ("  --+------------------------+----------+----------+-------")

$idx = 0
foreach ($r in $fileResults) {
    $idx++
    $statusColor = if ($r.Status -eq "OK") { "Green" } else { "Red" }
    $lineNum = ("{0,3}" -f $idx)
    $fileName = ("{0,-22}" -f $r.File)
    $lineCount = ("{0,8}" -f $r.Lines)
    $sizeStr = ("{0,8:F1}" -f $r.SizeKB)
    Write-Host ("  $lineNum | $fileName | $lineCount | $sizeStr | ") -NoNewline
    Write-Host $r.Status -ForegroundColor $statusColor
}

Write-Host ""
$totalStr = ("TOTAL ({0} files):" -f $SourceOrder.Count)
$totalLinesStr = ("{0,8}" -f $totalLines)
$totalSizeStr = ("{0,8:F1}" -f ($totalBytes / 1024))
Write-Host ("  $totalStr".PadRight(53) + "$totalLinesStr" + "  " + "$totalSizeStr" + " KB") -ForegroundColor Cyan
Write-Host ""

# Check for missing files
if ($missingCount -gt 0) {
    Write-Host "FAIL: $missingCount file(s) missing!" -ForegroundColor Red
    foreach ($r in $fileResults) {
        if ($r.Status -eq "MISSING") {
            Write-Host "  MISSING: $($r.File)" -ForegroundColor Red
        }
    }
    exit 1
}

# Concatenate
Write-Host "Concatenating source files..." -ForegroundColor Yellow

if (-not (Test-Path $CombinedDir)) {
    New-Item -ItemType Directory -Path $CombinedDir -Force | Out-Null
}

$combined = ""
foreach ($file in $SourceOrder) {
    $fullPath = Join-Path $SrcDir $file
    if (-not (Test-Path $fullPath)) { continue }
    $content = Get-Content $fullPath -Raw
    $combined += "// === FILE: $file ===`r`n"
    $combined += $content
    if (-not $content.EndsWith("`n")) {
        $combined += "`r`n"
    }
    $combined += "`r`n"
}

$outPath = "$CombinedDir\kainc_bootstrap.kn"
Set-Content -Path $outPath -Value $combined -Encoding UTF8

$combinedSize = (Get-Item $outPath).Length
$combinedKB = [math]::Round($combinedSize / 1024, 1)
$combinedLines = ($combined -split "`r`n").Count

Write-Host "Combined source: $outPath" -ForegroundColor Green
Write-Host "  Size:  ${combinedKB}KB ($combinedSize bytes)" -ForegroundColor Green
Write-Host "  Lines: $combinedLines" -ForegroundColor Green
Write-Host ""

# Verify combined is non-empty
if ($combinedSize -eq 0) {
    Write-Host "FAIL: Combined source is EMPTY!" -ForegroundColor Red
    exit 1
}

if ($combinedSize -lt 100000) {
    Write-Host "WARNING: Combined source is suspiciously small (${combinedKB}KB)" -ForegroundColor Yellow
}

# Results
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  RESULT: PASS" -ForegroundColor Green
Write-Host "  All $($SourceOrder.Count) source files present." -ForegroundColor Green
Write-Host "  Combined: ${combinedKB}KB, $combinedLines lines" -ForegroundColor Green
Write-Host "==============================================" -ForegroundColor Cyan
exit 0
