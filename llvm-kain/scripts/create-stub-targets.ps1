$targets = @(
  "AMDGPU","ARC","ARM","AVR","BPF","CSKY","DirectX",
  "Hexagon","Lanai","LoongArch","M68k","MSP430","Mips",
  "NVPTX","PPC","RISCV","SPIR","Sparc","SystemZ","TCE",
  "VE","WebAssembly","XCore","Xtensa","OSTargets"
)
$dir = "X:\llvm-kain\clang\src\basic\Targets"
foreach ($t in $targets) {
  $cpp = Join-Path $dir "$t.cpp"
  $h = Join-Path $dir "$t.h"
  Set-Content $cpp "// stripped target" -Force
  Set-Content $h "// stripped target" -Force
}
Write-Host "Created stub files"
