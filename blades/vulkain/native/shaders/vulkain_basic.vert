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

layout(location = 0) out vec3 mesh_color;

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

void main() {
    if (pc.depth_bias < -4.0) {
        vec2 positions[3] = vec2[](
            vec2(-1.0, -1.0),
            vec2(3.0, -1.0),
            vec2(-1.0, 3.0)
        );
        gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);
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

    float depth = 3.15 + pc.depth_bias + p.z;
    float inv_depth = 1.0 / max(depth, 0.35);
    vec2 projected = p.xy * inv_depth * 1.72;
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
