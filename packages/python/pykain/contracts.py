"""Kain-flavored semantic envelopes for Python ecosystem objects."""

from __future__ import annotations

from dataclasses import dataclass, field
import json
from typing import Any


@dataclass
class KainEnvelope:
    kind: str
    source: Any = None
    info: dict[str, Any] = field(default_factory=dict)
    labels: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = dict(self.info)
        payload["kind"] = self.kind
        if self.labels:
            payload["labels"] = dict(self.labels)
        return payload

    def descriptor_json(self) -> str:
        return json.dumps(self.descriptor(), default=str, separators=(",", ":"))


@dataclass
class KainBuffer(KainEnvelope):
    kind: str = "buffer"


@dataclass
class KainTensor(KainEnvelope):
    kind: str = "tensor"


@dataclass
class KainImage(KainEnvelope):
    kind: str = "image"


@dataclass
class KainGpuResource(KainEnvelope):
    kind: str = "gpu_resource"
    policy: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload["policy"] = dict(self.policy)
        return payload


@dataclass
class KainShaderModule(KainEnvelope):
    kind: str = "shader_module"
    stage: str = "compute"
    entry_point: str = "main"
    bindings: list[dict[str, Any]] = field(default_factory=list)
    uniforms: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({
            "stage": self.stage,
            "entry_point": self.entry_point,
            "bindings": list(self.bindings),
            "uniforms": dict(self.uniforms),
        })
        return payload


@dataclass
class KainActorRef(KainEnvelope):
    kind: str = "actor_ref"
    actor_id: int | None = None
    name: str = ""

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({"actor_id": self.actor_id, "name": self.name})
        return payload


@dataclass
class KainWorldRef(KainEnvelope):
    kind: str = "world_ref"
    name: str = ""
    state: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({"name": self.name, "state": dict(self.state)})
        return payload


@dataclass
class KainEntangleLink(KainEnvelope):
    kind: str = "entangle_link"
    left: str = ""
    right: str = ""
    policy: str = "single_writer"

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({"left": self.left, "right": self.right, "policy": self.policy})
        return payload


@dataclass
class KainPatchEvent(KainEnvelope):
    kind: str = "patch_event"
    target: str = ""
    changes: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({"target": self.target, "changes": dict(self.changes)})
        return payload


@dataclass
class KainRuntimeSession(KainEnvelope):
    kind: str = "runtime_session"
    name: str = "pykain"
    handles: dict[str, Any] = field(default_factory=dict)

    def descriptor(self) -> dict[str, Any]:
        payload = super().descriptor()
        payload.update({"name": self.name, "handles": dict(self.handles)})
        return payload
