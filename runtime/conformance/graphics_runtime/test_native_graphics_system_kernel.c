#include "../../native/include/graphics_system.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "[FAIL] %s\n", label);
        return 0;
    }
    return 1;
}

static int expect_text(const char* actual, const char* expected, const char* label) {
    if (!actual || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "[FAIL] %s expected '%s' got '%s'\n",
            label,
            expected ? expected : "<null>",
            actual ? actual : "<null>"
        );
        return 0;
    }
    return 1;
}

static int expect_contains(const char* actual, const char* needle, const char* label) {
    if (!actual || !needle || strstr(actual, needle) == 0) {
        fprintf(
            stderr,
            "[FAIL] %s expected substring '%s' in '%s'\n",
            label,
            needle ? needle : "<null>",
            actual ? actual : "<null>"
        );
        return 0;
    }
    return 1;
}

static int build_authored_submission(
    int64_t session,
    const char* submission_label,
    const char* vertex_hex,
    const char* index_hex,
    int64_t vertex_count,
    int64_t index_count,
    int64_t* out_mesh,
    int64_t* out_pipeline
) {
    int64_t vertex_buffer = abi_graphics_buffer_create_from_hex(
        session,
        "vertex",
        submission_label,
        vertex_hex,
        12
    );
    int64_t index_buffer = abi_graphics_buffer_create_from_hex(
        session,
        "index",
        submission_label,
        index_hex,
        4
    );
    int64_t vertex_shader = abi_graphics_shader_spirv_from_hex(
        session,
        "author.vertex",
        "vertex",
        "main",
        "03022307"
    );
    int64_t fragment_shader = abi_graphics_shader_spirv_from_hex(
        session,
        "author.fragment",
        "fragment",
        "main",
        "03022307"
    );
    int64_t mesh;
    int64_t pipeline;

    if (!expect_true(vertex_buffer > 0, "vertex buffer created")) return 0;
    if (!expect_true(index_buffer > 0, "index buffer created")) return 0;
    if (!expect_true(vertex_shader > 0, "vertex shader created")) return 0;
    if (!expect_true(fragment_shader > 0, "fragment shader created")) return 0;
    if (!expect_text(
            abi_graphics_buffer_kind(session, vertex_buffer),
            "vertex",
            "vertex buffer kind"
        )) return 0;
    if (!expect_true(
            abi_graphics_buffer_byte_at(session, vertex_buffer, 4) == 1,
            "authored vertex bytes retained"
        )) return 0;
    if (!expect_true(
            abi_graphics_shader_byte_at(session, vertex_shader, 3) == 7,
            "authored SPIR-V bytes retained"
        )) return 0;

    mesh = abi_graphics_mesh_create(
        session,
        submission_label,
        vertex_buffer,
        index_buffer,
        vertex_count,
        index_count
    );
    pipeline = abi_graphics_pipeline_create(
        session,
        submission_label,
        vertex_shader,
        fragment_shader,
        "d3d12"
    );

    if (!expect_true(mesh > 0, "mesh created")) return 0;
    if (!expect_true(pipeline > 0, "pipeline created")) return 0;
    if (!expect_text(
            abi_graphics_pipeline_backend(session, pipeline),
            "d3d12",
            "pipeline backend records requested target"
        )) return 0;

    *out_mesh = mesh;
    *out_pipeline = pipeline;
    return 1;
}

int main(void) {
    int64_t session_a;
    int64_t session_b;
    int64_t mesh_a;
    int64_t mesh_b;
    int64_t pipeline_a;
    int64_t pipeline_b;
    int64_t draw_count;

    if (!expect_true(abi_graphics_reset() == 0, "reset")) return 1;
    if (!expect_true(abi_graphics_session_count() == 0, "initial session count")) return 2;
    if (!expect_true(abi_graphics_backend_supported("vulkan") == 1, "vulkan supported target")) return 3;
    if (!expect_true(abi_graphics_backend_supported("directx12") == 1, "directx12 alias supported")) return 4;
    if (!expect_true(abi_graphics_backend_available("vulkan") == 0, "vulkan executor not claimed")) return 5;
    if (!expect_contains(
            abi_graphics_backend_status("d3d12"),
            "no direct D3D12 executor",
            "d3d12 status is honest"
        )) return 6;

    session_a = abi_graphics_session_create("first-authored-engine", 1280, 720);
    session_b = abi_graphics_session_create("second-authored-engine", 640, 480);
    if (!expect_true(session_a > 0 && session_b > 0 && session_a != session_b, "distinct sessions")) return 7;
    if (!expect_true(abi_graphics_session_count() == 2, "session count after create")) return 8;

    if (!expect_true(abi_graphics_backend_select(session_a, "vulkan") == 0, "select vulkan target")) return 9;
    if (!expect_text(abi_graphics_active_backend(session_a), "vulkan", "active vulkan target")) return 10;
    if (!expect_text(abi_graphics_last_error_kind(), "degraded-backend", "degraded backend status")) return 11;

    if (!build_authored_submission(
            session_a,
            "triangle-authored-by-kain",
            "000000000100000002000000",
            "000000000100000002000000",
            3,
            3,
            &mesh_a,
            &pipeline_a
        )) return 12;

    if (!build_authored_submission(
            session_b,
            "quad-authored-by-kain",
            "00000000010000000200000003000000",
            "0000000001000000020000000200000003000000",
            4,
            6,
            &mesh_b,
            &pipeline_b
        )) return 13;

    if (!expect_true(abi_graphics_mesh_vertex_count(session_a, mesh_a) == 3, "submission A vertex count")) return 15;
    if (!expect_true(abi_graphics_mesh_vertex_count(session_b, mesh_b) == 4, "submission B vertex count")) return 16;
    if (!expect_text(
            abi_graphics_mesh_label(session_a, mesh_a),
            "triangle-authored-by-kain",
            "submission A label retained"
        )) return 17;
    if (!expect_text(
            abi_graphics_mesh_label(session_b, mesh_b),
            "quad-authored-by-kain",
            "submission B label retained"
        )) return 18;

    if (!expect_true(abi_graphics_begin_frame(session_a, 8.33) == 1, "begin frame")) return 19;
    draw_count = abi_graphics_draw_mesh(session_a, pipeline_a, mesh_a, 2);
    if (!expect_true(draw_count == 1, "draw command appended")) return 20;
    if (!expect_text(
            abi_graphics_draw_command_kind(session_a, 0),
            "draw_mesh",
            "draw command kind"
        )) return 21;
    if (!expect_true(
            abi_graphics_draw_command_instances(session_a, 0) == 2,
            "draw command instance count"
        )) return 22;
    if (!expect_true(abi_graphics_end_frame(session_a) == 1, "end frame command count")) return 23;
    if (!expect_true(abi_graphics_present(session_a) == 1, "present frame")) return 24;

    if (!expect_true(
            abi_graphics_mesh_create(session_a, "bad", 999, 999, 3, 3) < 0,
            "invalid mesh rejected"
        )) return 25;
    if (!expect_text(abi_graphics_last_error_kind(), "invalid-resource", "invalid resource status")) return 26;

    if (!expect_true(abi_graphics_session_destroy(session_a) == 0, "destroy session A")) return 27;
    if (!expect_true(abi_graphics_session_destroy(session_b) == 0, "destroy session B")) return 28;
    if (!expect_true(abi_graphics_session_count() == 0, "final session count")) return 29;

    (void)pipeline_b;
    return 0;
}
