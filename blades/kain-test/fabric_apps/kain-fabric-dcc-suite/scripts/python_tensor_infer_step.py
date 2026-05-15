import importlib.util


def app_state_path(name):
    return f"apps/kain-fabric-dcc-suite/state/{name}"


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    train_report = fabric_inputs.get("tensor_train_stage", {}).get("tensor_training_report", {})
    torch_available = importlib.util.find_spec("torch") is not None
    status = "ready" if torch_available else "planned"
    summary = train_report.get("summary", "tensor-train:unknown") if isinstance(train_report, dict) else str(train_report)
    model_path = train_report.get("checkpoint_path", app_state_path("tensor_train_checkpoint.json")) if isinstance(train_report, dict) else app_state_path("tensor_train_checkpoint.json")
    input_shape = train_report.get("tensor_feature_shape", [0]) if isinstance(train_report, dict) else [0]
    output_shape = [settings.get("preview_count", 0), max(settings.get("tensor_feature_count", 0), 1)]
    artifact_shape = {
        "input_dims": input_shape,
        "output_dims": output_shape,
        "dispatch_receipt": app_state_path("tensor_infer_dispatch.json"),
        "result_receipt": app_state_path("tensor_infer_result.json"),
    }
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "dispatch_mode": "python_worker" if torch_available else "external_worker_required",
        "model_id": f"{settings.get('project_name', 'fabric-dcc-suite')}-preview-model",
        "model_path": model_path,
        "input_uri": "buffer://tensor/features",
        "input_shape": input_shape,
        "output_binding": "buffer://tensor/inference/output",
        "output_shape": output_shape,
        "artifact_shape": artifact_shape,
        "output_path": app_state_path("tensor_infer_result.json"),
        "dispatch_receipt_path": app_state_path("tensor_infer_dispatch.json"),
        "inference_profile": "preview_surface_probe",
        "summary": f"tensor-infer:{status}:upstream={summary}:torch={torch_available}:shape={input_shape}->{output_shape}",
    }
