#!/usr/bin/env python3
"""Extract all diag:: references from clang src/basic/ files and generate enum entries."""
import re
import os
import subprocess
import tempfile

# First, build and capture ALL diag errors
# Then extract the missing names

# Actually, let me just do a comprehensive scan of diag:: references
cwd = r"X:\llvm-kain"
src_dir = os.path.join(cwd, "clang", "src")

# Find all diag:: references in clang src files
diag_refs = set()
for root, dirs, files in os.walk(src_dir):
    for f in files:
        if f.endswith(('.cpp', '.h')):
            path = os.path.join(root, f)
            with open(path, 'r', errors='ignore') as fh:
                content = fh.read()
                # Find diag::X where X is an identifier (not Severity, Flavor, Group, kind, etc.)
                matches = re.finditer(r'diag::([a-zA-Z_][a-zA-Z0-9_]*)', content)
                for m in matches:
                    name = m.group(1)
                    # Skip known non-diagnostic-ID types/functions
                    if name in ('Severity', 'Flavor', 'Group', 'kind', 'CustomDiagInfo',
                                'getCustomDiagID', 'getDiagInfo', 'getDiagIDForStableID',
                                'getNumberOfCategories', 'getCategoryNameFromID', 'getCategoryIDFromName'):
                        continue
                    diag_refs.add(name)

print(f"Found {len(diag_refs)} unique diag:: references")
for name in sorted(diag_refs):
    print(f"  {name}")
