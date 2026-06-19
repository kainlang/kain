"""mgl_surface — thin ModernGL submission surface for Kain-owned renderer.

Kain OWNS:  uniform computation, state, cadence, world, patch, law.
Python OWNS: OpenGL 4.6 window, GLSL compilation, draw submission, event pump.

When Kain's SPIR-V vertex→fragment varying linkage ships, swap GLSL strings
for Kain-authored `shader vertex`/`shader fragment` → SPIR-V bytes.
"""

import os
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import moderngl
import numpy as np
import pygame

_VERT_GLSL = """#version 460 core
    layout(location = 0) in vec2 in_pos;
    layout(location = 1) in vec2 in_uv;
    out vec2 v_uv;
    void main() {
        gl_Position = vec4(in_pos, 0.0, 1.0);
        v_uv = in_uv;
    }
"""

_FRAG_GLSL = """#version 460 core
    uniform float u_time;
    uniform vec2 u_resolution;
    uniform vec3 u_color;
    in vec2 v_uv;
    out vec4 out_color;
    void main() {
        vec2 p = v_uv;
        float aspect = u_resolution.x / u_resolution.y;
        float wave = sin(p.x * 10.0 * aspect + u_time)
                   * cos(p.y * 8.0 + u_time * 0.7) * 0.5 + 0.5;
        vec3 base = mix(vec3(0.05, 0.05, 0.15), u_color, wave);
        base += vec3(0.15, 0.05, 0.25) * sin(p.y * 20.0 - u_time * 1.3) * 0.3;
        out_color = vec4(base, 1.0);
    }
"""

_state = {
    "screen": None,
    "ctx": None,
    "clock": None,
    "running": False,
    "vao": None,
    "prog": None,
}


def kain_mgl_open(width=1024, height=768, title="Kain + ModernGL"):
    """Open pygame window with OpenGL 4.6 core + ModernGL context.
    OpenGL 4.6 required for future SPIR-V path (GL_SHADER_BINARY_FORMAT_SPIR_V)."""
    st = _state
    if st["running"]:
        return 1
    pygame.init()
    pygame.display.gl_set_attribute(pygame.GL_CONTEXT_MAJOR_VERSION, 4)
    pygame.display.gl_set_attribute(pygame.GL_CONTEXT_MINOR_VERSION, 6)
    pygame.display.gl_set_attribute(
        pygame.GL_CONTEXT_PROFILE_MASK, pygame.GL_CONTEXT_PROFILE_CORE
    )
    st["screen"] = pygame.display.set_mode(
        (width, height), pygame.OPENGL | pygame.DOUBLEBUF
    )
    pygame.display.set_caption(title)
    st["ctx"] = moderngl.create_context()
    st["clock"] = pygame.time.Clock()
    st["running"] = True
    return 1


def kain_mgl_load_shaders() -> int:
    """Compile built-in GLSL shaders into GPU program. Returns 1 on success."""
    st = _state
    if not st["running"]:
        return 0
    try:
        st["prog"] = st["ctx"].program(
            vertex_shader=_VERT_GLSL,
            fragment_shader=_FRAG_GLSL,
        )
    except Exception as e:
        print(f"[mgl_surface] Shader compile error: {e}")
        return 0
    vertices = np.array(
        [-1.0, -1.0, 0.0, 0.0,
          3.0, -1.0, 1.0, 0.0,
         -1.0,  3.0, 0.0, 1.0],
        dtype="f4",
    )
    st["vao"] = st["ctx"].vertex_array(
        st["prog"],
        [(st["ctx"].buffer(vertices.tobytes()), "2f 2f", "in_pos", "in_uv")],
    )
    return 1


def kain_mgl_submit(time_val: float, res_x: float, res_y: float,
                    r: float, g: float, b: float) -> int:
    """Submit one frame with Kain-computed uniforms.
    Returns 1 on success, -1 on quit, 0 if stopped."""
    st = _state
    if not st["running"]:
        return 0
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            st["running"] = False
            return -1
        if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
            st["running"] = False
            return -1
    if st["prog"] is None:
        return 0
    try:
        st["prog"]["u_time"] = time_val
        st["prog"]["u_resolution"] = (res_x, res_y)
        st["prog"]["u_color"] = (r, g, b)
    except KeyError:
        pass
    st["vao"].render()
    pygame.display.flip()
    st["clock"].tick(60)
    return 1


def kain_mgl_close():
    """Tear down all GL and pygame resources."""
    st = _state
    if st["vao"] is not None:
        st["vao"].release()
    if st["prog"] is not None:
        st["prog"].release()
    if st["ctx"] is not None:
        st["ctx"].release()
    if st["running"]:
        pygame.display.quit()
        pygame.quit()
    st["running"] = False
    return 1
