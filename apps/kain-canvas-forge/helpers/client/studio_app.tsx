import { useEffect, useMemo, useRef, useState } from "preact/hooks";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

type Workspace = {
  id: string;
  name: string;
  summary: string;
  hero: string;
  modes: string[];
  scene_id: string;
};

type Tool = {
  id: string;
  name: string;
  hotkey: string;
  group: string;
  summary: string;
};

type BrushPreset = {
  id: string;
  name: string;
  size: number;
  opacity: number;
  flow: number;
  hardness: number;
  spacing: number;
  tip: string;
};

type PanelDefinition = {
  id: string;
  title: string;
  region: string;
  summary: string;
};

type SceneObject = {
  id: string;
  type: string;
  color: string;
  position: [number, number, number];
  scale: [number, number, number];
};

type SceneDefinition = {
  id: string;
  name: string;
  summary: string;
  fog: string;
  ambient: string;
  background: string;
  objects: SceneObject[];
};

type AppModel = {
  id: string;
  name: string;
  tagline: string;
  summary: string;
  document: {
    name: string;
    width: number;
    height: number;
    dpi: number;
    background: string;
  };
  theme: Record<string, string>;
  workspaces: Workspace[];
  tools: Tool[];
  brushes: BrushPreset[];
  panels: PanelDefinition[];
  scenes: SceneDefinition[];
  default_workspace_id: string;
};

type PaintLayer = {
  id: string;
  name: string;
  visible: boolean;
  locked: boolean;
  opacity: number;
};

type StrokePoint = {
  x: number;
  y: number;
};

type DrawingSession = {
  isDrawing: boolean;
  lastPoint: StrokePoint | null;
};

const defaultModel: AppModel = {
  id: "kain_canvas_forge",
  name: "Kain Canvas Forge",
  tagline: "Paint, stage, ink, and compose with a Node-first studio shell.",
  summary: "Illustration and 3D composition workstation scaffold.",
  document: {
    name: "chapter_ink_keyart",
    width: 1600,
    height: 900,
    dpi: 300,
    background: "#f4efe6",
  },
  theme: {
    shell: "#121318",
    panel: "#1b1f27",
    panel_alt: "#242a35",
    outline: "#394152",
    text: "#f5f2eb",
    muted: "#a8b0bf",
    accent: "#ff8f3f",
    accent_soft: "#ffd18c",
    canvas_backdrop: "#ddd1bb",
    viewport_backdrop: "#0d1118",
  },
  workspaces: [],
  tools: [],
  brushes: [],
  panels: [],
  scenes: [],
  default_workspace_id: "paint_lab",
};

function readModel(rawModel: unknown): AppModel {
  if (!rawModel || typeof rawModel !== "object") {
    return defaultModel;
  }

  return {
    ...defaultModel,
    ...(rawModel as Partial<AppModel>),
  };
}

function buildInitialLayers(): PaintLayer[] {
  return [
    { id: "paper", name: "Paper Tone", visible: true, locked: true, opacity: 1 },
    { id: "sketch", name: "Sketch", visible: true, locked: false, opacity: 0.84 },
    { id: "inks", name: "Inks", visible: true, locked: false, opacity: 1 },
    { id: "color", name: "Color Flats", visible: true, locked: false, opacity: 0.92 },
    { id: "fx", name: "Atmosphere FX", visible: true, locked: false, opacity: 0.66 },
  ];
}

function createLayerCanvas(width: number, height: number) {
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  return canvas;
}

function clampNumber(value: number, minValue: number, maxValue: number) {
  return Math.min(maxValue, Math.max(minValue, value));
}

function createBrushGradient(
  context: CanvasRenderingContext2D,
  x: number,
  y: number,
  brush: BrushPreset,
  color: string,
) {
  const radius = brush.size * 0.5;
  const hardCoreStop = clampNumber(brush.hardness * 0.85, 0.06, 0.95);
  const gradient = context.createRadialGradient(x, y, radius * 0.08, x, y, radius);
  gradient.addColorStop(0, color);
  gradient.addColorStop(hardCoreStop, color);
  gradient.addColorStop(1, "rgba(0, 0, 0, 0)");
  return gradient;
}

function stampBrush(
  context: CanvasRenderingContext2D,
  point: StrokePoint,
  brush: BrushPreset,
  color: string,
  isEraser: boolean,
) {
  const radius = brush.size * 0.5;
  const alpha = clampNumber(brush.opacity * brush.flow, 0.05, 1);

  context.save();
  context.globalAlpha = alpha;
  context.globalCompositeOperation = isEraser ? "destination-out" : "source-over";
  context.fillStyle = createBrushGradient(context, point.x, point.y, brush, color);

  if (brush.tip === "square") {
    context.fillRect(point.x - radius, point.y - radius, brush.size, brush.size);
  } else {
    context.beginPath();
    context.arc(point.x, point.y, radius, 0, Math.PI * 2);
    context.fill();
  }

  context.restore();
}

function drawStrokeSegment(
  context: CanvasRenderingContext2D,
  startPoint: StrokePoint,
  endPoint: StrokePoint,
  brush: BrushPreset,
  color: string,
  isEraser: boolean,
) {
  const deltaX = endPoint.x - startPoint.x;
  const deltaY = endPoint.y - startPoint.y;
  const distance = Math.hypot(deltaX, deltaY);
  const spacingDistance = Math.max(1, brush.size * clampNumber(brush.spacing, 0.02, 1));
  const stepCount = Math.max(1, Math.ceil(distance / spacingDistance));

  for (let stepIndex = 0; stepIndex <= stepCount; stepIndex += 1) {
    const progress = stepIndex / stepCount;
    const point = {
      x: startPoint.x + deltaX * progress,
      y: startPoint.y + deltaY * progress,
    };
    stampBrush(context, point, brush, color, isEraser);
  }
}

function hexToRgba(hexColor: string, alpha: number) {
  const normalized = hexColor.replace("#", "");
  if (normalized.length !== 6) {
    return `rgba(255, 255, 255, ${alpha})`;
  }

  const red = Number.parseInt(normalized.slice(0, 2), 16);
  const green = Number.parseInt(normalized.slice(2, 4), 16);
  const blue = Number.parseInt(normalized.slice(4, 6), 16);
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}

function meshFromSceneObject(sceneObject: SceneObject) {
  let geometry: THREE.BufferGeometry;

  switch (sceneObject.type) {
    case "box":
      geometry = new THREE.BoxGeometry(1, 1, 1);
      break;
    case "capsule":
      geometry = new THREE.CapsuleGeometry(0.5, 1, 8, 16);
      break;
    case "cylinder":
      geometry = new THREE.CylinderGeometry(0.5, 0.5, 1, 32);
      break;
    case "plane":
      geometry = new THREE.PlaneGeometry(1, 1);
      break;
    case "ring":
      geometry = new THREE.TorusGeometry(0.7, 0.08, 16, 64);
      break;
    case "sphere":
      geometry = new THREE.SphereGeometry(0.5, 24, 24);
      break;
    case "torusKnot":
      geometry = new THREE.TorusKnotGeometry(0.45, 0.14, 160, 18);
      break;
    default:
      geometry = new THREE.BoxGeometry(1, 1, 1);
      break;
  }

  const material = new THREE.MeshStandardMaterial({
    color: sceneObject.color,
    emissive: sceneObject.type === "ring" ? sceneObject.color : "#000000",
    emissiveIntensity: sceneObject.type === "ring" ? 0.55 : 0.08,
    metalness: sceneObject.type === "torusKnot" ? 0.45 : 0.18,
    roughness: sceneObject.type === "plane" ? 0.92 : 0.58,
    side: THREE.DoubleSide,
  });

  const mesh = new THREE.Mesh(geometry, material);
  mesh.name = sceneObject.id;
  mesh.position.set(...sceneObject.position);
  mesh.scale.set(...sceneObject.scale);

  if (sceneObject.type === "plane") {
    mesh.rotation.y = Math.PI;
  }

  return mesh;
}

function formatToolLabel(tool: Tool) {
  return `${tool.name} · ${tool.hotkey}`;
}

function WorkspaceHero({
  activeWorkspace,
  scene,
}: {
  activeWorkspace: Workspace | undefined;
  scene: SceneDefinition | undefined;
}) {
  if (!activeWorkspace) {
    return null;
  }

  return (
    <section class="hero-card panel-card">
      <div class="hero-copy">
        <p class="eyebrow">Workspace</p>
        <h1>{activeWorkspace.name}</h1>
        <p class="hero-text">{activeWorkspace.hero}</p>
      </div>
      <div class="hero-meta">
        <div>
          <span class="meta-label">Scene</span>
          <strong>{scene?.name ?? "Unassigned"}</strong>
        </div>
        <div>
          <span class="meta-label">Modes</span>
          <strong>{activeWorkspace.modes.join(" / ")}</strong>
        </div>
      </div>
    </section>
  );
}

function SceneViewport({
  sceneDefinition,
  viewportBackdrop,
}: {
  sceneDefinition: SceneDefinition | undefined;
  viewportBackdrop: string;
}) {
  const viewportRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const mountNode = viewportRef.current;
    if (!mountNode || !sceneDefinition) {
      return undefined;
    }

    const width = mountNode.clientWidth;
    const height = mountNode.clientHeight;

    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(width, height);
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.setClearColor(sceneDefinition.background ?? viewportBackdrop);
    mountNode.innerHTML = "";
    mountNode.appendChild(renderer.domElement);

    const stage = new THREE.Scene();
    stage.background = new THREE.Color(sceneDefinition.background ?? viewportBackdrop);
    stage.fog = new THREE.Fog(sceneDefinition.fog, 6, 18);

    const camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 100);
    camera.position.set(3.9, 2.8, 4.8);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.target.set(0, 1.1, 0);
    controls.maxDistance = 10;
    controls.minDistance = 2.1;

    const ambientLight = new THREE.AmbientLight(sceneDefinition.ambient, 1.6);
    stage.add(ambientLight);

    const keyLight = new THREE.DirectionalLight("#fff1d3", 2.4);
    keyLight.position.set(4, 5, 3);
    stage.add(keyLight);

    const rimLight = new THREE.DirectionalLight("#7dc9ff", 1.35);
    rimLight.position.set(-4, 3, -5);
    stage.add(rimLight);

    const gridHelper = new THREE.GridHelper(10, 20, "#4d667f", "#243140");
    gridHelper.position.y = -0.78;
    stage.add(gridHelper);

    const stageMeshes = sceneDefinition.objects.map((sceneObject) => {
      const mesh = meshFromSceneObject(sceneObject);
      stage.add(mesh);
      return mesh;
    });

    let frameHandle = 0;
    const renderFrame = () => {
      frameHandle = window.requestAnimationFrame(renderFrame);
      stageMeshes.forEach((mesh, index) => {
        if (mesh.name.includes("hero") || mesh.name.includes("torso") || mesh.name.includes("head")) {
          mesh.rotation.y += 0.004 + index * 0.0008;
        }
        if (mesh.name.includes("ring")) {
          mesh.rotation.z += 0.008;
        }
      });
      controls.update();
      renderer.render(stage, camera);
    };
    renderFrame();

    const resizeObserver = new ResizeObserver((entries) => {
      const contentRect = entries[0]?.contentRect;
      const nextWidth = contentRect?.width ?? mountNode.clientWidth;
      const nextHeight = contentRect?.height ?? mountNode.clientHeight;
      renderer.setSize(nextWidth, nextHeight);
      camera.aspect = nextWidth / Math.max(nextHeight, 1);
      camera.updateProjectionMatrix();
    });
    resizeObserver.observe(mountNode);

    return () => {
      window.cancelAnimationFrame(frameHandle);
      resizeObserver.disconnect();
      controls.dispose();
      renderer.dispose();
      stageMeshes.forEach((mesh) => {
        mesh.geometry.dispose();
        if (Array.isArray(mesh.material)) {
          mesh.material.forEach((material) => material.dispose());
        } else {
          mesh.material.dispose();
        }
      });
      mountNode.innerHTML = "";
    };
  }, [sceneDefinition, viewportBackdrop]);

  return <div class="viewport-surface" ref={viewportRef} />;
}

export function StudioApp({ rawModel }: { rawModel: unknown }) {
  const model = useMemo(() => readModel(rawModel), [rawModel]);
  const [activeWorkspaceId, setActiveWorkspaceId] = useState(model.default_workspace_id);
  const [activeToolId, setActiveToolId] = useState("brush");
  const [activeBrushId, setActiveBrushId] = useState(model.brushes[0]?.id ?? "");
  const [brushColor, setBrushColor] = useState("#1f2430");
  const [statusText, setStatusText] = useState("Canvas ready for blocking, inks, and pose staging.");
  const [layers, setLayers] = useState<PaintLayer[]>(() => buildInitialLayers());
  const [activeLayerId, setActiveLayerId] = useState("inks");
  const [layerCount, setLayerCount] = useState(0);
  const [viewportSceneId, setViewportSceneId] = useState<string | null>(null);

  const compositeCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const layerCanvasMapRef = useRef<Map<string, HTMLCanvasElement>>(new Map());
  const drawingSessionRef = useRef<DrawingSession>({ isDrawing: false, lastPoint: null });

  const activeWorkspace = useMemo(
    () => model.workspaces.find((workspace) => workspace.id === activeWorkspaceId) ?? model.workspaces[0],
    [activeWorkspaceId, model.workspaces],
  );
  const activeBrush = useMemo(
    () => model.brushes.find((brush) => brush.id === activeBrushId) ?? model.brushes[0],
    [activeBrushId, model.brushes],
  );
  const currentScene = useMemo(() => {
    const preferredSceneId = viewportSceneId ?? activeWorkspace?.scene_id;
    return model.scenes.find((scene) => scene.id === preferredSceneId) ?? model.scenes[0];
  }, [activeWorkspace?.scene_id, model.scenes, viewportSceneId]);
  const sceneObjects = currentScene?.objects ?? [];
  const groupNames = useMemo(
    () => Array.from(new Set(model.tools.map((tool) => tool.group))),
    [model.tools],
  );

  useEffect(() => {
    const previousLayerCanvasMap = layerCanvasMapRef.current;
    const nextLayerCanvasMap = new Map<string, HTMLCanvasElement>();

    layers.forEach((layer) => {
      let canvas = previousLayerCanvasMap.get(layer.id) ?? null;
      if (!canvas || canvas.width !== model.document.width || canvas.height !== model.document.height) {
        canvas = createLayerCanvas(model.document.width, model.document.height);
        const context = canvas.getContext("2d");
        if (context && layer.id === "paper") {
          context.fillStyle = model.document.background;
          context.fillRect(0, 0, canvas.width, canvas.height);
          context.strokeStyle = hexToRgba("#6f604b", 0.08);
          context.lineWidth = 2;
          for (let y = 32; y < canvas.height; y += 36) {
            context.beginPath();
            context.moveTo(0, y);
            context.lineTo(canvas.width, y);
            context.stroke();
          }
        }
      }

      if (!canvas) {
        return;
      }

      nextLayerCanvasMap.set(layer.id, canvas);
    });

    layerCanvasMapRef.current = nextLayerCanvasMap;
    setLayerCount(layers.length);
  }, [layers, model.document.background, model.document.height, model.document.width]);

  useEffect(() => {
    redrawCompositeCanvas();
  }, [layers]);

  useEffect(() => {
    document.title = model.name;
  }, [model.name]);

  useEffect(() => {
    if (!viewportSceneId && activeWorkspace?.scene_id) {
      setViewportSceneId(activeWorkspace.scene_id);
    }
  }, [activeWorkspace?.scene_id, viewportSceneId]);

  function getLayerCanvas(layerId: string) {
    return layerCanvasMapRef.current.get(layerId) ?? null;
  }

  function redrawCompositeCanvas() {
    const compositeCanvas = compositeCanvasRef.current;
    if (!compositeCanvas) {
      return;
    }

    compositeCanvas.width = model.document.width;
    compositeCanvas.height = model.document.height;

    const compositeContext = compositeCanvas.getContext("2d");
    if (!compositeContext) {
      return;
    }

    compositeContext.clearRect(0, 0, compositeCanvas.width, compositeCanvas.height);

    layers.forEach((layer) => {
      if (!layer.visible) {
        return;
      }

      const layerCanvas = getLayerCanvas(layer.id);
      if (!layerCanvas) {
        return;
      }

      compositeContext.save();
      compositeContext.globalAlpha = layer.opacity;
      compositeContext.drawImage(layerCanvas, 0, 0);
      compositeContext.restore();
    });
  }

  function activePaintLayer() {
    return layers.find((layer) => layer.id === activeLayerId);
  }

  function activePaintContext() {
    const paintLayer = activePaintLayer();
    if (!paintLayer || paintLayer.locked) {
      return null;
    }

    const paintCanvas = getLayerCanvas(paintLayer.id);
    return paintCanvas?.getContext("2d") ?? null;
  }

  function pointerToCanvasPoint(event: PointerEvent | MouseEvent) {
    const compositeCanvas = compositeCanvasRef.current;
    if (!compositeCanvas) {
      return null;
    }

    const canvasRect = compositeCanvas.getBoundingClientRect();
    const scaleX = compositeCanvas.width / canvasRect.width;
    const scaleY = compositeCanvas.height / canvasRect.height;

    return {
      x: (event.clientX - canvasRect.left) * scaleX,
      y: (event.clientY - canvasRect.top) * scaleY,
    };
  }

  function beginStroke(event: PointerEvent) {
    if (activeToolId !== "brush" && activeToolId !== "eraser") {
      setStatusText("Paint surface is in inspection mode. Switch back to Brush or Eraser to draw.");
      return;
    }

    const point = pointerToCanvasPoint(event);
    const context = activePaintContext();
    if (!point || !context || !activeBrush) {
      setStatusText("Select an editable layer and a brush preset before painting.");
      return;
    }

    drawingSessionRef.current = { isDrawing: true, lastPoint: point };
    stampBrush(context, point, activeBrush, brushColor, activeToolId === "eraser");
    redrawCompositeCanvas();
    setStatusText(`${activeToolId === "eraser" ? "Erasing" : "Painting"} on ${activeLayerId}.`);
  }

  function continueStroke(event: PointerEvent) {
    const drawingSession = drawingSessionRef.current;
    if (!drawingSession.isDrawing || !drawingSession.lastPoint) {
      return;
    }

    const point = pointerToCanvasPoint(event);
    const context = activePaintContext();
    if (!point || !context || !activeBrush) {
      return;
    }

    drawStrokeSegment(
      context,
      drawingSession.lastPoint,
      point,
      activeBrush,
      brushColor,
      activeToolId === "eraser",
    );
    drawingSessionRef.current = { isDrawing: true, lastPoint: point };
    redrawCompositeCanvas();
  }

  function endStroke() {
    if (!drawingSessionRef.current.isDrawing) {
      return;
    }

    drawingSessionRef.current = { isDrawing: false, lastPoint: null };
    setStatusText("Stroke committed to the active layer.");
  }

  function toggleLayerVisibility(layerId: string) {
    setLayers((currentLayers) =>
      currentLayers.map((layer) =>
        layer.id === layerId ? { ...layer, visible: !layer.visible } : layer,
      ),
    );
  }

  function createNewLayer() {
    const newLayerNumber = layers.length + 1;
    const newLayerId = `paint_pass_${newLayerNumber}`;
    setLayers((currentLayers) => [
      ...currentLayers,
      {
        id: newLayerId,
        name: `Paint Pass ${newLayerNumber}`,
        visible: true,
        locked: false,
        opacity: 1,
      },
    ]);
    setActiveLayerId(newLayerId);
    setStatusText(`Created ${newLayerId} for additional paint passes.`);
  }

  function clearActiveLayer() {
    const context = activePaintContext();
    const layerCanvas = getLayerCanvas(activeLayerId);
    if (!context || !layerCanvas) {
      return;
    }

    context.clearRect(0, 0, layerCanvas.width, layerCanvas.height);
    redrawCompositeCanvas();
    setStatusText(`Cleared ${activeLayerId}.`);
  }

  function exportCanvas() {
    const exportCanvas = document.createElement("canvas");
    exportCanvas.width = model.document.width;
    exportCanvas.height = model.document.height;
    const exportContext = exportCanvas.getContext("2d");
    if (!exportContext) {
      return;
    }

    exportContext.fillStyle = model.document.background;
    exportContext.fillRect(0, 0, exportCanvas.width, exportCanvas.height);

    layers.forEach((layer) => {
      if (!layer.visible) {
        return;
      }

      const layerCanvas = getLayerCanvas(layer.id);
      if (!layerCanvas) {
        return;
      }

      exportContext.save();
      exportContext.globalAlpha = layer.opacity;
      exportContext.drawImage(layerCanvas, 0, 0);
      exportContext.restore();
    });

    const downloadLink = document.createElement("a");
    downloadLink.href = exportCanvas.toDataURL("image/png");
    downloadLink.download = `${model.document.name}.png`;
    downloadLink.click();
    setStatusText(`Exported ${model.document.name}.png`);
  }

  const timelineSteps = [
    { title: "Thumbnail", summary: "Lock silhouette, energy, and camera intent." },
    { title: "Pose Pass", summary: "Use the 3D stage to solve gesture and framing." },
    { title: "Ink", summary: "Tighten lines, edge rhythm, and focal contrast." },
    { title: "Paint", summary: "Flat, shade, accent, and FX passes for final read." },
  ];

  return (
    <div
      class="studio-shell"
      style={{
        "--shell": model.theme.shell,
        "--panel": model.theme.panel,
        "--panel-alt": model.theme.panel_alt,
        "--outline": model.theme.outline,
        "--text": model.theme.text,
        "--muted": model.theme.muted,
        "--accent": model.theme.accent,
        "--accent-soft": model.theme.accent_soft,
        "--canvas-backdrop": model.theme.canvas_backdrop,
        "--viewport-backdrop": model.theme.viewport_backdrop,
      }}
    >
      <header class="topbar">
        <div class="brand-block">
          <p class="eyebrow">Node-first studio app</p>
          <strong>{model.name}</strong>
          <span>{model.tagline}</span>
        </div>
        <div class="workspace-strip">
          {model.workspaces.map((workspace) => (
            <button
              key={workspace.id}
              class={workspace.id === activeWorkspaceId ? "workspace-pill active" : "workspace-pill"}
              onClick={() => {
                setActiveWorkspaceId(workspace.id);
                setViewportSceneId(workspace.scene_id);
                setStatusText(`Switched to ${workspace.name}.`);
              }}
              type="button"
            >
              {workspace.name}
            </button>
          ))}
        </div>
        <div class="status-chip-row">
          <span class="status-chip">{model.document.width} x {model.document.height}</span>
          <span class="status-chip">{layerCount} layers</span>
          <span class="status-chip">{sceneObjects.length} scene props</span>
        </div>
      </header>

      <main class="studio-grid">
        <aside class="left-column">
          <WorkspaceHero activeWorkspace={activeWorkspace} scene={currentScene} />

          <section class="panel-card">
            <div class="panel-header">
              <h2>Workspace Modes</h2>
              <span>{activeWorkspace?.modes.length ?? 0}</span>
            </div>
            <div class="mode-grid">
              {(activeWorkspace?.modes ?? []).map((mode) => (
                <button
                  key={mode}
                  class="mode-chip"
                  onClick={() => setStatusText(`Mode focus: ${mode}`)}
                  type="button"
                >
                  {mode}
                </button>
              ))}
            </div>
          </section>

          <section class="panel-card">
            <div class="panel-header">
              <h2>Tool Rail</h2>
              <span>{model.tools.length}</span>
            </div>
            <div class="tool-groups">
              {groupNames.map((groupName) => (
                <div key={groupName} class="tool-group">
                  <p class="group-label">{groupName}</p>
                  <div class="tool-list">
                    {model.tools
                      .filter((tool) => tool.group === groupName)
                      .map((tool) => (
                        <button
                          key={tool.id}
                          class={tool.id === activeToolId ? "tool-button active" : "tool-button"}
                          onClick={() => {
                            setActiveToolId(tool.id);
                            setStatusText(tool.summary);
                          }}
                          type="button"
                        >
                          <span>{formatToolLabel(tool)}</span>
                          <small>{tool.summary}</small>
                        </button>
                      ))}
                  </div>
                </div>
              ))}
            </div>
          </section>
        </aside>

        <section class="center-column">
          <div class="center-stack">
            <section class="panel-card canvas-panel">
              <div class="panel-header">
                <h2>Canvas Stack</h2>
                <div class="action-row">
                  <label class="color-control">
                    <span>Ink</span>
                    <input
                      type="color"
                      value={brushColor}
                      onInput={(event) => setBrushColor((event.currentTarget as HTMLInputElement).value)}
                    />
                  </label>
                  <button class="ghost-button" onClick={clearActiveLayer} type="button">
                    Clear Layer
                  </button>
                  <button class="accent-button" onClick={exportCanvas} type="button">
                    Export PNG
                  </button>
                </div>
              </div>
              <div class="canvas-surface-shell">
                <canvas
                  class="paint-surface"
                  ref={compositeCanvasRef}
                  onPointerDown={(event) => beginStroke(event as unknown as PointerEvent)}
                  onPointerLeave={endStroke}
                  onPointerMove={(event) => continueStroke(event as unknown as PointerEvent)}
                  onPointerUp={endStroke}
                />
              </div>
            </section>

            <section class="panel-card viewport-panel">
              <div class="panel-header">
                <h2>Viewport Stage</h2>
                <div class="action-row">
                  <select
                    class="scene-select"
                    value={currentScene?.id}
                    onInput={(event) =>
                      setViewportSceneId((event.currentTarget as HTMLSelectElement).value)
                    }
                  >
                    {model.scenes.map((scene) => (
                      <option key={scene.id} value={scene.id}>
                        {scene.name}
                      </option>
                    ))}
                  </select>
                  <button
                    class="ghost-button"
                    onClick={() => setStatusText(currentScene?.summary ?? "Scene ready.")}
                    type="button"
                  >
                    Scene Notes
                  </button>
                </div>
              </div>
              <SceneViewport
                sceneDefinition={currentScene}
                viewportBackdrop={model.theme.viewport_backdrop}
              />
            </section>
          </div>

          <section class="panel-card timeline-panel">
            <div class="panel-header">
              <h2>Timeline</h2>
              <span>{timelineSteps.length} phases</span>
            </div>
            <div class="timeline-track">
              {timelineSteps.map((timelineStep, index) => (
                <button
                  key={timelineStep.title}
                  class="timeline-step"
                  onClick={() => setStatusText(`${timelineStep.title}: ${timelineStep.summary}`)}
                  type="button"
                >
                  <span class="timeline-index">{index + 1}</span>
                  <strong>{timelineStep.title}</strong>
                  <p>{timelineStep.summary}</p>
                </button>
              ))}
            </div>
          </section>
        </section>

        <aside class="right-column">
          <section class="panel-card">
            <div class="panel-header">
              <h2>Layers</h2>
              <button class="ghost-button" onClick={createNewLayer} type="button">
                Add Layer
              </button>
            </div>
            <div class="layer-list">
              {layers
                .slice()
                .reverse()
                .map((layer) => (
                  <button
                    key={layer.id}
                    class={layer.id === activeLayerId ? "layer-row active" : "layer-row"}
                    onClick={() => {
                      setActiveLayerId(layer.id);
                      setStatusText(`Active layer: ${layer.name}`);
                    }}
                    type="button"
                  >
                    <div class="layer-copy">
                      <strong>{layer.name}</strong>
                      <small>{layer.locked ? "Locked foundation" : `${Math.round(layer.opacity * 100)}% opacity`}</small>
                    </div>
                    <div class="layer-actions">
                      <span class={layer.visible ? "layer-visibility visible" : "layer-visibility"}>
                        {layer.visible ? "Visible" : "Hidden"}
                      </span>
                      {!layer.locked && (
                        <span
                          class="layer-toggle"
                          onClick={(event) => {
                            event.stopPropagation();
                            toggleLayerVisibility(layer.id);
                          }}
                        >
                          Toggle
                        </span>
                      )}
                    </div>
                  </button>
                ))}
            </div>
          </section>

          <section class="panel-card">
            <div class="panel-header">
              <h2>Brush Presets</h2>
              <span>{model.brushes.length}</span>
            </div>
            <div class="brush-list">
              {model.brushes.map((brush) => (
                <button
                  key={brush.id}
                  class={brush.id === activeBrushId ? "brush-row active" : "brush-row"}
                  onClick={() => {
                    setActiveBrushId(brush.id);
                    setStatusText(`${brush.name} ready for the active layer.`);
                  }}
                  type="button"
                >
                  <div>
                    <strong>{brush.name}</strong>
                    <p>{brush.tip} tip · size {brush.size}</p>
                  </div>
                  <small>{Math.round(brush.opacity * 100)}%</small>
                </button>
              ))}
            </div>
          </section>

          <section class="panel-card">
            <div class="panel-header">
              <h2>Scene Inspector</h2>
              <span>{sceneObjects.length}</span>
            </div>
            <div class="inspector-block">
              <p class="inspector-summary">{currentScene?.summary}</p>
              <div class="scene-object-list">
                {sceneObjects.map((sceneObject) => (
                  <div key={sceneObject.id} class="scene-object-row">
                    <strong>{sceneObject.id}</strong>
                    <span>{sceneObject.type}</span>
                    <code>{sceneObject.position.join(", ")}</code>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section class="panel-card status-panel">
            <div class="panel-header">
              <h2>Session Feed</h2>
              <span>Live</span>
            </div>
            <p>{statusText}</p>
            <div class="feed-grid">
              {model.panels.map((panel) => (
                <div key={panel.id} class="feed-cell">
                  <strong>{panel.title}</strong>
                  <span>{panel.summary}</span>
                </div>
              ))}
            </div>
          </section>
        </aside>
      </main>
    </div>
  );
}
