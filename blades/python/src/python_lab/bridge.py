from __future__ import annotations

import importlib.util
import json
import os
from typing import Any

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import fastmcp
import numpy as np
import pygame
import torch
import z3

_PYGAME_READY = False
_PYGAME_SURFACE: pygame.Surface | None = None


def _plan(plan_text: str) -> dict[str, Any]:
    return json.loads(plan_text)


def _module_available(name: str) -> bool:
    return importlib.util.find_spec(name) is not None


def _int(plan: dict[str, Any], key: str, default: int) -> int:
    return int(plan.get(key, default))


def _float(plan: dict[str, Any], key: str, default: float) -> float:
    return float(plan.get(key, default))


def _bool(plan: dict[str, Any], key: str, default: bool) -> bool:
    return bool(plan.get(key, default))


def _clamp8(value: int) -> int:
    return max(0, min(255, int(value)))


def _pygame_bias(plan: dict[str, Any]) -> int:
    return _int(plan, "teleport_bias", 5) + _int(plan, "teleport_phase", 11)


def module_digest(plan_text: str) -> int:
    plan = _plan(plan_text)
    score = 0
    if _module_available("numpy"):
        score += _int(plan, "numpy_bonus", 101)
    if _module_available("fastmcp"):
        score += _int(plan, "fastmcp_bonus", 103)
    if _module_available("torch"):
        score += _int(plan, "torch_bonus", 107)
    if _module_available("z3"):
        score += _int(plan, "z3_bonus", 109)
    if _module_available("pygame"):
        score += _int(plan, "pygame_bonus", 113)
    return score


def grid_shape(plan_text: str) -> list[int]:
    plan = _plan(plan_text)
    return [_int(plan, "tensor_rows", 3), _int(plan, "tensor_cols", 4)]


def make_numpy_grid(plan_text: str, salt: int) -> np.ndarray:
    plan = _plan(plan_text)
    rows, cols = grid_shape(plan_text)
    start = _float(plan, "numpy_start", -1.0)
    stop = _float(plan, "numpy_stop", 1.0)
    total = rows * cols
    base = np.linspace(start, stop, total, dtype=np.float32).reshape(rows, cols)
    return base + np.float32(salt)


def make_torch_grid(plan_text: str, salt: int) -> torch.Tensor:
    plan = _plan(plan_text)
    rows, cols = grid_shape(plan_text)
    total = rows * cols
    base = torch.arange(0, total, dtype=torch.float32).reshape(rows, cols)
    scale = _float(plan, "torch_scale", 1.0)
    return (base * scale) + float(salt)


def tensor_signature(values: Any) -> int:
    if isinstance(values, torch.Tensor):
        return int(round(float(values.detach().cpu().sum().item())))
    array = np.asarray(values, dtype=np.float64)
    return int(round(float(array.sum())))


def tensor_tail(values: Any) -> float:
    if isinstance(values, torch.Tensor):
        flat = values.detach().cpu().reshape(-1)
        return float(flat[-1].item())
    array = np.asarray(values)
    return float(array.reshape(-1)[-1])


def fastmcp_name(plan_text: str) -> str:
    plan = _plan(plan_text)
    app = fastmcp.FastMCP(str(plan.get("fastmcp_server_name", "kain-python-lab")))
    return str(app.name)


def solve_lane_plan(plan_text: str, bias: int) -> str:
    plan = _plan(plan_text)
    route_start = _int(plan, "solver_route_start", 9) + (int(bias) % 4)
    route_stride = _int(plan, "solver_route_stride", 3)
    route_curve = _int(plan, "solver_route_curve", 1)

    expected = [
        route_start,
        route_start + route_stride,
        route_start + route_stride * 2 + route_curve,
        route_start + route_stride * 3 + route_curve,
    ]
    goal = sum((index + 1) * value for index, value in enumerate(expected))

    route = [z3.Int(f"route_{index}") for index in range(4)]
    solver = z3.Solver()
    solver.add(route[0] >= _int(plan, "solver_min", 1))
    solver.add(route[3] <= _int(plan, "solver_max", 256))
    solver.add(route[1] - route[0] == route_stride)
    solver.add(route[2] - route[1] == route_stride + route_curve)
    solver.add(route[3] - route[2] == route_stride)
    solver.add(route[0] < route[1], route[1] < route[2], route[2] < route[3])
    solver.add(sum((index + 1) * route[index] for index in range(4)) == goal)
    assert solver.check() == z3.sat
    model = solver.model()
    solved = [int(model.evaluate(item).as_long()) for item in route]
    checksum = sum((index + 7) * value for index, value in enumerate(solved)) + int(bias)
    payload = {
        "goal": goal,
        "route": solved,
        "stride": route_stride,
        "curve": route_curve,
        "checksum": checksum,
    }
    return json.dumps(payload)


def solve_lane_plan_default(plan_text: str) -> str:
    return solve_lane_plan(plan_text, 29)


def _ensure_pygame(plan: dict[str, Any]) -> None:
    global _PYGAME_READY
    if _PYGAME_READY:
        return
    os.environ.setdefault("SDL_VIDEODRIVER", str(plan.get("pygame_driver", "dummy")))
    pygame.display.init()
    pygame.font.init()
    _PYGAME_READY = True


def _surface(plan_text: str, bias: int) -> pygame.Surface:
    global _PYGAME_SURFACE
    plan = _plan(plan_text)
    _ensure_pygame(plan)
    width = _int(plan, "pygame_width", 96)
    height = _int(plan, "pygame_height", 72)
    use_display = _bool(plan, "pygame_use_display", True)
    hidden = _bool(plan, "pygame_hidden", True)

    if use_display:
        flags = getattr(pygame, "HIDDEN", 0) if hidden else 0
        surface = pygame.display.get_surface()
        if surface is None or surface.get_width() != width or surface.get_height() != height:
            surface = pygame.display.set_mode((width, height), flags)
        _PYGAME_SURFACE = surface
    elif _PYGAME_SURFACE is None or _PYGAME_SURFACE.get_width() != width or _PYGAME_SURFACE.get_height() != height:
        _PYGAME_SURFACE = pygame.Surface((width, height), depth=32)

    assert _PYGAME_SURFACE is not None
    surface = _PYGAME_SURFACE
    surface.fill(tuple(_clamp8(value) for value in plan.get("pygame_clear", [12, 18, 28])))

    accent = _clamp8((_int(plan, "pygame_accent_scale", 17) * int(bias)) % 255)
    hot = (_clamp8(accent + 48), 56, _clamp8(255 - accent // 2))
    cool = (32, _clamp8(80 + accent // 3), _clamp8(140 + accent // 4))
    glow = (_clamp8(200 - accent // 2), _clamp8(40 + accent), 220)

    pygame.draw.rect(surface, hot, pygame.Rect(4, 4, max(8, width // 2), max(8, height // 3)))
    pygame.draw.circle(
        surface,
        cool,
        (max(8, width - 18), max(8, height // 2)),
        max(6, min(width, height) // 5),
    )
    pygame.draw.line(surface, glow, (0, height - 1), (width - 1, 0), 3)
    pygame.draw.line(surface, (220, 220, 255), (width // 4, 0), (width - 1, height - 1), 2)
    pygame.event.pump()
    if use_display:
        pygame.display.flip()
    return surface


def make_pygame_pixel_view(plan_text: str, bias: int) -> np.ndarray:
    surface = _surface(plan_text, bias)
    return pygame.surfarray.pixels3d(surface)


def make_pygame_pixel_view_default(plan_text: str) -> np.ndarray:
    plan = _plan(plan_text)
    return make_pygame_pixel_view(plan_text, _pygame_bias(plan))


def pygame_surface_signature(plan_text: str, bias: int) -> int:
    surface = _surface(plan_text, bias)
    return int(np.asarray(pygame.surfarray.array3d(surface), dtype=np.int64).sum())


def pygame_surface_signature_default(plan_text: str) -> int:
    plan = _plan(plan_text)
    return pygame_surface_signature(plan_text, _pygame_bias(plan))


def pygame_surface_pixel(plan_text: str, bias: int, x: int, y: int) -> list[int]:
    surface = _surface(plan_text, bias)
    color = surface.get_at((int(x), int(y)))
    return [int(color.r), int(color.g), int(color.b)]


def pygame_driver_name(plan_text: str) -> str:
    plan = _plan(plan_text)
    _ensure_pygame(plan)
    return str(pygame.display.get_driver())
