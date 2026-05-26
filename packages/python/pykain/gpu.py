"""Compute-first GPU descriptors for Kain interop."""

from __future__ import annotations

from typing import Any

from .api import adapt
from .contracts import KainGpuResource

GPU_STAGE_COMPUTE = 4
GPU_QUEUE_COMPUTE = 2
GPU_QUEUE_TRANSFER = 4
GPU_QUEUE_HOST = 16
GPU_ACCESS_READ = 1
GPU_ACCESS_WRITE = 2
GPU_ACCESS_READ_WRITE = GPU_ACCESS_READ | GPU_ACCESS_WRITE
GPU_RESIDENCY_HOST_VISIBLE = 1
GPU_RESIDENCY_HOST_COHERENT = 2
GPU_RESIDENCY_SHARED = 8
GPU_RESIDENCY_IMPORTED = 16
GPU_RESIDENCY_ZERO_COPY = 256
GPU_BUFFER_USAGE_STORAGE = 4
GPU_BUFFER_USAGE_TRANSFER_SRC = 1
GPU_BUFFER_USAGE_TRANSFER_DST = 2


def resource(
    obj: Any,
    *,
    debug_name: str = "pykain.gpu.resource",
    descriptor_kind: str = "storage_buffer",
    usage_flags: int = GPU_BUFFER_USAGE_STORAGE | GPU_BUFFER_USAGE_TRANSFER_SRC | GPU_BUFFER_USAGE_TRANSFER_DST,
    access_flags: int = GPU_ACCESS_READ_WRITE,
    queue_flags: int = GPU_QUEUE_COMPUTE | GPU_QUEUE_TRANSFER | GPU_QUEUE_HOST,
    layout_kind: str = "tight",
    residency_flags: int = GPU_RESIDENCY_HOST_VISIBLE | GPU_RESIDENCY_HOST_COHERENT | GPU_RESIDENCY_SHARED | GPU_RESIDENCY_IMPORTED | GPU_RESIDENCY_ZERO_COPY,
) -> KainGpuResource:
    envelope = adapt(obj)
    policy = {
        "debug_name": debug_name,
        "descriptor_kind": descriptor_kind,
        "usage_flags": usage_flags,
        "access_flags": access_flags,
        "queue_flags": queue_flags,
        "layout_kind": layout_kind,
        "residency_flags": residency_flags,
        "stage_flags": GPU_STAGE_COMPUTE,
    }
    info = envelope.descriptor()
    info.update({
        "resource_kind": "buffer" if envelope.kind != "image" else "image",
        "byte_length": int(info.get("byte_length", 0) or 0),
    })
    return KainGpuResource(source=obj, info=info, labels=envelope.labels, policy=policy)


def compute_buffer(obj: Any, binding: int = 0, debug_name: str = "pykain.compute.buffer") -> KainGpuResource:
    gpu_resource = resource(obj, debug_name=debug_name)
    gpu_resource.policy["binding"] = int(binding)
    return gpu_resource
