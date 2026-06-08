/*
 * check_crash_handler_linux.c — CBMC verification of the Linux crash handler
 *
 * Self-contained: #includes the core and Linux platform sources so that
 * all static functions (lookup_crash_entry, walk_callstack_x64, etc.)
 * are in the same translation unit.
 *
 * Run:
 *   cbmc --bounds-check --pointer-check --trace \
 *     check_crash_handler_linux.c -I ../../include -I .. \
 *     --no-unwinding-assertions --unwind 8
 */

/* ── Include the sources directly, not as separate TUs ────────────── */
#include "../../src/core/crash_handler.c"
#include "../../src/platform/linux/crash_handler_linux.c"

#include "check_crash_handler.c"
