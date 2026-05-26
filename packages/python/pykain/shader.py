"""Shader and compute module envelopes for Kain interop."""

from __future__ import annotations

from typing import Any

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
