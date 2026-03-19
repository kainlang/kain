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
    const char* key,
    KainServiceRequirement requirement
) {
    return kain_service_registry_register(
        registry,
        key,
        key,
        "test runtime service",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        requirement,
        KAIN_RUNTIME_ABI_VERSION_CURRENT,
        NULL
    ) == 0;
}

int main(void) {
    KainServiceRegistry runtime_services;
    KainHostBridgeRegistry bridge;
    KainHostBridgeModuleDescriptor module;
    KainHostBridgeServiceDescriptor service;
    KainDiagnostic diag;
    const KainHostBridgeServiceDescriptor* resolved_service;
    const KainForeignBridgeContract* python_contract;

    kain_service_registry_init(&runtime_services);
    if (!register_runtime_service(&runtime_services, KAIN_SERVICE_KEY_CONTRACT, KAIN_SERVICE_REQUIREMENT_REQUIRED)) {
        fprintf(stderr, "failed to register contract runtime service\n");
        return 1;
    }
    if (!register_runtime_service(&runtime_services, KAIN_SERVICE_KEY_BASE_DIAGNOSTICS, KAIN_SERVICE_REQUIREMENT_REQUIRED)) {
        fprintf(stderr, "failed to register diagnostics runtime service\n");
        return 1;
    }

    kain_host_bridge_registry_init(&bridge);
    kain_host_bridge_module_descriptor_init(&module);
    kain_host_bridge_service_descriptor_init(&service);
    kain_diagnostic_init(&diag);

    copy_text(module.module_id, sizeof(module.module_id), "bridge.python.tools");
    copy_text(module.module_name, sizeof(module.module_name), "Python Tool Bridge");
    module.provider = KAIN_SERVICE_PROVIDER_HOST_PYTHON;
    module.lane = KAIN_FOREIGN_RUNTIME_PYTHON;
    module.required_capability_mask = 0x1u;
    module.hot_reload_capable = 1;
    if (kain_host_bridge_module_add_required_service(&module, KAIN_SERVICE_KEY_CONTRACT) != 0) {
        fprintf(stderr, "failed to add required service to module descriptor\n");
        return 1;
    }

    if (kain_host_bridge_install_module(&bridge, &runtime_services, &module, 0x1u, &diag) != 0) {
        fprintf(stderr, "module install failed: %s\n", diag.message);
        return 1;
    }
    if (kain_host_bridge_activate_module(&bridge, module.module_id, &diag) != 0) {
        fprintf(stderr, "module activation failed: %s\n", diag.message);
        return 1;
    }

    copy_text(service.service_key, sizeof(service.service_key), "python.exec");
    copy_text(service.service_name, sizeof(service.service_name), "Python Execution");
    copy_text(service.module_id, sizeof(service.module_id), module.module_id);
    service.provider = KAIN_SERVICE_PROVIDER_HOST_PYTHON;
    service.capability_mask = 0x1u;
    if (kain_host_bridge_register_service(&bridge, &service, &diag) != 0) {
        fprintf(stderr, "service registration failed: %s\n", diag.message);
        return 1;
    }

    resolved_service = kain_host_bridge_lookup_service(&bridge, "python.exec");
    if (!resolved_service) {
        fprintf(stderr, "expected host bridge service lookup to succeed\n");
        return 1;
    }
    if (strcmp(resolved_service->module_id, module.module_id) != 0) {
        fprintf(stderr, "resolved service module id mismatch\n");
        return 1;
    }
    if (kain_host_bridge_count_services_for_module(&bridge, module.module_id) != 1) {
        fprintf(stderr, "expected one host bridge service for the module\n");
        return 1;
    }

    python_contract = kain_host_bridge_contract_for_lane(KAIN_FOREIGN_RUNTIME_PYTHON);
    if (!python_contract) {
        fprintf(stderr, "expected Python bridge contract to be available\n");
        return 1;
    }
    if (strcmp(python_contract->lane_name, "python") != 0) {
        fprintf(stderr, "unexpected lane name for Python bridge contract\n");
        return 1;
    }

    printf("PASS: host bridge registration test completed successfully\n");
    return 0;
}
