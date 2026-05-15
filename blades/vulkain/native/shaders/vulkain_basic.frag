#version 450

layout(location = 0) in vec3 triangle_color;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(triangle_color, 1.0);
}

