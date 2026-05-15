import json
from pathlib import Path


def _resolve_sculpt_pipeline_path():
    candidates = [
        Path("config/sculpt_pipeline.json"),
        Path("apps/kain-fabric-dcc-suite/config/sculpt_pipeline.json"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("Unable to locate config/sculpt_pipeline.json from the current Fabric working directory.")


def _load_sculpt_pipeline():
    config_path = _resolve_sculpt_pipeline_path()
    with config_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def _load_mesh_resource_contract():
    candidates = [
        Path("config/mesh_resource_contract.json"),
        Path("apps/kain-fabric-dcc-suite/config/mesh_resource_contract.json"),
    ]
    for candidate in candidates:
        if candidate.exists():
            with candidate.open("r", encoding="utf-8") as handle:
                return json.load(handle)
    raise FileNotFoundError("Unable to locate config/mesh_resource_contract.json from the current Fabric working directory.")


def run(fabric_inputs):
    sculpt_pipeline = _load_sculpt_pipeline()
    mesh_contract = _load_mesh_resource_contract()
    brush = sculpt_pipeline["brush"]
    height_range = sculpt_pipeline["height_range"]
    grid_resolution = int(sculpt_pipeline["grid_resolution"])
    sample_count = int(sculpt_pipeline.get("sample_count", grid_resolution * grid_resolution))
    mesh_documents = mesh_contract["mesh_documents"]
    resource_uris = {document["id"]: document["resource_uri"] for document in mesh_documents}

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
        "mesh_contract_resource_uris": resource_uris,
    }
