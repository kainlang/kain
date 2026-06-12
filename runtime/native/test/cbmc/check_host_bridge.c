/*
 * check_host_bridge.c -- CBMC verification harness for host_bridge module
 *
 * Covers: registry init, module/service descriptor init, add_required_service,
 * install_module, activate_module, unregister_module, register_service,
 * lookup_module, lookup_service, count_services_for_module,
 * contract_for_lane, lane_name.
 *
 * External functions (kain_diagnostic_create, kain_service_registry_is_available,
 * version_check_abi_compatibility) are nondeterministic -- CBMC explores all
 * return values and side effects through pointer arguments.
 *
 * Combined translation unit: host_bridge.c + check_host_bridge.c.
 */

#include "host_bridge.h"

/* Static backing buffers for string copy provenance */
static unsigned char g_text_buffer[1024];
static unsigned char g_service_key_buf[KAIN_SERVICE_KEY_MAX];
static unsigned char g_module_id_buf[KAIN_HOST_BRIDGE_MODULE_ID_MAX];

/* Static service registry -- used by install/register */
static KainServiceRegistry g_runtime_services;

/* Static diagnostic record -- used by functions that accept KainDiagnostic* */
static KainDiagnostic g_diag;

/* ======================================================================
 * Static function forward declarations (from host_bridge.c)
 * ====================================================================== */
static void kain_copy_text(char* out, size_t out_cap, const char* text);
static void kain_host_bridge_set_diag(
    KainDiagnostic* diag,
    int code,
    const char* message,
    const char* detail
);
static int kain_host_bridge_validate_module(
    const KainServiceRegistry* runtime_services,
    const KainHostBridgeModuleDescriptor* descriptor,
    unsigned int available_capability_mask,
    KainDiagnostic* diag
);

/* External functions -- CBMC treats as nondeterministic */
void kain_diagnostic_create(
    KainDiagnostic* diag,
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail,
    const char* source_path
);
int kain_service_registry_is_available(
    const KainServiceRegistry* registry,
    const char* key
);
int version_check_abi_compatibility(
    unsigned int required_abi_version_encoded
);


/* ======================================================================
 * Factories
 * ====================================================================== */

/* Create a valid, initialized registry */
static KainHostBridgeRegistry* create_valid_registry(void) {
    static KainHostBridgeRegistry registry;
    __CPROVER_havoc_object(&registry);
    kain_host_bridge_registry_init(&registry);

    __CPROVER_assert(registry.initialized != 0,
                     "create_registry: initialized == 1");
    __CPROVER_assert(registry.module_count == 0,
                     "create_registry: module_count == 0");
    __CPROVER_assert(registry.service_count == 0,
                     "create_registry: service_count == 0");
    return &registry;
}

/* Create a valid module descriptor with nondet but constrained fields */
static KainHostBridgeModuleDescriptor* create_valid_module_descriptor(void) {
    static KainHostBridgeModuleDescriptor desc;
    __CPROVER_havoc_object(&desc);
    kain_host_bridge_module_descriptor_init(&desc);

    /* Set module_id to a known string for deterministic lookups */
    kain_copy_text(desc.module_id, sizeof(desc.module_id), "test_module");
    kain_copy_text(desc.module_name, sizeof(desc.module_name), "Test Module");

    /* Constrain remaining fields */
    __CPROVER_assume(desc.required_service_count >= 0 &&
                     desc.required_service_count <=
                         KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES);
    __CPROVER_assume(desc.lane >= KAIN_FOREIGN_RUNTIME_UNKNOWN &&
                     desc.lane <= KAIN_FOREIGN_RUNTIME_ZIG);

    return &desc;
}

/* Create a valid service descriptor */
static KainHostBridgeServiceDescriptor* create_valid_service_descriptor(void) {
    static KainHostBridgeServiceDescriptor desc;
    __CPROVER_havoc_object(&desc);
    kain_host_bridge_service_descriptor_init(&desc);

    kain_copy_text(desc.service_key, sizeof(desc.service_key),
                   "test.service");
    kain_copy_text(desc.service_name, sizeof(desc.service_name),
                   "Test Service");
    kain_copy_text(desc.module_id, sizeof(desc.module_id), "test_module");
    desc.capability_mask = 0u;
    desc.function_table = NULL;

    return &desc;
}


/* ======================================================================
 * Check: registry init
 * ====================================================================== */
static void check_registry_init(void) {
    KainHostBridgeRegistry* reg = create_valid_registry();
    (void)reg;

    /* Ensure all module slots are zeroed */
    int i;
    for (i = 0; i < KAIN_HOST_BRIDGE_MAX_MODULES; i++) {
        __CPROVER_assert(
            reg->modules[i].state == KAIN_HOST_BRIDGE_MODULE_UNINSTALLED,
            "registry_init: all module states UNINSTALLED");
    }
}

static void check_registry_init_null(void) {
    kain_host_bridge_registry_init(NULL);
}


/* ======================================================================
 * Check: module descriptor init
 * ====================================================================== */
static void check_module_descriptor_init(void) {
    KainHostBridgeModuleDescriptor desc;
    __CPROVER_havoc_object(&desc);

    kain_host_bridge_module_descriptor_init(&desc);

    __CPROVER_assert(desc.provider == KAIN_SERVICE_PROVIDER_EXTERNAL,
                     "module_desc_init: provider == EXTERNAL");
    __CPROVER_assert(desc.lane == KAIN_FOREIGN_RUNTIME_UNKNOWN,
                     "module_desc_init: lane == UNKNOWN");
    __CPROVER_assert(desc.abi_version == RUNTIME_ABI_VERSION_CURRENT,
                     "module_desc_init: abi_version == CURRENT");
    __CPROVER_assert(desc.required_service_count == 0,
                     "module_desc_init: required_service_count == 0");
    __CPROVER_assert(desc.module_id[0] == '\0',
                     "module_desc_init: module_id empty");
    __CPROVER_assert(desc.hot_reload_capable == 0,
                     "module_desc_init: hot_reload_capable == 0");
}

static void check_module_descriptor_init_null(void) {
    kain_host_bridge_module_descriptor_init(NULL);
}


/* ======================================================================
 * Check: service descriptor init
 * ====================================================================== */
static void check_service_descriptor_init(void) {
    KainHostBridgeServiceDescriptor desc;
    __CPROVER_havoc_object(&desc);

    kain_host_bridge_service_descriptor_init(&desc);

    __CPROVER_assert(desc.provider == KAIN_SERVICE_PROVIDER_EXTERNAL,
                     "service_desc_init: provider == EXTERNAL");
    __CPROVER_assert(desc.abi_version == RUNTIME_ABI_VERSION_CURRENT,
                     "service_desc_init: abi_version == CURRENT");
    __CPROVER_assert(desc.function_table == NULL,
                     "service_desc_init: function_table == NULL");
    __CPROVER_assert(desc.service_key[0] == '\0',
                     "service_desc_init: service_key empty");
}

static void check_service_descriptor_init_null(void) {
    kain_host_bridge_service_descriptor_init(NULL);
}


/* ======================================================================
 * Check: add_required_service
 * ====================================================================== */
static void check_add_required_service(void) {
    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);

    int rc = kain_host_bridge_module_add_required_service(
        &desc, "required.service.one");
    __CPROVER_assert(rc == 0,
                     "add_required_service: first add succeeds");
    __CPROVER_assert(desc.required_service_count == 1,
                     "add_required_service: count == 1");
    __CPROVER_assert(
        strcmp(desc.required_services[0], "required.service.one") == 0,
        "add_required_service: first key stored");

    rc = kain_host_bridge_module_add_required_service(
        &desc, "required.service.two");
    __CPROVER_assert(rc == 0,
                     "add_required_service: second add succeeds");
    __CPROVER_assert(desc.required_service_count == 2,
                     "add_required_service: count == 2");
    __CPROVER_assert(
        strcmp(desc.required_services[1], "required.service.two") == 0,
        "add_required_service: second key stored");
}

static void check_add_required_service_null(void) {
    int rc1 = kain_host_bridge_module_add_required_service(NULL, "key");
    __CPROVER_assert(rc1 == -1,
                     "add_required_service_null: NULL descriptor returns -1");

    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);
    int rc2 = kain_host_bridge_module_add_required_service(&desc, NULL);
    __CPROVER_assert(rc2 == -1,
                     "add_required_service_null: NULL key returns -1");
}

static void check_add_required_service_capacity(void) {
    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);

    int i;
    for (i = 0; i < KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "svc.%d", i);
        int rc = kain_host_bridge_module_add_required_service(&desc, key);
        __CPROVER_assert(rc == 0,
                         "add_required_service_capacity: fill succeeds");
    }
    __CPROVER_assert(
        desc.required_service_count == KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES,
        "add_required_service_capacity: count == max");

    /* Overflow must fail */
    int rc = kain_host_bridge_module_add_required_service(
        &desc, "overflow");
    __CPROVER_assert(rc == -1,
                     "add_required_service_capacity: overflow returns -1");
    __CPROVER_assert(
        desc.required_service_count == KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES,
        "add_required_service_capacity: count unchanged after overflow");
}


/* ======================================================================
 * Check: install_module -- with valid descriptor
 * ====================================================================== */
static void check_install_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    unsigned int cap_mask;

    __CPROVER_havoc_object(&cap_mask);

    kain_diagnostic_init(&g_diag);

    int rc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, cap_mask, &g_diag);

    if (rc == 0) {
        __CPROVER_assert(registry->module_count == 1,
                         "install: module_count == 1");
        __CPROVER_assert(
            registry->modules[0].state == KAIN_HOST_BRIDGE_MODULE_INSTALLED,
            "install: state == INSTALLED");
        __CPROVER_assert(
            strcmp(registry->modules[0].descriptor.module_id,
                   desc->module_id) == 0,
            "install: module_id preserved");
        __CPROVER_assert(
            registry->modules[0].descriptor.required_service_count ==
                desc->required_service_count,
            "install: required_service_count preserved");
    } else {
        /* Install failed -- module_count unchanged */
        __CPROVER_assert(registry->module_count == 0,
                         "install: module_count unchanged on failure");
    }
}

static void check_install_module_null(void) {
    kain_diagnostic_init(&g_diag);

    /* NULL registry */
    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);
    int rc1 = kain_host_bridge_install_module(
        NULL, NULL, &desc, 0u, &g_diag);
    __CPROVER_assert(rc1 == -1,
                     "install_null: NULL registry returns -1");

    /* NULL descriptor */
    KainHostBridgeRegistry* reg = create_valid_registry();
    int rc2 = kain_host_bridge_install_module(
        reg, NULL, NULL, 0u, &g_diag);
    __CPROVER_assert(rc2 == -1,
                     "install_null: NULL descriptor returns -1");
}

static void check_install_module_duplicate_id(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();

    kain_diagnostic_init(&g_diag);

    /* Install once */
    int rc1 = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);

    /* Install again with same id -- must fail */
    kain_diagnostic_init(&g_diag);
    int rc2 = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (rc1 == 0) {
        __CPROVER_assert(rc2 == -1,
                         "install_duplicate: duplicate id returns -1");
        __CPROVER_assert(registry->module_count == 1,
                         "install_duplicate: module_count unchanged");
    }
}

static void check_install_module_full(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    int i;

    kain_diagnostic_init(&g_diag);

    /* Fill the registry to capacity */
    for (i = 0; i < KAIN_HOST_BRIDGE_MAX_MODULES; i++) {
        KainHostBridgeModuleDescriptor desc;
        kain_host_bridge_module_descriptor_init(&desc);
        char id[64];
        snprintf(id, sizeof(id), "module.%d", i);
        kain_copy_text(desc.module_id, sizeof(desc.module_id), id);

        int rc = kain_host_bridge_install_module(
            registry, &g_runtime_services, &desc, 0u, &g_diag);
        if (rc != 0) {
            break;
        }
    }

    /* Try one more -- must fail if full, succeed if not yet full */
    KainHostBridgeModuleDescriptor overflow;
    kain_host_bridge_module_descriptor_init(&overflow);
    kain_copy_text(overflow.module_id, sizeof(overflow.module_id),
                   "overflow_module");

    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_install_module(
        registry, &g_runtime_services, &overflow, 0u, &g_diag);
    if (registry->module_count >= KAIN_HOST_BRIDGE_MAX_MODULES) {
        __CPROVER_assert(rc == -1,
                         "install_full: overflow returns -1");
    }
}


/* ======================================================================
 * Check: activate_module
 * ====================================================================== */
static void check_activate_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();

    kain_diagnostic_init(&g_diag);

    int install_rc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);

    if (install_rc == 0) {
        kain_diagnostic_init(&g_diag);
        int rc = kain_host_bridge_activate_module(
            registry, desc->module_id, &g_diag);
        __CPROVER_assert(rc == 0,
                         "activate: succeeds");
        __CPROVER_assert(
            registry->modules[0].state == KAIN_HOST_BRIDGE_MODULE_ACTIVE,
            "activate: state == ACTIVE");
    }
}

static void check_activate_module_not_found(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();

    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_activate_module(
        registry, "nonexistent_module", &g_diag);
    __CPROVER_assert(rc == -1,
                     "activate_not_found: unknown module returns -1");
}

static void check_activate_module_null(void) {
    kain_diagnostic_init(&g_diag);
    int rc1 = kain_host_bridge_activate_module(NULL, "id", &g_diag);
    __CPROVER_assert(rc1 == -1,
                     "activate_null: NULL registry returns -1");

    KainHostBridgeRegistry* reg = create_valid_registry();
    int rc2 = kain_host_bridge_activate_module(reg, NULL, &g_diag);
    __CPROVER_assert(rc2 == -1,
                     "activate_null: NULL module_id returns -1");
}


/* ======================================================================
 * Check: unregister_module
 * ====================================================================== */
static void check_unregister_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();

    kain_diagnostic_init(&g_diag);

    int install_rc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);

    if (install_rc == 0) {
        kain_diagnostic_init(&g_diag);
        int rc = kain_host_bridge_unregister_module(
            registry, desc->module_id, &g_diag);
        __CPROVER_assert(rc == 0,
                         "unregister: succeeds");
        __CPROVER_assert(registry->module_count == 0,
                         "unregister: module_count == 0");
    }
}

static void check_unregister_module_not_found(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();

    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_unregister_module(
        registry, "nonexistent", &g_diag);
    __CPROVER_assert(rc == -1,
                     "unregister_not_found: returns -1");
}

static void check_unregister_module_null(void) {
    kain_diagnostic_init(&g_diag);
    int rc1 = kain_host_bridge_unregister_module(NULL, "id", &g_diag);
    __CPROVER_assert(rc1 == -1,
                     "unregister_null: NULL registry returns -1");

    KainHostBridgeRegistry* reg = create_valid_registry();
    int rc2 = kain_host_bridge_unregister_module(reg, NULL, &g_diag);
    __CPROVER_assert(rc2 == -1,
                     "unregister_null: NULL module_id returns -1");
}

static void check_unregister_module_removes_services(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);

    /* Install module */
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (irc != 0) return;

    /* Register service under module */
    kain_diagnostic_init(&g_diag);
    int src = kain_host_bridge_register_service(
        registry, svc, &g_diag);
    if (src != 0) return;

    __CPROVER_assert(registry->service_count == 1,
                     "unregister_removes: service_count == 1 after register");

    /* Unregister module should also remove its services */
    kain_diagnostic_init(&g_diag);
    int urc = kain_host_bridge_unregister_module(
        registry, desc->module_id, &g_diag);
    __CPROVER_assert(urc == 0,
                     "unregister_removes: unregister succeeds");
    __CPROVER_assert(registry->module_count == 0,
                     "unregister_removes: module_count == 0");
    __CPROVER_assert(registry->service_count == 0,
                     "unregister_removes: service_count == 0 (services removed)");
}


/* ======================================================================
 * Check: register_service
 * ====================================================================== */
static void check_register_service(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);

    /* Must install module first */
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (irc != 0) return;

    /* Register service */
    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_register_service(registry, svc, &g_diag);

    if (rc == 0) {
        __CPROVER_assert(registry->service_count == 1,
                         "register_service: service_count == 1");
        __CPROVER_assert(
            strcmp(registry->services[0].service_key, svc->service_key) == 0,
            "register_service: service_key preserved");
        __CPROVER_assert(
            strcmp(registry->services[0].module_id, svc->module_id) == 0,
            "register_service: module_id preserved");
        __CPROVER_assert(
            registry->services[0].abi_version == svc->abi_version,
            "register_service: abi_version preserved");
    }
}

static void check_register_service_no_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);

    /* Service key points to module that isn't installed */
    int rc = kain_host_bridge_register_service(registry, svc, &g_diag);
    __CPROVER_assert(rc == -1,
                     "register_service_no_module: returns -1 (module not installed)");
}

static void check_register_service_duplicate_key(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (irc != 0) return;

    /* Register twice with same key */
    kain_diagnostic_init(&g_diag);
    int rc1 = kain_host_bridge_register_service(registry, svc, &g_diag);
    kain_diagnostic_init(&g_diag);
    int rc2 = kain_host_bridge_register_service(registry, svc, &g_diag);

    if (rc1 == 0) {
        __CPROVER_assert(rc2 == -1,
                         "register_service_duplicate_key: duplicate key returns -1");
        __CPROVER_assert(registry->service_count == 1,
                         "register_service_duplicate_key: service_count unchanged");
    }
}

static void check_register_service_null(void) {
    KainHostBridgeRegistry* reg = create_valid_registry();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);
    int rc1 = kain_host_bridge_register_service(NULL, svc, &g_diag);
    __CPROVER_assert(rc1 == -1,
                     "register_service_null: NULL registry returns -1");

    kain_diagnostic_init(&g_diag);
    int rc2 = kain_host_bridge_register_service(reg, NULL, &g_diag);
    __CPROVER_assert(rc2 == -1,
                     "register_service_null: NULL descriptor returns -1");

    /* Empty service_key */
    KainHostBridgeServiceDescriptor empty;
    kain_host_bridge_service_descriptor_init(&empty);
    kain_diagnostic_init(&g_diag);
    int rc3 = kain_host_bridge_register_service(reg, &empty, &g_diag);
    __CPROVER_assert(rc3 == -1,
                     "register_service_null: empty service_key returns -1");
}


/* ======================================================================
 * Check: lookup_module
 * ====================================================================== */
static void check_lookup_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();

    kain_diagnostic_init(&g_diag);
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);

    if (irc == 0) {
        const KainHostBridgeInstalledModule* found =
            kain_host_bridge_lookup_module(registry, desc->module_id);
        __CPROVER_assert(found != NULL,
                         "lookup_module: installed module found");
        __CPROVER_assert(
            strcmp(found->descriptor.module_id, desc->module_id) == 0,
            "lookup_module: module_id matches");
    }
}

static void check_lookup_module_not_found(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    const KainHostBridgeInstalledModule* found =
        kain_host_bridge_lookup_module(registry, "nonexistent");
    __CPROVER_assert(found == NULL,
                     "lookup_module_not_found: returns NULL");
}

static void check_lookup_module_null(void) {
    const KainHostBridgeInstalledModule* r1 =
        kain_host_bridge_lookup_module(NULL, "id");
    __CPROVER_assert(r1 == NULL,
                     "lookup_module_null: NULL registry returns NULL");

    KainHostBridgeRegistry* reg = create_valid_registry();
    const KainHostBridgeInstalledModule* r2 =
        kain_host_bridge_lookup_module(reg, NULL);
    __CPROVER_assert(r2 == NULL,
                     "lookup_module_null: NULL module_id returns NULL");
}


/* ======================================================================
 * Check: lookup_service
 * ====================================================================== */
static void check_lookup_service(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();

    kain_diagnostic_init(&g_diag);
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (irc != 0) return;

    kain_diagnostic_init(&g_diag);
    int src = kain_host_bridge_register_service(registry, svc, &g_diag);
    if (src != 0) return;

    const KainHostBridgeServiceDescriptor* found =
        kain_host_bridge_lookup_service(registry, svc->service_key);
    __CPROVER_assert(found != NULL,
                     "lookup_service: registered service found");
    __CPROVER_assert(
        strcmp(found->service_key, svc->service_key) == 0,
        "lookup_service: service_key matches");
}

static void check_lookup_service_not_found(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    const KainHostBridgeServiceDescriptor* found =
        kain_host_bridge_lookup_service(registry, "nonexistent.service");
    __CPROVER_assert(found == NULL,
                     "lookup_service_not_found: returns NULL");
}

static void check_lookup_service_null(void) {
    const KainHostBridgeServiceDescriptor* r1 =
        kain_host_bridge_lookup_service(NULL, "key");
    __CPROVER_assert(r1 == NULL,
                     "lookup_service_null: NULL registry returns NULL");

    KainHostBridgeRegistry* reg = create_valid_registry();
    const KainHostBridgeServiceDescriptor* r2 =
        kain_host_bridge_lookup_service(reg, NULL);
    __CPROVER_assert(r2 == NULL,
                     "lookup_service_null: NULL key returns NULL");
}


/* ======================================================================
 * Check: count_services_for_module
 * ====================================================================== */
static void check_count_services_for_module(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();

    kain_diagnostic_init(&g_diag);
    int irc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (irc != 0) return;

    /* Count before any services registered */
    int count0 = kain_host_bridge_count_services_for_module(
        registry, desc->module_id);
    __CPROVER_assert(count0 == 0,
                     "count_services: 0 before registration");

    /* Register a service */
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();
    kain_diagnostic_init(&g_diag);
    int src = kain_host_bridge_register_service(registry, svc, &g_diag);
    if (src != 0) return;

    int count1 = kain_host_bridge_count_services_for_module(
        registry, desc->module_id);
    __CPROVER_assert(count1 == 1,
                     "count_services: 1 after registration");
}

static void check_count_services_for_module_null(void) {
    int r1 = kain_host_bridge_count_services_for_module(NULL, "id");
    __CPROVER_assert(r1 == 0,
                     "count_services_null: NULL registry returns 0");

    KainHostBridgeRegistry* reg = create_valid_registry();
    int r2 = kain_host_bridge_count_services_for_module(reg, NULL);
    __CPROVER_assert(r2 == 0,
                     "count_services_null: NULL module_id returns 0");
}


/* ======================================================================
 * Check: contract_for_lane
 * ====================================================================== */
static void check_contract_for_lane(void) {
    KainForeignRuntimeLane lane;
    int i;
    KainForeignRuntimeLane all_lanes[] = {
        KAIN_FOREIGN_RUNTIME_UNKNOWN,
        KAIN_FOREIGN_RUNTIME_RUST,
        KAIN_FOREIGN_RUNTIME_PYTHON,
        KAIN_FOREIGN_RUNTIME_NODE,
        KAIN_FOREIGN_RUNTIME_C,
        KAIN_FOREIGN_RUNTIME_ZIG,
    };

    for (i = 0; i < (int)(sizeof(all_lanes) / sizeof(all_lanes[0])); i++) {
        const KainForeignBridgeContract* c =
            kain_host_bridge_contract_for_lane(all_lanes[i]);
        if (all_lanes[i] == KAIN_FOREIGN_RUNTIME_UNKNOWN) {
            __CPROVER_assert(c == NULL,
                             "contract_for_lane: UNKNOWN returns NULL");
        } else {
            __CPROVER_assert(c != NULL,
                             "contract_for_lane: known lane returns non-NULL");
            __CPROVER_assert(c->lane == all_lanes[i],
                             "contract_for_lane: lane field matches");
            __CPROVER_assert(c->lane_name[0] != '\0',
                             "contract_for_lane: lane_name non-empty");
            __CPROVER_assert(c->marshaling_model[0] != '\0',
                             "contract_for_lane: marshaling_model non-empty");
            __CPROVER_assert(c->ownership_model[0] != '\0',
                             "contract_for_lane: ownership_model non-empty");
            __CPROVER_assert(c->failure_model[0] != '\0',
                             "contract_for_lane: failure_model non-empty");
        }
    }
}


/* ======================================================================
 * Check: lane_name
 * ====================================================================== */
static void check_lane_name(void) {
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_UNKNOWN),
               "unknown") == 0,
        "lane_name: UNKNOWN -> 'unknown'");
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_RUST),
               "rust") == 0,
        "lane_name: RUST -> 'rust'");
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_PYTHON),
               "python") == 0,
        "lane_name: PYTHON -> 'python'");
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_NODE),
               "node") == 0,
        "lane_name: NODE -> 'node'");
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_C),
               "c") == 0,
        "lane_name: C -> 'c'");
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(KAIN_FOREIGN_RUNTIME_ZIG),
               "zig") == 0,
        "lane_name: ZIG -> 'zig'");

    /* Unknown lane returns "unknown" */
    KainForeignRuntimeLane bad;
    __CPROVER_havoc_object(&bad);
    __CPROVER_assume(bad > KAIN_FOREIGN_RUNTIME_ZIG);
    __CPROVER_assert(
        strcmp(kain_host_bridge_lane_name(bad), "unknown") == 0,
        "lane_name: invalid lane -> 'unknown'");
}


/* ======================================================================
 * Check: kain_copy_text (static) copies safely
 * ====================================================================== */
static void check_copy_text(void) {
    char buf[64];
    __CPROVER_havoc_object(buf);

    kain_copy_text(buf, sizeof(buf), "test text");
    __CPROVER_assert(strcmp(buf, "test text") == 0,
                     "copy_text: copies correctly");
    __CPROVER_assert(buf[sizeof(buf) - 1] == '\0',
                     "copy_text: buffer null-terminated");

    /* NULL source empties buffer */
    buf[0] = 'X';
    kain_copy_text(buf, sizeof(buf), NULL);
    __CPROVER_assert(buf[0] == '\0',
                     "copy_text: NULL source empties buffer");

    /* Zero capacity does nothing */
    kain_copy_text(NULL, 0, "text");
    /* Must not crash */

    /* String that exceeds buffer */
    char small[4];
    kain_copy_text(small, sizeof(small), "too-long-string");
    __CPROVER_assert(strlen(small) <= sizeof(small) - 1,
                     "copy_text: truncated to fit buffer");
    __CPROVER_assert(small[sizeof(small) - 1] == '\0',
                     "copy_text: small buf null-terminated");
}


/* ======================================================================
 * Check: kain_host_bridge_set_diag (static) sets fields correctly
 * ====================================================================== */
static void check_set_diag(void) {
    kain_diagnostic_init(&g_diag);

    kain_host_bridge_set_diag(&g_diag, 8001, "test message", "test detail");

    /* kain_diagnostic_create is external (nondet), so we only check
     * that the call doesn't crash.  CBMC explores all side effects. */

    /* NULL diag is a no-op */
    kain_host_bridge_set_diag(NULL, 8001, "msg", "detail");
}


/* ======================================================================
 * Check: kain_host_bridge_validate_module (static) against edge cases
 * ====================================================================== */
static void check_validate_module(void) {
    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);
    kain_copy_text(desc.module_id, sizeof(desc.module_id), "valid_module");
    desc.required_capability_mask = 0u;

    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_validate_module(
        &g_runtime_services, &desc, 0xFFFFFFFFu, &g_diag);

    /* Validation may succeed or fail depending on nondet external fns */
    if (rc == 0) {
        /* Everything checks out -- module id present, ABI compatible,
         * caps satisfied, all required services available */
        __CPROVER_assert(desc.module_id[0] != '\0',
                         "validate: module_id is set");
    }
}

static void check_validate_module_empty_id(void) {
    KainHostBridgeModuleDescriptor desc;
    kain_host_bridge_module_descriptor_init(&desc);
    /* module_id is intentionally left empty */

    kain_diagnostic_init(&g_diag);
    int rc = kain_host_bridge_validate_module(
        &g_runtime_services, &desc, 0u, &g_diag);
    __CPROVER_assert(rc == -1,
                     "validate_module: empty module_id returns -1");
}


/* ======================================================================
 * Check: chain operations -- full lifecycle
 * ====================================================================== */
static void check_full_lifecycle(void) {
    KainHostBridgeRegistry* registry = create_valid_registry();
    KainHostBridgeModuleDescriptor* desc = create_valid_module_descriptor();
    KainHostBridgeServiceDescriptor* svc = create_valid_service_descriptor();
    int rc;

    /* 1. Install module */
    kain_diagnostic_init(&g_diag);
    rc = kain_host_bridge_install_module(
        registry, &g_runtime_services, desc, 0u, &g_diag);
    if (rc != 0) return;

    __CPROVER_assert(registry->module_count == 1,
                     "lifecycle: module installed");

    /* 2. Activate module */
    kain_diagnostic_init(&g_diag);
    rc = kain_host_bridge_activate_module(registry, desc->module_id, &g_diag);
    if (rc == 0) {
        __CPROVER_assert(
            registry->modules[0].state == KAIN_HOST_BRIDGE_MODULE_ACTIVE,
            "lifecycle: module activated");
    }

    /* 3. Register service */
    kain_diagnostic_init(&g_diag);
    rc = kain_host_bridge_register_service(registry, svc, &g_diag);
    if (rc == 0) {
        __CPROVER_assert(registry->service_count == 1,
                         "lifecycle: service registered");

        /* 3a. Lookup service */
        const KainHostBridgeServiceDescriptor* found =
            kain_host_bridge_lookup_service(registry, svc->service_key);
        __CPROVER_assert(found != NULL,
                         "lifecycle: service lookup succeeds");

        /* 3b. Count services for module */
        int cnt = kain_host_bridge_count_services_for_module(
            registry, desc->module_id);
        __CPROVER_assert(cnt == 1,
                         "lifecycle: service count == 1");
    }

    /* 4. Lookup module */
    const KainHostBridgeInstalledModule* mod =
        kain_host_bridge_lookup_module(registry, desc->module_id);
    __CPROVER_assert(mod != NULL,
                     "lifecycle: module lookup succeeds");

    /* 5. Unregister module (also removes its services) */
    kain_diagnostic_init(&g_diag);
    rc = kain_host_bridge_unregister_module(registry, desc->module_id, &g_diag);
    if (rc == 0) {
        __CPROVER_assert(registry->module_count == 0,
                         "lifecycle: module unregistered");
        __CPROVER_assert(registry->service_count == 0,
                         "lifecycle: services also removed");

        /* After unregister, lookups must fail */
        __CPROVER_assert(
            kain_host_bridge_lookup_module(registry, desc->module_id) == NULL,
            "lifecycle: module lookup fails after unregister");
    }
}


/* ======================================================================
 * Main -- run all host_bridge checks
 * ====================================================================== */
int main(void) {
    check_registry_init();
    check_registry_init_null();
    check_module_descriptor_init();
    check_module_descriptor_init_null();
    check_service_descriptor_init();
    check_service_descriptor_init_null();
    check_add_required_service();
    check_add_required_service_null();
    check_add_required_service_capacity();
    check_install_module();
    check_install_module_null();
    check_install_module_duplicate_id();
    check_install_module_full();
    check_activate_module();
    check_activate_module_not_found();
    check_activate_module_null();
    check_unregister_module();
    check_unregister_module_not_found();
    check_unregister_module_null();
    check_unregister_module_removes_services();
    check_register_service();
    check_register_service_no_module();
    check_register_service_duplicate_key();
    check_register_service_null();
    check_lookup_module();
    check_lookup_module_not_found();
    check_lookup_module_null();
    check_lookup_service();
    check_lookup_service_not_found();
    check_lookup_service_null();
    check_count_services_for_module();
    check_count_services_for_module_null();
    check_contract_for_lane();
    check_lane_name();
    check_copy_text();
    check_set_diag();
    check_validate_module();
    check_validate_module_empty_id();
    check_full_lifecycle();
    return 0;
}
