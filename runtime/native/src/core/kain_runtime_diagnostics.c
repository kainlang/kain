#include "../../include/kain_runtime_diagnostics.h"
#include "../../include/kain_runtime_version.h"
#include "../../include/kain_runtime_base.h"
#include <string.h>
#include <stdio.h>

void kain_diagnostic_init(KainDiagnostic* diag) {
    if (!diag) {
        return;
    }
    ZeroMemory(diag, sizeof(*diag));
    diag->subsystem = KAIN_DIAG_SUBSYSTEM_UNKNOWN;
    diag->severity = KAIN_DIAG_SEVERITY_INFO;
    diag->code = KAIN_DIAG_CODE_SUCCESS;
}

void kain_diagnostic_create(
    KainDiagnostic* diag,
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail,
    const char* source_path
) {
    KainRuntimeVersionInfo version_info;
    
    if (!diag) {
        return;
    }
    
    kain_diagnostic_init(diag);
    
    diag->subsystem = subsystem;
    diag->severity = severity;
    diag->code = code;
    
    if (message) {
        strncpy(diag->message, message, KAIN_DIAG_MESSAGE_MAX - 1);
        diag->message[KAIN_DIAG_MESSAGE_MAX - 1] = '\0';
    }
    
    if (detail) {
        strncpy(diag->detail, detail, KAIN_DIAG_DETAIL_MAX - 1);
        diag->detail[KAIN_DIAG_DETAIL_MAX - 1] = '\0';
    }
    
    if (source_path) {
        strncpy(diag->source_path, source_path, KAIN_DIAG_SOURCE_PATH_MAX - 1);
        diag->source_path[KAIN_DIAG_SOURCE_PATH_MAX - 1] = '\0';
    }
    
    /* Capture runtime ABI version */
    if (kain_runtime_version_get_info(&version_info) == 0) {
        diag->runtime_abi_version = version_info.abi_version_encoded;
    }
}

int kain_diagnostic_format(
    const KainDiagnostic* diag,
    char* out,
    size_t out_size
) {
    int written = 0;
    
    if (!diag || !out || out_size == 0) {
        return 0;
    }
    
    written = snprintf(out, out_size,
        "[%s] %s: %s",
        kain_diagnostic_subsystem_name(diag->subsystem),
        kain_diagnostic_severity_name(diag->severity),
        diag->message);
    
    if (diag->detail[0] && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "\n  Detail: %s", diag->detail);
    }
    
    if (diag->source_path[0] && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "\n  Source: %s", diag->source_path);
    }
    
    if (diag->code != KAIN_DIAG_CODE_SUCCESS && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "\n  Code: %d", diag->code);
    }
    
    return written;
}

void kain_diagnostic_print(const KainDiagnostic* diag) {
    char buffer[2048];
    
    if (!diag) {
        return;
    }
    
    kain_diagnostic_format(diag, buffer, sizeof(buffer));
    
    /* Print to stderr for errors and fatal, stdout for info/warning */
    if (diag->severity >= KAIN_DIAG_SEVERITY_ERROR) {
        fprintf(stderr, "%s\n", buffer);
    } else {
        printf("%s\n", buffer);
    }
}

const char* kain_diagnostic_subsystem_name(KainDiagSubsystem subsystem) {
    switch (subsystem) {
        case KAIN_DIAG_SUBSYSTEM_CONTRACT:      return "CONTRACT";
        case KAIN_DIAG_SUBSYSTEM_REFLECTION:    return "REFLECTION";
        case KAIN_DIAG_SUBSYSTEM_ACTOR:         return "ACTOR";
        case KAIN_DIAG_SUBSYSTEM_ASYNC:         return "ASYNC";
        case KAIN_DIAG_SUBSYSTEM_UI:            return "UI";
        case KAIN_DIAG_SUBSYSTEM_GFX:           return "GFX";
        case KAIN_DIAG_SUBSYSTEM_PLATFORM:      return "PLATFORM";
        case KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE:   return "HOST_BRIDGE";
        case KAIN_DIAG_SUBSYSTEM_MEMORY:        return "MEMORY";
        case KAIN_DIAG_SUBSYSTEM_COMPATIBILITY: return "COMPATIBILITY";
        case KAIN_DIAG_SUBSYSTEM_UNKNOWN:
        default:                                 return "UNKNOWN";
    }
}

const char* kain_diagnostic_severity_name(KainDiagSeverity severity) {
    switch (severity) {
        case KAIN_DIAG_SEVERITY_INFO:    return "INFO";
        case KAIN_DIAG_SEVERITY_WARNING: return "WARNING";
        case KAIN_DIAG_SEVERITY_ERROR:   return "ERROR";
        case KAIN_DIAG_SEVERITY_FATAL:   return "FATAL";
        default:                          return "UNKNOWN";
    }
}

