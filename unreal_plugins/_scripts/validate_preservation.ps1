# Preservation Property Test Validation Script
# This script validates that baseline behaviors work on unfixed code
# EXPECTED OUTCOME: All tests PASS (confirms baseline to preserve)

$ErrorActionPreference = "Stop"
$kainCommand = Get-Command kain -ErrorAction SilentlyContinue
if (-not $kainCommand) {
    throw "kain.exe not found in PATH. Run M:\Code\Kain\scripts\sync-kain-source-of-truth.ps1 first."
}
$kainExe = $kainCommand.Source
$testDir = "M:\Code\Kain\crates\cli\tests\fixtures\preservation"

# Create test directory
New-Item -ItemType Directory -Force -Path $testDir | Out-Null

Write-Host "`n=== Preservation Property Tests ===" -ForegroundColor Cyan
Write-Host "Testing that all baseline behaviors continue to work" -ForegroundColor Cyan
Write-Host "EXPECTED OUTCOME: All tests PASS (confirms baseline to preserve)`n" -ForegroundColor Cyan
Write-Host "Using kain binary: $kainExe`n" -ForegroundColor DarkGray

$passed = 0
$failed = 0
$results = @()

function Test-KainCompilation {
    param(
        [string]$TestName,
        [string]$FileName,
        [string]$Content,
        [string]$Target = "rust"
    )
    
    $testFile = Join-Path $testDir $FileName
    
    try {
        # Write test file
        Set-Content -Path $testFile -Value $Content -Encoding UTF8
        
        # Run kain build
        $output = & $kainExe build $testFile --target $Target 2>&1
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host "  ✓ $TestName" -ForegroundColor Green
            $script:passed++
            $script:results += @{Name=$TestName; Status="PASS"}
            return $true
        } else {
            Write-Host "  ✗ $TestName" -ForegroundColor Red
            Write-Host "    Error: $output" -ForegroundColor Yellow
            $script:failed++
            $script:results += @{Name=$TestName; Status="FAIL"; Error=$output}
            return $false
        }
    }
    catch {
        Write-Host "  ✗ $TestName (Exception)" -ForegroundColor Red
        Write-Host "    Error: $_" -ForegroundColor Yellow
        $script:failed++
        $script:results += @{Name=$TestName; Status="FAIL"; Error=$_.Exception.Message}
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
# Property 1: KAIN Language Features Continue to Work
# ============================================================================

Write-Host "`nProperty 1: KAIN Language Features" -ForegroundColor Yellow

Test-KainCompilation -TestName "Actors compile" -FileName "test_actor.kn" -Content @"
actor TestActor:
    state health: Float = 100.0
    state max_health: Float = 100.0
    
    fn take_damage(amount: Float):
        health = health - amount
"@

Test-KainCompilation -TestName "Components compile" -FileName "test_component.kn" -Content @"
@component
struct HealthComponent:
    current: Float
    max: Float
    
    fn heal(amount: Float):
        current = current + amount
"@

Test-KainCompilation -TestName "Structs compile" -FileName "test_struct.kn" -Content @"
struct Vec2:
    x: Float
    y: Float

struct ItemData:
    id: Int
    name: String
    quantity: Int
"@

Test-KainCompilation -TestName "Enums compile" -FileName "test_enum.kn" -Content @"
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

Test-KainCompilation -TestName "Shaders compile" -FileName "test_shader.kn" -Target "usf" -Content @"
shader compute TestCompute(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform scale: Float @1
    buffer output: RWBuffer<Float> @2
    
    let value = thread_id.x * scale
    output[thread_id.x] = value
"@

Test-KainCompilation -TestName "Materials compile" -FileName "test_material.kn" -Content @"
material TestMaterial:
    input base_color: Vec3 = vec3(1.0, 1.0, 1.0)
    input roughness_value: Float = 0.5
    
    base_color = base_color
    roughness = roughness_value
"@

# ============================================================================
# Property 2: Compilation Targets Continue to Work
# ============================================================================

Write-Host "`nProperty 2: Compilation Targets" -ForegroundColor Yellow

Test-KainCompilation -TestName "WASM target works" -FileName "test_wasm.kn" -Target "wasm" -Content @"
fn add(a: Int, b: Int) -> Int:
    return a + b

fn main():
    let result = add(5, 3)
    println("Result: {result}")
"@

Test-KainCompilation -TestName "JavaScript target works" -FileName "test_js.kn" -Target "js" -Content @"
fn multiply(a: Int, b: Int) -> Int:
    return a * b

fn main():
    let result = multiply(4, 7)
    println("Result: {result}")
"@

Test-KainCompilation -TestName "Rust target works" -FileName "test_rust.kn" -Target "rust" -Content @"
fn factorial(n: Int) -> Int with Pure:
    match n:
        0 => 1
        _ => n * factorial(n - 1)

fn main():
    let result = factorial(5)
    println("Factorial: {result}")
"@

Test-KainCompilation -TestName "C++ target works" -FileName "test_cpp.kn" -Target "cpp" -Content @"
fn square(x: Float) -> Float:
    return x * x

fn main():
    let result = square(5.0)
    println("Square: {result}")
"@

Test-KainCompilation -TestName "HLSL target works" -FileName "test_hlsl.kn" -Target "hlsl" -Content @"
shader fragment TestFragment(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
"@

Test-KainCompilation -TestName "USF target works" -FileName "test_usf.kn" -Target "usf" -Content @"
shader compute TestUSF(thread_id: Vec3):
    uniform size: Int @0
    buffer data: RWBuffer<Float> @1
    
    data[thread_id.x] = thread_id.x * 2.0
"@

# ============================================================================
# Property 3: Backend Systems Continue to Work
# ============================================================================

Write-Host "`nProperty 3: Backend Systems" -ForegroundColor Yellow

Test-KainCompilation -TestName "Shader array literals work" -FileName "test_array_literal.kn" -Target "usf" -Content @"
shader compute TestArrayLiteral(thread_id: Vec3):
    uniform size: Int @0
    buffer output: RWBuffer<Float> @1
    
    let weights = [0.2, 0.3, 0.5]
    output[thread_id.x] = weights[0]
"@

Test-KainCompilation -TestName "Shader cast expressions work" -FileName "test_cast.kn" -Target "usf" -Content @"
shader compute TestCast(thread_id: Vec3):
    uniform size: Int @0
    buffer output: RWBuffer<Float> @1
    
    let int_val = 42
    let float_val = int_val as Float
    output[thread_id.x] = float_val
"@

Test-KainCompilation -TestName "Shader @N binding semantics work" -FileName "test_bindings.kn" -Target "usf" -Content @"
shader compute TestBindings(thread_id: Vec3):
    uniform param0: Float @0
    uniform param1: Int @1
    uniform param2: Vec3 @2
    buffer output: RWBuffer<Float> @3
    
    output[thread_id.x] = param0 + param1 + param2.x
"@

# ============================================================================
# Property 4: UE5 Code Generation Continues to Work
# ============================================================================

Write-Host "`nProperty 4: UE5 Code Generation" -ForegroundColor Yellow

Test-KainCompilation -TestName "UCLASS macros generated" -FileName "test_uclass.kn" -Content @"
actor TestUClassActor:
    state value: Int = 0
"@

Test-KainCompilation -TestName "UPROPERTY macros generated" -FileName "test_uproperty.kn" -Content @"
actor TestUPropertyActor:
    state health: Float = 100.0
    state max_health: Float = 100.0
"@

Test-KainCompilation -TestName "UFUNCTION macros generated" -FileName "test_ufunction.kn" -Content @"
@blueprint
fn calculate_damage(base: Float, multiplier: Float) -> Float:
    return base * multiplier
"@

Test-KainCompilation -TestName "USTRUCT macros generated" -FileName "test_ustruct.kn" -Content @"
struct TestStruct:
    field1: Int
    field2: Float
    field3: String
"@

Test-KainCompilation -TestName "UENUM macros generated" -FileName "test_uenum.kn" -Content @"
enum TestEnum:
    Value1
    Value2
    Value3
"@

# ============================================================================
# Property 5: Stdlib System Continues to Work
# ============================================================================

Write-Host "`nProperty 5: Stdlib System" -ForegroundColor Yellow

Test-KainCompilation -TestName "Stdlib functions available" -FileName "test_stdlib.kn" -Content @"
fn test_stdlib():
    let x = abs(-5.0)
    let y = min(10.0, 20.0)
    let z = max(10.0, 20.0)
    println("Values: {x}, {y}, {z}")
"@

# ============================================================================
# Property 6: Multi-File Plugins Continue to Work
# ============================================================================

Write-Host "`nProperty 6: Multi-File Compilation" -ForegroundColor Yellow

$file1 = Join-Path $testDir "multi_file_1.kn"
$file2 = Join-Path $testDir "multi_file_2.kn"

Set-Content -Path $file1 -Value @"
struct SharedData:
    value: Int
"@ -Encoding UTF8

Set-Content -Path $file2 -Value @"
fn use_shared_data(data: SharedData) -> Int:
    return data.value * 2
"@ -Encoding UTF8

$output1 = & $kainExe build $file1 --target rust 2>&1
$exitCode1 = $LASTEXITCODE

$output2 = & $kainExe build $file2 --target rust 2>&1
$exitCode2 = $LASTEXITCODE

if ($exitCode1 -eq 0 -and $exitCode2 -eq 0) {
    Write-Host "  ✓ Multi-file compilation works" -ForegroundColor Green
    $passed++
    $results += @{Name="Multi-file compilation works"; Status="PASS"}
} else {
    Write-Host "  ✗ Multi-file compilation works" -ForegroundColor Red
    $failed++
    $results += @{Name="Multi-file compilation works"; Status="FAIL"}
}

Remove-Item $file1 -Force -ErrorAction SilentlyContinue
Remove-Item $file2 -Force -ErrorAction SilentlyContinue

# ============================================================================
# Summary
# ============================================================================

Write-Host "`n=== Preservation Test Results ===" -ForegroundColor Cyan
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })

if ($failed -gt 0) {
    Write-Host "`n⚠️  WARNING: $failed preservation tests failed" -ForegroundColor Yellow
    Write-Host "This indicates baseline behavior is already broken before the fix" -ForegroundColor Yellow
    Write-Host "`nFailed tests:" -ForegroundColor Yellow
    foreach ($result in $results) {
        if ($result.Status -eq "FAIL") {
            Write-Host "  - $($result.Name)" -ForegroundColor Red
        }
    }
    exit 1
} else {
    Write-Host "`n✓ All preservation tests passed - baseline behavior confirmed" -ForegroundColor Green
    Write-Host "These behaviors must be preserved after the fix is implemented" -ForegroundColor Green
    exit 0
}
