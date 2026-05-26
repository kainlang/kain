use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ProcessStdioMode {
    Inherit,
    Pipe,
    Null,
}

impl Default for ProcessStdioMode {
    fn default() -> Self {
        Self::Inherit
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ProcessEnvironmentEntry {
    pub key: String,
    pub value: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ProcessSpec {
    pub executable: String,
    pub arguments: Vec<String>,
    pub current_working_directory: Option<String>,
    pub environment: Vec<ProcessEnvironmentEntry>,
    pub inherit_environment: bool,
    pub stdin_mode: ProcessStdioMode,
    pub stdout_mode: ProcessStdioMode,
    pub stderr_mode: ProcessStdioMode,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct CurrentProcessInfo {
    pub executable_path: String,
    pub arguments: Vec<String>,
    pub current_working_directory: String,
    pub operating_system_process_id: Option<i64>,
}

impl ProcessSpec {
    pub fn new(executable: impl Into<String>) -> Self {
        Self {
            executable: executable.into(),
            arguments: Vec::new(),
            current_working_directory: None,
            environment: Vec::new(),
            inherit_environment: true,
            stdin_mode: ProcessStdioMode::Inherit,
            stdout_mode: ProcessStdioMode::Inherit,
            stderr_mode: ProcessStdioMode::Inherit,
        }
    }

    pub fn with_argument(mut self, argument: impl Into<String>) -> Self {
        self.arguments.push(argument.into());
        self
    }

    pub fn with_environment(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.environment.push(ProcessEnvironmentEntry {
            key: key.into(),
            value: value.into(),
        });
        self
    }

    pub fn with_current_working_directory(mut self, path: impl Into<String>) -> Self {
        self.current_working_directory = Some(path.into());
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct ProcessHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct PtyHandle {
    pub id: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct PtySize {
    pub columns: i64,
    pub rows: i64,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PtySpec {
    pub process: ProcessSpec,
    pub size: PtySize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ProcessLifecycleState {
    Running,
    Exited,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ProcessStatus {
    pub state: ProcessLifecycleState,
    pub exit_code: Option<i64>,
    pub operating_system_process_id: Option<i64>,
    pub is_pty: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct ProcessOutput {
    pub stdout_text: String,
    pub stderr_text: String,
    pub stdout_hex: String,
    pub stderr_hex: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct PtyOutput {
    pub text: String,
    pub hex: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Error)]
pub enum ProcessError {
    #[error("invalid process specification")]
    InvalidSpec,
    #[error("invalid process handle")]
    InvalidHandle,
    #[error("process subsystem capacity exceeded")]
    CapacityExceeded,
    #[error("process subsystem does not support this platform capability")]
    UnsupportedPlatform,
    #[error("process subsystem I/O channel is not available")]
    MissingPipe,
    #[error("{message}")]
    Message { message: String },
}

#[cfg(test)]
mod tests {
    use super::{CurrentProcessInfo, ProcessSpec, ProcessStdioMode, PtySize};

    #[test]
    fn process_spec_defaults_to_inherit_modes_and_environment() {
        let spec = ProcessSpec::new("cmd.exe");
        assert!(spec.inherit_environment);
        assert_eq!(spec.stdin_mode, ProcessStdioMode::Inherit);
        assert_eq!(spec.stdout_mode, ProcessStdioMode::Inherit);
        assert_eq!(spec.stderr_mode, ProcessStdioMode::Inherit);
        assert!(spec.arguments.is_empty());
        assert!(spec.environment.is_empty());
        assert!(spec.current_working_directory.is_none());
    }

    #[test]
    fn process_spec_builder_accumulates_arguments_and_environment() {
        let spec = ProcessSpec::new("tool")
            .with_argument("--serve")
            .with_argument("stdio")
            .with_environment("KAIN_MODE", "native")
            .with_current_working_directory("repo");
        assert_eq!(
            spec.arguments,
            vec!["--serve".to_string(), "stdio".to_string()]
        );
        assert_eq!(spec.environment.len(), 1);
        assert_eq!(spec.environment[0].key, "KAIN_MODE");
        assert_eq!(spec.environment[0].value, "native");
        assert_eq!(spec.current_working_directory.as_deref(), Some("repo"));
    }

    #[test]
    fn pty_size_is_plain_data() {
        let size = PtySize {
            columns: 120,
            rows: 40,
        };
        assert_eq!(size.columns, 120);
        assert_eq!(size.rows, 40);
    }

    #[test]
    fn current_process_info_is_plain_data() {
        let info = CurrentProcessInfo {
            executable_path: "repo/kg.exe".to_string(),
            arguments: vec!["kg.exe".to_string(), "needle".to_string()],
            current_working_directory: "repo".to_string(),
            operating_system_process_id: Some(77),
        };
        assert_eq!(info.arguments.len(), 2);
        assert_eq!(info.arguments[1], "needle");
        assert_eq!(info.current_working_directory, "repo");
        assert_eq!(info.operating_system_process_id, Some(77));
    }
}
