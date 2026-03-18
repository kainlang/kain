#include "../../include/kain_runtime_platform.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    KainPlatformKind kind;
    int supported;
    unsigned int supported_service_mask;
    unsigned int optional_service_mask;
    const char* name;
    const char* family;
    const char* diagnostic;
} KainPlatformDescriptorTemplate;

static const KainPlatformDescriptorTemplate KAIN_PLATFORM_TEMPLATES[] = {
    {
        KAIN_PLATFORM_KIND_UNKNOWN,
        0,
        KAIN_PLATFORM_SERVICE_FILESYSTEM | KAIN_PLATFORM_SERVICE_PROCESS | KAIN_PLATFORM_SERVICE_TIMERS,
        0,
        "unknown",
        "stub",
        "Unknown platform kind"
    },
    {
        KAIN_PLATFORM_KIND_WIN32,
        1,
        KAIN_PLATFORM_SERVICE_APP_HOST |
        KAIN_PLATFORM_SERVICE_INPUT |
        KAIN_PLATFORM_SERVICE_VIEWPORT |
        KAIN_PLATFORM_SERVICE_GRAPHICS |
        KAIN_PLATFORM_SERVICE_FILESYSTEM |
        KAIN_PLATFORM_SERVICE_PROCESS |
        KAIN_PLATFORM_SERVICE_TIMERS |
        KAIN_PLATFORM_SERVICE_NETWORK |
        KAIN_PLATFORM_SERVICE_CLIPBOARD,
        KAIN_PLATFORM_SERVICE_HOT_RELOAD,
        "win32",
        "native",
        "Win32 runtime lane is available"
    },
    {
        KAIN_PLATFORM_KIND_LINUX,
        0,
        KAIN_PLATFORM_SERVICE_FILESYSTEM |
        KAIN_PLATFORM_SERVICE_PROCESS |
        KAIN_PLATFORM_SERVICE_TIMERS |
        KAIN_PLATFORM_SERVICE_NETWORK,
        0,
        "linux",
        "stub",
        "Linux native lane is currently a stub and does not provide app host, input, viewport, or graphics services"
    },
    {
        KAIN_PLATFORM_KIND_MACOS,
        0,
        KAIN_PLATFORM_SERVICE_FILESYSTEM |
        KAIN_PLATFORM_SERVICE_PROCESS |
        KAIN_PLATFORM_SERVICE_TIMERS |
        KAIN_PLATFORM_SERVICE_NETWORK,
        0,
        "macos",
        "stub",
        "macOS native lane is currently a stub and does not provide app host, input, viewport, or graphics services"
    },
};

static const char* KAIN_PLATFORM_SERVICE_NAMES[] = {
    "app_host",
    "input",
    "viewport",
    "graphics",
    "filesystem",
    "process",
    "timers",
    "network",
    "clipboard",
    "hot_reload",
};

static void kain_platform_append_text(char* out, size_t out_cap, const char* text) {
    size_t len;

    if (out == NULL || text == NULL || out_cap == 0) {
        return;
    }

    len = strlen(out);
    if (len >= out_cap - 1) {
        return;
    }

    snprintf(out + len, out_cap - len, "%s", text);
}

static const KainPlatformDescriptorTemplate* kain_platform_template_for_kind(KainPlatformKind kind) {
    size_t i;
    for (i = 0; i < sizeof(KAIN_PLATFORM_TEMPLATES) / sizeof(KAIN_PLATFORM_TEMPLATES[0]); i++) {
        if (KAIN_PLATFORM_TEMPLATES[i].kind == kind) {
            return &KAIN_PLATFORM_TEMPLATES[i];
        }
    }
    return &KAIN_PLATFORM_TEMPLATES[0];
}

static void kain_platform_copy_template(
    const KainPlatformDescriptorTemplate* template_value,
    KainPlatformDescriptor* out
) {
    if (out == NULL || template_value == NULL) {
        return;
    }

    out->kind = template_value->kind;
    out->supported = template_value->supported;
    out->supported_service_mask = template_value->supported_service_mask;
    out->optional_service_mask = template_value->optional_service_mask;
    snprintf(out->name, sizeof(out->name), "%s", template_value->name);
    snprintf(out->family, sizeof(out->family), "%s", template_value->family);
    snprintf(out->diagnostic, sizeof(out->diagnostic), "%s", template_value->diagnostic);
}

static const KainPlatformDescriptorTemplate* kain_platform_current_template(void) {
#if defined(_WIN32)
    return kain_platform_template_for_kind(KAIN_PLATFORM_KIND_WIN32);
#elif defined(__APPLE__)
    return kain_platform_template_for_kind(KAIN_PLATFORM_KIND_MACOS);
#elif defined(__linux__)
    return kain_platform_template_for_kind(KAIN_PLATFORM_KIND_LINUX);
#else
    return kain_platform_template_for_kind(KAIN_PLATFORM_KIND_UNKNOWN);
#endif
}

void kain_platform_descriptor_init(KainPlatformDescriptor* descriptor) {
    if (descriptor == NULL) {
        return;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    kain_platform_copy_template(kain_platform_template_for_kind(KAIN_PLATFORM_KIND_UNKNOWN), descriptor);
}

const char* kain_platform_kind_name(KainPlatformKind kind) {
    return kain_platform_template_for_kind(kind)->name;
}

void kain_platform_describe_kind(KainPlatformKind kind, KainPlatformDescriptor* out) {
    if (out == NULL) {
        return;
    }
    kain_platform_copy_template(kain_platform_template_for_kind(kind), out);
}

void kain_platform_describe_current(KainPlatformDescriptor* out) {
    if (out == NULL) {
        return;
    }
    kain_platform_copy_template(kain_platform_current_template(), out);
}

KainPlatformKind kain_platform_current_kind(void) {
    return kain_platform_current_template()->kind;
}

unsigned int kain_platform_current_service_mask(void) {
    return kain_platform_current_template()->supported_service_mask;
}

unsigned int kain_platform_current_optional_service_mask(void) {
    return kain_platform_current_template()->optional_service_mask;
}

int kain_platform_is_current_kind(KainPlatformKind kind) {
    return kain_platform_current_kind() == kind;
}

int kain_platform_supports_kind(KainPlatformKind kind, unsigned int service_mask) {
    const KainPlatformDescriptorTemplate* template_value = kain_platform_template_for_kind(kind);
    return (template_value->supported_service_mask & service_mask) == service_mask;
}

void kain_platform_format_service_mask(unsigned int service_mask, char* out, size_t out_cap) {
    size_t i;
    int first = 1;

    if (out == NULL || out_cap == 0) {
        return;
    }

    out[0] = '\0';
    for (i = 0; i < sizeof(KAIN_PLATFORM_SERVICE_NAMES) / sizeof(KAIN_PLATFORM_SERVICE_NAMES[0]); i++) {
        unsigned int bit = 1u << i;
        if ((service_mask & bit) == 0) {
            continue;
        }
        if (!first) {
            kain_platform_append_text(out, out_cap, ",");
        }
        kain_platform_append_text(out, out_cap, KAIN_PLATFORM_SERVICE_NAMES[i]);
        first = 0;
    }

    if (first) {
        snprintf(out, out_cap, "none");
    }
}

static void kain_platform_require_detail(
    unsigned int required_mask,
    unsigned int supported_mask,
    char* out,
    size_t out_cap
) {
    unsigned int missing_mask = required_mask & ~supported_mask;
    char missing_text[256];

    if (out == NULL || out_cap == 0) {
        return;
    }

    kain_platform_format_service_mask(missing_mask, missing_text, sizeof(missing_text));
    snprintf(out, out_cap, "missing platform services: %s", missing_text);
}

int kain_platform_require_kind(KainPlatformKind kind, unsigned int required_mask, KainDiagnostic* diag) {
    const KainPlatformDescriptorTemplate* template_value = kain_platform_template_for_kind(kind);
    if ((template_value->supported_service_mask & required_mask) == required_mask) {
        return 0;
    }

    if (diag != NULL) {
        char detail[256];
        kain_diagnostic_create(
            diag,
            KAIN_DIAG_SUBSYSTEM_PLATFORM,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED,
            "Platform service unavailable",
            NULL,
            NULL
        );
        kain_platform_require_detail(required_mask, template_value->supported_service_mask, detail, sizeof(detail));
        snprintf(diag->detail, sizeof(diag->detail), "%s", detail);
        snprintf(diag->source_path, sizeof(diag->source_path), "%s", template_value->name);
    }

    return -1;
}

int kain_platform_require_current(unsigned int required_mask, KainDiagnostic* diag) {
    return kain_platform_require_kind(kain_platform_current_kind(), required_mask, diag);
}
