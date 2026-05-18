#version 450

layout(push_constant) uniform VulkainPush {
    float time_seconds;
    float accent_r;
    float accent_g;
    float accent_b;
    float camera_yaw;
    float camera_pitch;
    float mesh_scale;
    float mesh_twist;
    float depth_bias;
    float energy;
} pc;

layout(location = 0) in vec3 mesh_color;
layout(location = 0) out vec4 out_color;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; ++i) {
        value += noise2(p) * amp;
        p = p * 2.03 + vec2(7.1, 3.7);
        amp *= 0.5;
    }
    return value;
}

vec2 box_intersect(vec3 ro, vec3 rd, vec3 b) {
    vec3 inv_rd = 1.0 / rd;
    vec3 t0 = (-b - ro) * inv_rd;
    vec3 t1 = ( b - ro) * inv_rd;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float near_t = max(max(tmin.x, tmin.y), tmin.z);
    float far_t = min(min(tmax.x, tmax.y), tmax.z);
    return vec2(near_t, far_t);
}

vec3 box_normal(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    if (q.x > q.y && q.x > q.z) {
        return vec3(sign(p.x), 0.0, 0.0);
    }
    if (q.y > q.z) {
        return vec3(0.0, sign(p.y), 0.0);
    }
    return vec3(0.0, 0.0, sign(p.z));
}

vec3 rotate_y(vec3 p, float a) {
    float s = sin(a);
    float c = cos(a);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

vec3 rotate_x(vec3 p, float a) {
    float s = sin(a);
    float c = cos(a);
    return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

vec3 tonemap(vec3 color) {
    color = max(color, vec3(0.0));
    return color / (color + vec3(1.0));
}

vec3 raytrace_scene(vec2 uv) {
    vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
    vec3 ro = vec3(0.0, 1.35 + pc.camera_pitch * 0.25, -5.4);
    vec3 rd = normalize(vec3(uv.x, uv.y * 0.72, 1.55));
    rd = rotate_x(rd, -0.12 + pc.camera_pitch * 0.16);
    rd = rotate_y(rd, pc.camera_yaw * 0.22);

    vec3 cube_center = vec3(
        sin(pc.camera_yaw + pc.time_seconds * 0.85) * 1.25,
        0.78 + abs(sin(pc.mesh_twist + pc.time_seconds * 1.6)) * 1.10,
        1.2 + cos(pc.mesh_twist * 0.7) * 0.55
    );
    float cube_spin = pc.mesh_twist + pc.time_seconds * (1.25 + pc.energy * 0.12);
    vec3 local_ro = rotate_y(ro - cube_center, -cube_spin);
    vec3 local_rd = rotate_y(rd, -cube_spin);
    vec3 half_extent = vec3(0.58 + pc.mesh_scale * 0.12);
    vec2 hit = box_intersect(local_ro, local_rd, half_extent);

    vec3 sky = mix(vec3(0.006, 0.009, 0.016), vec3(0.05, 0.10, 0.16), smoothstep(-0.4, 1.0, rd.y));
    vec3 color = sky;
    float best_t = 100000.0;

    if (hit.x > 0.0 && hit.x < hit.y) {
        best_t = hit.x;
        vec3 lp = local_ro + local_rd * hit.x;
        vec3 n = rotate_y(box_normal(lp, half_extent), cube_spin);
        vec3 world_p = ro + rd * hit.x;
        vec3 light_dir = normalize(vec3(-0.35, 0.85, -0.28));
        float ndotl = max(dot(n, light_dir), 0.0);
        float rim = pow(max(1.0 - max(dot(n, -rd), 0.0), 0.0), 2.0);
        float proc = fbm(world_p.xz * 3.0 + pc.time_seconds * 0.08);
        color = accent * (0.28 + ndotl * 1.45) + vec3(1.0, 0.72, 0.28) * rim * 0.55 + proc * 0.18;
    }

    float plane_t = (-0.72 - ro.y) / rd.y;
    if (plane_t > 0.0 && plane_t < best_t) {
        vec3 p = ro + rd * plane_t;
        float cells = step(0.5, fract(p.x * 0.55) * fract(p.z * 0.55));
        float terrain = fbm(p.xz * 0.85 + pc.mesh_twist);
        float glow = exp(-abs(length(p.xz - cube_center.xz) - 1.15) * 2.8);
        color = mix(vec3(0.018, 0.024, 0.030), vec3(0.05, 0.12, 0.11), cells);
        color += accent * (0.12 + terrain * 0.18 + glow * 0.55);
    }

    float fog = 1.0 - exp(-0.025 * min(best_t, 28.0) * min(best_t, 28.0));
    return tonemap(mix(color, sky, fog) * (1.0 + pc.energy * 0.08));
}

void main() {
    if (pc.depth_bias < -4.0) {
        vec2 uv = (gl_FragCoord.xy / vec2(1280.0, 720.0)) * 2.0 - 1.0;
        uv.x *= 1280.0 / 720.0;
        out_color = vec4(raytrace_scene(uv), 1.0);
        return;
    }

    out_color = vec4(mesh_color, 1.0);
}

