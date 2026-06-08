#include "../../include/diagnostics.h"
#include "../../include/version.h"
#include "../../include/base.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static KainDiagSeverity kain_diag_clamp_severity(int severity) {
    if (severity < KAIN_DIAG_SEVERITY_INFO) {
        return KAIN_DIAG_SEVERITY_INFO;
    }
    if (severity > KAIN_DIAG_SEVERITY_FATAL) {
        return KAIN_DIAG_SEVERITY_FATAL;
    }
    return (KainDiagSeverity)severity;
}

static void kain_diag_normalize_channel(KainDiagChannel* channel) {
    KainDiagSeverity file_level;
    KainDiagSeverity tty_level;
    KainDiagSeverity popup_level;
    if (!channel) {
        return;
    }
    file_level = kain_diag_clamp_severity(channel->file_level);
    tty_level = kain_diag_clamp_severity(channel->tty_level);
    popup_level = kain_diag_clamp_severity(channel->popup_level);
    if (tty_level < file_level) {
        tty_level = file_level;
    }
    if (popup_level < tty_level) {
        popup_level = tty_level;
    }
    channel->file_level = (uint8_t)file_level;
    channel->tty_level = (uint8_t)tty_level;
    channel->popup_level = (uint8_t)popup_level;
    channel->emit_level = (uint8_t)file_level;
}

static KainDiagChannel KAIN_DIAG_CHANNELS[] = {
    { KAIN_DIAG_SUBSYSTEM_UNKNOWN,       KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_CONTRACT,      KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_REFLECTION,    KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_ACTOR,         KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_ASYNC,         KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_UI,            KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_GFX,           KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_PLATFORM,      KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE,   KAIN_DIAG_SEVERITY_INFO,    KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_INFO,    KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_MEMORY,        KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_COMPATIBILITY, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_FIXUP,         KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
    { KAIN_DIAG_SUBSYSTEM_PROFILE,       KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_FATAL,   KAIN_DIAG_SEVERITY_ERROR,   KAIN_RUNTIME_TIER_FULL },
    { KAIN_DIAG_SUBSYSTEM_MACHINE,       KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_WARNING, KAIN_DIAG_SEVERITY_ERROR,   KAIN_DIAG_SEVERITY_WARNING, KAIN_RUNTIME_TIER_GATED },
};

static size_t kain_diag_channel_count(void) {
    return sizeof(KAIN_DIAG_CHANNELS) / sizeof(KAIN_DIAG_CHANNELS[0]);
}

static KainDiagChannel* kain_diag_channel_mutable(KainDiagSubsystem subsystem) {
    size_t index;
    for (index = 0u; index < kain_diag_channel_count(); ++index) {
        if (KAIN_DIAG_CHANNELS[index].subsystem == subsystem) {
            return &KAIN_DIAG_CHANNELS[index];
        }
    }
    return &KAIN_DIAG_CHANNELS[0];
}

const KainDiagChannel* kain_diagnostic_channel(KainDiagSubsystem subsystem) {
    return kain_diag_channel_mutable(subsystem);
}

int kain_diagnostic_channel_should_emit(KainDiagSubsystem subsystem, KainDiagSeverity severity) {
    const KainDiagChannel* channel;
#if KAIN_RUNTIME_DIAG_TIER == KAIN_RUNTIME_TIER_NOOP
    return severity >= KAIN_DIAG_SEVERITY_FATAL;
#else
    channel = kain_diagnostic_channel(subsystem);
    if (!channel) {
        return severity >= KAIN_DIAG_SEVERITY_WARNING;
    }
    if (!KAIN_RUNTIME_TIER_AT_LEAST(KAIN_RUNTIME_DIAG_TIER, channel->runtime_tier)) {
        return severity >= KAIN_DIAG_SEVERITY_FATAL;
    }
    return severity >= (KainDiagSeverity)channel->emit_level;
#endif
}

int kain_diagnostic_channel_set_levels(
    KainDiagSubsystem subsystem,
    KainDiagSeverity file_level,
    KainDiagSeverity tty_level,
    KainDiagSeverity popup_level
) {
    KainDiagChannel* channel = kain_diag_channel_mutable(subsystem);
    if (!channel) {
        return -1;
    }
    channel->file_level = (uint8_t)file_level;
    channel->tty_level = (uint8_t)tty_level;
    channel->popup_level = (uint8_t)popup_level;
    kain_diag_normalize_channel(channel);
    return 0;
}

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

    kain_copy_text(diag->message, sizeof(diag->message), message);
    kain_copy_text(diag->detail, sizeof(diag->detail), detail);
    kain_copy_text(diag->source_path, sizeof(diag->source_path), source_path);

    /* Capture runtime ABI version */
    if (version_get_info(&version_info) == 0) {
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
    const KainDiagChannel* channel;

    if (!diag) {
        return;
    }

    if (!kain_diagnostic_channel_should_emit(diag->subsystem, diag->severity)) {
        return;
    }

    channel = kain_diagnostic_channel(diag->subsystem);
    if (!channel || diag->severity < (KainDiagSeverity)channel->tty_level) {
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
        case KAIN_DIAG_SUBSYSTEM_FIXUP:         return "FIXUP";
        case KAIN_DIAG_SUBSYSTEM_PROFILE:       return "PROFILE";
        case KAIN_DIAG_SUBSYSTEM_MACHINE:       return "MACHINE";
        case KAIN_DIAG_SUBSYSTEM_CRASH:         return "CRASH";
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

/* Diagnostic Collector Implementation */

void kain_diagnostic_collector_init(KainDiagnosticCollector* collector) {
    if (!collector) {
        return;
    }
    ZeroMemory(collector, sizeof(*collector));
}

int kain_diagnostic_collector_add(
    KainDiagnosticCollector* collector,
    const KainDiagnostic* diag
) {
    if (!collector || !diag) {
        return -1;
    }

    if (collector->count >= KAIN_DIAG_COLLECTOR_MAX_DIAGNOSTICS) {
        return -1;
    }

    /* Copy diagnostic */
    memcpy(&collector->diagnostics[collector->count], diag, sizeof(KainDiagnostic));
    collector->count++;

    /* Update severity counters */
    switch (diag->severity) {
        case KAIN_DIAG_SEVERITY_ERROR:
            collector->error_count++;
            break;
        case KAIN_DIAG_SEVERITY_WARNING:
            collector->warning_count++;
            break;
        case KAIN_DIAG_SEVERITY_FATAL:
            collector->fatal_count++;
            break;
        default:
            break;
    }

    return 0;
}

int kain_diagnostic_collector_add_new(
    KainDiagnosticCollector* collector,
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail,
    const char* source_path
) {
    KainDiagnostic diag;

    if (!collector) {
        return -1;
    }

    kain_diagnostic_create(&diag, subsystem, severity, code, message, detail, source_path);
    return kain_diagnostic_collector_add(collector, &diag);
}

int kain_diagnostic_collector_has_errors(const KainDiagnosticCollector* collector) {
    if (!collector) {
        return 0;
    }
    return collector->error_count > 0 || collector->fatal_count > 0;
}

int kain_diagnostic_collector_has_fatals(const KainDiagnosticCollector* collector) {
    if (!collector) {
        return 0;
    }
    return collector->fatal_count > 0;
}

int kain_diagnostic_collector_count_by_severity(
    const KainDiagnosticCollector* collector,
    KainDiagSeverity severity
) {
    if (!collector) {
        return 0;
    }

    switch (severity) {
        case KAIN_DIAG_SEVERITY_ERROR:
            return collector->error_count;
        case KAIN_DIAG_SEVERITY_WARNING:
            return collector->warning_count;
        case KAIN_DIAG_SEVERITY_FATAL:
            return collector->fatal_count;
        case KAIN_DIAG_SEVERITY_INFO:
            return collector->count - collector->error_count -
                   collector->warning_count - collector->fatal_count;
        default:
            return 0;
    }
}

void kain_diagnostic_collector_print_all(const KainDiagnosticCollector* collector) {
    int i;

    if (!collector) {
        return;
    }

    if (collector->count == 0) {
        printf("No diagnostics collected.\n");
        return;
    }

    printf("=== Collected Diagnostics (%d total) ===\n", collector->count);
    printf("Errors: %d, Warnings: %d, Fatals: %d\n\n",
        collector->error_count, collector->warning_count, collector->fatal_count);

    for (i = 0; i < collector->count; i++) {
        const KainDiagChannel* channel =
            kain_diagnostic_channel(collector->diagnostics[i].subsystem);
        if (channel &&
            kain_diagnostic_channel_should_emit(
                collector->diagnostics[i].subsystem,
                collector->diagnostics[i].severity
            ) &&
            collector->diagnostics[i].severity >= (KainDiagSeverity)channel->tty_level) {
            kain_diagnostic_print(&collector->diagnostics[i]);
            printf("\n");
        }
    }
}

int kain_diagnostic_collector_format_summary(
    const KainDiagnosticCollector* collector,
    char* out,
    size_t out_size
) {
    if (!collector || !out || out_size == 0) {
        return 0;
    }

    return snprintf(out, out_size,
        "Diagnostics: %d total (%d errors, %d warnings, %d fatals)",
        collector->count,
        collector->error_count,
        collector->warning_count,
        collector->fatal_count);
}

void kain_diagnostic_collector_clear(KainDiagnosticCollector* collector) {
    if (!collector) {
        return;
    }
    kain_diagnostic_collector_init(collector);
}

/* Startup Validation Result Implementation */

void kain_startup_validation_result_init(KainStartupValidationResult* result) {
    if (!result) {
        return;
    }
    ZeroMemory(result, sizeof(*result));
    kain_diagnostic_collector_init(&result->diagnostics);
}

int kain_startup_validation_result_format(
    const KainStartupValidationResult* result,
    char* out,
    size_t out_size
) {
    int written = 0;

    if (!result || !out || out_size == 0) {
        return 0;
    }

    /* Header */
    written = snprintf(out, out_size,
        "=== KAIN Runtime Startup Validation ===\n\n");

    /* Version Information */
    if (written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Runtime ABI Version: 0x%08X\n", result->runtime_abi_version);
    }

    if (written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Runtime Version: 0x%08X\n", result->runtime_version);
    }

    if (result->bundle_abi_version != 0 && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Bundle ABI Version: 0x%08X\n", result->bundle_abi_version);
    }

    /* Validation Status */
    if (written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "\nValidation Status: %s\n",
            result->validation_passed ? "PASSED" : "FAILED");
    }

    /* Service Status */
    if (written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Required Services Available: %d\n", result->required_services_available);
    }

    if (written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Optional Services Available: %d\n", result->optional_services_available);
    }

    if (result->optional_services_degraded > 0 && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "Optional Services Degraded: %d\n", result->optional_services_degraded);
    }

    /* Summary */
    if (result->summary[0] && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written,
            "\nSummary: %s\n", result->summary);
    }

    /* Diagnostics Summary */
    if (result->diagnostics.count > 0 && written > 0 && (size_t)written < out_size - 1) {
        written += snprintf(out + written, out_size - written, "\n");
        kain_diagnostic_collector_format_summary(&result->diagnostics,
            out + written, out_size - written);
        written = (int)strlen(out);
    }

    return written;
}

void kain_startup_validation_result_print(const KainStartupValidationResult* result) {
    char buffer[2048];

    if (!result) {
        return;
    }

    kain_startup_validation_result_format(result, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    /* Print detailed diagnostics if any */
    if (result->diagnostics.count > 0) {
        printf("\n");
        kain_diagnostic_collector_print_all(&result->diagnostics);
    }
}

