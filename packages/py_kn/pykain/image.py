"""pykain.image — normalized image/raster crossing for Kain↔Python.

This lane stays deliberately boring: it produces normalized numpy rasters that
Kain can treat as shared images. Window hosting lives in pykain.window via
external adapters, so image generation is not tied to a specific UI toolkit.
"""

from __future__ import annotations

import json
from typing import Any

import numpy as np

from . import _native_loader


def _plan(plan_text: str) -> dict[str, Any]:
    return json.loads(plan_text)


def _int(payload: dict[str, Any], key: str, default: int) -> int:
    return int(payload.get(key, default))


def _clamp8(value: int) -> int:
    return max(0, min(255, int(value)))


# ═══════════════════════════════════════════════════════════════════════════
#  Render — backend-agnostic "give me a frame"
# ═══════════════════════════════════════════════════════════════════════════

def render(plan_text: str, state_text: str = "", backend: str = "auto") -> Any:
    """Render a normalized raster host object.

    `backend` stays in the signature for forward compatibility, but today the
    image lane intentionally returns a numpy-backed raster no matter what.
    """
    plan = _plan(plan_text)
    if backend not in {"auto", "numpy"}:
        raise ValueError(f"unsupported image backend '{backend}'")
    return _render_numpy(plan, state_text)


def render_signature(plan_text: str, state_text: str = "", backend: str = "auto") -> int:
    return signature(render(plan_text, state_text, backend))


def render_byte_length(plan_text: str, state_text: str = "", backend: str = "auto") -> int:
    return int(info(render(plan_text, state_text, backend)).get("byte_length", 0))


def render_contract(plan_text: str, state_text: str = "", backend: str = "auto") -> int:
    """Return 0 when the generated raster matches the plan-level image contract."""
    try:
        plan = _plan(plan_text)
        expected_w = _int(plan, "image_width", 96)
        expected_h = _int(plan, "image_height", 72)
        expected_c = _int(plan, "image_channels", 3)
        meta = info(render(plan_text, state_text, backend))
        if not meta.get("valid", False):
            return 1
        if int(meta.get("width", 0)) != expected_w:
            return 2
        if int(meta.get("height", 0)) != expected_h:
            return 3
        if int(meta.get("channels", 0)) != expected_c:
            return 4
        if str(meta.get("layout", "")) != "HWC":
            return 5
        if int(meta.get("byte_length", 0)) != expected_w * expected_h * expected_c:
            return 6
        if str(meta.get("contract", "")) != "kain.shared.image":
            return 7
        return 0
    except Exception:
        return 9


def render_ok(plan_text: str, state_text: str = "", backend: str = "auto") -> bool:
    """Bool-shaped Kain-facing contract probe for current host-object semantics."""
    return render_contract(plan_text, state_text=state_text, backend=backend) == 0


def _render_numpy(plan: dict[str, Any], state_text: str) -> np.ndarray:
    """Generate a plain normalized raster with a tiny diagnostic overlay."""
    width = _int(plan, "image_width", 96)
    height = _int(plan, "image_height", 72)
    channels = _int(plan, "image_channels", 3)
    clear = plan.get("clear_color", [12, 18, 28])
    raster = np.zeros((height, width, channels), dtype=np.uint8)
    for c in range(min(channels, 3)):
        raster[:, :, c] = _clamp8(clear[c])

    if state_text:
        state = json.loads(state_text)
        accent = int(state.get("accent", 90)) & 255
        hot = np.array(
            [_clamp8(accent + 48), 56, _clamp8(255 - accent // 2)],
            dtype=np.uint8,
        )
        cool = np.array(
            [32, _clamp8(80 + accent // 3), _clamp8(140 + accent // 4)],
            dtype=np.uint8,
        )

        rect_h = max(8, height // 3)
        rect_w = max(8, width // 2)
        raster[4 : min(height, 4 + rect_h), 4 : min(width, 4 + rect_w), : min(channels, 3)] = hot[: min(channels, 3)]

        cx = max(8, width - 18)
        cy = max(8, height // 2)
        radius = max(6, min(width, height) // 5)
        yy, xx = np.ogrid[:height, :width]
        mask = ((xx - cx) * (xx - cx)) + ((yy - cy) * (yy - cy)) <= radius * radius
        for channel in range(min(channels, 3)):
            raster[:, :, channel][mask] = cool[channel]

    return np.ascontiguousarray(raster)


# ═══════════════════════════════════════════════════════════════════════════
#  Info — structured metadata
# ═══════════════════════════════════════════════════════════════════════════

def info(image: Any) -> dict[str, Any]:
    """Return normalized image metadata as a dict."""
    try:
        native_info = _native_loader.inspect_object(image, kind="image")
        arr = np.asarray(image)
        shape = arr.shape
        if len(shape) == 2:
            h, w = shape
            c = 1
        elif len(shape) == 3:
            h, w, c = shape
        else:
            return {"valid": False, "error": f"unexpected ndim={len(shape)}"}

        layout = "HWC" if len(shape) == 3 and shape[2] <= 4 else "HW"
        result: dict[str, Any] = native_info if native_info.get("valid") else {"valid": False}
        result.update({
            "width": int(w),
            "height": int(h),
            "channels": int(c),
            "layout": layout,
            "dtype": str(arr.dtype),
            "element_type": str(arr.dtype),
            "element_size": int(arr.dtype.itemsize),
            "shape": list(shape),
            "byte_length": int(arr.nbytes),
            "element_count": int(arr.size),
            "row_stride": int(arr.strides[0]) if len(shape) >= 2 else 0,
            "is_contiguous": bool(arr.flags["C_CONTIGUOUS"]),
            "backend": "numpy",
            "source_runtime": "python",
            "source_backend": "numpy",
            "ownership": "python-borrowed",
            "contract": "kain.shared.image",
            "contract_version": 1,
            "valid": True,
        })
        return result
    except Exception as exc:
        result: dict[str, Any] = {"valid": False}
        result["error"] = str(exc)
        return result


def info_json(image: Any) -> str:
    return json.dumps(info(image), default=str, separators=(",", ":"))


def signature(image: Any) -> int:
    """Checksum for verification."""
    try:
        return _native_loader.signature(image)
    except Exception:
        return int(np.asarray(image, dtype=np.int64).sum())


# ═══════════════════════════════════════════════════════════════════════════
#  Validation
# ═══════════════════════════════════════════════════════════════════════════

def validate(image: Any, expected_width: int = 0, expected_height: int = 0,
             expected_channels: int = 0, expected_layout: str = "") -> dict[str, Any]:
    """Full validation with optional expected params."""
    result = info(image)
    if expected_width > 0 and result.get("width", 0) != expected_width:
        result["valid"] = False
        result["error"] = f"width mismatch: got {result.get('width')} expected {expected_width}"
    if expected_height > 0 and result.get("height", 0) != expected_height:
        result["valid"] = False
        result["error"] = f"height mismatch: got {result.get('height')} expected {expected_height}"
    if expected_channels > 0 and result.get("channels", 0) != expected_channels:
        result["valid"] = False
        result["error"] = f"channels mismatch: got {result.get('channels')} expected {expected_channels}"
    if expected_layout and result.get("layout", "") != expected_layout:
        result["valid"] = False
        result["error"] = f"layout mismatch: got {result.get('layout')} expected {expected_layout}"
    return result
