#version 450

layout(push_constant) uniform KQuantumPush {
    float time_seconds;
    float particle_count;
    int mode;
    float chaos;
} pc;

layout(location = 0) out vec4 particle_color;

float hash11(float value) {
    return fract(sin(value * 127.1) * 43758.5453123);
}

void main() {
    float index = float(gl_VertexIndex);
    float total = max(pc.particle_count, 1.0);
    float u = (index + 0.5) / total;
    float strand = fract(index * 0.61803398875);
    float shell = sqrt(fract(index * 0.754877666));
    float theta = u * 628.3185307 + pc.time_seconds * (0.35 + pc.chaos * 0.18);
    float harmonic = sin(theta * 0.071 + pc.time_seconds * 1.7);
    float mode_phase = float(pc.mode) * 0.17320508;

    vec3 p;
    if (pc.mode == 17) {
        float r = 0.08 + shell * 0.82;
        p = vec3(cos(theta) * r, sin(theta * 0.53) * 0.45, sin(theta) * r);
        p.xy += vec2(sin(strand * 31.0 + pc.time_seconds), cos(strand * 19.0 - pc.time_seconds)) * 0.055;
    } else if (pc.mode == 20) {
        float plume = pow(strand, 0.42);
        p = vec3((hash11(index) - 0.5) * plume, -0.84 + plume * 1.72, sin(theta) * 0.22);
        p.xz += vec2(sin(theta), cos(theta * 0.81)) * plume * 0.34;
    } else if (pc.mode == 22) {
        float r = 0.18 + shell * 0.68;
        p = vec3(cos(theta * 1.7) * r, fract(u * 47.0 + pc.time_seconds * 0.13) * 1.76 - 0.88, sin(theta * 1.7) * r);
        p.xz += normalize(p.xz + vec2(0.001)) * sin(p.y * 9.0 + pc.time_seconds * 3.0) * 0.13;
    } else {
        float r = 0.12 + shell * 0.78;
        p = vec3(cos(theta + mode_phase) * r, sin(theta * 0.37 + mode_phase) * 0.55, sin(theta + mode_phase) * r);
    }

    p += vec3(
        sin(index * 0.013 + pc.time_seconds * 0.9),
        cos(index * 0.017 - pc.time_seconds * 0.7),
        sin(index * 0.019 + pc.time_seconds * 0.5)
    ) * pc.chaos * 0.035;

    float depth = 1.28 + p.z * 0.42;
    vec2 projected = p.xy / max(depth, 0.42);
    gl_Position = vec4(projected, p.z * 0.12, 1.0);
    gl_PointSize = 1.35 + harmonic * 0.45 + pc.chaos * 0.22;

    vec3 nebula = mix(vec3(0.0, 1.0, 0.82), vec3(1.0, 0.2, 0.02), smoothstep(-0.7, 0.9, harmonic));
    vec3 spectral = mix(vec3(0.33, 0.05, 1.0), vec3(1.0, 0.9, 0.12), strand);
    particle_color = vec4(mix(nebula, spectral, 0.45 + 0.35 * sin(mode_phase + u * 31.0)), 0.82);
}
