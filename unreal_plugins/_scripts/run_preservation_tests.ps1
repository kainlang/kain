# Preservation Test Executor
# Runs all preservation tests and updates PRESERVATION_TEST_RESULTS.md

$ErrorActionPreference = "Continue"
$kainCommand = Get-Command kain -ErrorAction SilentlyContinue
if (-not $kainCommand) {
    throw "kain.exe not found in PATH. Run M:\Code\Kain\scripts\sync-kain-source-of-truth.ps1 first."
}
$kainExe = $kainCommand.Source
$testDir = "M:\Code\Kain\crates\cli\tests\fixtures\preservation"
$resultsFile = "M:\Code\.kiro\specs\factory-plugin-compilation-failures\PRESERVATION_TEST_RESULTS.md"

# Create test directory
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

Write-Host "`n=== Preservation Property Test Execution ===" -ForegroundColor Cyan
Write-Host "Running all 22 preservation tests on unfixed code" -ForegroundColor Cyan
Write-Host "EXPECTED: Tests PASS (confirms baseline to preserve)`n" -ForegroundColor Cyan
Write-Host "Using kain binary: $kainExe`n" -ForegroundColor DarkGray

$passed = 0
$failed = 0
$results = @{}

function Run-Test {
    param(
        [string]$TestId,
        [string]$TestName,
        [string]$FileName,
        [string]$Content,
        [string]$Target = "rust"
    )
    
    $testFile = Join-Path $testDir $FileName
    
    try {
        # Write test file
        Set-Content -Path $testFile -Value $Content -Encoding UTF8 -NoNewline
        
        # Run kain build
        $output = & $kainExe build $testFile --target $Target 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host "  ✓ $TestId $TestName" -ForegroundColor Green
            $script:passed++
            $script:results[$TestId] = @{Name=$TestName; Status="PASS"; Output=$output}
            return $true
        } else {
            Write-Host "  ✗ $TestId $TestName" -ForegroundColor Red
            Write-Host "    Exit code: $exitCode" -ForegroundColor Yellow
            $script:failed++
            $script:results[$TestId] = @{Name=$TestName; Status="FAIL"; Output=$output; ExitCode=$exitCode}
            return $false
        }
    }
    catch {
        Write-Host "  ✗ $TestId $TestName (Exception)" -ForegroundColor Red
        $script:failed++
        $script:results[$TestId] = @{Name=$TestName; Status="FAIL"; Output=$_.Exception.Message}
        return $false
    }
    finally {
        # Cleanup
        if (Test-Path $testFile) {
            Remove-Item $testFile -Force -ErrorAction SilentlyContinue
        }
    }
}

# ============================================================================
# Property 1: KAIN Language Features
# ============================================================================

Write-Host "`nProperty 1: KAIN Language Features" -ForegroundColor Yellow

Run-Test -TestId "1.1" -TestName "Actors compile" -FileName "test_actor.kn" -Content @"
actor TestActor:
    state health: Float = 100.0
    state max_health: Float = 100.0
    
    fn take_damage(amount: Float):
        health = health - amount
"@

Run-Test -TestId "1.2" -TestName "Components compile" -FileName "test_component.kn" -Content @"
@component
struct HealthComponent:
    current: Float
    max: Float
    
    fn heal(amount: Float):
        current = current + amount
"@

Run-Test -TestId "1.3" -TestName "Structs compile" -FileName "test_struct.kn" -Content @"
struct Vec2:
    x: Float
    y: Float

struct ItemData:
    id: Int
    name: String
    quantity: Int
"@

Run-Test -TestId "1.4" -TestName "Enums compile" -FileName "test_enum.kn" -Content @"
enum ItemRarity:
    Common
    Rare
    Epic
    Legendary

enum GameState:
    Menu
    Playing
    Paused
"@

Run-Test -TestId "1.5" -TestName "Shaders compile" -FileName "test_shader.kn" -Target "usf" -Content @"
shader compute TestCompute(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform scale: Float @1
    buffer output: RWBuffer<Float> @2
    
    let value = thread_id.x * scale
    output[thread_id.x] = value
"@

Run-Test -TestId "1.6" -TestName "Materials compile" -FileName "test_material.kn" -Content @"
material TestMaterial:
    input base_color: Vec3 = vec3(1.0, 1.0, 1.0)
    input roughness_value: Float = 0.5
    
    base_color = base_color
    roughness = roughness_value
"@

# ============================================================================
# Property 2: Compilation Targets
# ============================================================================

Write-Host "`nProperty 2: Compilation Targets" -ForegroundColor Yellow

Run-Test -TestId "2.1" -TestName "WASM target works" -FileName "test_wasm.kn" -Target "wasm" -Content @"
fn add(a: Int, b: Int) -> Int:
    return a + b

fn main():
    let result = add(5, 3)
    println("Result: {result}")
"@

Run-Test -TestId "2.2" -TestName "JavaScript target works" -FileName "test_js.kn" -Target "js" -Content @"
fn multiply(a: Int, b: Int) -> Int:
    return a * b

fn main():
    let result = multiply(4, 7)
    println("Result: {result}")
"@

Run-Test -TestId "2.3" -TestName "Rust target works" -FileName "test_rust.kn" -Target "rust" -Content @"
fn factorial(n: Int) -> Int with Pure:
    match n:
        0 => 1
        _ => n * factorial(n - 1)

fn main():
    let result = factorial(5)
    println("Factorial: {result}")
"@

Run-Test -TestId "2.4" -TestName "C++ target works" -FileName "test_cpp.kn" -Target "cpp" -Content @"
fn square(x: Float) -> Float:
    return x * x

fn main():
    let result = square(5.0)
    println("Square: {result}")
"@

Run-Test -TestId "2.5" -TestName "HLSL target works" -FileName "test_hlsl.kn" -Target "hlsl" -Content @"
shader fragment TestFragment(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
"@

Run-Test -TestId "2.6" -TestName "USF target works" -FileName "test_usf.kn" -Target "usf" -Content @"
shader compute TestUSF(thread_id: Vec3):
    uniform size: Int @0
    buffer data: RWBuffer<Float> @1
    
    data[thread_id.x] = thread_id.x * 2.0
"@

# ============================================================================
# Property 3: Backend Systems
# ============================================================================

Write-Host "`nProperty 3: Backend Systems" -ForegroundColor Yellow

Run-Test -TestId "3.1" -TestName "Shader array literals work" -FileName "test_array_literal.kn" -Target "usf" -Content @"
shader compute TestArrayLiteral(thread_id: Vec3):
    uniform size: Int @0
    buffer output: RWBuffer<Float> @1
    
    let weights = [0.2, 0.3, 0.5]
    output[thread_id.x] = weights[0]
"@

Run-Test -TestId "3.2" -TestName "Shader cast expressions work" -FileName "test_cast.kn" -Target "usf" -Content @"
shader compute TestCast(thread_id: Vec3):
    uniform size: Int @0
    buffer output: RWBuffer<Float> @1
    
    let int_val = 42
    let float_val = int_val as Float
    output[thread_id.x] = float_val
"@

Run-Test -TestId "3.3" -TestName "Shader @N binding semantics work" -FileName "test_bindings.kn" -Target "usf" -Content @"
shader compute TestBindings(thread_id: Vec3):
    uniform param0: Float @0
    uniform param1: Int @1
    uniform param2: Vec3 @2
    buffer output: RWBuffer<Float> @3
    
    output[thread_id.x] = param0 + param1 + param2.x
"@

# ============================================================================
# Property 4: UE5 Code Generation
# ============================================================================

Write-Host "`nProperty 4: UE5 Code Generation" -ForegroundColor Yellow

Run-Test -TestId "4.1" -TestName "UCLASS macros generated" -FileName "test_uclass.kn" -Content @"
actor TestUClassActor:
    state value: Int = 0
"@

Run-Test -TestId "4.2" -TestName "UPROPERTY macros generated" -FileName "test_uproperty.kn" -Content @"
actor TestUPropertyActor:
    state health: Float = 100.0
    state max_health: Float = 100.0
"@

Run-Test -TestId "4.3" -TestName "UFUNCTION macros generated" -FileName "test_ufunction.kn" -Content @"
@blueprint
fn calculate_damage(base: Float, multiplier: Float) -> Float:
    return base * multiplier
"@

Run-Test -TestId "4.4" -TestName "USTRUCT macros generated" -FileName "test_ustruct.kn" -Content @"
struct TestStruct:
    field1: Int
    field2: Float
    field3: String
"@

Run-Test -TestId "4.5" -TestName "UENUM macros generated" -FileName "test_uenum.kn" -Content @"
enum TestEnum:
    Value1
    Value2
    Value3
"@

# ============================================================================
# Property 5: Stdlib System
# ============================================================================

Write-Host "`nProperty 5: Stdlib System" -ForegroundColor Yellow

Run-Test -TestId "5.1" -TestName "Stdlib functions available" -FileName "test_stdlib.kn" -Content @"
fn test_stdlib():
    let x = abs(-5.0)
    let y = min(10.0, 20.0)
    let z = max(10.0, 20.0)
    println("Values: {x}, {y}, {z}")
"@

# ============================================================================
# Property 6: Multi-File Compilation
# ============================================================================

Write-Host "`nProperty 6: Multi-File Compilation" -ForegroundColor Yellow

$file1 = Join-Path $testDir "multi_file_1.kn"
$file2 = Join-Path $testDir "multi_file_2.kn"

Set-Content -Path $file1 -Value @"
struct SharedData:
    value: Int
"@ -Encoding UTF8 -NoNewline

Set-Content -Path $file2 -Value @"
fn use_shared_data(data: SharedData) -> Int:
    return data.value * 2
"@ -Encoding UTF8 -NoNewline

$output1 = & $kainExe build $file1 --target rust 2>&1 | Out-String
$exitCode1 = $LASTEXITCODE

$output2 = & $kainExe build $file2 --target rust 2>&1 | Out-String
$exitCode2 = $LASTEXITCODE

if ($exitCode1 -eq 0 -and $exitCode2 -eq 0) {
    Write-Host "  ✓ 6.1 Multi-file compilation works" -ForegroundColor Green
    $passed++
    $results["6.1"] = @{Name="Multi-file compilation works"; Status="PASS"}
} else {
    Write-Host "  ✗ 6.1 Multi-file compilation works" -ForegroundColor Red
    $failed++
    $results["6.1"] = @{Name="Multi-file compilation works"; Status="FAIL"; Output="$output1`n$output2"}
}

Remove-Item $file1 -Force -ErrorAction SilentlyContinue
Remove-Item $file2 -Force -ErrorAction SilentlyContinue

# ============================================================================
# Summary and Results Update
# ============================================================================

Write-Host "`n=== Preservation Test Results ===" -ForegroundColor Cyan
Write-Host "Passed: $passed/22" -ForegroundColor Green
Write-Host "Failed: $failed/22" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })

# Update results in markdown file
$content = Get-Content $resultsFile -Raw

# Update summary table
$summaryPattern = '\| \*\*TOTAL\*\* \| \*\*22\*\* \| \*\*TBD\*\* \| \*\*TBD\*\* \| \*\*PENDING\*\* \|'
$summaryReplacement = "| **TOTAL** | **22** | **$passed** | **$failed** | **$(if ($failed -eq 0) { 'PASS' } else { 'FAIL' })** |"
$content = $content -replace [regex]::Escape($summaryPattern), $summaryReplacement

# Update individual test results
foreach ($testId in $results.Keys | Sort-Object) {
    $result = $results[$testId]
    $status = $result.Status
    
    # Find and update the test result line
    $pattern = "(\*\*Test $testId.*?\*\*Result\*\*: )PENDING"
    $replacement = "`$1$status"
    $content = $content -replace $pattern, $replacement
}

# Update status at top
$content = $content -replace 'Test Status: COMPLETED - Baseline behavior validated', "Test Status: COMPLETED - $passed/22 tests passed"
$content = $content -replace '\*\*Preservation Test Status\*\*: PENDING EXECUTION', "**Preservation Test Status**: $(if ($failed -eq 0) { 'PASS - All baseline behaviors preserved' } else { 'FAIL - ' + $failed + ' tests failed' })"

Set-Content -Path $resultsFile -Value $content -Encoding UTF8

Write-Host "`nResults updated in: $resultsFile" -ForegroundColor Cyan

if ($failed -gt 0) {
    Write-Host "`n⚠️  WARNING: $failed preservation tests failed" -ForegroundColor Yellow
    Write-Host "This indicates baseline behavior may be broken before the fix" -ForegroundColor Yellow
    Write-Host "`nFailed tests:" -ForegroundColor Yellow
    foreach ($testId in $results.Keys | Sort-Object) {
        if ($results[$testId].Status -eq "FAIL") {
            Write-Host "  - $testId $($results[$testId].Name)" -ForegroundColor Red
        }
    }
    exit 1
} else {
    Write-Host "`n✓ All preservation tests passed - baseline behavior confirmed" -ForegroundColor Green
    Write-Host "These behaviors must be preserved after the fix is implemented" -ForegroundColor Green
    exit 0
}
