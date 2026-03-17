import numpy as np


def make_quad_prism(width: int, height: int, phase: int):
    y = np.linspace(-1.0, 1.0, height, dtype=np.float32)[:, None]
    x = np.linspace(-1.0, 1.0, width, dtype=np.float32)[None, :]
    radial = np.sqrt(x * x + y * y + 1e-6)
    angle = np.arctan2(y, x)
    wave = 0.5 + 0.5 * np.sin((x * 9.0 + y * 13.0 + phase * 0.07) * np.pi)
    spin = 0.5 + 0.5 * np.cos((angle * 6.0 - radial * 8.0 + phase * 0.04))
    glow = np.exp(-2.8 * radial * radial)

    image = np.zeros((height, width, 4), dtype=np.uint8)
    image[..., 0] = np.clip(255.0 * (0.18 + 0.62 * glow + 0.22 * wave), 0, 255).astype(np.uint8)
    image[..., 1] = np.clip(255.0 * (0.10 + 0.45 * spin + 0.35 * glow), 0, 255).astype(np.uint8)
    image[..., 2] = np.clip(255.0 * (0.22 + 0.58 * wave * spin), 0, 255).astype(np.uint8)
    image[..., 3] = 255
    return image
