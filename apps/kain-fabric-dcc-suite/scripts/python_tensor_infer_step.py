import importlib.util


def run(fabric_inputs):
    settings = fabric_inputs.get("python_suite_bootstrap", {}).get("project_settings", {})
    train_report = fabric_inputs.get("tensor_train_stage", {}).get("tensor_training_report", {})
    torch_available = importlib.util.find_spec("torch") is not None
    status = "ready" if torch_available else "extension-seam"
    summary = train_report.get("summary", "tensor-train:unknown") if isinstance(train_report, dict) else str(train_report)
    return {
        "status": status,
        "project": settings.get("project_name", "fabric-dcc-suite"),
        "summary": f"tensor-infer:{status}:upstream={summary}:torch={torch_available}",
    }
