param(
    [switch]$Release,
    [switch]$SkipFabricRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Escape-KainText([string]$Value) {
    if ([string]::IsNullOrEmpty($Value)) {
        return ""
    }

    return $Value.Replace("\", "\\").Replace('"', '\"').Replace("`r", " ").Replace("`n", " ")
}

function Convert-ToForwardSlashPath([string]$Value) {
    return $Value.Replace("\", "/")
}

function Get-StepDurationMs($Step) {
    if ($null -eq $Step.started_unix_ms -or $null -eq $Step.finished_unix_ms) {
        return 0
    }

    return [int64]$Step.finished_unix_ms - [int64]$Step.started_unix_ms
}

function Get-StepOutputNames($Step) {
    if ($null -eq $Step.outputs) {
        return @()
    }

    return @($Step.outputs | ForEach-Object { $_.name })
}

function Get-RelativePathText([string]$FullPath, [string]$RootPath) {
    $resolvedFull = [System.IO.Path]::GetFullPath($FullPath)
    $resolvedRoot = [System.IO.Path]::GetFullPath($RootPath)

    if ($resolvedFull.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $resolvedFull.Substring($resolvedRoot.Length).TrimStart('\', '/')
        if (-not [string]::IsNullOrWhiteSpace($relative)) {
            return (Convert-ToForwardSlashPath $relative)
        }
    }

    return (Convert-ToForwardSlashPath $resolvedFull)
}

function Get-ReportStep($Report, [string]$StepId) {
    return @($Report.step_results | Where-Object { $_.id -eq $StepId } | Select-Object -First 1)[0]
}

function Write-Utf8NoBomFile([string]$Path, [string]$Content) {
    $Encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $Encoding)
}

function Get-ObjectPropertyValue($Object, [string]$PropertyName) {
    if ($null -eq $Object) {
        return $null
    }

    $Property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $Property) {
        return $null
    }

    return $Property.Value
}

function Get-StepOutputSummary($Step, [string]$OutputName) {
    if ($null -eq $Step -or $null -eq $Step.outputs) {
        return ""
    }

    $Output = @($Step.outputs | Where-Object { $_.name -eq $OutputName } | Select-Object -First 1)[0]
    if ($null -eq $Output) {
        return ""
    }

    if ($Output.payload.kind -eq "value") {
        return [string]$Output.payload.value.summary
    }

    if ($Output.payload.kind -eq "shared_buffer") {
        return [string]$Output.payload.buffer.byte_length
    }

    return [string]$Output.payload.kind
}

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $SmokeRoot "..\..\..")).Path
$ManifestRelativePath = "smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml"
$GeneratedRoot = Join-Path $SmokeRoot "generated"
$TemplatePath = Join-Path $SmokeRoot "templates\visual_showcase.template.kn"
$GeneratedSourcePath = Join-Path $GeneratedRoot "main.generated.kn"
$SnapshotPath = Join-Path $GeneratedRoot "visual_snapshot.json"
$NativeAppRoot = Join-Path $SmokeRoot "visual-native-app"
$ExpectedExecutableName = "fabric-studio-3d-editor.exe"
$ReportsRoot = Join-Path $SmokeRoot ".kain\fabric\reports"

New-Item -ItemType Directory -Force -Path $GeneratedRoot | Out-Null

Push-Location $RepoRoot
try {
    if (-not $SkipFabricRun) {
        & cargo run -q -p cli --bin kain -- fabric run --manifest $ManifestRelativePath
        if ($LASTEXITCODE -ne 0) {
            throw "Fabric run failed for $ManifestRelativePath."
        }
    }

    $LatestReportDirectory = Get-ChildItem -Path $ReportsRoot -Directory |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    if ($null -eq $LatestReportDirectory) {
        throw "No Fabric report directories were found under $ReportsRoot."
    }

    $ReportPath = Join-Path $LatestReportDirectory.FullName "report.json"
    $Report = Get-Content -Raw -Path $ReportPath | ConvertFrom-Json

    $SessionId = [string]$Report.session_id
    $SessionStatus = [string]$Report.status
    $DurationMs = [int64]$Report.finished_unix_ms - [int64]$Report.started_unix_ms
    $StepResults = @($Report.step_results)
    $SucceededSteps = @($StepResults | Where-Object { $_.status -eq "succeeded" }).Count
    $TotalSteps = @($StepResults).Count

    $PythonStep = Get-ReportStep $Report "python_source"
    $KainStep = Get-ReportStep $Report "kain_orchestrator"
    $GpuStep = Get-ReportStep $Report "gpu_enrich"
    $NodeStep = Get-ReportStep $Report "node_packager"

    $Summary = Get-StepOutputSummary $NodeStep "summary"
    $KainReport = Get-StepOutputSummary $KainStep "report"
    $GpuByteLength = Get-StepOutputSummary $GpuStep "dst"
    if ([string]::IsNullOrWhiteSpace($GpuByteLength)) {
        $GpuByteLength = "0"
    }

    $GpuSignature = "gpu output unavailable"
    if ($Summary -match "gpu=([^|]+)") {
        $GpuSignature = $Matches[1]
    }

    $Highlight = "#ffd36b"
    $AccentPrimary = "#6dd3ff"
    $AccentSoft = "#b6ecff"
    $SignatureNumbers = @()
    if ($GpuSignature -ne "gpu output unavailable") {
        $SignatureNumbers = @($GpuSignature.Split(",") | ForEach-Object { [int]($_.Trim()) })
    }
    if (@($SignatureNumbers).Count -ge 4) {
        $AccentPrimary = "#{0:x2}{1:x2}{2:x2}" -f (48 + ($SignatureNumbers[0] * 22)), (120 + ($SignatureNumbers[1] * 18)), (180 + ($SignatureNumbers[2] * 12))
        $AccentSoft = "#{0:x2}{1:x2}{2:x2}" -f (138 + ($SignatureNumbers[1] * 12)), (200 + ($SignatureNumbers[2] * 8)), (220 + ($SignatureNumbers[3] * 6))
        $Highlight = "#{0:x2}{1:x2}{2:x2}" -f 255, (160 + ($SignatureNumbers[3] * 10)), (84 + ($SignatureNumbers[0] * 12))
    }

    $RuntimeCountNames = @("python", "kain", "gpu_compute", "node", "rust_crate", "c_abi")
    $RuntimeCountParts = New-Object System.Collections.Generic.List[string]
    foreach ($RuntimeName in $RuntimeCountNames) {
        $RuntimeCountValue = Get-ObjectPropertyValue $Report.validation.runtime_counts $RuntimeName
        if ($null -ne $RuntimeCountValue) {
            $RuntimeCountParts.Add("$RuntimeName=$RuntimeCountValue")
        }
    }
    $RuntimeCounts = [string]::Join(" | ", $RuntimeCountParts)

    $HeroTitle = "All four runtime lanes converged in $DurationMs ms."
    $HeroSubtitle = "Final node proof: $Summary"
    $SummaryTitle = "Node returned the final signed summary."
    $SummaryCaption = $Summary

    $RelativeReportPath = Get-RelativePathText $ReportPath $RepoRoot
    $RelativeEventsPath = Get-RelativePathText $Report.events_path $RepoRoot
    $RelativeLockPath = Get-RelativePathText $Report.lock_path $RepoRoot
    $RelativeWorkspaceRoot = Get-RelativePathText (Join-Path $SmokeRoot ".") $RepoRoot
    $RelativeSnapshotPath = Get-RelativePathText $SnapshotPath $RepoRoot
    $RelativeGeneratedSourcePath = Get-RelativePathText $GeneratedSourcePath $RepoRoot
    $RelativeNativeAppRoot = Get-RelativePathText $NativeAppRoot $RepoRoot
    $RelativeReportDirectory = Get-RelativePathText $LatestReportDirectory.FullName $RepoRoot

    $StepCards = New-Object System.Collections.Generic.List[string]
    foreach ($Step in $StepResults) {
        $OutputNames = Get-StepOutputNames $Step
        $OutputText = if (@($OutputNames).Count -gt 0) {
            [string]::Join(", ", $OutputNames)
        }
        else {
            "none"
        }

        $StepCards.Add(@"
            <panel scope="mission" variant="card" title="$(Escape-KainText $Step.id)">
                <text role="eyebrow">$(Escape-KainText ([string]$Step.runtime).ToUpperInvariant())</text>
                <text role="metric">$(Escape-KainText ([string]$Step.status).ToUpperInvariant())</text>
                <text role="caption">$(Get-StepDurationMs $Step) ms</text>
                <text role="caption">Outputs: $(Escape-KainText $OutputText)</text>
            </panel>
"@.TrimEnd())
    }

    $TemplateContent = Get-Content -Raw -Path $TemplatePath
    $TokenMap = [ordered]@{
        "__ACCENT_PRIMARY__" = $AccentPrimary
        "__ACCENT_SOFT__" = $AccentSoft
        "__HIGHLIGHT__" = $Highlight
        "__SESSION_ID__" = Escape-KainText $SessionId
        "__SESSION_STATUS__" = Escape-KainText $SessionStatus.ToUpperInvariant()
        "__RUNTIME_COUNTS__" = Escape-KainText $RuntimeCounts
        "__HERO_TITLE__" = Escape-KainText $HeroTitle
        "__HERO_SUBTITLE__" = Escape-KainText $HeroSubtitle
        "__STEP_SUCCESS__" = "$SucceededSteps/$TotalSteps"
        "__DURATION_MS__" = [string]$DurationMs
        "__GPU_BYTE_LENGTH__" = [string]$GpuByteLength
        "__SUMMARY_TITLE__" = Escape-KainText $SummaryTitle
        "__SUMMARY_CAPTION__" = Escape-KainText $SummaryCaption
        "__SUMMARY_INLINE__" = Escape-KainText ($SummaryCaption.Replace("|", " | "))
        "__GPU_SIGNATURE__" = Escape-KainText $GpuSignature
        "__KAIN_REPORT__" = Escape-KainText $KainReport
        "__REPORT_FILE__" = Escape-KainText $RelativeReportPath
        "__EVENTS_FILE__" = Escape-KainText $RelativeEventsPath
        "__LOCK_FILE__" = Escape-KainText $RelativeLockPath
        "__WORKSPACE_ROOT__" = Escape-KainText $RelativeWorkspaceRoot
        "__SNAPSHOT_FILE__" = Escape-KainText $RelativeSnapshotPath
        "__GENERATED_SOURCE__" = Escape-KainText $RelativeGeneratedSourcePath
        "__EXECUTABLE_HINT__" = Escape-KainText $RelativeNativeAppRoot
        "__REPORT_DIRECTORY__" = Escape-KainText $RelativeReportDirectory
        "__STEP_CARDS__" = [string]::Join("`n", $StepCards)
    }

    foreach ($Token in $TokenMap.Keys) {
        $TemplateContent = $TemplateContent.Replace($Token, $TokenMap[$Token])
    }

    Write-Utf8NoBomFile -Path $GeneratedSourcePath -Content $TemplateContent

    $VisualSnapshot = [ordered]@{
        session_id = $SessionId
        status = $SessionStatus
        summary = $Summary
        duration_ms = $DurationMs
        gpu_signature = $GpuSignature
        gpu_byte_length = [int]$GpuByteLength
        runtime_counts = $RuntimeCountParts
        report_path = $RelativeReportPath
        events_path = $RelativeEventsPath
        lock_path = $RelativeLockPath
        generated_source = $RelativeGeneratedSourcePath
        visual_native_app_root = $RelativeNativeAppRoot
        steps = @(
            $StepResults | ForEach-Object {
                [ordered]@{
                    id = $_.id
                    runtime = $_.runtime
                    status = $_.status
                    duration_ms = Get-StepDurationMs $_
                    outputs = Get-StepOutputNames $_
                }
            }
        )
    }
    Write-Utf8NoBomFile -Path $SnapshotPath -Content ($VisualSnapshot | ConvertTo-Json -Depth 8)

    $BuildArguments = @(
        "run",
        "-q",
        "-p",
        "cli",
        "--bin",
        "kain",
        "--",
        "build",
        "native-ui",
        "smoketest/fabric/gpu_compute_convergence/generated/main.generated.kn",
        "--app-name",
        "fabric_studio_3d_editor",
        "--window-title",
        "Fabric Studio 3D Editor",
        "-o",
        "smoketest/fabric/gpu_compute_convergence/visual-native-app"
    )
    if ($Release) {
        $BuildArguments += "--release"
    }

    & cargo @BuildArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Native UI build failed for the Fabric visual showcase."
    }

    $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter $ExpectedExecutableName -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $ExecutablePath) {
        $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter *.exe -File |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
    }
    if ($null -eq $ExecutablePath) {
        throw "The native UI build finished without producing an executable in $NativeAppRoot."
    }

    Write-Host "Fabric visual executable built successfully."
    Write-Host "  Session: $SessionId"
    Write-Host "  Summary: $Summary"
    Write-Host "  Generated source: $RelativeGeneratedSourcePath"
    Write-Host "  Executable: $(Get-RelativePathText $ExecutablePath.FullName $RepoRoot)"
    Write-Host "  Snapshot: $RelativeSnapshotPath"
}
finally {
    Pop-Location
}
