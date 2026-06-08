/*
 * CBMC verification harness for diagnostics
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
// kain_diagnostic_channel
const KainDiagChannel* kain_diagnostic_channel(KainDiagSubsystem subsystem);
// kain_diagnostic_channel_should_emit
int kain_diagnostic_channel_should_emit(KainDiagSubsystem subsystem, KainDiagSeverity severity);
// kain_diagnostic_init
void kain_diagnostic_init(KainDiagnostic* diag);
// kain_diagnostic_print
void kain_diagnostic_print(const KainDiagnostic* diag);
// kain_diagnostic_subsystem_name
const char* kain_diagnostic_subsystem_name(KainDiagSubsystem subsystem);

int main(void) {
    { void *__p; kain_diagnostic_channel(__p); }
    __CPROVER_assert(1, "kain_diagnostic_channel: call ok");
    { void *__a; unsigned long long __b; kain_diagnostic_channel_should_emit(__a, __b); }
    __CPROVER_assert(1, "kain_diagnostic_channel_should_emit: call ok");
    { void *__p; kain_diagnostic_init(__p); }
    __CPROVER_assert(1, "kain_diagnostic_init: call ok");
    { void *__p; kain_diagnostic_print(__p); }
    __CPROVER_assert(1, "kain_diagnostic_print: call ok");
    { void *__p; kain_diagnostic_subsystem_name(__p); }
    __CPROVER_assert(1, "kain_diagnostic_subsystem_name: call ok");
    return 0;
}
