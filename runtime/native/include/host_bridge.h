#ifndef HOST_BRIDGE_H
#define HOST_BRIDGE_H

#include "diagnostics.h"
#include "services.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_HOST_BRIDGE_MODULE_ID_MAX 128
#define KAIN_HOST_BRIDGE_MODULE_NAME_MAX 128
#define KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES 8
#define KAIN_HOST_BRIDGE_MAX_MODULES 32
#define KAIN_HOST_BRIDGE_MAX_SERVICES 64
#define KAIN_HOST_BRIDGE_TEXT_MAX 128

typedef enum {
    KAIN_FOREIGN_RUNTIME_UNKNOWN = 0,
    KAIN_FOREIGN_RUNTIME_RUST,
    KAIN_FOREIGN_RUNTIME_PYTHON,
    KAIN_FOREIGN_RUNTIME_NODE,
    KAIN_FOREIGN_RUNTIME_C,
    KAIN_FOREIGN_RUNTIME_ZIG,
} KainForeignRuntimeLane;

typedef enum {
    KAIN_HOST_BRIDGE_MODULE_UNINSTALLED = 0,
    KAIN_HOST_BRIDGE_MODULE_INSTALLED,
    KAIN_HOST_BRIDGE_MODULE_ACTIVE,
    KAIN_HOST_BRIDGE_MODULE_FAILED,
} KainHostBridgeModuleState;

typedef struct {
    char module_id[KAIN_HOST_BRIDGE_MODULE_ID_MAX];
    char module_name[KAIN_HOST_BRIDGE_MODULE_NAME_MAX];
    KainServiceProvider provider;
    KainForeignRuntimeLane lane;
    unsigned int abi_version;
    unsigned int required_capability_mask;
    int required_service_count;
    char required_services[KAIN_HOST_BRIDGE_MAX_REQUIRED_SERVICES][KAIN_SERVICE_KEY_MAX];
    int hot_reload_capable;
} KainHostBridgeModuleDescriptor;

typedef struct {
    char service_key[KAIN_SERVICE_KEY_MAX];
    char service_name[KAIN_SERVICE_NAME_MAX];
    char module_id[KAIN_HOST_BRIDGE_MODULE_ID_MAX];
    KainServiceProvider provider;
    unsigned int abi_version;
    unsigned int capability_mask;
    void* function_table;
} KainHostBridgeServiceDescriptor;

typedef struct {
    KainHostBridgeModuleDescriptor descriptor;
    KainHostBridgeModuleState state;
} KainHostBridgeInstalledModule;

typedef struct {
    int initialized;
    int module_count;
    int service_count;
    KainHostBridgeInstalledModule modules[KAIN_HOST_BRIDGE_MAX_MODULES];
    KainHostBridgeServiceDescriptor services[KAIN_HOST_BRIDGE_MAX_SERVICES];
} KainHostBridgeRegistry;

typedef struct {
    KainForeignRuntimeLane lane;
    char lane_name[32];
    char marshaling_model[KAIN_HOST_BRIDGE_TEXT_MAX];
    char ownership_model[KAIN_HOST_BRIDGE_TEXT_MAX];
    char failure_model[KAIN_HOST_BRIDGE_TEXT_MAX];
} KainForeignBridgeContract;

void kain_host_bridge_registry_init(KainHostBridgeRegistry* registry);
void kain_host_bridge_module_descriptor_init(KainHostBridgeModuleDescriptor* descriptor);
void kain_host_bridge_service_descriptor_init(KainHostBridgeServiceDescriptor* descriptor);
int kain_host_bridge_module_add_required_service(
    KainHostBridgeModuleDescriptor* descriptor,
    const char* service_key
);
int kain_host_bridge_install_module(
    KainHostBridgeRegistry* registry,
    const KainServiceRegistry* runtime_services,
    const KainHostBridgeModuleDescriptor* descriptor,
    unsigned int available_capability_mask,
    KainDiagnostic* diag
);
int kain_host_bridge_activate_module(
    KainHostBridgeRegistry* registry,
    const char* module_id,
    KainDiagnostic* diag
);
int kain_host_bridge_unregister_module(
    KainHostBridgeRegistry* registry,
    const char* module_id,
    KainDiagnostic* diag
);
int kain_host_bridge_register_service(
    KainHostBridgeRegistry* registry,
    const KainHostBridgeServiceDescriptor* descriptor,
    KainDiagnostic* diag
);
const KainHostBridgeInstalledModule* kain_host_bridge_lookup_module(
    const KainHostBridgeRegistry* registry,
    const char* module_id
);
const KainHostBridgeServiceDescriptor* kain_host_bridge_lookup_service(
    const KainHostBridgeRegistry* registry,
    const char* service_key
);
int kain_host_bridge_count_services_for_module(
    const KainHostBridgeRegistry* registry,
    const char* module_id
);
const KainForeignBridgeContract* kain_host_bridge_contract_for_lane(
    KainForeignRuntimeLane lane
);
const char* kain_host_bridge_lane_name(KainForeignRuntimeLane lane);

#ifdef __cplusplus
}
#endif

#endif /* HOST_BRIDGE_H */
