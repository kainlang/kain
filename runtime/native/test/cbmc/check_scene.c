/*
 * check_scene.c -- CBMC verification harness for scene module
 *
 * Covers: scene handle encoding/decoding, mutation/query request/result init,
 * append_hit, descriptor inits, feature support checks, name lookup functions,
 * and runtime_format_feature_mask.
 *
 * Key pattern: static buffers for pointer provenance, __CPROVER_havoc_object
 * for nondet state, __CPROVER_assume for valid ranges.
 *
 * Combined translation unit: scene.c + check_scene.c.
 */

#include "scene.h"

/* Static backing buffers for pointer provenance and output formatting */
static unsigned char g_format_buffer[1024];

/* ======================================================================
 * Static function forward declarations (from scene.c)
 * ====================================================================== */
static unsigned long long scene_pack_handle(
    KainSceneResourceKind kind,
    unsigned int slot,
    unsigned int generation
);
static int runtime_append_text(char* out, size_t out_cap, const char* text);
static int runtime_append_feature_name(
    unsigned long long feature_mask,
    unsigned long long flag,
    const char* name,
    int* emitted_any,
    char* out,
    size_t out_cap
);


/* ======================================================================
 * Factory: create a valid KainSceneHandle
 * ====================================================================== */
static KainSceneHandle create_valid_handle(void) {
    KainSceneHandle h;
    unsigned int slot;
    unsigned int generation;
    KainSceneResourceKind kind;

    __CPROVER_havoc_object(&slot);
    __CPROVER_havoc_object(&generation);
    __CPROVER_havoc_object(&kind);

    /* Constrain to valid ranges */
    __CPROVER_assume(kind > KAIN_SCENE_RESOURCE_UNKNOWN &&
                     kind <= KAIN_SCENE_RESOURCE_PANEL);
    __CPROVER_assume(slot < 0x1000000u);     /* 24-bit slot max */
    __CPROVER_assume(generation < 0x1000000u); /* 24-bit generation */

    h = kain_scene_handle_make(kind, slot, generation);
    return h;
}


/* ======================================================================
 * Check: handle pack/unpack round-trip for all resource kinds
 * ====================================================================== */
static void check_handle_roundtrip(void) {
    KainSceneResourceKind kind;
    unsigned int slot;
    unsigned int generation;

    __CPROVER_havoc_object(&kind);
    __CPROVER_havoc_object(&slot);
    __CPROVER_havoc_object(&generation);

    __CPROVER_assume(kind > KAIN_SCENE_RESOURCE_UNKNOWN &&
                     kind <= KAIN_SCENE_RESOURCE_PANEL);
    __CPROVER_assume(slot < 0x1000000u);
    __CPROVER_assume(generation < 0x1000000u);

    KainSceneHandle h = kain_scene_handle_make(kind, slot, generation);

    __CPROVER_assert(kain_scene_handle_kind(h) == kind,
                     "handle roundtrip: kind preserved");
    __CPROVER_assert(kain_scene_handle_slot(h) == slot,
                     "handle roundtrip: slot preserved");
    __CPROVER_assert(kain_scene_handle_generation(h) == generation,
                     "handle roundtrip: generation preserved");
    __CPROVER_assert(kain_scene_handle_is_valid(h) != 0,
                     "handle roundtrip: valid (non-zero)");
}


/* ======================================================================
 * Check: handle encoding boundary values
 * ====================================================================== */
static void check_handle_boundaries(void) {
    unsigned int slot, generation;

    /* Slot = 0, Generation = 0 -- should still be valid if kind != UNKNOWN */
    __CPROVER_havoc_object(&slot);
    __CPROVER_havoc_object(&generation);
    __CPROVER_assume(slot == 0);
    __CPROVER_assume(generation == 0);

    KainSceneHandle h0 = kain_scene_handle_make(
        KAIN_SCENE_RESOURCE_ENTITY, 0, 0);
    __CPROVER_assert(kain_scene_handle_is_valid(h0) != 0,
                     "boundary: handle(ENTITY,0,0) is valid");
    __CPROVER_assert(kain_scene_handle_slot(h0) == 0,
                     "boundary: slot is 0");
    __CPROVER_assert(kain_scene_handle_generation(h0) == 0,
                     "boundary: generation is 0");

    /* Max slot and generation */
    KainSceneHandle hmax = kain_scene_handle_make(
        KAIN_SCENE_RESOURCE_SCENE, 0xFFFFFFu, 0xFFFFFFu);
    __CPROVER_assert(kain_scene_handle_slot(hmax) == 0xFFFFFFu,
                     "boundary: max slot preserved");
    __CPROVER_assert(kain_scene_handle_generation(hmax) == 0xFFFFFFu,
                     "boundary: max generation preserved");

    /* Zero handle (init) */
    KainSceneHandle hz;
    kain_scene_handle_init(&hz);
    __CPROVER_assert(hz.value == 0ull,
                     "init: handle zeroed");
    __CPROVER_assert(kain_scene_handle_is_valid(hz) == 0,
                     "init: zero handle is NOT valid");
}


/* ======================================================================
 * Check: handle NULL safety
 * ====================================================================== */
static void check_handle_null(void) {
    kain_scene_handle_init(NULL);
    /* Must not crash -- no assertion needed, CBMC proves no UB */
}


/* ======================================================================
 * Check: mutation request init zeros all fields
 * ====================================================================== */
static void check_mutation_request_init(void) {
    KainSceneMutationRequest req;
    __CPROVER_havoc_object(&req);

    kain_scene_mutation_request_init(&req);

    __CPROVER_assert(req.kind == KAIN_SCENE_MUTATION_UNKNOWN,
                     "mutation_request_init: kind == UNKNOWN");
    __CPROVER_assert(req.subject_kind == KAIN_SCENE_RESOURCE_UNKNOWN,
                     "mutation_request_init: subject_kind == UNKNOWN");
    __CPROVER_assert(req.scene.value == 0ull,
                     "mutation_request_init: scene handle == 0");
    __CPROVER_assert(req.transaction_id == 0ull,
                     "mutation_request_init: transaction_id == 0");
    __CPROVER_assert(req.flags == 0u,
                     "mutation_request_init: flags == 0");
    __CPROVER_assert(req.subject_name[0] == '\0',
                     "mutation_request_init: subject_name empty");
    __CPROVER_assert(req.binding_key[0] == '\0',
                     "mutation_request_init: binding_key empty");
}

static void check_mutation_request_null(void) {
    kain_scene_mutation_request_init(NULL);
    /* Must not crash */
}


/* ======================================================================
 * Check: mutation receipt init zeros all fields
 * ====================================================================== */
static void check_mutation_receipt_init(void) {
    KainSceneMutationReceipt rcpt;
    __CPROVER_havoc_object(&rcpt);

    kain_scene_mutation_receipt_init(&rcpt);

    __CPROVER_assert(rcpt.accepted == 0,
                     "receipt_init: accepted == 0");
    __CPROVER_assert(rcpt.status == KAIN_SCENE_MUTATION_STATUS_UNKNOWN,
                     "receipt_init: status == UNKNOWN");
    __CPROVER_assert(rcpt.transaction_id == 0ull,
                     "receipt_init: transaction_id == 0");
    __CPROVER_assert(rcpt.message[0] == '\0',
                     "receipt_init: message empty");
}

static void check_mutation_receipt_null(void) {
    kain_scene_mutation_receipt_init(NULL);
}


/* ======================================================================
 * Check: query request init zeros + sets max_hits
 * ====================================================================== */
static void check_query_request_init(void) {
    KainSceneQueryRequest req;
    __CPROVER_havoc_object(&req);

    kain_scene_query_request_init(&req);

    __CPROVER_assert(req.kind == KAIN_SCENE_QUERY_UNKNOWN,
                     "query_request_init: kind == UNKNOWN");
    __CPROVER_assert(req.scene.value == 0ull,
                     "query_request_init: scene == 0");
    __CPROVER_assert(req.max_hits == SCENE_QUERY_MAX_HITS,
                     "query_request_init: max_hits == SCENE_QUERY_MAX_HITS");
    __CPROVER_assert(req.require_visible == 0,
                     "query_request_init: require_visible == 0");
    __CPROVER_assert(req.max_distance == 0.0,
                     "query_request_init: max_distance == 0.0");
}

static void check_query_request_null(void) {
    kain_scene_query_request_init(NULL);
}


/* ======================================================================
 * Check: query result init zeros all fields
 * ====================================================================== */
static void check_query_result_init(void) {
    KainSceneQueryResult res;
    __CPROVER_havoc_object(&res);

    kain_scene_query_result_init(&res);

    __CPROVER_assert(res.status == KAIN_SCENE_QUERY_STATUS_UNKNOWN,
                     "query_result_init: status == UNKNOWN");
    __CPROVER_assert(res.hit_count == 0,
                     "query_result_init: hit_count == 0");
    __CPROVER_assert(res.primary_hit.value == 0ull,
                     "query_result_init: primary_hit == 0");
    __CPROVER_assert(res.message[0] == '\0',
                     "query_result_init: message empty");
}

static void check_query_result_null(void) {
    kain_scene_query_result_init(NULL);
}


/* ======================================================================
 * Check: append_hit adds hits and tracks primary
 * ====================================================================== */
static void check_append_hit(void) {
    KainSceneQueryResult result;
    KainSceneQueryHit hit;
    int i;

    __CPROVER_havoc_object(&result);
    __CPROVER_havoc_object(&hit);

    /* Start with a valid initialized result */
    kain_scene_query_result_init(&result);

    /* Make the hit nondet but with a valid subject handle */
    KainSceneHandle h = kain_scene_handle_make(
        KAIN_SCENE_RESOURCE_ENTITY, 42, 1);
    hit.subject = h;

    int rc = kain_scene_query_result_append_hit(&result, &hit);

    __CPROVER_assert(rc == 1, "append_hit: first hit succeeds");
    __CPROVER_assert(result.hit_count == 1,
                     "append_hit: hit_count == 1");
    __CPROVER_assert(result.primary_hit.value == h.value,
                     "append_hit: primary_hit set to first hit subject");
    __CPROVER_assert(result.hits[0].subject.value == h.value,
                     "append_hit: hits[0] stored");
}


/* ======================================================================
 * Check: append_hit fills to capacity then fails
 * ====================================================================== */
static void check_append_hit_capacity(void) {
    KainSceneQueryResult result;
    int i;

    kain_scene_query_result_init(&result);

    /* Fill all hits */
    for (i = 0; i < SCENE_QUERY_MAX_HITS; i++) {
        KainSceneQueryHit hit;
        __CPROVER_havoc_object(&hit);
        hit.subject = kain_scene_handle_make(
            KAIN_SCENE_RESOURCE_MESH, (unsigned int)i, 0);

        int rc = kain_scene_query_result_append_hit(&result, &hit);
        __CPROVER_assert(rc == 1,
                         "append_hit_capacity: fill hit succeeds");
    }

    __CPROVER_assert(result.hit_count == SCENE_QUERY_MAX_HITS,
                     "append_hit_capacity: hit_count == max");

    /* Next append must fail */
    KainSceneQueryHit overflow;
    __CPROVER_havoc_object(&overflow);
    int rc2 = kain_scene_query_result_append_hit(&result, &overflow);
    __CPROVER_assert(rc2 == 0,
                     "append_hit_capacity: overflow returns 0");
    __CPROVER_assert(result.hit_count == SCENE_QUERY_MAX_HITS,
                     "append_hit_capacity: hit_count unchanged after overflow");
}


/* ======================================================================
 * Check: append_hit NULL safety
 * ====================================================================== */
static void check_append_hit_null(void) {
    KainSceneQueryResult result;
    kain_scene_query_result_init(&result);

    /* Null result */
    int rc1 = kain_scene_query_result_append_hit(NULL, NULL);
    __CPROVER_assert(rc1 == 0, "append_hit_null: NULL args return 0");

    /* Null hit */
    KainSceneQueryHit hit;
    __CPROVER_havoc_object(&hit);
    int rc2 = kain_scene_query_result_append_hit(&result, NULL);
    __CPROVER_assert(rc2 == 0, "append_hit_null: NULL hit return 0");

    /* Negative hit_count should be rejected */
    result.hit_count = -1;
    hit.subject = kain_scene_handle_make(
        KAIN_SCENE_RESOURCE_CAMERA, 1, 0);
    int rc3 = kain_scene_query_result_append_hit(&result, &hit);
    __CPROVER_assert(rc3 == 0,
                     "append_hit_null: negative hit_count returns 0");
}


/* ======================================================================
 * Check: backend/display/device descriptor inits
 * ====================================================================== */
static void check_descriptor_inits(void) {
    KainRuntimeBackendDescriptor backend;
    KainRuntimeDisplayDescriptor display;
    KainRuntimeDeviceDescriptor device;

    __CPROVER_havoc_object(&backend);
    __CPROVER_havoc_object(&display);
    __CPROVER_havoc_object(&device);

    backend_descriptor_init(&backend);
    display_descriptor_init(&display);
    device_descriptor_init(&device);

    __CPROVER_assert(backend.kind == BACKEND_UNKNOWN,
                     "backend_init: kind == UNKNOWN");
    __CPROVER_assert(backend.feature_mask == 0ull,
                     "backend_init: feature_mask == 0");
    __CPROVER_assert(backend.api_name[0] == '\0',
                     "backend_init: api_name empty");

    __CPROVER_assert(display.width == 0u,
                     "display_init: width == 0");
    __CPROVER_assert(display.height == 0u,
                     "display_init: height == 0");
    __CPROVER_assert(display.is_primary == 0,
                     "display_init: is_primary == 0");

    __CPROVER_assert(device.backend_kind == BACKEND_UNKNOWN,
                     "device_init: backend_kind == UNKNOWN");
    __CPROVER_assert(device.feature_mask == 0ull,
                     "device_init: feature_mask == 0");
    __CPROVER_assert(device.online == 0,
                     "device_init: online == 0");
}

static void check_descriptor_inits_null(void) {
    backend_descriptor_init(NULL);
    display_descriptor_init(NULL);
    device_descriptor_init(NULL);
}


/* ======================================================================
 * Check: backend_supports_feature / device_supports_feature
 * ====================================================================== */
static void check_feature_support(void) {
    KainRuntimeBackendDescriptor backend;
    KainRuntimeDeviceDescriptor device;

    __CPROVER_havoc_object(&backend);
    __CPROVER_havoc_object(&device);

    /* Constrain to valid feature masks */
    __CPROVER_assume(
        backend.feature_mask <=
        (RUNTIME_FEATURE_GRAPHICS | RUNTIME_FEATURE_COMPUTE |
         RUNTIME_FEATURE_PRESENT | RUNTIME_FEATURE_VIEWPORT_INPUT |
         RUNTIME_FEATURE_SCENE_QUERY | RUNTIME_FEATURE_SCENE_MUTATION |
         RUNTIME_FEATURE_RUNTIME_REFLECTION | RUNTIME_FEATURE_INGESTION |
         RUNTIME_FEATURE_HOTPLUG | RUNTIME_FEATURE_PACKAGING));
    __CPROVER_assume(device.feature_mask == backend.feature_mask);

    /* Positive check */
    int bs = backend_supports_feature(&backend, RUNTIME_FEATURE_GRAPHICS);
    int ds = device_supports_feature(&device, RUNTIME_FEATURE_GRAPHICS);
    if (backend.feature_mask & RUNTIME_FEATURE_GRAPHICS) {
        __CPROVER_assert(bs != 0,
                         "backend_supports: GRAPHICS when set");
        __CPROVER_assert(ds != 0,
                         "device_supports: GRAPHICS when set");
    } else {
        __CPROVER_assert(bs == 0,
                         "backend_supports: GRAPHICS when unset");
        __CPROVER_assert(ds == 0,
                         "device_supports: GRAPHICS when unset");
    }

    /* Null descriptor */
    __CPROVER_assert(
        backend_supports_feature(NULL, RUNTIME_FEATURE_COMPUTE) == 0,
        "backend_supports: NULL descriptor returns 0");
    __CPROVER_assert(
        device_supports_feature(NULL, RUNTIME_FEATURE_COMPUTE) == 0,
        "device_supports: NULL descriptor returns 0");

    /* Zero flag */
    __CPROVER_assert(
        backend_supports_feature(&backend, 0ull) == 0,
        "backend_supports: zero flag returns 0");
    __CPROVER_assert(
        device_supports_feature(&device, 0ull) == 0,
        "device_supports: zero flag returns 0");
}


/* ======================================================================
 * Check: all name lookup functions return non-NULL strings
 * ====================================================================== */
static void check_name_lookup_functions(void) {
    /* Resource kind names -- every valid kind has a name */
    __CPROVER_assert(
        kain_scene_resource_kind_name(KAIN_SCENE_RESOURCE_UNKNOWN) != NULL,
        "resource_kind_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        kain_scene_resource_kind_name(KAIN_SCENE_RESOURCE_SCENE) != NULL,
        "resource_kind_name: SCENE returns non-NULL");
    __CPROVER_assert(
        kain_scene_resource_kind_name(KAIN_SCENE_RESOURCE_ENTITY) != NULL,
        "resource_kind_name: ENTITY returns non-NULL");
    __CPROVER_assert(
        kain_scene_resource_kind_name(KAIN_SCENE_RESOURCE_PANEL) != NULL,
        "resource_kind_name: PANEL returns non-NULL");

    /* Mutation kind names */
    __CPROVER_assert(
        kain_scene_mutation_kind_name(KAIN_SCENE_MUTATION_UNKNOWN) != NULL,
        "mutation_kind_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        kain_scene_mutation_kind_name(KAIN_SCENE_MUTATION_CREATE) != NULL,
        "mutation_kind_name: CREATE returns non-NULL");

    /* Mutation status names */
    __CPROVER_assert(
        kain_scene_mutation_status_name(
            KAIN_SCENE_MUTATION_STATUS_UNKNOWN) != NULL,
        "mutation_status_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        kain_scene_mutation_status_name(
            KAIN_SCENE_MUTATION_STATUS_ACCEPTED) != NULL,
        "mutation_status_name: ACCEPTED returns non-NULL");

    /* Query kind names */
    __CPROVER_assert(
        kain_scene_query_kind_name(KAIN_SCENE_QUERY_UNKNOWN) != NULL,
        "query_kind_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        kain_scene_query_kind_name(KAIN_SCENE_QUERY_PICK) != NULL,
        "query_kind_name: PICK returns non-NULL");

    /* Query status names */
    __CPROVER_assert(
        kain_scene_query_status_name(KAIN_SCENE_QUERY_STATUS_UNKNOWN) != NULL,
        "query_status_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        kain_scene_query_status_name(KAIN_SCENE_QUERY_STATUS_OK) != NULL,
        "query_status_name: OK returns non-NULL");

    /* Backend kind names */
    __CPROVER_assert(
        backend_kind_name(BACKEND_UNKNOWN) != NULL,
        "backend_kind_name: UNKNOWN returns non-NULL");
    __CPROVER_assert(
        backend_kind_name(BACKEND_VULKAN) != NULL,
        "backend_kind_name: VULKAN returns non-NULL");
    __CPROVER_assert(
        backend_kind_name(BACKEND_D3D12) != NULL,
        "backend_kind_name: D3D12 returns non-NULL");
}


/* ======================================================================
 * Check: name lookup functions return distinct strings
 * ====================================================================== */
static void check_name_lookup_distinct(void) {
    /* Verify that different kinds return different names */
    const char* scene_name = kain_scene_resource_kind_name(
        KAIN_SCENE_RESOURCE_SCENE);
    const char* entity_name = kain_scene_resource_kind_name(
        KAIN_SCENE_RESOURCE_ENTITY);
    __CPROVER_assert(scene_name != entity_name,
                     "distinct: scene != entity");

    const char* create_name = kain_scene_mutation_kind_name(
        KAIN_SCENE_MUTATION_CREATE);
    const char* delete_name = kain_scene_mutation_kind_name(
        KAIN_SCENE_MUTATION_DELETE);
    __CPROVER_assert(create_name != delete_name,
                     "distinct: create != delete");
}


/* ======================================================================
 * Check: runtime_format_feature_mask produces well-formed output
 * ====================================================================== */
static void check_format_feature_mask(void) {
    char* out = (char*)g_format_buffer;
    size_t cap = sizeof(g_format_buffer);
    unsigned long long feature_mask;

    __CPROVER_havoc_object(&feature_mask);
    __CPROVER_assume(
        feature_mask <=
        (RUNTIME_FEATURE_GRAPHICS | RUNTIME_FEATURE_COMPUTE |
         RUNTIME_FEATURE_PRESENT | RUNTIME_FEATURE_VIEWPORT_INPUT |
         RUNTIME_FEATURE_SCENE_QUERY | RUNTIME_FEATURE_SCENE_MUTATION |
         RUNTIME_FEATURE_RUNTIME_REFLECTION | RUNTIME_FEATURE_INGESTION |
         RUNTIME_FEATURE_HOTPLUG | RUNTIME_FEATURE_PACKAGING));

    int rc = runtime_format_feature_mask(feature_mask, out, cap);

    /* rc is strlen(out) -- output must be null-terminated */
    __CPROVER_assert(rc >= 0, "format: return >= 0 (strlen)");
    __CPROVER_assert(rc < (int)cap,
                     "format: return < cap");
    __CPROVER_assert(out[cap - 1] == '\0',
                     "format: output null-terminated (buffer safety)");
    __CPROVER_assert(out[0] != '\0',
                     "format: output non-empty (either features or 'none')");
}


/* ======================================================================
 * Check: runtime_format_feature_mask NULL/small-buffer safety
 * ====================================================================== */
static void check_format_feature_mask_edges(void) {
    /* NULL output */
    int rc1 = runtime_format_feature_mask(RUNTIME_FEATURE_GRAPHICS, NULL, 0);
    __CPROVER_assert(rc1 == 0, "format_edges: NULL out returns 0");

    /* Zero capacity */
    int rc2 = runtime_format_feature_mask(
        RUNTIME_FEATURE_GRAPHICS, (char*)g_format_buffer, 0);
    __CPROVER_assert(rc2 == 0, "format_edges: zero cap returns 0");

    /* Single byte output -- just enough for null terminator */
    char tiny[1];
    tiny[0] = 0xFF;
    int rc3 = runtime_format_feature_mask(
        RUNTIME_FEATURE_GRAPHICS, tiny, sizeof(tiny));
    __CPROVER_assert(rc3 == 0, "format_edges: tiny buf returns 0");
    __CPROVER_assert(tiny[0] == '\0',
                     "format_edges: tiny buf null-terminated");
}


/* ======================================================================
 * Check: scene_pack_handle (static) packs fields correctly
 * ====================================================================== */
static void check_scene_pack_handle(void) {
    KainSceneResourceKind kind;
    unsigned int slot;
    unsigned int generation;

    __CPROVER_havoc_object(&kind);
    __CPROVER_havoc_object(&slot);
    __CPROVER_havoc_object(&generation);

    __CPROVER_assume(kind > KAIN_SCENE_RESOURCE_UNKNOWN &&
                     kind <= KAIN_SCENE_RESOURCE_PANEL);
    __CPROVER_assume(slot < 0x1000000u);
    __CPROVER_assume(generation < 0x1000000u);

    unsigned long long packed = scene_pack_handle(kind, slot, generation);

    /* Extract fields back */
    KainSceneResourceKind got_kind =
        (KainSceneResourceKind)((packed >> 56) & 0xffull);
    unsigned int got_generation =
        (unsigned int)((packed >> 32) & 0x00ffffffull);
    unsigned int got_slot =
        (unsigned int)(packed & 0xffffffffull);

    __CPROVER_assert(got_kind == kind,
                     "scene_pack: kind extracted");
    __CPROVER_assert(got_slot == slot,
                     "scene_pack: slot extracted");
    __CPROVER_assert(got_generation == generation,
                     "scene_pack: generation extracted");
}


/* ======================================================================
 * Check: runtime_append_text (static) appends within bounds
 * ====================================================================== */
static void check_runtime_append_text(void) {
    char* buf = (char*)g_format_buffer;
    size_t cap = sizeof(g_format_buffer);

    buf[0] = '\0';

    int rc = runtime_append_text(buf, cap, "hello");

    __CPROVER_assert(rc == 1, "append_text: first append succeeds");
    __CPROVER_assert(strcmp(buf, "hello") == 0,
                     "append_text: content correct");

    rc = runtime_append_text(buf, cap, "-world");
    __CPROVER_assert(rc == 1, "append_text: second append succeeds");
    __CPROVER_assert(strcmp(buf, "hello-world") == 0,
                     "append_text: concatenated content");
}


/* ======================================================================
 * Main -- run all scene checks
 * ====================================================================== */
int main(void) {
    check_handle_roundtrip();
    check_handle_boundaries();
    check_handle_null();
    check_mutation_request_init();
    check_mutation_request_null();
    check_mutation_receipt_init();
    check_mutation_receipt_null();
    check_query_request_init();
    check_query_request_null();
    check_query_result_init();
    check_query_result_null();
    check_append_hit();
    check_append_hit_capacity();
    check_append_hit_null();
    check_descriptor_inits();
    check_descriptor_inits_null();
    check_feature_support();
    check_name_lookup_functions();
    check_name_lookup_distinct();
    check_format_feature_mask();
    check_format_feature_mask_edges();
    check_scene_pack_handle();
    check_runtime_append_text();
    return 0;
}
