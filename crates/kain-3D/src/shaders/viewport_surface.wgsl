
struct SceneUniforms {
    view_proj: mat4x4<f32>,
    camera_position: vec4<f32>,
    ambient_color_intensity: vec4<f32>,
    directional_directions: array<vec4<f32>, 2>,
    directional_colors: array<vec4<f32>, 2>,
    point_positions_intensity: array<vec4<f32>, 4>,
    point_colors_range: array<vec4<f32>, 4>,
    counts: vec4<u32>,
    background_top: vec4<f32>,
    background_bottom: vec4<f32>,
};

@group(0) @binding(0)
var<uniform> scene: SceneUniforms;

struct SceneVertexInput {
    @location(0) position_and_ambient: vec4<f32>,
    @location(1) normal_and_diffuse: vec4<f32>,
    @location(2) base_color_and_specular_strength: vec4<f32>,
    @location(3) specular_color_and_shininess: vec4<f32>,
};

struct SceneVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) base_color: vec3<f32>,
    @location(3) ambient_strength: f32,
    @location(4) diffuse_strength: f32,
    @location(5) specular_color: vec3<f32>,
    @location(6) specular_strength: f32,
    @location(7) shininess: f32,
};

struct BackgroundVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) gradient_t: f32,
};

struct PickVertexInput {
    @location(0) world_position: vec4<f32>,
    @location(1) instance_id: u32,
};

struct PickVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) @interpolate(flat) instance_id: u32,
};

struct GizmoVertexInput {
    @location(0) world_position: vec4<f32>,
    @location(1) color: vec4<f32>,
};

struct ParticleVertexInput {
    @location(0) world_position: vec4<f32>,
    @location(1) color: vec4<f32>,
    @location(2) quad_uv: vec2<f32>,
};

struct GizmoVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

struct ParticleVertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) quad_uv: vec2<f32>,
};

@vertex
fn scene_vs_main(input: SceneVertexInput) -> SceneVertexOutput {
    var output: SceneVertexOutput;
    let world_position = vec4<f32>(input.position_and_ambient.xyz, 1.0);
    output.clip_position = scene.view_proj * world_position;
    output.world_position = input.position_and_ambient.xyz;
    output.world_normal = normalize(input.normal_and_diffuse.xyz);
    output.base_color = input.base_color_and_specular_strength.xyz;
    output.ambient_strength = input.position_and_ambient.w;
    output.diffuse_strength = input.normal_and_diffuse.w;
    output.specular_color = input.specular_color_and_shininess.xyz;
    output.specular_strength = input.base_color_and_specular_strength.w;
    output.shininess = input.specular_color_and_shininess.w;
    return output;
}

@vertex
fn background_vs_main(@builtin(vertex_index) vertex_index: u32) -> BackgroundVertexOutput {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>(3.0, 1.0),
        vec2<f32>(-1.0, 1.0),
    );

    let position = positions[vertex_index];
    var output: BackgroundVertexOutput;
    output.clip_position = vec4<f32>(position, 0.0, 1.0);
    output.gradient_t = clamp(1.0 - ((position.y * 0.5) + 0.5), 0.0, 1.0);
    return output;
}

fn directional_light(
    light_direction: vec3<f32>,
    light_color: vec3<f32>,
    intensity: f32,
    normal: vec3<f32>,
    view_direction: vec3<f32>,
    base_color: vec3<f32>,
    diffuse_strength: f32,
    specular_color: vec3<f32>,
    specular_strength: f32,
    shininess: f32,
) -> vec3<f32> {
    let to_light = normalize(-light_direction);
    let diffuse = max(dot(normal, to_light), 0.0) * diffuse_strength;
    let halfway = normalize(to_light + view_direction);
    let specular = pow(max(dot(normal, halfway), 0.0), max(shininess, 1.0)) * specular_strength;
    return ((base_color * diffuse) + (specular_color * specular)) * light_color * intensity;
}

fn point_light(
    light_position: vec3<f32>,
    light_color: vec3<f32>,
    intensity: f32,
    range: f32,
    world_position: vec3<f32>,
    normal: vec3<f32>,
    view_direction: vec3<f32>,
    base_color: vec3<f32>,
    diffuse_strength: f32,
    specular_color: vec3<f32>,
    specular_strength: f32,
    shininess: f32,
) -> vec3<f32> {
    let to_light = light_position - world_position;
    let distance = max(length(to_light), 0.0001);
    let direction = to_light / distance;
    let attenuation = pow(max(1.0 - (distance / max(range, 0.001)), 0.0), 2.0);
    let diffuse = max(dot(normal, direction), 0.0) * diffuse_strength;
    let reflected = normalize((normal * (2.0 * dot(normal, direction))) - direction);
    let specular = pow(max(dot(reflected, view_direction), 0.0), max(shininess, 1.0)) * specular_strength;
    return ((base_color * diffuse) + (specular_color * specular)) * light_color * intensity * attenuation;
}

@fragment
fn scene_fs_main(input: SceneVertexOutput) -> @location(0) vec4<f32> {
    let normal = normalize(input.world_normal);
    let view_direction = normalize(scene.camera_position.xyz - input.world_position);
    var color =
        scene.ambient_color_intensity.xyz *
        scene.ambient_color_intensity.w *
        input.base_color *
        input.ambient_strength;

    for (var index: u32 = 0u; index < scene.counts.x; index = index + 1u) {
        let light_direction = scene.directional_directions[index];
        let light_color = scene.directional_colors[index];
        color += directional_light(
            light_direction.xyz,
            light_color.xyz,
            light_direction.w,
            normal,
            view_direction,
            input.base_color,
            input.diffuse_strength,
            input.specular_color,
            input.specular_strength,
            input.shininess,
        );
    }

    for (var index: u32 = 0u; index < scene.counts.y; index = index + 1u) {
        let light_position = scene.point_positions_intensity[index];
        let light_color = scene.point_colors_range[index];
        color += point_light(
            light_position.xyz,
            light_color.xyz,
            light_position.w,
            light_color.w,
            input.world_position,
            normal,
            view_direction,
            input.base_color,
            input.diffuse_strength,
            input.specular_color,
            input.specular_strength,
            input.shininess,
        );
    }

    return vec4<f32>(clamp(color, vec3<f32>(0.0), vec3<f32>(1.0)), 1.0);
}

@fragment
fn background_fs_main(input: BackgroundVertexOutput) -> @location(0) vec4<f32> {
    let t = clamp(input.gradient_t, 0.0, 1.0);
    let color = scene.background_top.xyz * (1.0 - t) + scene.background_bottom.xyz * t;
    return vec4<f32>(color, 1.0);
}

@vertex
fn pick_vs_main(input: PickVertexInput) -> PickVertexOutput {
    var output: PickVertexOutput;
    output.clip_position = scene.view_proj * vec4<f32>(input.world_position.xyz, 1.0);
    output.instance_id = input.instance_id;
    return output;
}

@fragment
fn pick_fs_main(input: PickVertexOutput) -> @location(0) u32 {
    return input.instance_id;
}

@vertex
fn gizmo_vs_main(input: GizmoVertexInput) -> GizmoVertexOutput {
    var output: GizmoVertexOutput;
    output.clip_position = scene.view_proj * vec4<f32>(input.world_position.xyz, 1.0);
    output.color = input.color;
    return output;
}

@fragment
fn gizmo_fs_main(input: GizmoVertexOutput) -> @location(0) vec4<f32> {
    return input.color;
}

@vertex
fn particle_vs_main(input: ParticleVertexInput) -> ParticleVertexOutput {
    var output: ParticleVertexOutput;
    output.clip_position = scene.view_proj * vec4<f32>(input.world_position.xyz, 1.0);
    output.color = input.color;
    output.quad_uv = input.quad_uv;
    return output;
}

@fragment
fn particle_fs_main(input: ParticleVertexOutput) -> @location(0) vec4<f32> {
    let radial = dot(input.quad_uv, input.quad_uv);
    if (radial > 1.0) {
        discard;
    }
    let glow = pow(max(1.0 - radial, 0.0), 1.75) * input.color.a;
    return vec4<f32>(input.color.rgb * glow, glow);
}
