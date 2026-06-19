"""Zero-friction pyglet host window adapter for pykain.window."""

from __future__ import annotations

import importlib.util
from typing import Any

import numpy as np


BACKEND_NAME = "pyglet"


def _int(plan: dict[str, Any], key: str, default: int) -> int:
    return int(plan.get(key, default))


def _bool(plan: dict[str, Any], key: str, default: bool) -> bool:
    raw = plan.get(key, default)
    if isinstance(raw, str):
        return raw.lower() in {"1", "true", "yes", "on"}
    return bool(raw)


def is_available(plan: dict[str, Any] | None = None) -> bool:
    return importlib.util.find_spec("pyglet") is not None


def backend_info(plan: dict[str, Any] | None = None) -> dict[str, Any]:
    if not is_available(plan):
        return {"driver": "pyglet", "available": False}
    import pyglet

    return {
        "driver": "pyglet",
        "available": True,
        "pyglet_version": getattr(pyglet, "version", ""),
    }


def open_window(plan: dict[str, Any]) -> dict[str, Any]:
    import pyglet

    width = _int(plan, "window_width", 640)
    height = _int(plan, "window_height", 360)
    title = str(plan.get("window_title", "Kain // pykain pyglet host"))
    hidden = _bool(plan, "window_hidden", True)
    resizable = _bool(plan, "window_resizable", False)
    window = pyglet.window.Window(
        width=width,
        height=height,
        caption=title,
        visible=not hidden,
        resizable=resizable,
    )
    window.switch_to()
    window.dispatch_events()
    return {
        "_pykain_handle": window,
        "backend": BACKEND_NAME,
        "driver": "pyglet",
        "width": width,
        "height": height,
        "hidden": hidden,
        "valid": True,
    }


def close_window(handle: Any, plan: dict[str, Any] | None = None) -> int:
    if handle is None:
        return 0
    try:
        handle.dispatch_events()
    except Exception:
        pass
    try:
        handle.close()
    except Exception:
        pass
    return 0


def capture_window(handle: Any, plan: dict[str, Any] | None = None) -> np.ndarray:
    plan = plan or {}
    width = _int(plan, "window_width", getattr(handle, "width", 1))
    height = _int(plan, "window_height", getattr(handle, "height", 1))
    raster = np.zeros((height, width, 4), dtype=np.uint8)
    raster[:, :, 0] = 12
    raster[:, :, 1] = 24
    raster[:, :, 2] = 40
    raster[:, :, 3] = 255
    return np.ascontiguousarray(raster)
