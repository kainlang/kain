use std::fs;
use std::path::Path;

use kade_desktop_controller::{KadeDesktopController, NewSessionRequest, ToolApprovalDecision};
use tempfile::tempdir;

fn write_file(path: &Path, content: &str) {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).expect("create parent");
    }
    fs::write(path, content).expect("write file");
}

fn write_fixture_app(app_root: &Path) {
    write_file(
        &app_root.join("config/app_manifest.json"),
        r#"{
  "app_id": "fixture.desktop",
  "name": "Fixture Desktop",
  "version": "0.1.0",
  "window_title": "Fixture Desktop",
  "root_component": "App",
  "layout_id": "fixture_shell",
  "manifests": {
    "panels": "config/panels.json",
    "commands": "config/commands.json",
    "providers": "config/providers.json",
    "tools": "config/tools.json"
  },
  "required_runtime_capabilities": ["ui.runtime", "host.bridge"],
  "target_outputs": ["native-ui-bundle"],
  "persistence": {
    "session_store": "state/sessions",
    "workspace_store": "state/workspaces",
    "provider_store": "state/providers.json"
  }
}"#,
    );
    write_file(
        &app_root.join("config/panels.json"),
        r#"{
  "layout": "dock",
  "panels": [
    { "id": "workspace", "title": "Workspace", "dock": "left", "kind": "tree" },
    { "id": "chat", "title": "Chat", "dock": "center", "kind": "conversation" }
  ]
}"#,
    );
    write_file(
        &app_root.join("config/commands.json"),
        r#"{
  "commands": [
    { "id": "new_chat", "label": "New Chat", "surface": "titlebar", "intent": "session.create" }
  ]
}"#,
    );
    write_file(
        &app_root.join("config/providers.json"),
        r#"{
  "default_provider": "openai_codex",
  "providers": [
    {
      "id": "openai_codex",
      "label": "OpenAI Codex",
      "transport": "http",
      "profile_kind": "openai-compatible",
      "supports_streaming": true,
      "supports_tools": true
    },
    {
      "id": "local_ollama",
      "label": "Local Ollama",
      "transport": "http",
      "profile_kind": "local",
      "supports_streaming": true,
      "supports_tools": false
    }
  ]
}"#,
    );
    write_file(
        &app_root.join("config/tools.json"),
        r#"{
  "tools": [
    { "id": "read_file", "label": "Read File", "capability": "filesystem.read", "approval": "workspace" },
    { "id": "run_terminal", "label": "Run Terminal", "capability": "process.exec", "approval": "explicit" }
  ]
}"#,
    );
}

#[test]
fn bootstrap_writes_state_files_and_snapshot() {
    let temp = tempdir().expect("tempdir");
    write_fixture_app(temp.path());
    let controller = KadeDesktopController::load(temp.path()).expect("load controller");

    let snapshot = controller.bootstrap_state().expect("bootstrap");

    assert_eq!(snapshot.name, "Fixture Desktop");
    assert_eq!(snapshot.providers.len(), 2);
    assert!(controller.runtime_snapshot_path().exists());
    assert!(temp.path().join("state/providers.json").exists());
    assert!(temp.path().join("state/task_history.json").exists());
    assert!(temp.path().join("state/tool_approvals.json").exists());
}

#[test]
fn session_and_approval_flow_updates_snapshot() {
    let temp = tempdir().expect("tempdir");
    write_fixture_app(temp.path());
    let controller = KadeDesktopController::load(temp.path()).expect("load controller");
    controller.bootstrap_state().expect("bootstrap");

    controller
        .set_active_provider("local_ollama")
        .expect("set provider");
    controller
        .set_provider_profile(
            "local_ollama",
            serde_json::json!({
                "base_url": "http://localhost:11434",
                "model": "qwen-coder"
            }),
        )
        .expect("set provider profile");
    controller
        .set_tool_approval("read_file", "workspace", ToolApprovalDecision::Allow)
        .expect("approve tool");

    let session = controller
        .create_session(NewSessionRequest {
            title: "Port the desktop shell".to_string(),
            workspace_root: Some("M:/Code/Kain".to_string()),
        })
        .expect("create session");
    let session = controller
        .append_message(
            &session.id,
            "user",
            "Build the manifest-driven session layer.",
        )
        .expect("append message");

    assert_eq!(session.provider_id, "local_ollama");
    assert_eq!(session.message_count, 1);

    let snapshot_json =
        fs::read_to_string(controller.runtime_snapshot_path()).expect("read runtime snapshot");
    let snapshot: serde_json::Value =
        serde_json::from_str(&snapshot_json).expect("parse runtime snapshot");

    assert_eq!(snapshot["sessions"]["total_sessions"], 1);
    assert_eq!(snapshot["sessions"]["active_provider"], "local_ollama");
    assert_eq!(
        snapshot["sessions"]["recent_session_title"],
        "Port the desktop shell"
    );
    assert_eq!(snapshot["tools"][0]["decision"], "allow");
    assert_eq!(snapshot["providers"][1]["profile_configured"], true);
    assert_eq!(snapshot["recent_sessions"][0]["last_message_role"], "user");
    assert_eq!(snapshot["workspaces"][0]["root"], "M:/Code/Kain");
}

#[test]
fn generated_shell_includes_manifest_driven_state() {
    let temp = tempdir().expect("tempdir");
    write_fixture_app(temp.path());
    let controller = KadeDesktopController::load(temp.path()).expect("load controller");
    controller.bootstrap_state().expect("bootstrap");
    controller
        .set_provider_profile(
            "openai_codex",
            serde_json::json!({
                "base_url": "https://api.openai.com",
                "model": "gpt-5.4"
            }),
        )
        .expect("set provider profile");
    controller
        .set_tool_approval("run_terminal", "explicit", ToolApprovalDecision::Ask)
        .expect("set terminal approval");
    let session = controller
        .create_session(NewSessionRequest {
            title: "Generated shell session".to_string(),
            workspace_root: Some("M:/Code/Kain".to_string()),
        })
        .expect("create session");
    controller
        .append_message(
            &session.id,
            "assistant",
            "Materialize the shell from manifests.",
        )
        .expect("append message");

    let shell_path = controller
        .write_generated_shell()
        .expect("write generated shell");
    let shell = fs::read_to_string(shell_path).expect("read shell");

    assert!(shell.contains("Generated shell session"));
    assert!(shell.contains("openai_codex"));
    assert!(shell.contains("Run Terminal"));
    assert!(shell.contains("Materialize the shell from manifests."));
}
