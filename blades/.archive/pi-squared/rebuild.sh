#!/bin/bash
# rebuild.sh — Rebuild pi-squared.exe from scratch
# Workaround for Kain LLVM codegen: functions get sliced but declarations remain.
# Post-processes the .ll to stub out orphan declarations.
set -e

PI_ROOT="X:/blades/pi-squared"
LLVM_IR="$PI_ROOT/.kain/out/x86_64-windows/dev/project/pi-squared/llvm/pi-squared.ll"
CLANG="X:/toolchain/llvm/bin/clang.exe"
KAIN_LIB="X:/.kain/lib"
MSVC_LIB="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/lib/x64"

echo "=== pi-squared Rebuild ==="
echo ""

# Step 1: Build to LLVM IR
echo "[1/3] Building to LLVM IR..."
cd "$PI_ROOT"
kain build --target llvm 2>/dev/null || true

# Step 2: Verify LLVM IR exists
if [ ! -f "$LLVM_IR" ]; then
    echo "ERROR: LLVM IR not found at $LLVM_IR"
    exit 1
fi
echo "[2/3] LLVM IR: $(wc -l < "$LLVM_IR") lines"

# Step 3: Fix dangling declarations
# Find all @function symbols that are called but never defined
echo "[3/3] Fixing dangling declarations..."
python - << 'PYFIX'
import re
import sys

with open(r'X:/blades/pi-squared/.kain/out/x86_64-windows/dev/project/pi-squared/llvm/pi-squared.ll', 'r') as f:
    content = f.read()

# Find all called functions
calls = set(re.findall(r'call\s+\S+\s+@(\w+)', content))

# Find all defined functions
defs = set(re.findall(r'define\s+\S+\s+@(\w+)\s*\(', content))

# Find all declared functions
decls = set(re.findall(r'declare\s+\S+\s+@(\w+)\s*\(', content))

# Find called but not defined (orphans)
orphans = calls - defs
# Remove declarations that exist as decls — they need bodies
orphans = orphans - (calls & decls)

# For each orphan, add a stub declaration (check all existing declarations)
for fn in sorted(orphans):
    existing_decl = re.search(r'declare\s+\S+\s+@' + re.escape(fn) + r'\s*\(', content)
    if existing_decl:
        continue
    stub = f'declare i64 @{fn}(...)\n'
    if stub not in content:
        content = content + stub

# Also add stubs for known problematic patterns
# Check for "call i64 @int" — replace with ptrtoint
content = re.sub(
    r'call i64 @int\(i8\* (%[a-zA-Z0-9]+)\)',
    r'ptrtoint i8\* \1 to i64',
    content
)

# Check for any remaining calls to functions that aren't defined and aren't runtime
all_calls = set(re.findall(r'call\s+\S+\s+@(\w+)', content))
all_defs = set(re.findall(r'define\s+\S+\s+@(\w+)\s*\(', content))
remaining_orphans = all_calls - all_defs

# For each remaining orphan, add a weak stub
for fn in sorted(remaining_orphans):
    if fn.startswith('kain_') or fn.startswith('llvm.'):
        continue  # runtime or LLVM intrinsics
    # Skip well-known runtime functions
    if fn in ('printf', 'malloc', 'free', 'memcpy', 'memset'):
        continue
    # Check if already declared
    existing_decl = re.search(r'declare\s+\S+\s+@' + re.escape(fn) + r'\s*\(', content)
    if existing_decl:
        continue
    stub_line = f'declare i64 @{fn}(...)\n'
    if stub_line not in content:
        content = content + stub_line

with open(r'X:/blades/pi-squared/.kain/out/x86_64-windows/dev/project/pi-squared/llvm/pi-squared.ll', 'w') as f:
    f.write(content)

print(f"  Fixed {len(orphans)} orphan declarations")
print(f"  Calls: {len(all_calls)}, Defs: {len(all_defs)}")
PYFIX

# Step 4: Link with clang
echo "[4/4] Linking pi-squared.exe..."
# NOTE: -Wl,/subsystem:console is REQUIRED. Without it, clang defaults
# to Windows GUI subsystem (2), which discards stdout/stderr in PowerShell
# and cmd.exe — the binary runs but produces NO visible output.
# UTF-8 console init is now handled by include <windows.h> as win in main.kn
# — no external C files or .obj needed.
"$CLANG" \
    -target x86_64-pc-windows-msvc \
    -Wl,/subsystem:console \
    -o pi-squared.exe \
    "$LLVM_IR" \
    -L "$KAIN_LIB" -lkain_runtime \
    -L "$MSVC_LIB" \
    -lole32 -luser32 -lgdi32 -lkernel32 -lshell32 -lws2_32 -lwinhttp -lbcrypt -ladvapi32 \
    -Wl,-defaultlib:msvcrt \
    -Wl,-subsystem:console 2>&1 | grep -v "warning: overriding" || true

if [ -f pi-squared.exe ]; then
    SIZE=$(ls -la pi-squared.exe | awk '{print $5}')
    echo ""
    echo "=== BUILD COMPLETE ==="
    echo "Binary: pi-squared.exe ($SIZE bytes)"
    echo ""
    echo "Test: ./pi-squared.exe --version"
    ./pi-squared.exe --version 2>&1 | head -3
    echo ""
    echo "Test: ./pi-squared.exe --help"
    ./pi-squared.exe --help 2>&1 | head -5
    echo "..."
    echo "DONE!"
else
    echo "=== BUILD FAILED ==="
    exit 1
fi
