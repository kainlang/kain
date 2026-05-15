import importlib.util


def app_state_path(name):
    return f"apps/kain-fabric-dcc-suite/state/{name}"


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    torch_available = importlib.util.find_spec("torch") is not None
    tensor_features = fabric_inputs.get("dcc_suite_seed", {}).get("tensor_features", {})
    byte_length = tensor_features.get("byte_length", 0)
    status = "ready" if torch_available else "planned"
    epochs = 4 if torch_available else 1
    batch_size = max(4, byte_length // 12) if byte_length else 4
    feature_shape = [byte_length] if byte_length else [0]
    artifact_shape = {
        "feature_bytes": byte_length,
        "feature_dims": feature_shape,
        "checkpoint_dims": [epochs, batch_size],
        "dispatch_receipt": app_state_path("tensor_train_dispatch.json"),
        "checkpoint_receipt": app_state_path("tensor_train_checkpoint.json"),
    }
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "byte_length": byte_length,
        "dispatch_mode": "python_worker" if torch_available else "external_worker_required",
        "model_architecture": f"fabric_preview_mlp_{max(byte_length, 16)}",
        "dataset_uri": "buffer://tensor/features",
        "tensor_feature_shape": feature_shape,
        "artifact_shape": artifact_shape,
        "epochs": epochs,
        "batch_size": batch_size,
        "checkpoint_tag": f"{settings.get('project_name', 'fabric-dcc-suite')}-train-{byte_length}",
        "dispatch_receipt_path": app_state_path("tensor_train_dispatch.json"),
        "checkpoint_path": app_state_path("tensor_train_checkpoint.json"),
        "summary": f"tensor-train:{status}:bytes={byte_length}:torch={torch_available}:shape={byte_length}x{batch_size}",
    }
