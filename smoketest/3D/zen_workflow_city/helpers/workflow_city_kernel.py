import math

import numpy as np
import trimesh


def _rgba(hex_color, alpha=255):
    value = hex_color.lstrip("#")
    if len(value) != 6:
        return np.array([160, 180, 220, alpha], dtype=np.uint8)
    return np.array(
        [
            int(value[0:2], 16),
            int(value[2:4], 16),
            int(value[4:6], 16),
            alpha,
        ],
        dtype=np.uint8,
    )


def _colorize(mesh, rgba):
    mesh.visual.vertex_colors = np.tile(rgba, (len(mesh.vertices), 1))
    return mesh


def _pad(name, x, z, span, color):
    pad = trimesh.creation.box(extents=(span, 0.18, span * 0.76))
    pad.apply_translation((x, 0.09, z))
    return _colorize(pad, _rgba(color, 255)), f"group_{name.lower()}"


def _beacon(name, x, z, height, color):
    beacon = trimesh.creation.icosphere(subdivisions=2, radius=0.28 + height * 0.02)
    beacon.apply_translation((x, 0.92 + height * 0.12, z))
    return _colorize(beacon, _rgba(color, 235)), f"beacon_{name.lower()}"


def _module_tower(name, x, z, height, color, energy):
    tower = trimesh.creation.box(extents=(0.92, height, 0.92))
    tower.apply_translation((x, height * 0.5, z))
    tower = _colorize(tower, _rgba(color, 250))

    cap = trimesh.creation.box(extents=(1.06, 0.16 + energy * 0.05, 1.06))
    cap.apply_translation((x, height + 0.08 + energy * 0.02, z))
    cap = _colorize(cap, _rgba("#ffd166", 255))

    return [
        (tower, f"module_{name.lower().replace('-', '_')}"),
        (cap, f"cap_{name.lower().replace('-', '_')}"),
    ]


def _bridge(name, start, end, color):
    dx = end[0] - start[0]
    dz = end[1] - start[1]
    length = max(math.sqrt(dx * dx + dz * dz), 0.01)
    bridge = trimesh.creation.box(extents=(length, 0.12, 0.34))
    angle = math.atan2(dz, dx)
    bridge.apply_transform(trimesh.transformations.rotation_matrix(angle, [0, 1, 0]))
    bridge.apply_translation(((start[0] + end[0]) * 0.5, 0.34, (start[1] + end[1]) * 0.5))
    return _colorize(bridge, _rgba(color, 220)), name


def _core():
    core = trimesh.creation.icosphere(subdivisions=3, radius=1.35)
    heat = np.clip((core.vertices[:, 1] + 1.35) / 2.7, 0.0, 1.0)
    rgba = np.stack(
        [
            (255.0 - heat * 25.0).astype(np.uint8),
            (104.0 + heat * 86.0).astype(np.uint8),
            (86.0 + heat * 120.0).astype(np.uint8),
            np.full_like((heat * 255.0).astype(np.uint8), 255),
        ],
        axis=1,
    )
    core.visual.vertex_colors = rgba
    core.apply_translation((0.0, 1.48, 0.0))
    return core, "core_nexus"


def _ground():
    ground = trimesh.creation.box(extents=(29.5, 0.18, 29.5))
    ground.apply_translation((0.0, -0.09, 0.0))
    ground = _colorize(ground, _rgba("#101727", 255))
    ring = trimesh.creation.box(extents=(22.0, 0.1, 22.0))
    ring.apply_translation((0.0, 0.02, 0.0))
    ring = _colorize(ring, _rgba("#14243c", 255))
    return [(ground, "ground"), (ring, "ground_ring")]


def kain_workflow_city_scene(
    group_labels,
    group_colors,
    group_anchor_xs,
    group_anchor_zs,
    group_spans,
    module_names,
    module_group_indices,
    module_xs,
    module_zs,
    module_heights,
    module_energies,
):
    scene = trimesh.Scene()
    for mesh, name in _ground():
        scene.add_geometry(mesh, geom_name=name)

    for index, label in enumerate(group_labels):
        x = float(group_anchor_xs[index])
        z = float(group_anchor_zs[index])
        span = float(group_spans[index])
        color = group_colors[index]
        pad, pad_name = _pad(label, x, z, span, color)
        beacon, beacon_name = _beacon(label, x, z, span, color)
        scene.add_geometry(pad, geom_name=pad_name)
        scene.add_geometry(beacon, geom_name=beacon_name)

    for index, name in enumerate(module_names):
        group_index = int(module_group_indices[index])
        color = group_colors[group_index]
        x = float(module_xs[index])
        z = float(module_zs[index])
        height = float(module_heights[index])
        energy = float(module_energies[index])
        for mesh, mesh_name in _module_tower(name, x, z, height, color, energy):
            scene.add_geometry(mesh, geom_name=mesh_name)

    for group_index, label in enumerate(group_labels):
        indices = [i for i, value in enumerate(module_group_indices) if int(value) == group_index]
        for start_index, end_index in zip(indices[:-1], indices[1:]):
            bridge, bridge_name = _bridge(
                f"bridge_{label.lower()}_{start_index}_{end_index}",
                (float(module_xs[start_index]), float(module_zs[start_index])),
                (float(module_xs[end_index]), float(module_zs[end_index])),
                group_colors[group_index],
            )
            scene.add_geometry(bridge, geom_name=bridge_name)

    core, core_name = _core()
    scene.add_geometry(core, geom_name=core_name)
    return scene


def kain_workflow_city_mesh(
    group_labels,
    group_colors,
    group_anchor_xs,
    group_anchor_zs,
    group_spans,
    module_names,
    module_group_indices,
    module_xs,
    module_zs,
    module_heights,
    module_energies,
):
    scene = kain_workflow_city_scene(
        group_labels,
        group_colors,
        group_anchor_xs,
        group_anchor_zs,
        group_spans,
        module_names,
        module_group_indices,
        module_xs,
        module_zs,
        module_heights,
        module_energies,
    )
    return scene.dump(concatenate=True)


def kain_workflow_city_export_any(target, path):
    if isinstance(target, trimesh.Scene):
        scene = target
    else:
        scene = trimesh.Scene(target)
    blob = scene.export(file_type="glb")
    with open(path, "wb") as handle:
        handle.write(blob)
    combined = scene.dump(concatenate=True)
    extents = combined.extents.tolist() if combined is not None else [0.0, 0.0, 0.0]
    return {
        "path": path,
        "bytes": len(blob),
        "header": blob[:4].decode("ascii"),
        "geometry_count": len(scene.geometry),
        "vertex_count": int(len(combined.vertices)),
        "face_count": int(len(combined.faces)),
        "extents": extents,
        "scene_name": "zen_workflow_city",
    }
