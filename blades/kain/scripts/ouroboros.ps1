# ============================================================================
# ouroboros.ps1 - Full Bootstrap Pipeline for Kain Self-Host Compiler
# ============================================================================
# Phase 1: Combined source assembly (concatenate 23 files in source_order)
# Phase 2: Compile via Rust bootstrap (Stage 0 -> Stage 1)
# Phase 3: Self-compile (Stage 1 -> Stage 2)
# Phase 4: Verification - byte-identical binary comparison
# ============================================================================

param(
    [switch]$SkipCombine,
    [switch]$SkipStage1,
    [switch]$SkipStage2,
    [switch]$SkipVerify,
    [switch]$OnlyCombine
)

$ErrorActionPreference = "Stop"
$script:StartTime = Get-Date
$script:ReportLines = New-Object System.Collections.ArrayList
$script:PhaseResults = New-Object System.Collections.ArrayList

# Paths
$BladeRoot = (Resolve-Path "$PSScriptRoot\..").Path
$SrcDir    = "$BladeRoot\src"
$ReviewDir = "$BladeRoot\review"
$CombinedDir = "$BladeRoot\combined"
$Stage1Dir   = "$BladeRoot\stage1"
$Stage2Dir   = "$BladeRoot\stage2"

$CombinedSource = "$CombinedDir\kainc_bootstrap.kn"
$Stage1LL        = "$Stage1Dir\kainc_bootstrap.ll"
$Stage1Exe       = "$Stage1Dir\kainc.exe"
$Stage2LL        = "$Stage2Dir\kainc_bootstrap.ll"
$Stage2Exe       = "$Stage2Dir\kainc.exe"
$ReportPath      = "$ReviewDir\ouroboros_result.md"

# Find kain.exe
$KainExe = $null
@(
    "$env:KAIN_HOME\bin\kain.exe",
    "$BladeRoot\..\..\.kain\bin\kain.exe",
    "$BladeRoot\..\..\.kain\bin\kain.cmd"
) | ForEach-Object {
    if ((-not $KainExe) -and (Test-Path $_)) { $KainExe = $_ }
}
if (-not $KainExe) {
    $found = Get-Command kain.exe -ErrorAction SilentlyContinue
    if ($found) { $KainExe = $found.Source }
}

# Find clang.exe
$ClangExe = $null
$clangFound = Get-Command clang.exe -ErrorAction SilentlyContinue
if ($clangFound) { $ClangExe = $clangFound.Source }

# Source order from KAIN.toml
$SourceOrder = @(
    "token.kn", "error.kn", "span.kn", "ast.kn", "build.kn",
    "lexer.kn", "builtins.kn", "runtime.kn", "llvm_ffi.kn",
    "jit_metal.kn", "jit_x86.kn", "jit_orc.kn", "jit_cache.kn", "jit.kn",
    "parser.kn", "types.kn", "effects.kn", "monomorphize.kn", "codegen.kn",
    "orchestrator.kn", "compiler.kn", "cli.kn", "main.kn"
)

$TotalSourceLines = 0
$SourceFileCount = $SourceOrder.Count

# ============================================================================
# Helpers
# ============================================================================
function Write-Log {
    param([string]$msg)
    $null = $script:ReportLines.Add($msg)
    Write-Host $msg
}

function Get-Dur {
    param($ts)
    return "{0:F1}s" -f $ts.TotalSeconds
}

function Add-Result {
    param($phase, $status, $duration)
    $null = $script:PhaseResults.Add(@{ P = $phase; S = $status; D = $duration })
}

# ============================================================================
# PHASE 1
# ============================================================================
function Do-Phase1 {
    Write-Log ""
    Write-Log "--- Phase 1: Combined Source Assembly ---"
    $start = Get-Date

    if (-not (Test-Path $CombinedDir)) {
        New-Item -ItemType Directory -Path $CombinedDir -Force | Out-Null
    }

    $combined = ""
    $total = 0
    $missing = @()

    foreach ($f in $SourceOrder) {
        $fp = Join-Path $SrcDir $f
        if (-not (Test-Path $fp)) {
            $missing += $f
            Write-Log "  MISSING: $f"
            continue
        }
        $ct = Get-Content $fp -Raw
        $lc = ($ct -split "`r?`n").Count
        $total += $lc
        $combined += "// === FILE: $f ===`r`n"
        $combined += $ct
        if (-not $ct.EndsWith("`n")) { $combined += "`r`n" }
        $combined += "`r`n"
        Write-Host "  + $f   lines=$lc"
    }

    if ($missing.Count -gt 0) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 1 (Combine)" "FAIL" $dur
        Write-Log "  FAIL: $($missing.Count) missing files."
        return $false
    }

    Set-Content -Path $CombinedSource -Value $combined -Encoding UTF8
    $script:TotalSourceLines = $total
    $sz = (Get-Item $CombinedSource).Length
    $szKB = [math]::Round($sz / 1024, 1)
    $dur = Get-Dur ((Get-Date) - $start)

    Add-Result "Phase 1 (Combine)" "PASS" $dur
    Write-Log "  PASS: $SourceFileCount files, $total lines, $szKB KB combined"
    return $true
}

# ============================================================================
# PHASE 2
# ============================================================================
function Do-Phase2 {
    Write-Log ""
    Write-Log "--- Phase 2: Rust Bootstrap (Stage 0 -> Stage 1) ---"
    $start = Get-Date

    if (-not $KainExe) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 2 (Stage 0->1)" "FAIL" $dur
        Write-Log "  FAIL: kain.exe not found."
        return $false
    }

    if (-not (Test-Path $CombinedSource)) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 2 (Stage 0->1)" "FAIL" $dur
        Write-Log "  FAIL: Combined source not found."
        return $false
    }

    if (-not (Test-Path $Stage1Dir)) {
        New-Item -ItemType Directory -Path $Stage1Dir -Force | Out-Null
    }

    Write-Log "  Compiler: $KainExe"

    # Try kain build on the combined source
    $buildArgs = @("build", $CombinedSource, "--target", "llvm")
    $out = cmd /c "`"$KainExe`" build `"$CombinedSource`" --target llvm 2>&1"
    $ec = $LASTEXITCODE

    if ($ec -ne 0) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 2 (Stage 0->1)" "FAIL" $dur
        Write-Log "  FAIL: kain build exited with code $ec"
        $lines = $out -split "`r`n"
        foreach ($l in $lines) {
            if ($l.Trim() -ne "") { Write-Log "    $l" }
        }
        Write-Log ""
        Write-Log "  NOTE: Combined source fails because llvm_ffi.kn has an"
        Write-Log "  unresolved 'include <llvm-c/Core.h>' statement."
        Write-Log "  Individual files pass check (21/23) when compiled via build.kn."
        return $false
    }

    Write-Log "  Build succeeded. Looking for LLVM IR..."

    $od = "$BladeRoot\.kain\out"
    $lls = @(Get-ChildItem -Path $od -Recurse -Filter "*.ll" -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending)

    if ($lls.Count -gt 0) {
        Copy-Item $lls[0].FullName $Stage1LL -Force
        $sz = (Get-Item $Stage1LL).Length
        $szKB = [math]::Round($sz / 1024, 1)
        Write-Log "  LLVM IR: $szKB KB"
    }

    if ((Test-Path $Stage1LL) -and $ClangExe) {
        Write-Log "  Linking via clang..."
        $ca = @($Stage1LL, "-o", $Stage1Exe, "-O0", "-g", "-target", "x86_64-pc-windows-msvc")
        & $ClangExe $ca 2>&1 | Out-Null
        if (Test-Path $Stage1Exe) {
            $sz = (Get-Item $Stage1Exe).Length
            $szKB = [math]::Round($sz / 1024, 0)
            Write-Log "  Native binary: $szKB KB"
        }
    }

    $dur = Get-Dur ((Get-Date) - $start)

    if (Test-Path $Stage1Exe) {
        Add-Result "Phase 2 (Stage 0->1)" "PASS" $dur
        Write-Log "  PASS: Native binary produced."
    }
    elseif (Test-Path $Stage1LL) {
        Add-Result "Phase 2 (Stage 0->1)" "PARTIAL" $dur
        Write-Log "  PARTIAL: LLVM IR generated but no native binary."
    }
    else {
        Add-Result "Phase 2 (Stage 0->1)" "FAIL" $dur
        return $false
    }
    return $true
}

# ============================================================================
# PHASE 3
# ============================================================================
function Do-Phase3 {
    Write-Log ""
    Write-Log "--- Phase 3: Self-Compile (Stage 1 -> Stage 2) ---"
    $start = Get-Date

    if (-not (Test-Path $Stage1Exe)) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 3 (Stage 1->2)" "SKIP" $dur
        Write-Log "  SKIP: No Stage 1 binary available."
        Write-Log ""
        Write-Log "  OUROBOROS NOT READY: Cannot self-compile without Stage 1 binary."
        return $false
    }

    if (-not (Test-Path $CombinedSource)) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 3 (Stage 1->2)" "FAIL" $dur
        Write-Log "  FAIL: Combined source not found."
        return $false
    }

    if (-not (Test-Path $Stage2Dir)) {
        New-Item -ItemType Directory -Path $Stage2Dir -Force | Out-Null
    }

    Write-Log "  Compiler: $Stage1Exe"

    $ec = 1
    try {
        $rawOut = cmd /c "`"$Stage1Exe`" build `"$CombinedSource`" --target llvm 2>&1"
        $ec = $LASTEXITCODE
        $out = $rawOut
    }
    catch {
        $out = $_.Exception.Message
        $ec = -1
    }

    if ($ec -ne 0) {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 3 (Stage 1->2)" "FAIL" $dur
        Write-Log "  FAIL: Stage 1 exited with code $ec"
        $lines = $out -split "`r`n"
        foreach ($l in $lines) {
            if ($l.Trim() -ne "") { Write-Log "    $l" }
        }
        Write-Log ""
        Write-Log "  OUROBOROS NOT READY."
        Write-Log "  Reason: Self-host compiler has stub typechecker and codegen."
        Write-Log "  See review/FINAL_GAPS.md and review/bootstrap_assessment.md"
        return $false
    }

    $od = "$BladeRoot\.kain\out"
    $lls = @(Get-ChildItem -Path $od -Recurse -Filter "*.ll" -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending)
    if ($lls.Count -gt 0) {
        Copy-Item $lls[0].FullName $Stage2LL -Force
    }

    if ((Test-Path $Stage2LL) -and $ClangExe) {
        $ca = @($Stage2LL, "-o", $Stage2Exe, "-O0", "-g", "-target", "x86_64-pc-windows-msvc")
        & $ClangExe $ca 2>&1 | Out-Null
    }

    $dur = Get-Dur ((Get-Date) - $start)
    Add-Result "Phase 3 (Stage 1->2)" "PASS" $dur
    Write-Log "  PASS: Stage 2 compilation succeeded."
    return $true
}

# ============================================================================
# PHASE 4
# ============================================================================
function Do-Phase4 {
    Write-Log ""
    Write-Log "--- Phase 4: Verification ---"
    $start = Get-Date

    $s1e = Test-Path $Stage1Exe
    $s2e = Test-Path $Stage2Exe
    $s1l = Test-Path $Stage1LL
    $s2l = Test-Path $Stage2LL

    if ($s1e -and $s2e) {
        Write-Log "  Comparing native binaries..."
        $b1 = [System.IO.File]::ReadAllBytes((Resolve-Path $Stage1Exe))
        $b2 = [System.IO.File]::ReadAllBytes((Resolve-Path $Stage2Exe))
        $r = Comp-Bytes $b1 $b2
    }
    elseif ($s1l -and $s2l) {
        Write-Log "  Comparing LLVM IR..."
        $b1 = [System.IO.File]::ReadAllBytes((Resolve-Path $Stage1LL))
        $b2 = [System.IO.File]::ReadAllBytes((Resolve-Path $Stage2LL))
        $r = Comp-Bytes $b1 $b2
    }
    else {
        $dur = Get-Dur ((Get-Date) - $start)
        Add-Result "Phase 4 (Verify)" "SKIP" $dur
        Write-Log "  SKIP: No comparable artifacts available."
        Write-Log ""
        Write-Log "  OUROBOROS NOT READY."
        Write-Log "  Reason: Self-host compiler has stub typechecker and codegen."
        return $false
    }

    $dur = Get-Dur ((Get-Date) - $start)
    if ($r.Ok) {
        Add-Result "Phase 4 (Verify)" "OUROBOROS VERIFIED" $dur
        Write-Log "  OUROBOROS VERIFIED!"
        Write-Log "  Byte-identical across $($r.Len) bytes."
    }
    else {
        Add-Result "Phase 4 (Verify)" "MISMATCH" $dur
        Write-Log "  MISMATCH at byte $($r.Off) of $($r.Min)"
    }
    return $r.Ok
}

function Comp-Bytes {
    param([byte[]]$b1, [byte[]]$b2)
    $l1 = $b1.Length
    $l2 = $b2.Length
    Write-Log "  Stage 1: $l1 bytes"
    Write-Log "  Stage 2: $l2 bytes"
    $mn = [math]::Min($l1, $l2)
    $mm = -1
    for ($i = 0; $i -lt $mn; $i++) {
        if ($b1[$i] -ne $b2[$i]) { $mm = $i; break }
    }
    if ($mm -ge 0) {
        return @{ Ok = $false; Off = $mm; Min = $mn; Len = 0 }
    }
    if ($l1 -ne $l2) {
        return @{ Ok = $false; Off = $mn; Min = $mn; Len = 0 }
    }
    return @{ Ok = $true; Off = -1; Min = $mn; Len = $l1 }
}

# ============================================================================
# Report
# ============================================================================
function Write-ReportHeader {
    Write-Log "# Ouroboros Verification Report"
    Write-Log ""
    $dt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Log "Date: $dt"
    Write-Log "Project: kainc (Kain Self-Host Compiler)"
    Write-Log "Source files: $SourceFileCount"
    Write-Log "Total source lines: $script:TotalSourceLines"
    $combinedSize = 0
    if (Test-Path $CombinedSource) { $combinedSize = (Get-Item $CombinedSource).Length }
    $combinedSizeKB = [math]::Round($combinedSize / 1024, 1)
    Write-Log "Combined source: $CombinedSource / $combinedSizeKB KB"
    Write-Log ""
}

function Write-FinalReport {
    $txt = $script:ReportLines -join "`r`n"
    Set-Content -Path $ReportPath -Value $txt -Encoding UTF8
    Write-Host ""
    Write-Host "Report saved to: $ReportPath" -ForegroundColor Cyan
}

function Write-Summary {
    Write-Log ""
    Write-Log "## Phase Results"
    Write-Log ""
    Write-Log "| Phase | Status | Duration |"
    Write-Log "|-------|--------|----------|"
    foreach ($e in $script:PhaseResults) {
        Write-Log "| $($e.P) | $($e.S) | $($e.D) |"
    }
    Write-Log ""

    Write-Log "## Artifacts"
    Write-Log ""
    @(
        @{N="Combined source"; P=$CombinedSource},
        @{N="Stage 1 LLVM IR"; P=$Stage1LL},
        @{N="Stage 1 binary";  P=$Stage1Exe},
        @{N="Stage 2 LLVM IR"; P=$Stage2LL},
        @{N="Stage 2 binary";  P=$Stage2Exe}
    ) | ForEach-Object {
        if (Test-Path $_.P) {
            $sz = (Get-Item $_.P).Length
            $kb = [math]::Round($sz / 1024, 1)
            Write-Log ("  " + $_.N + ": " + $_.P + " / " + $kb + " KB / " + $sz + " bytes")
        }
    }
    Write-Log ""

    $td = Get-Dur ((Get-Date) - $script:StartTime)
    Write-Log "Total duration: $td"
    Write-Log ""
}

# ============================================================================
# MAIN
# ============================================================================

# Phase 1
if (-not $SkipCombine) {
    $ok = Do-Phase1
    if (-not $ok) { Write-Summary; Write-FinalReport; exit 1 }
}
else { Write-Log "--- Phase 1: SKIPPED ---" }

# Now write the report header (after we have source line counts)
Write-ReportHeader

if ($OnlyCombine) {
    Write-Log ""
    Write-Log "--- Combine-only mode ---"
    Write-Summary; Write-FinalReport; exit 0
}

# Phase 2
if (-not $SkipStage1) { $null = Do-Phase2 }
else { Write-Log "--- Phase 2: SKIPPED ---" }

# Phase 3
if (-not $SkipStage2) { $null = Do-Phase3 }
else { Write-Log "--- Phase 3: SKIPPED ---" }

# Phase 4
if (-not $SkipVerify) { $null = Do-Phase4 }
else { Write-Log "--- Phase 4: SKIPPED ---" }

Write-Summary
Write-FinalReport
Write-Log "Done."
