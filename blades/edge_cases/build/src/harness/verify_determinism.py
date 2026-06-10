#!/usr/bin/env python3
"""
verify_determinism.py — Build determinism verification script

Compares two build output directories byte-for-byte using SHA256.
Walks both directory trees, hashes every file, and produces a JSON
report with pass/fail per file.

Invocation:
    python verify_determinism.py \\
        --pass1-root <dir> \\
        --pass2-root <dir> \\
        --report <path>

Exit codes:
    0 — All files match (deterministic)
    1 — One or more files differ (non-deterministic)
"""

import argparse
import hashlib
import json
import os
import sys


def sha256_file(path: str) -> str:
    """Compute SHA256 hex digest of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def walk_relative(root: str) -> dict[str, str]:
    """Walk a directory tree, returning {relative_path: sha256_hex}."""
    result: dict[str, str] = {}
    root = os.path.normpath(root)
    if not os.path.isdir(root):
        return result
    for dirpath, dirnames, filenames in os.walk(root):
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root)
            try:
                result[rel] = sha256_file(full)
            except (OSError, IOError) as e:
                result[rel] = f"ERROR:{e}"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify two build output directories are byte-for-byte identical"
    )
    parser.add_argument(
        "--pass1-root",
        required=True,
        help="First build output directory (reference)",
    )
    parser.add_argument(
        "--pass2-root",
        required=True,
        help="Second build output directory (comparison)",
    )
    parser.add_argument(
        "--report",
        required=True,
        help="Path for JSON report output",
    )
    args = parser.parse_args()

    pass1_root = os.path.normpath(args.pass1_root)
    pass2_root = os.path.normpath(args.pass2_root)

    # Walk both directories
    pass1_files = walk_relative(pass1_root)
    pass2_files = walk_relative(pass2_root)

    all_paths = sorted(set(pass1_files.keys()) | set(pass2_files.keys()))

    identical: list[str] = []
    different: list[dict] = []
    missing: list[str] = []
    extra: list[str] = []

    for rel in all_paths:
        in_p1 = rel in pass1_files
        in_p2 = rel in pass2_files

        if in_p1 and in_p2:
            h1 = pass1_files[rel]
            h2 = pass2_files[rel]
            if h1 == h2:
                identical.append(rel)
            else:
                different.append({
                    "file": rel,
                    "pass1_sha256": h1,
                    "pass2_sha256": h2,
                })
        elif in_p1 and not in_p2:
            missing.append(rel)
        else:
            # in_p2 but not in_p1
            extra.append(rel)

    deterministic = (
        len(different) == 0 and len(missing) == 0 and len(extra) == 0
    )

    report = {
        "pass1_root": pass1_root,
        "pass2_root": pass2_root,
        "total_files": len(all_paths),
        "identical": len(identical),
        "different": len(different),
        "missing": missing,
        "extra": extra,
        "deterministic": deterministic,
    }

    # Write JSON report
    report_dir = os.path.dirname(args.report)
    if report_dir:
        os.makedirs(report_dir, exist_ok=True)
    with open(args.report, "w") as f:
        json.dump(report, f, indent=2)

    # Console output
    if deterministic:
        print(
            f"Determinism verification: PASS\n"
            f"  {len(identical)} files compared: {len(identical)} identical, "
            f"0 different"
        )
        return 0
    else:
        print(
            f"Determinism verification: FAIL\n"
            f"  {len(all_paths)} files compared: {len(identical)} identical, "
            f"{len(different)} different"
        )
        for entry in different:
            print(f"  DIFFERENT: {entry['file']}")
            print(f"    Pass1 SHA256: {entry['pass1_sha256']}")
            print(f"    Pass2 SHA256: {entry['pass2_sha256']}")
        for rel in missing:
            print(f"  MISSING from pass2: {rel}")
        for rel in extra:
            print(f"  EXTRA in pass2: {rel}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
