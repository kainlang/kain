declare global {
  interface Window {
    __KAIN_THREE_SPACE_MODEL__?: unknown;
  }
}

export type Vector3Tuple = [number, number, number];

export type ViewportBehavior = "orbit" | "fly";

export type SceneConfig = {
  id: string;
  name: string;
  summary: string;
  camera: {
    fov: number;
    near: number;
    far: number;
    spawn: Vector3Tuple;
    fly_walk_speed: number;
    fly_sprint_multiplier: number;
    fly_rise_speed: number;
    orbit_target: Vector3Tuple;
    orbit_distance: number;
    orbit_min_distance: number;
    orbit_max_distance: number;
  };
  hero_orb: {
    radius: number;
    detail: number;
    position: Vector3Tuple;
    float_amplitude: number;
    float_speed: number;
    surface_color: string;
    emissive_color: string;
    halo_color: string;
    wire_color: string;
    halo_scale: number;
    rotation_speed: number;
  };
  environment: {
    background: string;
    fog: string;
    grid_size: number;
    grid_divisions: number;
    star_count: number;
    star_field_radius: number;
    beacon_count: number;
    beacon_ring_radius: number;
    beacon_height: number;
    platform_radius: number;
    platform_color: string;
  };
  lights: {
    ambient_intensity: number;
    hemisphere_intensity: number;
    key_intensity: number;
    rim_intensity: number;
  };
};

export type SculptToolConfig = {
  id: string;
  label: string;
  description: string;
  operation_code: number;
  default_radius: number;
  default_strength: number;
  falloff_power: number;
  accent_color: string;
};

export type SculptSuiteConfig = {
  id: string;
  name: string;
  summary: string;
  defaults: {
    initial_tool: string;
    brush_radius: number;
    brush_strength: number;
    falloff_power: number;
  };
  tools: SculptToolConfig[];
};

export type ViewportModeConfig = {
  id: string;
  label: string;
  description: string;
  camera_behavior: ViewportBehavior;
  allow_sculpt: boolean;
  allow_pointer_lock: boolean;
  show_crosshair: boolean;
  hotkey: string;
};

export type ViewportProfilesConfig = {
  id: string;
  name: string;
  summary: string;
  primary_mode: string;
  modes: ViewportModeConfig[];
};

export type WasmPipelineConfig = {
  id: string;
  name: string;
  crate_name: string;
  public_path: string;
  target: string;
};

export type HybridPipelineConfig = {
  id: string;
  name: string;
  bundle_name: string;
  public_descriptor_path: string;
  public_js_path: string;
  public_wasm_path: string;
  float_cycle_units: number;
  float_amplitude_units: number;
  rotation_cycle_units: number;
  beacon_spin_speed_units: number;
  star_drift_speed_units: number;
};

export type AppModel = {
  name: string;
  tagline: string;
  summary: string;
  scene: SceneConfig;
  sculpt_suite: SculptSuiteConfig;
  viewport_profiles: ViewportProfilesConfig;
  hybrid_pipeline: HybridPipelineConfig;
  wasm_pipeline: WasmPipelineConfig;
};

function asObject(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`Expected ${label} to be an object.`);
  }

  return value as Record<string, unknown>;
}

function asArray(value: unknown, label: string): unknown[] {
  if (!Array.isArray(value)) {
    throw new Error(`Expected ${label} to be an array.`);
  }

  return value;
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

function asBoolean(value: unknown, label: string): boolean {
  if (typeof value !== "boolean") {
    throw new Error(`Expected ${label} to be a boolean.`);
  }

  return value;
}

function asVector3Tuple(value: unknown, label: string): Vector3Tuple {
  if (!Array.isArray(value) || value.length !== 3) {
    throw new Error(`Expected ${label} to be a three-number tuple.`);
  }

  return [
    asNumber(value[0], `${label}[0]`),
    asNumber(value[1], `${label}[1]`),
    asNumber(value[2], `${label}[2]`),
  ];
}

function readSceneConfig(value: unknown): SceneConfig {
  const scene = asObject(value, "scene");
  const camera = asObject(scene.camera, "scene.camera");
  const heroOrb = asObject(scene.hero_orb, "scene.hero_orb");
  const environment = asObject(scene.environment, "scene.environment");
  const lights = asObject(scene.lights, "scene.lights");

  return {
    id: asString(scene.id, "scene.id"),
    name: asString(scene.name, "scene.name"),
    summary: asString(scene.summary, "scene.summary"),
    camera: {
      fov: asNumber(camera.fov, "scene.camera.fov"),
      near: asNumber(camera.near, "scene.camera.near"),
      far: asNumber(camera.far, "scene.camera.far"),
      spawn: asVector3Tuple(camera.spawn, "scene.camera.spawn"),
      fly_walk_speed: asNumber(camera.fly_walk_speed, "scene.camera.fly_walk_speed"),
      fly_sprint_multiplier: asNumber(
        camera.fly_sprint_multiplier,
        "scene.camera.fly_sprint_multiplier",
      ),
      fly_rise_speed: asNumber(camera.fly_rise_speed, "scene.camera.fly_rise_speed"),
      orbit_target: asVector3Tuple(camera.orbit_target, "scene.camera.orbit_target"),
      orbit_distance: asNumber(camera.orbit_distance, "scene.camera.orbit_distance"),
      orbit_min_distance: asNumber(camera.orbit_min_distance, "scene.camera.orbit_min_distance"),
      orbit_max_distance: asNumber(camera.orbit_max_distance, "scene.camera.orbit_max_distance"),
    },
    hero_orb: {
      radius: asNumber(heroOrb.radius, "scene.hero_orb.radius"),
      detail: asNumber(heroOrb.detail, "scene.hero_orb.detail"),
      position: asVector3Tuple(heroOrb.position, "scene.hero_orb.position"),
      float_amplitude: asNumber(heroOrb.float_amplitude, "scene.hero_orb.float_amplitude"),
      float_speed: asNumber(heroOrb.float_speed, "scene.hero_orb.float_speed"),
      surface_color: asString(heroOrb.surface_color, "scene.hero_orb.surface_color"),
      emissive_color: asString(heroOrb.emissive_color, "scene.hero_orb.emissive_color"),
      halo_color: asString(heroOrb.halo_color, "scene.hero_orb.halo_color"),
      wire_color: asString(heroOrb.wire_color, "scene.hero_orb.wire_color"),
      halo_scale: asNumber(heroOrb.halo_scale, "scene.hero_orb.halo_scale"),
      rotation_speed: asNumber(heroOrb.rotation_speed, "scene.hero_orb.rotation_speed"),
    },
    environment: {
      background: asString(environment.background, "scene.environment.background"),
      fog: asString(environment.fog, "scene.environment.fog"),
      grid_size: asNumber(environment.grid_size, "scene.environment.grid_size"),
      grid_divisions: asNumber(environment.grid_divisions, "scene.environment.grid_divisions"),
      star_count: asNumber(environment.star_count, "scene.environment.star_count"),
      star_field_radius: asNumber(
        environment.star_field_radius,
        "scene.environment.star_field_radius",
      ),
      beacon_count: asNumber(environment.beacon_count, "scene.environment.beacon_count"),
      beacon_ring_radius: asNumber(
        environment.beacon_ring_radius,
        "scene.environment.beacon_ring_radius",
      ),
      beacon_height: asNumber(environment.beacon_height, "scene.environment.beacon_height"),
      platform_radius: asNumber(environment.platform_radius, "scene.environment.platform_radius"),
      platform_color: asString(environment.platform_color, "scene.environment.platform_color"),
    },
    lights: {
      ambient_intensity: asNumber(lights.ambient_intensity, "scene.lights.ambient_intensity"),
      hemisphere_intensity: asNumber(
        lights.hemisphere_intensity,
        "scene.lights.hemisphere_intensity",
      ),
      key_intensity: asNumber(lights.key_intensity, "scene.lights.key_intensity"),
      rim_intensity: asNumber(lights.rim_intensity, "scene.lights.rim_intensity"),
    },
  };
}

function readSculptTool(value: unknown, label: string): SculptToolConfig {
  const tool = asObject(value, label);

  return {
    id: asString(tool.id, `${label}.id`),
    label: asString(tool.label, `${label}.label`),
    description: asString(tool.description, `${label}.description`),
    operation_code: asNumber(tool.operation_code, `${label}.operation_code`),
    default_radius: asNumber(tool.default_radius, `${label}.default_radius`),
    default_strength: asNumber(tool.default_strength, `${label}.default_strength`),
    falloff_power: asNumber(tool.falloff_power, `${label}.falloff_power`),
    accent_color: asString(tool.accent_color, `${label}.accent_color`),
  };
}

function readSculptSuite(value: unknown): SculptSuiteConfig {
  const suite = asObject(value, "sculpt_suite");
  const defaults = asObject(suite.defaults, "sculpt_suite.defaults");
  const tools = asArray(suite.tools, "sculpt_suite.tools");

  return {
    id: asString(suite.id, "sculpt_suite.id"),
    name: asString(suite.name, "sculpt_suite.name"),
    summary: asString(suite.summary, "sculpt_suite.summary"),
    defaults: {
      initial_tool: asString(defaults.initial_tool, "sculpt_suite.defaults.initial_tool"),
      brush_radius: asNumber(defaults.brush_radius, "sculpt_suite.defaults.brush_radius"),
      brush_strength: asNumber(defaults.brush_strength, "sculpt_suite.defaults.brush_strength"),
      falloff_power: asNumber(defaults.falloff_power, "sculpt_suite.defaults.falloff_power"),
    },
    tools: tools.map((entry, index) => readSculptTool(entry, `sculpt_suite.tools[${index}]`)),
  };
}

function readViewportMode(value: unknown, label: string): ViewportModeConfig {
  const mode = asObject(value, label);
  const behavior = asString(mode.camera_behavior, `${label}.camera_behavior`);

  if (behavior !== "orbit" && behavior !== "fly") {
    throw new Error(`Expected ${label}.camera_behavior to be "orbit" or "fly".`);
  }

  return {
    id: asString(mode.id, `${label}.id`),
    label: asString(mode.label, `${label}.label`),
    description: asString(mode.description, `${label}.description`),
    camera_behavior: behavior,
    allow_sculpt: asBoolean(mode.allow_sculpt, `${label}.allow_sculpt`),
    allow_pointer_lock: asBoolean(mode.allow_pointer_lock, `${label}.allow_pointer_lock`),
    show_crosshair: asBoolean(mode.show_crosshair, `${label}.show_crosshair`),
    hotkey: asString(mode.hotkey, `${label}.hotkey`),
  };
}

function readViewportProfiles(value: unknown): ViewportProfilesConfig {
  const profiles = asObject(value, "viewport_profiles");
  const modes = asArray(profiles.modes, "viewport_profiles.modes");

  return {
    id: asString(profiles.id, "viewport_profiles.id"),
    name: asString(profiles.name, "viewport_profiles.name"),
    summary: asString(profiles.summary, "viewport_profiles.summary"),
    primary_mode: asString(profiles.primary_mode, "viewport_profiles.primary_mode"),
    modes: modes.map((entry, index) =>
      readViewportMode(entry, `viewport_profiles.modes[${index}]`),
    ),
  };
}

function readWasmPipeline(value: unknown): WasmPipelineConfig {
  const pipeline = asObject(value, "wasm_pipeline");

  return {
    id: asString(pipeline.id, "wasm_pipeline.id"),
    name: asString(pipeline.name, "wasm_pipeline.name"),
    crate_name: asString(pipeline.crate_name, "wasm_pipeline.crate_name"),
    public_path: asString(pipeline.public_path, "wasm_pipeline.public_path"),
    target: asString(pipeline.target, "wasm_pipeline.target"),
  };
}

function readHybridPipeline(value: unknown): HybridPipelineConfig {
  const pipeline = asObject(value, "hybrid_pipeline");

  return {
    id: asString(pipeline.id, "hybrid_pipeline.id"),
    name: asString(pipeline.name, "hybrid_pipeline.name"),
    bundle_name: asString(pipeline.bundle_name, "hybrid_pipeline.bundle_name"),
    public_descriptor_path: asString(
      pipeline.public_descriptor_path,
      "hybrid_pipeline.public_descriptor_path",
    ),
    public_js_path: asString(pipeline.public_js_path, "hybrid_pipeline.public_js_path"),
    public_wasm_path: asString(pipeline.public_wasm_path, "hybrid_pipeline.public_wasm_path"),
    float_cycle_units: asNumber(
      pipeline.float_cycle_units,
      "hybrid_pipeline.float_cycle_units",
    ),
    float_amplitude_units: asNumber(
      pipeline.float_amplitude_units,
      "hybrid_pipeline.float_amplitude_units",
    ),
    rotation_cycle_units: asNumber(
      pipeline.rotation_cycle_units,
      "hybrid_pipeline.rotation_cycle_units",
    ),
    beacon_spin_speed_units: asNumber(
      pipeline.beacon_spin_speed_units,
      "hybrid_pipeline.beacon_spin_speed_units",
    ),
    star_drift_speed_units: asNumber(
      pipeline.star_drift_speed_units,
      "hybrid_pipeline.star_drift_speed_units",
    ),
  };
}

export function readAppModel(rawValue: unknown): AppModel {
  const root = asObject(rawValue, "window.__KAIN_THREE_SPACE_MODEL__");

  return {
    name: asString(root.name, "name"),
    tagline: asString(root.tagline, "tagline"),
    summary: asString(root.summary, "summary"),
    scene: readSceneConfig(root.scene),
    sculpt_suite: readSculptSuite(root.sculpt_suite),
    viewport_profiles: readViewportProfiles(root.viewport_profiles),
    hybrid_pipeline: readHybridPipeline(root.hybrid_pipeline),
    wasm_pipeline: readWasmPipeline(root.wasm_pipeline),
  };
}
