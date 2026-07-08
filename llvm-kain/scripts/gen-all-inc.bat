@echo off
REM gen-all-inc.bat — Generate .inc files from .td files using llvm-tblgen
REM llvm-kain tablegen pipeline
setlocal enabledelayedexpansion

set "LLVM_TBLGEN=X:\llvm-kain\build\bin\llvm-tblgen.exe"
set "INC_ROOT=X:\llvm-kain\include"
set "TD_ROOT=X:\llvm-kain"

if not exist "%LLVM_TBLGEN%" (
    echo ERROR: llvm-tblgen.exe not found at %LLVM_TBLGEN%
    echo Build it first: cd X:\llvm-kain ^&^& ninja -C build LLVMTableGen
    echo Then link manually with lld-link /wholearchive
    exit /b 1
)

echo === llvm-kain TableGen .inc Generator ===
echo.

REM ============================================================
REM Phase 1: LLVM Core .td files (backends available)
REM ============================================================

REM --- Attributes.inc (gen-attrs backend) ---
echo [1] Generating Attributes.inc...
%LLVM_TBLGEN% -gen-attrs -I %INC_ROOT% -I %INC_ROOT%/core/ir %TD_ROOT%/include/core/ir/Attributes.td -o %INC_ROOT%/core/ir/Attributes_gen.inc
if %ERRORLEVEL% equ 0 (echo     OK) else (echo     FAILED)

REM ============================================================
REM Phase 2: Target .td files (backends NOT YET available)
REM ============================================================
echo.
echo === Target backends NOT YET available ===
echo The following backends need to be fetched from LLVM 19 utils/TableGen/:
echo   - IntrinsicEmitter.cpp    (gen-intrinsic-enums, gen-intrinsic-impl)
echo   - RegisterInfoEmitter.cpp (gen-register-info)
echo   - InstrInfoEmitter.cpp    (gen-instr-info) 
echo   - SubtargetFeatureEmitter.cpp (gen-subtarget)
echo   - AsmWriterEmitter.cpp    (gen-asm-writer)
echo   - AsmMatcherEmitter.cpp   (gen-asm-matcher)
echo   - CallingConvEmitter.cpp  (gen-callingconv)
echo   - DAGISelEmitter.cpp      (gen-dag-isel)
echo   - SearchableTableEmitter.cpp (gen-searchable-tables)
echo   - DisassemblerEmitter.cpp (gen-disassembler)
echo.
echo Plus Common/ dependencies:
echo   - Common/CodeGenTarget.cpp
echo   - Common/CodeGenRegisters.cpp
echo   - Common/CodeGenInstruction.cpp
echo   - Common/CodeGenDAGPatterns.cpp
echo   - Common/GlobalISel/...
echo.
echo Current backend list:
%LLVM_TBLGEN% --help 2>&1 | findstr "gen-"
echo.
echo === Done ===
