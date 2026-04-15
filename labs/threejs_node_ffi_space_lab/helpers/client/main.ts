import * as THREE from "three";

import { readAppModel, type AppModel, type SculptToolConfig } from "./model";
import { UniversalViewportController } from "./universal-viewport";
import { WasmSculptCore } from "./wasm-sculpt-core";

type BrushState = {
  radius: number;
  strength: number;
  falloffPower: number;
};

type HybridMotionBindings = {
  orb_hover_units: (frameUnits: number, cycleUnits: number, amplitudeUnits: number) => number;
  orb_hover_units_async: (
    frameUnits: number,
    cycleUnits: number,
    amplitudeUnits: number,
  ) => Promise<number>;
  beacon_spin_units: (frameUnits: number, speedUnits: number, cycleUnits: number) => number;
};

declare global {
  interface Window {
    __KAIN_HYBRID_BINDINGS__?: Record<string, unknown>;
  }
}

function mustQuery<T extends Element>(scope: ParentNode, selector: string): T {
  const element = scope.querySelector<T>(selector);

  if (!element) {
    throw new Error(`Missing required element ${selector}.`);
  }

  return element;
}

function formatVector(vector: THREE.Vector3): string {
  return `${vector.x.toFixed(1)}, ${vector.y.toFixed(1)}, ${vector.z.toFixed(1)}`;
}

function readHybridMotionBindings(source: Window & typeof globalThis): HybridMotionBindings {
  const bindings = source.__KAIN_HYBRID_BINDINGS__;

  if (
    !bindings ||
    typeof bindings.orb_hover_units !== "function" ||
    typeof bindings.orb_hover_units_async !== "function" ||
    typeof bindings.beacon_spin_units !== "function"
  ) {
    throw new Error("Missing required Kain hybrid motion bindings.");
  }

  return {
    orb_hover_units: bindings.orb_hover_units as HybridMotionBindings["orb_hover_units"],
    orb_hover_units_async: bindings.orb_hover_units_async as HybridMotionBindings["orb_hover_units_async"],
    beacon_spin_units: bindings.beacon_spin_units as HybridMotionBindings["beacon_spin_units"],
  };
}

function rotationUnitsToRadians(rotationUnits: number, cycleUnits: number): number {
  if (cycleUnits <= 0) {
    return 0;
  }

  return (rotationUnits / cycleUnits) * Math.PI * 2;
}

function createStarField(scene: THREE.Scene, starCount: number, radius: number) {
  const positions = new Float32Array(starCount * 3);

  for (let index = 0; index < starCount; index += 1) {
    const angle = Math.random() * Math.PI * 2;
    const polar = Math.acos(THREE.MathUtils.randFloatSpread(2));
    const distance = THREE.MathUtils.randFloat(radius * 0.3, radius);
    const offset = index * 3;
    positions[offset] = Math.sin(polar) * Math.cos(angle) * distance;
    positions[offset + 1] = Math.cos(polar) * distance;
    positions[offset + 2] = Math.sin(polar) * Math.sin(angle) * distance;
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
  const material = new THREE.PointsMaterial({
    color: "#bddcff",
    size: 1.25,
    sizeAttenuation: true,
  });

  const points = new THREE.Points(geometry, material);
  scene.add(points);
  return points;
}

function createBeaconRing(scene: THREE.Scene, model: AppModel) {
  const beaconGroup = new THREE.Group();
  const beaconGeometry = new THREE.BoxGeometry(4.2, model.scene.environment.beacon_height, 4.2);
  const beaconMaterial = new THREE.MeshStandardMaterial({
    color: "#2b7eff",
    emissive: "#123b8a",
    metalness: 0.28,
    roughness: 0.3,
  });

  for (let index = 0; index < model.scene.environment.beacon_count; index += 1) {
    const fraction = index / model.scene.environment.beacon_count;
    const angle = fraction * Math.PI * 2;
    const beacon = new THREE.Mesh(beaconGeometry, beaconMaterial);
    beacon.position.set(
      Math.cos(angle) * model.scene.environment.beacon_ring_radius,
      model.scene.environment.beacon_height / 2 - 3,
      Math.sin(angle) * model.scene.environment.beacon_ring_radius,
    );
    beacon.rotation.y = angle;
    beaconGroup.add(beacon);
  }

  scene.add(beaconGroup);
  return beaconGroup;
}

function createShellMarkup(model: AppModel) {
  const modeButtons = model.viewport_profiles.modes
    .map(
      (mode) => `
        <button type="button" class="mode-button" data-mode-button data-mode-id="${mode.id}">
          <span>${mode.label}</span>
          <small>${mode.hotkey}</small>
        </button>
      `,
    )
    .join("");

  const toolButtons = model.sculpt_suite.tools
    .map(
      (tool) => `
        <button
          type="button"
          class="tool-button"
          data-tool-button
          data-tool-id="${tool.id}"
          style="--tool-accent:${tool.accent_color}"
        >
          <span>${tool.label}</span>
        </button>
      `,
    )
    .join("");

  return `
    <div class="suite-shell">
      <header class="suite-header">
        <div>
          <span class="eyebrow">Kain Node FFI + Three.js + WASM</span>
          <h1>${model.name}</h1>
          <p>${model.summary}</p>
        </div>
        <div class="header-chip-row">
          <span class="header-chip">${model.scene.name}</span>
          <span class="header-chip">${model.hybrid_pipeline.bundle_name}.hybrid</span>
          <span class="header-chip">${model.wasm_pipeline.crate_name}.wasm</span>
          <span class="header-chip">${model.viewport_profiles.name}</span>
        </div>
      </header>
      <div class="suite-grid">
        <aside class="chrome-rail">
          <section class="chrome-card">
            <span class="card-kicker">Universal Viewport</span>
            <h2>Modes</h2>
            <div class="button-stack">${modeButtons}</div>
          </section>
          <section class="chrome-card">
            <span class="card-kicker">Brush Rack</span>
            <h2>Tools</h2>
            <div class="button-stack">${toolButtons}</div>
            <p class="card-copy" data-tool-description>${model.sculpt_suite.summary}</p>
          </section>
          <section class="chrome-card">
            <span class="card-kicker">Actions</span>
            <h2>Stage</h2>
            <div class="button-stack compact">
              <button type="button" class="action-button" data-lock-button>Enter Fly</button>
              <button type="button" class="action-button" data-focus-button>Frame Orb</button>
              <button type="button" class="action-button" data-reset-button>Reset Sculpt</button>
            </div>
            <p class="card-copy" data-mode-description>${model.viewport_profiles.summary}</p>
          </section>
        </aside>
        <main class="viewport-column">
          <div class="viewport-stage" data-viewport-stage></div>
          <div class="viewport-overlay">
            <div class="mode-badge" data-active-mode-badge>${model.viewport_profiles.primary_mode}</div>
            <div class="crosshair" data-crosshair></div>
          </div>
          <div class="status-strip">
            <div class="status-pill"><span>Mode</span><strong data-mode-value>-</strong></div>
            <div class="status-pill"><span>Tool</span><strong data-tool-value>-</strong></div>
            <div class="status-pill"><span>Camera</span><strong data-camera-value>0, 0, 0</strong></div>
            <div class="status-pill"><span>Range</span><strong data-range-value>0.0</strong></div>
            <div class="status-pill"><span>Hybrid</span><strong data-hybrid-value>Booting</strong></div>
            <div class="status-pill"><span>WASM</span><strong data-wasm-value>Loading</strong></div>
            <div class="status-pill"><span>Stroke</span><strong data-stroke-value>0</strong></div>
          </div>
        </main>
        <aside class="chrome-rail">
          <section class="chrome-card">
            <span class="card-kicker">Brush Envelope</span>
            <h2>Shape</h2>
            <label class="slider-field">
              <span>Radius</span>
              <input type="range" min="1" max="12" step="0.1" data-radius-slider />
              <strong data-radius-value>0.0</strong>
            </label>
            <label class="slider-field">
              <span>Strength</span>
              <input type="range" min="0.05" max="1.2" step="0.01" data-strength-slider />
              <strong data-strength-value>0.0</strong>
            </label>
            <label class="slider-field">
              <span>Falloff</span>
              <input type="range" min="0.5" max="4" step="0.1" data-falloff-slider />
              <strong data-falloff-value>0.0</strong>
            </label>
          </section>
          <section class="chrome-card">
            <span class="card-kicker">Metrics</span>
            <h2>Orb</h2>
            <div class="metric-row"><span>Vertices</span><strong data-vertex-value>0</strong></div>
            <div class="metric-row"><span>Triangles</span><strong data-triangle-value>0</strong></div>
            <div class="metric-row"><span>Bounds Radius</span><strong data-bounds-value>0.0</strong></div>
            <div class="metric-row"><span>Pointer</span><strong data-pointer-value>Idle</strong></div>
          </section>
          <section class="chrome-card">
            <span class="card-kicker">Hints</span>
            <h2>Controls</h2>
            <p class="card-copy">Hotkeys: <strong>1</strong>/<strong>2</strong>/<strong>3</strong> switch modes, <strong>F</strong> enters fly, <strong>R</strong> resets the orb, <strong>[</strong>/<strong>]</strong> resize the brush.</p>
            <p class="card-copy">Sculpt mode uses left-drag for strokes and right-drag for camera tumble. Fly mode uses pointer lock plus <strong>WASD</strong>, <strong>Shift</strong>, <strong>Space</strong>, and <strong>Ctrl</strong>.</p>
          </section>
        </aside>
      </div>
    </div>
  `;
}

async function bootstrap() {
  const model = readAppModel(window.__KAIN_THREE_SPACE_MODEL__);
  const mountTarget = document.getElementById("app-root");

  if (!mountTarget) {
    throw new Error("Missing app root.");
  }

  mountTarget.innerHTML = createShellMarkup(model);
  document.title = model.name;

  const stage = mustQuery<HTMLElement>(mountTarget, "[data-viewport-stage]");
  const crosshair = mustQuery<HTMLElement>(mountTarget, "[data-crosshair]");
  const lockButton = mustQuery<HTMLButtonElement>(mountTarget, "[data-lock-button]");
  const focusButton = mustQuery<HTMLButtonElement>(mountTarget, "[data-focus-button]");
  const resetButton = mustQuery<HTMLButtonElement>(mountTarget, "[data-reset-button]");
  const radiusSlider = mustQuery<HTMLInputElement>(mountTarget, "[data-radius-slider]");
  const strengthSlider = mustQuery<HTMLInputElement>(mountTarget, "[data-strength-slider]");
  const falloffSlider = mustQuery<HTMLInputElement>(mountTarget, "[data-falloff-slider]");
  const radiusValue = mustQuery<HTMLElement>(mountTarget, "[data-radius-value]");
  const strengthValue = mustQuery<HTMLElement>(mountTarget, "[data-strength-value]");
  const falloffValue = mustQuery<HTMLElement>(mountTarget, "[data-falloff-value]");
  const cameraValue = mustQuery<HTMLElement>(mountTarget, "[data-camera-value]");
  const rangeValue = mustQuery<HTMLElement>(mountTarget, "[data-range-value]");
  const modeValue = mustQuery<HTMLElement>(mountTarget, "[data-mode-value]");
  const toolValue = mustQuery<HTMLElement>(mountTarget, "[data-tool-value]");
  const hybridValue = mustQuery<HTMLElement>(mountTarget, "[data-hybrid-value]");
  const wasmValue = mustQuery<HTMLElement>(mountTarget, "[data-wasm-value]");
  const strokeValue = mustQuery<HTMLElement>(mountTarget, "[data-stroke-value]");
  const vertexValue = mustQuery<HTMLElement>(mountTarget, "[data-vertex-value]");
  const triangleValue = mustQuery<HTMLElement>(mountTarget, "[data-triangle-value]");
  const boundsValue = mustQuery<HTMLElement>(mountTarget, "[data-bounds-value]");
  const pointerValue = mustQuery<HTMLElement>(mountTarget, "[data-pointer-value]");
  const activeModeBadge = mustQuery<HTMLElement>(mountTarget, "[data-active-mode-badge]");
  const toolDescription = mustQuery<HTMLElement>(mountTarget, "[data-tool-description]");
  const modeDescription = mustQuery<HTMLElement>(mountTarget, "[data-mode-description]");

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.shadowMap.enabled = false;
  stage.appendChild(renderer.domElement);
  renderer.domElement.classList.add("viewport-canvas");
  renderer.domElement.addEventListener("contextmenu", (event) => event.preventDefault());

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(model.scene.environment.background);
  scene.fog = new THREE.Fog(model.scene.environment.fog, 140, 1900);

  const camera = new THREE.PerspectiveCamera(
    model.scene.camera.fov,
    1,
    model.scene.camera.near,
    model.scene.camera.far,
  );
  camera.position.set(...model.scene.camera.spawn);
  scene.add(camera);

  const viewportController = new UniversalViewportController(
    camera,
    renderer.domElement,
    model.viewport_profiles.modes,
    {
      walkSpeed: model.scene.camera.fly_walk_speed,
      sprintMultiplier: model.scene.camera.fly_sprint_multiplier,
      riseSpeed: model.scene.camera.fly_rise_speed,
    },
    new THREE.Vector3(...model.scene.camera.orbit_target),
    model.viewport_profiles.primary_mode,
  );

  const ambientLight = new THREE.AmbientLight("#ffffff", model.scene.lights.ambient_intensity);
  const hemisphereLight = new THREE.HemisphereLight(
    "#9dd9ff",
    "#06101c",
    model.scene.lights.hemisphere_intensity,
  );
  const keyLight = new THREE.DirectionalLight("#ffffff", model.scene.lights.key_intensity);
  keyLight.position.set(26, 48, 18);
  const rimLight = new THREE.DirectionalLight("#75c8ff", model.scene.lights.rim_intensity);
  rimLight.position.set(-24, 16, -22);

  scene.add(ambientLight);
  scene.add(hemisphereLight);
  scene.add(keyLight);
  scene.add(rimLight);

  const grid = new THREE.GridHelper(
    model.scene.environment.grid_size,
    model.scene.environment.grid_divisions,
    "#6eb8ff",
    "#15324e",
  );
  grid.position.y = -3;
  scene.add(grid);

  const platform = new THREE.Mesh(
    new THREE.CircleGeometry(model.scene.environment.platform_radius, 96),
    new THREE.MeshBasicMaterial({
      color: model.scene.environment.platform_color,
      transparent: true,
      opacity: 0.55,
      side: THREE.DoubleSide,
    }),
  );
  platform.rotation.x = -Math.PI / 2;
  platform.position.y = -2.96;
  scene.add(platform);

  const starField = createStarField(
    scene,
    model.scene.environment.star_count,
    model.scene.environment.star_field_radius,
  );
  const beaconRing = createBeaconRing(scene, model);

  const heroGroup = new THREE.Group();
  heroGroup.position.set(...model.scene.hero_orb.position);
  scene.add(heroGroup);

  const heroGeometry = new THREE.IcosahedronGeometry(
    model.scene.hero_orb.radius,
    model.scene.hero_orb.detail,
  );
  heroGeometry.computeVertexNormals();
  heroGeometry.computeBoundingSphere();

  const heroMaterial = new THREE.MeshStandardMaterial({
    color: model.scene.hero_orb.surface_color,
    emissive: model.scene.hero_orb.emissive_color,
    emissiveIntensity: 0.9,
    metalness: 0.12,
    roughness: 0.26,
  });

  const heroMesh = new THREE.Mesh(heroGeometry, heroMaterial);
  heroGroup.add(heroMesh);

  const haloMesh = new THREE.Mesh(
    new THREE.IcosahedronGeometry(
      model.scene.hero_orb.radius * model.scene.hero_orb.halo_scale,
      Math.max(model.scene.hero_orb.detail - 1, 3),
    ),
    new THREE.MeshBasicMaterial({
      color: model.scene.hero_orb.halo_color,
      transparent: true,
      opacity: 0.12,
      wireframe: true,
    }),
  );
  heroGroup.add(haloMesh);

  const wireShell = new THREE.Mesh(
    new THREE.IcosahedronGeometry(
      model.scene.hero_orb.radius * 1.03,
      Math.max(model.scene.hero_orb.detail - 2, 2),
    ),
    new THREE.MeshBasicMaterial({
      color: model.scene.hero_orb.wire_color,
      transparent: true,
      opacity: 0.12,
      wireframe: true,
    }),
  );
  heroGroup.add(wireShell);

  const accentLight = new THREE.PointLight(model.scene.hero_orb.halo_color, 25, 160, 2);
  heroGroup.add(accentLight);

  const brushIndicator = new THREE.Mesh(
    new THREE.RingGeometry(0.84, 1, 48),
    new THREE.MeshBasicMaterial({
      color: "#f6fcff",
      transparent: true,
      opacity: 0.88,
      side: THREE.DoubleSide,
    }),
  );
  brushIndicator.visible = false;
  scene.add(brushIndicator);

  const initialPositionArray = new Float32Array(
    (heroGeometry.attributes.position.array as Float32Array).slice(),
  );

  const toolById = new Map(model.sculpt_suite.tools.map((tool) => [tool.id, tool]));
  let activeTool = toolById.get(model.sculpt_suite.defaults.initial_tool) ?? model.sculpt_suite.tools[0];
  const brushState: BrushState = {
    radius: activeTool.default_radius,
    strength: activeTool.default_strength,
    falloffPower: activeTool.falloff_power,
  };

  const hybridMotion = readHybridMotionBindings(window);
  let hybridStatusText = "JS ready";

  try {
    await hybridMotion.orb_hover_units_async(
      0,
      model.hybrid_pipeline.float_cycle_units,
      model.hybrid_pipeline.float_amplitude_units,
    );
    hybridStatusText = "JS + WASM";
  } catch (error) {
    console.error("[Kain Hybrid] Warmup failed.", error);
    hybridStatusText = "JS only";
  }

  hybridValue.textContent = hybridStatusText;

  const wasmCore = await WasmSculptCore.load(
    new URL(model.wasm_pipeline.public_path, window.location.href).toString(),
  );
  wasmValue.textContent = `${model.wasm_pipeline.crate_name}.wasm`;

  const modeButtons = Array.from(
    mountTarget.querySelectorAll<HTMLButtonElement>("[data-mode-button]"),
  );
  const toolButtons = Array.from(
    mountTarget.querySelectorAll<HTMLButtonElement>("[data-tool-button]"),
  );

  const raycaster = new THREE.Raycaster();
  const pointer = new THREE.Vector2();
  const worldHitNormal = new THREE.Vector3();
  const localHitPoint = new THREE.Vector3();
  const localHitNormal = new THREE.Vector3();
  const ringNormal = new THREE.Vector3(0, 0, 1);
  const orbitFrameOffset = new THREE.Vector3(32, 22, 36);
  const targetVector = new THREE.Vector3();

  let lastStrokeAffectedCount = 0;
  let sculptStrokeActive = false;
  let lastStrokePoint: THREE.Vector3 | null = null;

  const setBrushValues = () => {
    radiusSlider.value = brushState.radius.toFixed(2);
    strengthSlider.value = brushState.strength.toFixed(2);
    falloffSlider.value = brushState.falloffPower.toFixed(2);
    radiusValue.textContent = brushState.radius.toFixed(1);
    strengthValue.textContent = brushState.strength.toFixed(2);
    falloffValue.textContent = brushState.falloffPower.toFixed(1);
  };

  const setActiveTool = (tool: SculptToolConfig) => {
    activeTool = tool;
    brushState.radius = tool.default_radius;
    brushState.strength = tool.default_strength;
    brushState.falloffPower = tool.falloff_power;
    toolDescription.textContent = tool.description;
    setBrushValues();
    toolButtons.forEach((button) => {
      button.classList.toggle("is-active", button.dataset.toolId === tool.id);
    });
  };

  const setActiveMode = (modeId: string) => {
    viewportController.setMode(modeId);
    const activeMode = viewportController.activeMode;
    modeDescription.textContent = activeMode.description;
    crosshair.classList.toggle("is-visible", activeMode.show_crosshair);
    activeModeBadge.textContent = activeMode.label;
    lockButton.textContent = activeMode.id === "fly" ? "Enter Fly" : "Jump To Fly";
    modeButtons.forEach((button) => {
      button.classList.toggle("is-active", button.dataset.modeId === modeId);
    });
  };

  const updatePointerFromEvent = (event: PointerEvent) => {
    const rect = renderer.domElement.getBoundingClientRect();
    pointer.x = ((event.clientX - rect.left) / Math.max(rect.width, 1)) * 2 - 1;
    pointer.y = -(((event.clientY - rect.top) / Math.max(rect.height, 1)) * 2 - 1);
  };

  const updateBrushIndicator = (event: PointerEvent) => {
    updatePointerFromEvent(event);
    raycaster.setFromCamera(pointer, camera);
    const hit = raycaster.intersectObject(heroMesh, false)[0];

    if (!hit || !hit.face) {
      brushIndicator.visible = false;
      pointerValue.textContent = "No surface";
      return null;
    }

    worldHitNormal.copy(hit.face.normal).normalize();
    brushIndicator.visible = viewportController.activeMode.allow_sculpt;
    brushIndicator.position.copy(hit.point);
    brushIndicator.scale.setScalar(brushState.radius);
    brushIndicator.quaternion.setFromUnitVectors(ringNormal, worldHitNormal);
    pointerValue.textContent = "Surface locked";

    localHitPoint.copy(hit.point);
    heroMesh.worldToLocal(localHitPoint);
    localHitNormal.copy(hit.face.normal).normalize();

    return {
      point: localHitPoint.clone(),
      normal: localHitNormal.clone(),
    };
  };

  const applyBrushStroke = (point: THREE.Vector3, normal: THREE.Vector3) => {
    const positionAttribute = heroGeometry.getAttribute("position");
    const positionArray = positionAttribute.array as Float32Array;

    lastStrokeAffectedCount = wasmCore.applyBrush(positionArray, {
      center: point,
      normal,
      radius: brushState.radius,
      strength: brushState.strength,
      operationCode: activeTool.operation_code,
      falloffPower: brushState.falloffPower,
    });

    positionAttribute.needsUpdate = true;
    heroGeometry.computeVertexNormals();
    heroGeometry.computeBoundingSphere();
  };

  const resetHeroMesh = () => {
    const positionAttribute = heroGeometry.getAttribute("position");
    (positionAttribute.array as Float32Array).set(initialPositionArray);
    positionAttribute.needsUpdate = true;
    heroGeometry.computeVertexNormals();
    heroGeometry.computeBoundingSphere();
    lastStrokeAffectedCount = 0;
  };

  const frameHero = () => {
    camera.position.copy(heroGroup.position).add(orbitFrameOffset);
    targetVector.copy(heroGroup.position);
    viewportController.syncOrbitTarget(targetVector);
  };

  setActiveTool(activeTool);
  setActiveMode(model.viewport_profiles.primary_mode);

  radiusSlider.addEventListener("input", () => {
    brushState.radius = Number(radiusSlider.value);
    setBrushValues();
  });
  strengthSlider.addEventListener("input", () => {
    brushState.strength = Number(strengthSlider.value);
    setBrushValues();
  });
  falloffSlider.addEventListener("input", () => {
    brushState.falloffPower = Number(falloffSlider.value);
    setBrushValues();
  });

  modeButtons.forEach((button) => {
    button.addEventListener("click", () => {
      setActiveMode(button.dataset.modeId ?? model.viewport_profiles.primary_mode);
    });
  });

  toolButtons.forEach((button) => {
    button.addEventListener("click", () => {
      const tool = toolById.get(button.dataset.toolId ?? "");
      if (tool) {
        setActiveTool(tool);
      }
    });
  });

  lockButton.addEventListener("click", () => {
    if (viewportController.activeMode.id !== "fly") {
      setActiveMode("fly");
    }
    viewportController.requestPointerLock();
  });

  focusButton.addEventListener("click", () => {
    if (viewportController.activeMode.id === "fly") {
      setActiveMode("orbit");
    }
    frameHero();
  });

  resetButton.addEventListener("click", () => {
    resetHeroMesh();
  });

  const onResize = () => {
    const width = Math.max(stage.clientWidth, 1);
    const height = Math.max(stage.clientHeight, 1);
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
  };

  onResize();
  window.addEventListener("resize", onResize);

  const onPointerDown = (event: PointerEvent) => {
    if (event.button !== 0 || !viewportController.activeMode.allow_sculpt) {
      return;
    }

    const hit = updateBrushIndicator(event);

    if (!hit) {
      return;
    }

    sculptStrokeActive = true;
    lastStrokePoint = hit.point.clone();
    viewportController.setSculptGestureActive(true);
    renderer.domElement.setPointerCapture(event.pointerId);
    applyBrushStroke(hit.point, hit.normal);
  };

  const onPointerMove = (event: PointerEvent) => {
    const hit = updateBrushIndicator(event);

    if (!sculptStrokeActive || !hit) {
      return;
    }

    if (lastStrokePoint && lastStrokePoint.distanceTo(hit.point) < brushState.radius * 0.14) {
      return;
    }

    lastStrokePoint = hit.point.clone();
    applyBrushStroke(hit.point, hit.normal);
  };

  const endStroke = () => {
    sculptStrokeActive = false;
    lastStrokePoint = null;
    viewportController.setSculptGestureActive(false);
  };

  renderer.domElement.addEventListener("pointerdown", onPointerDown);
  renderer.domElement.addEventListener("pointermove", onPointerMove);
  renderer.domElement.addEventListener("pointerup", endStroke);
  renderer.domElement.addEventListener("pointerleave", () => {
    if (!sculptStrokeActive) {
      brushIndicator.visible = false;
      pointerValue.textContent = "Idle";
    }
  });

  const isTypingContext = () => {
    const activeElement = document.activeElement;
    return Boolean(activeElement && /INPUT|TEXTAREA|SELECT/.test(activeElement.tagName));
  };

  const onKeyDown = (event: KeyboardEvent) => {
    if (isTypingContext()) {
      return;
    }

    if (event.code === "Digit1") setActiveMode("sculpt");
    if (event.code === "Digit2") setActiveMode("orbit");
    if (event.code === "Digit3") setActiveMode("fly");
    if (event.code === "KeyF") {
      setActiveMode("fly");
      viewportController.requestPointerLock();
    }
    if (event.code === "KeyR") resetHeroMesh();
    if (event.code === "BracketLeft") {
      brushState.radius = Math.max(1, brushState.radius - 0.4);
      setBrushValues();
    }
    if (event.code === "BracketRight") {
      brushState.radius = Math.min(12, brushState.radius + 0.4);
      setBrushValues();
    }
  };

  window.addEventListener("keydown", onKeyDown);

  const clock = new THREE.Clock();
  const heroBaseY = model.scene.hero_orb.position[1];
  let frameUnits = 0;

  const renderFrame = () => {
    requestAnimationFrame(renderFrame);

    const deltaSeconds = Math.min(clock.getDelta(), 0.05);
    frameUnits += 1;

    const orbHoverUnits = hybridMotion.orb_hover_units(
      frameUnits,
      model.hybrid_pipeline.float_cycle_units,
      model.hybrid_pipeline.float_amplitude_units,
    );
    const beaconSpinUnits = hybridMotion.beacon_spin_units(
      frameUnits,
      model.hybrid_pipeline.beacon_spin_speed_units,
      model.hybrid_pipeline.rotation_cycle_units,
    );
    const starDriftUnits = hybridMotion.beacon_spin_units(
      frameUnits,
      model.hybrid_pipeline.star_drift_speed_units,
      model.hybrid_pipeline.rotation_cycle_units,
    );

    heroGroup.position.y =
      heroBaseY +
      (orbHoverUnits / model.hybrid_pipeline.float_amplitude_units) * model.scene.hero_orb.float_amplitude;
    haloMesh.rotation.y -= model.scene.hero_orb.rotation_speed * 0.65 * deltaSeconds;
    haloMesh.rotation.x += model.scene.hero_orb.rotation_speed * 0.3 * deltaSeconds;
    wireShell.rotation.y += model.scene.hero_orb.rotation_speed * deltaSeconds;
    beaconRing.rotation.y = rotationUnitsToRadians(
      beaconSpinUnits,
      model.hybrid_pipeline.rotation_cycle_units,
    );
    starField.rotation.y = rotationUnitsToRadians(
      starDriftUnits,
      model.hybrid_pipeline.rotation_cycle_units,
    );

    viewportController.syncOrbitTarget(heroGroup.position);
    viewportController.update(deltaSeconds);

    modeValue.textContent = viewportController.activeMode.label;
    toolValue.textContent = activeTool.label;
    cameraValue.textContent = formatVector(camera.position);
    rangeValue.textContent = camera.position.distanceTo(heroGroup.position).toFixed(1);
    strokeValue.textContent = String(lastStrokeAffectedCount);
    vertexValue.textContent = heroGeometry.getAttribute("position").count.toLocaleString();
    triangleValue.textContent = String((heroGeometry.index?.count ?? 0) / 3);
    boundsValue.textContent = heroGeometry.boundingSphere?.radius.toFixed(1) ?? "0.0";
    if (viewportController.isPointerLocked) {
      pointerValue.textContent = "Fly lock";
    }

    renderer.render(scene, camera);
  };

  renderFrame();

  window.addEventListener("beforeunload", () => {
    viewportController.dispose();
    wasmCore.dispose();
  });
}

void bootstrap();
