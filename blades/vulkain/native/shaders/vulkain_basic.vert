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
    float clone_count;
    float layout_mode;
    float grid_width;
    float spacing;
    float radial_radius;
    float wave_amount;
    float animation_speed;
    float target_fps;
    float ui_command_count;
    float ui_checksum;
    float viewport_width;
    float viewport_height;
    float honeycomb_bias;
    float row_count;
    float column_count;
    float lod_budget;
    float camera_zoom;
    float camera_pan_x;
    float camera_pan_y;
    float control_overlay;
} pc;

layout(location = 0) out vec3 mesh_color;
layout(location = 1) out vec2 sphere_uv;
layout(location = 2) flat out int sphere_variant;

vec2 billboard_corner(int index) {
    vec2 corners[6] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0,  1.0),
        vec2(-1.0, -1.0)
    );
    return corners[index % 6];
}

vec3 cube_corner(int index) {
    vec3 corners[8] = vec3[](
        vec3(-1.0, -1.0, -1.0),
        vec3( 1.0, -1.0, -1.0),
        vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0, -1.0),
        vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0,  1.0),
        vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0,  1.0)
    );
    return corners[index & 7];
}

vec3 cube_vertex(int vertex_index) {
    int indices[36] = int[](
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        5, 4, 7, 7, 6, 5,
        4, 0, 3, 3, 7, 4,
        3, 2, 6, 6, 7, 3,
        4, 5, 1, 1, 0, 4
    );
    return cube_corner(indices[vertex_index % 36]);
}

vec3 rotate_y(vec3 p, float radians) {
    float s = sin(radians);
    float c = cos(radians);
    return vec3((p.x * c) + (p.z * s), p.y, (-p.x * s) + (p.z * c));
}

vec3 rotate_x(vec3 p, float radians) {
    float s = sin(radians);
    float c = cos(radians);
    return vec3(p.x, (p.y * c) - (p.z * s), (p.y * s) + (p.z * c));
}

vec3 clone_center_grid(int index, float side, float spacing) {
    int stride = max(int(side), 1);
    int ix = index % stride;
    int iz = (index / stride) % stride;
    int iy = index / max(stride * stride, 1);
    vec3 p = vec3(float(ix), float(iy), float(iz));
    p -= vec3(float(stride - 1), max(1.0, pc.row_count) * 0.5, float(stride - 1)) * 0.5;
    p *= spacing;
    return p;
}

vec3 clone_center_radial(int index, float count, float radius) {
    float fi = float(index);
    float ring = floor(sqrt(fi + 0.5));
    float t = fi * 2.39996323 + pc.time_seconds * pc.animation_speed * 0.22;
    float r = radius * sqrt((fi + 1.0) / max(count, 1.0));
    return vec3(cos(t) * r, (sin(ring * 0.73 + pc.time_seconds) * pc.wave_amount) + ring * 0.015, sin(t) * r);
}

vec3 clone_center_honeycomb(int index, float columns, float spacing) {
    int cols = max(int(columns), 1);
    int row = index / cols;
    int col = index - row * cols;
    float odd = float(row & 1) * 0.5;
    float x = (float(col) + odd - float(cols) * 0.5) * spacing;
    float z = (float(row) - max(pc.row_count, 1.0) * 0.5) * spacing * pc.honeycomb_bias;
    float y = sin(float(col) * 0.31 + float(row) * 0.17 + pc.time_seconds * pc.animation_speed) * pc.wave_amount;
    return vec3(x, y, z);
}

vec3 clone_center_helix(int index, float count, float spacing) {
    float fi = float(index);
    float turns = 18.0 + pc.radial_radius * 0.15;
    float t = (fi / max(count, 1.0)) * turns * 6.2831853 + pc.time_seconds * pc.animation_speed;
    float r = pc.radial_radius * (0.45 + 0.35 * sin(t * 0.17));
    float y = (fi / max(count, 1.0) - 0.5) * spacing * 80.0;
    return vec3(cos(t) * r, y, sin(t) * r);
}

void main() {
    sphere_uv = vec2(0.0);
    sphere_variant = 0;

    if (pc.depth_bias < -4.0) {
        vec2 positions[3] = vec2[](
            vec2(-1.0, -1.0),
            vec2( 3.0, -1.0),
            vec2(-1.0,  3.0)
        );
        gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);
        mesh_color = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
        return;
    }

    if (pc.clone_count > 1.5) {
        int clone_index = gl_InstanceIndex;
        int layout_id = int(pc.layout_mode + 0.5);
        float count = max(pc.clone_count, 1.0);
        float spacing = max(pc.spacing, 0.04);
        vec3 center = clone_center_grid(clone_index, max(pc.grid_width, 1.0), spacing);
        if (layout_id == 2) {
            center = clone_center_radial(clone_index, count, max(pc.radial_radius, 0.2));
        } else if (layout_id == 3) {
            center = clone_center_honeycomb(clone_index, max(pc.column_count, 1.0), spacing);
        } else if (layout_id >= 4) {
            center = clone_center_helix(clone_index, count, spacing);
        }

        float t = pc.time_seconds * pc.animation_speed + float(clone_index % 997) * 0.017;
        center.y += sin(t) * pc.wave_amount;
        center = rotate_y(center, pc.camera_yaw + pc.time_seconds * 0.035);
        center = rotate_x(center, pc.camera_pitch);

        float depth = max(pc.camera_zoom + pc.depth_bias + center.z * 0.18, 0.65);
        float aspect = max(pc.viewport_width / max(pc.viewport_height, 1.0), 0.1);
        float inv_depth = 1.0 / depth;
        vec2 projected = vec2(center.x / aspect, center.y) * inv_depth * 2.1 + vec2(pc.camera_pan_x, pc.camera_pan_y);
        vec2 corner = billboard_corner(gl_VertexIndex);
        float density_scale = mix(1.0, 0.42, smoothstep(75000.0, 1000000.0, count));
        float billboard_radius = max(pc.mesh_scale, 0.015) * density_scale * inv_depth * 5.8;

        sphere_uv = corner;
        sphere_variant = clone_index % 5;
        gl_Position = vec4(projected + corner * billboard_radius, 0.0, 1.0);
        mesh_color = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
        return;
    }

    int vertex_index = gl_VertexIndex % 36;
    int face_index = vertex_index / 6;
    vec3 p = cube_vertex(vertex_index);

    float time_twist = pc.time_seconds * (0.35 + (pc.energy * 0.12));
    p.x += sin((p.y * 1.7) + pc.mesh_twist + time_twist) * 0.16;
    p.y += cos((p.z * 1.3) + pc.mesh_twist + (time_twist * 0.73)) * 0.08;
    p *= max(pc.mesh_scale, 0.15);

    p = rotate_y(p, pc.camera_yaw + (pc.time_seconds * 0.28));
    p = rotate_x(p, pc.camera_pitch);

    float depth = pc.camera_zoom + pc.depth_bias + p.z;
    float inv_depth = 1.0 / max(depth, 0.35);
    vec2 projected = p.xy * inv_depth * 1.72 + vec2(pc.camera_pan_x, pc.camera_pan_y);
    gl_Position = vec4(projected, 0.0, 1.0);

    vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
    vec3 face_tint[6] = vec3[](
        accent,
        vec3(accent.z, min(accent.x + 0.24, 1.0), min(accent.y + 0.10, 1.0)),
        vec3(min(accent.x + 0.30, 1.0), accent.y, min(accent.z + 0.22, 1.0)),
        vec3(max(accent.x - 0.10, 0.0), min(accent.y + 0.34, 1.0), accent.z),
        vec3(min(accent.x + 0.18, 1.0), min(accent.y + 0.18, 1.0), max(accent.z - 0.18, 0.0)),
        vec3(accent.y, accent.z, min(accent.x + 0.38, 1.0))
    );
    float light = 0.64 + (0.12 * float(face_index)) + (0.10 * sin(pc.time_seconds + float(vertex_index)));
    mesh_color = clamp(face_tint[face_index] * light, vec3(0.0), vec3(1.0));
}
