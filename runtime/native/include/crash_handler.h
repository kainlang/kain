// Crash forensics handler — self-diagnosing crash reports for Kain programs.
//
// Called automatically from the Kain-generated main() when compiled with -g.
// No external tools needed — the compiler embeds a crash table, and this
// handler binary-searches it on SIGSEGV / SIGILL / SIGFPE.
//
// Architecture:
//   1. __kain_crash_handler_init() registers OS signal handlers.
//   2. On crash, the handler gets the faulting instruction pointer.
//   3. Binary-searches @__kain_crash_table (emitted by the LLVM generator).
//   4. Walks the frame pointer chain for a human-readable callstack.
//   5. Renders the crash report to stderr, then _Exit(1).

#ifndef KAIN_CRASH_HANDLER_H
#define KAIN_CRASH_HANDLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Matches %KainCrashEntry emitted by the LLVM codegen.
// { fn_ptr_as_i64, line, col, fn_name_str, file_str }
typedef struct {
    uint64_t fn_ptr;
    uint32_t line;
    uint32_t col;
    const char *fn_name;
    const char *file;
} KainCrashEntry;

// One-time init — register signal handlers / VEH.
// Idempotent; safe to call more than once.
void __kain_crash_handler_init(void);

// Look up a faulting instruction pointer in the crash table.
// Returns NULL when the address isn't covered by any known function.
const KainCrashEntry *__kain_crash_lookup(const void *ip);

#ifdef __cplusplus
}
#endif

#endif // KAIN_CRASH_HANDLER_H
