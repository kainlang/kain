param(
    [string]$AppRoot,
    [int]$PollMs = 1000,
    [int]$MaxIterations = 0,
    [switch]$ExecuteFabricHotPath,
    [switch]$RegenerateShell
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($AppRoot)) {
    $AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
}

$AppRoot = (Resolve-Path $AppRoot).Path
$QueuePath = Join-Path $AppRoot "state/command_queue.jsonl"
$ProcessorPath = Join-Path $AppRoot "scripts/process-command-queue.ps1"
$Iteration = 0

Write-Host "Starting dispatcher loop for $AppRoot"
Write-Host "PollMs=$PollMs ExecuteFabricHotPath=$ExecuteFabricHotPath RegenerateShell=$RegenerateShell"

while ($true) {
    if ($MaxIterations -gt 0 -and $Iteration -ge $MaxIterations) {
        Write-Host "Dispatcher loop reached MaxIterations=$MaxIterations"
        break
    }

    $Iteration += 1

    if (-not (Test-Path $QueuePath)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $QueuePath) | Out-Null
        Set-Content -Path $QueuePath -Value ""
    }

    $PendingLines = @(Get-Content $QueuePath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if (@($PendingLines).Count -gt 0) {
        Write-Host "Dispatcher iteration $Iteration processing $(@($PendingLines).Count) queued command(s)"
        if ($ExecuteFabricHotPath -and $RegenerateShell) {
            & $ProcessorPath -AppRoot $AppRoot -ExecuteFabricHotPath -RegenerateShell
        }
        elseif ($ExecuteFabricHotPath) {
            & $ProcessorPath -AppRoot $AppRoot -ExecuteFabricHotPath
        }
        elseif ($RegenerateShell) {
            & $ProcessorPath -AppRoot $AppRoot -RegenerateShell
        }
        else {
            & $ProcessorPath -AppRoot $AppRoot
        }
    }

    Start-Sleep -Milliseconds $PollMs
}
