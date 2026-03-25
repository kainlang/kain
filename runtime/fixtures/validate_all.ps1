# Validate all native runtime smoke fixtures
# Spec: .kiro/specs/kain-native-runtime-completion
# Task: 0.3 Create native runtime smoke fixtures

$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Native Runtime Smoke Fixtures Validation" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FixturesDir = $ScriptDir

# Track results
$Passed = 0
$Failed = 0
$Skipped = 0

# Function to validate a fixture
function Validate-Fixture {
    param(
        [string]$FixtureName
    )
    
    $FixtureDir = Join-Path $FixturesDir $FixtureName
    
    Write-Host "----------------------------------------" -ForegroundColor Gray
    Write-Host "Validating: $FixtureName" -ForegroundColor White
    Write-Host "----------------------------------------" -ForegroundColor Gray
    
    if (-not (Test-Path $FixtureDir)) {
        Write-Host "FAILED: Directory not found: $FixtureDir" -ForegroundColor Red
        $script:Failed++
        return $false
    }
    
    Push-Location $FixtureDir
    
    try {
        # Check required files exist
        if (-not (Test-Path "main.kn")) {
            Write-Host "FAILED: main.kn not found" -ForegroundColor Red
            $script:Failed++
            return $false
        }
        
        if (-not (Test-Path "README.md")) {
            Write-Host "FAILED: README.md not found" -ForegroundColor Red
            $script:Failed++
            return $false
        }
        
        # Try to compile (this may fail if kain CLI is not available)
        $KainExists = Get-Command kain -ErrorAction SilentlyContinue
        if ($KainExists) {
            Write-Host "Compiling $FixtureName..." -ForegroundColor Gray
            $LogFile = Join-Path $env:TEMP "kain_build_$FixtureName.log"
            
            try {
                kain build main.kn --target rust 2>&1 | Tee-Object -FilePath $LogFile
                Write-Host "PASSED: $FixtureName compiled successfully" -ForegroundColor Green
                $script:Passed++
                return $true
            }
            catch {
                Write-Host "FAILED: $FixtureName compilation failed" -ForegroundColor Red
                Write-Host "See $LogFile for details" -ForegroundColor Yellow
                $script:Failed++
                return $false
            }
        }
        else {
            Write-Host "SKIPPED: kain CLI not available, cannot compile" -ForegroundColor Yellow
            $script:Skipped++
            return $true
        }
    }
    finally {
        Pop-Location
        Write-Host ""
    }
}

# Validate each fixture
Validate-Fixture "contract_startup"
Validate-Fixture "realtime_startup"
Validate-Fixture "ui_startup"
Validate-Fixture "viewport_startup"

# Summary
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Validation Summary" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "PASSED:  $Passed" -ForegroundColor Green
Write-Host "FAILED:  $Failed" -ForegroundColor Red
Write-Host "SKIPPED: $Skipped" -ForegroundColor Yellow
Write-Host ""

if ($Failed -gt 0) {
    Write-Host "Validation FAILED" -ForegroundColor Red
    exit 1
}
else {
    Write-Host "Validation PASSED" -ForegroundColor Green
    exit 0
}
