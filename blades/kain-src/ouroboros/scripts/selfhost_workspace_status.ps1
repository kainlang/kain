param(
    [string]$PipelineRoot = "",
    [string]$Phase2Root = "",
    [string]$RepairedRoot = ""
)

$ErrorActionPreference = "Stop"
$OuroborosRoot = Split-Path -Parent $PSScriptRoot
if (-not $PipelineRoot) { $PipelineRoot = Join-Path $OuroborosRoot "out\selfhost\pipeline" }
if (-not $Phase2Root) { $Phase2Root = Join-Path $OuroborosRoot "out\selfhost\phase2" }
if (-not $RepairedRoot) { $RepairedRoot = Join-Path $OuroborosRoot "out\selfhost\phase2_repaired" }

function Read-JsonFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $null }
    return Get-Content $Path -Raw | ConvertFrom-Json
}

function Get-PipelineStep {
    param(
        $Summary,
        [string]$StepId
    )
    if (-not $Summary -or -not $Summary.steps) { return $null }
    foreach ($step in $Summary.steps) {
        if ($step.id -eq $StepId) { return $step }
    }
    return $null
}

function Get-DuplicateTypeBlockers {
    param($FrontErrorSource)

    if (-not $FrontErrorSource -or -not $FrontErrorSource.front_errors) {
        return @()
    }

    $duplicatesBySymbol = @{}
    foreach ($error in $FrontErrorSource.front_errors) {
        if ($error.code -ne "E0428" -or -not $error.text) {
            continue
        }

        $match = [regex]::Match(
            $error.text,
            'the name `(?<symbol>[^`]+)` is defined multiple times'
        )
        if (-not $match.Success) {
            continue
        }

        $symbol = $match.Groups["symbol"].Value
        if (-not $duplicatesBySymbol.ContainsKey($symbol)) {
            $duplicatesBySymbol[$symbol] = [ordered]@{
                symbol = $symbol
                occurrences = 0
                files = @()
                lines = @()
            }
        }

        $entry = $duplicatesBySymbol[$symbol]
        $entry.occurrences = [int]$entry.occurrences + 1
        if ($error.file) {
            $entry.files += $error.file
        }
        if ($error.line -ne $null) {
            $entry.lines += [int]$error.line
        }
    }

    $result = @()
    foreach ($symbol in ($duplicatesBySymbol.Keys | Sort-Object)) {
        $entry = $duplicatesBySymbol[$symbol]
        $result += [ordered]@{
            symbol = $entry.symbol
            occurrences = $entry.occurrences
            files = @($entry.files | Select-Object -Unique)
            lines = @($entry.lines | Sort-Object -Unique)
        }
    }

    return $result
}

$coreSummary = Read-JsonFile (Join-Path $PipelineRoot "phase2-core_summary.json")
$fullSummary = Read-JsonFile (Join-Path $PipelineRoot "phase2-full_summary.json")
$repairReport = Read-JsonFile (Join-Path $RepairedRoot "phase2_repair_report.json")
$frontErrors = Read-JsonFile (Join-Path $RepairedRoot "front_errors.json")
$phase1Report = Read-JsonFile (Join-Path $OuroborosRoot "out\selfhost\phase1_report.json")
$phase2Report = Read-JsonFile (Join-Path $Phase2Root "phase2_report.json")
$phase2RepairedReport = Read-JsonFile (Join-Path $RepairedRoot "phase2_report.json")

$stage2Binary = $null
foreach ($candidate in @(
    (Join-Path $RepairedRoot "stage2_workspace\\target\\debug\\kain.exe"),
    (Join-Path $RepairedRoot "stage2_workspace\\target\\debug\\kain"),
    (Join-Path $RepairedRoot "stage2_workspace\\target\\release\\kain.exe"),
    (Join-Path $RepairedRoot "stage2_workspace\\target\\release\\kain"),
    (Join-Path $Phase2Root "stage2_workspace\\target\\debug\\kain.exe"),
    (Join-Path $Phase2Root "stage2_workspace\\target\\debug\\kain"),
    (Join-Path $Phase2Root "stage2_workspace\\target\\release\\kain.exe"),
    (Join-Path $Phase2Root "stage2_workspace\\target\\release\\kain")
)) {
    if (Test-Path $candidate) {
        $stage2Binary = $candidate
        break
    }
}

$bucketCounts = @{}
if ($repairReport -and $repairReport.after -and $repairReport.after.bucket_counts) {
    $bucketCounts = $repairReport.after.bucket_counts
} elseif ($repairReport -and $repairReport.before -and $repairReport.before.bucket_counts) {
    $bucketCounts = $repairReport.before.bucket_counts
}

$hotspots = @()
if ($repairReport -and $repairReport.files_still_failing_hardest) {
    $hotspots = @($repairReport.files_still_failing_hardest | Select-Object -First 20)
}

$coreCheckStep = Get-PipelineStep -Summary $coreSummary -StepId "core_check"
$phase2BuildSource = $phase2RepairedReport
$phase2BuildReportPath = Join-Path $RepairedRoot "phase2_report.json"
if (
    -not $phase2BuildSource -or
    (
        -not ($phase2BuildSource.PSObject.Properties.Name -contains "stage2_build_log_path") -and
        -not ($phase2BuildSource.PSObject.Properties.Name -contains "stage2_build_exit_code")
    )
) {
    $phase2BuildSource = $phase2Report
    $phase2BuildReportPath = Join-Path $Phase2Root "phase2_report.json"
}

$frontErrorSource = $frontErrors
if (-not $frontErrorSource -and $coreSummary -and $coreSummary.front_errors) {
    $frontErrorSource = $coreSummary.front_errors
}
$duplicateTypeBlockers = Get-DuplicateTypeBlockers -FrontErrorSource $frontErrorSource

$frontBlocker = $null
if ($frontErrorSource -and $frontErrorSource.front_errors -and $frontErrorSource.front_errors.Count -gt 0) {
    $front = $frontErrorSource.front_errors[0]
    $frontText = $front.text
    if ($frontText) {
        $frontText = ($frontText -split "`n")[0].Trim()
    }
    $frontBlocker = [ordered]@{
        code = $front.code
        bucket = $front.bucket
        file = $front.file
        line = $front.line
        col = $front.col
        summary = $frontText
    }
}

$inventoryInputs = @()
if ($phase1Report -and $phase1Report.inventory_inputs) {
    foreach ($entry in $phase1Report.inventory_inputs) {
        $inventoryInputs += [ordered]@{
            inventory_key = $entry.inventory_key
            path = $entry.path
            byte_size = $entry.byte_size
            exists = if ($entry.path) { [bool](Test-Path $entry.path) } else { $false }
        }
    }
}

$payload = [ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    phase2_core = $coreSummary
    phase2_full = $fullSummary
    phase2_core_check = [ordered]@{
        success = if ($coreCheckStep) { [bool]$coreCheckStep.success } else { $null }
        returncode = if ($coreCheckStep) { $coreCheckStep.returncode } else { $null }
        log_path = if ($coreCheckStep) { $coreCheckStep.log_path } else { $null }
    }
    phase2_build_evidence = [ordered]@{
        report_path = $phase2BuildReportPath
        stage2_build_success = if ($phase2BuildSource) { $phase2BuildSource.stage2_build_success } else { $null }
        stage2_build_artifact = if ($phase2BuildSource) { $phase2BuildSource.stage2_build_artifact } else { $null }
        stage2_build_log_path = if ($phase2BuildSource) { $phase2BuildSource.stage2_build_log_path } else { $null }
        stage2_build_exit_code = if ($phase2BuildSource) { $phase2BuildSource.stage2_build_exit_code } else { $null }
    }
    phase1_inventory_evidence = [ordered]@{
        report_path = (Join-Path $OuroborosRoot "out\selfhost\phase1_report.json")
        inventory_dir = if ($phase1Report) { $phase1Report.inventory_dir } else { $null }
        inventory_inputs = $inventoryInputs
    }
    latest_logs = [ordered]@{
        phase2_core = Join-Path $PipelineRoot "phase2-core_summary.json"
        phase2_full = Join-Path $PipelineRoot "phase2-full_summary.json"
        repaired_report = Join-Path $RepairedRoot "phase2_repair_report.json"
        core_check = Join-Path $RepairedRoot "stage2_workspace\\stage2_kain-core_check.log"
        full_build = Join-Path $RepairedRoot "stage2_workspace\\stage2_build.log"
    }
    blocker_bucket_counts = $bucketCounts
    front_blocker = $frontBlocker
    front_errors = $frontErrorSource
    duplicate_type_blockers = $duplicateTypeBlockers
    top_blocker_signatures = $hotspots
    stage2_binary = [ordered]@{
        exists = [bool]$stage2Binary
        path = $stage2Binary
    }
}

$payload | ConvertTo-Json -Depth 8
