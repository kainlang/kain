param(
    [string]$RepoRoot = "M:\Code\Kain",
    [string[]]$Subcommands = @(
        "",
        "build",
        "run",
        "doctor",
        "import-c",
        "import-rust",
        "import-ts",
        "import-asm",
        "gpu-artifacts",
        "inject",
        "selfhost",
        "omni"
    )
)

$binary = Join-Path $RepoRoot "target\debug\kain.exe"

Push-Location $RepoRoot
try {
    if (-not (Test-Path $binary)) {
        Write-Host "Building debug kain binary in $RepoRoot"
        cargo build -p cli
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build failed"
        }
    }

    foreach ($sub in $Subcommands) {
        if ([string]::IsNullOrWhiteSpace($sub)) {
            $label = "kain"
            $args = @("--help")
        } else {
            $label = "kain $sub"
            $args = $sub.Split(" ") + "--help"
        }

        Write-Host ""
        Write-Host ("=" * 78)
        Write-Host "$label --help"
        Write-Host ("=" * 78)
        & $binary @args
        if ($LASTEXITCODE -ne 0) {
            throw "$label --help failed"
        }
    }
}
finally {
    Pop-Location
}
