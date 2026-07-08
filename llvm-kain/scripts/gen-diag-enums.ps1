$srcDir = "X:\llvm-kain\clang\src"
$diags = @{}

# Scan all clang src files for diag::name references
Get-ChildItem $srcDir -Recurse -Filter *.cpp | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $matches = [regex]::Matches($content, 'diag::([a-zA-Z_][a-zA-Z0-9_]*)')
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        # Skip non-diagnostic-ID names
        if ($name -in @('Severity','Flavor','Group','kind','CustomDiagInfo','getCustomDiagID','getDiagInfo','getDiagIDForStableID','getNumberOfCategories','getCategoryNameFromID','getCategoryIDFromName','getWarningOptionForDiag','getWarningOptionForGroup','getGroupForWarningOption','getDiagnosticFlags','getNearestOption','isUnrecoverable','IgnoreAll','isARCDiagnostic','NUM_BUILTIN_COMMON_DIAGNOSTICS','NUM_BUILTIN_DRIVER_DIAGNOSTICS','NUM_BUILTIN_FRONTEND_DIAGNOSTICS','NUM_BUILTIN_SERIALIZATION_DIAGNOSTICS','NUM_BUILTIN_LEX_DIAGNOSTICS','NUM_BUILTIN_PARSE_DIAGNOSTICS','NUM_BUILTIN_AST_DIAGNOSTICS','NUM_BUILTIN_COMMENT_DIAGNOSTICS','NUM_BUILTIN_CROSSTU_DIAGNOSTICS','NUM_BUILTIN_SEMA_DIAGNOSTICS','NUM_BUILTIN_ANALYSIS_DIAGNOSTICS','NUM_BUILTIN_REFACTORING_DIAGNOSTICS','NUM_BUILTIN_INSTALLAPI_DIAGNOSTICS','NUM_BUILTIN_TRAP_DIAGNOSTICS')) {
            return
        }
        $diags[$name] = $true
    }
}

# Add other commonly referenced diag IDs
$additional = @'
err_cannot_open_file,err_file_too_large,err_file_modified,err_unsupported_bom
err_sloc_space_too_large,note_total_sloc_usage,note_file_sloc_usage,note_file_misc_sloc_usage
warn_stack_exhausted,warn_c11_keyword,warn_c99_keyword,warn_cxx11_keyword,warn_cxx20_keyword,warn_c23_keyword
err_opencl_feature_requires,err_opencl_extension_and_feature_differs
'@ -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
foreach ($a in $additional) {
    $diags[$a] = $true
}

# Header for DiagnosticIDs.h
$sorted = $diags.Keys | Sort-Object
Write-Host "// Auto-generated diagnostic IDs ($($sorted.Count) total)"
Write-Host "enum {"
$i = 200  # Offset after manually maintained entries
foreach ($name in $sorted) {
    Write-Host "    $name = DIAG_START_COMMON + $i,"
    $i++
}
Write-Host "};"
