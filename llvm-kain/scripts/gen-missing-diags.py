#!/usr/bin/env python3
"""Scan clang source for missing diag:: IDs and generate enum entries."""
import os
import re
import sys

def collect_used_diags(src_dir):
    """Scan all clang source files for diag::XXX references."""
    used = set()
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if not f.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(root, f)
            try:
                with open(path, 'r', errors='ignore') as fh:
                    content = fh.read()
            except:
                continue
            # Match diag::identifier
            for m in re.finditer(r'diag::(\w+)', content):
                name = m.group(1)
                if name not in ('Severity', 'Group', 'kind', 'CLASS_ERROR', 'CLASS_WARNING', 'CLASS_NOTE', 'CLASS_REMARK'):
                    used.add(name)
    return used

def collect_defined_diags(header_path):
    """Get all diag identifiers already defined in DiagnosticIDs.h."""
    defined = set()
    with open(header_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.match(r'\s+(\w+)\s*=\s*DIAG_START', line)
            if m:
                defined.add(m.group(1))
    return defined

def collect_used_builtins(src_dir):
    """Scan for Builtin::BI_xxx references."""
    used = set()
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if not f.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(root, f)
            try:
                with open(path, 'r', errors='ignore') as fh:
                    content = fh.read()
            except:
                continue
            for m in re.finditer(r'Builtin::(BI_\w+)', content):
                used.add(m.group(1))
    return used

def main():
    kain_dir = 'X:/llvm-kain'
    src_dir = os.path.join(kain_dir, 'clang', 'src')
    header_path = os.path.join(kain_dir, 'clang', 'include', 'basic', 'DiagnosticIDs.h')
    
    # Missing diags
    used = collect_used_diags(src_dir)
    defined = collect_defined_diags(header_path)
    missing = sorted(used - defined)
    
    print(f"Used diagnostics: {len(used)}")
    print(f"Defined diagnostics: {len(defined)}")
    print(f"Missing diagnostics: {len(missing)}")
    
    # Generate enum entries starting at next available offset
    # Last entry is at DIAG_START_COMMON + 3635
    offset_base = 3636
    
    for i, name in enumerate(missing):
        print(f"    {name} = DIAG_START_COMMON + {offset_base + i},")
    
    # Missing builtins
    used_builtins = sorted(collect_used_builtins(src_dir))
    print(f"\n# Used builtins: {len(used_builtins)}")
    for b in used_builtins[:30]:
        print(f"  {b}")
    if len(used_builtins) > 30:
        print(f"  ... and {len(used_builtins) - 30} more")

if __name__ == '__main__':
    main()
