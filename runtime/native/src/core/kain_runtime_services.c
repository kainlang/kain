#include "../../include/kain_runtime_services.h"
#include "../../include/kain_runtime_base.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static void kain_copy_text(char* out, size_t out_size, const char* text) {
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

/* Global service registry singleton */
static KainServiceRegistry g_service_registry = {0};

void kain_service_registry_init(KainServiceRegistry* registry) {
    if (!registry) {
        return;
    }
    ZeroMemory(registry, sizeof(*registry));
    registry->initialized = 1;
}

int kain_service_registry_register(
    KainServiceRegistry* registry,
    const char* key,
    const char* name,
    const char* description,
    KainServiceProvider provider,
    KainServiceStatus status,
    KainServiceRequirement requirement,
    unsigned int abi_version,
    void* function_table
) {
    KainServiceDescriptor* descriptor;
    
    if (!registry || !key || !name) {
        return -1;
    }
    
    if (!registry->initialized) {
        kain_service_registry_init(registry);
    }
    
    /* Check if registry is full */
    if (registry->service_count >= KAIN_SERVICE_REGISTRY_MAX_SERVICES) {
        return -2;
    }
    
    /* Check if service already exists */
    if (kain_service_registry_lookup(registry, key) != NULL) {
        return -3;
    }
    
    /* Register the service */
    descriptor = &registry->services[registry->service_count];
    ZeroMemory(descriptor, sizeof(*descriptor));
    
    kain_copy_text(descriptor->key, sizeof(descriptor->key), key);
    kain_copy_text(descriptor->name, sizeof(descriptor->name), name);
    kain_copy_text(descriptor->description, sizeof(descriptor->description), description);
    
    descriptor->provider = provider;
    descriptor->status = status;
    descriptor->requirement = requirement;
    descriptor->abi_version = abi_version;
    descriptor->function_table = function_table;
    
    registry->service_count++;
    return 0;
}

const KainServiceDescriptor* kain_service_registry_lookup(
    const KainServiceRegistry* registry,
    const char* key
) {
    int i;
    
    if (!registry || !key) {
        return NULL;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        if (strcmp(registry->services[i].key, key) == 0) {
            return &registry->services[i];
        }
    }
    
    return NULL;
}

int kain_service_registry_is_available(
    const KainServiceRegistry* registry,
    const char* key
) {
    const KainServiceDescriptor* descriptor = kain_service_registry_lookup(registry, key);
    if (!descriptor) {
        return 0;
    }
    return descriptor->status == KAIN_SERVICE_STATUS_AVAILABLE;
}

KainServiceStatus kain_service_registry_get_status(
    const KainServiceRegistry* registry,
    const char* key
) {
    const KainServiceDescriptor* descriptor = kain_service_registry_lookup(registry, key);
    if (!descriptor) {
        return KAIN_SERVICE_STATUS_UNAVAILABLE;
    }
    return descriptor->status;
}

int kain_service_registry_count_by_status(
    const KainServiceRegistry* registry,
    KainServiceStatus status
) {
    int count = 0;
    int i;
    
    if (!registry) {
        return 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        if (registry->services[i].status == status) {
            count++;
        }
    }
    
    return count;
}

int kain_service_registry_count_by_requirement(
    const KainServiceRegistry* registry,
    KainServiceRequirement requirement
) {
    int count = 0;
    int i;
    
    if (!registry) {
        return 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        if (registry->services[i].requirement == requirement) {
            count++;
        }
    }
    
    return count;
}

int kain_service_registry_validate_required(
    const KainServiceRegistry* registry,
    KainDiagnostic* diagnostics,
    int max_diagnostics,
    int* diagnostic_count
) {
    int i;
    int failures = 0;
    int diag_idx = 0;
    
    if (!registry) {
        return -1;
    }
    
    if (diagnostic_count) {
        *diagnostic_count = 0;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];
        
        /* Only check required services */
        if (service->requirement != KAIN_SERVICE_REQUIREMENT_REQUIRED) {
            continue;
        }
        
        /* Check if service is available */
        if (service->status == KAIN_SERVICE_STATUS_AVAILABLE) {
            continue;
        }
        
        /* Service is required but not available */
        failures++;
        
        /* Add diagnostic if space available */
        if (diagnostics && diag_idx < max_diagnostics) {
            char message[KAIN_DIAG_MESSAGE_MAX];
            char detail[KAIN_DIAG_DETAIL_MAX];
            
            snprintf(message, sizeof(message),
                "Required service '%s' is not available", service->key);
            
            snprintf(detail, sizeof(detail),
                "Service: %s\nStatus: %s\nProvider: %d",
                service->name,
                service->status == KAIN_SERVICE_STATUS_UNAVAILABLE ? "unavailable" :
                service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" : "failed",
                service->provider);
            
            kain_diagnostic_create(
                &diagnostics[diag_idx],
                KAIN_DIAG_SUBSYSTEM_CONTRACT,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
                message,
                detail,
                NULL
            );
            
            diag_idx++;
        }
    }
    
    if (diagnostic_count) {
        *diagnostic_count = diag_idx;
    }
    
    return failures;
}

int kain_service_registry_validate_required_collector(
    const KainServiceRegistry* registry,
    KainDiagnosticCollector* collector
) {
    int i;
    int failures = 0;
    
    if (!registry || !collector) {
        return -1;
    }
    
    for (i = 0; i < registry->service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];
        
        /* Only check required services */
        if (service->requirement != KAIN_SERVICE_REQUIREMENT_REQUIRED) {
            continue;
        }
        
        /* Check if service is available */
        if (service->status == KAIN_SERVICE_STATUS_AVAILABLE) {
            continue;
        }
        
        /* Service is required but not available */
        failures++;
        
        /* Add diagnostic to collector */
        char message[KAIN_DIAG_MESSAGE_MAX];
        char detail[KAIN_DIAG_DETAIL_MAX];
        
        snprintf(message, sizeof(message),
            "Required service '%s' is not available", service->key);
        
        snprintf(detail, sizeof(detail),
            "Service: %s\nStatus: %s\nProvider: %d",
            service->name,
            service->status == KAIN_SERVICE_STATUS_UNAVAILABLE ? "unavailable" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" : "failed",
            service->provider);
        
        kain_diagnostic_collector_add_new(
            collector,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
            message,
            detail,
            NULL
        );
    }
    
    return failures;
}

int kain_service_registry_format_list(
    const KainServiceRegistry* registry,
    char* out,
    size_t out_size
) {
    int i;
    size_t written = 0;
    
    if (!registry || !out || out_size == 0) {
        return 0;
    }
    
    out[0] = '\0';
    
    for (i = 0; i < registry->service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];
        char line[256];
        int line_len;
        
        line_len = snprintf(line, sizeof(line),
            "%s%s (%s) - %s\n",
            i > 0 ? "" : "",
            service->key,
            service->status == KAIN_SERVICE_STATUS_AVAILABLE ? "available" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" :
            service->status == KAIN_SERVICE_STATUS_FAILED ? "failed" : "unavailable",
            service->name);
        
        if (written + line_len >= out_size - 1) {
            break;
        }
        
        memcpy(out + written, line, (size_t)line_len);
        written += (size_t)line_len;
        out[written] = '\0';
    }
    
    return (int)written;
}

void kain_service_registry_print(const KainServiceRegistry* registry) {
    int i;
    
    if (!registry) {
        printf("Service registry is NULL\n");
        return;
    }
    
    printf("=== KAIN Service Registry ===\n");
    printf("Services registered: %d / %d\n\n",
        registry->service_count, KAIN_SERVICE_REGISTRY_MAX_SERVICES);
    
    for (i = 0; i < registry->service_count; i++) {
        const KainServiceDescriptor* service = &registry->services[i];
        
        printf("Service %d:\n", i + 1);
        printf("  Key:         %s\n", service->key);
        printf("  Name:        %s\n", service->name);
        printf("  Description: %s\n", service->description[0] ? service->description : "(none)");
        printf("  Provider:    %d\n", service->provider);
        printf("  Status:      %s\n",
            service->status == KAIN_SERVICE_STATUS_AVAILABLE ? "available" :
            service->status == KAIN_SERVICE_STATUS_DEGRADED ? "degraded" :
            service->status == KAIN_SERVICE_STATUS_FAILED ? "failed" : "unavailable");
        printf("  Requirement: %s\n",
            service->requirement == KAIN_SERVICE_REQUIREMENT_REQUIRED ? "required" : "optional");
        printf("  ABI Version: 0x%08X\n", service->abi_version);
        printf("\n");
    }
}

KainServiceRegistry* kain_service_registry_global(void) {
    if (!g_service_registry.initialized) {
        kain_service_registry_init(&g_service_registry);
    }
    return &g_service_registry;
}

