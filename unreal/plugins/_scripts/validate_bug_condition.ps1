# Bug Condition Validation Script
# This script validates that the bug condition exists by checking existing BUILD_LOG.md files
# 
# Expected: 22 out of 25 plugins fail (88% failure rate)
# - 13 plugins with parse errors
# - 9 plugins with UE5 build errors

$ErrorActionPreference = "Continue"

Write-Host "`n=== Bug Condition Exploration ===" -ForegroundColor Cyan
Write-Host "Validating that FULLBUILD.bat fails for 22 plugins where spec shows tasks complete`n" -ForegroundColor Cyan

$factoryPath = Split-Path -Parent $PSScriptRoot

# Define all 22 failing plugins with their expected failure categories
$failingPlugins = @(
    # Parse Errors (13 plugins)
    @{ Name = "VoxelForgePro"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "AeroTunnel"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "AlphagenKain"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "AutoInstancer"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "CineMasterPro"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "FluidFlow"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "Materialize"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "OmniCam"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "PSOEliminator"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "TitanGraph"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "ToonShaderz"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "UESculpt"; Category = "ParseError"; Expected = "Parse error" },
    @{ Name = "VRAMSniper"; Category = "ParseError"; Expected = "Parse error" },
    
    # Name Collisions (5 plugins)
    @{ Name = "BulkMatte"; Category = "NameCollision"; Expected = "EParameterType|FMaterialInstanceInfo" },
    @{ Name = "Example"; Category = "NameCollision"; Expected = "EQuestStatus|UPhysicsComponent" },
    @{ Name = "NarrativeGraph"; Category = "NameCollision"; Expected = "EDialogueNodeType" },
    
    # Missing .generated.h (3 plugins)
    @{ Name = "Cosmos"; Category = "MissingGeneratedHeader"; Expected = "FVec2.generated.h" },
    @{ Name = "MetaHumanVAT"; Category = "MissingGeneratedHeader"; Expected = "FVec2.generated.h" },
    @{ Name = "TickOptimizer"; Category = "MissingGeneratedHeader"; Expected = "FVec2.generated.h" },
    
    # Function Conflicts (2 plugins)
    @{ Name = "Cinema4DMograph"; Category = "FunctionConflict"; Expected = "Remap" },
    @{ Name = "TemporalBlueprint"; Category = "FunctionConflict"; Expected = "ease_in_out" },
    
    # C++ Syntax Errors (1 plugin)
    @{ Name = "UltimateVFX"; Category = "CppSyntaxError"; Expected = "missing type specifier|syntax error" },
    
    # Unknown Errors (1 plugin)
    @{ Name = "MetaFitter"; Category = "Unknown"; Expected = "UnrealBuildTool failed" }
)

$counterexamples = @{
    ParseError = @()
    NameCollision = @()
    MissingGeneratedHeader = @()
    FunctionConflict = @()
    CppSyntaxError = @()
    Unknown = @()
}

$totalChecked = 0
$totalFailed = 0
$totalMissingLogs = 0

foreach ($plugin in $failingPlugins) {
    $pluginPath = Join-Path $factoryPath $plugin.Name
    $buildLogPath = Join-Path $pluginPath "BUILD_LOG.md"
    
    Write-Host "Checking: $($plugin.Name) (Expected: $($plugin.Category))" -ForegroundColor Yellow
    
    if (Test-Path $buildLogPath) {
        $buildLog = Get-Content $buildLogPath -Raw
        
        # Check if the expected error pattern exists
        $patterns = $plugin.Expected -split '\|'
        $foundError = $false
        
        foreach ($pattern in $patterns) {
            if ($buildLog -match [regex]::Escape($pattern)) {
                $foundError = $true
                break
            }
        }
        
        if ($foundError) {
            Write-Host "  ✓ EXPECTED FAILURE - Found error pattern in BUILD_LOG.md" -ForegroundColor Green
            $counterexamples[$plugin.Category] += $plugin.Name
            $totalFailed++
        } else {
            Write-Host "  ⚠ BUILD_LOG.md exists but expected error pattern not found" -ForegroundColor Magenta
            Write-Host "    Expected pattern: $($plugin.Expected)" -ForegroundColor Gray
        }
        
        $totalChecked++
    } else {
        Write-Host "  ⚠ BUILD_LOG.md not found - need to run FULLBUILD.bat" -ForegroundColor Magenta
        $totalMissingLogs++
    }
}

Write-Host "`n=== Counterexamples Summary ===" -ForegroundColor Cyan
Write-Host "Total plugins checked: $totalChecked" -ForegroundColor White
Write-Host "Total failures documented: $totalFailed" -ForegroundColor White
Write-Host "Missing build logs: $totalMissingLogs" -ForegroundColor Yellow

Write-Host "`nParse Errors ($($counterexamples.ParseError.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.ParseError) {
    Write-Host "  - $name" -ForegroundColor Gray
}

Write-Host "`nName Collisions ($($counterexamples.NameCollision.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.NameCollision) {
    Write-Host "  - $name" -ForegroundColor Gray
}

Write-Host "`nMissing .generated.h ($($counterexamples.MissingGeneratedHeader.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.MissingGeneratedHeader) {
    Write-Host "  - $name" -ForegroundColor Gray
}

Write-Host "`nFunction Conflicts ($($counterexamples.FunctionConflict.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.FunctionConflict) {
    Write-Host "  - $name" -ForegroundColor Gray
}

Write-Host "`nC++ Syntax Errors ($($counterexamples.CppSyntaxError.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.CppSyntaxError) {
    Write-Host "  - $name" -ForegroundColor Gray
}

Write-Host "`nUnknown Errors ($($counterexamples.Unknown.Count) plugins):" -ForegroundColor Yellow
foreach ($name in $counterexamples.Unknown) {
    Write-Host "  - $name" -ForegroundColor Gray
}

$failureRate = [math]::Round(($totalFailed / $failingPlugins.Count) * 100, 1)
Write-Host "`n=== Bug Condition Status ===" -ForegroundColor Cyan
Write-Host "Failure rate: $totalFailed/$($failingPlugins.Count) plugins ($failureRate%)" -ForegroundColor White

if ($totalFailed -ge 20) {
    Write-Host "`n✓ BUG CONDITION CONFIRMED" -ForegroundColor Red
    Write-Host "The bug exists: $totalFailed plugins fail despite spec showing tasks complete" -ForegroundColor Red
    Write-Host "This is EXPECTED on unfixed code." -ForegroundColor Yellow
    exit 1  # Exit with error code to indicate bug exists
} else {
    Write-Host "`n✓ BUG APPEARS TO BE FIXED" -ForegroundColor Green
    Write-Host "Only $totalFailed plugins fail - bug may be partially or fully fixed" -ForegroundColor Green
    exit 0  # Exit with success code
}
