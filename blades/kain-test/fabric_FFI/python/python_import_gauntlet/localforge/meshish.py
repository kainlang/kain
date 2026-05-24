import hashlib
import os

import numpy as np
import trimesh


def make_mesh(subdivisions, radius):
    radius = float(radius)
    mesh = trimesh.creation.icosphere(int(subdivisions), radius)
    heat = np.clip(((mesh.vertices[:, 2] + radius) / max(radius * 2.0, 1e-6)) * 255.0, 0, 255).astype(np.uint8)
    mesh.visual.vertex_colors = np.stack(
        [
            255 - (heat // 3),
            90 + (heat // 2),
            120 + (heat // 3),
            np.full_like(heat, 255),
        ],
        axis=1,
    )
    return mesh


def vertex_y(target, index):
    return float(target.vertices[int(index)][1])


def mesh_checksum(target):
    if isinstance(target, trimesh.Scene):
        mesh = target.dump(concatenate=True)
    else:
        mesh = target
    digest = hashlib.sha256(np.asarray(mesh.vertices, dtype=np.float32).tobytes()).hexdigest()
    return int(digest[:12], 16) % 1000000007


def save_glb(target, path):
    if isinstance(target, trimesh.Scene):
        scene = target
    elif isinstance(target, trimesh.Trimesh):
        scene = trimesh.Scene(target)
    else:
        scene = trimesh.Scene(target)
    blob = scene.export(file_type="glb")
    with open(path, "wb") as handle:
        handle.write(blob)
    return int(os.path.getsize(path))
