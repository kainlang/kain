#version 450

layout(push_constant) uniform ZenderPush {
    float time_seconds;
    float particle_count;
    float orbit_speed;
    float chaos;
    int mode;
    int sphere_instances;
    int ring_resolution;
    int shell_resolution;
} pc;

layout(location = 0) out vec4 particle_color;

float hash11(float value) {
    return fract(sin(value * 91.173 + 17.17) * 43758.5453123);
}

void main() {
    float index = float(gl_VertexIndex);
    float sphere_count = max(float(pc.sphere_instances), 1.0);
    float rings = max(float(pc.ring_resolution), 8.0);
    float shells = max(float(pc.shell_resolution), 8.0);
    float particles_per_sphere = max(rings * shells, 1.0);
    float sphere_index = floor(index / particles_per_sphere);
    float local_index = mod(index, particles_per_sphere);
    float ring_index = mod(local_index, rings);
    float shell_index = floor(local_index / rings);
    float sphere_ratio = sphere_index / sphere_count;
    float phi = (ring_index / rings) * 6.283185307179586;
    float shell_ratio = (shell_index + 0.5) / shells;
    float theta = acos(clamp(1.0 - 2.0 * shell_ratio, -1.0, 1.0));
    float shell_radius = 0.05 + 0.015 * sin(pc.time_seconds * 0.7 + sphere_ratio * 6.283185307179586);

    vec3 local = vec3(
        cos(phi) * sin(theta),
        cos(theta),
        sin(phi) * sin(theta)
    ) * shell_radius;

    float orbit_angle = sphere_ratio * 6.283185307179586 + pc.time_seconds * pc.orbit_speed;
    float orbit_radius = 0.26 + float(pc.mode % 11) * 0.012 + hash11(sphere_index) * 0.08;
    vec3 center = vec3(
        cos(orbit_angle) * orbit_radius,
        sin(orbit_angle * 0.5 + sphere_ratio * 9.0) * 0.28,
        sin(orbit_angle) * orbit_radius
    );

    vec3 wobble = vec3(
        sin(index * 0.013 + pc.time_seconds * 1.1),
        cos(index * 0.017 - pc.time_seconds * 0.8),
        sin(index * 0.019 + pc.time_seconds * 0.6)
    ) * pc.chaos * 0.028;

    vec3 p = center + local + wobble;
    float depth = 1.55 + p.z * 0.55;
    vec2 projected = p.xy / max(depth, 0.42);
    gl_Position = vec4(projected, p.z * 0.16, 1.0);
    gl_PointSize = 2.1 + 0.9 * shell_radius * 10.0 + pc.chaos * 0.45;

    vec3 palette_a = mix(vec3(0.10, 0.72, 0.98), vec3(1.0, 0.40, 0.18), sphere_ratio);
    vec3 palette_b = mix(vec3(0.92, 0.92, 1.0), vec3(0.24, 0.08, 1.0), shell_ratio);
    float accent = 0.5 + 0.5 * sin(phi * 3.0 + pc.time_seconds + sphere_ratio * 11.0);
    particle_color = vec4(mix(palette_a, palette_b, accent), 0.88);
}
