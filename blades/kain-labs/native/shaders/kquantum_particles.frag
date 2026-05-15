#version 450

layout(location = 0) in vec4 particle_color;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    float core = smoothstep(1.0, 0.0, radius2);
    float hot = smoothstep(0.18, 0.0, radius2);
    vec3 color = particle_color.rgb * core + vec3(1.0, 0.92, 0.55) * hot * 0.55;
    out_color = vec4(color, particle_color.a * core);
}
