/*
 * KAIN Native Runtime Crash Forensics
 *
 * Cross-platform crash handler core: table lookup and report rendering.
 * Platform-specific signal/VEH registration lives in the platform layer:
 *   - platform/win32/crash_handler_win32.c
 *   - platform/linux/crash_handler_linux.c
 *   - platform/macos/crash_handler_macos.c
 *
 * The compiler emits @__kain_crash_table when -g is passed.  This handler
 * binary-searches it on SIGSEGV / SIGILL / SIGFPE and renders a human-readable
 * crash report to stderr before _Exit(1).
 *
 * No external dependencies — no libunwind, no libdwarf, no addr2line.
 */

#include "../../include/crash_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Compiler-embedded crash table (weak, defined only when -g is on) ─── */

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) const KainCrashEntry __kain_crash_table[1];
#else
const KainCrashEntry __kain_crash_table[1];
#endif

static int   crash_handler_initialized = 0;
static size_t crash_table_count        = 0;

/* ── Table lookup (binary search by instruction pointer) ──────────────── */

static const KainCrashEntry *lookup_crash_entry(const void *ip) {
    if (crash_table_count == 0) return NULL;

    uint64_t ip_val = (uint64_t)(uintptr_t)ip;
    size_t lo = 0;
    size_t hi = crash_table_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (__kain_crash_table[mid].fn_ptr <= ip_val) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) return NULL;
    return &__kain_crash_table[lo - 1];
}

const KainCrashEntry *__kain_crash_lookup(const void *ip) {
    return lookup_crash_entry(ip);
}

/* ── Crash report rendering ───────────────────────────────────────────── */

void __kain_crash_render_report(
    const char              *signal_name,
    const KainCrashEntry    *entry,
    const void              *fault_ip,
    void                   **callstack,
    int                      callstack_depth)
{
    FILE *out = stderr;

    fprintf(out, "\n");
    fprintf(out, "fatal runtime error: %s", signal_name);
    if (entry) {
        fprintf(out, " in `%s` at %s:%u:%u\n",
                entry->fn_name, entry->file,
                entry->line, entry->col);
    } else {
        fprintf(out, " at %p\n", fault_ip);
    }

    if (callstack_depth > 0) {
        fprintf(out, "  callstack:\n");
        for (int i = 0; i < callstack_depth; i++) {
            const KainCrashEntry *frame = lookup_crash_entry(callstack[i]);
            if (frame) {
                fprintf(out, "    #%-2d  %-24s %s:%u\n",
                        i, frame->fn_name, frame->file, frame->line);
            } else {
                fprintf(out, "    #%-2d  %p\n", i, callstack[i]);
            }
        }
    }

    fprintf(out, "\n");
    fflush(out);
}

/* ── One-time init ────────────────────────────────────────────────────── */

void __kain_crash_handler_init(void) {
    if (crash_handler_initialized) return;
    crash_handler_initialized = 1;

    /* Count table entries once (sentinel has fn_ptr == 0). */
    if (__kain_crash_table) {
        while (crash_table_count < 4096 && __kain_crash_table[crash_table_count].fn_ptr != 0) {
            crash_table_count++;
        }
    }
    if (crash_table_count == 0) return;

    __kain_crash_platform_register_handlers();
}
