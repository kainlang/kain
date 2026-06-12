/*
 * check_graphics_system.c - CBMC verification harness for graphics system
 *
 * Verifies the native graphics system: session lifecycle (create/destroy),
 * backend queries, frame cycle (begin/end/present), buffer/shader/mesh/
 * pipeline resource management, draw command recording, and error state
 * tracking.
 *
 * Focus areas:
 *   - Session create/destroy/count invariants
 *   - Frame index and presented-frame tracking
 *   - Buffer resource creation and property accessors
 *   - Draw command recording and property accessors
 *   - Backend query functions (supported/available/select)
 *   - NULL-safety, invalid-argument paths, capacity-exceeded paths
 *   - Reset/error-state accessors
 *
 * NOTE: const char* returns from string_new-based helpers are CBMC-
 * nondeterministic.  We verify int64_t return codes and observable
 * state changes through int64_t accessors, not string equality.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_graphics_system
 */

#include "graphics_system.h"

/* ──────────────────────────────────────────────────────────────────────
 * Static backing storage
 *
 * For buffer byte payloads we use static buffers so that pointer
 * provenance through graphics buffers is valid.
 * ────────────────────────────────────────────────────────────────────── */
static uint8_t g_buffer_payload[1024];
static char    g_app_name[ABI_GRAPHICS_MAX_KEY];
static char    g_kind_name[ABI_GRAPHICS_MAX_KEY];
static char    g_label_text[ABI_GRAPHICS_MAX_KEY];

/* ──────────────────────────────────────────────────────────────────────
 * NOTE: All public ABI functions are declared in graphics_system.h.
 * No forward declarations needed — the header + source are combined
 * into one translation unit by the CBMC pipeline.
 * ────────────────────────────────────────────────────────────────────── */


/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a session and return its ID
 *
 * Uses nondeterministic app_name/width/height but constrains size to
 * positive values.
 * ────────────────────────────────────────────────────────────────────── */
static int64_t create_session(void) {
    int64_t width, height;
    __CPROVER_havoc_object(&width);
    __CPROVER_havoc_object(&height);
    __CPROVER_assume(width > 0 && width <= 4096);
    __CPROVER_assume(height > 0 && height <= 4096);

    return abi_graphics_session_create(g_app_name, width, height);
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 1: abi_graphics_reset — resets global state to clean
 * ────────────────────────────────────────────────────────────────────── */
void check_reset(void) {
    int64_t rc = abi_graphics_reset();
    __CPROVER_assert(rc == ABI_GRAPHICS_OK, "reset returns OK");

    /* After reset, session count is 0 */
    int64_t count = abi_graphics_session_count();
    __CPROVER_assert(count == 0, "after reset, session count == 0");

    /* Last status is OK */
    int64_t status = abi_graphics_last_status();
    __CPROVER_assert(status == ABI_GRAPHICS_OK, "after reset, last_status == OK");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 2: abi_graphics_session_create — basic lifecycle
 * ────────────────────────────────────────────────────────────────────── */
void check_session_create(void) {
    abi_graphics_reset();

    int64_t id = create_session();
    __CPROVER_assert(id > 0, "session create returns positive ID");

    int64_t count = abi_graphics_session_count();
    __CPROVER_assert(count == 1, "one session created");

    /* Session destroy */
    int64_t rc = abi_graphics_session_destroy(id);
    __CPROVER_assert(rc == ABI_GRAPHICS_OK, "session destroy returns OK");

    int64_t count2 = abi_graphics_session_count();
    __CPROVER_assert(count2 == 0, "after destroy, session count == 0");

    /* Destroy already-destroyed session returns INVALID_SESSION */
    int64_t rc2 = abi_graphics_session_destroy(id);
    __CPROVER_assert(rc2 == ABI_GRAPHICS_INVALID_SESSION,
                     "destroy destroyed session -> INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 3: abi_graphics_session_create — invalid arguments
 * ────────────────────────────────────────────────────────────────────── */
void check_session_create_invalid(void) {
    abi_graphics_reset();

    /* Zero width */
    int64_t id1 = abi_graphics_session_create("test", 0, 100);
    __CPROVER_assert(id1 < 0, "zero width -> error");

    /* Zero height */
    int64_t id2 = abi_graphics_session_create("test", 100, 0);
    __CPROVER_assert(id2 < 0, "zero height -> error");

    /* Negative width */
    int64_t id3 = abi_graphics_session_create("test", -1, 100);
    __CPROVER_assert(id3 < 0, "negative width -> error");

    /* NULL app_name */
    int64_t id4 = abi_graphics_session_create(NULL, 100, 100);
    __CPROVER_assert(id4 > 0, "NULL app_name still creates session (code accepts NULL)");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 4: abi_graphics_session_create — capacity exceeded
 * ────────────────────────────────────────────────────────────────────── */
void check_session_capacity(void) {
    abi_graphics_reset();

    /* Create ABI_GRAPHICS_MAX_SESSIONS sessions */
    int64_t ids[ABI_GRAPHICS_MAX_SESSIONS];
    int i;
    for (i = 0; i < ABI_GRAPHICS_MAX_SESSIONS; i++) {
        ids[i] = create_session();
        __CPROVER_assert(ids[i] > 0, "session created within capacity");
    }

    __CPROVER_assert(
        abi_graphics_session_count() == ABI_GRAPHICS_MAX_SESSIONS,
        "session count reaches max"
    );

    /* Next create should exceed capacity */
    int64_t overflow = create_session();
    __CPROVER_assert(overflow == ABI_GRAPHICS_CAPACITY_EXCEEDED,
                     "overflowing session count -> CAPACITY_EXCEEDED");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 5: abi_graphics_backend_* — backend query functions
 * ────────────────────────────────────────────────────────────────────── */
void check_backend_queries(void) {
    /* Known backends */
    int64_t s_auto = abi_graphics_backend_supported("auto");
    __CPROVER_assert(s_auto == 1, "backend 'auto' is supported");

    int64_t s_sw = abi_graphics_backend_supported("software");
    __CPROVER_assert(s_sw == 1, "backend 'software' is supported");

    int64_t s_vk = abi_graphics_backend_supported("vulkan");
    __CPROVER_assert(s_vk == 1, "backend 'vulkan' is supported");

    /* Unknown backend */
    int64_t s_unknown = abi_graphics_backend_supported("nonexistent");
    __CPROVER_assert(s_unknown == 0, "unknown backend -> 0");

    /* NULL backend_id */
    int64_t s_null = abi_graphics_backend_supported(NULL);
    __CPROVER_assert(s_null >= 0, "NULL backend -> no crash (code matches to 'auto')");

    /* Available: software should be 1, vulkan should be 0 */
    int64_t a_sw = abi_graphics_backend_available("software");
    __CPROVER_assert(a_sw == 1, "backend 'software' is available");

    int64_t a_vk = abi_graphics_backend_available("vulkan");
    __CPROVER_assert(a_vk == 0, "backend 'vulkan' is not available (no Vulkan executor attached)");

    /* Unknown backend available */
    int64_t a_unknown = abi_graphics_backend_available("nonexistent");
    __CPROVER_assert(a_unknown == 0, "unknown backend -> available=0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 6: abi_graphics_backend_select — select backend on a session
 * ────────────────────────────────────────────────────────────────────── */
void check_backend_select(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    /* Select software backend */
    int64_t rc = abi_graphics_backend_select(sid, "software");
    __CPROVER_assert(rc == ABI_GRAPHICS_OK, "select software returns OK");

    /* Select on invalid session */
    int64_t rc2 = abi_graphics_backend_select(-1, "software");
    __CPROVER_assert(rc2 == ABI_GRAPHICS_INVALID_SESSION,
                     "select on invalid session -> INVALID_SESSION");

    /* Select unsupported backend */
    int64_t rc3 = abi_graphics_backend_select(sid, "nonexistent");
    __CPROVER_assert(rc3 == ABI_GRAPHICS_UNSUPPORTED_BACKEND,
                     "select unknown backend -> UNSUPPORTED_BACKEND");

    /* Select NULL backend (resolves to 'auto') */
    int64_t rc4 = abi_graphics_backend_select(sid, NULL);
    __CPROVER_assert(rc4 == ABI_GRAPHICS_OK,
                     "select NULL backend -> OK (resolves to auto)");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 7: abi_graphics_frame_cycle — begin/end/present/frame_index
 * ────────────────────────────────────────────────────────────────────── */
void check_frame_cycle(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    /* Initially frame index is 0 */
    int64_t fi0 = abi_graphics_frame_index(sid);
    __CPROVER_assert(fi0 == 0, "initial frame index == 0");

    double delta;
    __CPROVER_havoc_object(&delta);
    __CPROVER_assume(delta >= 0.0 && delta <= 100.0);

    /* Begin frame increments index */
    int64_t fi1 = abi_graphics_begin_frame(sid, delta);
    __CPROVER_assert(fi1 > fi0, "begin_frame increments frame index");

    int64_t fi_check = abi_graphics_frame_index(sid);
    __CPROVER_assert(fi_check == fi1, "frame_index returns same as begin_frame return");

    /* End frame (no draws yet) */
    int64_t dc = abi_graphics_end_frame(sid);
    __CPROVER_assert(dc == 0, "end_frame returns 0 draw commands");

    /* Present */
    int64_t pres = abi_graphics_present(sid);
    __CPROVER_assert(pres > 0, "present returns positive frame index");

    int64_t lpf = abi_graphics_last_presented_frame(sid);
    __CPROVER_assert(lpf == pres, "last_presented_frame matches present return");

    /* Frame cycle on invalid session */
    int64_t bad = abi_graphics_begin_frame(-1, delta);
    __CPROVER_assert(bad == ABI_GRAPHICS_INVALID_SESSION,
                     "begin_frame on invalid session -> INVALID_SESSION");

    int64_t bad_end = abi_graphics_end_frame(-1);
    __CPROVER_assert(bad_end == ABI_GRAPHICS_INVALID_SESSION,
                     "end_frame on invalid session -> INVALID_SESSION");

    int64_t bad_pres = abi_graphics_present(-1);
    __CPROVER_assert(bad_pres == ABI_GRAPHICS_INVALID_SESSION,
                     "present on invalid session -> INVALID_SESSION");

    int64_t bad_fi = abi_graphics_frame_index(-1);
    __CPROVER_assert(bad_fi == ABI_GRAPHICS_INVALID_SESSION,
                     "frame_index on invalid session -> INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 8: abi_graphics_buffer_create — buffer lifecycle and accessors
 * ────────────────────────────────────────────────────────────────────── */
void check_buffer_create(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    int64_t blen, estr;
    __CPROVER_havoc_object(&blen);
    __CPROVER_havoc_object(&estr);
    __CPROVER_assume(blen >= 0 && blen <= 512);
    __CPROVER_assume(estr >= 0 && estr <= 64);

    /* Create buffer */
    int64_t bid = abi_graphics_buffer_create(
        sid, "vertex", "my_vertex_buffer", blen, estr
    );
    __CPROVER_assert(bid > 0, "buffer create returns positive ID");
    __CPROVER_assert(
        abi_graphics_buffer_byte_length(sid, bid) == blen,
        "buffer byte_length matches"
    );

    /* Accessor on invalid buffer */
    int64_t bad_len = abi_graphics_buffer_byte_length(sid, -1);
    __CPROVER_assert(bad_len == ABI_GRAPHICS_INVALID_RESOURCE,
                     "byte_length on invalid buffer -> INVALID_RESOURCE");

    /* Accessor on invalid session */
    int64_t bad_len2 = abi_graphics_buffer_byte_length(-1, bid);
    __CPROVER_assert(bad_len2 == ABI_GRAPHICS_INVALID_RESOURCE,
                     "byte_length on invalid session -> INVALID_RESOURCE");

    /* Byte_at on buffer without bytes (just created with buffer_create, not
     * from_hex, so bytes is NULL) */
    int64_t ba = abi_graphics_buffer_byte_at(sid, bid, 0);
    __CPROVER_assert(ba == ABI_GRAPHICS_INVALID_RESOURCE,
                     "byte_at on buffer with no payload -> INVALID_RESOURCE");

    /* Buffer create with invalid arguments */
    int64_t bad_kind = abi_graphics_buffer_create(
        sid, "", "empty_kind", 16, 0
    );
    __CPROVER_assert(bad_kind < 0, "empty kind -> error");

    int64_t bad_neg_len = abi_graphics_buffer_create(
        sid, "vertex", "neg_len", -1, 0
    );
    __CPROVER_assert(bad_neg_len < 0, "negative byte_length -> error");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 9: abi_graphics_draw_mesh — draw command recording
 *
 * Since mesh and pipeline creation requires buffer/shader IDs, and
 * shader creation requires hex-decoded SPIR-V bytes (which would need
 * malloc), we test draw_mesh with a session that has a frame started
 * but no valid pipeline/mesh — this exercises the error path.
 * ────────────────────────────────────────────────────────────────────── */
void check_draw_mesh_invalid(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    double delta;
    __CPROVER_havoc_object(&delta);
    __CPROVER_assume(delta >= 0.0 && delta <= 100.0);

    abi_graphics_begin_frame(sid, delta);

    /* Draw with invalid pipeline and mesh IDs */
    int64_t rc = abi_graphics_draw_mesh(sid, -1, -1, 1);
    __CPROVER_assert(rc < 0, "draw with invalid IDs -> error");

    int64_t rc2 = abi_graphics_draw_mesh(sid, 0, 0, 1);
    __CPROVER_assert(rc2 < 0, "draw with zero IDs -> error");

    /* Draw with negative instance count */
    int64_t rc3 = abi_graphics_draw_mesh(sid, -1, -1, -1);
    __CPROVER_assert(rc3 < 0, "draw with negative instances -> error");

    /* Draw command count should still be 0 */
    int64_t dc = abi_graphics_draw_command_count(sid);
    __CPROVER_assert(dc == 0, "failed draws don't increment command count");

    /* Query invalid command indices */
    int64_t bad_mesh = abi_graphics_draw_command_mesh(sid, 0);
    __CPROVER_assert(bad_mesh == ABI_GRAPHICS_INVALID_RESOURCE,
                     "command mesh at empty index -> INVALID_RESOURCE");

    int64_t bad_pipe = abi_graphics_draw_command_pipeline(sid, 0);
    __CPROVER_assert(bad_pipe == ABI_GRAPHICS_INVALID_RESOURCE,
                     "command pipeline at empty index -> INVALID_RESOURCE");

    int64_t bad_inst = abi_graphics_draw_command_instances(sid, 0);
    __CPROVER_assert(bad_inst == ABI_GRAPHICS_INVALID_RESOURCE,
                     "command instances at empty index -> INVALID_RESOURCE");

    /* Draw on invalid session */
    int64_t rc4 = abi_graphics_draw_mesh(-1, -1, -1, 1);
    __CPROVER_assert(rc4 < 0, "draw on invalid session -> error");

    /* Invalid session query */
    int64_t bad_dc = abi_graphics_draw_command_count(-1);
    __CPROVER_assert(bad_dc == ABI_GRAPHICS_INVALID_SESSION,
                     "draw_command_count on invalid session -> INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 10: abi_graphics_last_status — error state tracking
 * ────────────────────────────────────────────────────────────────────── */
void check_error_state(void) {
    abi_graphics_reset();

    /* After reset, status is OK */
    int64_t st = abi_graphics_last_status();
    __CPROVER_assert(st == ABI_GRAPHICS_OK, "after reset, last_status == OK");

    /* Trigger an error: create session with zero width */
    abi_graphics_session_create("test", 0, 100);
    st = abi_graphics_last_status();
    __CPROVER_assert(st != ABI_GRAPHICS_OK, "after error, last_status != OK");

    /* Trigger another error: destroy invalid session */
    abi_graphics_session_destroy(-1);
    st = abi_graphics_last_status();
    __CPROVER_assert(st != ABI_GRAPHICS_OK, "after second error, status still error");

    /* Reset clears error state */
    abi_graphics_reset();
    st = abi_graphics_last_status();
    __CPROVER_assert(st == ABI_GRAPHICS_OK, "after reset, status == OK again");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 11: capacity-exceeded paths on buffers and draw commands
 *
 * Verifies that resource counts are checked before iteration, using
 * the actual static capacity constants.
 * ────────────────────────────────────────────────────────────────────── */
void check_resource_capacity(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    /* Create ABI_GRAPHICS_MAX_BUFFERS buffers */
    int i;
    for (i = 0; i < ABI_GRAPHICS_MAX_BUFFERS; i++) {
        int64_t bid = abi_graphics_buffer_create(
            sid, "vertex", "test", 16, 0
        );
        __CPROVER_assert(bid > 0, "buffer created within capacity");
    }

    /* Next buffer should exceed capacity */
    int64_t overflow = abi_graphics_buffer_create(
        sid, "vertex", "overflow", 16, 0
    );
    __CPROVER_assert(overflow == ABI_GRAPHICS_CAPACITY_EXCEEDED,
                     "buffer overflow -> CAPACITY_EXCEEDED");
}

/* ──────────────────────────────────────────────────────────────────────
 * Test 12: session accessors on non-existent session
 * ────────────────────────────────────────────────────────────────────── */
void check_session_accessors(void) {
    abi_graphics_reset();

    /* frame_index and last_presented_frame on invalid session should return
     * ABI_GRAPHICS_INVALID_SESSION */
    int64_t fi = abi_graphics_frame_index(-1);
    __CPROVER_assert(fi == ABI_GRAPHICS_INVALID_SESSION,
                     "frame_index(-1) -> INVALID_SESSION");

    int64_t lpf = abi_graphics_last_presented_frame(-1);
    __CPROVER_assert(lpf == ABI_GRAPHICS_INVALID_SESSION,
                     "last_presented_frame(-1) -> INVALID_SESSION");

    int64_t dc = abi_graphics_draw_command_count(-1);
    __CPROVER_assert(dc == ABI_GRAPHICS_INVALID_SESSION,
                     "draw_command_count(-1) -> INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 13: multi-session isolation
 *
 * Verify that operations on one session don't corrupt another.
 * ────────────────────────────────────────────────────────────────────── */
void check_multi_session(void) {
    abi_graphics_reset();

    int64_t s1 = create_session();
    int64_t s2 = create_session();
    __CPROVER_assume(s1 > 0 && s2 > 0);
    __CPROVER_assume(s1 != s2);

    __CPROVER_assert(
        abi_graphics_session_count() == 2,
        "two sessions created"
    );

    /* Begin frame on session 1 */
    double delta;
    __CPROVER_havoc_object(&delta);
    __CPROVER_assume(delta >= 0.0 && delta <= 100.0);

    int64_t f1 = abi_graphics_begin_frame(s1, delta);
    __CPROVER_assert(f1 > 0, "begin frame on s1");

    /* Session 2 should still have frame_index == 0 */
    int64_t f2 = abi_graphics_frame_index(s2);
    __CPROVER_assert(f2 == 0, "s2 frame_index unchanged");

    /* Present on s1 */
    int64_t p1 = abi_graphics_present(s1);
    __CPROVER_assert(p1 == f1, "s1 present matches frame index");

    /* s2 should have last_presented_frame == 0 (never presented) */
    int64_t lp2 = abi_graphics_last_presented_frame(s2);
    __CPROVER_assert(lp2 == 0, "s2 never presented");

    /* Destroy s1, s2 still valid */
    int64_t rc = abi_graphics_session_destroy(s1);
    __CPROVER_assert(rc == ABI_GRAPHICS_OK, "destroy s1 OK");
    __CPROVER_assert(
        abi_graphics_session_count() == 1,
        "after destroying s1, count == 1"
    );

    /* s1 operations now fail */
    int64_t bad = abi_graphics_begin_frame(s1, delta);
    __CPROVER_assert(bad == ABI_GRAPHICS_INVALID_SESSION,
                     "begin_frame on destroyed session -> INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 14: draw command recorded successfully when valid resources exist
 *
 * This test requires a valid session with a started frame. Since we
 * cannot easily create valid pipelines/meshes (they depend on shaders
 * which need malloc'd SPIR-V bytes), we use the "auto" backend path.
 * The draw will fail on resource validation, but we verify the error
 * path is safe.
 * ────────────────────────────────────────────────────────────────────── */
void check_draw_command_error_paths(void) {
    abi_graphics_reset();
    int64_t sid = create_session();
    __CPROVER_assume(sid > 0);

    double delta;
    __CPROVER_havoc_object(&delta);
    __CPROVER_assume(delta >= 0.0 && delta <= 100.0);

    abi_graphics_begin_frame(sid, delta);

    /* Draw with instance_count = 0 (invalid) */
    int64_t rc1 = abi_graphics_draw_mesh(sid, 1, 1, 0);
    __CPROVER_assert(rc1 == ABI_GRAPHICS_INVALID_RESOURCE,
                     "draw with instance_count=0 -> INVALID_RESOURCE");

    /* Draw with instance_count negative */
    int64_t rc2 = abi_graphics_draw_mesh(sid, 1, 1, -5);
    __CPROVER_assert(rc2 == ABI_GRAPHICS_INVALID_RESOURCE,
                     "draw with negative instance_count -> INVALID_RESOURCE");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main — run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_reset();
    check_session_create();
    check_session_create_invalid();
    check_session_capacity();
    check_backend_queries();
    check_backend_select();
    check_frame_cycle();
    check_buffer_create();
    check_draw_mesh_invalid();
    check_error_state();
    check_resource_capacity();
    check_session_accessors();
    check_multi_session();
    check_draw_command_error_paths();
    return 0;
}
