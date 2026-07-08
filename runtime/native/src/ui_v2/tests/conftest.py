"""
conftest.py — pytest configuration for Kaintana test runner.

Discovers TSV specs and parametrizes tests. The test_runner C binary
parses and executes each spec, emitting JSON results that pytest asserts.
"""

import csv
import json
import os
import subprocess
from pathlib import Path

# Paths relative to this file
TEST_DIR = Path(__file__).resolve().parent
SPECS_DIR = TEST_DIR / "specs"
GOLDEN_DIR = TEST_DIR / "golden"
TEST_RUNNER = TEST_DIR / "test_runner.exe"


def pytest_addoption(parser):
    """Add --test-runner-path option for specifying an alternate binary."""
    parser.addoption(
        "--test-runner-path",
        action="store",
        default=None,
        help="Path to the test_runner executable",
    )


def pytest_generate_tests(metafunc):
    """Parametrize test_spec with all test cases from all spec files."""
    if "spec_case" not in metafunc.fixturenames:
        return

    spec_files = sorted(SPECS_DIR.glob("*.tsv"))
    cases = []

    for spec_file in spec_files:
        with open(spec_file, "r", newline="") as f:
            reader = csv.reader(f, delimiter="\t")
            for row in reader:
                if not row or row[0].startswith("#"):
                    continue
                if row[0] == "name" and len(row) > 1 and row[1] == "width":
                    continue
                if len(row) < 4:
                    continue

                name = row[0].strip()
                if not name:
                    continue

                cases.append({
                    "spec_name": spec_file.stem,
                    "spec_path": str(spec_file),
                    "name": name,
                    "width": int(row[1].strip()) if row[1].strip() else 0,
                    "height": int(row[2].strip()) if row[2].strip() else 0,
                    "calls": row[3].strip() if len(row) > 3 else "-",
                    "expect_cmds": row[4].strip() if len(row) > 4 else "-",
                    "golden": row[5].strip() if len(row) > 5 else "-",
                    "desc": row[6].strip() if len(row) > 6 else "",
                })

    metafunc.parametrize("spec_case", cases, ids=[c["name"] for c in cases])


def get_test_runner(request):
    """Locate the test runner binary."""
    runner = request.config.getoption("--test-runner-path")
    if runner:
        runner_path = Path(runner).resolve()
        if not runner_path.exists():
            pytest.fail(f"Test runner not found: {runner_path}")
        return str(runner_path)

    # Default: look in test directory
    runner_path = TEST_RUNNER
    if not runner_path.exists():
        pytest.fail(
            f"Test runner not found at {runner_path}. "
            f"Run 'make' to build it first."
        )
    return str(runner_path)


def run_test_case(runner_path: str, spec_case: dict) -> dict:
    """Run a single test case via the C test runner and return parsed JSON."""
    cmd = [
        runner_path,
        spec_case["spec_path"],
        "--filter", spec_case["name"],
    ]

    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=30
    )

    if result.returncode != 0 and result.returncode != 1:
        raise RuntimeError(
            f"Test runner exited with code {result.returncode}: "
            f"{result.stderr[:500]}"
        )

    # Parse the last JSON object from stdout (multi-line JSON)
    # Find the last '{' and extract the complete object
    stdout = result.stdout.strip()
    last_brace = stdout.rfind('{')
    if last_brace >= 0:
        # Find matching closing brace
        depth = 0
        for i in range(last_brace, len(stdout)):
            if stdout[i] == '{':
                depth += 1
            elif stdout[i] == '}':
                depth -= 1
                if depth == 0:
                    try:
                        return json.loads(stdout[last_brace:i+1])
                    except json.JSONDecodeError:
                        break

    raise RuntimeError(
        f"No JSON output from test runner for '{spec_case['name']}': "
        f"{result.stdout[:500]}"
    )
