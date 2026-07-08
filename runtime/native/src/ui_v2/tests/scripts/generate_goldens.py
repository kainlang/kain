#!/usr/bin/env python3
"""
generate_goldens.py — Kaintana Golden File Generator

Builds and runs each test spec from a TSV file against the null backend,
captures the raw uint32_t framebuffer, and writes golden .bin files.

Usage:
    python generate_goldens.py --spec specs/core.tsv
    python generate_goldens.py --spec specs/core.tsv --bin kaintana-test-runner.exe
    python generate_goldens.py --spec specs/core.tsv --build --out golden/

Flags:
    --spec PATH       Path to the spec TSV file (required)
    --bin PATH        Path to pre-built test runner .exe
    --build           Build the test runner via `kain build` before running
    --out DIR         Output directory for golden .bin files (default: golden/)
    --record-flag     Flag passed to test runner to enable record mode
                      (default: --record)
"""

import argparse
import csv
import hashlib
import os
import subprocess
import sys
import tempfile
import json
from pathlib import Path
from typing import Iterator


# ── Column indices in the TSV ──────────────────────────────────────────────
COL_NAME = 0
COL_WIDTH = 1
COL_HEIGHT = 2
COL_CALLS = 3
COL_EXPECT_CMDS = 4
COL_GOLDEN = 5
COL_DESC = 6

REQUIRED_COLS = 7


def parse_tsv(path: Path) -> list[dict]:
    """Parse a spec TSV into a list of test case dicts."""
    tests = []
    with open(path, "r", newline="") as f:
        reader = csv.reader(f, delimiter="\t")
        for row in reader:
            # Skip comment/header lines
            if not row or row[0].startswith("#"):
                continue
            if row[0] == "name" and row[1] == "width":
                continue  # skip header row

            if len(row) < REQUIRED_COLS:
                print(f"  [WARN] Short row ({len(row)} cols), skipping: {row}")
                continue

            name = row[COL_NAME].strip()
            golden = row[COL_GOLDEN].strip()
            if not name:
                continue

            tests.append({
                "name": name,
                "width": int(row[COL_WIDTH].strip()),
                "height": int(row[COL_HEIGHT].strip()),
                "calls": row[COL_CALLS].strip(),
                "expect_cmds": row[COL_EXPECT_CMDS].strip(),
                "golden": golden,
                "desc": row[COL_DESC].strip() if len(row) > COL_DESC else "",
            })
    return tests


def find_runner(path_hint: str | None) -> Path:
    """Locate the test runner executable."""
    if path_hint:
        p = Path(path_hint)
        if p.exists():
            return p.resolve()
        print(f"[ERROR] Specified binary not found: {path_hint}")
        sys.exit(1)

    # Search in common locations
    candidates = [
        Path("kaintana-test-runner.exe"),
        Path("target/kaintana-test-runner.exe"),
        Path("../kaintana-test-runner.exe"),
        Path("build/kaintana-test-runner.exe"),
    ]
    test_dir = Path(__file__).resolve().parent.parent  # tests/
    for c in candidates:
        p = test_dir / c
        if p.exists():
            return p.resolve()

    print("[ERROR] Could not find kaintana-test-runner.exe")
    print("  Pass --bin PATH or run --build to build it first.")
    sys.exit(1)


def build_runner(test_dir: Path) -> Path:
    """Build the test runner via `kain build`."""
    build_dir = test_dir
    print(f"[BUILD] Building test runner in {build_dir} ...")
    result = subprocess.run(
        ["kain", "build", str(build_dir), "--target", "llvm"],
        capture_output=True, text=True, timeout=120,
    )
    if result.returncode != 0:
        print("[BUILD] Failed:")
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)
    print("[BUILD] OK")

    # Look for the output binary
    out_patterns = [
        build_dir / ".kain" / "out" / "**" / "*.exe",
        build_dir / "starter.exe",
    ]
    from glob import glob
    for pattern in out_patterns:
        matches = list(glob(str(pattern), recursive=True))
        if matches:
            # Pick the most recent
            matches.sort(key=lambda p: os.path.getmtime(p), reverse=True)
            return Path(matches[0])

    print("[ERROR] Build succeeded but could not locate .exe output")
    sys.exit(1)


def run_test(runner: Path, spec_path: Path, test_name: str) -> dict:
    """Run a single test case and return JSON result with framebuffer data."""
    cmd = [
        str(runner),
        str(spec_path),
        "--json",
        "--filter", test_name,
        "--record",           # emit framebuffer in JSON output
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"  [RUN] Exit code {result.returncode}")
        if result.stdout:
            print(f"  stdout: {result.stdout[:500]}")
        if result.stderr:
            print(f"  stderr: {result.stderr[:500]}")
        return None

    # Parse JSON output
    try:
        lines = result.stdout.strip().splitlines()
        # Find the last JSON line (the test runner may emit multiple lines)
        json_line = None
        for line in reversed(lines):
            line = line.strip()
            if line.startswith("{"):
                json_line = line
                break
        if json_line is None:
            print(f"  [PARSE] No JSON line found in output")
            return None
        return json.loads(json_line)
    except json.JSONDecodeError as e:
        print(f"  [PARSE] JSON decode error: {e}")
        return None


def write_golden(out_dir: Path, test_name: str,
                 framebuffer: list[int], width: int, height: int) -> Path:
    """Write pixel data as raw uint32_t little-endian binary."""
    golden_path = out_dir / f"{test_name}.bin"
    # Pack uint32_t list to little-endian bytes
    data = bytearray()
    for px in framebuffer:
        data.extend(px.to_bytes(4, "little"))
    golden_path.write_bytes(data)
    return golden_path


def verify_golden(golden_path: Path, framebuffer: list[int]) -> bool:
    """Byte-for-byte comparison against existing golden."""
    existing = golden_path.read_bytes()
    expected_len = len(framebuffer) * 4
    if len(existing) != expected_len:
        print(f"    Size mismatch: golden={len(existing)}B, framebuffer={expected_len}B")
        return False

    data = bytearray()
    for px in framebuffer:
        data.extend(px.to_bytes(4, "little"))

    if data != existing:
        # Find first mismatch
        for i in range(len(data)):
            if data[i] != existing[i]:
                pixel_idx = i // 4
                byte_in_pixel = i % 4
                print(f"    Mismatch at byte {i} (pixel {pixel_idx}, byte {byte_in_pixel})")
                print(f"      golden[0x{i:08x}] = 0x{existing[i]:02x}")
                print(f"      output[0x{i:08x}] = 0x{data[i]:02x}")
                return False
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generate golden .bin files for Kaintana tests"
    )
    parser.add_argument("--spec", required=True,
                        help="Path to spec TSV file")
    parser.add_argument("--bin", default=None,
                        help="Path to pre-built test runner .exe")
    parser.add_argument("--build", action="store_true",
                        help="Build test runner via kain build before running")
    parser.add_argument("--out", default="golden",
                        help="Output directory for golden .bin files")
    parser.add_argument("--verify", action="store_true",
                        help="Verify existing goldens instead of regenerating")
    parser.add_argument("--filter", default=None,
                        help="Only process tests matching this substring")
    args = parser.parse_args()

    spec_path = Path(args.spec).resolve()
    if not spec_path.exists():
        print(f"[ERROR] Spec file not found: {spec_path}")
        sys.exit(1)

    test_dir = spec_path.parent.parent  # tests/
    tests = parse_tsv(spec_path)
    print(f"[INFO] Loaded {len(tests)} test cases from {spec_path.name}")

    # Filter tests that need goldens
    render_tests = [t for t in tests if t["golden"] != "-"]
    if args.filter:
        render_tests = [t for t in render_tests if args.filter in t["name"]]

    if not render_tests:
        print("[INFO] No render tests to process (all have golden='-')")
        return

    # Locate or build runner
    runner = find_runner(args.bin)
    if args.build:
        runner = build_runner(test_dir)

    print(f"[INFO] Using runner: {runner}")
    print()

    out_dir = Path(args.out)
    if not out_dir.is_absolute():
        out_dir = (test_dir / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    passed = 0
    failed = 0
    skipped = 0

    for tc in render_tests:
        name = tc["name"]
        w, h = tc["width"], tc["height"]
        golden_path = out_dir / f"{name}.bin"

        print(f"[TEST] {name} ({w}x{h}) ... ", end="", flush=True)

        # Check if golden already exists in verify mode
        if args.verify:
            if not golden_path.exists():
                print("SKIP (golden not found)")
                skipped += 1
                continue
            # Run test and compare
            result = run_test(runner, spec_path, name)
            if result is None:
                print("FAIL (runner error)")
                failed += 1
                continue

            fb = result.get("framebuffer")
            if not fb or not isinstance(fb, list):
                print("FAIL (no framebuffer data)")
                failed += 1
                continue

            if verify_golden(golden_path, fb):
                print("OK")
                passed += 1
            else:
                sha = hashlib.sha256(golden_path.read_bytes()).hexdigest()[:16]
                print(f"FAIL (golden mismatch, sha={sha})")
                failed += 1
            continue

        # Generate mode: run test and capture framebuffer
        result = run_test(runner, spec_path, name)
        if result is None:
            print("FAIL")
            failed += 1
            continue

        fb = result.get("framebuffer")
        if not fb or not isinstance(fb, list):
            print("FAIL (no framebuffer data)")
            failed += 1
            continue

        expected_pixels = w * h
        actual_pixels = len(fb)
        if actual_pixels != expected_pixels:
            print(f"FAIL (pixel count: expected {expected_pixels}, got {actual_pixels})")
            failed += 1
            continue

        # Write golden file
        golden_path = write_golden(out_dir, name, fb, w, h)
        sha = hashlib.sha256(golden_path.read_bytes()).hexdigest()
        size_kb = golden_path.stat().st_size / 1024
        print(f"OK ({size_kb:.1f}KB, sha256={sha[:16]}...)")
        passed += 1

    # Summary
    print()
    print(f"[SUMMARY] Total: {passed + failed + skipped}  "
          f"Pass: {passed}  Fail: {failed}  Skip: {skipped}")

    # If verifying, also report any golden files not covered by current tests
    if args.verify:
        all_goldens = set(out_dir.glob("*.bin"))
        covered = {f"{t['name']}.bin" for t in render_tests}
        orphans = all_goldens - {out_dir / g for g in covered}
        if orphans:
            print(f"[INFO] Orphan goldens (no matching test): {len(orphans)}")
            for o in sorted(orphans):
                print(f"  {o.name}")

    if failed > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
