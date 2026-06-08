/*
 * check_cpu.c — CBMC verification harness for cpu.c
 *
 * cpu.c is heavily platform-dependent (cpuid, Windows API, pthread, inline asm).
 * CBMC cannot model cpuid instructions, Windows VirtualProtect, or inline asm,
 * so this harness covers only the pure utility functions that are OS-independent.
 *
 * Testable functions:
 *   - abi_text_equals       (pure string comparison wrapper)
 *   - abi_prefetch_locality (pure integer clamp)
 *   - abi_vm_byte_count_fits_platform (pure integer bounds check)
 *   - abi_cpu_pause         (inline asm — CBMC treats as no-op)
 *
 * Untestable with CBMC (platform deps / inline asm / CPUID):
 *   - abi_cpuid, abi_xgetbv0, abi_cpu_detect_feature_mask
 *   - abi_cpu_feature_mask, abi_cpu_feature_fingerprint
 *   - abi_cpu_capability_mask_for_key, abi_cpu_has_capability
 *   - abi_cpu_rdtsc, abi_cpu_cpuid_lane
 *   - abi_cpu_prefetch_read, abi_cpu_prefetch_write
 *   - abi_windows_count_topology_relation
 *   - abi_vm_default_huge_page_bytes, abi_vm_protection_mode, abi_vm_protect
 */

/* cpu.c includes <pthread.h> and <sched.h> which need _GNU_SOURCE
 * on Linux/WSL to expose CPU_ZERO, CPU_SETSIZE, etc. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpu.h"
#include <stdint.h>
#include <string.h>

/* ── Forward declarations of static utility functions from cpu.c ────── */
static int abi_text_equals(const char* left, const char* right);
static int abi_prefetch_locality(int64_t locality);
static int abi_vm_byte_count_fits_platform(int64_t byte_count);


/* ══════════════════════════════════════════════════════════════════════
 * Check: abi_text_equals — string comparison wrapper
 * ══════════════════════════════════════════════════════════════════════ */
void check_text_equals(void) {
    /* 1a: Both NULL → 0 (not equal) */
    {
        int r = abi_text_equals(NULL, NULL);
        __CPROVER_assert(r == 0, "text_equals(NULL, NULL) -> 0");
    }

    /* 1b: One NULL → 0 */
    {
        static char buf[16];
        int r = abi_text_equals(NULL, buf);
        __CPROVER_assert(r == 0, "text_equals(NULL, valid) -> 0");
    }
    {
        static char buf[16];
        int r = abi_text_equals(buf, NULL);
        __CPROVER_assert(r == 0, "text_equals(valid, NULL) -> 0");
    }

    /* 1c: Equal strings → 1 */
    {
        static char a[] = "hello";
        int r = abi_text_equals(a, a);
        __CPROVER_assert(r == 1, "text_equals(same ptr) -> 1");
    }
    {
        static char a[] = "hello";
        static char b[] = "hello";
        int r = abi_text_equals(a, b);
        __CPROVER_assert(r == 1, "text_equals(equal strings) -> 1");
    }

    /* 1d: Different strings → 0 */
    {
        static char a[] = "hello";
        static char b[] = "world";
        int r = abi_text_equals(a, b);
        __CPROVER_assert(r == 0, "text_equals(different) -> 0");
    }

    /* 1e: Nondet strings — only possible results are 0 or 1 */
    {
        static char a[16];
        static char b[16];
        __CPROVER_havoc_object(a);
        __CPROVER_havoc_object(b);

        /* Ensure null-terminated (CBMC's nondet bytes may not include \0) */
        a[sizeof(a) - 1] = '\0';
        b[sizeof(b) - 1] = '\0';

        int r = abi_text_equals(a, b);
        __CPROVER_assert(r == 0 || r == 1,
                         "text_equals(nondet): result is boolean");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * Check: abi_prefetch_locality — locality clamp
 * ══════════════════════════════════════════════════════════════════════ */
void check_prefetch_locality(void) {
    /* 2a: Valid localities return themselves */
    for (int64_t loc = 0; loc <= 3; loc++) {
        int r = abi_prefetch_locality(loc);
        __CPROVER_assert(r == (int)loc,
                         "prefetch_locality(0..3): preserves value");
    }

    /* 2b: Negative clamps to 0 */
    {
        int64_t neg;
        __CPROVER_havoc_object(&neg);
        __CPROVER_assume(neg < 0);
        int r = abi_prefetch_locality(neg);
        __CPROVER_assert(r == 0,
                         "prefetch_locality(neg): clamps to 0");
    }

    /* 2c: >3 clamps to 3 */
    {
        int64_t high;
        __CPROVER_havoc_object(&high);
        __CPROVER_assume(high > 3);
        int r = abi_prefetch_locality(high);
        __CPROVER_assert(r == 3,
                         "prefetch_locality(>3): clamps to 3");
    }

    /* 2d: Nondet locality — result always in [0,3] */
    {
        int64_t loc;
        __CPROVER_havoc_object(&loc);
        int r = abi_prefetch_locality(loc);
        __CPROVER_assert(r >= 0 && r <= 3,
                         "prefetch_locality(nondet): result in [0,3]");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * Check: abi_vm_byte_count_fits_platform — size bounds check
 * ══════════════════════════════════════════════════════════════════════ */
void check_vm_byte_count(void) {
    /* 3a: Zero is valid (query) */
    {
        int r = abi_vm_byte_count_fits_platform(0);
        __CPROVER_assert(r == 1,
                         "vm_byte_count(0): fits (query path)");
    }

    /* 3b: Negative doesn't fit */
    {
        int64_t neg;
        __CPROVER_havoc_object(&neg);
        __CPROVER_assume(neg < 0);
        int r = abi_vm_byte_count_fits_platform(neg);
        __CPROVER_assert(r == 0,
                         "vm_byte_count(neg): doesn't fit");
    }

    /* 3c: Nondet — result is truly nondet due to CBMC's SIZE_MAX model.
       Test safety instead: if we constrain to known ranges, it must return 1. */
    {
        int64_t sz;
        __CPROVER_havoc_object(&sz);
        __CPROVER_assume(sz > 0 && sz <= 0x100000);  /* small positive always fits */
        int r = abi_vm_byte_count_fits_platform(sz);
        /* On any real platform with size_t >= 32 bits, this fits */
        __CPROVER_assert(r != 0, "vm_byte_count(small positive): fits");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════ */
int main(void) {
    check_text_equals();
    check_prefetch_locality();
    check_vm_byte_count();
    return 0;
}
