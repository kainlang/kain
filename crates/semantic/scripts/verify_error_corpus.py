#!/usr/bin/env python3
"""Verify annotated Kain semantic error-corpus fixtures."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
EXPECTED_RE = {
    "code": re.compile(r"(?m)^//\s*@expected_code:\s*([A-Za-z0-9-]+)\s*$"),
    "mode": re.compile(r"(?m)^//\s*@expected_mode:\s*([A-Za-z0-9_]+)\s*$"),
    "repair": re.compile(r"(?m)^//\s*@expected_repair:\s*(.+?)\s*$"),
}

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_CORPUS = REPO_ROOT / "crates" / "semantic" / "error_corpus"


@dataclass
class Fixture:
    path: Path
    code: str
    mode: str
    repair: str


def clean(text: str) -> str:
    return ANSI_RE.sub("", text)


def read_fixture(path: Path) -> Fixture | None:
    text = path.read_text(encoding="utf-8")
    values: dict[str, str] = {}
    for key, regex in EXPECTED_RE.items():
        match = regex.search(text)
        if not match:
            return None
        values[key] = match.group(1).strip()
    return Fixture(path=path, code=values["code"], mode=values["mode"], repair=values["repair"])


def all_annotated(corpus: Path) -> list[Fixture]:
    fixtures: list[Fixture] = []
    for path in sorted(corpus.glob("*.kn")):
        fixture = read_fixture(path)
        if fixture is not None:
            fixtures.append(fixture)
    return fixtures


def changed_files() -> set[Path]:
    result = subprocess.run(
        ["git", "diff", "--name-only", "--", "crates/semantic/error_corpus"],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    staged = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--", "crates/semantic/error_corpus"],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    names = set(result.stdout.splitlines()) | set(staged.stdout.splitlines())
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "--", "crates/semantic/error_corpus"],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    names |= set(untracked.stdout.splitlines())
    return {(REPO_ROOT / name).resolve() for name in names if name.endswith(".kn")}


def run_kain_check(fixture: Fixture, kain_bin: str, target: str, timeout: int) -> tuple[bool, str]:
    command = [kain_bin, "check", str(fixture.path), "--target", target]
    env = os.environ.copy()
    env.setdefault("TMP", "Z:\\_b\\tmp")
    env.setdefault("TEMP", "Z:\\_b\\tmp")
    env.setdefault("TMPDIR", "Z:\\_b\\tmp")
    proc = subprocess.run(
        command,
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        timeout=timeout,
        env=env,
        check=False,
    )
    output = clean(proc.stdout + proc.stderr)
    failed_as_expected = proc.returncode != 0
    code_seen = fixture.code in output
    return failed_as_expected and code_seen, output


def run_pipeline_tests(timeout: int) -> int:
    tests = [
        ["cargo", "test", "-p", "kain-semantic", "test_error_corpus_cases"],
        ["cargo", "test", "-p", "kain-semantic", "sidecar_pack"],
    ]
    status = 0
    for command in tests:
        print("+ " + " ".join(command))
        proc = subprocess.run(command, cwd=REPO_ROOT, timeout=timeout, check=False)
        if proc.returncode != 0:
            status = proc.returncode
    return status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument("--fixture", action="append", type=Path, help="specific fixture path; repeatable")
    parser.add_argument("--changed", action="store_true", help="verify changed/untracked corpus files only")
    parser.add_argument("--kain-bin", default=os.environ.get("KAIN_BIN", "kain"))
    parser.add_argument("--target", default="llvm")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--pipeline", action="store_true", help="also run semantic Rust pipeline tests")
    parser.add_argument("--list", action="store_true", help="list annotated fixtures and exit")
    args = parser.parse_args()

    corpus = args.corpus.resolve()
    if args.fixture:
        fixtures = []
        for path in args.fixture:
            fixture = read_fixture(path.resolve())
            if fixture is None:
                print(f"missing metadata: {path}", file=sys.stderr)
                return 2
            fixtures.append(fixture)
    else:
        fixtures = all_annotated(corpus)

    if args.changed:
        changed = changed_files()
        fixtures = [fixture for fixture in fixtures if fixture.path.resolve() in changed]

    if args.list:
        for fixture in fixtures:
            print(f"{fixture.path.name}\t{fixture.code}\t{fixture.mode}\t{fixture.repair}")
        return 0

    if not fixtures:
        print("no annotated fixtures selected")
        return 0

    failures = 0
    for fixture in fixtures:
        ok, output = run_kain_check(fixture, args.kain_bin, args.target, args.timeout)
        if ok:
            print(f"ok   {fixture.path.name} {fixture.code} {fixture.mode}")
        else:
            failures += 1
            first_lines = "\n".join(output.splitlines()[:20])
            print(f"FAIL {fixture.path.name} expected {fixture.code} {fixture.mode}")
            print(first_lines)

    print(f"fixtures={len(fixtures)} failures={failures}")
    if failures:
        return 1

    if args.pipeline:
        return run_pipeline_tests(args.timeout * 4)

    print("pipeline: skipped (use --pipeline to run cargo semantic tests)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
