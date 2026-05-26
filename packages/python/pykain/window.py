"""pykain.window — normalized window hosting for Kain↔Python.

This module deliberately does NOT pick a favorite Python GUI stack. Instead it
loads adapter modules from plan/env configuration and lets those adapters speak
for pygame, pyglet, Qt, tkinter, glfw, DearPyGui, or anything else that can
own a host window.

Adapter contract:
    backend_name() -> str                          # optional
    is_available(plan: dict) -> bool               # required
    open_window(plan: dict) -> Any                 # required
    close_window(handle, plan: dict) -> int        # required
    capture_window(handle, plan: dict) -> Any      # optional
    backend_info(plan: dict) -> dict               # optional

The public pykain API stays stable even if the adapter implementation later
moves into a compiled extension.
"""

from __future__ import annotations

from dataclasses import dataclass
import importlib
import inspect
import json
import os
from types import ModuleType
from typing import Any, Callable


@dataclass
class _AdapterRecord:
    name: str
    source: str
    availability: Callable[[dict[str, Any]], bool]
    open_window: Callable[..., Any]
    close_window: Callable[..., Any]
    capture_window: Callable[..., Any] | None = None
    info: Callable[..., dict[str, Any]] | None = None


_REGISTERED_ADAPTERS: dict[str, _AdapterRecord] = {}
_IMPORTED_ADAPTERS: dict[str, _AdapterRecord] = {}
_ACTIVE_ADAPTER: _AdapterRecord | None = None
_ACTIVE_HANDLE: Any = None
_ACTIVE_PLAN: dict[str, Any] = {}


def _plan(plan_text: str) -> dict[str, Any]:
    if not plan_text:
        return {}
    return json.loads(plan_text)


def _int(payload: dict[str, Any], key: str, default: int) -> int:
    return int(payload.get(key, default))


def _str(payload: dict[str, Any], key: str, default: str) -> str:
    return str(payload.get(key, default))


def _resolve_member(target: Any, name: str, default: Any = None) -> Any:
    if isinstance(target, dict):
        return target.get(name, default)
    return getattr(target, name, default)


def _call_adapter(func: Callable[..., Any], *args: Any) -> Any:
    try:
        signature = inspect.signature(func)
        params = list(signature.parameters.values())
        if any(param.kind == inspect.Parameter.VAR_POSITIONAL for param in params):
            return func(*args)
        positional = [
            param
            for param in params
            if param.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD)
        ]
        return func(*args[: len(positional)])
    except (TypeError, ValueError):
        return func(*args)


def _record_from_target(source: str, target: Any) -> _AdapterRecord:
    raw_name = _resolve_member(target, "BACKEND_NAME")
    if raw_name is None:
        raw_name = _resolve_member(target, "backend_name")
    if callable(raw_name):
        raw_name = raw_name()
    name = str(raw_name or source.rsplit(".", 1)[-1])

    availability = _resolve_member(target, "is_available")
    open_window = _resolve_member(target, "open_window")
    close_window = _resolve_member(target, "close_window")
    capture_window = _resolve_member(target, "capture_window")
    info = _resolve_member(target, "backend_info")

    if not callable(availability):
        raise ValueError(f"adapter '{source}' is missing is_available(plan)")
    if not callable(open_window):
        raise ValueError(f"adapter '{source}' is missing open_window(plan)")
    if not callable(close_window):
        raise ValueError(f"adapter '{source}' is missing close_window(handle, plan)")

    return _AdapterRecord(
        name=name,
        source=source,
        availability=availability,
        open_window=open_window,
        close_window=close_window,
        capture_window=capture_window if callable(capture_window) else None,
        info=info if callable(info) else None,
    )


def register_adapter(source: str, adapter: ModuleType | dict[str, Any] | Any) -> str:
    """Register an adapter object or module under a stable source key."""
    record = _record_from_target(source, adapter)
    _REGISTERED_ADAPTERS[source] = record
    return record.name


def _load_adapter(source: str) -> _AdapterRecord:
    if source in _REGISTERED_ADAPTERS:
        return _REGISTERED_ADAPTERS[source]
    if source in _IMPORTED_ADAPTERS:
        return _IMPORTED_ADAPTERS[source]

    module = importlib.import_module(source)
    target = getattr(module, "PYKAIN_WINDOW_ADAPTER", module)
    record = _record_from_target(source, target)
    _IMPORTED_ADAPTERS[source] = record
    return record


def _coerce_specs(raw_specs: Any) -> list[str]:
    if raw_specs is None:
        return []
    if isinstance(raw_specs, str):
        return [part.strip() for part in raw_specs.split(",") if part.strip()]
    if not isinstance(raw_specs, list):
        return []

    specs: list[str] = []
    for item in raw_specs:
        if isinstance(item, str) and item.strip():
            specs.append(item.strip())
    return specs


def _adapter_specs(plan: dict[str, Any]) -> list[str]:
    specs = _coerce_specs(plan.get("window_adapters"))
    specs.extend(_coerce_specs(os.environ.get("PYKAIN_WINDOW_ADAPTERS", "")))

    seen: set[str] = set()
    ordered: list[str] = []
    for spec in specs:
        if spec not in seen:
            seen.add(spec)
            ordered.append(spec)
    return ordered


def _resolve_records(plan: dict[str, Any]) -> tuple[list[_AdapterRecord], list[str]]:
    records: list[_AdapterRecord] = []
    errors: list[str] = []
    ordered_specs = _adapter_specs(plan)

    for source in ordered_specs:
        try:
            records.append(_load_adapter(source))
        except Exception as exc:
            errors.append(f"{source}: {exc}")

    for source, record in _REGISTERED_ADAPTERS.items():
        if source not in ordered_specs:
            records.append(record)

    return records, errors


def _available_records(records: list[_AdapterRecord], plan: dict[str, Any]) -> list[_AdapterRecord]:
    available: list[_AdapterRecord] = []
    for record in records:
        try:
            if bool(_call_adapter(record.availability, plan)):
                available.append(record)
        except Exception:
            continue
    return available


def _match_requested(records: list[_AdapterRecord], requested: str) -> _AdapterRecord | None:
    for record in records:
        if record.name == requested or record.source == requested:
            return record
    return None


def _info_payload(record: _AdapterRecord, plan: dict[str, Any], active: bool) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "backend": record.name,
        "source": record.source,
        "active": active,
        "valid": True,
    }
    if record.info is not None:
        try:
            extra = _call_adapter(record.info, plan)
            if isinstance(extra, dict):
                payload.update(extra)
        except Exception as exc:
            payload["info_error"] = str(exc)
    return payload


# ═══════════════════════════════════════════════════════════════════════════
#  Backend detection
# ═══════════════════════════════════════════════════════════════════════════

def detect_backend(plan_text: str = "") -> str:
    """Detect the best available configured adapter, or 'none'."""
    plan = _plan(plan_text)
    records, _errors = _resolve_records(plan)
    available = _available_records(records, plan)
    if available:
        return available[0].name
    return "none"


def backend_info(plan_text: str = "") -> dict[str, Any]:
    """Return info about the active or best available configured adapter."""
    plan = _plan(plan_text)
    records, errors = _resolve_records(plan)
    available = _available_records(records, plan)

    if _ACTIVE_ADAPTER is not None:
        payload = _info_payload(_ACTIVE_ADAPTER, _ACTIVE_PLAN or plan, active=True)
        payload["configured_adapters"] = [record.name for record in records]
        payload["available_adapters"] = [record.name for record in available]
        if errors:
            payload["adapter_errors"] = errors
        return payload

    if available:
        payload = _info_payload(available[0], plan, active=False)
        payload["configured_adapters"] = [record.name for record in records]
        payload["available_adapters"] = [record.name for record in available]
        if errors:
            payload["adapter_errors"] = errors
        return payload

    return {
        "backend": "none",
        "valid": False,
        "configured_adapters": [record.name for record in records],
        "available_adapters": [],
        "adapter_errors": errors,
        "error": "no available window adapter",
    }


# ═══════════════════════════════════════════════════════════════════════════
#  Window lifecycle
# ═══════════════════════════════════════════════════════════════════════════

def open(plan_text: str) -> dict[str, Any]:
    """Open a window using the configured adapter.

    Plan keys:
        window_backend: "auto" or adapter name/module path
        window_adapters: ["package.module.adapter", ...]
        window_width / window_height / window_hidden / window_title / window_options
    """
    global _ACTIVE_ADAPTER, _ACTIVE_HANDLE, _ACTIVE_PLAN

    plan = _plan(plan_text)
    requested = _str(plan, "window_backend", "auto")
    width = _int(plan, "window_width", 320)
    height = _int(plan, "window_height", 200)
    records, errors = _resolve_records(plan)
    available = _available_records(records, plan)

    if requested == "auto":
        record = available[0] if available else None
    else:
        record = _match_requested(records, requested)
        if record is not None and record not in available:
            return {
                "backend": requested,
                "driver": "",
                "width": 0,
                "height": 0,
                "valid": False,
                "error": f"adapter '{requested}' is configured but unavailable",
                "configured_adapters": [item.name for item in records],
                "adapter_errors": errors,
            }

    if record is None:
        return {
            "backend": requested if requested != "auto" else "none",
            "driver": "",
            "width": 0,
            "height": 0,
            "valid": False,
            "error": "no available window adapter",
            "configured_adapters": [item.name for item in records],
            "adapter_errors": errors,
        }

    try:
        opened = _call_adapter(record.open_window, plan)
    except Exception as exc:
        return {
            "backend": record.name,
            "driver": "",
            "width": 0,
            "height": 0,
            "valid": False,
            "error": str(exc),
            "configured_adapters": [item.name for item in records],
            "adapter_errors": errors,
        }

    payload = _info_payload(record, plan, active=True)
    payload.update({
        "width": width,
        "height": height,
        "hidden": bool(plan.get("window_hidden", True)),
    })
    if isinstance(opened, dict):
        _ACTIVE_HANDLE = opened.get("_pykain_handle", opened.get("handle", opened))
        payload.update({key: value for key, value in opened.items() if key not in {"handle", "_pykain_handle"}})
    else:
        _ACTIVE_HANDLE = opened

    _ACTIVE_ADAPTER = record
    _ACTIVE_PLAN = dict(plan)
    payload["configured_adapters"] = [item.name for item in records]
    payload["available_adapters"] = [item.name for item in available]
    if errors:
        payload["adapter_errors"] = errors
    return payload


def close() -> int:
    """Close the active window and clean up. Returns 0 on success."""
    global _ACTIVE_ADAPTER, _ACTIVE_HANDLE, _ACTIVE_PLAN

    try:
        if _ACTIVE_ADAPTER is not None:
            result = _call_adapter(_ACTIVE_ADAPTER.close_window, _ACTIVE_HANDLE, _ACTIVE_PLAN)
            status = 0 if result is None else int(result)
        else:
            status = 0
    except Exception:
        status = 1

    _ACTIVE_ADAPTER = None
    _ACTIVE_HANDLE = None
    _ACTIVE_PLAN = {}
    return status


def is_open() -> bool:
    """Check if a window is currently open."""
    return _ACTIVE_ADAPTER is not None


# ═══════════════════════════════════════════════════════════════════════════
#  Frame capture
# ═══════════════════════════════════════════════════════════════════════════

def capture(plan_text: str = "") -> Any:
    """Capture the current frame as a host object.

    The active adapter decides how capture works. For graphics stacks that can
    produce a raster, the expected output is a contiguous HWC uint8 array
    suitable for python_shared_image() on the Kain side.
    """
    if _ACTIVE_ADAPTER is None:
        raise RuntimeError("no window is open — call pykain.window.open() first")

    if _ACTIVE_ADAPTER.capture_window is None:
        raise RuntimeError(f"capture not implemented for backend '{_ACTIVE_ADAPTER.name}'")

    plan = dict(_ACTIVE_PLAN)
    plan.update(_plan(plan_text))
    return _call_adapter(_ACTIVE_ADAPTER.capture_window, _ACTIVE_HANDLE, plan)
