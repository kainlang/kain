// resources/shaders/ui_fragment.glsl
// Fragment shader for reson8 UI: rounded rectangles with anti-aliased SDF
// Supports gradients, borders, hover/pressed state overlays
// Layer: L6 Machine — GPU fragment processing
#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(binding = 0) uniform FragUBO {
    vec4 uBgColor;
    float uCornerRadius;
    vec4 uBorderColor;
    float uBorderWidth;
    float uIsPressed;
    float uIsHovered;
};

layout(location = 0) out vec4 fragColor;

void main() {
    vec2 size = vec2(1.0, 1.0); // Normalized, actual size in texcoords
    vec2 halfSize = size * 0.5;
    vec2 uv = vUV;

    // SDF for rounded rectangle
    // Distance from interior of rounded rect
    vec2 q = abs(uv - halfSize) - halfSize + uCornerRadius;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - uCornerRadius;

    // Smooth anti-aliased edge
    float aa = 1.0 - smoothstep(-1.0, 1.0, d);
    float alpha = aa;

    // Interior fill color
    vec3 col = uBgColor.rgb;

    // Hover/pressed state overlay
    if (uIsPressed > 0.5) {
        col = col * 0.85; // Darken
    } else if (uIsHovered > 0.5) {
        col = col * 1.05; // Lighten
    }

    // Border rendering
    if (uBorderWidth > 0.0) {
        float borderD = abs(d) - uBorderWidth;
        float borderAlpha = 1.0 - smoothstep(-1.0, 1.0, borderD);
        col = mix(col, uBorderColor.rgb, borderAlpha);
        alpha = max(alpha, borderAlpha);
    }

    fragColor = vec4(col, uBgColor.a * alpha);
}
