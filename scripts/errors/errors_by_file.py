#!/usr/bin/env python3
"""
errors_by_file.py — Extract errors from parser.rs, types.rs, codegen_llvm into clean TSV.

Output: scripts/errors/errors_by_file.tsv (next to this script)
"""

import csv, os
from pathlib import Path
from collections import defaultdict

TSV_IN = Path(__file__).parent.parent.parent / "docs-tsv" / "errors_mined_compiler.tsv"
TSV_OUT = Path(__file__).parent / "errors_by_file.tsv"

TARGET_FILES = {
    "parser.rs":      ("core/src/parser.rs", "Parser"),
    "types.rs":       ("core/src/types.rs", "Typechecker"),
    "codegen_llvm":   ("sys-codegen/src/codegen_llvm", "LLVM Codegen"),
}

rows = []
with open(TSV_IN, encoding='utf-8') as f:
    reader = csv.reader(f, delimiter='\t')
    header = next(reader)
    for row in reader:
        if len(row) < 5: continue
        fpath = row[1]
        pat = row[3]
        msg = row[4]
        line = row[2]

        # Skip noise
        if pat in ('KainError::', 'Err(', 'error_corpus_fixture'):
            continue

        # Match to target files
        for key, (substr, category) in TARGET_FILES.items():
            if substr in fpath.replace('\\', '/'):
                rows.append((category, key, line, pat, msg[:120]))
                break

# Write TSV
with open(TSV_OUT, 'w', encoding='utf-8', newline='') as f:
    w = csv.writer(f, delimiter='\t')
    w.writerow(["category", "file", "line", "pattern", "message"])
    for r in sorted(rows, key=lambda x: (x[0], x[1], int(x[2]) if x[2].isdigit() else 0)):
        w.writerow(r)

# Print summary
from collections import Counter
cats = Counter(r[0] for r in rows)
files = Counter(r[1] for r in rows)
pats = Counter(r[3] for r in rows)

print(f"Written: {TSV_OUT}")
print(f"Total:   {len(rows)} error points")
print()
print("By category:")
for c, n in cats.most_common():
    print(f"  {n:4d}  {c}")
print()
print("By pattern:")
for p, n in pats.most_common():
    print(f"  {n:4d}  {p}")
print()
print("By file:")
for f, n in files.most_common():
    print(f"  {n:4d}  {f}")
