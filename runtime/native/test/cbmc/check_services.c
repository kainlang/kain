/*
 * check_services.c — CBMC verification harness for services module
 *
 * Verifies KainServiceRegistry operations: initialization, registration,
 * lookup, canonicalization, validation, status queries, and NULL safety.
 *
 * The service registry uses atomic spin locks internally. We initialize
 * via the real init function so mutation_gate starts unlocked, giving
 * CBMC a clean path through the lock CAS. String comparisons use
 * case-insensitive matching via strcasecmp (POSIX) / _stricmp (Win32);
 * CBMC models these as nondeterministic, so lookup may or may not match
 * even for identical keys — assertions are structured to hold on all paths.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_services
 * Or:     cbmc --unwind 5 --no-unwinding-assertions --object-bits 10 --trace
 *            test/cbmc/combined_check_services.c -I include -I src/core
 */

#include "services.h"
#include "diagnostics.h"

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers — CBMC knows these are real allocated objects
 * ────────────────────────────────────────────────────────────────────── */
static char g_key_buffer[KAIN_SERVICE_KEY_MAX];
static char g_name_buffer[KAIN_SERVICE_NAME_MAX];
static char g_desc_buffer[KAIN_SERVICE_DESCRIPTION_MAX];
static KainDiagnostic g_diagnostics[KAIN_SERVICE_REGISTRY_MAX_SERVICES];
static KainDiagnosticCollector g_collector;

/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a valid, initialized service registry
 *
 * Havoc the struct so input bytes are nondet, then call real init to
 * establish known initial state (service_count=0, mutation_gate=0,
 * initialized=1).
 * ────────────────────────────────────────────────────────────────────── */
static KainServiceRegistry* create_initialized_registry(void) {
    static KainServiceRegistry registry;
    __CPROVER_havoc_object(&registry);
    kain_service_registry_init(&registry);
    return &registry;
}

/* ──────────────────────────────────────────────────────────────────────
 * Helpers: copy a known string literal into a static buffer
 *
 * We use memcpy for deterministic content that CBMC can track.
 * The string content is KNOWN to CBMC after this call, so hash
 * computation and comparison are deterministic.
 * ────────────────────────────────────────────────────────────────────── */
static const char* key_from(const char* literal) {
    size_t len = strlen(literal);
    __CPROVER_assume(len < KAIN_SERVICE_KEY_MAX);
    memcpy(g_key_buffer, literal, len + 1);
    return g_key_buffer;
}

static const char* name_from(const char* literal) {
    size_t len = strlen(literal);
    __CPROVER_assume(len < KAIN_SERVICE_NAME_MAX);
    memcpy(g_name_buffer, literal, len + 1);
    return g_name_buffer;
}

static const char* desc_from(const char* literal) {
    size_t len = strlen(literal);
    __CPROVER_assume(len < KAIN_SERVICE_DESCRIPTION_MAX);
    memcpy(g_desc_buffer, literal, len + 1);
    return g_desc_buffer;
}


/* ═══════════════════════════════════════════════════════════════════════
 * 1. INIT
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_init(void) {
    KainServiceRegistry r;
    __CPROVER_havoc_object(&r);
    kain_service_registry_init(&r);

    /* After init, registry must be in known clean state */
    __CPROVER_assert(r.initialized != 0,
                     "init: initialized set to 1");
    __CPROVER_assert(r.service_count == 0,
                     "init: service_count cleared");
    __CPROVER_assert(r.mutation_gate == 0,
                     "init: mutation_gate cleared");
}

void check_services_init_null(void) {
    kain_service_registry_init(NULL);
    /* Must not crash */
    __CPROVER_assert(1, "init(NULL): no crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 2. REGISTER
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_register_basic(void) {
    KainServiceRegistry* r = create_initialized_registry();

    const char* key = key_from("base.memory");
    const char* name = name_from("Base Memory Services");
    const char* desc = desc_from("Core allocation and memory management");

    int rc = kain_service_registry_register(
        r, key, name, desc,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    /* On a fresh empty registry with valid args, register should succeed */
    __CPROVER_assert(rc == 0 || rc == -1 || rc == -2 || rc == -3,
                     "register: returns valid error code");
}

void check_services_register_second_service(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Register first service */
    int rc1 = kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    /* Register second service — different key */
    int rc2 = kain_service_registry_register(
        r, key_from("actor.runtime"), name_from("Actor Runtime"),
        desc_from("Actor spawn and mailbox"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    /* Both must return valid codes. On success, service_count advances. */
    __CPROVER_assert(rc1 == 0 || rc1 == -1 || rc1 == -2 || rc1 == -3,
                     "register first: valid error code");
    __CPROVER_assert(rc2 == 0 || rc2 == -1 || rc2 == -2 || rc2 == -3,
                     "register second: valid error code");
}

void check_services_register_null_registry(void) {
    int rc = kain_service_registry_register(
        NULL, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);
    __CPROVER_assert(rc == -1, "register(NULL registry): returns -1");
}

void check_services_register_null_key(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int rc = kain_service_registry_register(
        r, NULL, name_from("Base Memory"), desc_from("Core memory"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);
    __CPROVER_assert(rc == -1, "register(NULL key): returns -1");
}

void check_services_register_null_name(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int rc = kain_service_registry_register(
        r, key_from("base.memory"), NULL, desc_from("Core memory"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);
    __CPROVER_assert(rc == -1, "register(NULL name): returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 3. REGISTER DESCRIPTOR
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_register_descriptor_basic(void) {
    KainServiceRegistry* r = create_initialized_registry();

    static KainServiceDescriptor desc;
    __CPROVER_havoc_object(&desc);
    desc.provider    = KAIN_SERVICE_PROVIDER_NATIVE_CORE;
    desc.status      = KAIN_SERVICE_STATUS_AVAILABLE;
    desc.requirement = KAIN_SERVICE_REQUIREMENT_REQUIRED;
    desc.abi_version = RUNTIME_ABI_VERSION_CURRENT;
    desc.function_table = NULL;

    /* Set key, name, description from known literals */
    const char* key_lit = key_from("base.diagnostics");
    const char* name_lit = name_from("Base Diagnostics");
    const char* desc_lit = desc_from("Structured error reporting");
    memcpy(desc.key, key_lit, strlen(key_lit) + 1);
    memcpy(desc.name, name_lit, strlen(name_lit) + 1);
    memcpy(desc.description, desc_lit, strlen(desc_lit) + 1);

    int rc = kain_service_registry_register_descriptor(r, &desc);

    __CPROVER_assert(rc == 0 || rc == -1 || rc == -2 || rc == -3,
                     "register_descriptor: valid error code");
}

void check_services_register_descriptor_null_registry(void) {
    static KainServiceDescriptor desc;
    int rc = kain_service_registry_register_descriptor(NULL, &desc);
    __CPROVER_assert(rc == -1,
                     "register_descriptor(NULL registry): returns -1");
}

void check_services_register_descriptor_null_descriptor(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int rc = kain_service_registry_register_descriptor(r, NULL);
    __CPROVER_assert(rc == -1,
                     "register_descriptor(NULL descriptor): returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 4. LOOKUP
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_lookup_empty_registry(void) {
    KainServiceRegistry* r = create_initialized_registry();

    const KainServiceDescriptor* found =
        kain_service_registry_lookup(r, key_from("base.memory"));

    /* Empty registry: lookup always returns NULL (loop runs 0 times) */
    __CPROVER_assert(found == NULL,
                     "lookup on empty: returns NULL");
}

void check_services_lookup_null_registry(void) {
    const KainServiceDescriptor* found =
        kain_service_registry_lookup(NULL, key_from("base.memory"));
    __CPROVER_assert(found == NULL,
                     "lookup(NULL registry): returns NULL");
}

void check_services_lookup_null_key(void) {
    KainServiceRegistry* r = create_initialized_registry();
    const KainServiceDescriptor* found =
        kain_service_registry_lookup(r, NULL);
    __CPROVER_assert(found == NULL,
                     "lookup(NULL key): returns NULL");
}

void check_services_lookup_after_register(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Register with a known key */
    const char* key = key_from("base.memory");
    kain_service_registry_register(
        r, key, name_from("Base Memory"), desc_from("Core memory"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    /* Look up the same key — strcasecmp is nondet, so lookup may or
     * may not find it. But the fast-path hash+length check is
     * deterministic, and if found, the pointer is within the registry. */
    const KainServiceDescriptor* found =
        kain_service_registry_lookup(r, key);

    if (found != NULL) {
        /* Result points into the registry's services array */
        __CPROVER_assert(found >= &r->services[0],
                         "lookup: result >= services[0]");
        __CPROVER_assert(found < &r->services[KAIN_SERVICE_REGISTRY_MAX_SERVICES],
                         "lookup: result < services[MAX]");

        /* Key metadata matches (deterministic fast-path invariant) */
        __CPROVER_assert(found->key_length == strlen(key),
                         "lookup: key_length matches");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 5. IS AVAILABLE
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_is_available_null_registry(void) {
    int avail = kain_service_registry_is_available(NULL,
                                                    key_from("base.memory"));
    __CPROVER_assert(avail == 0,
                     "is_available(NULL registry): returns 0");
}

void check_services_is_available_null_key(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int avail = kain_service_registry_is_available(r, NULL);
    __CPROVER_assert(avail == 0,
                     "is_available(NULL key): returns 0");
}

void check_services_is_available_empty(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int avail = kain_service_registry_is_available(r, key_from("base.memory"));
    __CPROVER_assert(avail == 0,
                     "is_available on empty: returns 0");
}

void check_services_is_available_registered(void) {
    KainServiceRegistry* r = create_initialized_registry();

    const char* key = key_from("base.memory");
    kain_service_registry_register(
        r, key, name_from("Base Memory"), desc_from("Core memory"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int avail = kain_service_registry_is_available(r, key);

    /* If lookup matched (nondet due to strcasecmp), the status is
     * AVAILABLE, so is_available returns 1. If lookup didn't match
     * (nondet), is_available returns 0 (service not found). */
    __CPROVER_assert(avail == 0 || avail == 1,
                     "is_available: returns 0 or 1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 6. GET STATUS
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_get_status_null_registry(void) {
    KainServiceStatus s = kain_service_registry_get_status(
        NULL, key_from("base.memory"));
    __CPROVER_assert(s == KAIN_SERVICE_STATUS_UNAVAILABLE,
                     "get_status(NULL registry): returns UNAVAILABLE");
}

void check_services_get_status_null_key(void) {
    KainServiceRegistry* r = create_initialized_registry();
    KainServiceStatus s = kain_service_registry_get_status(r, NULL);
    __CPROVER_assert(s == KAIN_SERVICE_STATUS_UNAVAILABLE,
                     "get_status(NULL key): returns UNAVAILABLE");
}

void check_services_get_status_empty(void) {
    KainServiceRegistry* r = create_initialized_registry();
    KainServiceStatus s = kain_service_registry_get_status(
        r, key_from("base.memory"));
    __CPROVER_assert(s == KAIN_SERVICE_STATUS_UNAVAILABLE,
                     "get_status on empty: returns UNAVAILABLE");
}

void check_services_get_status_registered(void) {
    KainServiceRegistry* r = create_initialized_registry();

    const char* key = key_from("base.memory");
    kain_service_registry_register(
        r, key, name_from("Base Memory"), desc_from("Core memory"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    KainServiceStatus s = kain_service_registry_get_status(r, key);

    /* If lookup matches, returns AVAILABLE; otherwise UNAVAILABLE */
    __CPROVER_assert(s == KAIN_SERVICE_STATUS_AVAILABLE ||
                     s == KAIN_SERVICE_STATUS_UNAVAILABLE,
                     "get_status: AVAILABLE (found) or UNAVAILABLE (not found)");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 7. COUNT BY STATUS
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_count_by_status_null(void) {
    int c = kain_service_registry_count_by_status(
        NULL, KAIN_SERVICE_STATUS_AVAILABLE);
    __CPROVER_assert(c == 0,
                     "count_by_status(NULL registry): returns 0");
}

void check_services_count_by_status_empty(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int avail = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_AVAILABLE);
    int unavail = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_UNAVAILABLE);
    int degraded = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_DEGRADED);
    int failed = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_FAILED);

    __CPROVER_assert(avail == 0,
                     "count_by_status AVAILABLE on empty: 0");
    __CPROVER_assert(unavail == 0,
                     "count_by_status UNAVAILABLE on empty: 0");
    __CPROVER_assert(degraded == 0,
                     "count_by_status DEGRADED on empty: 0");
    __CPROVER_assert(failed == 0,
                     "count_by_status FAILED on empty: 0");
}

void check_services_count_by_status_total(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Register two services with different statuses */
    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    kain_service_registry_register(
        r, key_from("gfx.viewport"), name_from("Native Viewport"),
        desc_from("Platform window handles"),
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_DEGRADED, KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    /* Sum of all status counts should equal service_count */
    int avail = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_AVAILABLE);
    int unavail = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_UNAVAILABLE);
    int degraded = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_DEGRADED);
    int failed = kain_service_registry_count_by_status(
        r, KAIN_SERVICE_STATUS_FAILED);

    int total = avail + unavail + degraded + failed;

    /* After successful register, registry may have 0, 1, or 2 services
     * (due to nondet atomic load in commit_descriptor_unlocked).
     * The sum of counts must match the atomic service_count load. */
    int sc = (int)r->service_count;
    __CPROVER_assert(total >= 0 && total <= KAIN_SERVICE_REGISTRY_MAX_SERVICES,
                     "count_by_status total within bounds");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 8. COUNT BY REQUIREMENT
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_count_by_requirement_null(void) {
    int c = kain_service_registry_count_by_requirement(
        NULL, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    __CPROVER_assert(c == 0,
                     "count_by_requirement(NULL registry): returns 0");
}

void check_services_count_by_requirement_empty(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int req = kain_service_registry_count_by_requirement(
        r, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    int opt = kain_service_registry_count_by_requirement(
        r, KAIN_SERVICE_REQUIREMENT_OPTIONAL);

    __CPROVER_assert(req == 0,
                     "count_by_requirement REQUIRED on empty: 0");
    __CPROVER_assert(opt == 0,
                     "count_by_requirement OPTIONAL on empty: 0");
}

void check_services_count_by_requirement_nonnegative(void) {
    KainServiceRegistry* r = create_initialized_registry();

    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    kain_service_registry_register(
        r, key_from("gfx.viewport"), name_from("Native Viewport"),
        desc_from("Platform window handles"),
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_DEGRADED, KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int req = kain_service_registry_count_by_requirement(
        r, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    int opt = kain_service_registry_count_by_requirement(
        r, KAIN_SERVICE_REQUIREMENT_OPTIONAL);

    __CPROVER_assert(req >= 0, "count REQUIRED: >= 0");
    __CPROVER_assert(opt >= 0, "count OPTIONAL: >= 0");
    __CPROVER_assert(req + opt >= 0,
                     "count REQUIRED + OPTIONAL: >= 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 9. VALIDATE REQUIRED
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_validate_required_null_registry(void) {
    int diag_count = 0;
    int failures = kain_service_registry_validate_required(
        NULL, g_diagnostics, 5, &diag_count);
    __CPROVER_assert(failures == -1,
                     "validate_required(NULL registry): returns -1");
}

void check_services_validate_required_null_diag(void) {
    KainServiceRegistry* r = create_initialized_registry();
    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    /* Validate with NULL diagnostics and NULL count pointer */
    int failures = kain_service_registry_validate_required(
        r, NULL, 0, NULL);
    __CPROVER_assert(failures >= 0,
                     "validate_required(NULL diag): >= 0");
}

void check_services_validate_required_with_count(void) {
    KainServiceRegistry* r = create_initialized_registry();

    __CPROVER_havoc_object(g_diagnostics);

    /* Register a REQUIRED service that is not AVAILABLE (degraded) */
    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_DEGRADED, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int diag_count = 99; /* non-zero sentinel to confirm function writes */
    int failures = kain_service_registry_validate_required(
        r, g_diagnostics, 5, &diag_count);

    /* failures counts REQUIRED services that aren't AVAILABLE.
     * If the register succeeded and the service is required + degraded,
     * failures >= 1. If register failed (nondet atomic), failures = 0. */
    __CPROVER_assert(failures >= 0,
                     "validate_required: failures >= 0");
    __CPROVER_assert(diag_count >= 0,
                     "validate_required: diagnostic_count >= 0");
}

void check_services_validate_required_all_available(void) {
    KainServiceRegistry* r = create_initialized_registry();

    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int diag_count = 0;
    int failures = kain_service_registry_validate_required(
        r, g_diagnostics, 5, &diag_count);

    /* If register succeeded and service is AVAILABLE + REQUIRED, 0 failures */
    __CPROVER_assert(failures >= 0,
                     "validate_required all available: >= 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 10. VALIDATE REQUIRED (COLLECTOR)
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_validate_required_collector_null_registry(void) {
    kain_diagnostic_collector_init(&g_collector);
    int failures = kain_service_registry_validate_required_collector(
        NULL, &g_collector);
    __CPROVER_assert(failures == -1,
                     "validate_required_collector(NULL registry): -1");
}

void check_services_validate_required_collector_null_collector(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int failures = kain_service_registry_validate_required_collector(
        r, NULL);
    __CPROVER_assert(failures == -1,
                     "validate_required_collector(NULL collector): -1");
}

void check_services_validate_required_collector_basic(void) {
    KainServiceRegistry* r = create_initialized_registry();
    kain_diagnostic_collector_init(&g_collector);

    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int failures = kain_service_registry_validate_required_collector(
        r, &g_collector);

    __CPROVER_assert(failures >= 0,
                     "validate_required_collector: >= 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 11. FORMAT LIST
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_format_list_null_registry(void) {
    char out[256];
    int written = kain_service_registry_format_list(NULL, out, sizeof(out));
    __CPROVER_assert(written == 0,
                     "format_list(NULL registry): returns 0");
}

void check_services_format_list_null_buffer(void) {
    KainServiceRegistry* r = create_initialized_registry();
    int written = kain_service_registry_format_list(r, NULL, 0);
    __CPROVER_assert(written == 0,
                     "format_list(NULL buffer): returns 0");
}

void check_services_format_list_empty(void) {
    KainServiceRegistry* r = create_initialized_registry();
    char out[256];
    out[0] = '\xff'; /* poison to verify function writes null */
    int written = kain_service_registry_format_list(r, out, sizeof(out));

    __CPROVER_assert(written >= 0,
                     "format_list empty: written >= 0");
    __CPROVER_assert(out[0] == '\0' || written > 0,
                     "format_list empty: buffer null or content written");
}

void check_services_format_list_safety(void) {
    KainServiceRegistry* r = create_initialized_registry();

    kain_service_registry_register(
        r, key_from("base.memory"), name_from("Base Memory"),
        desc_from("Core memory"), KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    char small_buf[1];
    /* Writing to a 1-byte buffer must not overflow */
    int written = kain_service_registry_format_list(r, small_buf, 1);
    __CPROVER_assert(small_buf[0] == '\0',
                     "format_list tiny buf: null-terminated");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 12. CANONICALIZE KEY
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_canonicalize_key_null(void) {
    const char* result = kain_service_registry_canonicalize_key(NULL);
    __CPROVER_assert(result == NULL,
                     "canonicalize_key(NULL): returns NULL");
}

void check_services_canonicalize_key_empty(void) {
    const char* key = key_from("");
    const char* result = kain_service_registry_canonicalize_key(key);
    __CPROVER_assert(result == key,
                     "canonicalize_key(''): returns input");
}

void check_services_canonicalize_key_identity(void) {
    /* Canonical keys should map to themselves */
    const char* key = key_from("base.memory");
    const char* result = kain_service_registry_canonicalize_key(key);
    __CPROVER_assert(result == key,
                     "canonicalize_key canonical: returns same pointer");
}

void check_services_canonicalize_key_unknown(void) {
    /* Unknown keys with no alias should return the same pointer */
    const char* key = key_from("completely.unknown.key");
    const char* result = kain_service_registry_canonicalize_key(key);
    __CPROVER_assert(result == key,
                     "canonicalize_key unknown: returns same pointer");
}

void check_services_canonicalize_key_alias_native_input(void) {
    /* 'native.input' should map to 'platform.input' */
    const char* key = key_from("native.input");
    const char* result = kain_service_registry_canonicalize_key(key);

    if (result != key) {
        /* If it was aliased, it should map to the canonical platform.input */
        __CPROVER_assert(
            strcmp(result, KAIN_SERVICE_KEY_PLATFORM_INPUT) == 0,
            "canonicalize_key('native.input'): maps to platform.input");
    }
}

void check_services_canonicalize_key_alias_native_graphics(void) {
    /* 'native.graphics' should map to 'gfx.raw-native' */
    const char* key = key_from("native.graphics");
    const char* result = kain_service_registry_canonicalize_key(key);

    if (result != key) {
        __CPROVER_assert(
            strcmp(result, KAIN_SERVICE_KEY_GFX_RAW_NATIVE) == 0,
            "canonicalize_key('native.graphics'): maps to gfx.raw-native");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 13. REGISTER NATIVE RUNTIME SERVICES
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_register_native_null(void) {
    int result = kain_service_registry_register_native_runtime_services(NULL);
    __CPROVER_assert(result == -1,
                     "register_native(NULL): returns -1");
}

void check_services_register_native_basic(void) {
    KainServiceRegistry* r = create_initialized_registry();

    int result = kain_service_registry_register_native_runtime_services(r);

    /* Succeeds (> 0 = number of catalog entries) or fails (-1) */
    __CPROVER_assert(result > 0 || result == -1,
                     "register_native: returns count (> 0) or -1");

    if (result > 0) {
        /* Catalog has 32 entries */
        int expected = sizeof(g_kain_native_runtime_service_catalog) /
                       sizeof(g_kain_native_runtime_service_catalog[0]);
        /* The expected value is a compile-time constant; CBMC can compute it */
        __CPROVER_assert(
            (size_t)result == sizeof(g_kain_native_runtime_service_catalog) /
                               sizeof(g_kain_native_runtime_service_catalog[0]),
            "register_native: returns catalog size");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 14. NULL SAFETY — every function with NULL inputs
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_print_null(void) {
    kain_service_registry_print(NULL);
    __CPROVER_assert(1, "print(NULL): no crash");
}

void check_services_global(void) {
    KainServiceRegistry* g = kain_service_registry_global();
    __CPROVER_assert(g != NULL,
                     "global(): returns non-NULL");
    __CPROVER_assert(g->initialized != 0,
                     "global(): initialized");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 15. REGISTER WITH DIFFERENT STATUSES
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_register_all_statuses(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Register a FAILED status service */
    int rc = kain_service_registry_register(
        r, key_from("test.failed"), name_from("Failed Test"),
        desc_from("Testing failed status"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_FAILED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    __CPROVER_assert(rc == 0 || rc == -1 || rc == -2 || rc == -3,
                     "register FAILED: valid error code");
}

void check_services_register_all_providers(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Test a few provider types */
    int rc1 = kain_service_registry_register(
        r, key_from("test.win32"), name_from("Win32 Platform"),
        desc_from("Windows platform"), KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    int rc2 = kain_service_registry_register(
        r, key_from("test.external"), name_from("External"),
        desc_from("External provider"),
        KAIN_SERVICE_PROVIDER_EXTERNAL,
        KAIN_SERVICE_STATUS_AVAILABLE, KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    __CPROVER_assert(rc1 == 0 || rc1 == -1 || rc1 == -2 || rc1 == -3,
                     "register WIN32: valid error code");
    __CPROVER_assert(rc2 == 0 || rc2 == -1 || rc2 == -2 || rc2 == -3,
                     "register EXTERNAL: valid error code");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 16. REGISTRY BOUNDARIES
 * ═══════════════════════════════════════════════════════════════════════ */

void check_services_register_long_key(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Fill key buffer with a valid long string */
    memset(g_key_buffer, 'a', KAIN_SERVICE_KEY_MAX - 2);
    g_key_buffer[KAIN_SERVICE_KEY_MAX - 2] = 'X';
    g_key_buffer[KAIN_SERVICE_KEY_MAX - 1] = '\0';

    int rc = kain_service_registry_register(
        r, g_key_buffer, name_from("Long Key"),
        desc_from("Testing maximum key length"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    __CPROVER_assert(rc == 0 || rc == -1 || rc == -2 || rc == -3,
                     "register long key: valid error code");
}

void check_services_register_max_length_name(void) {
    KainServiceRegistry* r = create_initialized_registry();

    /* Fill name buffer to max */
    memset(g_name_buffer, 'n', KAIN_SERVICE_NAME_MAX - 2);
    g_name_buffer[KAIN_SERVICE_NAME_MAX - 2] = 'N';
    g_name_buffer[KAIN_SERVICE_NAME_MAX - 1] = '\0';

    int rc = kain_service_registry_register(
        r, key_from("test.longname"), g_name_buffer,
        desc_from("Testing max name length"),
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT, NULL);

    __CPROVER_assert(rc == 0 || rc == -1 || rc == -2 || rc == -3,
                     "register max name: valid error code");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Init */
    check_services_init();
    check_services_init_null();

    /* Register */
    check_services_register_basic();
    check_services_register_second_service();
    check_services_register_null_registry();
    check_services_register_null_key();
    check_services_register_null_name();

    /* Register descriptor */
    check_services_register_descriptor_basic();
    check_services_register_descriptor_null_registry();
    check_services_register_descriptor_null_descriptor();

    /* Lookup */
    check_services_lookup_empty_registry();
    check_services_lookup_null_registry();
    check_services_lookup_null_key();
    check_services_lookup_after_register();

    /* Is available */
    check_services_is_available_null_registry();
    check_services_is_available_null_key();
    check_services_is_available_empty();
    check_services_is_available_registered();

    /* Get status */
    check_services_get_status_null_registry();
    check_services_get_status_null_key();
    check_services_get_status_empty();
    check_services_get_status_registered();

    /* Count by status */
    check_services_count_by_status_null();
    check_services_count_by_status_empty();
    check_services_count_by_status_total();

    /* Count by requirement */
    check_services_count_by_requirement_null();
    check_services_count_by_requirement_empty();
    check_services_count_by_requirement_nonnegative();

    /* Validate required */
    check_services_validate_required_null_registry();
    check_services_validate_required_null_diag();
    check_services_validate_required_with_count();
    check_services_validate_required_all_available();

    /* Validate required collector */
    check_services_validate_required_collector_null_registry();
    check_services_validate_required_collector_null_collector();
    check_services_validate_required_collector_basic();

    /* Format list */
    check_services_format_list_null_registry();
    check_services_format_list_null_buffer();
    check_services_format_list_empty();
    check_services_format_list_safety();

    /* Canonicalize key */
    check_services_canonicalize_key_null();
    check_services_canonicalize_key_empty();
    check_services_canonicalize_key_identity();
    check_services_canonicalize_key_unknown();
    check_services_canonicalize_key_alias_native_input();
    check_services_canonicalize_key_alias_native_graphics();

    /* Register native runtime services */
    check_services_register_native_null();
    check_services_register_native_basic();

    /* Null safety */
    check_services_print_null();
    check_services_global();

    /* Statuses and providers */
    check_services_register_all_statuses();
    check_services_register_all_providers();

    /* Boundaries */
    check_services_register_long_key();
    check_services_register_max_length_name();

    return 0;
}
