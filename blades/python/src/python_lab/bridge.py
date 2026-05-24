from __future__ import annotations

import importlib.util
import json
from typing import Any

import fastmcp
import numpy as np


def _plan(plan_text: str) -> dict[str, Any]:
    return json.loads(plan_text)


def module_digest(plan_text: str) -> int:
    plan = _plan(plan_text)
    score = 0
    if importlib.util.find_spec("numpy"):
        score += int(plan.get("numpy_bonus", 101))
    if importlib.util.find_spec("fastmcp"):
        score += int(plan.get("fastmcp_bonus", 103))
    if importlib.util.find_spec("torch"):
        score += int(plan.get("torch_bonus", 107))
    return score


def grid_shape(plan_text: str) -> list[int]:
    plan = _plan(plan_text)
    return [int(plan.get("tensor_rows", 3)), int(plan.get("tensor_cols", 4))]


def make_numpy_grid(plan_text: str, salt: int) -> np.ndarray:
    plan = _plan(plan_text)
    rows, cols = grid_shape(plan_text)
    start = float(plan.get("numpy_start", -1.0))
    stop = float(plan.get("numpy_stop", 1.0))
    total = rows * cols
    base = np.linspace(start, stop, total, dtype=np.float32).reshape(rows, cols)
    return base + np.float32(salt)


def tensor_signature(values: Any) -> int:
    array = np.asarray(values, dtype=np.float64)
    return int(round(float(array.sum())))


def tensor_tail(values: Any) -> float:
    array = np.asarray(values)
    return float(array.reshape(-1)[-1])


def fastmcp_name(plan_text: str) -> str:
    plan = _plan(plan_text)
    app = fastmcp.FastMCP(str(plan.get("fastmcp_server_name", "kain-python-lab")))
    return str(app.name)
