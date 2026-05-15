# Rebuild COMBINEDLOG_KAIN.md - KAIN compilation only (no UE5 builds)
# This script runs 'kain build --ue5' on all plugins and logs BOTH successes and failures
# Purpose: Fast validation of KAIN parser fixes without expensive UE5 compilation

$ErrorActionPreference = "Continue"
$factoryRoot = Split-Path -Parent $PSScriptRoot
$combinedLog = Join-Path $factoryRoot "COMBINEDLOG_KAIN.md"
$totalPlugins = 0
$successCount = 0
$failedCount = 0

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Rebuilding COMBINEDLOG_KAIN.md - KAIN compilation only (no UE5)..." -ForegroundColor Cyan
Write-Host "============================================================================`n" -ForegroundColor Cyan

# Create new COMBINEDLOG_KAIN.md with header
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$header = @"
# KAIN Compilation Status Log (No UE5 Builds)

**Generated**: $timestamp
**Purpose**: Fast validation of KAIN parser fixes without UE5 compilation

## Summary

"@

Set-Content -Path $combinedLog -Value $header -Encoding UTF8

# Temporary arrays for results
$summaryLines = @()
$detailsLines = @()

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "[STEP 1] Verifying KAIN compiler..." -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan

$kainExe = Get-Command kain -ErrorAction SilentlyContinue
if (-not $kainExe) {
    Write-Host "[ERROR] kain.exe not found in PATH" -ForegroundColor Red
    Write-Host "[INFO] Run M:\Code\Kain\scripts\sync-kain-source-of-truth.ps1" -ForegroundColor Yellow
    exit 1
}

$kainExePath = $kainExe.Source
Write-Host "[SUCCESS] KAIN compiler found and ready: $kainExePath`n" -ForegroundColor Green

# Scan all plugin directories
$pluginDirs = Get-ChildItem -Path $factoryRoot -Directory | Where-Object {
    $_.Name -notmatch '^_' -and (Test-Path (Join-Path $_.FullName "KAIN.toml"))
}

foreach ($pluginDir in $pluginDirs) {
    $totalPlugins++
    $pluginName = $pluginDir.Name
    
    Write-Host "`n============================================================================" -ForegroundColor Cyan
    Write-Host "[$totalPlugins] KAIN build: $pluginName" -ForegroundColor Cyan
    Write-Host "============================================================================" -ForegroundColor Cyan
    
    # Run kain build --ue5 and capture output
    $tempLog = Join-Path $env:TEMP "kain_${pluginName}_$(Get-Random).log"
    
    Push-Location $pluginDir.FullName
    $output = & $kainExePath build --ue5 2>&1 | Out-String
    $buildResult = $LASTEXITCODE
    Pop-Location
    
    Set-Content -Path $tempLog -Value $output -Encoding UTF8
    
    # Append to details
    $detailsLines += "`n============================================================================`n"
    
    if ($buildResult -eq 0) {
        Write-Host "[SUCCESS] $pluginName KAIN compilation passed" -ForegroundColor Green
        
        $summaryLines += "- $pluginName`: ✅ KAIN PASS"
        
        $detailsLines += "### $pluginName - ✅ KAIN PASS`n"
        $detailsLines += "**Status**: KAIN compilation successful"
        $detailsLines += "**KAIN Parser**: ✅ PASS"
        $detailsLines += "**C++ Generation**: ✅ PASS`n"
        
        $successCount++
    }
    else {
        Write-Host "[FAILED] $pluginName KAIN compilation failed with error code $buildResult" -ForegroundColor Red
        
        # Determine failure stage
        if ($output -match "Parse error|parse error|ParseError") {
            $stage = "KAIN Parse Error"
            $summaryLines += "- $pluginName`: ❌ KAIN PARSE ERROR"
        }
        elseif ($output -match "Type error|type error|TypeError") {
            $stage = "KAIN Type Error"
            $summaryLines += "- $pluginName`: ❌ KAIN TYPE ERROR"
        }
        elseif ($output -match "Validation error|validation error|Oracle") {
            $stage = "KAIN Validation Error"
            $summaryLines += "- $pluginName`: ❌ KAIN VALIDATION ERROR"
        }
        else {
            $stage = "KAIN Unknown Error"
            $summaryLines += "- $pluginName`: ❌ KAIN UNKNOWN ERROR"
        }
        
        $detailsLines += "### $pluginName - ❌ KAIN FAILED`n"
        $detailsLines += "**Status**: KAIN compilation failed"
        $detailsLines += "**Failure Stage**: $stage`n"
        $detailsLines += "#### Error Details`n"
        $detailsLines += "``````"
        
        # Extract errors
        $errors = $output -split "`n" | Where-Object { $_ -match "error|Error:|ERROR|failed|FAILED|Parse|parse" }
        $errorCount = $errors.Count
        
        if ($errorCount -gt 0) {
            $displayErrors = $errors | Select-Object -First 50
            $detailsLines += ($displayErrors -join "`n")
            
            if ($errorCount -gt 50) {
                $remaining = $errorCount - 50
                $detailsLines += "`n`n[TRUNCATED: $remaining more errors not shown]"
            }
        }
        
        $detailsLines += "``````"
        $detailsLines += "`n**Total Errors**: $errorCount`n"
        
        $failedCount++
    }
    
    $detailsLines += "============================================================================`n"
    
    # Cleanup temp log
    Remove-Item $tempLog -Force -ErrorAction SilentlyContinue
}

# Append summary to combined log
Add-Content -Path $combinedLog -Value ($summaryLines -join "`n") -Encoding UTF8
Add-Content -Path $combinedLog -Value "`n**Total Plugins**: $totalPlugins" -Encoding UTF8
Add-Content -Path $combinedLog -Value "**KAIN Pass**: $successCount" -Encoding UTF8
Add-Content -Path $combinedLog -Value "**KAIN Fail**: $failedCount`n" -Encoding UTF8
Add-Content -Path $combinedLog -Value "---`n" -Encoding UTF8
Add-Content -Path $combinedLog -Value "## Detailed Results`n" -Encoding UTF8

# Append details
Add-Content -Path $combinedLog -Value ($detailsLines -join "`n") -Encoding UTF8

Write-Host "`n============================================================================" -ForegroundColor Cyan
Write-Host "KAIN compilation check complete!" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Total plugins: $totalPlugins" -ForegroundColor White
Write-Host "KAIN Pass: $successCount" -ForegroundColor Green
Write-Host "KAIN Fail: $failedCount" -ForegroundColor $(if ($failedCount -gt 0) { "Red" } else { "Green" })
Write-Host "`nCOMBINEDLOG_KAIN.md updated: $combinedLog`n" -ForegroundColor Cyan
Write-Host "NOTE: This only validates KAIN compilation. UE5 build errors are not checked." -ForegroundColor Yellow
Write-Host "      Run rebuild_combinedlog.ps1 for full UE5 build validation.`n" -ForegroundColor Yellow

# Exit with error code if any builds failed
if ($failedCount -gt 0) {
    exit 1
}

exit 0
