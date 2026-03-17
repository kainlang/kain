import numpy as np


def make_prism_image(width: int, height: int):
    y = np.linspace(0.0, 1.0, height, dtype=np.float32)[:, None]
    x = np.linspace(0.0, 1.0, width, dtype=np.float32)[None, :]
    field = 0.5 + 0.5 * np.sin((x * 8.0 + y * 11.0) * np.pi)
    sweep = 0.5 + 0.5 * np.cos((x * 5.0 - y * 7.0) * np.pi)
    image = np.zeros((height, width, 3), dtype=np.uint8)
    image[..., 0] = np.clip(255.0 * field, 0, 255).astype(np.uint8)
    image[..., 1] = np.clip(255.0 * sweep, 0, 255).astype(np.uint8)
    image[..., 2] = np.clip(255.0 * (0.35 + 0.65 * (field * sweep)), 0, 255).astype(np.uint8)
    return image
