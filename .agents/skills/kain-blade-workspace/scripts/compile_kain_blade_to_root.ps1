param(
    [Parameter(Mandatory = $true)]
    [string]$Entry,

    [string]$OutputName,
    [string]$KainBin = $env:KAIN_BIN,
    [ValidateSet("bazel", "cargo", "auto")]
    [string]$CompilerBuild = "bazel",
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [switch]$Run,
    [switch]$VerifyLlvm
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..\..")

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    if ($CompilerBuild -eq "bazel" -or $CompilerBuild -eq "auto") {
        $bazelCommand = Get-Command bazel -ErrorAction SilentlyContinue
        if ($bazelCommand) {
            Push-Location $repoRoot
            try {
                & $bazelCommand.Source build "//:kain" "--config=$BazelConfig"
                if ($LASTEXITCODE -ne 0) {
                    if ($CompilerBuild -eq "bazel") {
                        throw "bazel build //:kain --config=$BazelConfig failed with exit code $LASTEXITCODE"
                    }
                } else {
                    $bazelBinText = & $bazelCommand.Source info bazel-bin "--config=$BazelConfig"
                    if ($LASTEXITCODE -ne 0) {
                        throw "bazel info bazel-bin --config=$BazelConfig failed with exit code $LASTEXITCODE"
                    }

                    $ansiEscape = [char]27
                    $bazelBin = $bazelBinText |
                        ForEach-Object { ($_ -replace "$ansiEscape\[[0-9;]*m", "").Trim() } |
                        Where-Object { $_ -and ($_ -match "[:/\\]") } |
                        Select-Object -Last 1

                    if (!$bazelBin) {
                        throw "Unable to resolve bazel-bin from bazel info output."
                    }

                    $bazelKain = Join-Path $bazelBin "crates\cli\kain.exe"
                    if (Test-Path $bazelKain) {
                        return (Resolve-Path $bazelKain).Path
                    }

                    $aliasKain = Join-Path $bazelBin "kain.exe"
                    if (Test-Path $aliasKain) {
                        return (Resolve-Path $aliasKain).Path
                    }

                    throw "Bazel build completed but no kain.exe artifact was found under $bazelBin"
                }
            }
            finally {
                Pop-Location
            }
        } elseif ($CompilerBuild -eq "bazel") {
            throw "Bazel is required by -CompilerBuild bazel, but no bazel command was found on PATH."
        }
    }

    $candidates = @(
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    if ($CompilerBuild -eq "bazel") {
        throw "Bazel did not produce kain.exe and -CompilerBuild bazel forbids Cargo fallback."
    }

    Push-Location $repoRoot
    try {
        & cargo build -p cli --bin kain --bin kn
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build -p cli failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    $built = Join-Path $repoRoot "target\debug\kain.exe"
    if (!(Test-Path $built)) {
        throw "Expected built Kain binary was not found: $built"
    }

    return (Resolve-Path $built).Path
}

$entryPath = if ([System.IO.Path]::IsPathRooted($Entry)) {
    Resolve-Path $Entry
} else {
    Resolve-Path (Join-Path $repoRoot $Entry)
}

if (!$OutputName) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension((Split-Path -Parent (Split-Path -Parent $entryPath.Path)))
    if (!$baseName) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($entryPath.Path)
    }
    $OutputName = "$baseName.exe"
}

if (![System.IO.Path]::GetExtension($OutputName)) {
    $OutputName = "$OutputName.exe"
}

$outputExe = if ([System.IO.Path]::IsPathRooted($OutputName)) {
    $OutputName
} else {
    Join-Path $repoRoot $OutputName
}
$outputDir = Split-Path -Parent $outputExe
if ($outputDir -and !(Test-Path $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

$resolvedKain = Resolve-KainBinary -Requested $KainBin

Push-Location $repoRoot
try {
    & $resolvedKain check $entryPath.Path --target llvm
    if ($LASTEXITCODE -ne 0) {
        throw "kain check failed with exit code $LASTEXITCODE"
    }

    & $resolvedKain $entryPath.Path -t llvm -o $outputExe
    if ($LASTEXITCODE -ne 0) {
        throw "native LLVM compile failed with exit code $LASTEXITCODE"
    }

    if (!(Test-Path $outputExe)) {
        throw "Expected root executable was not created: $outputExe"
    }

    $llvmPath = [System.IO.Path]::ChangeExtension($outputExe, ".ll")
    if ($VerifyLlvm -and (Test-Path $llvmPath)) {
        $llvmAs = Join-Path $repoRoot "toolchain\llvm\bin\llvm-as.exe"
        $bcPath = [System.IO.Path]::ChangeExtension($outputExe, ".bc")
        & $llvmAs $llvmPath -o $bcPath
        if ($LASTEXITCODE -ne 0) {
            throw "llvm-as verification failed with exit code $LASTEXITCODE"
        }
    }

    if ($Run) {
        & $outputExe
        if ($LASTEXITCODE -ne 0) {
            throw "Executable exited with code $LASTEXITCODE"
        }
    }

    Write-Host "[PASS] Kain entry compiled to root executable: $outputExe"
}
finally {
    Pop-Location
}
