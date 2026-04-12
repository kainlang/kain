use std::{
    env,
    error::Error,
    fmt, fs, io,
    path::{Path, PathBuf},
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
                "no Qt Quick runtime was found for the default non-egui host; searched {}",
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

fn render_main_qml(manifest: &QtQuickSessionManifest, screenshot_path: Option<&Path>) -> String {
    let session_json = serde_json::to_string_pretty(manifest).unwrap_or_else(|_| "{}".to_string());
    let screenshot_json = serde_json::to_string(
        &screenshot_path
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default(),
    )
    .unwrap_or_else(|_| "\"\"".to_string());

    r##"import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: Math.max(1280, kainSession.initial_window_size[0])
    height: Math.max(780, kainSession.initial_window_size[1])
    title: kainSession.window_title
    color: "#04080d"
    readonly property var kainSession: (__KAIN_SESSION_JSON__)
    readonly property string screenshotPath: __KAIN_SCREENSHOT_PATH__

    function accentForRole(role) {
        switch (role) {
        case "viewport":
            return "#5ed0ff"
        case "devtools":
            return "#a48cff"
        case "fallback":
            return "#ffbf69"
        default:
            return "#67d0ff"
        }
    }

    function surfaceCountLabel() {
        return (kainSession.document_panes.length + kainSession.viewport_panes.length
            + kainSession.devtools_panes.length + kainSession.fallback_panes.length) + " live panes"
    }

    Timer {
        id: screenshotTimer
        interval: 450
        repeat: false
        running: root.screenshotPath.length > 0
        onTriggered: chrome.grabToImage(function(result) {
            result.saveToFile(root.screenshotPath)
            Qt.quit()
        })
    }

    component PaneCard: Frame {
        required property var paneData
        readonly property color accentColor: root.accentForRole(paneData.role)
        Layout.fillWidth: true
        padding: 16

        background: Rectangle {
            radius: 22
            color: "#161d29"
            border.width: 1
            border.color: Qt.rgba(PaneCard.accentColor.r, PaneCard.accentColor.g, PaneCard.accentColor.b, 0.38)
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#243348" }
                GradientStop { position: 1.0; color: "#151c28" }
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 4
                radius: 2
                color: PaneCard.accentColor
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: paneData.title
                        color: "#f4f7ff"
                        font.pixelSize: 18
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    Label {
                        text: paneData.summary
                        color: "#b7ddff"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    radius: 11
                    color: Qt.rgba(PaneCard.accentColor.r, PaneCard.accentColor.g, PaneCard.accentColor.b, 0.18)
                    border.width: 1
                    border.color: Qt.rgba(PaneCard.accentColor.r, PaneCard.accentColor.g, PaneCard.accentColor.b, 0.45)
                    implicitHeight: 32
                    implicitWidth: badgeLabel.implicitWidth + 18

                    Label {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: paneData.role.toUpperCase()
                        color: PaneCard.accentColor
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#0f1723"
                border.width: 1
                border.color: "#24354a"
                implicitHeight: adapterLabel.implicitHeight + 22

                Label {
                    id: adapterLabel
                    anchors.fill: parent
                    anchors.margins: 11
                    text: paneData.adapter_state_label
                    wrapMode: Text.Wrap
                    color: "#d5e7ff"
                    font.pixelSize: 13
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: paneData.detail_lines

                    delegate: Rectangle {
                        required property string modelData
                        radius: 12
                        color: "#111c2a"
                        border.width: 1
                        border.color: "#223448"
                        implicitHeight: chipLabel.implicitHeight + 12
                        implicitWidth: Math.min(320, chipLabel.implicitWidth + 18)

                        Label {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: "#9fb3ca"
                            font.pixelSize: 12
                            width: Math.min(300, implicitWidth)
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: chrome
        anchors.fill: parent
        color: "#070b12"

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d1624" }
            GradientStop { position: 0.38; color: "#0b1018" }
            GradientStop { position: 1.0; color: "#09121d" }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: -120
            anchors.rightMargin: -140
            width: 420
            height: 420
            radius: 210
            color: "#2867ff"
            opacity: 0.14
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: -140
            anchors.leftMargin: -100
            width: 360
            height: 360
            radius: 180
            color: "#1fc7ff"
            opacity: 0.10
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 78
                radius: 24
                color: "#121b29"
                border.width: 1
                border.color: "#2a3a53"

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#1a2940" }
                    GradientStop { position: 1.0; color: "#111824" }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    RowLayout {
                        spacing: 8

                        Repeater {
                            model: ["#ff6b81", "#ffcc66", "#4be3c2"]

                            delegate: Rectangle {
                                required property string modelData
                                width: 12
                                height: 12
                                radius: 6
                                color: modelData
                                opacity: 0.92
                            }
                        }
                    }

                    Rectangle {
                        radius: 18
                        color: "#0d1522"
                        border.width: 1
                        border.color: "#2b4663"
                        implicitHeight: 42
                        implicitWidth: titleColumn.implicitWidth + 34

                        ColumnLayout {
                            id: titleColumn
                            anchors.centerIn: parent
                            spacing: 1

                            Label {
                                text: kainSession.window_title
                                color: "#f7fbff"
                                font.pixelSize: 20
                                font.bold: true
                            }

                            Label {
                                text: kainSession.root_component + " / " + root.surfaceCountLabel()
                                color: "#92b8d9"
                                font.pixelSize: 12
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Repeater {
                        model: [
                            "shell " + kainSession.shell_backend,
                            "layout " + kainSession.layout_engine,
                            "render " + kainSession.render_engine
                        ]

                        delegate: Rectangle {
                            required property string modelData
                            radius: 14
                            color: "#0f1623"
                            border.width: 1
                            border.color: "#29425d"
                            implicitHeight: 34
                            implicitWidth: modeLabel.implicitWidth + 24

                            Label {
                                id: modeLabel
                                anchors.centerIn: parent
                                text: modelData
                                color: "#afd4ff"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                    }
                }
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                Rectangle {
                    SplitView.preferredWidth: 270
                    radius: 26
                    color: "#101925"
                    border.width: 1
                    border.color: "#22354a"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 22
                            color: "#17263b"
                            border.width: 1
                            border.color: "#35527a"
                            implicitHeight: 150

                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#20355a" }
                                GradientStop { position: 1.0; color: "#151e2a" }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 8

                                Label {
                                    text: "Plasma Operator Deck"
                                    color: "#f7fbff"
                                    font.pixelSize: 20
                                    font.bold: true
                                }

                                Label {
                                    text: "A Qt Quick shell proving Kain can present document, viewport, and devtools lanes inside one polished control surface."
                                    color: "#b5cee7"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    radius: 14
                                    color: "#0d1623"
                                    border.width: 1
                                    border.color: "#2a4360"
                                    implicitHeight: 34
                                    implicitWidth: sessionBadge.implicitWidth + 24

                                    Label {
                                        id: sessionBadge
                                        anchors.centerIn: parent
                                        text: kainSession.mixed_backend_session ? "mixed backend session" : "single backend session"
                                        color: "#87d0ff"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: "Runtime Summary"
                                color: "#f4f7ff"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Repeater {
                                model: kainSession.summary_lines

                                delegate: Rectangle {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    radius: 14
                                    color: "#0d131d"
                                    border.width: 1
                                    border.color: "#1f2c3f"
                                    implicitHeight: lineLabel.implicitHeight + 18

                                    Label {
                                        id: lineLabel
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        text: modelData
                                        wrapMode: Text.Wrap
                                        color: "#99afc4"
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Repeater {
                                model: [
                                    { label: "Docs", count: kainSession.document_panes.length, color: "#60c5ff" },
                                    { label: "Views", count: kainSession.viewport_panes.length, color: "#67f0c4" },
                                    { label: "Tools", count: kainSession.devtools_panes.length, color: "#aa8cff" }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    radius: 18
                                    color: "#0f1824"
                                    border.width: 1
                                    border.color: "#25384f"
                                    implicitHeight: 74

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        Label {
                                            text: modelData.count
                                            color: modelData.color
                                            font.pixelSize: 24
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter
                                            width: parent.width
                                        }

                                        Label {
                                            text: modelData.label
                                            color: "#b2c4d8"
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignHCenter
                                            width: parent.width
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 18
                            color: "#0d1420"
                            border.width: 1
                            border.color: "#203147"
                            implicitHeight: 80

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 6

                                Label {
                                    text: "Operator Note"
                                    color: "#f4f7ff"
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                Label {
                                    text: "Viewport and ImGui adapters still route through explicit placeholders, but the shell, routing, metadata, and capture flow are live."
                                    color: "#9bb1c8"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    SplitView.fillWidth: true
                    radius: 28
                    color: "#0f141d"
                    border.width: 1
                    border.color: "#223348"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 16

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                text: "Runtime Canvas"
                                color: "#f7fbff"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                radius: 14
                                color: "#132030"
                                border.width: 1
                                border.color: "#27415d"
                                implicitHeight: 34
                                implicitWidth: statusLabel.implicitWidth + 24

                                Label {
                                    id: statusLabel
                                    anchors.centerIn: parent
                                    text: "Qt Quick shell active"
                                    color: "#78d7ff"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                        }

                        SplitView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            orientation: Qt.Vertical

                            Rectangle {
                                SplitView.fillHeight: true
                                radius: 24
                                color: "#121b28"
                                border.width: 1
                                border.color: "#2b415b"

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    contentWidth: availableWidth

                                    ColumnLayout {
                                        width: parent.width
                                        spacing: 12

                                        Label {
                                            text: "Document Deck"
                                            color: "#f4f7ff"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }

                                        Repeater {
                                            model: kainSession.document_panes

                                            delegate: PaneCard {
                                                paneData: modelData
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                SplitView.preferredHeight: 280
                                radius: 24
                                color: "#101826"
                                border.width: 1
                                border.color: "#25384f"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true

                                        Label {
                                            text: "Viewport Stage"
                                            color: "#f4f7ff"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }

                                        Item { Layout.fillWidth: true }

                                        Label {
                                            text: "bgfx handoff next"
                                            color: "#8fdfff"
                                            font.pixelSize: 12
                                        }
                                    }

                                    Repeater {
                                        model: kainSession.viewport_panes

                                        delegate: PaneCard {
                                            paneData: modelData
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    SplitView.preferredWidth: 360
                    radius: 26
                    color: "#101722"
                    border.width: 1
                    border.color: "#203246"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 16
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 12

                            Label {
                                text: "Devtools Rail"
                                color: "#f4f7ff"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Repeater {
                                model: kainSession.devtools_panes

                                delegate: PaneCard {
                                    paneData: modelData
                                }
                            }

                            Label {
                                visible: kainSession.fallback_panes.length > 0
                                text: "Staged Adapters"
                                color: "#f4f7ff"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Repeater {
                                model: kainSession.fallback_panes

                                delegate: PaneCard {
                                    paneData: modelData
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 54
                radius: 20
                color: "#101823"
                border.width: 1
                border.color: "#223349"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Label {
                        text: "kain ui runtime"
                        color: "#f4f7ff"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        color: "#223349"
                    }

                    Label {
                        text: "Plasma-style shell with deterministic screenshot capture for the Qt host smoke lane."
                        color: "#93a9c0"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.screenshotPath.length > 0 ? "capture armed" : "interactive session"
                        color: "#80d6ff"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
        }
    }
}
"##
    .replace("__KAIN_SESSION_JSON__", &session_json)
    .replace("__KAIN_SCREENSHOT_PATH__", &screenshot_json)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        no_egui::KainUiNativeBackendPlan, no_egui_session::build_qt_quick_session_manifest,
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
        let manifest =
            build_qt_quick_session_manifest(&bundle, &KainUiNativeBackendPlan::default());
        let qml = render_main_qml(&manifest, None);
        assert!(qml.contains("Plasma Operator Deck"));
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
