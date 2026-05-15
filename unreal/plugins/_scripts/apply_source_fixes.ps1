#!/usr/bin/env pwsh
<#
.SYNOPSIS
    KAIN Source File Pattern Fixer - PowerShell version
.DESCRIPTION
    Applies systematic fixes to .kn source files to resolve parse errors.
.PARAMETER PluginDirectory
    Path to plugin directory containing .kn files
.PARAMETER DryRun
    Preview changes without modifying files
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$PluginDirectory,
    
    [Parameter(Mandatory=$false)]
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

# Statistics
$script:FixesApplied = @{}
$script:NewPatternsFound = @()

function Write-Log {
    param([string]$Message)
    Write-Host "[SourceFixer] $Message"
}

function Apply-Pattern1-VarToLet {
    param([string]$Content)
    $count = ([regex]::Matches($Content, 'var ')).Count
    $modified = $Content -replace 'var ', 'let '
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern2-NotToEqualsFalse {
    param([string]$Content)
    $count = ([regex]::Matches($Content, ' not ')).Count
    $modified = $Content -replace ' not ', ' == false '
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern3-AndOperator {
    param([string]$Content)
    $count = ([regex]::Matches($Content, ' && ')).Count
    $modified = $Content -replace ' && ', ' and '
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern4-OrOperator {
    param([string]$Content)
    $count = ([regex]::Matches($Content, ' \|\| ')).Count
    $modified = $Content -replace ' \|\| ', ' or '
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern5-LetMut {
    param([string]$Content)
    $count = ([regex]::Matches($Content, 'let mut ')).Count
    $modified = $Content -replace 'let mut ', 'let '
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern7-StructFieldAccess {
    param([string]$Content)
    # Replace lowercase_var::field with lowercase_var.field
    $pattern = '\b([a-z_][a-z0-9_]*)::([a-z_][a-z0-9_]*)'
    $count = ([regex]::Matches($Content, $pattern)).Count
    $modified = $Content -replace $pattern, '$1.$2'
    return @{ Content = $modified; Count = $count }
}

function Apply-Pattern8-StructLiterals {
    param([string]$Content)
    # Replace Vec3i { x, y, z } with vec3i(x, y, z)
    $pattern = '(Vec\d+[if]?)\s*\{\s*([a-z_][a-z0-9_]*)\s*,\s*([a-z_][a-z0-9_]*)\s*,\s*([a-z_][a-z0-9_]*)\s*\}'
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    $count = $matches.Count
    
    foreach ($match in $matches) {
        $typeName = $match.Groups[1].Value.ToLower()
        $x = $match.Groups[2].Value
        $y = $match.Groups[3].Value
        $z = $match.Groups[4].Value
        $replacement = "$typeName($x, $y, $z)"
        $Content = $Content.Replace($match.Value, $replacement)
    }
    
    return @{ Content = $Content; Count = $count }
}

function Apply-AllPatterns {
    param(
        [string]$Content,
        [string]$FilePath
    )
    
    $originalContent = $Content
    $patterns = @(
        @{ Name = 'var_to_let'; Func = ${function:Apply-Pattern1-VarToLet} },
        @{ Name = 'not_to_equals_false'; Func = ${function:Apply-Pattern2-NotToEqualsFalse} },
        @{ Name = 'and_operator'; Func = ${function:Apply-Pattern3-AndOperator} },
        @{ Name = 'or_operator'; Func = ${function:Apply-Pattern4-OrOperator} },
        @{ Name = 'let_mut'; Func = ${function:Apply-Pattern5-LetMut} },
        @{ Name = 'struct_field_access'; Func = ${function:Apply-Pattern7-StructFieldAccess} },
        @{ Name = 'struct_literals'; Func = ${function:Apply-Pattern8-StructLiterals} }
    )
    
    foreach ($pattern in $patterns) {
        $result = & $pattern.Func -Content $Content
        $Content = $result.Content
        if ($result.Count -gt 0) {
            Write-Log "  $($pattern.Name): $($result.Count) replacements"
            if (-not $script:FixesApplied.ContainsKey($pattern.Name)) {
                $script:FixesApplied[$pattern.Name] = 0
            }
            $script:FixesApplied[$pattern.Name] += $result.Count
        }
    }
    
    return $Content
}

function Process-File {
    param([string]$FilePath)
    
    Write-Log "Processing: $FilePath"
    
    try {
        # Read original content
        $originalContent = Get-Content -Path $FilePath -Raw -Encoding UTF8
        
        # Apply fixes
        $modifiedContent = Apply-AllPatterns -Content $originalContent -FilePath $FilePath
        
        # Check if anything changed
        if ($originalContent -eq $modifiedContent) {
            Write-Log "  No changes needed"
            return $true
        }
        
        # Create backup
        if (-not $DryRun) {
            $backupPath = "$FilePath.bak"
            Copy-Item -Path $FilePath -Destination $backupPath -Force
            Write-Log "  Created backup: $backupPath"
        }
        
        # Write modified content
        if (-not $DryRun) {
            Set-Content -Path $FilePath -Value $modifiedContent -Encoding UTF8 -NoNewline
            Write-Log "  ✓ File updated"
        } else {
            Write-Log "  [DRY RUN] Would update file"
        }
        
        return $true
    }
    catch {
        Write-Log "  ERROR: $_"
        return $false
    }
}

function Process-Directory {
    param([string]$Directory)
    
    $knFiles = Get-ChildItem -Path $Directory -Filter "*.kn" -Recurse -File
    
    if ($knFiles.Count -eq 0) {
        Write-Log "No .kn files found in $Directory"
        return @{ Success = 0; Failed = 0 }
    }
    
    Write-Log "Found $($knFiles.Count) .kn files"
    
    $successCount = 0
    $failCount = 0
    
    foreach ($file in $knFiles) {
        if (Process-File -FilePath $file.FullName) {
            $successCount++
        } else {
            $failCount++
        }
    }
    
    return @{ Success = $successCount; Failed = $failCount }
}

function Print-Summary {
    Write-Host ""
    Write-Host ("=" * 60)
    Write-Host "SUMMARY"
    Write-Host ("=" * 60)
    
    if ($script:FixesApplied.Count -gt 0) {
        Write-Host "`nFixes Applied:"
        foreach ($key in $script:FixesApplied.Keys | Sort-Object) {
            Write-Host "  ${key}: $($script:FixesApplied[$key]) replacements"
        }
    } else {
        Write-Host "`nNo fixes applied"
    }
    
    if ($script:NewPatternsFound.Count -gt 0) {
        Write-Host "`nNew Patterns Found (Manual Review Needed):"
        foreach ($pattern in $script:NewPatternsFound) {
            Write-Host "  - $pattern"
        }
    }
    
    Write-Host ("=" * 60)
}

# Main execution
$pluginDir = Resolve-Path $PluginDirectory -ErrorAction Stop

Write-Host "`nProcessing plugin: $(Split-Path $pluginDir -Leaf)"
Write-Host "Directory: $pluginDir"
if ($DryRun) {
    Write-Host "Mode: DRY RUN (no files will be modified)"
}
Write-Host ""

$results = Process-Directory -Directory $pluginDir

Print-Summary

Write-Host "`nResults:"
Write-Host "  Success: $($results.Success) files"
Write-Host "  Failed: $($results.Failed) files"

if ($results.Failed -gt 0) {
    exit 1
} else {
    exit 0
}
