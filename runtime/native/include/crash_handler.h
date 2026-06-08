/*
 * KAIN Crash Forensics — Public API
 *
 * Called from the compiler-emitted main() preamble when -g is on.
 * Platform-specific signal/VEH registration is internal (platform layer).
 */

#ifndef KAIN_CRASH_HANDLER_H
#define KAIN_CRASH_HANDLER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matches %KainCrashEntry emitted by the LLVM codegen.
 * { fn_ptr_as_i64, line, col, fn_name_str, file_str } */
typedef struct {
    uint64_t    fn_ptr;
    uint32_t    line;
    uint32_t    col;
    const char *fn_name;
    const char *file;
} KainCrashEntry;

/* One-time init — registers OS signal handlers.  Idempotent. */
void __kain_crash_handler_init(void);

/* Look up a faulting instruction pointer in the crash table.
 * Returns NULL when the address isn't covered by any known function. */
const KainCrashEntry *__kain_crash_lookup(const void *ip);

/* Render a formatted crash report to stderr.
 * Called by platform signal handlers with the fault context. */
void __kain_crash_render_report(
    const char              *signal_name,
    const KainCrashEntry    *entry,
    const void              *fault_ip,
    void                   **callstack,
    int                      callstack_depth);

/* ── Platform internal hook ────────────────────────────────────────── */

/* Each platform file implements this to register OS-specific handlers.
 * Called once from __kain_crash_handler_init(). */
void __kain_crash_platform_register_handlers(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_CRASH_HANDLER_H */
