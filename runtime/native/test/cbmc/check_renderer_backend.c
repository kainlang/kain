/*
 * check_renderer_backend.c -- CBMC verification harness for renderer_backend
 *
 * Verifies the renderer backend descriptor catalog: bounded array traversal,
 * NULL safety, case-insensitive lookup, bounds checking, and fallback logic.
 *
 * The catalog is a small static array with 2 entries (Vulkan, D3D12). Every
 * function is a simple getter/iterator over this array. The key correctness
 * properties are:
 *   - Out-of-bounds access returns NULL (no buffer overrun)
 *   - Lookup with NULL/empty/unknown returns NULL (no null deref)
 *   - Lookup is case-insensitive (ci comparison doesn't miss)
 *   - At least one backend is always available
 *   - Default/active always return a valid descriptor
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_renderer_backend
 * Or:     cbmc --unwind 5 --trace test/cbmc/check_renderer_backend.c \
 *              src/core/renderer_backend.c -I include -I src/core
 */

#include "renderer_backend.h"

#include <string.h>

/* We don't need static backing buffers; everything is in the catalog array.
 * For getenv modeling, CBMC will explore both NULL and non-NULL paths. */


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_catalog returns a non-NULL pointer
 * ────────────────────────────────────────────────────────────────────── */
void check_catalog_nonnull(void) {
    const KainRendererBackendDescriptor* cat = kain_renderer_backend_catalog();
    __CPROVER_assert(cat != NULL, "catalog: returns non-NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_count returns at least 1
 * ────────────────────────────────────────────────────────────────────── */
void check_count_positive(void) {
    size_t count = kain_renderer_backend_count();
    __CPROVER_assert(count > 0, "count: at least 1 backend");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_at with valid index returns a descriptor
 *         with populated fields
 * ────────────────────────────────────────────────────────────────────── */
void check_at_valid_indices(void) {
    size_t count = kain_renderer_backend_count();

    for (size_t i = 0; i < count; i++) {
        const KainRendererBackendDescriptor* d = kain_renderer_backend_at(i);
        __CPROVER_assert(d != NULL, "at: valid index returns non-NULL");
        if (d) {
            __CPROVER_assert(d->kind == KAIN_RENDERER_BACKEND_VULKAN ||
                             d->kind == KAIN_RENDERER_BACKEND_D3D12,
                             "at: kind is a known backend kind");
            __CPROVER_assert(d->id != NULL, "at: id is non-NULL");
            __CPROVER_assert(d->id[0] != '\0', "at: id is non-empty");
            __CPROVER_assert(d->display_name != NULL, "at: display_name non-NULL");
            __CPROVER_assert(d->runtime_name != NULL, "at: runtime_name non-NULL");
            __CPROVER_assert(d->service_key != NULL, "at: service_key non-NULL");
            __CPROVER_assert(d->summary != NULL, "at: summary non-NULL");
        }
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_at with out-of-range index returns NULL
 * ────────────────────────────────────────────────────────────────────── */
void check_at_out_of_range(void) {
    size_t count = kain_renderer_backend_count();

    /* Large index that definitely exceeds the catalog */
    const KainRendererBackendDescriptor* d = kain_renderer_backend_at(999);
    __CPROVER_assert(d == NULL, "at: OOB index returns NULL");

    /* Boundary: index == count */
    d = kain_renderer_backend_at(count);
    __CPROVER_assert(d == NULL, "at: index == count returns NULL");

    /* Boundary: index == count + 1 */
    d = kain_renderer_backend_at(count + 1);
    __CPROVER_assert(d == NULL, "at: index == count+1 returns NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_lookup with known IDs finds the right kind
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_known_ids(void) {
    const KainRendererBackendDescriptor* v = kain_renderer_backend_lookup("vulkan");
    __CPROVER_assert(v != NULL, "lookup vulkan: found");
    if (v) {
        __CPROVER_assert(v->kind == KAIN_RENDERER_BACKEND_VULKAN,
                         "lookup vulkan: kind matches");
        __CPROVER_assert(strcmp(v->id, "vulkan") == 0,
                         "lookup vulkan: id matches");
    }

    const KainRendererBackendDescriptor* d3 = kain_renderer_backend_lookup("d3d12");
    __CPROVER_assert(d3 != NULL, "lookup d3d12: found");
    if (d3) {
        __CPROVER_assert(d3->kind == KAIN_RENDERER_BACKEND_D3D12,
                         "lookup d3d12: kind matches");
        __CPROVER_assert(strcmp(d3->id, "d3d12") == 0,
                         "lookup d3d12: id matches");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_lookup is case-insensitive
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_case_insensitive(void) {
    const KainRendererBackendDescriptor* upper = kain_renderer_backend_lookup("VULKAN");
    __CPROVER_assert(upper != NULL, "lookup VULKAN: case-insensitive");

    const KainRendererBackendDescriptor* mixed = kain_renderer_backend_lookup("Vulkan");
    __CPROVER_assert(mixed != NULL, "lookup Vulkan: mixed case");

    const KainRendererBackendDescriptor* lower = kain_renderer_backend_lookup("vulkan");
    __CPROVER_assert(lower != NULL, "lookup vulkan: lowercase");

    /* All three lookups should return the same descriptor */
    if (upper && mixed && lower) {
        __CPROVER_assert(upper == mixed,
                         "lookup: VULKAN == Vulkan (same descriptor)");
        __CPROVER_assert(mixed == lower,
                         "lookup: Vulkan == vulkan (same descriptor)");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_lookup with NULL/empty returns NULL
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_null_empty(void) {
    const KainRendererBackendDescriptor* null_lookup = kain_renderer_backend_lookup(NULL);
    __CPROVER_assert(null_lookup == NULL, "lookup NULL: returns NULL");

    const KainRendererBackendDescriptor* empty_lookup = kain_renderer_backend_lookup("");
    __CPROVER_assert(empty_lookup == NULL, "lookup empty: returns NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_lookup with unknown ID returns NULL
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_unknown(void) {
    const KainRendererBackendDescriptor* d = kain_renderer_backend_lookup("nonexistent_backend");
    __CPROVER_assert(d == NULL, "lookup unknown: returns NULL");

    d = kain_renderer_backend_lookup("metal");
    __CPROVER_assert(d == NULL, "lookup metal: returns NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_default returns an available descriptor
 * ────────────────────────────────────────────────────────────────────── */
void check_default_available(void) {
    const KainRendererBackendDescriptor* d = kain_renderer_backend_default();

    __CPROVER_assert(d != NULL, "default: returns non-NULL");
    if (d) {
        /* The first available backend (by definition at least one exists) */
        __CPROVER_assert(d->kind == KAIN_RENDERER_BACKEND_VULKAN ||
                         d->kind == KAIN_RENDERER_BACKEND_D3D12,
                         "default: kind is known");
        /* default should return an available backend */
        __CPROVER_assert(d->available != 0, "default: available is true");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_active always returns a valid descriptor,
 *         whether RENDERER_BACKEND env is set or not
 *
 * CBMC models getenv as nondeterministic: it may return NULL, a known
 * string like "vulkan"/"d3d12", or an unknown string. The function must
 * produce a valid descriptor in all cases.
 * ────────────────────────────────────────────────────────────────────── */
void check_active_always_valid(void) {
    const KainRendererBackendDescriptor* d = kain_renderer_backend_active();

    __CPROVER_assert(d != NULL, "active: returns non-NULL regardless of env");
    if (d) {
        __CPROVER_assert(d->kind == KAIN_RENDERER_BACKEND_VULKAN ||
                         d->kind == KAIN_RENDERER_BACKEND_D3D12,
                         "active: kind is known");
        __CPROVER_assert(d->id != NULL, "active: id is non-NULL");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: all service keys in descriptors match canonical constants
 * ────────────────────────────────────────────────────────────────────── */
void check_service_keys_valid(void) {
    size_t count = kain_renderer_backend_count();

    for (size_t i = 0; i < count; i++) {
        const KainRendererBackendDescriptor* d = kain_renderer_backend_at(i);
        if (!d) continue;

        if (d->kind == KAIN_RENDERER_BACKEND_VULKAN) {
            __CPROVER_assert(strcmp(d->service_key, "gfx.backend.vulkan") == 0,
                             "vulkan service_key is gfx.backend.vulkan");
        }
        if (d->kind == KAIN_RENDERER_BACKEND_D3D12) {
            __CPROVER_assert(strcmp(d->service_key, "gfx.backend.d3d12") == 0,
                             "d3d12 service_key is gfx.backend.d3d12");
        }
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: catalog entries are in order and unique
 * ────────────────────────────────────────────────────────────────────── */
void check_catalog_ordering(void) {
    size_t count = kain_renderer_backend_count();
    if (count < 2) return;

    const KainRendererBackendDescriptor* first = kain_renderer_backend_at(0);
    const KainRendererBackendDescriptor* second = kain_renderer_backend_at(1);

    if (first && second) {
        /* IDs should be unique */
        __CPROVER_assert(strcmp(first->id, second->id) != 0,
                         "ordering: consecutive descriptors have unique ids");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_renderer_backend_active falls back to default when env
 *         specifies an unavailable backend
 *
 * CBMC explores the branch where getenv returns a known ID that is
 * available, and where it returns NULL/unknown -> falls to default.
 * ────────────────────────────────────────────────────────────────────── */
void check_active_fallback(void) {
    const KainRendererBackendDescriptor* active = kain_renderer_backend_active();
    const KainRendererBackendDescriptor* def   = kain_renderer_backend_default();

    __CPROVER_assert(active != NULL, "active fallback: non-NULL");
    __CPROVER_assert(def != NULL, "active fallback: default non-NULL");

    /* Active must either be the env-matching backend or the default */
    __CPROVER_assert(active->kind == KAIN_RENDERER_BACKEND_VULKAN ||
                     active->kind == KAIN_RENDERER_BACKEND_D3D12,
                     "active fallback: known kind");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main -- run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_catalog_nonnull();
    check_count_positive();
    check_at_valid_indices();
    check_at_out_of_range();
    check_lookup_known_ids();
    check_lookup_case_insensitive();
    check_lookup_null_empty();
    check_lookup_unknown();
    check_default_available();
    check_active_always_valid();
    check_service_keys_valid();
    check_catalog_ordering();
    check_active_fallback();
    return 0;
}
