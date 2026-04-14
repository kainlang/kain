import * as THREE from "three";
import { PointerLockControls } from "three/examples/jsm/controls/PointerLockControls.js";

declare global {
  interface Window {
    __KAIN_THREE_SPACE_MODEL__?: unknown;
  }
}

type Vector3Tuple = [number, number, number];

type AppModel = {
  name: string;
  tagline: string;
  summary: string;
  scene: {
    name: string;
    summary: string;
    camera: {
      fov: number;
      near: number;
      far: number;
      spawn: Vector3Tuple;
      walk_speed: number;
      sprint_multiplier: number;
      rise_speed: number;
    };
    sphere: {
      radius: number;
      position: Vector3Tuple;
      bob_amplitude: number;
      bob_speed: number;
      rotation_speed: number;
      surface_color: string;
      emissive_color: string;
      halo_color: string;
    };
    space: {
      background: string;
      fog: string;
      grid_size: number;
      grid_divisions: number;
      star_count: number;
      star_field_radius: number;
      beacon_count: number;
      beacon_ring_radius: number;
      beacon_height: number;
    };
    lights: {
      ambient_intensity: number;
      hemisphere_intensity: number;
      key_intensity: number;
      rim_intensity: number;
    };
  };
};

type MovementState = {
  forward: boolean;
  backward: boolean;
  left: boolean;
  right: boolean;
  rise: boolean;
  descend: boolean;
  sprint: boolean;
};

function asObject(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`Expected ${label} to be an object.`);
  }
  return value as Record<string, unknown>;
}

function asString(value: unknown, label: string): string {
  if (typeof value !== "string") {
    throw new Error(`Expected ${label} to be a string.`);
  }
  return value;
}

function asNumber(value: unknown, label: string): number {
  if (typeof value !== "number" || Number.isNaN(value)) {
    throw new Error(`Expected ${label} to be a number.`);
  }
  return value;
}

function asVector3Tuple(value: unknown, label: string): Vector3Tuple {
  if (!Array.isArray(value) || value.length !== 3) {
    throw new Error(`Expected ${label} to be a 3-number tuple.`);
  }
  return [
    asNumber(value[0], `${label}[0]`),
    asNumber(value[1], `${label}[1]`),
    asNumber(value[2], `${label}[2]`),
  ];
}

function readModel(rawValue: unknown): AppModel {
  const root = asObject(rawValue, "window.__KAIN_THREE_SPACE_MODEL__");
  const scene = asObject(root.scene, "scene");
  const camera = asObject(scene.camera, "scene.camera");
  const sphere = asObject(scene.sphere, "scene.sphere");
  const space = asObject(scene.space, "scene.space");
  const lights = asObject(scene.lights, "scene.lights");

  return {
    name: asString(root.name, "name"),
    tagline: asString(root.tagline, "tagline"),
    summary: asString(root.summary, "summary"),
    scene: {
      name: asString(scene.name, "scene.name"),
      summary: asString(scene.summary, "scene.summary"),
      camera: {
        fov: asNumber(camera.fov, "scene.camera.fov"),
        near: asNumber(camera.near, "scene.camera.near"),
        far: asNumber(camera.far, "scene.camera.far"),
        spawn: asVector3Tuple(camera.spawn, "scene.camera.spawn"),
        walk_speed: asNumber(camera.walk_speed, "scene.camera.walk_speed"),
        sprint_multiplier: asNumber(camera.sprint_multiplier, "scene.camera.sprint_multiplier"),
        rise_speed: asNumber(camera.rise_speed, "scene.camera.rise_speed"),
      },
      sphere: {
        radius: asNumber(sphere.radius, "scene.sphere.radius"),
        position: asVector3Tuple(sphere.position, "scene.sphere.position"),
        bob_amplitude: asNumber(sphere.bob_amplitude, "scene.sphere.bob_amplitude"),
        bob_speed: asNumber(sphere.bob_speed, "scene.sphere.bob_speed"),
        rotation_speed: asNumber(sphere.rotation_speed, "scene.sphere.rotation_speed"),
        surface_color: asString(sphere.surface_color, "scene.sphere.surface_color"),
        emissive_color: asString(sphere.emissive_color, "scene.sphere.emissive_color"),
        halo_color: asString(sphere.halo_color, "scene.sphere.halo_color"),
      },
      space: {
        background: asString(space.background, "scene.space.background"),
        fog: asString(space.fog, "scene.space.fog"),
        grid_size: asNumber(space.grid_size, "scene.space.grid_size"),
        grid_divisions: asNumber(space.grid_divisions, "scene.space.grid_divisions"),
        star_count: asNumber(space.star_count, "scene.space.star_count"),
        star_field_radius: asNumber(space.star_field_radius, "scene.space.star_field_radius"),
        beacon_count: asNumber(space.beacon_count, "scene.space.beacon_count"),
        beacon_ring_radius: asNumber(space.beacon_ring_radius, "scene.space.beacon_ring_radius"),
        beacon_height: asNumber(space.beacon_height, "scene.space.beacon_height"),
      },
      lights: {
        ambient_intensity: asNumber(lights.ambient_intensity, "scene.lights.ambient_intensity"),
        hemisphere_intensity: asNumber(lights.hemisphere_intensity, "scene.lights.hemisphere_intensity"),
        key_intensity: asNumber(lights.key_intensity, "scene.lights.key_intensity"),
        rim_intensity: asNumber(lights.rim_intensity, "scene.lights.rim_intensity"),
      },
    },
  };
}

function formatVector(vector: THREE.Vector3): string {
  return `${vector.x.toFixed(1)}, ${vector.y.toFixed(1)}, ${vector.z.toFixed(1)}`;
}

function createStarField(scene: THREE.Scene, starCount: number, radius: number) {
  const positions = new Float32Array(starCount * 3);

  for (let index = 0; index < starCount; index += 1) {
    const angle = Math.random() * Math.PI * 2;
    const polar = Math.acos(THREE.MathUtils.randFloatSpread(2));
    const distance = THREE.MathUtils.randFloat(radius * 0.35, radius);
    const offset = index * 3;
    positions[offset] = Math.sin(polar) * Math.cos(angle) * distance;
    positions[offset + 1] = Math.cos(polar) * distance;
    positions[offset + 2] = Math.sin(polar) * Math.sin(angle) * distance;
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
  const material = new THREE.PointsMaterial({
    color: "#b8d8ff",
    size: 1.35,
    sizeAttenuation: true,
  });

  const points = new THREE.Points(geometry, material);
  scene.add(points);
  return points;
}

function createBeaconRing(scene: THREE.Scene, model: AppModel) {
  const beaconGroup = new THREE.Group();
  const beaconGeometry = new THREE.BoxGeometry(3, model.scene.space.beacon_height, 3);
  const beaconMaterial = new THREE.MeshStandardMaterial({
    color: "#3f7cff",
    emissive: "#123f92",
    metalness: 0.25,
    roughness: 0.32,
  });

  for (let index = 0; index < model.scene.space.beacon_count; index += 1) {
    const fraction = index / model.scene.space.beacon_count;
    const angle = fraction * Math.PI * 2;
    const beacon = new THREE.Mesh(beaconGeometry, beaconMaterial);
    beacon.position.set(
      Math.cos(angle) * model.scene.space.beacon_ring_radius,
      model.scene.space.beacon_height / 2 - 2,
      Math.sin(angle) * model.scene.space.beacon_ring_radius,
    );
    beacon.rotation.y = angle;
    beaconGroup.add(beacon);
  }

  scene.add(beaconGroup);
  return beaconGroup;
}

function createSphereAssembly(scene: THREE.Scene, model: AppModel) {
  const sphereGroup = new THREE.Group();
  const [sphereX, sphereY, sphereZ] = model.scene.sphere.position;
  sphereGroup.position.set(sphereX, sphereY, sphereZ);

  const sphereCore = new THREE.Mesh(
    new THREE.SphereGeometry(model.scene.sphere.radius, 48, 48),
    new THREE.MeshStandardMaterial({
      color: model.scene.sphere.surface_color,
      emissive: model.scene.sphere.emissive_color,
      emissiveIntensity: 0.8,
      metalness: 0.14,
      roughness: 0.2,
    }),
  );

  const sphereHalo = new THREE.Mesh(
    new THREE.SphereGeometry(model.scene.sphere.radius * 1.22, 36, 36),
    new THREE.MeshBasicMaterial({
      color: model.scene.sphere.halo_color,
      transparent: true,
      opacity: 0.11,
      wireframe: true,
    }),
  );

  const accentLight = new THREE.PointLight(model.scene.sphere.halo_color, 22, 110, 2);
  accentLight.position.set(0, 0, 0);

  sphereGroup.add(sphereCore);
  sphereGroup.add(sphereHalo);
  sphereGroup.add(accentLight);
  scene.add(sphereGroup);

  return { sphereGroup, sphereCore, sphereHalo };
}

function createShellMarkup(model: AppModel) {
  return `
    <div class="space-shell">
      <div class="space-stage" data-space-stage></div>
      <div class="hud-shell">
        <section class="hero-card">
          <span class="eyebrow">Kain Node FFI + Three.js</span>
          <h1>${model.name}</h1>
          <p>${model.summary}</p>
        </section>
        <section class="status-card">
          <div class="status-row"><span>Scene</span><strong>${model.scene.name}</strong></div>
          <div class="status-row"><span>Controls</span><strong>WASD + mouse + Space/Ctrl</strong></div>
          <div class="status-row"><span>Lock</span><button type="button" class="lock-button" data-lock-button>Enter Flight</button></div>
        </section>
        <section class="status-card">
          <div class="status-row"><span>Camera</span><strong data-camera-position>0, 0, 0</strong></div>
          <div class="status-row"><span>Sphere Range</span><strong data-sphere-range>0.0</strong></div>
          <div class="status-row"><span>Status</span><strong data-lock-state>Mouse unlocked</strong></div>
        </section>
        <section class="hint-card">
          <p>Click <strong>Enter Flight</strong>, then drag to look around. Hold <strong>Shift</strong> to sprint. Press <strong>Esc</strong> to release the pointer.</p>
        </section>
      </div>
      <div class="crosshair" data-crosshair></div>
    </div>
  `;
}

function installApp(model: AppModel) {
  const mountTarget = document.getElementById("app-root");
  if (!mountTarget) {
    throw new Error("Missing app root.");
  }

  mountTarget.innerHTML = createShellMarkup(model);

  const stage = mountTarget.querySelector<HTMLElement>("[data-space-stage]");
  const lockButton = mountTarget.querySelector<HTMLButtonElement>("[data-lock-button]");
  const cameraPositionValue = mountTarget.querySelector<HTMLElement>("[data-camera-position]");
  const sphereRangeValue = mountTarget.querySelector<HTMLElement>("[data-sphere-range]");
  const lockStateValue = mountTarget.querySelector<HTMLElement>("[data-lock-state]");
  const crosshair = mountTarget.querySelector<HTMLElement>("[data-crosshair]");

  if (!stage || !lockButton || !cameraPositionValue || !sphereRangeValue || !lockStateValue || !crosshair) {
    throw new Error("Missing required shell elements.");
  }

  document.title = model.name;

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  stage.appendChild(renderer.domElement);

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(model.scene.space.background);
  scene.fog = new THREE.Fog(model.scene.space.fog, 80, 900);

  const camera = new THREE.PerspectiveCamera(
    model.scene.camera.fov,
    1,
    model.scene.camera.near,
    model.scene.camera.far,
  );
  camera.position.set(...model.scene.camera.spawn);

  const controls = new PointerLockControls(camera, renderer.domElement);
  scene.add(controls.object);

  const ambientLight = new THREE.AmbientLight("#ffffff", model.scene.lights.ambient_intensity);
  const hemisphereLight = new THREE.HemisphereLight("#98d6ff", "#06111f", model.scene.lights.hemisphere_intensity);
  const keyLight = new THREE.DirectionalLight("#ffffff", model.scene.lights.key_intensity);
  keyLight.position.set(20, 38, 12);
  const rimLight = new THREE.DirectionalLight("#7bc1ff", model.scene.lights.rim_intensity);
  rimLight.position.set(-26, 14, -18);

  scene.add(ambientLight);
  scene.add(hemisphereLight);
  scene.add(keyLight);
  scene.add(rimLight);

  const grid = new THREE.GridHelper(
    model.scene.space.grid_size,
    model.scene.space.grid_divisions,
    "#5ea5ff",
    "#18314f",
  );
  grid.position.y = -2;
  scene.add(grid);

  const floorPlane = new THREE.Mesh(
    new THREE.CircleGeometry(model.scene.space.grid_size * 0.5, 96),
    new THREE.MeshBasicMaterial({
      color: "#071120",
      transparent: true,
      opacity: 0.38,
      side: THREE.DoubleSide,
    }),
  );
  floorPlane.rotation.x = -Math.PI / 2;
  floorPlane.position.y = -1.98;
  scene.add(floorPlane);

  const starField = createStarField(scene, model.scene.space.star_count, model.scene.space.star_field_radius);
  const beaconRing = createBeaconRing(scene, model);
  const { sphereGroup, sphereCore, sphereHalo } = createSphereAssembly(scene, model);

  const clock = new THREE.Clock();
  const movementState: MovementState = {
    forward: false,
    backward: false,
    left: false,
    right: false,
    rise: false,
    descend: false,
    sprint: false,
  };

  const onResize = () => {
    const width = Math.max(stage.clientWidth, 1);
    const height = Math.max(stage.clientHeight, 1);
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
  };

  onResize();
  window.addEventListener("resize", onResize);

  const setMovementFlag = (event: KeyboardEvent, isPressed: boolean) => {
    if (event.code === "KeyW") movementState.forward = isPressed;
    if (event.code === "KeyS") movementState.backward = isPressed;
    if (event.code === "KeyA") movementState.left = isPressed;
    if (event.code === "KeyD") movementState.right = isPressed;
    if (event.code === "Space") movementState.rise = isPressed;
    if (event.code === "ControlLeft" || event.code === "ControlRight") movementState.descend = isPressed;
    if (event.code === "ShiftLeft" || event.code === "ShiftRight") movementState.sprint = isPressed;
  };

  const onKeyDown = (event: KeyboardEvent) => setMovementFlag(event, true);
  const onKeyUp = (event: KeyboardEvent) => setMovementFlag(event, false);
  window.addEventListener("keydown", onKeyDown);
  window.addEventListener("keyup", onKeyUp);

  lockButton.addEventListener("click", () => controls.lock());
  controls.addEventListener("lock", () => {
    lockButton.textContent = "Flight Active";
    lockStateValue.textContent = "Mouse locked";
    crosshair.classList.add("active");
  });
  controls.addEventListener("unlock", () => {
    lockButton.textContent = "Enter Flight";
    lockStateValue.textContent = "Mouse unlocked";
    crosshair.classList.remove("active");
  });

  let disposed = false;

  const renderFrame = () => {
    if (disposed) {
      return;
    }

    requestAnimationFrame(renderFrame);
    const elapsedTime = clock.getElapsedTime();
    const deltaSeconds = Math.min(clock.getDelta(), 0.05);

    const speedMultiplier = movementState.sprint ? model.scene.camera.sprint_multiplier : 1;
    const planarSpeed = model.scene.camera.walk_speed * speedMultiplier * deltaSeconds;
    const verticalSpeed = model.scene.camera.rise_speed * speedMultiplier * deltaSeconds;

    if (controls.isLocked) {
      if (movementState.forward) controls.moveForward(planarSpeed);
      if (movementState.backward) controls.moveForward(-planarSpeed);
      if (movementState.left) controls.moveRight(-planarSpeed);
      if (movementState.right) controls.moveRight(planarSpeed);
      if (movementState.rise) camera.position.y += verticalSpeed;
      if (movementState.descend) camera.position.y -= verticalSpeed;
    }

    sphereGroup.position.y =
      model.scene.sphere.position[1] +
      Math.sin(elapsedTime * model.scene.sphere.bob_speed) * model.scene.sphere.bob_amplitude;
    sphereCore.rotation.y += model.scene.sphere.rotation_speed * deltaSeconds;
    sphereHalo.rotation.y -= model.scene.sphere.rotation_speed * 0.8 * deltaSeconds;
    sphereHalo.rotation.x += model.scene.sphere.rotation_speed * 0.35 * deltaSeconds;
    beaconRing.rotation.y += 0.03 * deltaSeconds;
    starField.rotation.y += 0.002 * deltaSeconds;

    cameraPositionValue.textContent = formatVector(camera.position);
    sphereRangeValue.textContent = camera.position.distanceTo(sphereGroup.position).toFixed(1);

    renderer.render(scene, camera);
  };

  renderFrame();

  return () => {
    disposed = true;
    window.removeEventListener("resize", onResize);
    window.removeEventListener("keydown", onKeyDown);
    window.removeEventListener("keyup", onKeyUp);
    controls.unlock();
    renderer.dispose();
    grid.geometry.dispose();
    (grid.material as THREE.Material | THREE.Material[]).dispose?.();
    floorPlane.geometry.dispose();
    (floorPlane.material as THREE.Material).dispose();
    starField.geometry.dispose();
    (starField.material as THREE.Material).dispose();
    stage.innerHTML = "";
  };
}

const model = readModel(window.__KAIN_THREE_SPACE_MODEL__);
installApp(model);
