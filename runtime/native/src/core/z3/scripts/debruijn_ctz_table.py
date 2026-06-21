"""
debruijn_ctz_table.py — Generate and verify de Bruijn-based count-trailing-zeros tables.

The de Bruijn CTZ technique:
  1. Isolate the lowest set bit:  lowbit = value & -value
  2. Multiply by a de Bruijn constant: hash = lowbit * DEBRUIJN_MULTIPLIER
  3. Extract top bits: index = hash >> (64 - table_bits)
  4. Look up in table: result = TABLE[index]

The 64-bit de Bruijn constant 0x03f79d71b4cb0a89 is a classic choice that
maps each of the 64 possible lowbit positions to a unique 6-bit index.

Usage:
    python debruijn_ctz_table.py              # Print full 64-entry table
    python debruijn_ctz_table.py --verify      # Verify no collisions
    python debruijn_ctz_table.py --smt2        # Generate SMT-LIB2 table
    python debruijn_ctz_table.py --c-array     # Generate C array definition
"""

import argparse

DEBRUIJN_MULTIPLIER = 0x03F79D71B4CB0A89
TABLE_BITS = 6  # 2^6 = 64 entries

def generate_ctz_table():
    """Generate the de Bruijn CTZ lookup table for 64-bit values."""
    table = [0] * 64
    MASK64 = (1 << 64) - 1
    for pos in range(64):
        lowbit = 1 << pos
        # 64-bit unsigned multiplication (wrap at 2^64)
        product = (lowbit * DEBRUIJN_MULTIPLIER) & MASK64
        hash_val = product >> (64 - TABLE_BITS)
        table[hash_val] = pos
    return table

def verify_table(table):
    """Verify that every position 0..63 appears exactly once."""
    seen = set()
    for idx, val in enumerate(table):
        if val in seen:
            print(f"COLLISION: table[{idx}] = {val}, but {val} already used")
            return False
        seen.add(val)
    expected = set(range(64))
    missing = expected - seen
    if missing:
        print(f"MISSING: positions {sorted(missing)} not in table")
        return False
    print(f"OK: All 64 positions present, no collisions.")
    print(f"Table: {table}")
    return True

def format_c_array(table):
    """Format as C static const uint8_t array."""
    lines = ["static const uint8_t DEBRUIJN_CTZ_64[64] = {"]
    for i in range(0, 64, 8):
        chunk = ", ".join(f"{v:3d}" for v in table[i:i+8])
        prefix = "    " if i == 0 else "    "
        lines.append(f"{prefix}{chunk},")
    lines.append("};")
    return "\n".join(lines)

def format_smt2_table(table):
    """Format as SMT-LIB2 ite chain for Z3 proofs."""
    lines = ["(define-fun debruijn_table ((idx (_ BitVec 6))) (_ BitVec 6)"]
    for idx, val in enumerate(table):
        idx_bits = f"#b{idx:06b}"
        val_bits = f"#b{val:06b}"
        if idx < 63:
            lines.append(f"  (ite (= idx {idx_bits}) {val_bits}")
        else:
            lines.append(f"  {val_bits}")
    # Close all the ite nesting
    for _ in range(63):
        lines.append(")")
    lines.append(")")
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(
        description="Generate and verify de Bruijn CTZ lookup table"
    )
    parser.add_argument("--verify", action="store_true",
                       help="Verify the full 64-entry table")
    parser.add_argument("--smt2", action="store_true",
                       help="Output SMT-LIB2 format")
    parser.add_argument("--c-array", action="store_true",
                       help="Output C array format")
    args = parser.parse_args()

    table = generate_ctz_table()

    if args.verify:
        verify_table(table)
    elif args.smt2:
        print(format_smt2_table(table))
    elif args.c_array:
        print(format_c_array(table))
    else:
        print(f"De Bruijn multiplier: 0x{DEBRUIJN_MULTIPLIER:016X}")
        print(f"CTZ table ({len(table)} entries):")
        for i in range(0, 64, 8):
            chunk = ", ".join(f"{v:2d}" for v in table[i:i+8])
            print(f"  [{i:2d}..{i+7:2d}]: {chunk}")
        verify_table(table)

if __name__ == "__main__":
    main()
