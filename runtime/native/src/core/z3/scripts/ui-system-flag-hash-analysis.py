#!/usr/bin/env python3
"""
UI system flag string hash analysis.
Computes FNV-1a hashes for 7 flag strings and checks for collisions.

Usage: python3 ui-system-flag-hash-analysis.py
"""

FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211


def fnv1a(s: str) -> int:
    """Compute 64-bit FNV-1a hash of a string."""
    h = FNV_OFFSET
    for c in s.encode():
        h ^= c
        h = (h * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def first_8_le(s: str) -> int:
    """Extract first 8 bytes as little-endian integer."""
    b = s.encode()[:8].ljust(8, b'\x00')
    return int.from_bytes(b, 'little')


FLAGS = [
    'hidden', 'visible', 'focusable',
    'interactive', 'disabled', 'hovered', 'pressed'
]

EXPECTED_FLAG_BITS = {
    'hidden':      1,   # ABI_UI_NODE_HIDDEN = 1 << 0
    'visible':     1,   # ALSO maps to ABI_UI_NODE_HIDDEN (hidden=0)
    'focusable':   2,   # ABI_UI_NODE_FOCUSABLE = 1 << 1
    'interactive': 4,   # ABI_UI_NODE_INTERACTIVE = 1 << 2
    'disabled':    8,   # ABI_UI_NODE_DISABLED = 1 << 3
    'hovered':     16,  # ABI_UI_NODE_HOVERED = 1 << 4
    'pressed':     32,  # ABI_UI_NODE_PRESSED = 1 << 5
}

print("=" * 60)
print("UI System Flag String Hash Analysis")
print("=" * 60)
print()

# FNV-1a hashes
print("FNV-1a Hashes:")
print("-" * 50)
hashes = {}
for f in FLAGS:
    h = fnv1a(f)
    hashes[f] = h
    print(f"  {f:15s} -> 0x{h:016x}  (bit {EXPECTED_FLAG_BITS[f]:2d})")

h_set = set(hashes.values())
print(f"\n  Unique: {len(h_set)}/{len(FLAGS)}")
if len(h_set) == len(FLAGS):
    print("  ✓ COLLISION-FREE")
else:
    print("  ✗ COLLISIONS DETECTED!")
print()

# First 8 bytes as LE
print("First 8 bytes (little-endian):")
print("-" * 50)
le_vals = {}
for f in FLAGS:
    v = first_8_le(f)
    le_vals[f] = v
    print(f"  {f:15s} -> 0x{v:016x}  (length={len(f)})")

le_set = set(le_vals.values())
print(f"\n  Unique: {len(le_set)}/{len(FLAGS)}")
if len(le_set) == len(FLAGS):
    print("  ✓ COLLISION-FREE")
else:
    print("  ✗ COLLISIONS DETECTED!")
print()

# Pre-computed token values from the C source
print("Pre-computed token values from C source:")
print("-" * 50)
token_values = {
    'hidden':      {'len': 6,  'lo': 0x00006e6564646968, 'state': 0x85daa81451a55c7a},
    'visible':     {'len': 7,  'lo': 0x00656c6269736976, 'state': 0x7f0f01206f964b92},
    'focusable':   {'len': 9,  'lo': 0x6c62617375636f66, 'state': 0x7a75024eba4e101f},
    'interactive': {'len': 11, 'lo': 0x7463617265746e69, 'state': 0x948038e6c1c6ea72},
    'disabled':    {'len': 8,  'lo': 0x64656c6261736964, 'state': 0x4f87286f47c95184},
    'hovered':     {'len': 7,  'lo': 0x0064657265766f68, 'state': 0x13bef354dde61301},
    'pressed':     {'len': 7,  'lo': 0x0064657373657270, 'state': 0x61f59c74a54f9887},
}

for f in FLAGS:
    v = token_values[f]
    print(f"  {f:15s}: len={v['len']}, lo=0x{v['lo']:016x}, state=0x{v['state']:016x}")

# Verify lengths and first-8-as-LE match
print()
print("Consistency check:")
print("-" * 50)
for f in FLAGS:
    v = token_values[f]
    le = first_8_le(f)
    ok = '✓' if le == v['lo'] else '✗'
    print(f"  {f:15s}: first_8_le=0x{le:016x}, C_lo=0x{v['lo']:016x} {ok}")
