"""Actor semantic envelopes for Python-side Kain interop."""

from __future__ import annotations

from typing import Any

from .contracts import KainActorRef


def ref(name: str = "", actor_id: int | None = None, **labels: Any) -> KainActorRef:
    info = {
        "contract": "kain.actor.ref",
        "contract_version": 1,
        "source_runtime": "python",
    }
    return KainActorRef(info=info, labels=labels, actor_id=actor_id, name=name)


def message(target: KainActorRef | str, name: str, payload: Any = None) -> dict[str, Any]:
    target_name = target.name if isinstance(target, KainActorRef) else str(target)
    target_id = target.actor_id if isinstance(target, KainActorRef) else None
    return {
        "contract": "kain.actor.message",
        "contract_version": 1,
        "target": target_name,
        "target_id": target_id,
        "message": name,
        "payload": payload,
    }
