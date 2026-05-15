use std::{
    collections::BTreeMap,
    env,
    error::Error,
    fs::{self, File},
    path::{Path, PathBuf},
};

use kain_3d::{RenderResolution, SceneCatalog, SoftwareRenderer, SoftwareRendererConfig};
use kain_ui::{
    ui_runtime_bundle_from_output, UiBuildOutput, UiHostBackendKind, UiLayoutEngineKind,
    UiLayoutKind, UiLayoutSpec, UiNode, UiNodeId, UiRenderEngineKind, UiRuntimeBundle,
    UiRuntimeMetadata, UiRuntimeSystems, UiStyleSpec, UiSurface, UiSurfaceCompositionMode,
    UiSurfaceKind, UiSurfaceRendererPreference, UiSurfaceShaderBinding, UiTree, UiWidgetKind,
};
use kain_ui_native::run_bundled_app;

const ARTIFACT_DIR_ENV: &str = "KAIN_UI_NATIVE_QT_ARTIFACT_DIR";
const BROWSER_URL_ENV: &str = "KAIN_UI_NATIVE_QT_BROWSER_URL";
const VIEWPORT_IMAGE_ENV: &str = "KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH";

fn main() -> Result<(), Box<dyn Error>> {
    let bundle = build_runtime_bundle();
    let artifact_dir = artifact_dir_from_env()?;
    fs::create_dir_all(&artifact_dir)?;

    let viewport_image_path = artifact_dir.join("viewport_preview.png");
    let browser_html_path = artifact_dir.join("browser_surface.html");

    write_viewport_preview(&viewport_image_path)?;
    write_browser_surface_html(&browser_html_path, &bundle)?;

    env::set_var(BROWSER_URL_ENV, file_url(&browser_html_path)?);
    env::set_var(
        VIEWPORT_IMAGE_ENV,
        viewport_image_path.canonicalize()?.display().to_string(),
    );

    run_bundled_app(bundle)
}

fn build_runtime_bundle() -> UiRuntimeBundle {
    ui_runtime_bundle_from_output(runtime_metadata(), runtime_output())
}

fn runtime_metadata() -> UiRuntimeMetadata {
    UiRuntimeMetadata {
        app_name: Some("ui-smoke-qt-plasma-runtime-lounge".to_string()),
        window_title: "Kain Plasma Runtime Lounge".to_string(),
        root_component: "PlasmaControlDeck".to_string(),
        source_file_name: Some("smoketest/UI/qt_plasma_runtime_lounge".to_string()),
        initial_window_size: [1560.0, 960.0],
        preferred_shell_host_backend: UiHostBackendKind::Qt,
        preferred_document_host_backend: UiHostBackendKind::RmlUi,
        preferred_devtools_host_backend: UiHostBackendKind::Imgui,
        preferred_layout_engine: UiLayoutEngineKind::Yoga,
        preferred_render_engine: UiRenderEngineKind::Skia,
        compatibility_host_backend: UiHostBackendKind::Qt,
        mixed_backend_session: true,
    }
}

fn runtime_output() -> UiBuildOutput {
    UiBuildOutput {
        tree: smoke_tree(),
        patches: Vec::new(),
        systems: smoke_runtime_systems(),
    }
}

fn smoke_tree() -> UiTree {
    let mut nodes = BTreeMap::new();

    let mut root = node(1, UiWidgetKind::Panel, "plasma-root");
    root.layout.kind = UiLayoutKind::Dock;
    root.children = vec![
        UiNodeId(10),
        UiNodeId(20),
        UiNodeId(30),
        UiNodeId(40),
        UiNodeId(50),
        UiNodeId(60),
        UiNodeId(70),
    ];
    nodes.insert(root.id, root);

    nodes.insert(
        UiNodeId(10),
        node(10, UiWidgetKind::Panel, "session-browser"),
    );
    nodes.insert(
        UiNodeId(20),
        node(20, UiWidgetKind::Panel, "material-atlas"),
    );
    nodes.insert(
        UiNodeId(30),
        node(30, UiWidgetKind::Viewport3D, "nebula-viewport"),
    );
    nodes.insert(
        UiNodeId(40),
        node(40, UiWidgetKind::Graph, "runtime-inspector"),
    );
    nodes.insert(
        UiNodeId(50),
        node(50, UiWidgetKind::Timeline, "transport-timeline"),
    );
    nodes.insert(UiNodeId(60), node(60, UiWidgetKind::Panel, "browser-panel"));
    nodes.insert(
        UiNodeId(70),
        node(
            70,
            UiWidgetKind::Element("shader_canvas".to_string()),
            "shader-canvas",
        ),
    );

    UiTree {
        root: Some(UiNodeId(1)),
        nodes,
    }
}

fn node(id: u64, kind: UiWidgetKind, identity_key: &str) -> UiNode {
    let mut node = UiNode::new(UiNodeId(id), kind);
    node.identity_key = Some(identity_key.to_string());
    node.layout = UiLayoutSpec::default();
    node.style = UiStyleSpec::default();
    node
}

fn smoke_runtime_systems() -> UiRuntimeSystems {
    let mut systems = UiRuntimeSystems::default();
    systems.surfaces = vec![
        UiSurface {
            id: "session-browser".to_string(),
            kind: UiSurfaceKind::Tree,
            node: UiNodeId(10),
            title: Some("Session Browser".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Dom,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::RmlUi,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Skia,
            gpu_backing_required: false,
            shader: None,
        },
        UiSurface {
            id: "material-atlas".to_string(),
            kind: UiSurfaceKind::Table,
            node: UiNodeId(20),
            title: Some("Material Atlas".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Dom,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::RmlUi,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Skia,
            gpu_backing_required: false,
            shader: None,
        },
        UiSurface {
            id: "nebula-viewport".to_string(),
            kind: UiSurfaceKind::Viewport3D,
            node: UiNodeId(30),
            title: Some("Nebula Viewport".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Wgpu,
            composition_mode: UiSurfaceCompositionMode::Viewport,
            preferred_host_backend: UiHostBackendKind::Qt,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "runtime-inspector".to_string(),
            kind: UiSurfaceKind::Graph,
            node: UiNodeId(40),
            title: Some("Runtime Inspector".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Native,
            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
            preferred_host_backend: UiHostBackendKind::Imgui,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "transport-timeline".to_string(),
            kind: UiSurfaceKind::Timeline,
            node: UiNodeId(50),
            title: Some("Transport Timeline".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Native,
            composition_mode: UiSurfaceCompositionMode::LayeredGpu,
            preferred_host_backend: UiHostBackendKind::Imgui,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Wgpu,
            gpu_backing_required: true,
            shader: None,
        },
        UiSurface {
            id: "browser-panel".to_string(),
            kind: UiSurfaceKind::Custom("browser_panel".to_string()),
            node: UiNodeId(60),
            title: Some("Reference Browser".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Dom,
            composition_mode: UiSurfaceCompositionMode::Host,
            preferred_host_backend: UiHostBackendKind::Cef,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Browser,
            gpu_backing_required: false,
            shader: None,
        },
        UiSurface {
            id: "shader-canvas".to_string(),
            kind: UiSurfaceKind::Canvas,
            node: UiNodeId(70),
            title: Some("Shader Canvas".to_string()),
            renderer_preference: UiSurfaceRendererPreference::Shader,
            composition_mode: UiSurfaceCompositionMode::ShaderCanvas,
            preferred_host_backend: UiHostBackendKind::Qt,
            preferred_layout_engine: UiLayoutEngineKind::Yoga,
            preferred_render_engine: UiRenderEngineKind::Shader,
            gpu_backing_required: true,
            shader: Some(UiSurfaceShaderBinding {
                shader_ref: "kain://shader/plasma-glow".to_string(),
                entry_point: Some("main".to_string()),
                stage: Some("fragment".to_string()),
                derived_format: Some("rgba8unorm".to_string()),
            }),
        },
    ];
    systems
}

fn artifact_dir_from_env() -> Result<PathBuf, Box<dyn Error>> {
    let Some(value) = env::var_os(ARTIFACT_DIR_ENV) else {
        return Err(format!("{ARTIFACT_DIR_ENV} must be set by run_smoke.sh").into());
    };
    Ok(PathBuf::from(value))
}

fn write_viewport_preview(path: &Path) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let catalog = SceneCatalog::empty();
    let mut renderer = SoftwareRenderer::default();
    renderer.config = SoftwareRendererConfig {
        wireframe_overlay: false,
        rim_light_strength: 0.22,
    };
    let frame = renderer.render_catalog_scene(
        &catalog,
        "geometry_fixture",
        1.45,
        RenderResolution::new(1440, 810),
    )?;
    write_png(path, frame.width, frame.height, &frame.rgba)?;
    Ok(())
}

fn write_browser_surface_html(path: &Path, bundle: &UiRuntimeBundle) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let title = bundle.metadata.window_title.as_str();
    let app_name = bundle.metadata.app_name.as_deref().unwrap_or("kain-ui");
    let root_component = bundle.metadata.root_component.as_str();
    let shell_backend = host_backend_label(bundle.metadata.preferred_shell_host_backend);
    let document_backend = host_backend_label(bundle.metadata.preferred_document_host_backend);
    let devtools_backend = host_backend_label(bundle.metadata.preferred_devtools_host_backend);
    let layout_engine = layout_engine_label(bundle.metadata.preferred_layout_engine);
    let render_engine = render_engine_label(bundle.metadata.preferred_render_engine);
    let surface_count = bundle.output.systems.surfaces.len();

    let cards = bundle
        .output
        .systems
        .surfaces
        .iter()
        .map(render_surface_card)
        .collect::<String>();

    let html = format!(
        r#"<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title}</title>
  <style>
    :root {{
      color-scheme: dark;
      --bg: #071018;
      --bg-2: #0a1724;
      --bg-3: #102033;
      --card: rgba(16, 24, 38, 0.82);
      --card-strong: rgba(21, 31, 49, 0.92);
      --stroke: rgba(103, 185, 255, 0.16);
      --stroke-strong: rgba(103, 185, 255, 0.34);
      --text: #eff6ff;
      --muted: #a7bfd6;
      --accent: #6be0c8;
      --accent-2: #89c8ff;
      --accent-3: #d38dff;
      --shadow: 0 32px 72px rgba(0, 0, 0, 0.42);
      --radius: 28px;
      font-family: Inter, "Segoe UI", "Helvetica Neue", sans-serif;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at top left, rgba(111, 208, 255, 0.24), transparent 32%),
        radial-gradient(circle at bottom right, rgba(211, 141, 255, 0.24), transparent 34%),
        linear-gradient(160deg, var(--bg), var(--bg-2) 54%, var(--bg) 100%);
      color: var(--text);
    }}
    .shell {{
      position: relative;
      max-width: 1780px;
      margin: 0 auto;
      padding: 34px;
      display: grid;
      gap: 22px;
    }}
    .ambient {{
      position: fixed;
      inset: auto;
      border-radius: 999px;
      filter: blur(4px);
      pointer-events: none;
      opacity: 0.55;
    }}
    .ambient-a {{
      width: 420px;
      height: 420px;
      left: -140px;
      top: -120px;
      background: radial-gradient(circle, rgba(107, 224, 200, 0.35), transparent 72%);
    }}
    .ambient-b {{
      width: 520px;
      height: 520px;
      right: -170px;
      bottom: -180px;
      background: radial-gradient(circle, rgba(137, 200, 255, 0.26), transparent 72%);
    }}
    .panel {{
      background: var(--card);
      border: 1px solid var(--stroke);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      backdrop-filter: blur(18px);
    }}
    .topbar {{
      padding: 22px 24px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 18px;
    }}
    .brand {{
      display: flex;
      align-items: center;
      gap: 16px;
    }}
    .brand-mark {{
      width: 52px;
      height: 52px;
      border-radius: 18px;
      display: grid;
      place-items: center;
      font-size: 24px;
      font-weight: 800;
      color: #09101a;
      background: linear-gradient(145deg, var(--accent), var(--accent-2));
      box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.08) inset;
    }}
    .eyebrow {{
      margin: 0 0 4px;
      letter-spacing: 0.24em;
      text-transform: uppercase;
      color: var(--accent);
      font-size: 11px;
      font-weight: 800;
    }}
    h1, h2, h3, p {{ margin: 0; }}
    .title-block h1 {{
      font-size: 28px;
      line-height: 1.05;
    }}
    .title-block p {{
      margin-top: 7px;
      color: var(--muted);
      font-size: 14px;
    }}
    .chip-row {{
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      justify-content: flex-end;
    }}
    .chip {{
      padding: 8px 12px;
      border-radius: 999px;
      border: 1px solid rgba(255, 255, 255, 0.09);
      background: rgba(255, 255, 255, 0.05);
      color: var(--text);
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.02em;
    }}
    .chip.accent-a {{ border-color: rgba(107, 224, 200, 0.3); color: var(--accent); }}
    .chip.accent-b {{ border-color: rgba(137, 200, 255, 0.3); color: var(--accent-2); }}
    .chip.accent-c {{ border-color: rgba(211, 141, 255, 0.3); color: var(--accent-3); }}
    .hero {{
      display: grid;
      grid-template-columns: 1.2fr 1fr;
      gap: 20px;
      align-items: stretch;
    }}
    .hero-copy {{
      padding: 28px;
      display: grid;
      gap: 20px;
    }}
    .hero-copy h2 {{
      font-size: 38px;
      line-height: 1.02;
      max-width: 11ch;
    }}
    .hero-copy p {{
      max-width: 58ch;
      color: var(--muted);
      font-size: 15px;
      line-height: 1.7;
    }}
    .metric-grid {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
    }}
    .metric {{
      padding: 16px 18px;
      border-radius: 20px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
    }}
    .metric span {{
      display: block;
      color: var(--muted);
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.16em;
      margin-bottom: 8px;
    }}
    .metric strong {{
      display: block;
      font-size: 16px;
      line-height: 1.35;
    }}
    .viewport-card {{
      overflow: hidden;
      padding: 18px;
      display: grid;
      gap: 14px;
    }}
    .viewport-head {{
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
    }}
    .viewport-head h3 {{
      font-size: 18px;
    }}
    .preview {{
      width: 100%;
      aspect-ratio: 16 / 9;
      object-fit: cover;
      border-radius: 22px;
      border: 1px solid rgba(255, 255, 255, 0.08);
      background: linear-gradient(145deg, rgba(12, 20, 31, 0.95), rgba(18, 27, 43, 0.9));
    }}
    .surface-grid {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 18px;
    }}
    .surface {{
      padding: 18px;
      display: grid;
      gap: 14px;
      border-left: 4px solid rgba(255, 255, 255, 0.16);
    }}
    .surface h3 {{
      font-size: 18px;
      line-height: 1.15;
    }}
    .surface .summary {{
      color: var(--muted);
      font-size: 14px;
      line-height: 1.6;
    }}
    .surface .meta {{
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }}
    .surface .meta span {{
      padding: 6px 10px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid rgba(255, 255, 255, 0.08);
      color: var(--text);
      font-size: 11px;
      letter-spacing: 0.02em;
    }}
    .surface.document {{ border-color: rgba(107, 224, 200, 0.8); }}
    .surface.viewport {{ border-color: rgba(137, 200, 255, 0.8); }}
    .surface.browser {{ border-color: rgba(92, 255, 175, 0.8); }}
    .surface.shader {{ border-color: rgba(211, 141, 255, 0.8); }}
    .surface.devtools {{ border-color: rgba(151, 174, 255, 0.8); }}
    .surface.fallback {{ border-color: rgba(255, 189, 122, 0.8); }}
    .surface .shader-ref {{
      color: var(--accent-3);
      font-size: 12px;
      line-height: 1.5;
      word-break: break-all;
    }}
    @media (max-width: 1280px) {{
      .hero,
      .surface-grid {{
        grid-template-columns: 1fr;
      }}
      .topbar {{
        flex-direction: column;
        align-items: flex-start;
      }}
      .chip-row {{
        justify-content: flex-start;
      }}
    }}
  </style>
</head>
<body>
  <div class="ambient ambient-a"></div>
  <div class="ambient ambient-b"></div>
  <main class="shell">
    <header class="topbar panel">
      <div class="brand">
        <div class="brand-mark">K</div>
        <div class="title-block">
          <p class="eyebrow">Kain UI smoke</p>
          <h1>{}</h1>
          <p>{} / {}</p>
        </div>
      </div>
      <div class="chip-row">
        <span class="chip accent-a">shell {}</span>
        <span class="chip accent-b">document {}</span>
        <span class="chip accent-c">devtools {}</span>
        <span class="chip">{} surfaces</span>
      </div>
    </header>

    <section class="hero">
      <article class="hero-copy panel">
        <div>
          <p class="eyebrow">Workstation proof</p>
          <h2>Plasma runtime lounge</h2>
        </div>
        <p>
          Kain owns the semantic UI model. Qt is presenting the shell, while the browser, shader,
          viewport, document, and devtools lanes are all declared in the same session bundle.
        </p>
        <div class="metric-grid">
          <div class="metric">
            <span>layout engine</span>
            <strong>{}</strong>
          </div>
          <div class="metric">
            <span>render engine</span>
            <strong>{}</strong>
          </div>
          <div class="metric">
            <span>root component</span>
            <strong>{}</strong>
          </div>
          <div class="metric">
            <span>mixed session</span>
            <strong>{}</strong>
          </div>
        </div>
      </article>

      <article class="viewport-card panel">
        <div class="viewport-head">
          <h3>Viewport preview</h3>
          <span class="chip accent-b">kain-3d software render</span>
        </div>
        <img class="preview" src="{}" alt="Kain 3D viewport preview">
      </article>
    </section>

    <section class="surface-grid">
      {}
    </section>
  </main>
</body>
</html>
"#,
        escape_html(title),
        escape_html(app_name),
        escape_html(root_component),
        escape_html(&shell_backend),
        escape_html(&document_backend),
        escape_html(&devtools_backend),
        surface_count,
        escape_html(&layout_engine),
        escape_html(&render_engine),
        escape_html(root_component),
        if bundle.metadata.mixed_backend_session {
            "true"
        } else {
            "false"
        },
        file_url(&path.parent().unwrap_or(path).join("viewport_preview.png"))?,
        cards,
    );

    fs::write(path, html)?;
    Ok(())
}

fn render_surface_card(surface: &UiSurface) -> String {
    let role = surface_role(surface);
    let title = surface.title.as_deref().unwrap_or(surface.id.as_str());
    let shader_ref = surface
        .shader
        .as_ref()
        .map(|shader| shader.shader_ref.as_str())
        .unwrap_or("");
    let shader_line = if shader_ref.is_empty() {
        String::new()
    } else {
        format!(
            "<div class=\"shader-ref\">shader {}</div>",
            escape_html(shader_ref)
        )
    };

    format!(
        r#"<article class="surface {role} panel">
  <div class="chip-row" style="justify-content:flex-start">
    <span class="chip accent-a">{}</span>
    <span class="chip">{}</span>
  </div>
  <h3>{}</h3>
  <p class="summary">{}</p>
  <div class="meta">
    <span>host {}</span>
    <span>render {}</span>
    <span>composition {}</span>
    <span>gpu {}</span>
  </div>
  {}
</article>"#,
        escape_html(role),
        escape_html(&surface.id),
        escape_html(title),
        escape_html(&surface_summary(surface)),
        escape_html(host_backend_label(surface.preferred_host_backend)),
        escape_html(render_engine_label(surface.preferred_render_engine)),
        escape_html(composition_label(surface.composition_mode)),
        if surface.gpu_backing_required {
            "required"
        } else {
            "optional"
        },
        shader_line,
    )
}

fn surface_role(surface: &UiSurface) -> &'static str {
    if surface.shader.is_some()
        || surface.composition_mode == UiSurfaceCompositionMode::ShaderCanvas
    {
        return "shader";
    }

    if matches!(
        surface.kind,
        UiSurfaceKind::Viewport2D | UiSurfaceKind::Viewport3D
    ) || surface.composition_mode == UiSurfaceCompositionMode::Viewport
    {
        return "viewport";
    }

    if surface.preferred_host_backend == UiHostBackendKind::Cef
        || matches!(&surface.kind, UiSurfaceKind::Custom(value) if value.contains("browser"))
    {
        return "browser";
    }

    if surface.preferred_host_backend == UiHostBackendKind::Imgui
        || matches!(
            surface.kind,
            UiSurfaceKind::Graph | UiSurfaceKind::Timeline | UiSurfaceKind::Overlay
        )
    {
        return "devtools";
    }

    "document"
}

fn surface_summary(surface: &UiSurface) -> String {
    format!(
        "{} / {} / {}",
        surface_kind_label(&surface.kind),
        host_backend_label(surface.preferred_host_backend),
        render_engine_label(surface.preferred_render_engine)
    )
}

fn host_backend_label(backend: UiHostBackendKind) -> &'static str {
    match backend {
        UiHostBackendKind::Auto => "auto",
        UiHostBackendKind::Native => "native",
        UiHostBackendKind::LegacyEgui => "legacy-egui",
        UiHostBackendKind::Imgui => "imgui",
        UiHostBackendKind::RmlUi => "rmlui",
        UiHostBackendKind::Slint => "slint",
        UiHostBackendKind::Qt => "qt",
        UiHostBackendKind::Cef => "cef",
    }
}

fn layout_engine_label(layout_engine: UiLayoutEngineKind) -> &'static str {
    match layout_engine {
        UiLayoutEngineKind::Auto => "auto",
        UiLayoutEngineKind::Native => "native",
        UiLayoutEngineKind::Yoga => "yoga",
        UiLayoutEngineKind::LegacyEgui => "legacy-egui",
    }
}

fn render_engine_label(render_engine: UiRenderEngineKind) -> &'static str {
    match render_engine {
        UiRenderEngineKind::Auto => "auto",
        UiRenderEngineKind::Native => "native",
        UiRenderEngineKind::Skia => "skia",
        UiRenderEngineKind::Wgpu => "wgpu",
        UiRenderEngineKind::Shader => "shader",
        UiRenderEngineKind::Browser => "browser",
        UiRenderEngineKind::LegacyEgui => "legacy-egui",
    }
}

fn composition_label(mode: UiSurfaceCompositionMode) -> &'static str {
    match mode {
        UiSurfaceCompositionMode::Host => "host",
        UiSurfaceCompositionMode::LayeredGpu => "layered_gpu",
        UiSurfaceCompositionMode::Viewport => "viewport",
        UiSurfaceCompositionMode::ShaderCanvas => "shader_canvas",
    }
}

fn surface_kind_label(kind: &UiSurfaceKind) -> String {
    match kind {
        UiSurfaceKind::Canvas => "canvas".to_string(),
        UiSurfaceKind::Graph => "graph".to_string(),
        UiSurfaceKind::Timeline => "timeline".to_string(),
        UiSurfaceKind::Table => "table".to_string(),
        UiSurfaceKind::Tree => "tree".to_string(),
        UiSurfaceKind::Viewport2D => "viewport2d".to_string(),
        UiSurfaceKind::Viewport3D => "viewport3d".to_string(),
        UiSurfaceKind::Overlay => "overlay".to_string(),
        UiSurfaceKind::Custom(value) => value.clone(),
    }
}

fn escape_html(input: &str) -> String {
    input
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}

fn file_url(path: &Path) -> Result<String, Box<dyn Error>> {
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        env::current_dir()?.join(path)
    };
    Ok(format!("file://{}", absolute.canonicalize()?.display()))
}

fn write_png(path: &Path, width: usize, height: usize, rgba: &[u8]) -> Result<(), Box<dyn Error>> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    let file = File::create(path)?;
    let mut encoder = png::Encoder::new(file, width as u32, height as u32);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header()?;
    writer.write_image_data(rgba)?;
    Ok(())
}
