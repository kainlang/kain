#ifndef KAIN_RUNTIME_PLATFORM_H
#define KAIN_RUNTIME_PLATFORM_H

#include "kain_runtime_diagnostics.h"
#include <stddef.h>

/*
 * KAIN Native Runtime Platform ABI
 *
 * This header defines a narrow platform boundary for capability discovery and
 * unsupported-feature diagnostics. It gives the runtime a portable way to talk
 * about current platform state without embedding Win32 assumptions directly
 * into every caller.
 */

typedef enum {
    KAIN_PLATFORM_KIND_UNKNOWN = 0,
    KAIN_PLATFORM_KIND_WIN32,
    KAIN_PLATFORM_KIND_LINUX,
    KAIN_PLATFORM_KIND_MACOS,
} KainPlatformKind;

typedef enum {
    KAIN_PLATFORM_SERVICE_APP_HOST = 1u << 0,
    KAIN_PLATFORM_SERVICE_INPUT = 1u << 1,
    KAIN_PLATFORM_SERVICE_VIEWPORT = 1u << 2,
    KAIN_PLATFORM_SERVICE_GRAPHICS = 1u << 3,
    KAIN_PLATFORM_SERVICE_FILESYSTEM = 1u << 4,
    KAIN_PLATFORM_SERVICE_PROCESS = 1u << 5,
    KAIN_PLATFORM_SERVICE_TIMERS = 1u << 6,
    KAIN_PLATFORM_SERVICE_NETWORK = 1u << 7,
    KAIN_PLATFORM_SERVICE_CLIPBOARD = 1u << 8,
    KAIN_PLATFORM_SERVICE_HOT_RELOAD = 1u << 9,
} KainPlatformServiceMask;

typedef struct {
    KainPlatformKind kind;
    int supported;
    unsigned int supported_service_mask;
    unsigned int optional_service_mask;
    char name[32];
    char family[32];
    char diagnostic[256];
} KainPlatformDescriptor;

void kain_platform_descriptor_init(KainPlatformDescriptor* descriptor);
const char* kain_platform_kind_name(KainPlatformKind kind);
void kain_platform_describe_kind(KainPlatformKind kind, KainPlatformDescriptor* out);
void kain_platform_describe_current(KainPlatformDescriptor* out);
KainPlatformKind kain_platform_current_kind(void);
unsigned int kain_platform_current_service_mask(void);
unsigned int kain_platform_current_optional_service_mask(void);
int kain_platform_is_current_kind(KainPlatformKind kind);
int kain_platform_supports_kind(KainPlatformKind kind, unsigned int service_mask);
int kain_platform_require_kind(KainPlatformKind kind, unsigned int required_mask, KainDiagnostic* diag);
int kain_platform_require_current(unsigned int required_mask, KainDiagnostic* diag);
void kain_platform_format_service_mask(unsigned int service_mask, char* out, size_t out_cap);

#endif /* KAIN_RUNTIME_PLATFORM_H */
