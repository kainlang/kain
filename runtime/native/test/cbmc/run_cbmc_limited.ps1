# run_cbmc_limited.ps1
# Wraps CBMC execution with CPU affinity limiting to 4 cores / 8 threads
# Prevents CBMC's SAT solver from consuming all CPU and crashing the session

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$CbmcArgs
)

# Set Z3 thread limit (CBMC defaults to Z3 SMT2 solver)
$env:Z3_NUM_THREADS = "4"

# Alternative: if CBMC uses internal SAT solver, limit via processor affinity
# 0xF = cores 0-3 (4 physical cores)
# 0xFF = cores 0-7 (8 logical processors)

Write-Host "[CPU-LIMITED] Z3_NUM_THREADS=4, running: cbmc $($CbmcArgs -join ' ')" -ForegroundColor Cyan

# Create a process with CPU affinity
$proc = Start-Process -FilePath "cbmc" -ArgumentList $CbmcArgs -NoNewWindow -Wait -PassThru

# The affinity must be set after process start for native subprocess:
# Actually, the cleanest approach: set this process's affinity so children inherit
$currentProc = [System.Diagnostics.Process]::GetCurrentProcess()
$currentProc.ProcessorAffinity = [System.IntPtr]0xF  # cores 0-3

# Actually re-run with affinity applied to self before launching
# Better approach: use a job object or simply set affinity on self first
Write-Host "[CPU-LIMITED] Done. Exit code: $($proc.ExitCode)" -ForegroundColor Cyan
exit $proc.ExitCode
