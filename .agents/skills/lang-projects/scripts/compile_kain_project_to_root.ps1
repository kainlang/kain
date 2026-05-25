param(
    [Parameter(Mandatory = $true)]
    [string]$Entry,

    [string]$OutputName,
    [string]$KainBin = $env:KAIN_BIN,
    [ValidateSet("bazel", "cargo", "auto")]
    [string]$CompilerBuild = "bazel",
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [ValidateSet("project-root", "repo-root")]
    [string]$OutputPlacement = "project-root",
    [string]$ArtifactRoot,
    [string]$RuntimeManifestPath,
    [switch]$Run,
    [switch]$VerifyLlvm
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Normalize-PathValue {
    param([string]$PathValue)

    if (!$PathValue) {
        return $PathValue
    }

    $providerPrefix = "Microsoft.PowerShell.Core\FileSystem::"
    if ($PathValue.StartsWith($providerPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return Normalize-PathValue $PathValue.Substring($providerPrefix.Length)
    }

    if ($PathValue.StartsWith("\\?\UNC\")) {
        return "\" + $PathValue.Substring(7)
    }

    if ($PathValue.StartsWith("\\?\")) {
        return $PathValue.Substring(4)
    }

    return $PathValue
}

function Resolve-NormalizedPath {
    param([string]$PathValue)

    return (Normalize-PathValue ((Resolve-Path $PathValue).Path))
}

$scriptPath = Normalize-PathValue $MyInvocation.MyCommand.Path
$scriptDir = Split-Path -Parent $scriptPath
$repoRoot = Resolve-NormalizedPath (Join-Path $scriptDir "..\..\..\..")

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-NormalizedPath $Requested)
    }

    $localCandidates = @(
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )

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

    foreach ($candidate in $localCandidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    if ($CompilerBuild -eq "bazel" -or $CompilerBuild -eq "auto") {
        throw "Bazel did not produce kain.exe. This helper now treats Bazel as canonical; use -CompilerBuild cargo only for an explicit local Rust override."
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

    return (Resolve-NormalizedPath $built)
}

function Find-ProjectRoot {
    param([string]$StartDir)

    $current = Resolve-NormalizedPath $StartDir
    $repo = Resolve-NormalizedPath $repoRoot

    while ($current -and $current.StartsWith($repo, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (
            (Test-Path (Join-Path $current "build.kn")) -or
            (Test-Path (Join-Path $current "platform.kn")) -or
            (Test-Path (Join-Path $current "KAIN.toml")) -or
            (Test-Path (Join-Path $current "kain.toml"))
        ) {
            return $current
        }

        $parent = Split-Path -Parent $current
        if (!$parent -or $parent -eq $current) {
            break
        }
        $current = $parent
    }

    return (Split-Path -Parent (Split-Path -Parent (Resolve-NormalizedPath $StartDir)))
}

function Move-GeneratedBuildSidecars {
    param(
        [string]$OutputExe,
        [string]$ArtifactDir
    )

    if (!(Test-Path $ArtifactDir)) {
        New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
    }

    $sidecars = @(
        [System.IO.Path]::ChangeExtension($OutputExe, ".ll"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".bc"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".ilk"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".pdb"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".lib"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".exp"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".runtime_contract.json"),
        [System.IO.Path]::ChangeExtension($OutputExe, ".realtime_app.json")
    )

    foreach ($sidecar in $sidecars) {
        if (Test-Path $sidecar) {
            $destination = Join-Path $ArtifactDir ([System.IO.Path]::GetFileName($sidecar))
            if ((Resolve-NormalizedPath $sidecar) -ne (Normalize-PathValue $destination)) {
                Move-Item -LiteralPath $sidecar -Destination $destination -Force
            }
        }
    }
}

$entryPath = if ([System.IO.Path]::IsPathRooted($Entry)) {
    Resolve-NormalizedPath $Entry
} else {
    Resolve-NormalizedPath (Join-Path $repoRoot $Entry)
}

if ($OutputName) {
    $OutputName = Normalize-PathValue $OutputName
}
if ($KainBin) {
    $KainBin = Normalize-PathValue $KainBin
}
if ($ArtifactRoot) {
    $ArtifactRoot = Normalize-PathValue $ArtifactRoot
}
if ($RuntimeManifestPath) {
    $RuntimeManifestPath = Normalize-PathValue $RuntimeManifestPath
}

if (!$OutputName) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension((Split-Path -Parent (Split-Path -Parent $entryPath)))
    if (!$baseName) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($entryPath)
    }
    $OutputName = "$baseName.exe"
}

if (![System.IO.Path]::GetExtension($OutputName)) {
    $OutputName = "$OutputName.exe"
}

$projectRoot = Find-ProjectRoot -StartDir (Split-Path -Parent $entryPath)
$artifactRootPath = if ($ArtifactRoot) {
    if ([System.IO.Path]::IsPathRooted($ArtifactRoot)) {
        $ArtifactRoot
    } else {
        Join-Path $projectRoot $ArtifactRoot
    }
} else {
    Join-Path $projectRoot ".kain\out"
}

$outputStem = [System.IO.Path]::GetFileNameWithoutExtension($OutputName)
$artifactDir = Join-Path $artifactRootPath $outputStem

$outputExe = if ([System.IO.Path]::IsPathRooted($OutputName)) {
    $OutputName
} elseif ($OutputPlacement -eq "repo-root") {
    Join-Path $repoRoot $OutputName
} else {
    Join-Path $projectRoot $OutputName
}
$outputDir = Split-Path -Parent $outputExe
if ($outputDir -and !(Test-Path $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
if (!(Test-Path $artifactDir)) {
    New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
}

$resolvedKain = Resolve-KainBinary -Requested $KainBin
$runtimeManifestForBuild = if ($RuntimeManifestPath) {
    if ([System.IO.Path]::IsPathRooted($RuntimeManifestPath)) {
        Resolve-NormalizedPath $RuntimeManifestPath
    } else {
        Resolve-NormalizedPath (Join-Path $repoRoot $RuntimeManifestPath)
    }
} else {
    Resolve-NormalizedPath (Join-Path $repoRoot "runtime\native_core_runtime.toml")
}
$previousRuntimeManifestPath = $env:KAIN_RUNTIME_MANIFEST_PATH
$env:KAIN_RUNTIME_MANIFEST_PATH = $runtimeManifestForBuild

Push-Location $projectRoot
try {
    & $resolvedKain check $entryPath --target llvm
    if ($LASTEXITCODE -ne 0) {
        throw "kain check failed with exit code $LASTEXITCODE"
    }

    & $resolvedKain $entryPath -t llvm -o $outputExe
    if ($LASTEXITCODE -ne 0) {
        throw "native LLVM compile failed with exit code $LASTEXITCODE"
    }

    if (!(Test-Path $outputExe)) {
        throw "Expected Kain project executable was not created: $outputExe"
    }

    $llvmPath = [System.IO.Path]::ChangeExtension($outputExe, ".ll")
    if ($VerifyLlvm -and (Test-Path $llvmPath)) {
        $llvmAs = Join-Path $repoRoot "toolchain\llvm\bin\llvm-as.exe"
        $bcPath = Join-Path $artifactDir ([System.IO.Path]::GetFileName([System.IO.Path]::ChangeExtension($outputExe, ".bc")))
        & $llvmAs $llvmPath -o $bcPath
        if ($LASTEXITCODE -ne 0) {
            throw "llvm-as verification failed with exit code $LASTEXITCODE"
        }
    }

    Move-GeneratedBuildSidecars -OutputExe $outputExe -ArtifactDir $artifactDir

    if ($Run) {
        & $outputExe
        if ($LASTEXITCODE -ne 0) {
            throw "Executable exited with code $LASTEXITCODE"
        }
    }

    Write-Host "[PASS] Kain entry compiled to project executable: $outputExe"
    Write-Host "[PASS] Local build artifacts: $artifactDir"
}
finally {
    Pop-Location
    if ($null -ne $previousRuntimeManifestPath) {
        $env:KAIN_RUNTIME_MANIFEST_PATH = $previousRuntimeManifestPath
    } else {
        Remove-Item Env:\KAIN_RUNTIME_MANIFEST_PATH -ErrorAction SilentlyContinue
    }
}
