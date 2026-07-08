#!/usr/bin/env python3
"""Extract intrinsic names from Intrinsics*.td files and generate IntrinsicEnums.inc."""
import re
import glob
import os

TD_DIR = "X:/llvm-kain/include/core/ir"
OUTPUT = "X:/llvm-kain/include/core/ir/IntrinsicEnums.inc"

# Extract all def int_xxx names from .td files
intrinsic_names = set()
tg_pattern = re.compile(r'^\s*def (int_\w+)\b')

td_files = sorted(glob.glob(os.path.join(TD_DIR, "Intrinsics*.td")))
print(f"Scanning {len(td_files)} .td files...")

for td_file in td_files:
    fname = os.path.basename(td_file)
    with open(td_file, 'r', encoding='utf-8') as f:
        for line in f:
            m = tg_pattern.match(line)
            if m:
                name = m.group(1)
                # Convert int_trap -> trap, int_aarch64_xx -> aarch64_xx
                assert name.startswith("int_")
                intrinsic_names.add(name[4:])  # strip "int_"

print(f"Found {len(intrinsic_names)} intrinsic names")

# Read target-specific includes from main .td
target_includes = set()
with open(os.path.join(TD_DIR, "Intrinsics.td"), 'r') as f:
    for line in f:
        m = re.match(r'include\s+"([^"]+)"', line)
        if m:
            incl = m.group(1)
            if "Intrinsics" in incl:
                target_includes.add(incl)

print(f"Target .td includes: {len(target_includes)}")

# Generate .inc
with open(OUTPUT, 'w') as f:
    f.write("//===- IntrinsicEnums.inc - Generated from Intrinsics.td ------------===//\n")
    f.write("//\n")
    f.write("// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.\n")
    f.write("// See https://llvm.org/LICENSE.txt for license information.\n")
    f.write("// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception\n")
    f.write("//\n")
    f.write("//===----------------------------------------------------------------------===//\n")
    f.write("//\n")
    f.write(f"// Auto-generated from Intrinsics.td + {len(target_includes)} target .td files.\n")
    f.write(f"// Total: {len(intrinsic_names)} intrinsics.\n")
    f.write("//\n")
    f.write("//===----------------------------------------------------------------------===//\n\n")

    f.write("#ifdef GET_INTRINSIC_ENUM_VALUES\n")
    for name in sorted(intrinsic_names):
        f.write(f"  {name},\n")
    f.write("#endif  // GET_INTRINSIC_ENUM_VALUES\n")
    f.write("#undef GET_INTRINSIC_ENUM_VALUES\n")
    f.write("\n")

    f.write("#ifdef GET_INTRINSIC_ANYKIND_ENUMS\n")
    f.write("enum AnyKindVectorConstraint : unsigned {\n")
    f.write("  VC_None = 0,\n")
    f.write("  VC_Vector = 1,\n")
    f.write("  VC_Scalar = 2,\n")
    f.write("};\n")
    f.write("\n")
    f.write("enum AnyKindElementConstraint : unsigned {\n")
    f.write("  EC_None = 0,\n")
    f.write("  EC_Integer = 1,\n")
    f.write("  EC_Float = 2,\n")
    f.write("  EC_Pointer = 3,\n")
    f.write("};\n")
    f.write("#endif  // GET_INTRINSIC_ANYKIND_ENUMS\n")
    f.write("#undef GET_INTRINSIC_ANYKIND_ENUMS\n")

print(f"Wrote {OUTPUT}")
