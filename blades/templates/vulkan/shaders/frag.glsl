#version 450
// ============================================================================
//  VULKAN TEMPLATE — Fragment Shader
//  Simple color passthrough.
//  Input:  v_color (location 0)
//  Output: out_color (location 0)
// ============================================================================

layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(v_color, 1.0);
}
