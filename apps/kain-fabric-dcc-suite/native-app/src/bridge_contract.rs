use std::path::PathBuf;

pub const RUNTIME_BUNDLE_ENV: &str = "KAIN_UI_NATIVE_RUNTIME_BUNDLE";
pub const REALTIME_BUNDLE_ENV: &str = "KAIN_UI_NATIVE_REALTIME_BUNDLE";
pub const APP_MANIFEST_ENV: &str = "KAIN_UI_NATIVE_APP_MANIFEST";
pub const APP_SNAPSHOT_ENV: &str = "KAIN_UI_NATIVE_APP_SNAPSHOT";

pub const CONTRACT_ROOT_URI: &str = "bridge://kain-fabric-dcc-suite/native";
pub const CONTRACT_ROOT_REPORT_URI: &str = "report://bridge/native-contract";

pub const TOPIC_TOPOLOGY: &str = "topology";
pub const TOPIC_SCULPT: &str = "sculpt";
pub const TOPIC_MESH: &str = "mesh";
pub const TOPIC_RENDER: &str = "render";
pub const TOPIC_RUST: &str = "rust";

#[derive(Clone, Debug)]
pub struct NativeBridgeSeam {
    pub topic: &'static str,
    pub command_prefix: &'static str,
    pub active_dirty_key: &'static str,
    pub report_key: &'static str,
}

#[derive(Clone, Debug)]
pub struct NativeBridgeContract {
    pub root_uri: &'static str,
    pub report_uri: &'static str,
    pub seam_paths: &'static [NativeBridgeSeam],
}

pub const NATIVE_BRIDGE_SEAMS: &[NativeBridgeSeam] = &[
    NativeBridgeSeam {
        topic: TOPIC_TOPOLOGY,
        command_prefix: "topology.",
        active_dirty_key: "topology_dirty",
        report_key: "topology_history_report",
    },
    NativeBridgeSeam {
        topic: TOPIC_SCULPT,
        command_prefix: "sculpt.",
        active_dirty_key: "sculpt_dirty",
        report_key: "sculpt_report",
    },
    NativeBridgeSeam {
        topic: TOPIC_MESH,
        command_prefix: "mesh.",
        active_dirty_key: "topology_dirty",
        report_key: "mesh_contract_report",
    },
    NativeBridgeSeam {
        topic: TOPIC_RENDER,
        command_prefix: "render.",
        active_dirty_key: "render_dirty",
        report_key: "render_report",
    },
    NativeBridgeSeam {
        topic: TOPIC_RUST,
        command_prefix: "rust.",
        active_dirty_key: "session_needs_save",
        report_key: "rust_helper_report",
    },
];

impl NativeBridgeContract {
    pub const fn new() -> Self {
        Self {
            root_uri: CONTRACT_ROOT_URI,
            report_uri: CONTRACT_ROOT_REPORT_URI,
            seam_paths: NATIVE_BRIDGE_SEAMS,
        }
    }

    pub fn seam_for_command(&self, command_id: &str) -> Option<&NativeBridgeSeam> {
        self.seam_paths
            .iter()
            .find(|seam| command_id.starts_with(seam.command_prefix))
    }

    pub fn command_environment_pairs(&self) -> [(&'static str, &'static str); 4] {
        [
            (RUNTIME_BUNDLE_ENV, "native_app_bundle.json"),
            (REALTIME_BUNDLE_ENV, "kain_realtime_app_bundle.json"),
            (APP_MANIFEST_ENV, "config/app_manifest.json"),
            (APP_SNAPSHOT_ENV, "state/runtime_snapshot.json"),
        ]
    }
}

pub fn resolve_bundle_path(file_name: &str, manifest_dir: &str) -> PathBuf {
    PathBuf::from(manifest_dir).join("generated").join(file_name)
}

pub fn resolve_project_path(manifest_dir: &str, relative_source_path: &str) -> PathBuf {
    PathBuf::from(manifest_dir).join(relative_source_path)
}
