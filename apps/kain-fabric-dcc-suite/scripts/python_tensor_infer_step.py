import importlib.util


def app_state_path(name):
    return f"apps/kain-fabric-dcc-suite/state/{name}"


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    train_report = fabric_inputs.get("tensor_train_stage", {}).get("tensor_training_report", {})
    torch_available = importlib.util.find_spec("torch") is not None
    status = "ready" if torch_available else "planned"
    summary = train_report.get("summary", "tensor-train:unknown") if isinstance(train_report, dict) else str(train_report)
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "dispatch_mode": "python_worker" if torch_available else "external_worker_required",
        "model_id": f"{settings.get('project_name', 'fabric-dcc-suite')}-preview-model",
        "model_path": train_report.get("checkpoint_path", app_state_path("tensor_train_checkpoint.json")) if isinstance(train_report, dict) else app_state_path("tensor_train_checkpoint.json"),
        "input_uri": "buffer://tensor/features",
        "output_binding": "buffer://tensor/inference/output",
        "output_path": app_state_path("tensor_infer_result.json"),
        "dispatch_receipt_path": app_state_path("tensor_infer_dispatch.json"),
        "inference_profile": "preview_surface_probe",
        "summary": f"tensor-infer:{status}:upstream={summary}:torch={torch_available}",
    }
