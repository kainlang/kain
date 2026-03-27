import importlib.util


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    torch_available = importlib.util.find_spec("torch") is not None
    tensor_features = fabric_inputs.get("dcc_suite_seed", {}).get("tensor_features", {})
    byte_length = tensor_features.get("byte_length", 0)
    status = "ready" if torch_available else "planned"
    epochs = 4 if torch_available else 1
    batch_size = max(4, byte_length // 12) if byte_length else 4
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "byte_length": byte_length,
        "dispatch_mode": "python_worker" if torch_available else "external_worker_required",
        "model_architecture": f"fabric_preview_mlp_{max(byte_length, 16)}",
        "dataset_uri": "buffer://tensor/features",
        "epochs": epochs,
        "batch_size": batch_size,
        "checkpoint_tag": f"{settings.get('project_name', 'fabric-dcc-suite')}-train-{byte_length}",
        "dispatch_receipt_path": "state/tensor_train_dispatch.json",
        "checkpoint_path": "state/tensor_train_checkpoint.json",
        "summary": f"tensor-train:{status}:bytes={byte_length}:torch={torch_available}",
    }
