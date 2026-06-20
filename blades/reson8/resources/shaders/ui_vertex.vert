// resources/shaders/ui_vertex.glsl
// Vertex shader for reson8 UI elements: position, UV, color
// Compatible with SPIR-V via glslangValidator
// Layer: L6 Machine — GPU vertex processing
#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(binding = 0) uniform ProjUBO {
    mat4 uProj;
};

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    gl_Position = uProj * vec4(aPosition, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
