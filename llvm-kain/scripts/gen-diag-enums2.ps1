$srcDir = "X:\llvm-kain\clang\src"
$diags = @{}

Get-ChildItem $srcDir -Recurse -Filter *.cpp | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $matches = [regex]::Matches($content, 'diag::([a-zA-Z_][a-zA-Z0-9_]*)')
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        $skipList = @('Severity','Flavor','Group','kind','CustomDiagInfo','getCustomDiagID','getDiagInfo',
                      'getDiagIDForStableID','getNumberOfCategories','getCategoryNameFromID',
                      'getCategoryIDFromName','getWarningOptionForDiag','getWarningOptionForGroup',
                      'getGroupForWarningOption','getDiagnosticFlags','getNearestOption',
                      'isUnrecoverable','IgnoreAll','isARCDiagnostic',
                      'NUM_BUILTIN_COMMON_DIAGNOSTICS','NUM_BUILTIN_DRIVER_DIAGNOSTICS',
                      'NUM_BUILTIN_FRONTEND_DIAGNOSTICS','NUM_BUILTIN_SERIALIZATION_DIAGNOSTICS',
                      'NUM_BUILTIN_LEX_DIAGNOSTICS','NUM_BUILTIN_PARSE_DIAGNOSTICS','NUM_BUILTIN_AST_DIAGNOSTICS',
                      'NUM_BUILTIN_COMMENT_DIAGNOSTICS','NUM_BUILTIN_CROSSTU_DIAGNOSTICS',
                      'NUM_BUILTIN_SEMA_DIAGNOSTICS','NUM_BUILTIN_ANALYSIS_DIAGNOSTICS',
                      'NUM_BUILTIN_REFACTORING_DIAGNOSTICS','NUM_BUILTIN_INSTALLAPI_DIAGNOSTICS',
                      'NUM_BUILTIN_TRAP_DIAGNOSTICS')
        if ($skipList -contains $name) { return }
        $diags[$name] = $true
    }
}

# Also scan include/basic for diagnostic references
Get-ChildItem "X:\llvm-kain\clang\include\basic" -Filter *.h | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $matches = [regex]::Matches($content, 'diag::([a-zA-Z_][a-zA-Z0-9_]*)')
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        if ($name -notin @('Severity','Flavor','Group','kind','CustomDiagInfo')) {
            $diags[$name] = $true
        }
    }
}

$sorted = $diags.Keys | Sort-Object
$outLines = @()
$outLines += "  // Auto-generated diagnostic IDs ($($sorted.Count) total)"
$outLines += "  enum {"
$i = 200  # Offset after manually maintained entries
foreach ($name in $sorted) {
    $outLines += "    $name = DIAG_START_COMMON + $i,"
    $i++
}
$outLines += "  };"

[System.IO.File]::WriteAllLines("X:\llvm-kain\scripts\diag-enums.txt", $outLines)
Write-Host "Generated $($sorted.Count) entries to scripts\diag-enums.txt"
