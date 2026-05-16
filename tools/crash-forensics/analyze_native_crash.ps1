param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$DumpPath,

    [string]$LlvmPath,
    [string]$FrameReportPath,
    [string]$HostReportPath,
    [string]$OutputPath,
    [int]$MaxStackFrames = 40,
    [int]$AssemblyContextBytes = 96
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

function Resolve-RequiredPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathText,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($PathText)) {
        throw "$Label was empty."
    }
    if (!(Test-Path $PathText)) {
        throw "$Label was not found: $PathText"
    }
    return (Resolve-Path $PathText).Path
}

function Resolve-OptionalPath {
    param([string]$PathText)

    if ([string]::IsNullOrWhiteSpace($PathText)) {
        return $null
    }
    if (!(Test-Path $PathText)) {
        return $null
    }
    return (Resolve-Path $PathText).Path
}

function Resolve-ToolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Candidates,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }
    throw "Unable to resolve $Label. Tried: $($Candidates -join ', ')"
}

function Invoke-CapturedTool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    return @{
        output = ($output | Out-String)
        exit_code = $exitCode
    }
}

function Parse-KeyValueReport {
    param([string]$PathText)

    $resolved = Resolve-OptionalPath $PathText
    if (!$resolved) {
        return $null
    }

    $map = @{}
    foreach ($line in Get-Content -LiteralPath $resolved) {
        if ($line -match '^\s*([^=]+?)\s*=\s*(.*)\s*$') {
            $map[$matches[1]] = $matches[2]
        }
    }
    return @{
        path = $resolved
        values = $map
    }
}

function Get-LastFrameEvidence {
    param(
        [hashtable]$FrameReport,
        [hashtable]$HostReport
    )

    $evidence = @()
    if ($FrameReport) {
        foreach ($key in @("frame_index", "frame.clock", "frames")) {
            if ($FrameReport.values.ContainsKey($key)) {
                $evidence += "$key=$($FrameReport.values[$key])"
            }
        }
    }
    if ($HostReport) {
        foreach ($key in @("frames", "last_error")) {
            if ($HostReport.values.ContainsKey($key)) {
                $evidence += "$key=$($HostReport.values[$key])"
            }
        }
    }
    if ($evidence.Count -eq 0) {
        return "unavailable"
    }
    return ($evidence -join ", ")
}

function Scan-NonEntryAllocas {
    param([string]$PathText)

    $resolved = Resolve-OptionalPath $PathText
    if (!$resolved) {
        return @{
            path = $null
            count = 0
            hits = @()
        }
    }

    $hits = New-Object System.Collections.Generic.List[string]
    $functionName = ""
    $currentBlock = "entry-implicit"
    $lineNumber = 0

    foreach ($rawLine in Get-Content -LiteralPath $resolved) {
        $lineNumber += 1
        $line = [string]$rawLine

        if ($line -match '^\s*define\b.*@("?[^"( ]+"?)\(') {
            $functionName = $matches[1]
            $currentBlock = "entry-implicit"
            continue
        }
        if ($line -match '^\s*}\s*$') {
            $functionName = ""
            $currentBlock = "entry-implicit"
            continue
        }
        if ([string]::IsNullOrWhiteSpace($functionName)) {
            continue
        }
        if ($line -match '^\s*([A-Za-z0-9_.-]+):') {
            $currentBlock = $matches[1]
            continue
        }
        if ($line -match '\balloca\b' -and $currentBlock -ne "entry" -and $currentBlock -ne "entry-implicit") {
            $hits.Add("${functionName}:${currentBlock}:${lineNumber}")
        }
    }

    return @{
        path = $resolved
        count = $hits.Count
        hits = $hits
    }
}

function New-TempFilePath {
    param([string]$Extension)

    $name = [System.Guid]::NewGuid().ToString("N")
    return (Join-Path ([System.IO.Path]::GetTempPath()) "$name$Extension")
}

$resolvedExe = Resolve-RequiredPath -PathText $ExePath -Label "ExePath"
$resolvedDump = Resolve-RequiredPath -PathText $DumpPath -Label "DumpPath"
$resolvedLlvm = Resolve-OptionalPath $LlvmPath

$exeStem = [System.IO.Path]::GetFileNameWithoutExtension($resolvedExe)
$exeModule = [System.IO.Path]::GetFileName($resolvedExe)

if (!$OutputPath) {
    $OutputPath = Join-Path (Join-Path (Split-Path -Parent $resolvedExe) ".kain\forensics") "$exeStem-crash-report.txt"
}
$outputDir = Split-Path -Parent $OutputPath
if ($outputDir -and !(Test-Path $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

if (!$resolvedLlvm) {
    $candidateLlvm = Join-Path (Split-Path -Parent $resolvedExe) ".kain\out\$exeStem\$exeStem.ll"
    if (Test-Path $candidateLlvm) {
        $resolvedLlvm = (Resolve-Path $candidateLlvm).Path
    }
}

$frameReport = Parse-KeyValueReport $FrameReportPath
$hostReport = Parse-KeyValueReport $HostReportPath

$lldbPath = Resolve-ToolPath -Candidates @(
    (Join-Path $repoRoot "toolchain\llvm\bin\lldb.exe"),
    "lldb.exe",
    "lldb"
) -Label "LLDB"

$objdumpPath = Resolve-ToolPath -Candidates @(
    (Join-Path $repoRoot "toolchain\llvm\bin\llvm-objdump.exe"),
    "llvm-objdump.exe",
    "llvm-objdump"
) -Label "llvm-objdump"

$lldbCommandFile = New-TempFilePath ".lldb"
$lldbRawPath = [System.IO.Path]::ChangeExtension($OutputPath, ".lldb.txt")
$objdumpRawPath = [System.IO.Path]::ChangeExtension($OutputPath, ".objdump.txt")

$lldbCommands = @(
    "thread list",
    "register read rip rsp rbp",
    "bt $MaxStackFrames",
    "disassemble --frame",
    "image lookup -a `$pc",
    "quit"
)
Set-Content -LiteralPath $lldbCommandFile -Value ($lldbCommands -join [Environment]::NewLine) -Encoding ASCII

$lldbResult = Invoke-CapturedTool -FilePath $lldbPath -Arguments @(
    $resolvedExe,
    "-c",
    $resolvedDump,
    "-s",
    $lldbCommandFile
)
Set-Content -LiteralPath $lldbRawPath -Value $lldbResult.output -Encoding UTF8

$exceptionCode = ""
$crashAddressHex = ""
$firstAppFrameAddressHex = ""
$crashSymbol = ""
$firstAppFrame = ""
$topFrames = New-Object System.Collections.Generic.List[string]

foreach ($line in ($lldbResult.output -split "`r?`n")) {
    if ([string]::IsNullOrWhiteSpace($exceptionCode) -and $line -match 'Exception (0x[0-9a-fA-F]+)') {
        $exceptionCode = $matches[1]
    }
    if ([string]::IsNullOrWhiteSpace($crashAddressHex) -and $line -match 'address (0x[0-9a-fA-F]+)') {
        $crashAddressHex = $matches[1]
    }
    if ([string]::IsNullOrWhiteSpace($crashAddressHex) -and $line -match '\brip = (0x[0-9a-fA-F]+)') {
        $crashAddressHex = $matches[1]
    }
    if ($line -match '^\s*\*?\s*frame #\d+:') {
        if ($topFrames.Count -lt 12) {
            $topFrames.Add($line.Trim())
        }
        if ([string]::IsNullOrWhiteSpace($firstAppFrame) -and $line.Contains($exeModule)) {
            $firstAppFrame = $line.Trim()
            if ($line -match 'frame #\d+:\s+(0x[0-9a-fA-F]+)') {
                $firstAppFrameAddressHex = $matches[1]
            }
            if ($line -match [regex]::Escape($exeModule) + '[!`](.+?)(?:\s+\+\s+\d+|\s+at\s+|$)') {
                $crashSymbol = $matches[1].Trim()
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($exceptionCode)) {
    $exceptionCode = "unknown"
}
if ([string]::IsNullOrWhiteSpace($crashAddressHex)) {
    $crashAddressHex = "unknown"
}
if ([string]::IsNullOrWhiteSpace($crashSymbol)) {
    $crashSymbol = "unknown"
}
if ([string]::IsNullOrWhiteSpace($firstAppFrame)) {
    $firstAppFrame = "not found in LLDB backtrace"
}

$objdumpHeaderResult = Invoke-CapturedTool -FilePath $objdumpPath -Arguments @("-p", $resolvedExe)
$imageBaseHex = ""
foreach ($line in ($objdumpHeaderResult.output -split "`r?`n")) {
    if ($line -match '^\s*ImageBase\s+([0-9A-Fa-f]+)\s*$') {
        $imageBaseHex = "0x$($matches[1])"
        break
    }
}

$moduleImageOffset = $null
if (![string]::IsNullOrWhiteSpace($firstAppFrameAddressHex)) {
    $lookupResult = Invoke-CapturedTool -FilePath $lldbPath -Arguments @(
        $resolvedExe,
        "-c",
        $resolvedDump,
        "-o",
        "image lookup -a $firstAppFrameAddressHex",
        "-o",
        "quit"
    )
    foreach ($line in ($lookupResult.output -split "`r?`n")) {
        if ($line -match 'module_image \+ ([0-9]+)') {
            $moduleImageOffset = [UInt64]$matches[1]
            break
        }
    }
}

$assemblyProbeAddressHex = $crashAddressHex
$assemblyProbeLabel = "faulting_pc"
if ($moduleImageOffset -ne $null -and ![string]::IsNullOrWhiteSpace($imageBaseHex)) {
    $imageBaseValue = [UInt64]::Parse($imageBaseHex.Substring(2), [System.Globalization.NumberStyles]::HexNumber)
    $assemblyProbeAddressHex = ("0x{0:x}" -f ($imageBaseValue + $moduleImageOffset))
    $assemblyProbeLabel = "first_app_frame_module_image_offset"
} elseif (![string]::IsNullOrWhiteSpace($firstAppFrameAddressHex)) {
    $assemblyProbeAddressHex = $firstAppFrameAddressHex
    $assemblyProbeLabel = "first_app_frame_loaded_address"
}

if ($assemblyProbeAddressHex -ne "unknown") {
    $assemblyProbeAddress = [UInt64]::Parse($assemblyProbeAddressHex.Substring(2), [System.Globalization.NumberStyles]::HexNumber)
    $startAddress = if ($assemblyProbeAddress -gt [uint64]$AssemblyContextBytes) { $assemblyProbeAddress - [uint64]$AssemblyContextBytes } else { 0 }
    $stopAddress = $assemblyProbeAddress + [uint64]$AssemblyContextBytes
    $objdumpResult = Invoke-CapturedTool -FilePath $objdumpPath -Arguments @(
        "--disassemble",
        "--line-numbers",
        ("--start-address=0x{0:x}" -f $startAddress),
        ("--stop-address=0x{0:x}" -f $stopAddress),
        $resolvedExe
    )
    Set-Content -LiteralPath $objdumpRawPath -Value $objdumpResult.output -Encoding UTF8
} else {
    Set-Content -LiteralPath $objdumpRawPath -Value "Crash address unavailable." -Encoding UTF8
}

$allocaSummary = Scan-NonEntryAllocas $resolvedLlvm

$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add("native_crash_forensics_report=1")
$reportLines.Add("exe_path=$resolvedExe")
$reportLines.Add("dump_path=$resolvedDump")
$reportLines.Add("llvm_path=" + ($(if ($allocaSummary.path) { $allocaSummary.path } else { "unavailable" })))
$reportLines.Add("lldb_path=$lldbPath")
$reportLines.Add("objdump_path=$objdumpPath")
$reportLines.Add("exception_code=$exceptionCode")
$reportLines.Add("crash_address=$crashAddressHex")
$reportLines.Add("image_base=" + ($(if ([string]::IsNullOrWhiteSpace($imageBaseHex)) { "unknown" } else { $imageBaseHex })))
$reportLines.Add("module_image_offset=" + ($(if ($moduleImageOffset -eq $null) { "unknown" } else { [string]$moduleImageOffset })))
$reportLines.Add("first_app_frame_address=" + ($(if ([string]::IsNullOrWhiteSpace($firstAppFrameAddressHex)) { "unknown" } else { $firstAppFrameAddressHex })))
$reportLines.Add("assembly_probe_address=$assemblyProbeAddressHex")
$reportLines.Add("assembly_probe_source=$assemblyProbeLabel")
$reportLines.Add("crash_symbol=$crashSymbol")
$reportLines.Add("first_app_frame=$firstAppFrame")
$reportLines.Add("last_frame_evidence=$(Get-LastFrameEvidence -FrameReport $frameReport -HostReport $hostReport)")
$reportLines.Add("non_entry_alloca_count=$($allocaSummary.count)")
$reportLines.Add("lldb_log_path=$lldbRawPath")
$reportLines.Add("objdump_log_path=$objdumpRawPath")
$reportLines.Add("")
$reportLines.Add("[top_frames]")
foreach ($frame in $topFrames) {
    $reportLines.Add($frame)
}
$reportLines.Add("")
$reportLines.Add("[frame_report]")
if ($frameReport) {
    $reportLines.Add("path=$($frameReport.path)")
    foreach ($key in ($frameReport.values.Keys | Sort-Object)) {
        $reportLines.Add("$key=$($frameReport.values[$key])")
    }
} else {
    $reportLines.Add("unavailable")
}
$reportLines.Add("")
$reportLines.Add("[host_report]")
if ($hostReport) {
    $reportLines.Add("path=$($hostReport.path)")
    foreach ($key in ($hostReport.values.Keys | Sort-Object)) {
        $reportLines.Add("$key=$($hostReport.values[$key])")
    }
} else {
    $reportLines.Add("unavailable")
}
$reportLines.Add("")
$reportLines.Add("[non_entry_allocas]")
if ($allocaSummary.count -eq 0) {
    $reportLines.Add("none")
} else {
    foreach ($hit in $allocaSummary.hits) {
        $reportLines.Add($hit)
    }
}

Set-Content -LiteralPath $OutputPath -Value $reportLines -Encoding UTF8
Remove-Item -LiteralPath $lldbCommandFile -ErrorAction SilentlyContinue

Write-Host "[PASS] native crash report: $OutputPath"
Write-Host "[PASS] LLDB log: $lldbRawPath"
Write-Host "[PASS] objdump log: $objdumpRawPath"
