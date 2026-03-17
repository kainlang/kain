import numpy as np


def kain_trinity_payload(width, height, count, orbit):
    angles = np.linspace(0.0, np.pi * 2.0, count, endpoint=False, dtype=np.float32)
    xs = width * 0.5 + np.cos(angles) * width * 0.28
    ys = height * 0.5 + np.sin(angles) * height * 0.22
    radii = (3 + ((np.arange(count) + orbit) % 5)).astype(np.int64)
    opacity = 0.28 + 0.58 * (0.5 + 0.5 * np.sin(angles * 5.0 + orbit * 0.05))
    bands = (
        24.0
        + 18.0
        * (
            0.5
            + 0.5
            * np.sin(
                np.linspace(-1.0, 1.0, count, dtype=np.float32) * 8.0
                + orbit * 0.07
            )
        )
    ).astype(np.int64)
    points = np.stack(
        [xs.astype(np.int64), ys.astype(np.int64), radii, opacity],
        axis=1,
    ).tolist()
    return {
        "width": int(width),
        "height": int(height),
        "points": points,
        "bands": bands.tolist(),
        "accent": "#8ef6ff",
        "secondary": "#ff8ad8",
        "background": "radial-gradient(circle at top, #1b2038 0%, #060812 74%)",
        "title": "Trinity Web Lattice",
    }
