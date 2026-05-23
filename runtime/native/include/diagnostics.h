#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stddef.h>

/*
 * KAIN Native Runtime Diagnostics Service
 *
 * This header defines the canonical diagnostics and error reporting service
 * for the KAIN native runtime. All runtime subsystems should emit structured
 * diagnostics through this interface rather than using ad hoc print/null-only
 * failure paths.
 *
 * Subsystems:
 * - contract: Runtime contract validation and loading
 * - reflection: Reflection payload loading and schema validation
 * - actor: Actor runtime operations (spawn, mailbox, supervision)
 * - async: Async/task/timer runtime operations
 * - ui: UI bundle loading and component runtime
 * - gfx: Graphics, shader, material, compute runtime
 * - platform: Platform-specific services (app host, input, window)
 * - host_bridge: Host/plugin bridge and foreign service integration
 * - memory: Low-level memory helpers and allocation
 * - compatibility: Hot reload, versioning, migration
 */

/* Diagnostic Subsystems */
typedef enum {
    KAIN_DIAG_SUBSYSTEM_UNKNOWN = 0,
    KAIN_DIAG_SUBSYSTEM_CONTRACT,
    KAIN_DIAG_SUBSYSTEM_REFLECTION,
    KAIN_DIAG_SUBSYSTEM_ACTOR,
    KAIN_DIAG_SUBSYSTEM_ASYNC,
    KAIN_DIAG_SUBSYSTEM_UI,
    KAIN_DIAG_SUBSYSTEM_GFX,
    KAIN_DIAG_SUBSYSTEM_PLATFORM,
    KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE,
    KAIN_DIAG_SUBSYSTEM_MEMORY,
    KAIN_DIAG_SUBSYSTEM_COMPATIBILITY,
} KainDiagSubsystem;

/* Diagnostic Severity Levels */
typedef enum {
    KAIN_DIAG_SEVERITY_INFO = 0,
    KAIN_DIAG_SEVERITY_WARNING,
    KAIN_DIAG_SEVERITY_ERROR,
    KAIN_DIAG_SEVERITY_FATAL,
} KainDiagSeverity;

/* Stable Error Code Families */
#define KAIN_DIAG_CODE_CONTRACT_BASE        1000
#define KAIN_DIAG_CODE_REFLECTION_BASE      2000
#define KAIN_DIAG_CODE_ACTOR_BASE           3000
#define KAIN_DIAG_CODE_ASYNC_BASE           4000
#define KAIN_DIAG_CODE_UI_BASE              5000
#define KAIN_DIAG_CODE_GFX_BASE             6000
#define KAIN_DIAG_CODE_PLATFORM_BASE        7000
#define KAIN_DIAG_CODE_HOST_BRIDGE_BASE     8000
#define KAIN_DIAG_CODE_MEMORY_BASE          9000
#define KAIN_DIAG_CODE_COMPATIBILITY_BASE   10000

/* Common Error Codes */
#define KAIN_DIAG_CODE_SUCCESS              0
#define KAIN_DIAG_CODE_GENERIC_ERROR        1

/* Contract Error Codes (1000-1999) */
#define KAIN_DIAG_CODE_CONTRACT_NOT_FOUND           (KAIN_DIAG_CODE_CONTRACT_BASE + 1)
#define KAIN_DIAG_CODE_CONTRACT_PARSE_FAILED        (KAIN_DIAG_CODE_CONTRACT_BASE + 2)
#define KAIN_DIAG_CODE_CONTRACT_INVALID_SCHEMA      (KAIN_DIAG_CODE_CONTRACT_BASE + 3)
#define KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE     (KAIN_DIAG_CODE_CONTRACT_BASE + 4)
#define KAIN_DIAG_CODE_CONTRACT_ABI_MISMATCH        (KAIN_DIAG_CODE_CONTRACT_BASE + 5)

/* Reflection Error Codes (2000-2999) */
#define KAIN_DIAG_CODE_REFLECTION_NOT_FOUND         (KAIN_DIAG_CODE_REFLECTION_BASE + 1)
#define KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED      (KAIN_DIAG_CODE_REFLECTION_BASE + 2)
#define KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA    (KAIN_DIAG_CODE_REFLECTION_BASE + 3)
#define KAIN_DIAG_CODE_REFLECTION_LOOKUP_FAILED     (KAIN_DIAG_CODE_REFLECTION_BASE + 4)

/* Actor Error Codes (3000-3999) */
#define KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED           (KAIN_DIAG_CODE_ACTOR_BASE + 1)
#define KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL           (KAIN_DIAG_CODE_ACTOR_BASE + 2)
#define KAIN_DIAG_CODE_ACTOR_INVALID_MESSAGE        (KAIN_DIAG_CODE_ACTOR_BASE + 3)
#define KAIN_DIAG_CODE_ACTOR_NOT_FOUND              (KAIN_DIAG_CODE_ACTOR_BASE + 4)
#define KAIN_DIAG_CODE_ACTOR_SHUTDOWN_FAILED        (KAIN_DIAG_CODE_ACTOR_BASE + 5)
#define KAIN_DIAG_CODE_ACTOR_MAILBOX_CLOSED         (KAIN_DIAG_CODE_ACTOR_BASE + 6)
#define KAIN_DIAG_CODE_ACTOR_MONITOR_FAILED         (KAIN_DIAG_CODE_ACTOR_BASE + 7)
#define KAIN_DIAG_CODE_ACTOR_LINK_FAILED            (KAIN_DIAG_CODE_ACTOR_BASE + 8)
#define KAIN_DIAG_CODE_ACTOR_REGISTRY_NAME_EXISTS   (KAIN_DIAG_CODE_ACTOR_BASE + 9)
#define KAIN_DIAG_CODE_ACTOR_REGISTRY_NOT_FOUND     (KAIN_DIAG_CODE_ACTOR_BASE + 10)
#define KAIN_DIAG_CODE_ACTOR_INVALID_STATE          (KAIN_DIAG_CODE_ACTOR_BASE + 11)
#define KAIN_DIAG_CODE_ACTOR_SUPERVISOR_FAILED      (KAIN_DIAG_CODE_ACTOR_BASE + 12)

/* Async Error Codes (4000-4999) */
#define KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED      (KAIN_DIAG_CODE_ASYNC_BASE + 1)
#define KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED         (KAIN_DIAG_CODE_ASYNC_BASE + 2)
#define KAIN_DIAG_CODE_ASYNC_TIMER_FAILED           (KAIN_DIAG_CODE_ASYNC_BASE + 3)
#define KAIN_DIAG_CODE_ASYNC_WAKE_FAILED            (KAIN_DIAG_CODE_ASYNC_BASE + 4)

/* UI Error Codes (5000-5999) */
#define KAIN_DIAG_CODE_UI_BUNDLE_NOT_FOUND          (KAIN_DIAG_CODE_UI_BASE + 1)
#define KAIN_DIAG_CODE_UI_BUNDLE_PARSE_FAILED       (KAIN_DIAG_CODE_UI_BASE + 2)
#define KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA     (KAIN_DIAG_CODE_UI_BASE + 3)
#define KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED     (KAIN_DIAG_CODE_UI_BASE + 4)

/* Graphics Error Codes (6000-6999) */
#define KAIN_DIAG_CODE_GFX_SHADER_LOAD_FAILED       (KAIN_DIAG_CODE_GFX_BASE + 1)
#define KAIN_DIAG_CODE_GFX_MATERIAL_LOAD_FAILED     (KAIN_DIAG_CODE_GFX_BASE + 2)
#define KAIN_DIAG_CODE_GFX_COMPUTE_DISPATCH_FAILED  (KAIN_DIAG_CODE_GFX_BASE + 3)
#define KAIN_DIAG_CODE_GFX_BINDING_FAILED           (KAIN_DIAG_CODE_GFX_BASE + 4)

/* Platform Error Codes (7000-7999) */
#define KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED         (KAIN_DIAG_CODE_PLATFORM_BASE + 1)
#define KAIN_DIAG_CODE_PLATFORM_INIT_FAILED         (KAIN_DIAG_CODE_PLATFORM_BASE + 2)
#define KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE (KAIN_DIAG_CODE_PLATFORM_BASE + 3)
#define KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT    (KAIN_DIAG_CODE_PLATFORM_BASE + 4)

/* Host Bridge Error Codes (8000-8999) */
#define KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED      (KAIN_DIAG_CODE_HOST_BRIDGE_BASE + 1)
#define KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH     (KAIN_DIAG_CODE_HOST_BRIDGE_BASE + 2)
#define KAIN_DIAG_CODE_HOST_BRIDGE_SERVICE_MISSING  (KAIN_DIAG_CODE_HOST_BRIDGE_BASE + 3)

/* Memory Error Codes (9000-9999) */
#define KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED          (KAIN_DIAG_CODE_MEMORY_BASE + 1)
#define KAIN_DIAG_CODE_MEMORY_INVALID_POINTER       (KAIN_DIAG_CODE_MEMORY_BASE + 2)
#define KAIN_DIAG_CODE_MEMORY_ALIGNMENT_ERROR       (KAIN_DIAG_CODE_MEMORY_BASE + 3)

/* Compatibility Error Codes (10000-10999) */
#define KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH      (KAIN_DIAG_CODE_COMPATIBILITY_BASE + 1)
#define KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED      (KAIN_DIAG_CODE_COMPATIBILITY_BASE + 2)
#define KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE   (KAIN_DIAG_CODE_COMPATIBILITY_BASE + 3)

/* String Buffer Sizes */
#define KAIN_DIAG_MESSAGE_MAX       256
#define KAIN_DIAG_DETAIL_MAX        512
#define KAIN_DIAG_SOURCE_PATH_MAX   512

/*
 * Diagnostic Record
 *
 * Structured diagnostic information emitted by runtime subsystems.
 * All fields should be populated for complete diagnostic reporting.
 */
typedef struct {
    KainDiagSubsystem subsystem;
    KainDiagSeverity severity;
    int code;
    char message[KAIN_DIAG_MESSAGE_MAX];
    char detail[KAIN_DIAG_DETAIL_MAX];
    char source_path[KAIN_DIAG_SOURCE_PATH_MAX];
    unsigned int runtime_abi_version;
} KainDiagnostic;

/*
 * Initialize Diagnostic Record
 *
 * Clears all fields and sets defaults.
 */
void kain_diagnostic_init(KainDiagnostic* diag);

/*
 * Create Diagnostic
 *
 * Convenience function to create a diagnostic with common fields.
 * Detail and source_path can be NULL if not applicable.
 */
void kain_diagnostic_create(
    KainDiagnostic* diag,
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail,
    const char* source_path
);

/*
 * Format Diagnostic to String
 *
 * Formats a diagnostic as a human-readable string.
 * Returns number of characters written (excluding null terminator).
 */
int kain_diagnostic_format(
    const KainDiagnostic* diag,
    char* out,
    size_t out_size
);

/*
 * Print Diagnostic
 *
 * Prints a diagnostic to stderr with appropriate formatting.
 */
void kain_diagnostic_print(const KainDiagnostic* diag);

/*
 * Get Subsystem Name
 *
 * Returns a string name for the given subsystem.
 */
const char* kain_diagnostic_subsystem_name(KainDiagSubsystem subsystem);

/*
 * Get Severity Name
 *
 * Returns a string name for the given severity level.
 */
const char* kain_diagnostic_severity_name(KainDiagSeverity severity);

/*
 * Diagnostic Collector
 *
 * A structure for collecting multiple diagnostics during startup and runtime
 * operations. Provides buffering and batch reporting capabilities.
 */
#define KAIN_DIAG_COLLECTOR_MAX_DIAGNOSTICS 32

typedef struct {
    KainDiagnostic diagnostics[KAIN_DIAG_COLLECTOR_MAX_DIAGNOSTICS];
    int count;
    int error_count;
    int warning_count;
    int fatal_count;
} KainDiagnosticCollector;

/*
 * Initialize Diagnostic Collector
 *
 * Clears the collector and prepares it for diagnostic collection.
 */
void kain_diagnostic_collector_init(KainDiagnosticCollector* collector);

/*
 * Add Diagnostic to Collector
 *
 * Adds a diagnostic to the collector. Returns 0 on success, -1 if the
 * collector is full. Updates severity counters automatically.
 */
int kain_diagnostic_collector_add(
    KainDiagnosticCollector* collector,
    const KainDiagnostic* diag
);

/*
 * Add Diagnostic to Collector (Create and Add)
 *
 * Convenience function that creates a diagnostic and adds it to the collector
 * in one call. Returns 0 on success, -1 if the collector is full.
 */
int kain_diagnostic_collector_add_new(
    KainDiagnosticCollector* collector,
    KainDiagSubsystem subsystem,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail,
    const char* source_path
);

/*
 * Check if Collector Has Errors
 *
 * Returns 1 if the collector contains any error or fatal diagnostics, 0 otherwise.
 */
int kain_diagnostic_collector_has_errors(const KainDiagnosticCollector* collector);

/*
 * Check if Collector Has Fatals
 *
 * Returns 1 if the collector contains any fatal diagnostics, 0 otherwise.
 */
int kain_diagnostic_collector_has_fatals(const KainDiagnosticCollector* collector);

/*
 * Get Diagnostic Count by Severity
 *
 * Returns the number of diagnostics with the given severity level.
 */
int kain_diagnostic_collector_count_by_severity(
    const KainDiagnosticCollector* collector,
    KainDiagSeverity severity
);

/*
 * Print All Diagnostics in Collector
 *
 * Prints all collected diagnostics to stderr/stdout based on severity.
 * Useful for batch reporting during startup or after operations.
 */
void kain_diagnostic_collector_print_all(const KainDiagnosticCollector* collector);

/*
 * Format Diagnostic Summary
 *
 * Formats a summary of collected diagnostics (counts by severity) into the
 * output buffer. Returns number of characters written (excluding null terminator).
 */
int kain_diagnostic_collector_format_summary(
    const KainDiagnosticCollector* collector,
    char* out,
    size_t out_size
);

/*
 * Clear Collector
 *
 * Clears all diagnostics from the collector and resets counters.
 */
void kain_diagnostic_collector_clear(KainDiagnosticCollector* collector);

/*
 * Startup Validation Result
 *
 * Aggregates diagnostics, version information, and validation status from
 * runtime startup. Used for comprehensive startup reporting.
 */
typedef struct {
    /* Version Information */
    unsigned int runtime_abi_version;
    unsigned int runtime_version;
    unsigned int bundle_abi_version;

    /* Validation Status */
    int validation_passed;
    int required_services_available;
    int optional_services_available;
    int optional_services_degraded;

    /* Diagnostics */
    KainDiagnosticCollector diagnostics;

    /* Summary Strings */
    char summary[256];
} KainStartupValidationResult;

/*
 * Initialize Startup Validation Result
 *
 * Clears the result structure and prepares it for validation reporting.
 */
void kain_startup_validation_result_init(KainStartupValidationResult* result);

/*
 * Format Startup Validation Report
 *
 * Formats a comprehensive startup validation report including version info,
 * service status, and diagnostics. Returns number of characters written.
 */
int kain_startup_validation_result_format(
    const KainStartupValidationResult* result,
    char* out,
    size_t out_size
);

/*
 * Print Startup Validation Report
 *
 * Prints a comprehensive startup validation report to stdout/stderr.
 */
void kain_startup_validation_result_print(const KainStartupValidationResult* result);

#endif /* DIAGNOSTICS_H */
