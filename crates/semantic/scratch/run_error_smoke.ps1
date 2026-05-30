# Kain Error System Smoke Test
# Runs every .kn file through `kain check` and captures output into .md report.
param(
    [string]$KainBin = "X:\.kain\bin\kain.exe",
    [string]$ScratchDir = "X:\crates\semantic\scratch",
    [string]$OutFile = $null,
    [string]$Target = "llvm"
)

$ErrorActionPreference = "Continue"
$scratch = Resolve-Path $ScratchDir
$ts = Get-Date -Format "yyyy-MM-dd_HHmmss"
if (-not $OutFile) { $OutFile = Join-Path $scratch "error_smoke_report_${ts}.md" }

$kain = Resolve-Path $KainBin
$files = Get-ChildItem -Path $scratch -Filter "*.kn" | Sort-Object Name
$total = $files.Count
$passed = 0
$failed = 0
$results = @()

Write-Host "=== Kain Error System Smoke Test ===" -ForegroundColor Cyan
Write-Host "Kain  : $kain"
Write-Host "Dir   : $scratch"
Write-Host "Files : $total"
Write-Host "Out   : $OutFile"
Write-Host ""

$i = 0
foreach ($f in $files) {
    $i++
    $name = $f.Name
    $firstLine = (Get-Content $f.FullName -TotalCount 1)
    Write-Host "[$i/$total] $name" -NoNewline

    $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-String
    $ec = $LASTEXITCODE

    $obj = [PSCustomObject]@{
        Name     = $name
        Header   = $firstLine
        ExitCode = $ec
        Passed   = ($ec -eq 0)
        Output   = $captured.Trim()
    }
    $results += $obj

    if ($ec -eq 0) {
        Write-Host "  PASS" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "  FAIL ($ec)" -ForegroundColor Red
        $failed++
    }
}

# Build report using StringBuilder
$sb = New-Object System.Text.StringBuilder

[void]$sb.AppendLine("# Kain Error System Smoke Test Report")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("**Generated:** $ts  ")
[void]$sb.Append("**Kain binary:** "); [void]$sb.AppendLine($kain)
[void]$sb.Append("**Target:** "); [void]$sb.AppendLine($Target)
[void]$sb.Append("**Files tested:** "); [void]$sb.Append($total); [void]$sb.Append(" ("); [void]$sb.Append($passed); [void]$sb.Append(" passed, "); [void]$sb.Append($failed); [void]$sb.AppendLine(" failed)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Summary")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Status | Count |")
[void]$sb.AppendLine("|--------|-------|")
[void]$sb.Append("| Passed | "); [void]$sb.Append($passed); [void]$sb.AppendLine(" |")
[void]$sb.Append("| Failed | "); [void]$sb.Append($failed); [void]$sb.AppendLine(" |")
[void]$sb.Append("| Total  | "); [void]$sb.Append($total); [void]$sb.AppendLine(" |")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Detailed Results")
[void]$sb.AppendLine("")

foreach ($r in $results) {
    $status = if ($r.Passed) { "PASS" } else { "FAIL (exit $($r.ExitCode))" }
    [void]$sb.Append("### "); [void]$sb.Append($r.Name); [void]$sb.Append(" -- "); [void]$sb.AppendLine($status)
    [void]$sb.AppendLine("")
    [void]$sb.Append("**Header:** "); [void]$sb.AppendLine($r.Header)
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine('```')
    [void]$sb.AppendLine($r.Output)
    [void]$sb.AppendLine('```')
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("---")
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("## Failed Files")
[void]$sb.AppendLine("")
$bad = $results | Where-Object { -not $_.Passed }
if ($bad) {
    foreach ($r in $bad) {
        $line = "- **" + $r.Name + "** -- " + $r.Header + " (exit " + $r.ExitCode + ")"
        [void]$sb.AppendLine($line)
    }
} else {
    [void]$sb.AppendLine("All files passed. No errors to report.")
}

[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Notes")
[void]$sb.AppendLine("")
[void]$sb.Append("- Exit 0 = check passed (no errors)")
[void]$sb.AppendLine("")
[void]$sb.Append("- Exit 1 = check failed (errors found)")
[void]$sb.AppendLine("")
[void]$sb.Append("- Exit 2 = usage error")
[void]$sb.AppendLine("")
[void]$sb.Append("- Other = compiler crash or internal error")
[void]$sb.AppendLine("")

$report = $sb.ToString()
$report | Out-File -FilePath $OutFile -Encoding UTF8

Write-Host ""
Write-Host "=== Report: $OutFile ===" -ForegroundColor Cyan
Write-Host "Passed: $passed / Failed: $failed"
Write-Host ""
Write-Host $report
