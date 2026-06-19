"""pykain.validate — validation helpers for Kain test harnesses.

Pure functions that return ints (0 = ok) or dicts Kain can unpack.
No side effects. Designed for use in Kain's module_probe_lane and similar
validation phases.
"""

from __future__ import annotations

import importlib.util
from typing import Any

import numpy as np


def module(name: str) -> int:
    """Check if a Python module is importable.

    Returns the module's "bonus score" if available, 0 if not.
    Kain uses this to skip tests when dependencies are missing.

    Usage from Kain:
        let numpy_ok = pykain.validate.module("numpy")
        if numpy_ok == 0:
            return 0   // skip — numpy not installed
    """
    if importlib.util.find_spec(name) is not None:
        # Return a positive score so Kain can use it as a truthy check
        return 1
    return 0


def all_modules(names: list[str]) -> int:
    """Check that ALL named modules are importable. Returns 0 if all ok,
    or the 1-based index of the first missing module (1 = names[0] missing).
    """
    for i, name in enumerate(names):
        if importlib.util.find_spec(name) is None:
            return i + 1
    return 0


def version() -> int:
    """Return 1 when pykain exposes a non-empty package version."""
    try:
        import pykain

        return 1 if len(str(getattr(pykain, "__version__", ""))) > 0 else 0
    except Exception:
        return 0


def installed_modules() -> dict[str, bool]:
    """Return a dict of common modules and their availability.

    Kain reads: json_bool_or(modules, "numpy", false)
    """
    common = [
        "numpy", "torch", "pygame", "z3", "fastmcp", "flet",
        "scipy", "pillow", "cv2", "matplotlib", "jax", "cupy",
        "pandas", "sklearn", "transformers", "sqlalchemy",
    ]
    return {name: importlib.util.find_spec(name) is not None for name in common}


# ═══════════════════════════════════════════════════════════════════════════
#  Shape / contract / type checks — return 0 on success
# ═══════════════════════════════════════════════════════════════════════════

def tensor_shape(tensor: Any, expected: list[int]) -> int:
    """Check tensor shape matches. Returns 0 on match, error code on mismatch."""
    try:
        arr = np.asarray(tensor)
        actual = list(arr.shape)
        if len(actual) != len(expected):
            return 10 + len(actual)
        for i, (a, e) in enumerate(zip(actual, expected)):
            if int(a) != int(e):
                return 20 + i
        return 0
    except Exception:
        return 1


def image_contract(image: Any, expected_contract: str = "kain.shared.image") -> int:
    """Verify image meets the expected shared contract. Returns 0 on success."""
    try:
        arr = np.asarray(image)
        if len(arr.shape) not in (2, 3):
            return 10
        if not arr.flags["C_CONTIGUOUS"]:
            return 20
        if arr.dtype != np.uint8:
            return 30
        return 0
    except Exception:
        return 1


def buffer_contract(buffer: Any, expected_type: str = "uint8",
                    expected_size: int = 1) -> int:
    """Verify buffer meets expected element type and size. Returns 0 on success."""
    try:
        arr = np.asarray(buffer)
        aliases = {
            "u8": "uint8",
            "i8": "int8",
            "u16": "uint16",
            "i16": "int16",
            "u32": "uint32",
            "i32": "int32",
            "f32": "float32",
            "f64": "float64",
        }
        actual_type = str(arr.dtype)
        expected_name = aliases.get(expected_type, expected_type)
        if actual_type != expected_name:
            return 10
        if arr.dtype.itemsize != expected_size:
            return 20
        if not arr.flags["C_CONTIGUOUS"]:
            return 30
        return 0
    except Exception:
        return 1


def not_none(value: Any) -> int:
    """Check that a Python value is not None. Returns 0 if ok, 1 if None."""
    return 0 if value is not None else 1


def string_not_empty(value: Any) -> int:
    """Check that a value is a non-empty string. Returns 0 if ok."""
    try:
        s = str(value)
        return 0 if len(s) > 0 else 1
    except Exception:
        return 2
