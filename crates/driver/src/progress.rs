//! Progress events emitted during vcpkg package fetching.
//!
//! These events are sent to the `ToolingProgressSink` during
//! vcpkg on-demand install operations, enabling the CLI to show
//! live status updates.

/// Event emitted during a vcpkg package fetch operation.
#[derive(Debug, Clone)]
pub enum PackageFetchEvent {
    /// vcpkg binary has been located.
    VcpkgFound {
        path: String,
    },
    /// A package install operation has started.
    InstallStarted {
        package: String,
        version: String,
        triple: String,
    },
    /// A package install completed successfully.
    InstallCompleted {
        package: String,
        version: String,
    },
    /// A package was already installed (sentinel file found).
    InstallSkipped {
        package: String,
        version: String,
    },
}

impl PackageFetchEvent {
    /// Human-readable description of this event.
    pub fn description(&self) -> String {
        match self {
            Self::VcpkgFound { path } => format!("vcpkg found at {path}"),
            Self::InstallStarted {
                package,
                version,
                triple,
            } => {
                format!("installing {package}:{triple}@{version} via vcpkg...")
            }
            Self::InstallCompleted { package, version } => {
                format!("installed {package} {version}")
            }
            Self::InstallSkipped { package, version } => {
                format!("{package} {version} already installed (cached)")
            }
        }
    }
}
