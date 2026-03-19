use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use chrono::Utc;
use serde::{Deserialize, Serialize};
use thiserror::Error;

pub type Result<T> = std::result::Result<T, ControllerError>;

const TASK_HISTORY_FILE_NAME: &str = "task_history.json";
const TOOL_APPROVALS_FILE_NAME: &str = "tool_approvals.json";
const RUNTIME_SNAPSHOT_FILE_NAME: &str = "runtime_snapshot.json";

#[derive(Debug, Error)]
pub enum ControllerError {
    #[error("missing app manifest at {0}")]
    MissingAppManifest(PathBuf),
    #[error("I/O error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    #[error("JSON error at {path}: {source}")]
    Json {
        path: PathBuf,
        #[source]
        source: serde_json::Error,
    },
    #[error("unknown provider `{0}`")]
    UnknownProvider(String),
    #[error("unknown tool `{0}`")]
    UnknownTool(String),
    #[error("unknown session `{0}`")]
    UnknownSession(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppManifest {
    pub app_id: String,
    pub name: String,
    pub version: String,
    pub window_title: String,
    pub root_component: String,
    pub layout_id: String,
    pub manifests: ManifestPaths,
    pub required_runtime_capabilities: Vec<String>,
    pub target_outputs: Vec<String>,
    pub persistence: PersistenceConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ManifestPaths {
    pub panels: String,
    pub commands: String,
    pub providers: String,
    pub tools: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PersistenceConfig {
    pub session_store: String,
    pub workspace_store: String,
    pub provider_store: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PanelsManifest {
    pub layout: String,
    pub panels: Vec<PanelDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PanelDefinition {
    pub id: String,
    pub title: String,
    pub dock: String,
    pub kind: String,
    pub split_ratio: Option<f32>,
    pub min_width: Option<u32>,
    pub max_width: Option<u32>,
    pub min_height: Option<u32>,
    pub max_height: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommandsManifest {
    pub commands: Vec<CommandDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommandDefinition {
    pub id: String,
    pub label: String,
    pub surface: String,
    pub intent: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProvidersManifest {
    pub default_provider: String,
    pub providers: Vec<ProviderDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProviderDefinition {
    pub id: String,
    pub label: String,
    pub transport: String,
    pub profile_kind: String,
    pub supports_streaming: bool,
    pub supports_tools: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolsManifest {
    pub tools: Vec<ToolDefinition>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolDefinition {
    pub id: String,
    pub label: String,
    pub capability: String,
    pub approval: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct ProviderStore {
    pub active_provider: String,
    pub profiles: BTreeMap<String, serde_json::Value>,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum ToolApprovalDecision {
    Allow,
    Deny,
    Ask,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolApprovalRecord {
    pub tool_id: String,
    pub scope: String,
    pub decision: ToolApprovalDecision,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct ToolApprovalStore {
    pub approvals: Vec<ToolApprovalRecord>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionMessage {
    pub role: String,
    pub text: String,
    pub created_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChatSessionRecord {
    pub id: String,
    pub title: String,
    pub provider_id: String,
    pub status: String,
    pub workspace_root: Option<String>,
    pub created_at: String,
    pub updated_at: String,
    pub message_count: usize,
    pub messages: Vec<SessionMessage>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionHistoryEntry {
    pub id: String,
    pub title: String,
    pub provider_id: String,
    pub status: String,
    pub workspace_root: Option<String>,
    pub updated_at: String,
    pub message_count: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct SessionHistoryStore {
    pub sessions: Vec<SessionHistoryEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeSnapshot {
    pub app_id: String,
    pub name: String,
    pub version: String,
    pub window_title: String,
    pub root_component: String,
    pub layout_id: String,
    pub required_runtime_capabilities: Vec<String>,
    pub panels: Vec<PanelSnapshot>,
    pub commands: Vec<CommandSnapshot>,
    pub providers: Vec<ProviderSnapshot>,
    pub tools: Vec<ToolSnapshot>,
    pub sessions: SessionSnapshot,
    pub recent_sessions: Vec<RecentSessionSnapshot>,
    pub workspaces: Vec<WorkspaceSnapshot>,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PanelSnapshot {
    pub id: String,
    pub title: String,
    pub dock: String,
    pub kind: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CommandSnapshot {
    pub id: String,
    pub label: String,
    pub surface: String,
    pub intent: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProviderSnapshot {
    pub id: String,
    pub label: String,
    pub transport: String,
    pub profile_kind: String,
    pub supports_tools: bool,
    pub supports_streaming: bool,
    pub active: bool,
    pub profile_configured: bool,
    pub profile_keys: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolSnapshot {
    pub id: String,
    pub label: String,
    pub capability: String,
    pub approval: String,
    pub decision: Option<ToolApprovalDecision>,
    pub scope_decisions: Vec<ToolApprovalScopeSnapshot>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionSnapshot {
    pub total_sessions: usize,
    pub active_provider: String,
    pub recent_session_id: Option<String>,
    pub recent_session_title: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolApprovalScopeSnapshot {
    pub scope: String,
    pub decision: ToolApprovalDecision,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RecentSessionSnapshot {
    pub id: String,
    pub title: String,
    pub provider_id: String,
    pub status: String,
    pub workspace_root: Option<String>,
    pub updated_at: String,
    pub message_count: usize,
    pub last_message_role: Option<String>,
    pub last_message_preview: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkspaceSnapshot {
    pub root: String,
    pub session_count: usize,
    pub recent_session_title: Option<String>,
}

#[derive(Debug, Clone)]
pub struct KadeDesktopController {
    pub app_root: PathBuf,
    pub manifest: AppManifest,
    pub panels: PanelsManifest,
    pub commands: CommandsManifest,
    pub providers: ProvidersManifest,
    pub tools: ToolsManifest,
}

#[derive(Debug, Clone)]
pub struct NewSessionRequest {
    pub title: String,
    pub workspace_root: Option<String>,
}

impl KadeDesktopController {
    pub fn load(app_root: impl AsRef<Path>) -> Result<Self> {
        let app_root = app_root.as_ref().to_path_buf();
        let manifest_path = app_root.join("config").join("app_manifest.json");
        if !manifest_path.exists() {
            return Err(ControllerError::MissingAppManifest(manifest_path));
        }

        let manifest: AppManifest = read_json_file(&manifest_path)?;
        let panels = read_json_file(&app_root.join(&manifest.manifests.panels))?;
        let commands = read_json_file(&app_root.join(&manifest.manifests.commands))?;
        let providers = read_json_file(&app_root.join(&manifest.manifests.providers))?;
        let tools = read_json_file(&app_root.join(&manifest.manifests.tools))?;

        Ok(Self {
            app_root,
            manifest,
            panels,
            commands,
            providers,
            tools,
        })
    }

    pub fn bootstrap_state(&self) -> Result<RuntimeSnapshot> {
        fs::create_dir_all(self.session_store_dir()).map_err(|source| ControllerError::Io {
            path: self.session_store_dir(),
            source,
        })?;
        fs::create_dir_all(self.workspace_store_dir()).map_err(|source| ControllerError::Io {
            path: self.workspace_store_dir(),
            source,
        })?;

        if !self.provider_store_path().exists() {
            let store = ProviderStore {
                active_provider: self.providers.default_provider.clone(),
                profiles: BTreeMap::new(),
                updated_at: now_string(),
            };
            write_json_file(&self.provider_store_path(), &store)?;
        }

        if !self.tool_approval_store_path().exists() {
            let store = ToolApprovalStore::default();
            write_json_file(&self.tool_approval_store_path(), &store)?;
        }

        if !self.session_history_path().exists() {
            let store = SessionHistoryStore::default();
            write_json_file(&self.session_history_path(), &store)?;
        }

        self.write_runtime_snapshot()
    }

    pub fn active_provider_store(&self) -> Result<ProviderStore> {
        if !self.provider_store_path().exists() {
            self.bootstrap_state()?;
        }
        read_json_file(&self.provider_store_path())
    }

    pub fn tool_approval_store(&self) -> Result<ToolApprovalStore> {
        if !self.tool_approval_store_path().exists() {
            self.bootstrap_state()?;
        }
        read_json_file(&self.tool_approval_store_path())
    }

    pub fn session_history_store(&self) -> Result<SessionHistoryStore> {
        if !self.session_history_path().exists() {
            self.bootstrap_state()?;
        }
        read_json_file(&self.session_history_path())
    }

    pub fn set_active_provider(&self, provider_id: &str) -> Result<ProviderStore> {
        if !self
            .providers
            .providers
            .iter()
            .any(|provider| provider.id == provider_id)
        {
            return Err(ControllerError::UnknownProvider(provider_id.to_string()));
        }
        let mut store = self.active_provider_store()?;
        store.active_provider = provider_id.to_string();
        store.updated_at = now_string();
        write_json_file(&self.provider_store_path(), &store)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(store)
    }

    pub fn set_provider_profile(
        &self,
        provider_id: &str,
        profile: serde_json::Value,
    ) -> Result<ProviderStore> {
        if !self
            .providers
            .providers
            .iter()
            .any(|provider| provider.id == provider_id)
        {
            return Err(ControllerError::UnknownProvider(provider_id.to_string()));
        }
        let mut store = self.active_provider_store()?;
        store.profiles.insert(provider_id.to_string(), profile);
        store.updated_at = now_string();
        write_json_file(&self.provider_store_path(), &store)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(store)
    }

    pub fn clear_provider_profile(&self, provider_id: &str) -> Result<ProviderStore> {
        if !self
            .providers
            .providers
            .iter()
            .any(|provider| provider.id == provider_id)
        {
            return Err(ControllerError::UnknownProvider(provider_id.to_string()));
        }
        let mut store = self.active_provider_store()?;
        store.profiles.remove(provider_id);
        store.updated_at = now_string();
        write_json_file(&self.provider_store_path(), &store)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(store)
    }

    pub fn set_tool_approval(
        &self,
        tool_id: &str,
        scope: &str,
        decision: ToolApprovalDecision,
    ) -> Result<ToolApprovalStore> {
        if !self.tools.tools.iter().any(|tool| tool.id == tool_id) {
            return Err(ControllerError::UnknownTool(tool_id.to_string()));
        }
        let mut store = self.tool_approval_store()?;
        if let Some(existing) = store
            .approvals
            .iter_mut()
            .find(|entry| entry.tool_id == tool_id && entry.scope == scope)
        {
            existing.decision = decision;
            existing.updated_at = now_string();
        } else {
            store.approvals.push(ToolApprovalRecord {
                tool_id: tool_id.to_string(),
                scope: scope.to_string(),
                decision,
                updated_at: now_string(),
            });
        }
        write_json_file(&self.tool_approval_store_path(), &store)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(store)
    }

    pub fn create_session(&self, request: NewSessionRequest) -> Result<ChatSessionRecord> {
        let provider_store = self.active_provider_store()?;
        let session_id = self.allocate_session_id();
        let now = now_string();
        let session = ChatSessionRecord {
            id: session_id.clone(),
            title: request.title,
            provider_id: provider_store.active_provider.clone(),
            status: "active".to_string(),
            workspace_root: request.workspace_root,
            created_at: now.clone(),
            updated_at: now,
            message_count: 0,
            messages: Vec::new(),
        };
        write_json_file(&self.session_path(&session_id), &session)?;

        let mut history = self.session_history_store()?;
        history.sessions.retain(|entry| entry.id != session_id);
        history.sessions.push(SessionHistoryEntry {
            id: session.id.clone(),
            title: session.title.clone(),
            provider_id: session.provider_id.clone(),
            status: session.status.clone(),
            workspace_root: session.workspace_root.clone(),
            updated_at: session.updated_at.clone(),
            message_count: session.message_count,
        });
        write_json_file(&self.session_history_path(), &history)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(session)
    }

    pub fn append_message(
        &self,
        session_id: &str,
        role: &str,
        text: &str,
    ) -> Result<ChatSessionRecord> {
        let mut session = self.load_session(session_id)?;
        session.messages.push(SessionMessage {
            role: role.to_string(),
            text: text.to_string(),
            created_at: now_string(),
        });
        session.message_count = session.messages.len();
        session.updated_at = now_string();
        write_json_file(&self.session_path(session_id), &session)?;

        let mut history = self.session_history_store()?;
        if let Some(entry) = history
            .sessions
            .iter_mut()
            .find(|entry| entry.id == session_id)
        {
            entry.title = session.title.clone();
            entry.provider_id = session.provider_id.clone();
            entry.status = session.status.clone();
            entry.workspace_root = session.workspace_root.clone();
            entry.updated_at = session.updated_at.clone();
            entry.message_count = session.message_count;
        } else {
            history.sessions.push(SessionHistoryEntry {
                id: session.id.clone(),
                title: session.title.clone(),
                provider_id: session.provider_id.clone(),
                status: session.status.clone(),
                workspace_root: session.workspace_root.clone(),
                updated_at: session.updated_at.clone(),
                message_count: session.message_count,
            });
        }
        write_json_file(&self.session_history_path(), &history)?;
        let _ = self.write_runtime_snapshot()?;
        Ok(session)
    }

    pub fn load_session(&self, session_id: &str) -> Result<ChatSessionRecord> {
        let path = self.session_path(session_id);
        if !path.exists() {
            return Err(ControllerError::UnknownSession(session_id.to_string()));
        }
        read_json_file(&path)
    }

    pub fn write_runtime_snapshot(&self) -> Result<RuntimeSnapshot> {
        let providers = self.active_provider_store()?;
        let approvals = self.tool_approval_store()?;
        let history = self.session_history_store()?;

        let mut sorted_history = history.sessions.clone();
        sorted_history.sort_by(|left, right| right.updated_at.cmp(&left.updated_at));
        let recent = sorted_history.first();
        let recent_sessions = sorted_history
            .iter()
            .take(5)
            .map(|entry| {
                let session = self.load_session(&entry.id).ok();
                let last_message = session
                    .as_ref()
                    .and_then(|session| session.messages.last())
                    .cloned();
                RecentSessionSnapshot {
                    id: entry.id.clone(),
                    title: entry.title.clone(),
                    provider_id: entry.provider_id.clone(),
                    status: entry.status.clone(),
                    workspace_root: entry.workspace_root.clone(),
                    updated_at: entry.updated_at.clone(),
                    message_count: entry.message_count,
                    last_message_role: last_message.as_ref().map(|message| message.role.clone()),
                    last_message_preview: last_message
                        .map(|message| truncate_message_preview(&message.text, 96)),
                }
            })
            .collect::<Vec<_>>();
        let workspaces = summarize_workspaces(&sorted_history);

        let snapshot = RuntimeSnapshot {
            app_id: self.manifest.app_id.clone(),
            name: self.manifest.name.clone(),
            version: self.manifest.version.clone(),
            window_title: self.manifest.window_title.clone(),
            root_component: self.manifest.root_component.clone(),
            layout_id: self.manifest.layout_id.clone(),
            required_runtime_capabilities: self.manifest.required_runtime_capabilities.clone(),
            panels: self
                .panels
                .panels
                .iter()
                .map(|panel| PanelSnapshot {
                    id: panel.id.clone(),
                    title: panel.title.clone(),
                    dock: panel.dock.clone(),
                    kind: panel.kind.clone(),
                })
                .collect(),
            commands: self
                .commands
                .commands
                .iter()
                .map(|command| CommandSnapshot {
                    id: command.id.clone(),
                    label: command.label.clone(),
                    surface: command.surface.clone(),
                    intent: command.intent.clone(),
                })
                .collect(),
            providers: self
                .providers
                .providers
                .iter()
                .map(|provider| ProviderSnapshot {
                    id: provider.id.clone(),
                    label: provider.label.clone(),
                    transport: provider.transport.clone(),
                    profile_kind: provider.profile_kind.clone(),
                    supports_tools: provider.supports_tools,
                    supports_streaming: provider.supports_streaming,
                    active: providers.active_provider == provider.id,
                    profile_configured: providers.profiles.contains_key(&provider.id),
                    profile_keys: providers
                        .profiles
                        .get(&provider.id)
                        .and_then(profile_object_keys)
                        .unwrap_or_default(),
                })
                .collect(),
            tools: self
                .tools
                .tools
                .iter()
                .map(|tool| ToolSnapshot {
                    id: tool.id.clone(),
                    label: tool.label.clone(),
                    capability: tool.capability.clone(),
                    approval: tool.approval.clone(),
                    decision: approvals
                        .approvals
                        .iter()
                        .find(|entry| entry.tool_id == tool.id && entry.scope == "workspace")
                        .map(|entry| entry.decision.clone()),
                    scope_decisions: approvals
                        .approvals
                        .iter()
                        .filter(|entry| entry.tool_id == tool.id)
                        .map(|entry| ToolApprovalScopeSnapshot {
                            scope: entry.scope.clone(),
                            decision: entry.decision.clone(),
                            updated_at: entry.updated_at.clone(),
                        })
                        .collect(),
                })
                .collect(),
            sessions: SessionSnapshot {
                total_sessions: history.sessions.len(),
                active_provider: providers.active_provider,
                recent_session_id: recent.map(|entry| entry.id.clone()),
                recent_session_title: recent.map(|entry| entry.title.clone()),
            },
            recent_sessions,
            workspaces,
            updated_at: now_string(),
        };

        write_json_file(&self.runtime_snapshot_path(), &snapshot)?;
        Ok(snapshot)
    }

    pub fn runtime_snapshot_path(&self) -> PathBuf {
        self.state_root().join(RUNTIME_SNAPSHOT_FILE_NAME)
    }

    pub fn generated_shell_path(&self) -> PathBuf {
        self.app_root.join("generated").join("main.generated.kn")
    }

    pub fn write_generated_shell(&self) -> Result<PathBuf> {
        let snapshot = self.write_runtime_snapshot()?;
        let content = render_generated_shell(&snapshot);
        let output_path = self.generated_shell_path();
        if let Some(parent) = output_path.parent() {
            fs::create_dir_all(parent).map_err(|source| ControllerError::Io {
                path: parent.to_path_buf(),
                source,
            })?;
        }
        fs::write(&output_path, content).map_err(|source| ControllerError::Io {
            path: output_path.clone(),
            source,
        })?;
        Ok(output_path)
    }

    fn provider_store_path(&self) -> PathBuf {
        self.app_root
            .join(&self.manifest.persistence.provider_store)
    }

    fn session_store_dir(&self) -> PathBuf {
        self.app_root.join(&self.manifest.persistence.session_store)
    }

    fn workspace_store_dir(&self) -> PathBuf {
        self.app_root
            .join(&self.manifest.persistence.workspace_store)
    }

    fn state_root(&self) -> PathBuf {
        self.provider_store_path()
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| self.app_root.join("state"))
    }

    fn tool_approval_store_path(&self) -> PathBuf {
        self.state_root().join(TOOL_APPROVALS_FILE_NAME)
    }

    fn session_history_path(&self) -> PathBuf {
        self.state_root().join(TASK_HISTORY_FILE_NAME)
    }

    fn session_path(&self, session_id: &str) -> PathBuf {
        self.session_store_dir().join(format!("{session_id}.json"))
    }

    fn allocate_session_id(&self) -> String {
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|duration| duration.as_millis())
            .unwrap_or(0);
        format!("session-{timestamp}")
    }
}

fn read_json_file<T: for<'de> Deserialize<'de>>(path: &Path) -> Result<T> {
    let content = fs::read_to_string(path).map_err(|source| ControllerError::Io {
        path: path.to_path_buf(),
        source,
    })?;
    serde_json::from_str(&content).map_err(|source| ControllerError::Json {
        path: path.to_path_buf(),
        source,
    })
}

fn write_json_file<T: Serialize>(path: &Path, value: &T) -> Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|source| ControllerError::Io {
            path: parent.to_path_buf(),
            source,
        })?;
    }
    let content = serde_json::to_string_pretty(value).map_err(|source| ControllerError::Json {
        path: path.to_path_buf(),
        source,
    })?;
    fs::write(path, content).map_err(|source| ControllerError::Io {
        path: path.to_path_buf(),
        source,
    })
}

fn now_string() -> String {
    Utc::now().to_rfc3339()
}

fn profile_object_keys(value: &serde_json::Value) -> Option<Vec<String>> {
    value
        .as_object()
        .map(|object| object.keys().cloned().collect())
}

fn summarize_workspaces(sessions: &[SessionHistoryEntry]) -> Vec<WorkspaceSnapshot> {
    let mut by_root: BTreeMap<String, WorkspaceSnapshot> = BTreeMap::new();
    for session in sessions {
        let Some(root) = session.workspace_root.clone() else {
            continue;
        };
        by_root
            .entry(root.clone())
            .and_modify(|entry| {
                entry.session_count += 1;
                if entry.recent_session_title.is_none() {
                    entry.recent_session_title = Some(session.title.clone());
                }
            })
            .or_insert_with(|| WorkspaceSnapshot {
                root,
                session_count: 1,
                recent_session_title: Some(session.title.clone()),
            });
    }
    by_root.into_values().collect()
}

fn truncate_message_preview(value: &str, max_chars: usize) -> String {
    let collapsed = value.split_whitespace().collect::<Vec<_>>().join(" ");
    let mut chars = collapsed.chars();
    let preview = chars.by_ref().take(max_chars).collect::<String>();
    if chars.next().is_some() {
        format!("{preview}...")
    } else {
        preview
    }
}

fn render_generated_shell(snapshot: &RuntimeSnapshot) -> String {
    let workspace_title = panel_title(snapshot, "workspace_rail", "Navigator");
    let chat_title = panel_title(snapshot, "chat_surface", "Assistant");
    let control_title = panel_title(snapshot, "control_plane", "Workspace");
    let execution_title = panel_title(snapshot, "execution_lane", "Access");
    let workspace_lines = if snapshot.workspaces.is_empty() {
        vec![render_text_line(
            "muted",
            "No workspace roots attached yet.",
        )]
    } else {
        snapshot
            .workspaces
            .iter()
            .flat_map(|workspace| {
                vec![
                    render_text_line("title", &workspace.root),
                    render_text_line(
                        "caption",
                        &format!(
                            "{} sessions  |  recent {}",
                            workspace.session_count,
                            workspace.recent_session_title.as_deref().unwrap_or("none")
                        ),
                    ),
                ]
            })
            .collect()
    };
    let capability_lines = snapshot
        .required_runtime_capabilities
        .iter()
        .map(|capability| {
            render_text_line(
                "caption",
                &format!("{} ready", capability.replace('.', " / ")),
            )
        })
        .collect::<Vec<_>>();
    let session_lines = if snapshot.recent_sessions.is_empty() {
        vec![render_text_line("muted", "No chat sessions yet.")]
    } else {
        snapshot
            .recent_sessions
            .iter()
            .flat_map(|session| {
                let mut lines = vec![render_text_line("title", &session.title)];
                lines.push(render_text_line(
                    "caption",
                    &format!(
                        "{}  |  {} message{}",
                        session.provider_id,
                        session.message_count,
                        if session.message_count == 1 { "" } else { "s" }
                    ),
                ));
                if let Some(workspace_root) = &session.workspace_root {
                    lines.push(render_text_line("body", workspace_root));
                }
                if let Some(preview) = &session.last_message_preview {
                    let role = session.last_message_role.as_deref().unwrap_or("message");
                    lines.push(render_text_line("caption", &format!("{role}: {preview}")));
                }
                lines
            })
            .collect()
    };
    let provider_lines = snapshot
        .providers
        .iter()
        .flat_map(|provider| {
            let configured = if provider.profile_configured {
                format!("configured  |  keys {}", provider.profile_keys.join(", "))
            } else {
                "needs profile".to_string()
            };
            vec![
                render_text_line(
                    if provider.active { "title" } else { "body" },
                    &provider.label,
                ),
                render_text_line(
                    "caption",
                    &format!(
                        "{}  |  {}  |  tools {}  |  streaming {}",
                        provider.transport,
                        provider.profile_kind,
                        if provider.supports_tools { "on" } else { "off" },
                        if provider.supports_streaming {
                            "on"
                        } else {
                            "off"
                        }
                    ),
                ),
                render_text_line("caption", &configured),
            ]
        })
        .collect::<Vec<_>>();
    let command_lines = snapshot
        .commands
        .iter()
        .flat_map(|command| {
            vec![
                render_text_line("body", &command.label),
                render_text_line(
                    "caption",
                    &format!("{}  |  {}", command.intent, command.surface),
                ),
            ]
        })
        .collect::<Vec<_>>();
    let tool_lines = snapshot
        .tools
        .iter()
        .flat_map(|tool| {
            let mut lines = vec![render_text_line("body", &tool.label)];
            lines.push(render_text_line(
                "caption",
                &format!(
                    "{}  |  approval {}  |  effective {}",
                    tool.capability,
                    tool.approval,
                    tool.decision.as_ref().map(render_decision).unwrap_or("ask")
                ),
            ));
            if tool.scope_decisions.is_empty() {
                lines.push(render_text_line("muted", "no scoped approvals saved"));
            } else {
                for decision in &tool.scope_decisions {
                    lines.push(render_text_line(
                        "caption",
                        &format!(
                            "{} => {} ({})",
                            decision.scope,
                            render_decision(&decision.decision),
                            decision.updated_at
                        ),
                    ));
                }
            }
            lines
        })
        .collect::<Vec<_>>();

    format!(
        "component App():\n    render <slot>\n        <theme name=\"kade_desktop\">\n            <scope name=\"shell\" selector=\"kade-shell\" />\n            <token name=\"theme.background.top\" category=\"color\" value=\"#090c12\" />\n            <token name=\"theme.background.bottom\" category=\"color\" value=\"#101620\" />\n            <token name=\"theme.surface.default\" category=\"color\" value=\"#131a25\" />\n            <token name=\"theme.surface.alt\" category=\"color\" value=\"#1a2230\" />\n            <token name=\"theme.surface.raised\" category=\"color\" value=\"#232c3b\" />\n            <token name=\"theme.outline.soft\" category=\"color\" value=\"#3f4a5d\" />\n            <token name=\"theme.outline.bright\" category=\"color\" value=\"#d7a56a\" />\n            <token name=\"theme.accent.primary\" category=\"color\" value=\"#c98847\" />\n            <token name=\"theme.accent.soft\" category=\"color\" value=\"#edc496\" />\n            <token name=\"text.default\" category=\"color\" value=\"#f5efe6\" />\n            <token name=\"theme.typography.scale\" category=\"type\" value={{1.08}} />\n            <token name=\"theme.spacing.scale\" category=\"space\" value={{1.02}} />\n            <token name=\"theme.radius.scale\" category=\"radius\" value={{1.12}} />\n            <token name=\"theme.chrome.topbar.visible\" category=\"state\" value={{true}} />\n            <token name=\"theme.chrome.inspector.visible\" category=\"state\" value={{false}} />\n            <token name=\"widget.panel.surface.stroke\" category=\"color\" value=\"#00000000\" />\n            <token name=\"widget.inspector.surface.mode\" category=\"surface\" value=\"ghost\" />\n            <token name=\"widget.inspector.surface.stroke\" category=\"color\" value=\"#00000000\" />\n            <token name=\"widget.tree.surface.mode\" category=\"surface\" value=\"ghost\" />\n            <token name=\"widget.tree.surface.stroke\" category=\"color\" value=\"#00000000\" />\n            <variant scope=\"shell\" name=\"shell_root\">\n                <token name=\"title.visible\" category=\"state\" value={{false}} />\n                <token name=\"surface.mode\" category=\"surface\" value=\"ghost\" />\n                <token name=\"surface.padding\" category=\"space\" value={{6}} />\n            </variant>\n            <variant scope=\"shell\" name=\"sidebar\">\n                <token name=\"surface.mode\" category=\"surface\" value=\"flat\" />\n                <token name=\"surface.padding\" category=\"space\" value={{14}} />\n            </variant>\n            <variant scope=\"shell\" name=\"workspace\">\n                <token name=\"surface.mode\" category=\"surface\" value=\"ghost\" />\n                <token name=\"surface.padding\" category=\"space\" value={{10}} />\n            </variant>\n            <variant scope=\"shell\" name=\"stage\">\n                <token name=\"surface.mode\" category=\"surface\" value=\"glass\" />\n                <token name=\"surface.padding\" category=\"space\" value={{20}} />\n                <token name=\"title.visible\" category=\"state\" value={{false}} />\n            </variant>\n            <variant scope=\"shell\" name=\"tray\">\n                <token name=\"surface.mode\" category=\"surface\" value=\"flat\" />\n                <token name=\"surface.padding\" category=\"space\" value={{14}} />\n            </variant>\n            <textvariant scope=\"shell\" name=\"hero\">\n                <token name=\"body.size\" category=\"type\" value={{34}} />\n            </textvariant>\n        </theme>\n        <panel title=\"{title}\" scope=\"shell\" variant=\"shell_root\" layout=\"dock\" persistent_layout_id=\"{layout_id}\" gap={{18}} padding={{18}}>\n            <panel title=\"{workspace_title}\" scope=\"shell\" variant=\"sidebar\" dock=\"left\" split_ratio={{0.22}} min_width={{240}} max_width={{360}} resizable={{true}}>\n                <inspector title=\"Projects\">\n{workspace_lines}\n                </inspector>\n                <inspector title=\"Runtime\">\n{capability_lines}\n                </inspector>\n            </panel>\n            <panel title=\"{chat_title}\" scope=\"shell\" variant=\"stage\" dock=\"center\" gap={{16}}>\n                <panel title=\"Session Overview\" scope=\"shell\" variant=\"workspace\" layout=\"column\" gap={{10}}>\n                    <text role=\"caption\">{{\"Native desktop assistant\"}}</text>\n                    <text role=\"hero\">{{\"{hero_text}\"}}</text>\n                    <text role=\"body\">{{\"{caption_text}\"}}</text>\n                </panel>\n                <panel title=\"Conversation\" scope=\"shell\" variant=\"workspace\" layout=\"column\" gap={{12}}>\n                    <inspector title=\"Recent Sessions\">\n{session_lines}\n                    </inspector>\n                </panel>\n            </panel>\n            <panel title=\"{control_title}\" scope=\"shell\" variant=\"sidebar\" dock=\"right\" split_ratio={{0.24}} min_width={{280}} max_width={{380}} resizable={{true}}>\n                <inspector title=\"Models\">\n{provider_lines}\n                </inspector>\n                <inspector title=\"Quick Actions\">\n{command_lines}\n                </inspector>\n            </panel>\n            <panel title=\"{execution_title}\" scope=\"shell\" variant=\"tray\" dock=\"bottom\" split_ratio={{0.22}} min_height={{180}} max_height={{320}} resizable={{true}}>\n                <inspector title=\"Access Rules\">\n{tool_lines}\n                </inspector>\n            </panel>\n        </panel>\n    </slot>\n",
        title = escape_kn_attr(&snapshot.window_title),
        layout_id = escape_kn_attr(&snapshot.layout_id),
        workspace_title = escape_kn_attr(workspace_title),
        chat_title = escape_kn_attr(chat_title),
        control_title = escape_kn_attr(control_title),
        execution_title = escape_kn_attr(execution_title),
        hero_text = escape_kn_text(
            snapshot
                .sessions
                .recent_session_title
                .as_deref()
                .unwrap_or(&snapshot.name),
        ),
        caption_text = escape_kn_text(
            "Workspace-aware native desktop shell with provider routing, saved session context, and governed tool access.",
        ),
        workspace_lines = workspace_lines.join("\n"),
        capability_lines = capability_lines.join("\n"),
        session_lines = session_lines.join("\n"),
        provider_lines = provider_lines.join("\n"),
        command_lines = command_lines.join("\n"),
        tool_lines = tool_lines.join("\n"),
    )
}

fn render_decision(decision: &ToolApprovalDecision) -> &'static str {
    match decision {
        ToolApprovalDecision::Allow => "allow",
        ToolApprovalDecision::Deny => "deny",
        ToolApprovalDecision::Ask => "ask",
    }
}

fn panel_title<'a>(snapshot: &'a RuntimeSnapshot, panel_id: &str, fallback: &'a str) -> &'a str {
    snapshot
        .panels
        .iter()
        .find(|panel| panel.id == panel_id)
        .map(|panel| panel.title.as_str())
        .unwrap_or(fallback)
}

fn render_text_line(role: &str, value: &str) -> String {
    format!(
        "                    <text role=\"{role}\">{{\"{value}\"}}</text>",
        role = role,
        value = escape_kn_text(value)
    )
}

fn escape_kn_text(value: &str) -> String {
    value
        .replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\r', " ")
        .replace('\n', " ")
}

fn escape_kn_attr(value: &str) -> String {
    escape_kn_text(value)
}
