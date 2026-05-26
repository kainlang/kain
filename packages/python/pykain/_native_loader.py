"""Lazy access to pykain's C acceleration layer."""

from __future__ import annotations

from typing import Any

_NATIVE = None
_NATIVE_ERROR: BaseException | None = None


def _load_native():
    global _NATIVE, _NATIVE_ERROR
    if _NATIVE is not None:
        return _NATIVE
    if _NATIVE_ERROR is not None:
        return None
    try:
        from . import _native as native_mod
    except BaseException as exc:  # pragma: no cover - depends on local build state
        _NATIVE_ERROR = exc
        return None
    _NATIVE = native_mod
    return _NATIVE


def native_available() -> bool:
    return _load_native() is not None


def native_error() -> str:
    return "" if _NATIVE_ERROR is None else str(_NATIVE_ERROR)


def native_version() -> str:
    native = _load_native()
    if native is None:
        return "python-fallback"
    return str(native.native_version())


def inspect_object(obj: Any, kind: str = "auto") -> dict[str, Any]:
    native = _load_native()
    if native is not None:
        return dict(native.inspect(obj, kind=kind))
    return _fallback_inspect(obj, kind)


def signature(obj: Any) -> int:
    native = _load_native()
    if native is not None:
        return int(native.signature(obj))
    return sum(as_bytes(obj)) % 1000000007


def as_bytes(obj: Any) -> bytes:
    native = _load_native()
    if native is not None:
        return bytes(native.as_bytes(obj))
    try:
        return memoryview(obj).tobytes()
    except TypeError:
        return bytes(obj)


def _fallback_inspect(obj: Any, kind: str) -> dict[str, Any]:
    try:
        view = memoryview(obj)
        shape = list(view.shape or [len(view)])
        strides = list(view.strides or [view.itemsize])
        readonly = bool(view.readonly)
        return {
            "valid": True,
            "kind": kind,
            "backend": _backend_name(obj),
            "python_type": type(obj).__name__,
            "source_runtime": "python",
            "ownership": "python-borrowed",
            "contract": "kain.shared.buffer",
            "contract_version": 1,
            "element_type": _format_to_type(view.format, view.itemsize),
            "dtype": _format_to_type(view.format, view.itemsize),
            "format": view.format,
            "element_size": int(view.itemsize),
            "byte_length": int(view.nbytes),
            "element_count": _element_count(shape),
            "shape": shape,
            "strides": strides,
            "is_contiguous": bool(view.contiguous),
            "is_writeable": not readonly,
            "readonly": readonly,
            "pointer_available": False,
        }
    except TypeError:
        return {
            "valid": False,
            "kind": kind,
            "backend": _backend_name(obj),
            "python_type": type(obj).__name__,
            "source_runtime": "python",
            "ownership": "python-host-object",
            "contract": "pykain.host.object",
            "pointer_available": False,
            "error": "object does not expose the buffer protocol",
        }


def _backend_name(obj: Any) -> str:
    module = type(obj).__module__
    if module.startswith("numpy"):
        return "numpy"
    if module.startswith("torch"):
        return "torch"
    if module.startswith("PIL"):
        return "pillow"
    if module.startswith("cv2"):
        return "opencv"
    return "python"


def _format_to_type(fmt: str | None, itemsize: int) -> str:
    table = {
        "B": "uint8",
        "b": "int8",
        "H": "uint16",
        "h": "int16",
        "I": "uint32",
        "i": "int32",
        "L": "uint32",
        "l": "int32",
        "Q": "uint64",
        "q": "int64",
        "f": "float32",
        "d": "float64",
        "?": "bool",
    }
    if fmt in table:
        return table[fmt]
    return "uint8" if itemsize == 1 else (fmt or "bytes")


def _element_count(shape: list[int]) -> int:
    count = 1
    for value in shape:
        count *= int(value)
    return count
