import json
from pathlib import Path


def _load_sculpt_pipeline():
    config_path = Path(__file__).resolve().parents[1] / "config" / "sculpt_pipeline.json"
    with config_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def run(fabric_inputs):
    sculpt_pipeline = _load_sculpt_pipeline()
    brush = sculpt_pipeline["brush"]
    height_range = sculpt_pipeline["height_range"]
    grid_resolution = int(sculpt_pipeline["grid_resolution"])
    sample_count = int(sculpt_pipeline.get("sample_count", grid_resolution * grid_resolution))

    return {
        "project_name": "fabric-dcc-suite",
        "viewport_width": 1600,
        "viewport_height": 960,
        "brush_accent": 53,
        "preview_count": 16,
        "tensor_feature_count": 12,
        "runtime_pack_count": 12,
        "workspace_mode": "scene_assembly",
        "sculpt_grid_resolution": grid_resolution,
        "sculpt_sample_count": sample_count,
        "sculpt_center_x_milli": int(brush["center_x_milli"]),
        "sculpt_center_y_milli": int(brush["center_y_milli"]),
        "sculpt_radius_milli": int(brush["radius_milli"]),
        "sculpt_strength_milli": int(brush["strength_milli"]),
        "sculpt_falloff_milli": int(brush["falloff_milli"]),
        "sculpt_invert": bool(brush["invert"]),
        "sculpt_height_min_milli": int(height_range["min_milli"]),
        "sculpt_height_max_milli": int(height_range["max_milli"]),
    }
