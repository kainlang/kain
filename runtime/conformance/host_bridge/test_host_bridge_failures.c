#include "../../native/include/kain_runtime_host_bridge.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void copy_text(char* out, size_t out_size, const char* text) {
    size_t length;

    if (!out || out_size == 0) {
        return;
    }
    if (!text) {
        out[0] = '\0';
        return;
    }

    length = strlen(text);
    if (length >= out_size) {
        length = out_size - 1;
    }

    memcpy(out, text, length);
    out[length] = '\0';
}

static int register_runtime_service(
    KainServiceRegistry* registry,
    const char* key
) {
    return kain_service_registry_register(
        registry,
        key,
        key,
        "test runtime service",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    ) == 0;
}

static int test_abi_mismatch(void) {
    KainServiceRegistry runtime_services;
    KainHostBridgeRegistry bridge;
    KainHostBridgeModuleDescriptor module;
    KainDiagnostic diag;

    kain_service_registry_init(&runtime_services);
    register_runtime_service(&runtime_services, KAIN_SERVICE_KEY_CONTRACT);
    kain_host_bridge_registry_init(&bridge);
    kain_host_bridge_module_descriptor_init(&module);
    kain_diagnostic_init(&diag);

    copy_text(module.module_id, sizeof(module.module_id), "bridge.bad_abi");
    module.provider = KAIN_SERVICE_PROVIDER_HOST_NODE;
    module.lane = KAIN_FOREIGN_RUNTIME_NODE;
    module.abi_version = KAIN_RUNTIME_ABI_VERSION_ENCODE(1, 0, 0);

    if (kain_host_bridge_install_module(&bridge, &runtime_services, &module, 0u, &diag) == 0) {
        fprintf(stderr, "expected ABI mismatch to fail\n");
        return 0;
    }
    return diag.code == KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH;
}

static int test_missing_service(void) {
    KainServiceRegistry runtime_services;
    KainHostBridgeRegistry bridge;
    KainHostBridgeModuleDescriptor module;
    KainDiagnostic diag;

    kain_service_registry_init(&runtime_services);
    kain_host_bridge_registry_init(&bridge);
    kain_host_bridge_module_descriptor_init(&module);
    kain_diagnostic_init(&diag);

    copy_text(module.module_id, sizeof(module.module_id), "bridge.missing_service");
    module.provider = KAIN_SERVICE_PROVIDER_HOST_PYTHON;
    module.lane = KAIN_FOREIGN_RUNTIME_PYTHON;
    if (kain_host_bridge_module_add_required_service(&module, KAIN_SERVICE_KEY_REFLECTION) != 0) {
        return 0;
    }

    if (kain_host_bridge_install_module(&bridge, &runtime_services, &module, 0u, &diag) == 0) {
        fprintf(stderr, "expected missing service validation to fail\n");
        return 0;
    }
    return diag.code == KAIN_DIAG_CODE_HOST_BRIDGE_SERVICE_MISSING;
}

static int test_uninstall_removes_services(void) {
    KainServiceRegistry runtime_services;
    KainHostBridgeRegistry bridge;
    KainHostBridgeModuleDescriptor module;
    KainHostBridgeServiceDescriptor service;
    KainDiagnostic diag;

    kain_service_registry_init(&runtime_services);
    register_runtime_service(&runtime_services, KAIN_SERVICE_KEY_CONTRACT);
    kain_host_bridge_registry_init(&bridge);
    kain_host_bridge_module_descriptor_init(&module);
    kain_host_bridge_service_descriptor_init(&service);
    kain_diagnostic_init(&diag);

    copy_text(module.module_id, sizeof(module.module_id), "bridge.cleanup");
    module.provider = KAIN_SERVICE_PROVIDER_HOST_RUST;
    module.lane = KAIN_FOREIGN_RUNTIME_RUST;
    if (kain_host_bridge_module_add_required_service(&module, KAIN_SERVICE_KEY_CONTRACT) != 0) {
        return 0;
    }
    if (kain_host_bridge_install_module(&bridge, &runtime_services, &module, 0u, &diag) != 0) {
        fprintf(stderr, "expected cleanup module install to succeed: %s\n", diag.message);
        return 0;
    }

    copy_text(service.service_key, sizeof(service.service_key), "rust.bundle_loader");
    copy_text(service.service_name, sizeof(service.service_name), "Rust Bundle Loader");
    copy_text(service.module_id, sizeof(service.module_id), module.module_id);
    service.provider = KAIN_SERVICE_PROVIDER_HOST_RUST;
    if (kain_host_bridge_register_service(&bridge, &service, &diag) != 0) {
        fprintf(stderr, "expected cleanup service registration to succeed: %s\n", diag.message);
        return 0;
    }

    if (kain_host_bridge_unregister_module(&bridge, module.module_id, &diag) != 0) {
        fprintf(stderr, "expected module uninstall to succeed: %s\n", diag.message);
        return 0;
    }
    if (kain_host_bridge_lookup_module(&bridge, module.module_id) != NULL) {
        fprintf(stderr, "expected module to be removed\n");
        return 0;
    }
    if (kain_host_bridge_lookup_service(&bridge, "rust.bundle_loader") != NULL) {
        fprintf(stderr, "expected module services to be removed\n");
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_abi_mismatch()) {
        fprintf(stderr, "ABI mismatch test failed\n");
        return 1;
    }
    if (!test_missing_service()) {
        fprintf(stderr, "missing service test failed\n");
        return 1;
    }
    if (!test_uninstall_removes_services()) {
        fprintf(stderr, "uninstall cleanup test failed\n");
        return 1;
    }

    printf("PASS: host bridge failure coverage completed successfully\n");
    return 0;
}
