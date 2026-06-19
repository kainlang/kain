"""Shader and compute module envelopes for Kain interop.

This module has two jobs:
1. carry Kain-authored shader metadata through Python without losing shape;
2. execute a small fragment/readback lane immediately for script-level tests.

If a future adapter exposes SPIR-V/Vulkan directly, it can slot behind the same
`render_fragment()` contract without changing Kain scripts.
"""

from __future__ import annotations

import math
from typing import Any

import numpy as np

from .contracts import KainShaderModule
from ._native_loader import signature


def module(
    payload: str | bytes | bytearray,
    *,
    stage: str = "compute",
    entry_point: str = "main",
    source_kind: str = "auto",
    bindings: list[dict[str, Any]] | None = None,
    uniforms: dict[str, Any] | None = None,
    label: str = "pykain.shader",
) -> KainShaderModule:
    if isinstance(payload, str):
        raw = payload.encode("utf-8")
        resolved_source = "source" if source_kind == "auto" else source_kind
    else:
        raw = bytes(payload)
        resolved_source = "bytecode" if source_kind == "auto" else source_kind

    info = {
        "label": label,
        "source_kind": resolved_source,
        "byte_length": len(raw),
        "signature": signature(raw),
        "contract": "kain.shader.module",
        "contract_version": 1,
    }
    return KainShaderModule(
        source=payload,
        info=info,
        stage=stage,
        entry_point=entry_point,
        bindings=list(bindings or []),
        uniforms=dict(uniforms or {}),
    )


def compute(payload: str | bytes | bytearray, **kwargs: Any) -> KainShaderModule:
    return module(payload, stage="compute", **kwargs)


def fragment(payload: str | bytes | bytearray, **kwargs: Any) -> KainShaderModule:
    return module(payload, stage="fragment", **kwargs)


def _shader_bytes(shader: KainShaderModule | str | bytes | bytearray) -> bytes:
    if isinstance(shader, KainShaderModule):
        source = shader.source
    else:
        source = shader
    if isinstance(source, str):
        return source.encode("utf-8")
    return bytes(source)


def _float(payload: dict[str, Any], key: str, default: float) -> float:
    return float(payload.get(key, default))


def _int(payload: dict[str, Any], key: str, default: int) -> int:
    return int(payload.get(key, default))


def _uniforms(shader: KainShaderModule | str | bytes | bytearray, uniforms: dict[str, Any] | None) -> dict[str, Any]:
    merged: dict[str, Any] = {}
    if isinstance(shader, KainShaderModule):
        merged.update(shader.uniforms)
    if uniforms:
        merged.update(uniforms)
    return merged


def _accent(uniforms: dict[str, Any]) -> tuple[float, float, float]:
    raw = uniforms.get("accent", (0.15, 0.85, 1.0))
    if isinstance(raw, str):
        parts = [float(part.strip()) for part in raw.split(",") if part.strip()]
        raw = parts
    if isinstance(raw, (list, tuple)) and len(raw) >= 3:
        return (float(raw[0]), float(raw[1]), float(raw[2]))
    return (0.15, 0.85, 1.0)


def _render_cpu(shader: KainShaderModule | str | bytes | bytearray, width: int, height: int,
                uniforms: dict[str, Any]) -> np.ndarray:
    """Execute the portable fallback fragment lane.

    It intentionally mirrors Kain's usual fragment-shader mental model: UV in,
    Vec4 out, contiguous RGBA8 readback. The source signature perturbs the field
    so Kain-authored text still affects the produced frame even before a native
    SPIR-V adapter is present.
    """
    phase = _float(uniforms, "phase", 0.0)
    energy = _float(uniforms, "energy", 0.0)
    accent = _accent(uniforms)
    digest = signature(_shader_bytes(shader)) & 1023
    yy, xx = np.mgrid[0:height, 0:width]
    uvx = xx.astype(np.float32) / max(1, width - 1)
    uvy = yy.astype(np.float32) / max(1, height - 1)
    cx = uvx - 0.5
    cy = uvy - 0.5
    radial = cx * cx + cy * cy
    wave_x = uvx * (1.0 - uvx)
    wave_y = uvy * (1.0 - uvy)
    pulse = math.sin((phase * 0.07) + (digest * 0.003)) * 0.15
    lane = ((wave_x + wave_y) * 2.0) - (radial * (0.58 + pulse)) + (energy * 0.00001)
    raster = np.empty((height, width, 4), dtype=np.uint8)
    raster[:, :, 0] = np.clip((accent[0] * (0.28 + lane)) * 255.0, 0, 255).astype(np.uint8)
    raster[:, :, 1] = np.clip((accent[1] * (0.18 + wave_x + phase * 0.002)) * 255.0, 0, 255).astype(np.uint8)
    raster[:, :, 2] = np.clip((accent[2] * (0.20 + wave_y + radial * 0.25)) * 255.0, 0, 255).astype(np.uint8)
    raster[:, :, 3] = 255
    return np.ascontiguousarray(raster)


def render_fragment(
    shader: KainShaderModule | str | bytes | bytearray,
    width: int = 192,
    height: int = 108,
    uniforms: dict[str, Any] | None = None,
    backend: str = "auto",
) -> np.ndarray:
    """Run a fragment shader contract and return a contiguous RGBA8 image.

    `backend="auto"` currently selects the portable CPU executor. GPU adapters
    can be registered later without changing this Kain-facing return contract.
    """
    if backend not in {"auto", "cpu", "numpy"}:
        raise ValueError(f"unsupported shader backend '{backend}'")
    return _render_cpu(shader, int(width), int(height), _uniforms(shader, uniforms))


def render_info(image: Any) -> dict[str, Any]:
    arr = np.asarray(image)
    return {
        "valid": arr.ndim == 3 and arr.shape[2] == 4 and arr.dtype == np.uint8,
        "width": int(arr.shape[1]) if arr.ndim >= 2 else 0,
        "height": int(arr.shape[0]) if arr.ndim >= 2 else 0,
        "channels": int(arr.shape[2]) if arr.ndim == 3 else 0,
        "layout": "HWC",
        "dtype": str(arr.dtype),
        "byte_length": int(arr.nbytes),
        "backend": "cpu",
        "contract": "kain.shader.rgba8.readback",
        "signature": signature(arr),
    }


def render_ok(
    payload: str | bytes | bytearray,
    width: int = 32,
    height: int = 18,
    backend: str = "auto",
) -> bool:
    try:
        image = render_fragment(fragment(payload), width=width, height=height, backend=backend)
        info = render_info(image)
        return bool(info["valid"]) and int(info["byte_length"]) == int(width) * int(height) * 4
    except Exception:
        return False
