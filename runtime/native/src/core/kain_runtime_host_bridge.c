#include "../../include/kain_runtime_host_bridge.h"

#include <stdio.h>
#include <string.h>

static void kain_host_bridge_set_diag(
    KainDiagnostic* diag,
    int code,
    const char* message,
    const char* detail
) {
    if (!diag) {
        return;
    }
    kain_diagnostic_create(
        diag,
        KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE,
        KAIN_DIAG_SEVERITY_ERROR,
        code,
        message,
        detail,
        NULL
    );
}

static const KainForeignBridgeContract KAIN_FOREIGN_CONTRACTS[] = {
    { KAIN_FOREIGN_RUNTIME_RUST, "rust", "c-abi function tables", "owned native handles", "diagnostics + non-zero status" },
    { KAIN_FOREIGN_RUNTIME_PYTHON, "python", "ffi-safe value marshalling", "borrowed runtime handles", "diagnostics + exception boundary" },
    { KAIN_FOREIGN_RUNTIME_NODE, "node", "json and ffi-safe payloads", "bridge-owned references", "diagnostics + rejected promise/status" },
    { KAIN_FOREIGN_RUNTIME_C, "c", "plain c abi structs", "caller-managed pointers", "diagnostics + integer status" },
    { KAIN_FOREIGN_RUNTIME_ZIG, "zig", "c abi + explicit slices", "caller-managed buffers", "diagnostics + error code" },
};

void kain_host_bridge_registry_init(KainHostBridgeRegistry* registry) {
    if (!registry) {
        return;
    }
    memset(registry, 0, sizeof(*registry));
    registry->initialized = 1;
}

void kain_host_bridge_module_descriptor_init(KainHostBridgeModuleDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    descriptor->provider = KAIN_SERVICE_PROVIDER_EXTERNAL;
    descriptor->lane = KAIN_FOREIGN_RUNTIME_UNKNOWN;
}

void kain_host_bridge_service_descriptor_init(KainHostBridgeServiceDescriptor* descriptor) {
    if (!descriptor) {
        return;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    descriptor->provider = KAIN_SERVICE_PROVIDER_EXTERNAL;
}

int kain_host_bridge_module_add_required_service(
    KainHostBridgeModuleDescriptor* descriptor,
    const char* service_key
) {
    if (!descriptor || !service_key) {
        return -1;
    }
    if (descriptor->required_service_count >= KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES) {
        return -1;
    }
    strncpy(
        descriptor->required_services[descriptor->required_service_count],
        service_key,
        KAIN_SERVICE_KEY_MAX - 1
    );
    descriptor->required_services[descriptor->required_service_count][KAIN_SERVICE_KEY_MAX - 1] = '\0';
    descriptor->required_service_count += 1;
    return 0;
}

static int kain_host_bridge_validate_module(
    const KainServiceRegistry* runtime_services,
    const KainHostBridgeModuleDescriptor* descriptor,
    unsigned int available_capability_mask,
    KainDiagnostic* diag
) {
    int index;

    if (!descriptor || descriptor->module_id[0] == '\0') {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module validation failed",
            "Module descriptor and module_id are required."
        );
        return -1;
    }

    if (!kain_runtime_version_check_abi_compatibility(descriptor->abi_version)) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH,
            "Host bridge module ABI mismatch",
            "Module requires an incompatible runtime ABI version."
        );
        return -1;
    }

    if ((descriptor->required_capability_mask & ~available_capability_mask) != 0u) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module is missing required capabilities",
            "Available capability mask does not satisfy the module requirement."
        );
        return -1;
    }

    for (index = 0; index < descriptor->required_service_count; ++index) {
        const char* service_key = descriptor->required_services[index];
        if (!runtime_services || !kain_service_registry_is_available(runtime_services, service_key)) {
            char detail[KAIN_DIAG_DETAIL_MAX];
            snprintf(
                detail,
                sizeof(detail),
                "Required service '%s' is not available for module '%s'.",
                service_key,
                descriptor->module_id
            );
            kain_host_bridge_set_diag(
                diag,
                KAIN_DIAG_CODE_HOST_BRIDGE_SERVICE_MISSING,
                "Host bridge module is missing required runtime services",
                detail
            );
            return -1;
        }
    }

    return 0;
}

int kain_host_bridge_install_module(
    KainHostBridgeRegistry* registry,
    const KainServiceRegistry* runtime_services,
    const KainHostBridgeModuleDescriptor* descriptor,
    unsigned int available_capability_mask,
    KainDiagnostic* diag
) {
    KainHostBridgeInstalledModule* module;

    if (!registry || !descriptor) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module installation failed",
            "Registry and descriptor are required."
        );
        return -1;
    }

    if (!registry->initialized) {
        kain_host_bridge_registry_init(registry);
    }

    if (registry->module_count >= KAIN_HOST_BRIDGE_MAX_MODULES) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module installation failed",
            "Host bridge module registry is full."
        );
        return -1;
    }

    if (kain_host_bridge_lookup_module(registry, descriptor->module_id)) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module installation failed",
            "Module id is already installed."
        );
        return -1;
    }

    if (kain_host_bridge_validate_module(runtime_services, descriptor, available_capability_mask, diag) != 0) {
        return -1;
    }

    module = &registry->modules[registry->module_count];
    memset(module, 0, sizeof(*module));
    module->descriptor = *descriptor;
    module->state = KAIN_HOST_BRIDGE_MODULE_INSTALLED;
    registry->module_count += 1;
    return 0;
}

int kain_host_bridge_activate_module(
    KainHostBridgeRegistry* registry,
    const char* module_id,
    KainDiagnostic* diag
) {
    int index;

    if (!registry || !module_id) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module activation failed",
            "Registry and module_id are required."
        );
        return -1;
    }

    for (index = 0; index < registry->module_count; ++index) {
        if (strcmp(registry->modules[index].descriptor.module_id, module_id) == 0) {
            registry->modules[index].state = KAIN_HOST_BRIDGE_MODULE_ACTIVE;
            return 0;
        }
    }

    kain_host_bridge_set_diag(
        diag,
        KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
        "Host bridge module activation failed",
        "Module id was not found in the host bridge registry."
    );
    return -1;
}

int kain_host_bridge_unregister_module(
    KainHostBridgeRegistry* registry,
    const char* module_id,
    KainDiagnostic* diag
) {
    int module_index;
    int service_index;
    int write_index;

    if (!registry || !module_id) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module uninstall failed",
            "Registry and module_id are required."
        );
        return -1;
    }

    module_index = -1;
    for (write_index = 0; write_index < registry->module_count; ++write_index) {
        if (strcmp(registry->modules[write_index].descriptor.module_id, module_id) == 0) {
            module_index = write_index;
            break;
        }
    }
    if (module_index < 0) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge module uninstall failed",
            "Module id was not found in the host bridge registry."
        );
        return -1;
    }

    for (write_index = module_index; write_index + 1 < registry->module_count; ++write_index) {
        registry->modules[write_index] = registry->modules[write_index + 1];
    }
    registry->module_count -= 1;

    write_index = 0;
    for (service_index = 0; service_index < registry->service_count; ++service_index) {
        if (strcmp(registry->services[service_index].module_id, module_id) == 0) {
            continue;
        }
        if (write_index != service_index) {
            registry->services[write_index] = registry->services[service_index];
        }
        write_index += 1;
    }
    registry->service_count = write_index;
    return 0;
}

int kain_host_bridge_register_service(
    KainHostBridgeRegistry* registry,
    const KainHostBridgeServiceDescriptor* descriptor,
    KainDiagnostic* diag
) {
    KainHostBridgeServiceDescriptor* service;

    if (!registry || !descriptor || descriptor->service_key[0] == '\0') {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge service registration failed",
            "Registry and service descriptor are required."
        );
        return -1;
    }

    if (!kain_host_bridge_lookup_module(registry, descriptor->module_id)) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge service registration failed",
            "Service owner module must be installed before service registration."
        );
        return -1;
    }

    if (registry->service_count >= KAIN_HOST_BRIDGE_MAX_SERVICES) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge service registration failed",
            "Host bridge service registry is full."
        );
        return -1;
    }

    if (kain_host_bridge_lookup_service(registry, descriptor->service_key)) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED,
            "Host bridge service registration failed",
            "Service key is already registered."
        );
        return -1;
    }

    if (!kain_runtime_version_check_abi_compatibility(descriptor->abi_version)) {
        kain_host_bridge_set_diag(
            diag,
            KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH,
            "Host bridge service ABI mismatch",
            "Service requires an incompatible runtime ABI version."
        );
        return -1;
    }

    service = &registry->services[registry->service_count];
    *service = *descriptor;
    registry->service_count += 1;
    return 0;
}

const KainHostBridgeInstalledModule* kain_host_bridge_lookup_module(
    const KainHostBridgeRegistry* registry,
    const char* module_id
) {
    int index;
    if (!registry || !module_id) {
        return NULL;
    }
    for (index = 0; index < registry->module_count; ++index) {
        if (strcmp(registry->modules[index].descriptor.module_id, module_id) == 0) {
            return &registry->modules[index];
        }
    }
    return NULL;
}

const KainHostBridgeServiceDescriptor* kain_host_bridge_lookup_service(
    const KainHostBridgeRegistry* registry,
    const char* service_key
) {
    int index;
    if (!registry || !service_key) {
        return NULL;
    }
    for (index = 0; index < registry->service_count; ++index) {
        if (strcmp(registry->services[index].service_key, service_key) == 0) {
            return &registry->services[index];
        }
    }
    return NULL;
}

int kain_host_bridge_count_services_for_module(
    const KainHostBridgeRegistry* registry,
    const char* module_id
) {
    int count = 0;
    int index;
    if (!registry || !module_id) {
        return 0;
    }
    for (index = 0; index < registry->service_count; ++index) {
        if (strcmp(registry->services[index].module_id, module_id) == 0) {
            count += 1;
        }
    }
    return count;
}

const KainForeignBridgeContract* kain_host_bridge_contract_for_lane(
    KainForeignRuntimeLane lane
) {
    size_t index;
    for (index = 0; index < sizeof(KAIN_FOREIGN_CONTRACTS) / sizeof(KAIN_FOREIGN_CONTRACTS[0]); ++index) {
        if (KAIN_FOREIGN_CONTRACTS[index].lane == lane) {
            return &KAIN_FOREIGN_CONTRACTS[index];
        }
    }
    return NULL;
}

const char* kain_host_bridge_lane_name(KainForeignRuntimeLane lane) {
    const KainForeignBridgeContract* contract = kain_host_bridge_contract_for_lane(lane);
    if (!contract) {
        return "unknown";
    }
    return contract->lane_name;
}
