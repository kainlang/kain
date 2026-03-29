# Rebuild COMBINEDLOG.md with complete status for all plugins
# This script runs FULLBUILD.bat on all plugins and logs BOTH successes and failures

$ErrorActionPreference = "Continue"
$factoryRoot = Split-Path -Parent $PSScriptRoot
$combinedLog = Join-Path $factoryRoot "COMBINEDLOG.md"
$totalPlugins = 0
$successCount = 0
$failedCount = 0

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Rebuilding COMBINEDLOG.md with complete status for all plugins..." -ForegroundColor Cyan
Write-Host "============================================================================`n" -ForegroundColor Cyan

# Create new COMBINEDLOG.md with header
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$header = @"
# Combined Build Status Log

**Generated**: $timestamp
**Purpose**: Complete build status for all Factory plugins

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
    $_.Name -notmatch '^_' -and (Test-Path (Join-Path $_.FullName "FULLBUILD.bat"))
}

foreach ($pluginDir in $pluginDirs) {
    $totalPlugins++
    $pluginName = $pluginDir.Name
    
    Write-Host "`n============================================================================" -ForegroundColor Cyan
    Write-Host "[$totalPlugins] Building: $pluginName" -ForegroundColor Cyan
    Write-Host "============================================================================" -ForegroundColor Cyan
    
    # Run FULLBUILD.bat and capture output
    $tempLog = Join-Path $env:TEMP "build_${pluginName}_$(Get-Random).log"
    
    Push-Location $pluginDir.FullName
    $output = & cmd /c "FULLBUILD.bat" 2>&1 | Out-String
    $buildResult = $LASTEXITCODE
    Pop-Location
    
    Set-Content -Path $tempLog -Value $output -Encoding UTF8
    
    # Append to details
    $detailsLines += "`n============================================================================`n"
    
    if ($buildResult -eq 0) {
        Write-Host "[SUCCESS] $pluginName built successfully" -ForegroundColor Green
        
        $summaryLines += "- $pluginName`: ✅ SUCCESS (Complete)"
        
        $detailsLines += "### $pluginName - ✅ SUCCESS`n"
        $detailsLines += "**Status**: Build completed successfully"
        $detailsLines += "**KAIN Compilation**: ✅ PASS"
        $detailsLines += "**UE5 Build**: ✅ PASS`n"
        
        $successCount++
    }
    else {
        Write-Host "[FAILED] $pluginName build failed with error code $buildResult" -ForegroundColor Red
        
        # Determine failure stage
        if ($output -match "Parse error") {
            $stage = "KAIN Parse Error"
            $summaryLines += "- $pluginName`: ❌ FAILED (KAIN Parse)"
        }
        elseif ($output -match "UnrealHeaderTool") {
            $stage = "UE5 Build Error"
            $summaryLines += "- $pluginName`: ❌ FAILED (UE5 Build)"
        }
        else {
            $stage = "Unknown Error"
            $summaryLines += "- $pluginName`: ❌ FAILED (Unknown)"
        }
        
        $detailsLines += "### $pluginName - ❌ FAILED`n"
        $detailsLines += "**Status**: Build failed"
        $detailsLines += "**Failure Stage**: $stage`n"
        $detailsLines += "#### Error Details`n"
        $detailsLines += "``````"
        
        # Extract errors
        $errors = $output -split "`n" | Where-Object { $_ -match "error|Error:|ERROR|failed|FAILED" }
        $errorCount = $errors.Count
        
        if ($errorCount -gt 0) {
            $displayErrors = $errors | Select-Object -First 30
            $detailsLines += ($displayErrors -join "`n")
            
            if ($errorCount -gt 30) {
                $remaining = $errorCount - 30
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
Add-Content -Path $combinedLog -Value "**Successful**: $successCount" -Encoding UTF8
Add-Content -Path $combinedLog -Value "**Failed**: $failedCount`n" -Encoding UTF8
Add-Content -Path $combinedLog -Value "---`n" -Encoding UTF8
Add-Content -Path $combinedLog -Value "## Detailed Results`n" -Encoding UTF8

# Append details
Add-Content -Path $combinedLog -Value ($detailsLines -join "`n") -Encoding UTF8

Write-Host "`n============================================================================" -ForegroundColor Cyan
Write-Host "Rebuild complete!" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Total plugins: $totalPlugins" -ForegroundColor White
Write-Host "Successful: $successCount" -ForegroundColor Green
Write-Host "Failed: $failedCount" -ForegroundColor $(if ($failedCount -gt 0) { "Red" } else { "Green" })
Write-Host "`nCOMBINEDLOG.md updated: $combinedLog`n" -ForegroundColor Cyan

# Exit with error code if any builds failed
if ($failedCount -gt 0) {
    exit 1
}

exit 0
