"""
test_from_specs.py — pytest test functions for Kaintana test runner.

Each test case from the TSV spec files is parametrized via conftest.py's
pytest_generate_tests hook, which populates the `spec_case` fixture.

Assertions:
  - The C test runner must exit successfully and emit valid JSON
  - test["pass"] must be true
  - Command count should match expectations
"""

import json
import os
from pathlib import Path

import pytest

# Import the test runner helper from conftest
from conftest import get_test_runner, run_test_case

# Marker: all tests in this file are Kaintana C substrate tests
pytestmark = pytest.mark.kaintana


def test_spec(spec_case, request):
    """Execute a single TSV spec case and verify result."""
    runner_path = get_test_runner(request)
    result = run_test_case(runner_path, spec_case)

    # Basic pass/fail assertion
    assert result.get("pass", False), (
        f"Test '{spec_case['name']}' failed: "
        f"{result.get('error', 'unknown error')}"
    )

    # Command count assertion (skip if expect_cmds is "-")
    expect_cmds = spec_case.get("expect_cmds", "-")
    if expect_cmds != "-" and expect_cmds != "":
        actual_cmds = result.get("cmds", -1)

        if expect_cmds.startswith(">="):
            # >= N format
            min_cmds = int(expect_cmds[2:].strip())
            assert actual_cmds >= min_cmds, (
                f"Expected >= {min_cmds} commands, got {actual_cmds} "
                f"for '{spec_case['name']}'"
            )
        else:
            # Exact match
            expected = int(expect_cmds)
            assert actual_cmds == expected, (
                f"Expected {expected} commands, got {actual_cmds} "
                f"for '{spec_case['name']}'"
            )

    # Verify test metadata matches
    assert result.get("name", "") == spec_case["name"], (
        f"Test name mismatch: expected '{spec_case['name']}', "
        f"got '{result.get('name')}'"
    )

    assert result.get("fb_width", -1) == spec_case["width"], (
        f"Width mismatch for '{spec_case['name']}': "
        f"expected {spec_case['width']}, got {result.get('fb_width')}"
    )

    assert result.get("fb_height", -1) == spec_case["height"], (
        f"Height mismatch for '{spec_case['name']}': "
        f"expected {spec_case['height']}, got {result.get('fb_height')}"
    )
