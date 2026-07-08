// ============================================================================
//  kaintana_runtime_stubs.c — Minimal stubs for native runtime symbols
//  needed by the Kaintana C substrate.
//
//  These provide just enough implementation for the static-linking case
//  where the native Kain runtime is not separately compiled/linked.
//  For production use, link against the full runtime library instead.
// ============================================================================

#include "kaintana.h"
#include <stdlib.h>
#include <string.h>

// ── string_new: Allocate and copy a string ───────────────────────────
// Used by input_system.c to internalize string constants.
// In the full runtime this uses kain_alloc_rc (ref-counted arena alloc).
// Here we use plain malloc.
char* string_new(char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, src, len + 1);
    return buf;
}
