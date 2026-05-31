#version 450

layout(location = 0) in vec4 particle_color;
layout(location = 0) out vec4 out_color;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    if (radius2 > 1.0) {
        discard;
    }

    vec3 normal = normalize(vec3(centered, sqrt(max(1.0 - radius2, 0.0))));
    vec3 light_dir = normalize(vec3(-0.35, 0.55, 1.0));
    float lambert = max(dot(normal, light_dir), 0.0);
    float fresnel = pow(1.0 - max(normal.z, 0.0), 2.0);
    float edge = smoothstep(1.0, 0.0, radius2);
    vec3 highlight = vec3(1.0, 0.96, 0.88) * pow(lambert, 6.0);
    vec3 shaded = particle_color.rgb * (0.28 + lambert * 0.92) + highlight + fresnel * vec3(0.15, 0.20, 0.35);
    out_color = vec4(shaded, particle_color.a * edge);
}
