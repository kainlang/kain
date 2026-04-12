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
const KAIN_UI_NATIVE_QT_BROWSER_URL_ENV: &str = "KAIN_UI_NATIVE_QT_BROWSER_URL";
const KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_ENV: &str = "KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH";

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
    let browser_url = detect_optional_text_env(KAIN_UI_NATIVE_QT_BROWSER_URL_ENV)?;
    let viewport_image_path = detect_optional_text_env(KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_ENV)?;
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
        render_main_qml(
            &manifest,
            screenshot_path.as_deref(),
            browser_url.as_deref(),
            viewport_image_path.as_deref(),
        )
        .as_bytes(),
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
        .env("QTWEBENGINE_DISABLE_SANDBOX", "1")
        .env(
            "QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-gpu --disable-software-rasterizer --disable-dev-shm-usage",
        )
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

fn detect_optional_text_env(env_key: &str) -> Result<Option<String>, QtQuickHostLaunchError> {
    let Some(value) = env::var_os(env_key) else {
        return Ok(None);
    };

    let value = value.to_string_lossy().into_owned();
    if value.is_empty() {
        return Ok(None);
    }

    Ok(Some(value))
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
    manifest: &QtQuickSessionManifest,
    screenshot_path: Option<&Path>,
    browser_url: Option<&str>,
    viewport_image_path: Option<&str>,
) -> String {
    let session_json = serde_json::to_string_pretty(manifest).unwrap_or_else(|_| "{}".to_string());
    let screenshot_json = serde_json::to_string(
        &screenshot_path
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default(),
    )
    .unwrap_or_else(|_| "\"\"".to_string());
    let browser_url_json =
        serde_json::to_string(&browser_url.unwrap_or_default()).unwrap_or_else(|_| "\"\"".to_string());
    let viewport_image_json = serde_json::to_string(&viewport_image_path.unwrap_or_default())
        .unwrap_or_else(|_| "\"\"".to_string());

    r##"import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine
import Qt5Compat.GraphicalEffects

ApplicationWindow {
    id: root
    visible: true
    width: Math.max(1560, kainSession.initial_window_size[0])
    height: Math.max(960, kainSession.initial_window_size[1])
    title: kainSession.window_title
    color: "#071018"
    readonly property var kainSession: (__KAIN_SESSION_JSON__)
    readonly property string screenshotPath: __KAIN_SCREENSHOT_PATH__
    readonly property string browserUrl: __KAIN_BROWSER_URL__
    readonly property string viewportImagePath: __KAIN_VIEWPORT_IMAGE_PATH__
    property bool browserReady: false

    function roleAccent(role) {
        switch (role) {
        case "viewport":
            return "#60c5ff"
        case "browser":
            return "#67f0c4"
        case "shader":
            return "#d78cff"
        case "devtools":
            return "#8fb4ff"
        case "fallback":
            return "#ffbd7a"
        default:
            return "#9cc8ff"
        }
    }

    Timer {
        id: screenshotTimer
        interval: 6000
        repeat: false
        running: root.screenshotPath.length > 0
        onTriggered: chrome.grabToImage(function(result) {
            result.saveToFile(root.screenshotPath)
            Qt.quit()
        })
    }

    component BadgePill: Rectangle {
        required property string pillText
        required property color pillColor
        radius: 12
        color: Qt.rgba(pillColor.r, pillColor.g, pillColor.b, 0.16)
        border.width: 1
        border.color: Qt.rgba(pillColor.r, pillColor.g, pillColor.b, 0.45)
        implicitHeight: 28
        implicitWidth: pillLabel.implicitWidth + 20

        Label {
            id: pillLabel
            anchors.centerIn: parent
            text: pillText
            color: pillColor
            font.pixelSize: 11
            font.bold: true
        }
    }

    component PaneCard: Frame {
        required property var paneData
        readonly property color accentColor: root.roleAccent(paneData.role)
        Layout.fillWidth: true
        padding: 14

        background: Rectangle {
            radius: 20
            color: "#111a26"
            border.width: 1
            border.color: Qt.rgba(PaneCard.accentColor.r, PaneCard.accentColor.g, PaneCard.accentColor.b, 0.33)
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1d2a3d" }
                GradientStop { position: 1.0; color: "#111821" }
            }
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: paneData.title
                        color: "#f5f8ff"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    Label {
                        text: paneData.summary
                        color: "#bcd6ef"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                BadgePill {
                    pillText: paneData.role.toUpperCase()
                    pillColor: PaneCard.accentColor
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 14
                color: "#0d1520"
                border.width: 1
                border.color: "#243247"
                implicitHeight: adapterLabel.implicitHeight + 20

                Label {
                    id: adapterLabel
                    anchors.fill: parent
                    anchors.margins: 10
                    text: paneData.adapter_state_label
                    wrapMode: Text.Wrap
                    color: "#d8e8ff"
                    font.pixelSize: 12
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: paneData.detail_lines
                    delegate: BadgePill {
                        required property string modelData
                        pillText: modelData
                        pillColor: "#9bb2c9"
                    }
                }
            }
        }
    }

    component BrowserPane: Frame {
        required property var paneData
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 10

        background: Rectangle {
            radius: 22
            color: "#101821"
            border.width: 1
            border.color: "#2d4a60"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#162738" }
                GradientStop { position: 1.0; color: "#0f151d" }
            }
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    text: paneData.title
                    color: "#f5f8ff"
                    font.pixelSize: 17
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                BadgePill {
                    pillText: "WEBENGINE"
                    pillColor: "#67f0c4"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#0a1017"
                border.width: 1
                border.color: "#23364a"
                Layout.preferredHeight: 20
                implicitHeight: 20

                Label {
                    anchors.centerIn: parent
                    text: paneData.adapter_state_label
                    color: "#aac4d8"
                    font.pixelSize: 11
                }
            }

            WebEngineView {
                id: browserView
                Layout.fillWidth: true
                Layout.fillHeight: true
                url: root.browserUrl.length > 0 ? root.browserUrl : "about:blank"
                onLoadingChanged: {
                    if (loadRequest.status === WebEngineView.LoadSucceededStatus
                        || loadRequest.status === WebEngineView.LoadFailedStatus) {
                        root.browserReady = true
                    }
                }
            }
        }
    }

    component ShaderPane: Frame {
        required property var paneData
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 10

        background: Rectangle {
            radius: 22
            color: "#11131f"
            border.width: 1
            border.color: "#50345f"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a1330" }
                GradientStop { position: 1.0; color: "#10131b" }
            }
        }

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    text: paneData.title
                    color: "#f5f8ff"
                    font.pixelSize: 17
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                BadgePill {
                    pillText: "SHADER"
                    pillColor: "#d78cff"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 14
                color: "#0a1017"
                border.width: 1
                border.color: "#312540"
                implicitHeight: shaderSummary.implicitHeight + 18

                Label {
                    id: shaderSummary
                    anchors.fill: parent
                    anchors.margins: 10
                    text: paneData.adapter_state_label
                    wrapMode: Text.Wrap
                    color: "#dbc8f4"
                    font.pixelSize: 11
                }
            }

            Item {
                id: shaderStage
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitHeight: 240

                Rectangle {
                    id: shaderSource
                    anchors.fill: parent
                    radius: 18
                    color: "#141b2d"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#2b4e7a" }
                        GradientStop { position: 0.42; color: "#4e2f6f" }
                        GradientStop { position: 1.0; color: "#101a25" }
                    }

                    Rectangle {
                        width: 170
                        height: 170
                        radius: 85
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 18
                        color: "#89d6ff"
                        opacity: 0.20
                    }

                    Rectangle {
                        width: 120
                        height: 120
                        radius: 60
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 26
                        color: "#ff8fd8"
                        opacity: 0.22
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        Label {
                            text: "Live Shader Canvas"
                            color: "#f5f8ff"
                            font.pixelSize: 24
                            font.bold: true
                        }

                        Label {
                            text: "Qt GraphicalEffects is sampling a live source item here so the shell has a real shader-backed surface."
                            color: "#d4d6ff"
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: ["animated source", "blur pipeline", "overlay pass"]
                                delegate: BadgePill {
                                    required property string modelData
                                    pillText: modelData
                                    pillColor: "#e8b0ff"
                                }
                            }
                        }
                    }
                }

                FastBlur {
                    anchors.fill: shaderSource
                    source: shaderSource
                    radius: 28
                    transparentBorder: true
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    color: "#0f1622"
                    opacity: 0.14
                    border.width: 1
                    border.color: "#60517c"
                }
            }
        }
    }

    Rectangle {
        id: chrome
        anchors.fill: parent
        color: "#070b12"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d1726" }
            GradientStop { position: 0.45; color: "#091119" }
            GradientStop { position: 1.0; color: "#0a131c" }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: -120
            anchors.rightMargin: -120
            width: 360
            height: 360
            radius: 180
            color: "#57c7ff"
            opacity: 0.16
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: -140
            anchors.leftMargin: -110
            width: 360
            height: 360
            radius: 180
            color: "#d78cff"
            opacity: 0.12
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 84
                radius: 24
                color: "#111927"
                border.width: 1
                border.color: "#274158"
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#1a2941" }
                    GradientStop { position: 1.0; color: "#101721" }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    RowLayout {
                        spacing: 8
                        Repeater {
                            model: ["#ff6f8b", "#ffd36d", "#69f0c7"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 12
                                height: 12
                                radius: 6
                                color: modelData
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: kainSession.window_title
                            color: "#f7fbff"
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Label {
                            text: kainSession.root_component + " / " + (kainSession.document_panes.length + kainSession.viewport_panes.length + kainSession.browser_panes.length + kainSession.shader_panes.length + kainSession.devtools_panes.length + kainSession.fallback_panes.length) + " surfaces"
                            color: "#9cb5c9"
                            font.pixelSize: 12
                        }
                    }

                    Repeater {
                        model: [
                            "shell " + kainSession.shell_backend,
                            "layout " + kainSession.layout_engine,
                            "render " + kainSession.render_engine
                        ]
                        delegate: BadgePill {
                            required property string modelData
                            pillText: modelData
                            pillColor: "#a8d9ff"
                        }
                    }
                }
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal

                Rectangle {
                    SplitView.preferredWidth: 310
                    radius: 24
                    color: "#0f1722"
                    border.width: 1
                    border.color: "#22334a"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 14
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 12

                            Rectangle {
                                Layout.fillWidth: true
                                radius: 20
                                color: "#142234"
                                border.width: 1
                                border.color: "#2a4a68"
                                implicitHeight: 140

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8
                                    Label {
                                        text: "Plasma Runtime Deck"
                                        color: "#f5f8ff"
                                        font.pixelSize: 19
                                        font.bold: true
                                    }
                                    Label {
                                        text: "Kain owns the semantic UI model. Qt is only the host shell."
                                        color: "#bdd3e8"
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                    BadgePill {
                                        pillText: kainSession.mixed_backend_session ? "mixed backend session" : "single backend session"
                                        pillColor: "#67f0c4"
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    text: "Runtime Summary"
                                    color: "#f5f8ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Repeater {
                                    model: kainSession.summary_lines
                                    delegate: PaneCard {
                                        required property string modelData
                                        paneData: ({
                                            "title": "summary",
                                            "summary": modelData,
                                            "role": "summary",
                                            "adapter_state_label": modelData,
                                            "detail_lines": [modelData]
                                        })
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    text: "Document Rail"
                                    color: "#f5f8ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Repeater {
                                    model: kainSession.document_panes
                                    delegate: PaneCard { paneData: modelData }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    SplitView.fillWidth: true
                    radius: 24
                    color: "#0e151f"
                    border.width: 1
                    border.color: "#223349"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 20
                            color: "#101928"
                            border.width: 1
                            border.color: "#264058"
                            implicitHeight: 300

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Viewport"
                                        color: "#f5f8ff"
                                        font.pixelSize: 17
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    BadgePill {
                                        pillText: "KAIN 3D PREVIEW"
                                        pillColor: "#60c5ff"
                                    }
                                }

                                Image {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    fillMode: Image.PreserveAspectCrop
                                    source: root.viewportImagePath.length > 0 ? root.viewportImagePath : ""
                                    asynchronous: true
                                    mipmap: true
                                    cache: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 20
                            color: "#101923"
                            border.width: 1
                            border.color: "#2a4a68"
                            implicitHeight: 290

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Browser"
                                        color: "#f5f8ff"
                                        font.pixelSize: 17
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    BadgePill {
                                        pillText: root.browserReady ? "loaded" : "loading"
                                        pillColor: "#67f0c4"
                                    }
                                }

                                BrowserPane {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    paneData: kainSession.browser_panes.length > 0 ? kainSession.browser_panes[0] : ({
                                        "title": "Browser",
                                        "summary": "browser lane",
                                        "role": "browser",
                                        "adapter_state_label": "Browser lane placeholder",
                                        "detail_lines": []
                                    })
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    SplitView.preferredWidth: 420
                    radius: 24
                    color: "#10171f"
                    border.width: 1
                    border.color: "#243449"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 14
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    text: "Shader Surface"
                                    color: "#f5f8ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Repeater {
                                    model: kainSession.shader_panes
                                    delegate: ShaderPane { paneData: modelData }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    text: "Devtools"
                                    color: "#f5f8ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                                Repeater {
                                    model: kainSession.devtools_panes
                                    delegate: PaneCard { paneData: modelData }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    text: "Fallback"
                                    color: "#f5f8ff"
                                    font.pixelSize: 16
                                    font.bold: true
                                    visible: kainSession.fallback_panes.length > 0
                                }
                                Repeater {
                                    model: kainSession.fallback_panes
                                    delegate: PaneCard { paneData: modelData }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
"##
    .replace("__KAIN_SESSION_JSON__", &session_json)
    .replace("__KAIN_SCREENSHOT_PATH__", &screenshot_json)
    .replace("__KAIN_BROWSER_URL__", &browser_url_json)
    .replace("__KAIN_VIEWPORT_IMAGE_PATH__", &viewport_image_json)
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
        let qml = render_main_qml(
            &manifest,
            Some(Path::new("/tmp/kain-smoke-shot.png")),
            Some("file:///tmp/kain-browser.html"),
            Some("/tmp/kain-viewport.png"),
        );
        assert!(qml.contains("Plasma Runtime Deck"));
        assert!(qml.contains("Qt Runtime"));
        assert!(qml.contains("WebEngineView"));
        assert!(qml.contains("FastBlur"));
        assert!(qml.contains("KAIN 3D PREVIEW"));
    }

    #[test]
    fn detect_qt_runtime_reports_missing_runtime_cleanly() {
        let result = detect_qt_runtime();
        if let Err(QtQuickHostLaunchError::QtRuntimeUnavailable { searched }) = result {
            assert!(!searched.is_empty());
        }
    }
}
