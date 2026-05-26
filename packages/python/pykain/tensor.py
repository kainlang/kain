"""pykain.tensor — normalized tensor crossing for Kain↔Python.

Unifies numpy, torch, and future backends behind a single stable surface.
Kain scripts call these functions; pykain handles all the backend-specific
layout, dtype, and ownership normalization on the Python side.

Every function that returns a tensor returns a HOST OBJECT — Kain keeps it
as a Python reference and uses it with python_shared_buffer / kain_tensor_*
as needed. Every info/signature/validate function returns a plain Python value
that auto-materializes into a Kain scalar or dict.
"""

from __future__ import annotations

import importlib.util
import json
from typing import Any

import numpy as np

from . import _native_loader

_HAS_TORCH = importlib.util.find_spec("torch") is not None
if _HAS_TORCH:
    import torch


def _plan(plan_text: str) -> dict[str, Any]:
    return json.loads(plan_text)


def _has_module(name: str) -> bool:
    return importlib.util.find_spec(name) is not None


# ═══════════════════════════════════════════════════════════════════════════
#  Grid creation — the "give me a tensor to test with" entry point
# ═══════════════════════════════════════════════════════════════════════════

def grid(plan_text: str, seed: int, backend: str = "auto") -> Any:
    """Create a test tensor grid. Returns a host object.

    backend: "auto" (prefers numpy), "numpy", or "torch"
    """
    plan = _plan(plan_text)
    rows = int(plan.get("tensor_rows", 3))
    cols = int(plan.get("tensor_cols", 4))
    total = rows * cols

    if backend == "auto":
        backend = "numpy"
    elif backend == "torch" and not _HAS_TORCH:
        backend = "numpy"

    if backend == "torch" and _HAS_TORCH:
        base = torch.arange(0, total, dtype=torch.float32).reshape(rows, cols)
        scale = float(plan.get("torch_scale", 1.0))
        return (base * scale) + float(seed)
    else:
        start = float(plan.get("numpy_start", -1.0))
        stop = float(plan.get("numpy_stop", 1.0))
        base = np.linspace(start, stop, total, dtype=np.float32).reshape(rows, cols)
        return np.ascontiguousarray(base + np.float32(seed))


def grid_signature(plan_text: str, seed: int, backend: str = "auto") -> int:
    return signature(grid(plan_text, seed, backend=backend))


def grid_byte_length(plan_text: str, seed: int, backend: str = "auto") -> int:
    return int(info(grid(plan_text, seed, backend=backend)).get("byte_length", 0))


def grid_contract(plan_text: str, seed: int, backend: str = "auto") -> int:
    """Return 0 when the generated grid matches the plan-level tensor contract."""
    try:
        plan = _plan(plan_text)
        rows = int(plan.get("tensor_rows", 3))
        cols = int(plan.get("tensor_cols", 4))
        meta = info(grid(plan_text, seed, backend=backend))
        if not meta.get("valid", False):
            return 1
        shape = meta.get("shape", [])
        if len(shape) != 2 or int(shape[0]) != rows or int(shape[1]) != cols:
            return 2
        if int(meta.get("byte_length", 0)) != rows * cols * 4:
            return 3
        if str(meta.get("contract", "")) != "kain.shared.tensor":
            return 4
        return 0
    except Exception:
        return 9


def grid_ok(plan_text: str, seed: int, backend: str = "auto") -> bool:
    """Bool-shaped Kain-facing contract probe for current host-object semantics."""
    return grid_contract(plan_text, seed, backend=backend) == 0


def zeros(shape: list[int], dtype: str = "float32", backend: str = "numpy") -> Any:
    """Create a zero tensor. Returns a host object."""
    shape_tuple = tuple(int(d) for d in shape)
    if backend == "torch" and _HAS_TORCH:
        torch_dtype = getattr(torch, dtype, torch.float32)
        return torch.zeros(shape_tuple, dtype=torch_dtype)
    return np.zeros(shape_tuple, dtype=np.dtype(dtype))


def byte_grid(plan_text: str, seed: int) -> np.ndarray:
    """Create a uint8 byte grid for buffer testing. Always returns numpy."""
    plan = _plan(plan_text)
    rows = int(plan.get("tensor_rows", 3))
    cols = int(plan.get("tensor_cols", 4))
    total = rows * cols
    base = np.arange(total, dtype=np.uint8).reshape(rows, cols)
    return np.ascontiguousarray(base + np.uint8(int(seed) % 251))


# ═══════════════════════════════════════════════════════════════════════════
#  Info — structured metadata that Kain unpacks with json_int_or / etc.
# ═══════════════════════════════════════════════════════════════════════════

def info(tensor: Any) -> dict[str, Any]:
    """Return normalized tensor metadata as a dict.

    Kain reads this with json_int_or(info, "element_count", 0) etc.
    Always returns: shape, dtype, backend, element_count, byte_length, valid.
    """
    if _HAS_TORCH and isinstance(tensor, torch.Tensor):
        native_info = _native_loader.inspect_object(tensor, kind="tensor")
        if native_info.get("valid"):
            native_info["backend"] = "torch"
            native_info["contract"] = "kain.shared.tensor"
            native_info["source_backend"] = "torch"
            return native_info
        arr = tensor.detach().cpu()
        result: dict[str, Any] = {"valid": False}
        result.update({
            "shape": list(arr.shape),
            "dtype": str(arr.dtype).replace("torch.", ""),
            "element_type": str(arr.dtype).replace("torch.", ""),
            "element_size": int(arr.element_size()),
            "backend": "torch",
            "source_runtime": "python",
            "source_backend": "torch",
            "ownership": "python-host-object",
            "contract": "kain.shared.tensor",
            "contract_version": 1,
            "element_count": int(arr.numel()),
            "byte_length": int(arr.element_size() * arr.numel()),
            "valid": True,
        })
        return result

    if isinstance(tensor, np.ndarray):
        native_info = _native_loader.inspect_object(tensor, kind="tensor")
        if native_info.get("valid"):
            native_info["backend"] = "numpy"
            native_info["contract"] = "kain.shared.tensor"
            native_info["source_backend"] = "numpy"
            return native_info
        result: dict[str, Any] = {"valid": False}
        result.update({
            "shape": list(tensor.shape),
            "dtype": str(tensor.dtype),
            "element_type": str(tensor.dtype),
            "element_size": int(tensor.dtype.itemsize),
            "backend": "numpy",
            "source_runtime": "python",
            "source_backend": "numpy",
            "ownership": "python-borrowed",
            "contract": "kain.shared.tensor",
            "contract_version": 1,
            "element_count": int(tensor.size),
            "byte_length": int(tensor.nbytes),
            "valid": True,
        })
        return result

    # Unknown — try to extract what we can
    try:
        native_info = _native_loader.inspect_object(tensor, kind="tensor")
        if native_info.get("valid"):
            native_info["contract"] = "kain.shared.tensor"
            return native_info
        arr = np.asarray(tensor)
        result: dict[str, Any] = {"valid": False}
        result.update({
            "shape": list(arr.shape),
            "dtype": str(arr.dtype),
            "element_type": str(arr.dtype),
            "element_size": int(arr.dtype.itemsize),
            "backend": "unknown",
            "source_runtime": "python",
            "source_backend": "unknown",
            "ownership": "python-borrowed",
            "contract": "kain.shared.tensor",
            "contract_version": 1,
            "element_count": int(arr.size),
            "byte_length": int(arr.nbytes),
            "valid": True,
        })
        return result
    except Exception:
        result: dict[str, Any] = {"valid": False}
        result["error"] = "cannot convert to array"
        return result


def info_json(tensor: Any) -> str:
    return json.dumps(info(tensor), default=str, separators=(",", ":"))


def signature(tensor: Any) -> int:
    """Checksum for verification. Fast path for numpy; detach+sum for torch."""
    try:
        if not (_HAS_TORCH and isinstance(tensor, torch.Tensor)):
            return _native_loader.signature(tensor)
    except Exception:
        pass
    if _HAS_TORCH and isinstance(tensor, torch.Tensor):
        return int(round(float(tensor.detach().cpu().sum().item())))
    arr = np.asarray(tensor, dtype=np.float64)
    return int(round(float(arr.sum())))


# ═══════════════════════════════════════════════════════════════════════════
#  Validation helpers
# ═══════════════════════════════════════════════════════════════════════════

def validate(tensor: Any) -> dict[str, Any]:
    """Full validation: returns info + is_contiguous + is_writeable + checks."""
    result = info(tensor)
    if not result["valid"]:
        return result

    result["is_contiguous"] = False
    result["is_writeable"] = True

    if _HAS_TORCH and isinstance(tensor, torch.Tensor):
        result["is_contiguous"] = bool(tensor.is_contiguous())
        return result

    if isinstance(tensor, np.ndarray):
        result["is_contiguous"] = bool(tensor.flags["C_CONTIGUOUS"])
        result["is_writeable"] = bool(tensor.flags["WRITEABLE"])
        return result

    return result


def check_shape(tensor: Any, expected: list[int]) -> int:
    """Return 0 if shapes match, nonzero error code otherwise."""
    inf = info(tensor)
    if not inf["valid"]:
        return 10
    actual = inf.get("shape", [])
    if len(actual) != len(expected):
        return 11
    for i, (a, e) in enumerate(zip(actual, expected)):
        if int(a) != int(e):
            return 12 + i
    return 0
