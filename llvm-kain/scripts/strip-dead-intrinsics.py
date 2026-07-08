#!/usr/bin/env python3
"""Remove dead-target intrinsic references from ConstantFolding.cpp."""
import re

path = "X:/llvm-kain/src/core/analysis/ConstantFolding.cpp"
with open(path, 'r') as f:
    lines = f.readlines()

# Target prefixes to keep
keep_prefixes = ('x86', 'aarch64')

# Target prefixes to remove (dead targets deleted in Phase 1/3)
dead_prefixes = (
    'amdgcn', 'nvvm', 'arm_', 'arm_mve', 'arm_neon', 'arm_sve',
    'webassembly', 'wasm', 'hexagon', 'ppc', 'powerpc', 'mips',
    'sparc', 'riscv', 'systemz', 'lanai', 'avr', 'msp430',
    'xtensa', 'xcore', 'arc', 'csky', 'directx', 'dx_', 'hlsl',
    'spirv', 'bpf', 've_', 'loongarch', 'm68k',
    'aarch64_sve', 'aarch64_neon', 'aarch64_sme',  # Keep aarch64_generic only
)

removed = 0
new_lines = []
skip_block = False
block_depth = 0

i = 0
while i < len(lines):
    line = lines[i]
    
    # Check if this line starts a dead intrinsic case
    stripped = line.strip()
    
    # Check for dead intrinsic references
    is_dead = False
    for prefix in dead_prefixes:
        if f'Intrinsic::{prefix}' in stripped:
            is_dead = True
            break
    
    if is_dead:
        # Skip this case and its body until next case or end of switch
        removed += 1
        i += 1
        # Skip the case body (lines that are more indented than the case)
        while i < len(lines):
            next_line = lines[i]
            if next_line.strip().startswith('case Intrinsic::') or next_line.strip() == '}':
                break
            if next_line.strip().startswith('default:'):
                break
            # Count braces to handle nested blocks
            i += 1
        continue
    
    new_lines.append(line)
    i += 1

with open(path, 'w') as f:
    f.writelines(new_lines)

print(f"Removed {removed} dead intrinsic references from ConstantFolding.cpp")
print(f"Lines before: {len(lines)}, after: {len(new_lines)}")
