#!/usr/bin/env python3
"""
clean_file.py — Strip CRLF, trailing whitespace, and trailing blank lines from any text file.
Usage: python docs/clean_file.py [FILE...]
       python docs/clean_file.py *.md
       python docs/clean_file.py --recursive docs/ *.md
"""

import os
import sys
import glob

def clean(path: str) -> bool:
    """Clean one file in-place. Returns True if changed."""
    try:
        with open(path, "rb") as f:
            raw = f.read()
    except OSError as e:
        print(f"  SKIP {path}: {e}", file=sys.stderr)
        return False

    # 1. CRLF → LF
    body = raw.replace(b"\r\n", b"\n").replace(b"\r", b"\n")

    # 2. Strip trailing whitespace on each line
    lines = body.split(b"\n")
    stripped = [line.rstrip(b" \t") for line in lines]

    # 3. Remove trailing blank lines (keep one final empty line for POSIX)
    while len(stripped) > 1 and stripped[-1] == b"" and stripped[-2] == b"":
        stripped.pop()

    result = b"\n".join(stripped)

    if result == raw:
        return False

    with open(path, "wb") as f:
        f.write(result)
    return True


def main():
    # Collect paths from args + glob expansion
    paths: list[str] = []
    recursive = False
    for arg in sys.argv[1:]:
        if arg == "--recursive":
            recursive = True
        else:
            expanded = glob.glob(arg, recursive=recursive)
            paths.extend(expanded if expanded else [arg])

    if not paths:
        print("Usage: python docs/clean_file.py [--recursive] FILE...")
        sys.exit(1)

    changed = 0
    total = 0
    for p in paths:
        if not os.path.isfile(p):
            continue
        total += 1
        if clean(p):
            print(f"  CLEAN {p}")
            changed += 1

    print(f"\nDone: {changed}/{total} files cleaned, {total - changed} already clean.")
    return 0 if changed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
