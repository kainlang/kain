#!/usr/bin/env python3
"""Generate ALL missing diag entries and append to DiagnosticIDs.h."""
import os, re, sys

KAIN = 'X:/llvm-kain'
SRC = os.path.join(KAIN, 'clang', 'src')
HDR = os.path.join(KAIN, 'clang', 'include', 'basic', 'DiagnosticIDs.h')

# Collect used diag:: identifiers
used = set()
for root, dirs, files in os.walk(SRC):
    for f in files:
        if not f.endswith(('.cpp', '.h')): continue
        try:
            with open(os.path.join(root, f), 'r', errors='replace') as fh:
                content = fh.read()
        except: continue
        for m in re.finditer(r'diag::(\w+)', content):
            n = m.group(1)
            if n not in ('Severity','Group','kind','CLASS_ERROR','CLASS_WARNING','CLASS_NOTE','CLASS_REMARK'):
                used.add(n)

# Collect already defined
defined = set()
with open(HDR, 'r', encoding='utf-8', errors='replace') as f:
    for line in f:
        m = re.match(r'\s+(\w+)\s*=\s*DIAG_START', line)
        if m: defined.add(m.group(1))

missing = sorted(used - defined)
print(f"Missing diag entries: {len(missing)}")

# Find last offset
last_offset = 3635
with open(HDR, 'r', encoding='utf-8', errors='replace') as f:
    for line in f:
        m = re.search(r'DIAG_START_COMMON \+ (\d+)', line)
        if m:
            off = int(m.group(1))
            if off > last_offset: last_offset = off
print(f"Last offset found: {last_offset}")

# Generate and append entries
entries = []
for i, name in enumerate(missing):
    entries.append(f"    {name} = DIAG_START_COMMON + {last_offset + 1 + i},")

entries_str = '\n'.join(entries) + '\n'

# Find insertion point: before the closing "};" of the enum
with open(HDR, 'r', encoding='utf-8', errors='replace') as f:
    text = f.read()

# Insert before the last "};" that's at start of line
# Find the "};" that closes the enum (after all entries)
insert_pos = text.rfind('\n};')
if insert_pos > 0:
    new_text = text[:insert_pos] + '\n' + entries_str + text[insert_pos:]
    with open(HDR, 'w', encoding='utf-8') as f:
        f.write(new_text)
    print(f"Appended {len(missing)} missing entries to DiagnosticIDs.h")
else:
    print("ERROR: Could not find closing '};'")
    sys.exit(1)
