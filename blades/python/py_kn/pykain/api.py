"""Zero-friction object adaptation for pykain."""

from __future__ import annotations

import json
from typing import Any

from . import _native_loader
from .contracts import KainBuffer, KainEnvelope, KainImage, KainTensor


def adapt(obj: Any, kind: str = "auto", **labels: Any) -> KainEnvelope:
    info = _native_loader.inspect_object(obj, kind=kind)
    resolved = _infer_kind(info, kind)
    info["kind"] = resolved
    if resolved == "image":
        return KainImage(source=obj, info=info, labels=labels)
    if resolved == "tensor":
        return KainTensor(source=obj, info=info, labels=labels)
    return KainBuffer(source=obj, info=info, labels=labels)


def as_buffer(obj: Any, **labels: Any) -> KainBuffer:
    envelope = adapt(obj, kind="buffer", **labels)
    if isinstance(envelope, KainBuffer):
        return envelope
    return KainBuffer(source=obj, info=envelope.info, labels=envelope.labels)


def as_tensor(obj: Any, **labels: Any) -> KainTensor:
    envelope = adapt(obj, kind="tensor", **labels)
    if isinstance(envelope, KainTensor):
        return envelope
    return KainTensor(source=obj, info=envelope.info, labels=envelope.labels)


def as_image(obj: Any, **labels: Any) -> KainImage:
    envelope = adapt(obj, kind="image", **labels)
    if isinstance(envelope, KainImage):
        return envelope
    return KainImage(source=obj, info=envelope.info, labels=envelope.labels)


def inspect(obj: Any, kind: str = "auto") -> dict[str, Any]:
    return _native_loader.inspect_object(obj, kind=kind)


def inspect_json(obj: Any, kind: str = "auto") -> str:
    return json.dumps(inspect(obj, kind=kind), default=str, separators=(",", ":"))


def signature(obj: Any) -> int:
    return _native_loader.signature(obj)


def as_bytes(obj: Any) -> bytes:
    return _native_loader.as_bytes(obj)


def native_available() -> bool:
    return _native_loader.native_available()


def native_version() -> str:
    return _native_loader.native_version()


def smoke_score() -> int:
    import numpy as np

    arr = np.arange(12, dtype=np.uint8).reshape(3, 4)
    buf = as_buffer(arr)
    gpu = __import__("pykain").gpu.compute_buffer(arr)
    shader = __import__("pykain").shader.compute("shader compute Pykain(id: UVec3) -> Vec4: return vec4(1.0, 1.0, 1.0, 1.0)")
    world = __import__("pykain").world.ref("PykainAuthority", {"score": 17})
    score = 0
    score += int(native_available())
    score += int(buf.info.get("byte_length", 0))
    score += int(gpu.info.get("byte_length", 0))
    score += int(shader.info.get("byte_length", 0))
    score += int(world.state.get("score", 0))
    return score


def _infer_kind(info: dict[str, Any], requested: str) -> str:
    if requested != "auto":
        return requested
    shape = info.get("shape") or []
    backend = str(info.get("backend", ""))
    dtype = str(info.get("dtype", info.get("element_type", "")))
    if len(shape) == 3 and int(shape[-1]) in (1, 2, 3, 4):
        return "image"
    if backend in {"torch", "numpy"} and dtype not in {"uint8", "bytes"}:
        return "tensor"
    return "buffer"
