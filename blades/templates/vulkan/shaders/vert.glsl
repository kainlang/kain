#version 450
// ============================================================================
//  VULKAN TEMPLATE — Vertex Shader
//  Colored cube with MVP uniform buffer.
//  Input:  position (location 0), color (location 1)
//  Uniform: mvp mat4 at binding 0 (std140)
//  Output: gl_Position (built-in), v_color (location 0)
// ============================================================================

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;

layout(binding = 0, std140) uniform UBO {
    mat4 mvp;
} ubo;

layout(location = 0) out vec3 v_color;

void main() {
    gl_Position = ubo.mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
