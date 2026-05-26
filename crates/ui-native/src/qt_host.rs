use std::{
    env,
    error::Error,
    fmt, fs, io,
    path::{Path, PathBuf},
    process::{Command, ExitStatus},
    time::{SystemTime, UNIX_EPOCH},
};

use crate::{
    app::KainUiNativeBackendPlan,
    session::{build_qt_quick_session_manifest, KainUiNativeSessionManifest},
};
use kain_ui::UiRuntimeBundle;

const KAIN_UI_NATIVE_QT_RUNTIME_ENV: &str = "KAIN_UI_NATIVE_QT_RUNTIME";
const KAIN_QT_QML_RUNTIME_ENV: &str = "KAIN_QT_QML_RUNTIME";
const KAIN_UI_NATIVE_QT_ARTIFACT_DIR_ENV: &str = "KAIN_UI_NATIVE_QT_ARTIFACT_DIR";
const KAIN_UI_NATIVE_QT_SCREENSHOT_PATH_ENV: &str = "KAIN_UI_NATIVE_QT_SCREENSHOT_PATH";

#[derive(Debug)]
pub enum QtQuickHostLaunchError {
    QtRuntimeUnavailable {
        searched: Vec<String>,
    },
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
                "no Qt Quick runtime was found for the native UI host; searched {}",
                searched.join(", ")
            ),
            Self::Io { context, source } => {
                write!(
                    formatter,
                    "failed to {} for the Qt host: {}",
                    context, source
                )
            }
            Self::ManifestSerialization(source) => {
                write!(
                    formatter,
                    "failed to serialize the Qt session manifest: {}",
                    source
                )
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
    let screenshot_path = detect_optional_screenshot_path()?;
    let artifact_dir = create_qt_artifact_dir()?;
    let session_json_path = artifact_dir.join("session.json");
    let main_qml_path = artifact_dir.join("Main.qml");
    let manifest_json = serde_json::to_string_pretty(&manifest)
        .map_err(QtQuickHostLaunchError::ManifestSerialization)?;

    fs::write(&session_json_path, manifest_json.as_bytes()).map_err(|source| {
        QtQuickHostLaunchError::Io {
            context: "write the Qt session manifest",
            source,
        }
    })?;
    fs::write(
        &main_qml_path,
        render_main_qml(&manifest, screenshot_path.as_deref()).as_bytes(),
    )
    .map_err(|source| QtQuickHostLaunchError::Io {
        context: "write the generated Main.qml host file",
        source,
    })?;

    let quick_controls_style =
        env::var("QT_QUICK_CONTROLS_STYLE").unwrap_or_else(|_| "Basic".to_string());

    let status = Command::new(&qt_runtime)
        .arg(&main_qml_path)
        .current_dir(&artifact_dir)
        .env("KAIN_UI_NATIVE_QT_SESSION_MANIFEST", &session_json_path)
        .env("QT_QUICK_CONTROLS_STYLE", quick_controls_style)
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

fn detect_optional_screenshot_path() -> Result<Option<PathBuf>, QtQuickHostLaunchError> {
    let Some(path) = env::var_os(KAIN_UI_NATIVE_QT_SCREENSHOT_PATH_ENV) else {
        return Ok(None);
    };

    let mut screenshot_path = PathBuf::from(path);
    if screenshot_path.is_relative() {
        screenshot_path = env::current_dir()
            .map_err(|source| QtQuickHostLaunchError::Io {
                context: "resolve the current working directory for the screenshot path",
                source,
            })?
            .join(screenshot_path);
    }

    if let Some(parent) = screenshot_path.parent() {
        fs::create_dir_all(parent).map_err(|source| QtQuickHostLaunchError::Io {
            context: "create the screenshot output directory",
            source,
        })?;
    }

    Ok(Some(screenshot_path))
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
    if let Some(configured_dir) = env::var_os(KAIN_UI_NATIVE_QT_ARTIFACT_DIR_ENV) {
        let dir = PathBuf::from(configured_dir);
        fs::create_dir_all(&dir).map_err(|source| QtQuickHostLaunchError::Io {
            context: "create the configured Qt host artifact directory",
            source,
        })?;
        return Ok(dir);
    }

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

fn render_main_qml(
    manifest: &KainUiNativeSessionManifest,
    screenshot_path: Option<&Path>,
) -> String {
    let session_json = serde_json::to_string_pretty(manifest).unwrap_or_else(|_| "{}".to_string());
    let screenshot_json = serde_json::to_string(
        &screenshot_path
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default(),
    )
    .unwrap_or_else(|_| "\"\"".to_string());

    AUTHORED_QML_HOST
        .replace("__KAIN_SESSION_JSON__", &session_json)
        .replace("__KAIN_SCREENSHOT_PATH__", &screenshot_json)
}

const AUTHORED_QML_HOST: &str = r##"import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: Math.max(320, kainSession.initial_window_size[0])
    height: Math.max(240, kainSession.initial_window_size[1])
    title: kainSession.window_title
    color: "#07090c"

    readonly property var kainSession: (__KAIN_SESSION_JSON__)
    readonly property string screenshotPath: __KAIN_SCREENSHOT_PATH__
    readonly property var nativeProjection: kainSession.native_projection ? kainSession.native_projection : ({ "nodes": [] })
    readonly property bool hasAuthoredNodes: nativeProjection.nodes && nativeProjection.nodes.length > 0

    function projectionNodes() {
        return (nativeProjection && nativeProjection.nodes) ? nativeProjection.nodes : []
    }

    function projectionRootNode() {
        const nodes = projectionNodes()
        if (nativeProjection && nativeProjection.root_id !== undefined && nativeProjection.root_id !== null) {
            for (let index = 0; index < nodes.length; index += 1) {
                if (nodes[index].id === nativeProjection.root_id) {
                    return nodes[index]
                }
            }
        }
        for (let index = 0; index < nodes.length; index += 1) {
            const node = nodes[index]
            if (node.parent_id === undefined || node.parent_id === null) {
                return node
            }
        }
        return null
    }

    function projectionChildren(parentId) {
        const children = []
        const nodes = projectionNodes()
        for (let index = 0; index < nodes.length; index += 1) {
            const node = nodes[index]
            if (node.parent_id === parentId) {
                children.push(node)
            }
        }
        return children
    }

    function displayNodes() {
        const rootNode = projectionRootNode()
        if (!rootNode) {
            return []
        }
        if (rootNode.kind === "ComponentRef") {
            const children = projectionChildren(rootNode.id)
            return children.length > 0 ? children : []
        }
        return [rootNode]
    }

    function nodeChildren(nodeData) {
        return nodeData && nodeData.id !== undefined ? projectionChildren(nodeData.id) : []
    }

    function authoredLabel(nodeData) {
        if (!nodeData) {
            return ""
        }
        if (nodeData.title && nodeData.title.length > 0) {
            return nodeData.title
        }
        if (nodeData.text && nodeData.text.length > 0) {
            return nodeData.text
        }
        if (nodeData.tag && nodeData.tag.length > 0) {
            return nodeData.tag
        }
        if (nodeData.scene && nodeData.scene.length > 0) {
            return nodeData.scene
        }
        return nodeData.kind ? nodeData.kind.toString() : ""
    }

    function authoredText(nodeData) {
        return (nodeData && nodeData.text && nodeData.text.length > 0) ? nodeData.text : ""
    }

    Timer {
        interval: 600
        repeat: false
        running: root.screenshotPath.length > 0
        onTriggered: root.contentItem.grabToImage(function(result) {
            result.saveToFile(root.screenshotPath)
            Qt.quit()
        })
    }

    Item {
        anchors.fill: parent
        visible: !root.hasAuthoredNodes
    }

    Flickable {
        id: authoredViewport
        anchors.fill: parent
        visible: root.hasAuthoredNodes
        clip: true
        contentWidth: width
        contentHeight: authoredColumn.implicitHeight + 24

        ColumnLayout {
            id: authoredColumn
            x: 12
            y: 12
            width: authoredViewport.width - 24
            spacing: 8

            Repeater {
                model: root.displayNodes()
                delegate: AuthoredNode {
                    required property var modelData
                    nodeData: modelData
                    childNodes: root.nodeChildren(modelData)
                    Layout.fillWidth: true
                }
            }
        }
    }

    component AuthoredNode: Rectangle {
        required property var nodeData
        required property var childNodes
        readonly property string labelText: root.authoredLabel(nodeData)
        readonly property string bodyText: root.authoredText(nodeData)
        Layout.fillWidth: true
        implicitHeight: Math.max(32, nodeColumn.implicitHeight + 16)
        radius: 2
        color: nodeData.kind === "Text" ? "transparent" : "#10141a"
        border.width: nodeData.kind === "Text" ? 0 : 1
        border.color: "#2a3544"

        ColumnLayout {
            id: nodeColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text {
                visible: labelText.length > 0
                text: labelText
                color: "#f4f7fb"
                font.pixelSize: nodeData.kind === "Text" ? 14 : 13
                font.bold: nodeData.kind !== "Text"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text {
                visible: bodyText.length > 0 && bodyText !== labelText
                text: bodyText
                color: "#c8d2df"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Repeater {
                model: childNodes
                delegate: AuthoredNode {
                    required property var modelData
                    nodeData: modelData
                    childNodes: root.nodeChildren(modelData)
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                }
            }
        }
    }
}
"##;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::session::build_qt_quick_session_manifest;
    use kain_ui::{ui_runtime_bundle_from_output, UiBuildOutput, UiRuntimeMetadata};

    #[test]
    fn generated_qml_is_thin_authored_projection_host() {
        let bundle = ui_runtime_bundle_from_output(
            UiRuntimeMetadata {
                window_title: "Blank".to_string(),
                root_component: "App".to_string(),
                ..UiRuntimeMetadata::default()
            },
            UiBuildOutput::default(),
        );
        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());
        let qml = render_main_qml(&manifest, None);

        assert!(qml.contains("AuthoredNode"));
        assert!(qml.contains("displayNodes"));
        assert!(qml.contains("projectionRootNode"));
    }
}
