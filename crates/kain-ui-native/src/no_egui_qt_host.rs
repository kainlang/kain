use std::{
    env,
    error::Error,
    fmt, fs,
    io,
    path::PathBuf,
    process::{Command, ExitStatus},
    time::{SystemTime, UNIX_EPOCH},
};

use crate::{
    no_egui::KainUiNativeBackendPlan,
    no_egui_session::{build_qt_quick_session_manifest, QtQuickSessionManifest},
};
use kain_ui::UiRuntimeBundle;

const KAIN_UI_NATIVE_QT_RUNTIME_ENV: &str = "KAIN_UI_NATIVE_QT_RUNTIME";
const KAIN_QT_QML_RUNTIME_ENV: &str = "KAIN_QT_QML_RUNTIME";

#[derive(Debug)]
pub enum QtQuickHostLaunchError {
    QtRuntimeUnavailable { searched: Vec<String> },
    Io {
        context: &'static str,
        source: io::Error,
    },
    ManifestSerialization(serde_json::Error),
    ProcessLaunch {
        program: PathBuf,
        source: io::Error,
    },
    ProcessFailed {
        program: PathBuf,
        status: ExitStatus,
    },
}

impl fmt::Display for QtQuickHostLaunchError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::QtRuntimeUnavailable { searched } => write!(
                formatter,
                "no Qt Quick runtime was found for the default non-egui host; searched {}",
                searched.join(", ")
            ),
            Self::Io { context, source } => {
                write!(formatter, "failed to {} for the Qt host: {}", context, source)
            }
            Self::ManifestSerialization(source) => {
                write!(formatter, "failed to serialize the Qt session manifest: {}", source)
            }
            Self::ProcessLaunch { program, source } => write!(
                formatter,
                "failed to launch the Qt runtime `{}`: {}",
                program.display(),
                source
            ),
            Self::ProcessFailed { program, status } => write!(
                formatter,
                "the Qt runtime `{}` exited unsuccessfully: {}",
                program.display(),
                status
            ),
        }
    }
}

impl Error for QtQuickHostLaunchError {}

pub fn launch_qt_quick_host(
    bundle: &UiRuntimeBundle,
    backend_plan: &KainUiNativeBackendPlan,
) -> Result<(), QtQuickHostLaunchError> {
    let manifest = build_qt_quick_session_manifest(bundle, backend_plan);
    let qt_runtime = detect_qt_runtime()?;
    let artifact_dir = create_qt_artifact_dir()?;
    let session_json_path = artifact_dir.join("session.json");
    let main_qml_path = artifact_dir.join("Main.qml");
    let manifest_json =
        serde_json::to_string_pretty(&manifest).map_err(QtQuickHostLaunchError::ManifestSerialization)?;

    fs::write(&session_json_path, manifest_json.as_bytes()).map_err(|source| {
        QtQuickHostLaunchError::Io {
            context: "write the Qt session manifest",
            source,
        }
    })?;
    fs::write(&main_qml_path, render_main_qml(&manifest).as_bytes()).map_err(|source| {
        QtQuickHostLaunchError::Io {
            context: "write the generated Main.qml host file",
            source,
        }
    })?;

    let status = Command::new(&qt_runtime)
        .arg(&main_qml_path)
        .current_dir(&artifact_dir)
        .env("KAIN_UI_NATIVE_QT_SESSION_MANIFEST", &session_json_path)
        .env("QT_QUICK_CONTROLS_STYLE", "Basic")
        .status()
        .map_err(|source| QtQuickHostLaunchError::ProcessLaunch {
            program: qt_runtime.clone(),
            source,
        })?;

    if !status.success() {
        return Err(QtQuickHostLaunchError::ProcessFailed {
            program: qt_runtime,
            status,
        });
    }

    Ok(())
}

fn detect_qt_runtime() -> Result<PathBuf, QtQuickHostLaunchError> {
    let mut searched = Vec::new();
    for env_key in [KAIN_UI_NATIVE_QT_RUNTIME_ENV, KAIN_QT_QML_RUNTIME_ENV] {
        if let Some(path) = env::var_os(env_key) {
            let candidate = PathBuf::from(path);
            searched.push(format!("{}={}", env_key, candidate.display()));
            if candidate.is_file() {
                return Ok(candidate);
            }
        }
    }

    for binary in ["qmlscene", "qml"] {
        searched.push(binary.to_string());
        if let Some(path) = find_binary_in_path(binary) {
            return Ok(path);
        }
    }

    Err(QtQuickHostLaunchError::QtRuntimeUnavailable { searched })
}

fn find_binary_in_path(binary_name: &str) -> Option<PathBuf> {
    let path_var = env::var_os("PATH")?;
    for directory in env::split_paths(&path_var) {
        let candidate = directory.join(binary_name);
        if candidate.is_file() {
            return Some(candidate);
        }
        #[cfg(windows)]
        {
            let exe_candidate = directory.join(format!("{}.exe", binary_name));
            if exe_candidate.is_file() {
                return Some(exe_candidate);
            }
        }
    }
    None
}

fn create_qt_artifact_dir() -> Result<PathBuf, QtQuickHostLaunchError> {
    let dir = env::temp_dir().join(format!(
        "kain-ui-native-qt-{}-{}",
        std::process::id(),
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis()
    ));
    fs::create_dir_all(&dir).map_err(|source| QtQuickHostLaunchError::Io {
        context: "create the Qt host temp directory",
        source,
    })?;
    Ok(dir)
}

fn render_main_qml(manifest: &QtQuickSessionManifest) -> String {
    let session_json = serde_json::to_string_pretty(manifest).unwrap_or_else(|_| "{}".to_string());
    format!(
        r##"import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {{
    id: root
    visible: true
    width: Math.max(960, kainSession.initial_window_size[0])
    height: Math.max(640, kainSession.initial_window_size[1])
    title: kainSession.window_title
    color: "#10161d"
    readonly property var kainSession: ({session_json})

    function sectionTitle(text) {{
        return text + " (" + (arguments.length > 1 ? arguments[1] : "") + ")"
    }}

    header: ToolBar {{
        contentHeight: 42
        RowLayout {{
            anchors.fill: parent
            anchors.margins: 10
            Label {{
                text: kainSession.window_title
                color: "#f3f7fb"
                font.pixelSize: 18
                font.bold: true
            }}
            Item {{ Layout.fillWidth: true }}
            Label {{
                text: "shell=" + kainSession.shell_backend + "  layout=" + kainSession.layout_engine + "  render=" + kainSession.render_engine
                color: "#a5bbcd"
            }}
        }}
    }}

    footer: Frame {{
        padding: 10
        background: Rectangle {{ color: "#16212c" }}
        contentItem: Label {{
            text: "Qt Quick host session: document/document-devtools lanes are metadata-backed now; viewport/devtools embeddings degrade to explicit placeholders until their adapters land."
            wrapMode: Text.Wrap
            color: "#9bb1c3"
        }}
    }}

    component PaneCard: Frame {{
        required property var paneData
        Layout.fillWidth: true
        padding: 12
        background: Rectangle {{
            radius: 12
            color: "#16212c"
            border.width: 1
            border.color: "#2b4357"
        }}
        contentItem: ColumnLayout {{
            spacing: 8
            Label {{
                text: paneData.title
                color: "#f3f7fb"
                font.pixelSize: 17
                font.bold: true
                wrapMode: Text.Wrap
            }}
            Label {{
                text: paneData.summary
                color: "#9bd2ff"
                wrapMode: Text.Wrap
            }}
            Rectangle {{
                Layout.fillWidth: true
                implicitHeight: adapterLabel.implicitHeight + 12
                radius: 8
                color: "#203346"
                Label {{
                    id: adapterLabel
                    anchors.fill: parent
                    anchors.margins: 6
                    text: paneData.adapter_state_label
                    wrapMode: Text.Wrap
                    color: "#d4e6f5"
                }}
            }}
            Repeater {{
                model: paneData.detail_lines
                delegate: Label {{
                    required property string modelData
                    text: "\u2022 " + modelData
                    color: "#8ea4b6"
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }}
            }}
        }}
    }}

    ScrollView {{
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {{
            width: parent.width
            spacing: 14
            anchors.margins: 14

            Frame {{
                Layout.fillWidth: true
                padding: 12
                background: Rectangle {{
                    radius: 14
                    color: "#16212c"
                    border.width: 1
                    border.color: "#31526a"
                }}
                contentItem: ColumnLayout {{
                    spacing: 8
                    Label {{
                        text: "Kain Qt Hybrid Session"
                        color: "#f3f7fb"
                        font.pixelSize: 20
                        font.bold: true
                    }}
                    Repeater {{
                        model: kainSession.summary_lines
                        delegate: Label {{
                            required property string modelData
                            text: modelData
                            color: "#9bb1c3"
                            wrapMode: Text.Wrap
                        }}
                    }}
                }}
            }}

            SplitView {{
                Layout.fillWidth: true
                Layout.preferredHeight: 720
                orientation: Qt.Horizontal

                Frame {{
                    SplitView.fillWidth: true
                    padding: 10
                    background: Rectangle {{ color: "#121b24"; radius: 14 }}
                    contentItem: ScrollView {{
                        contentWidth: availableWidth
                        ColumnLayout {{
                            width: parent.width
                            spacing: 10
                            Label {{
                                text: "Documents"
                                color: "#f3f7fb"
                                font.pixelSize: 18
                                font.bold: true
                            }}
                            Repeater {{
                                model: kainSession.document_panes
                                delegate: PaneCard {{
                                    paneData: modelData
                                }}
                            }}
                        }}
                    }}
                }}

                Frame {{
                    SplitView.fillWidth: true
                    padding: 10
                    background: Rectangle {{ color: "#121b24"; radius: 14 }}
                    contentItem: ScrollView {{
                        contentWidth: availableWidth
                        ColumnLayout {{
                            width: parent.width
                            spacing: 10
                            Label {{
                                text: "Viewport"
                                color: "#f3f7fb"
                                font.pixelSize: 18
                                font.bold: true
                            }}
                            Repeater {{
                                model: kainSession.viewport_panes
                                delegate: PaneCard {{
                                    paneData: modelData
                                }}
                            }}
                        }}
                    }}
                }}

                Frame {{
                    SplitView.fillWidth: true
                    padding: 10
                    background: Rectangle {{ color: "#121b24"; radius: 14 }}
                    contentItem: ScrollView {{
                        contentWidth: availableWidth
                        ColumnLayout {{
                            width: parent.width
                            spacing: 10
                            Label {{
                                text: "Devtools"
                                color: "#f3f7fb"
                                font.pixelSize: 18
                                font.bold: true
                            }}
                            Repeater {{
                                model: kainSession.devtools_panes
                                delegate: PaneCard {{
                                    paneData: modelData
                                }}
                            }}

                            Label {{
                                visible: kainSession.fallback_panes.length > 0
                                text: "Fallback"
                                color: "#f3f7fb"
                                font.pixelSize: 18
                                font.bold: true
                            }}
                            Repeater {{
                                model: kainSession.fallback_panes
                                delegate: PaneCard {{
                                    paneData: modelData
                                }}
                            }}
                        }}
                    }}
                }}
            }}
        }}
    }}
}}
"##
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        no_egui::KainUiNativeBackendPlan,
        no_egui_session::build_qt_quick_session_manifest,
    };
    use kain_ui::{ui_runtime_bundle_from_output, UiBuildOutput, UiRuntimeMetadata};

    #[test]
    fn generated_qml_contains_session_title() {
        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                window_title: "Qt Runtime".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            UiBuildOutput::default(),
        );
        let manifest = build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());
        let qml = render_main_qml(&manifest);
        assert!(qml.contains("Kain Qt Hybrid Session"));
        assert!(qml.contains("Qt Runtime"));
    }

    #[test]
    fn detect_qt_runtime_reports_missing_runtime_cleanly() {
        let result = detect_qt_runtime();
        if let Err(QtQuickHostLaunchError::QtRuntimeUnavailable { searched }) = result {
            assert!(!searched.is_empty());
        }
    }
}
