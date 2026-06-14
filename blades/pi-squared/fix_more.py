#!/usr/bin/env python3
"""Fix remaining escape sequences that the first pass missed."""
import os

BS = bytes([0x5C])  # backslash
BS_N = bytes([0x5C, 0x6E])

def fix_file(path, patterns):
    with open(path, 'rb') as f:
        data = f.read()
    changed = False
    for old, new in patterns:
        if old in data:
            data = data.replace(old, new)
            changed = True
            print(f"  Fixed in {path}")
    if changed:
        with open(path, 'wb') as f:
            f.write(data)

# session/tree.kn
fix_file('src/session/tree.kn', [
    (b'TextBlock("[Compaction Summary]"' + BS_N + b' + summary)',
     b'TextBlock("[Compaction Summary]" + NL + summary)'),
])

# tui/components/box.kn - lines with Unicode box-drawing chars + \n
fix_file('src/tui/components/box.kn', [
    (b'"\\u250c" + fmt_repeat("\\u2500", inner) + "\\u2510"' + BS_N + b'"',
     b'"\\u250c" + fmt_repeat("\\u2500", inner) + "\\u2510" + NL'),
    (b'"\\u2502" + fmt_repeat(_self.bg, inner) + "\\u2502"' + BS_N + b'"',
     b'"\\u2502" + fmt_repeat(_self.bg, inner) + "\\u2502" + NL'),
    # More box patterns...
])

# Actually, let me just read the bytes from box.kn to find the exact patterns
with open('src/tui/components/box.kn', 'rb') as f:
    box_data = f.read()

# Find all \n occurrences
for i, line in enumerate(box_data.split(b'\n'), 1):
    if BS_N in line and not line.strip().startswith(b'//'):
        # Check if it's already been fixed (has text_chr)
        if b'text_chr(10)' not in line and b'NL' not in line:
            print(f"box.kn:{i}: {line.decode('utf-8', errors='replace').rstrip()[:100]}")

# tui/components/editor.kn
with open('src/tui/components/editor.kn', 'rb') as f:
    ed_data = f.read()
for i, line in enumerate(ed_data.split(b'\n'), 1):
    if BS_N in line and not line.strip().startswith(b'//'):
        if b'text_chr(10)' not in line and b'NL' not in line:
            print(f"editor.kn:{i}: {line.decode('utf-8', errors='replace').rstrip()[:100]}")

# tui/components/markdown.kn
with open('src/tui/components/markdown.kn', 'rb') as f:
    md_data = f.read()
for i, line in enumerate(md_data.split(b'\n'), 1):
    if BS_N in line and not line.strip().startswith(b'//'):
        if b'text_chr(10)' not in line and b'NL' not in line:
            print(f"markdown.kn:{i}: {line.decode('utf-8', errors='replace').rstrip()[:100]}")

# tui/components/select_list.kn
with open('src/tui/components/select_list.kn', 'rb') as f:
    sl_data = f.read()
for i, line in enumerate(sl_data.split(b'\n'), 1):
    if BS_N in line and not line.strip().startswith(b'//'):
        if b'text_chr(10)' not in line and b'NL' not in line:
            print(f"select_list.kn:{i}: {line.decode('utf-8', errors='replace').rstrip()[:100]}")

# tui/components/text.kn
with open('src/tui/components/text.kn', 'rb') as f:
    tx_data = f.read()
for i, line in enumerate(tx_data.split(b'\n'), 1):
    if BS_N in line and not line.strip().startswith(b'//'):
        if b'text_chr(10)' not in line and b'NL' not in line:
            print(f"text.kn:{i}: {line.decode('utf-8', errors='replace').rstrip()[:100]}")

print("\nRemaining patterns listed above. Now fixing them...")
