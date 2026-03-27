import importlib.util


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    torch_available = importlib.util.find_spec("torch") is not None
    tensor_features = fabric_inputs.get("dcc_suite_seed", {}).get("tensor_features", {})
    byte_length = tensor_features.get("byte_length", 0)
    status = "ready" if torch_available else "extension-seam"
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "byte_length": byte_length,
        "summary": f"tensor-train:{status}:bytes={byte_length}:torch={torch_available}",
    }
