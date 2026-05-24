import hashlib
import math
import os

import networkx as nx
import numpy as np
from PIL import Image, ImageDraw


def seed_phrase():
    return "local-python-import-ok"


def _orbit_points(count, width, height):
    center_x = width * 0.5
    center_y = height * 0.5
    radius_x = width * 0.34
    radius_y = height * 0.28
    points = []
    for index in range(count):
        angle = (index / max(count, 1)) * math.tau
        points.append(
            (
                center_x + math.cos(angle) * radius_x,
                center_y + math.sin(angle) * radius_y,
            )
        )
    return points


def make_pillow_numpy_banner(width, height):
    width = int(width)
    height = int(height)
    graph = nx.cycle_graph(11)
    image = Image.new("RGBA", (width, height), (12, 18, 34, 255))
    draw = ImageDraw.Draw(image)
    orbit = _orbit_points(graph.number_of_nodes(), width, height)

    for left, right in graph.edges():
        x0, y0 = orbit[int(left)]
        x1, y1 = orbit[int(right)]
        draw.line((x0, y0, x1, y1), fill=(80, 200, 255, 180), width=3)

    for idx, (x, y) in enumerate(orbit):
        fill = (
            120 + ((idx * 11) % 120),
            90 + ((idx * 17) % 140),
            180 + ((idx * 7) % 70),
            255,
        )
        draw.ellipse((x - 8, y - 8, x + 8, y + 8), fill=fill, outline=(255, 248, 220, 255), width=2)

    draw.rectangle((8, 8, width - 8, height - 8), outline=(255, 180, 110, 255), width=2)
    draw.arc((width * 0.18, height * 0.14, width * 0.82, height * 0.86), start=15, end=330, fill=(255, 245, 210, 255), width=4)

    rgba = np.asarray(image, dtype=np.uint8)
    return np.require(rgba, dtype=np.uint8, requirements=["C", "W", "A"])


def image_signature(rgba):
    rgba = np.require(rgba, dtype=np.uint8, requirements=["C"])
    center = rgba[rgba.shape[0] // 2, rgba.shape[1] // 2]
    digest = hashlib.sha256(rgba.tobytes()).hexdigest()[:20]
    return f"{int(rgba.shape[1])}x{int(rgba.shape[0])}:{int(center[0])}-{int(center[1])}-{int(center[2])}-{int(center[3])}:{digest}"


def save_png(rgba, path):
    rgba = np.require(rgba, dtype=np.uint8, requirements=["C"])
    Image.fromarray(rgba, mode="RGBA").save(path)
    return int(os.path.getsize(path))


def make_tensor_cube(size):
    size = int(size)
    axis = np.linspace(-1.0, 1.0, size, dtype=np.float32)
    zz, yy, xx = np.meshgrid(axis, axis, axis, indexing="ij")
    field = np.sin((xx * 7.0) + (yy * 4.0)) + np.cos((yy * 6.0) - (zz * 5.0)) + np.sin((zz * 3.0) + (xx * 2.0))
    field = field.astype(np.float32, copy=False)
    return np.require(field, dtype=np.float32, requirements=["C", "W", "A"])


def tensor_signature(tensor):
    tensor = np.require(tensor, dtype=np.float32, requirements=["C"])
    mid = tensor.shape[0] // 2
    sample = float(tensor[mid, mid, mid])
    digest = hashlib.sha256(tensor.tobytes()).hexdigest()[:20]
    return f"{int(tensor.shape[0])}^3:{sample:.4f}:{digest}"


def save_npy(tensor, path):
    tensor = np.require(tensor, dtype=np.float32, requirements=["C"])
    np.save(path, tensor)
    return int(os.path.getsize(path))
