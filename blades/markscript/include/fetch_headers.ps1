$headers = @(
    'Orc.h','OrcEE.h','LLJIT.h','LLJITUtils.h',
    'Core.h','Error.h','ErrorHandling.h',
    'TargetMachine.h','Target.h','Types.h',
    'Support.h','ExecutionEngine.h','ExternC.h',
    'DataTypes.h','Visibility.h','Comdat.h',
    'Linker.h','Object.h','BitReader.h','BitWriter.h',
    'Analysis.h','IRReader.h','Disassembler.h','DisassemblerTypes.h',
    'Deprecated.h','blake3.h'
)
$base = 'https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-22.1.6/llvm/include/llvm-c/'
$out = 'X:\blades\markscript\include\llvm-c\'
foreach ($h in $headers) {
    $url = $base + $h
    $path = $out + $h
    Write-Host "Fetching $h..."
    try {
        Invoke-WebRequest -Uri $url -OutFile $path -ErrorAction Stop
        $len = (Get-Item $path).Length
        Write-Host "  OK $len bytes" -ForegroundColor Green
    } catch {
        Write-Host "  FAIL $h" -ForegroundColor Red
    }
}
Write-Host "DONE - all headers fetched"
