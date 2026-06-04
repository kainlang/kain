"""pykain — the official Python companion layer for Kain.

Import this from any Kain script to get normalized, stable Python interop.
Handles tensor crossing, image/buffer contracts, window management, and
adapter-specific quirks so your Kain scripts stay clean and backend-agnostic.

Usage from Kain:
    import pykain as pykain

    let tensor = pykain.tensor.grid(plan, seed)
    let info = pykain.tensor.info(tensor)
    let sig = pykain.tensor.signature(tensor)

    let image = pykain.image.render(plan, state)
    let img_info = pykain.image.info(image)

    let window = pykain.window.open(plan)
    let frame = pykain.window.capture(plan)

    let score = pykain.validate.module("numpy")

The goal: Kain scripts should never need to know whether the host window comes
from pyglet, tkinter, Qt, glfw, or something else. pykain normalizes all of it.
"""

__version__ = "0.1.0"

from .api import adapt, as_buffer, as_bytes, as_image, as_tensor, inspect, inspect_json, native_available, native_version, signature, smoke_score  # noqa: E402, F401
from .contracts import KainActorRef, KainBuffer, KainEnvelope, KainGpuResource, KainImage, KainPatchEvent, KainRuntimeSession, KainShaderModule, KainTensor, KainWorldRef  # noqa: E402, F401
from . import tensor  # noqa: E402, F401
from . import image   # noqa: E402, F401
from . import buffer  # noqa: E402, F401
from . import window  # noqa: E402, F401
from . import validate  # noqa: E402, F401
from . import gpu  # noqa: E402, F401
from . import shader  # noqa: E402, F401
from . import actor  # noqa: E402, F401
from . import world  # noqa: E402, F401

__all__ = [
    "adapt",
    "as_buffer",
    "as_bytes",
    "as_image",
    "as_tensor",
    "inspect",
    "inspect_json",
    "native_available",
    "native_version",
    "signature",
    "smoke_score",
    "KainActorRef",
    "KainBuffer",
    "KainEnvelope",
    "KainGpuResource",
    "KainImage",
    "KainPatchEvent",
    "KainRuntimeSession",
    "KainShaderModule",
    "KainTensor",
    "KainWorldRef",
    "tensor",
    "image",
    "buffer",
    "window",
    "validate",
    "gpu",
    "shader",
    "actor",
    "world",
    "__version__",
]
