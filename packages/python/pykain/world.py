"""World, entangle, patch, and runtime-session envelopes."""

from __future__ import annotations

from typing import Any

from .contracts import KainEntangleLink, KainPatchEvent, KainRuntimeSession, KainWorldRef


def ref(name: str, state: dict[str, Any] | None = None, **labels: Any) -> KainWorldRef:
    info = {
        "contract": "kain.world.ref",
        "contract_version": 1,
        "source_runtime": "python",
    }
    return KainWorldRef(info=info, labels=labels, name=name, state=dict(state or {}))


def entangle(left: str, right: str, policy: str = "single_writer", **labels: Any) -> KainEntangleLink:
    info = {
        "contract": "kain.entangle.link",
        "contract_version": 1,
        "source_runtime": "python",
    }
    return KainEntangleLink(info=info, labels=labels, left=left, right=right, policy=policy)


def patch(target: str, changes: dict[str, Any], **labels: Any) -> KainPatchEvent:
    info = {
        "contract": "kain.patch.event",
        "contract_version": 1,
        "source_runtime": "python",
    }
    return KainPatchEvent(info=info, labels=labels, target=target, changes=dict(changes))


def session(name: str = "pykain", handles: dict[str, Any] | None = None, **labels: Any) -> KainRuntimeSession:
    info = {
        "contract": "kain.runtime.session",
        "contract_version": 1,
        "source_runtime": "python",
    }
    return KainRuntimeSession(info=info, labels=labels, name=name, handles=dict(handles or {}))
