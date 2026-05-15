#version 450

layout(push_constant) uniform VulkainPush {
    float time_seconds;
    float accent_r;
    float accent_g;
    float accent_b;
} pc;

layout(location = 0) out vec3 triangle_color;

void main() {
    vec2 positions[3] = vec2[](
        vec2(0.0, -0.62),
        vec2(0.64, 0.48),
        vec2(-0.64, 0.48)
    );

    vec2 position = positions[gl_VertexIndex];
    float phase = pc.time_seconds * 1.35 + float(gl_VertexIndex) * 1.6180339;
    position.x += sin(phase) * 0.035;
    position.y += cos(phase * 0.73) * 0.02;
    gl_Position = vec4(position, 0.0, 1.0);

    vec3 accent = vec3(pc.accent_r, pc.accent_g, pc.accent_b);
    vec3 variants[3] = vec3[](
        accent,
        vec3(accent.z, min(accent.x + 0.32, 1.0), max(accent.y - 0.16, 0.0)),
        vec3(min(accent.x + 0.14, 1.0), min(accent.y + 0.22, 1.0), min(accent.z + 0.08, 1.0))
    );
    triangle_color = variants[gl_VertexIndex];
}

