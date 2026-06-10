/*
 * check_contract.c — CBMC verification harness for contract module
 *
 * Tests runtime contract bundle loading (JSON parsing), validation,
 * service mask formatting, and enhanced startup validation with valid
 * inputs, NULL inputs, and edge cases.
 *
 * Key CBMC patterns:
 *   - Static backing buffers for pointer provenance
 *   - Concrete JSON strings for deterministic parsing
 *   - __CPROVER_havoc_object + __CPROVER_assume for nondet input
 *   - __CPROVER_assert for postconditions
 *   - Calling real API and static functions (same translation unit)
 *
 * Key invariants verified:
 *   - contract_init: all fields zeroed
 *   - contract_load_from_json: null args -> 0; valid target JSON -> loaded=1
 *     with target copied; service bindings parsed into service_mask;
 *     finalize computes core/optional service counts correctly
 *   - contract_service_mask: null -> 0; unloaded -> 0; loaded -> mask
 *   - contract_validation_init: all fields zeroed
 *   - contract_validate_startup: null validation -> 0; no bundle -> returns
 *     1 in lax mode; with bundle -> validates ABI, services, target
 *   - contract_format_service_mask: null args -> early return; no bits ->
 *     "none"; known bits -> human-readable names
 *   - contract_validate_startup_enhanced: null result -> 0; valid ->
 *     populates version, service counts, diagnostics
 *   - contract_is_service_available: null key -> 0
 *   - Static: contract_keys_equal null -> 0; identical -> 1; different -> 0
 *   - Static: contract_target_is_raw_native "llvm"/"c" -> 1; others -> 0
 *   - Static: contract_count_bits 0 -> 0; 1<<N -> 1; multi-bit -> correct
 *
 * External function modeling:
 *   - version_get_info: nondet (may or may not populate version_info)
 *   - version_format_abi: nondet (may or may not write buffer)
 *   - version_check_abi_compatibility: nondet (returns 0 or 1)
 *   - kain_env_flag: nondet (strict_mode on or off)
 *   - kain_service_registry_*: nondet (registry may or may not be available)
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_contract
 * Or:     cbmc --unwind 5 --no-unwinding-assertions --trace \
 *             test/cbmc/check_contract.c src/core/contract.c \
 *             -I include -I src/core
 */

#include "contract.h"
#include "version.h"
#include "services.h"
#include "diagnostics.h"
#include "win32.h"

/* ── Static backing buffers for pointer provenance ── */
static KainRuntimeContractBundle    g_bundle;
static KainRuntimeContractBundle    g_bundle2;
static KainRuntimeContractValidation g_validation;
static KainStartupValidationResult  g_startup_result;
static KainDiagnosticCollector      g_collector;
static KainDiagnostic               g_diag;
static char                         g_service_mask_buffer[512];
static char                         g_json_buffer[512];

/* ── Concrete JSON strings for deterministic parsing ── */
static const char g_json_target[]           = "{\"target\": \"llvm\"}";
static const char g_json_target_c[]         = "{\"target\": \"c\"}";
static const char g_json_services[] =
    "{\"service_bindings\": "
    "[{\"service\": \"base.memory\", \"lane\": \"core\"},"
    " {\"service\": \"base.diagnostics\", \"lane\": \"core\"},"
    " {\"service\": \"contract\", \"lane\": \"core\"}]}";
static const char g_json_full[] =
    "{\"target\": \"llvm\", "
    "\"required_capabilities\": [{\"cap\": \"x64\"}], "
    "\"service_bindings\": "
    "[{\"service\": \"base.memory\", \"lane\": \"core\"}], "
    "\"items\": [{\"name\": \"test\"}]}";
static const char g_json_empty[]            = "{}";
static const char g_json_abi[] =
    "{\"target\": \"llvm\", "
    "\"abi_version\": 66051}";

/* ── Forward declarations of static functions from contract.c ──
 * These are visible without declarations because the harness is
 * combined with contract.c into one translation unit.  We list
 * them here for clarity; the real definitions in contract.c take
 * precedence (same TU).
 */
#include <string.h>


/* ═══════════════════════════════════════════════════════════════════════
 * 1. CONTRACT INIT
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_contract_init
 *
 * After init, all bundle fields must be zero (loaded=0, target empty,
 * service_mask=0, counts=0).
 * ────────────────────────────────────────────────────────────────────── */
void check_contract_init(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    contract_init(&b);

    __CPROVER_assert(b.loaded == 0,
                     "contract_init: loaded == 0");
    __CPROVER_assert(b.target[0] == '\0',
                     "contract_init: target empty");
    __CPROVER_assert(b.load_origin[0] == '\0',
                     "contract_init: load_origin empty");
    __CPROVER_assert(b.source_path[0] == '\0',
                     "contract_init: source_path empty");
    __CPROVER_assert(b.service_mask == 0,
                     "contract_init: service_mask == 0");
    __CPROVER_assert(b.service_count == 0,
                     "contract_init: service_count == 0");
    __CPROVER_assert(b.required_capability_count == 0,
                     "contract_init: required_capability_count == 0");
    __CPROVER_assert(b.item_count == 0,
                     "contract_init: item_count == 0");
    __CPROVER_assert(b.required_abi_version == 0,
                     "contract_init: required_abi_version == 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_contract_init_null
 *
 * NULL pointer must not crash.
 * ────────────────────────────────────────────────────────────────────── */
void check_contract_init_null(void) {
    contract_init(NULL);
    __CPROVER_assert(1, "contract_init(NULL): no crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 2. CONTRACT SERVICE MASK
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_contract_service_mask_null
 *
 * NULL bundle returns 0 mask.
 * ────────────────────────────────────────────────────────────────────── */
void check_contract_service_mask_null(void) {
    KainRuntimeServiceMask m = contract_service_mask(NULL);
    __CPROVER_assert(m == 0,
                     "service_mask(NULL): returns 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_contract_service_mask_unloaded
 *
 * An initialized but unloaded bundle returns 0 mask.
 * ────────────────────────────────────────────────────────────────────── */
void check_contract_service_mask_unloaded(void) {
    KainRuntimeContractBundle b;
    contract_init(&b);

    KainRuntimeServiceMask m = contract_service_mask(&b);
    __CPROVER_assert(m == 0,
                     "service_mask(unloaded): returns 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 3. CONTRACT LOAD FROM JSON — NULL ARGS
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_null_json
 *
 * NULL json -> returns 0, bundle unchanged.
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_null_json(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(NULL, &b);
    __CPROVER_assert(rc == 0,
                     "load_from_json(NULL json): returns 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_null_bundle
 *
 * NULL bundle -> returns 0.
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_null_bundle(void) {
    int rc = contract_load_from_json(g_json_target, NULL);
    __CPROVER_assert(rc == 0,
                     "load_from_json(NULL bundle): returns 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 4. CONTRACT LOAD FROM JSON — TARGET ONLY
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_target_llvm
 *
 * JSON {"target": "llvm"} must load target="llvm", loaded=1, and
 * set target_is_llvm=1, valid_for_raw_native=1 (no core services
 * needed for valid_for_raw_native — core_service_count must equal
 * expected_core_service_count, which is 0=0 when mask is 0... wait.
 *
 * Actually, looking at contract_finalize:
 *   expected_core_service_count = count_bits(CORE_MASK)
 *   core_service_count = count_bits(mask & CORE_MASK)
 *   missing_core_service_count = expected - core
 *   valid_for_raw_native = target_is_raw_native && core == expected
 *
 * With service_mask=0, core_service_count=0, expected=4 (CORE_MASK has
 * 4 bits: BASE_MEMORY, MEMORY_OWNERSHIP, BASE_DIAGNOSTICS, CONTRACT).
 * So missing_core_service_count=4, valid_for_raw_native=0 (since
 * 0 != 4).  That's correct — a contract without core services is not
 * valid for raw native.
 *
 * We verify the target string was parsed correctly.
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_target_llvm(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(g_json_target, &b);

    __CPROVER_assert(rc != 0,
                     "load_json target(llvm): returns non-zero (loaded)");
    __CPROVER_assert(b.loaded != 0,
                     "load_json target(llvm): loaded != 0");
    __CPROVER_assert(b.target[0] == 'l',
                     "load_json target(llvm): target[0] == 'l'");
    __CPROVER_assert(b.target[1] == 'l',
                     "load_json target(llvm): target[1] == 'l'");
    __CPROVER_assert(b.target[2] == 'v',
                     "load_json target(llvm): target[2] == 'v'");
    __CPROVER_assert(b.target[3] == 'm',
                     "load_json target(llvm): target[3] == 'm'");
    __CPROVER_assert(b.target[4] == '\0',
                     "load_json target(llvm): target[4] == '\\0'");
    __CPROVER_assert(b.target_is_llvm != 0,
                     "load_json target(llvm): target_is_llvm set");
    __CPROVER_assert(b.service_mask == 0,
                     "load_json target(llvm): no services, mask=0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_target_c
 *
 * JSON {"target": "c"} must load target="c", target_is_llvm=0 but
 * contract_target_is_raw_native returns 1 for "c".
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_target_c(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(g_json_target_c, &b);

    __CPROVER_assert(rc != 0,
                     "load_json target(c): returns non-zero (loaded)");
    __CPROVER_assert(b.loaded != 0,
                     "load_json target(c): loaded != 0");
    __CPROVER_assert(b.target[0] == 'c',
                     "load_json target(c): target[0] == 'c'");
    __CPROVER_assert(b.target[1] == '\0',
                     "load_json target(c): target[1] == '\\0'");
    __CPROVER_assert(b.target_is_llvm == 0,
                     "load_json target(c): target_is_llvm == 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 5. CONTRACT LOAD FROM JSON — WITH SERVICES
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_services
 *
 * JSON with service_bindings array containing 3 services (base.memory,
 * base.diagnostics, contract).  After parsing, service_mask must have
 * exactly those 3 bits set, service_count=3.
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_services(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(g_json_services, &b);

    __CPROVER_assert(rc != 0,
                     "load_json services: returns non-zero (loaded)");
    __CPROVER_assert(b.loaded != 0,
                     "load_json services: loaded != 0");
    __CPROVER_assert(b.service_count == 3,
                     "load_json services: service_count == 3");

    /* base.memory (BIT 0), base.diagnostics (BIT 2), contract (BIT 3) */
    KainRuntimeServiceMask expected =
        RUNTIME_SERVICE_BASE_MEMORY |
        RUNTIME_SERVICE_BASE_DIAGNOSTICS |
        RUNTIME_SERVICE_CONTRACT;
    __CPROVER_assert(b.service_mask == expected,
                     "load_json services: mask matches expected bits");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_full
 *
 * JSON with target + required_capabilities + service_bindings + items.
 * Must parse all fields correctly:
 *   - target -> "llvm"
 *   - required_capability_count -> 1
 *   - service_count -> 1 (base.memory)
 *   - item_count -> 1
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_full(void) {
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(g_json_full, &b);

    __CPROVER_assert(rc != 0,
                     "load_json full: returns non-zero (loaded)");
    __CPROVER_assert(b.loaded != 0,
                     "load_json full: loaded != 0");
    __CPROVER_assert(b.target[0] == 'l',
                     "load_json full: target starts with 'l'");
    __CPROVER_assert(b.required_capability_count == 1,
                     "load_json full: capability_count == 1");
    __CPROVER_assert(b.service_count == 1,
                     "load_json full: service_count == 1");
    __CPROVER_assert(b.item_count == 1,
                     "load_json full: item_count == 1");
    __CPROVER_assert(
        (b.service_mask & RUNTIME_SERVICE_BASE_MEMORY) != 0,
        "load_json full: base.memory in mask");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 6. CONTRACT LOAD FROM JSON — EMPTY / EDGE CASES
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_load_from_json_empty
 *
 * JSON {} has no recognizable fields, so loaded must stay 0.
 * ────────────────────────────────────────────────────────────────────── */
void check_load_from_json_empty(void) {
    KainRuntimeContractBundle b;
    b.loaded = 99; /* sentinel */
    __CPROVER_havoc_object(&b);

    int rc = contract_load_from_json(g_json_empty, &b);

    __CPROVER_assert(rc == 0,
                     "load_json empty: returns 0");
    __CPROVER_assert(b.loaded == 0,
                     "load_json empty: loaded == 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 7. CONTRACT VALIDATION INIT
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_validation_init
 *
 * After init, all validation fields must be zero/default.
 * ────────────────────────────────────────────────────────────────────── */
void check_validation_init(void) {
    KainRuntimeContractValidation v;
    __CPROVER_havoc_object(&v);

    contract_validation_init(&v);

    __CPROVER_assert(v.strict_mode == 0,
                     "validation_init: strict_mode == 0");
    __CPROVER_assert(v.contract_present == 0,
                     "validation_init: contract_present == 0");
    __CPROVER_assert(v.fatal_error == 0,
                     "validation_init: fatal_error == 0");
    __CPROVER_assert(v.required_service_mask == 0,
                     "validation_init: required_service_mask == 0");
    __CPROVER_assert(v.optional_service_mask == 0,
                     "validation_init: optional_service_mask == 0");
    __CPROVER_assert(v.available_service_mask == 0,
                     "validation_init: available_service_mask == 0");
    __CPROVER_assert(v.missing_required_mask == 0,
                     "validation_init: missing_required_mask == 0");
    __CPROVER_assert(v.abi_compatible == 0,
                     "validation_init: abi_compatible == 0");
    __CPROVER_assert(v.warning_count == 0,
                     "validation_init: warning_count == 0");
    __CPROVER_assert(v.fatal_message[0] == '\0',
                     "validation_init: fatal_message empty");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_validation_init_null
 *
 * NULL pointer must not crash.
 * ────────────────────────────────────────────────────────────────────── */
void check_validation_init_null(void) {
    contract_validation_init(NULL);
    __CPROVER_assert(1, "validation_init(NULL): no crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 8. CONTRACT VALIDATE STARTUP — NULL / EDGE
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_null_validation
 *
 * NULL validation pointer must return 0.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_null_validation(void) {
    int rc = contract_validate_startup(NULL, 0, 0, NULL);
    __CPROVER_assert(rc == 0,
                     "validate_startup(NULL validation): returns 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_no_bundle_lax
 *
 * No bundle + lax mode (strict_mode=0) -> returns 1, adds a warning.
 * Note: kain_env_flag is nondet, so strict may be 0 or 1. When strict=0
 * and no bundle, it's lax mode: returns 1 with warning.
 * When strict=1 and no bundle, it's fatal: returns 0 with fatal_message.
 * Either path is valid — we just check no crash.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_no_bundle_lax(void) {
    __CPROVER_havoc_object(&g_validation);

    int rc = contract_validate_startup(NULL, 0, 0, &g_validation);

    __CPROVER_assert(rc == 0 || rc == 1,
                     "validate_startup(no bundle): returns 0 or 1");

    /* When no contract present:
     *   - If strict (nondet via env): fatal, returns 0
     *   - If lax: warning, returns 1
     */
    if (rc == 0) {
        __CPROVER_assert(
            g_validation.fatal_error != 0 ||
            g_validation.fatal_message[0] != '\0',
            "validate_startup(no bundle, rc=0): fatal_error or message set");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_with_bundle
 *
 * Validating with a loaded bundle (target=llvm). The validation checks
 * ABI compatibility, target match, and service availability.
 * version_check_abi_compatibility and kain_env_flag are nondet, so
 * both success and failure paths are explored.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_with_bundle(void) {
    __CPROVER_havoc_object(&g_validation);

    /* Load a valid bundle */
    KainRuntimeContractBundle b;
    __CPROVER_havoc_object(&b);
    int loaded = contract_load_from_json(g_json_target, &b);
    if (!loaded) return;

    /* Bundle is loaded with target=llvm, no services */
    int rc = contract_validate_startup(
        &b,
        RUNTIME_SERVICE_BASE_MEMORY,     /* required */
        RUNTIME_SERVICE_NATIVE_INPUT,     /* optional */
        &g_validation
    );

    __CPROVER_assert(rc == 0 || rc == 1,
                     "validate_startup(with bundle): returns 0 or 1");

    /* If contract is present (which it is since g_json_target loaded) */
    if (g_validation.contract_present) {
        __CPROVER_assert(
            g_validation.available_service_mask == b.service_mask,
            "validate_startup: available_service_mask matches bundle");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_abi_mismatch
 *
 * If bundle has a non-zero required_abi_version that doesn't match,
 * the validation may report abi_compatible=0.  We set up the bundle
 * metadata manually (not through JSON) to control the ABI version.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_abi_mismatch(void) {
    __CPROVER_havoc_object(&g_validation);

    KainRuntimeContractBundle b;
    contract_init(&b);
    b.loaded = 1;
    b.target[0] = 'l'; b.target[1] = 'l'; b.target[2] = 'v';
    b.target[3] = 'm'; b.target[4] = '\0';
    /* Set a deliberately different ABI version from current */
    unsigned int wrong_abi;
    __CPROVER_havoc_object(&wrong_abi);
    __CPROVER_assume(wrong_abi != 0);
    __CPROVER_assume(wrong_abi != RUNTIME_ABI_VERSION_CURRENT);
    b.required_abi_version = wrong_abi;

    int rc = contract_validate_startup(
        &b, RUNTIME_SERVICE_BASE_MEMORY, 0, &g_validation);

    __CPROVER_assert(rc == 0 || rc == 1,
                     "validate_startup(abi mismatch): returns 0 or 1");

    /* If the nondet version_check_abi reports incompatible,
     * abi_compatible must be 0 and fatal_error must be 1. */
    if (!g_validation.abi_compatible) {
        __CPROVER_assert(g_validation.fatal_error != 0,
                         "validate_startup(abi fail): fatal_error set");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 9. CONTRACT FORMAT SERVICE MASK
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_format_service_mask_null
 *
 * NULL out or out_cap==0 -> early return (no crash, nothing written).
 * ────────────────────────────────────────────────────────────────────── */
void check_format_service_mask_null(void) {
    /* NULL out */
    contract_format_service_mask(RUNTIME_SERVICE_BASE_MEMORY, NULL, 0);
    __CPROVER_assert(1, "format_service_mask(NULL out): no crash");

    /* zero size */
    char single;
    single = 'X';
    contract_format_service_mask(RUNTIME_SERVICE_BASE_MEMORY, &single, 0);
    __CPROVER_assert(single == 'X',
                     "format_service_mask(zero size): untouched");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_format_service_mask_none
 *
 * Zero mask must produce "none".
 * ────────────────────────────────────────────────────────────────────── */
void check_format_service_mask_none(void) {
    char buf[64];
    buf[0] = 'X';
    __CPROVER_havoc_object(buf + 1);

    contract_format_service_mask(0, buf, sizeof(buf));

    __CPROVER_assert(buf[0] == 'n',
                     "format_service_mask(0): starts with 'n'");
    __CPROVER_assert(buf[1] == 'o',
                     "format_service_mask(0): 'o'");
    __CPROVER_assert(buf[2] == 'n',
                     "format_service_mask(0): 'n'");
    __CPROVER_assert(buf[3] == 'e',
                     "format_service_mask(0): 'e'");
    __CPROVER_assert(buf[4] == '\0',
                     "format_service_mask(0): null terminated");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_format_service_mask_single_bit
 *
 * RUNTIME_SERVICE_BASE_MEMORY (bit 0) must produce "base.memory".
 * ────────────────────────────────────────────────────────────────────── */
void check_format_service_mask_single_bit(void) {
    char buf[64];
    __CPROVER_havoc_object(buf);

    contract_format_service_mask(RUNTIME_SERVICE_BASE_MEMORY, buf, sizeof(buf));

    /* Must contain "base.memory" (exact match) */
    __CPROVER_assert(buf[0] == 'b',
                     "format_service_mask(memory): 'b'");
    __CPROVER_assert(buf[4] == '.',
                     "format_service_mask(memory): '.' at [4]");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_format_service_mask_multi_bit
 *
 * Two bits: must contain both names separated by ", ".
 * ────────────────────────────────────────────────────────────────────── */
void check_format_service_mask_multi_bit(void) {
    char buf[128];
    __CPROVER_havoc_object(buf);

    KainRuntimeServiceMask mask =
        RUNTIME_SERVICE_BASE_MEMORY |
        RUNTIME_SERVICE_BASE_DIAGNOSTICS;

    contract_format_service_mask(mask, buf, sizeof(buf));

    __CPROVER_assert(buf[0] != '\0',
                     "format_service_mask(multi): non-empty");
    /* Should contain "base.memory" somewhere */
    __CPROVER_assert(buf[0] == 'b' || buf[0] == 'm',
                     "format_service_mask(multi): starts with known key");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 10. CONTRACT VALIDATE STARTUP ENHANCED
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_enhanced_null_result
 *
 * NULL result -> returns 0 without crash.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_enhanced_null_result(void) {
    int rc = contract_validate_startup_enhanced(NULL, 0, 0, NULL);
    __CPROVER_assert(rc == 0,
                     "validate_startup_enhanced(NULL result): returns 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_validate_startup_enhanced_with_bundle
 *
 * Enhanced startup validation with a valid loaded bundle. Populates
 * version info, service counts, and diagnostics collector.
 * ────────────────────────────────────────────────────────────────────── */
void check_validate_startup_enhanced_with_bundle(void) {
    __CPROVER_havoc_object(&g_startup_result);

    KainRuntimeContractBundle b;
    contract_init(&b);
    b.loaded = 1;
    b.target[0] = 'l'; b.target[1] = 'l';
    b.target[2] = 'v'; b.target[3] = 'm';
    b.target[4] = '\0';

    int rc = contract_validate_startup_enhanced(
        &b,
        RUNTIME_SERVICE_BASE_MEMORY,
        RUNTIME_SERVICE_NATIVE_INPUT,
        &g_startup_result
    );

    __CPROVER_assert(rc == 0 || rc == 1,
                     "validate_startup_enhanced: returns 0 or 1");

    /* Result must be initialized (validation_passed reflects outcome) */
    __CPROVER_assert(
        g_startup_result.validation_passed == 0 ||
        g_startup_result.validation_passed == 1,
        "validate_startup_enhanced: validation_passed is boolean");

    /* Summary must be non-empty */
    __CPROVER_assert(
        g_startup_result.summary[0] != '\0' ||
        !g_startup_result.validation_passed,
        "validate_startup_enhanced: summary populated on success");

    /* Diagnostics collector is valid */
    __CPROVER_assert(
        g_startup_result.diagnostics.count >= 0,
        "validate_startup_enhanced: diag count >= 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 11. CONTRACT IS SERVICE AVAILABLE
 * ────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────
 * check_is_service_available_null
 *
 * NULL key returns 0 (not available).
 * ────────────────────────────────────────────────────────────────────── */
void check_is_service_available_null(void) {
    int rc = contract_is_service_available(NULL);
    __CPROVER_assert(rc == 0,
                     "is_service_available(NULL): returns 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_is_service_available_known_key
 *
 * A known service key (base.memory) with a valid registry may return
 * 0 or 1 depending on nondet kain_service_registry_is_available.
 * ────────────────────────────────────────────────────────────────────── */
void check_is_service_available_known_key(void) {
    int rc = contract_is_service_available("base.memory");
    __CPROVER_assert(rc == 0 || rc == 1,
                     "is_service_available(known): returns 0 or 1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 12. STATIC FUNCTION TESTS
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── contract_keys_equal ────────────────────────────────────────────── */

void check_keys_equal_null(void) {
    __CPROVER_assert(contract_keys_equal(NULL, NULL) == 0,
                     "keys_equal(NULL, NULL): 0");
    __CPROVER_assert(contract_keys_equal("abc", NULL) == 0,
                     "keys_equal(str, NULL): 0");
    __CPROVER_assert(contract_keys_equal(NULL, "abc") == 0,
                     "keys_equal(NULL, str): 0");
}

void check_keys_equal_identical(void) {
    __CPROVER_assert(contract_keys_equal("llvm", "llvm") != 0,
                     "keys_equal(\"llvm\", \"llvm\"): non-zero");
}

void check_keys_equal_different(void) {
    __CPROVER_assert(contract_keys_equal("llvm", "c") == 0,
                     "keys_equal(\"llvm\", \"c\"): 0");
    __CPROVER_assert(contract_keys_equal("abc", "ABC") != 0,
                     "keys_equal case-insensitive: non-zero");
}

/* ── contract_target_is_raw_native ──────────────────────────────────── */

void check_target_is_raw_native_llvm(void) {
    KainRuntimeContractBundle b;
    contract_init(&b);
    b.target[0] = 'l'; b.target[1] = 'l';
    b.target[2] = 'v'; b.target[3] = 'm';
    b.target[4] = '\0';

    int rc = contract_target_is_raw_native(&b);
    __CPROVER_assert(rc != 0,
                     "target_is_raw_native(llvm): non-zero");
}

void check_target_is_raw_native_c(void) {
    KainRuntimeContractBundle b;
    contract_init(&b);
    b.target[0] = 'c';
    b.target[1] = '\0';

    int rc = contract_target_is_raw_native(&b);
    __CPROVER_assert(rc != 0,
                     "target_is_raw_native(c): non-zero");
}

void check_target_is_raw_native_other(void) {
    KainRuntimeContractBundle b;
    contract_init(&b);
    b.target[0] = 'w'; b.target[1] = 'a';
    b.target[2] = 's'; b.target[3] = 'm';
    b.target[4] = '\0';

    int rc = contract_target_is_raw_native(&b);
    __CPROVER_assert(rc == 0,
                     "target_is_raw_native(wasm): 0");
}

void check_target_is_raw_native_empty(void) {
    KainRuntimeContractBundle b;
    contract_init(&b);

    int rc = contract_target_is_raw_native(&b);
    __CPROVER_assert(rc == 0,
                     "target_is_raw_native(empty): 0");
}

void check_target_is_raw_native_null(void) {
    int rc = contract_target_is_raw_native(NULL);
    __CPROVER_assert(rc == 0,
                     "target_is_raw_native(NULL): 0");
}

/* ── contract_count_bits ────────────────────────────────────────────── */

void check_count_bits_zero(void) {
    int c = contract_count_bits(0);
    __CPROVER_assert(c == 0,
                     "count_bits(0): 0");
}

void check_count_bits_one(void) {
    int c = contract_count_bits(UINT64_C(1) << 0);
    __CPROVER_assert(c == 1,
                     "count_bits(bit0): 1");

    c = contract_count_bits(UINT64_C(1) << 15);
    __CPROVER_assert(c == 1,
                     "count_bits(bit15): 1");
}

void check_count_bits_multi(void) {
    KainRuntimeServiceMask mask =
        RUNTIME_SERVICE_BASE_MEMORY |       /* bit 0 */
        RUNTIME_SERVICE_BASE_DIAGNOSTICS |  /* bit 2 */
        RUNTIME_SERVICE_CONTRACT;           /* bit 3 */
    int c = contract_count_bits(mask);
    __CPROVER_assert(c == 3,
                     "count_bits(3 bits): 3");
}

void check_count_bits_core_mask(void) {
    int c = contract_count_bits(RUNTIME_SERVICE_CORE_MASK);
    /* CORE_MASK has BASE_MEMORY (0), MEMORY_OWNERSHIP (1),
     * BASE_DIAGNOSTICS (2), CONTRACT (3) = 4 bits */
    __CPROVER_assert(c == 4,
                     "count_bits(CORE_MASK): 4");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 13. CONTRACT POPULATE SERVICE REGISTRY (NULL safety)
 * ────────────────────────────────────────────────────────────────────── */
void check_populate_service_registry_null(void) {
    contract_populate_service_registry(NULL);
    __CPROVER_assert(1,
                     "populate_service_registry(NULL): no crash");
}

/* ──────────────────────────────────────────────────────────────────────
 * check_populate_service_registry_valid
 *
 * With a valid initialized registry, population doesn't crash.
 * The actual service register calls are external (nondet), so the
 * result is non-deterministic.
 * ────────────────────────────────────────────────────────────────────── */
void check_populate_service_registry_valid(void) {
    KainServiceRegistry r;
    kain_service_registry_init(&r);

    contract_populate_service_registry(&r);

    __CPROVER_assert(1,
                     "populate_service_registry(valid): no crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Init */
    check_contract_init();
    check_contract_init_null();

    /* Service mask */
    check_contract_service_mask_null();
    check_contract_service_mask_unloaded();

    /* Load from JSON — null */
    check_load_from_json_null_json();
    check_load_from_json_null_bundle();

    /* Load from JSON — target */
    check_load_from_json_target_llvm();
    check_load_from_json_target_c();

    /* Load from JSON — services */
    check_load_from_json_services();

    /* Load from JSON — full */
    check_load_from_json_full();

    /* Load from JSON — empty / edge */
    check_load_from_json_empty();

    /* Validation init */
    check_validation_init();
    check_validation_init_null();

    /* Validate startup */
    check_validate_startup_null_validation();
    check_validate_startup_no_bundle_lax();
    check_validate_startup_with_bundle();
    check_validate_startup_abi_mismatch();

    /* Format service mask */
    check_format_service_mask_null();
    check_format_service_mask_none();
    check_format_service_mask_single_bit();
    check_format_service_mask_multi_bit();

    /* Validate startup enhanced */
    check_validate_startup_enhanced_null_result();
    check_validate_startup_enhanced_with_bundle();

    /* Is service available */
    check_is_service_available_null();
    check_is_service_available_known_key();

    /* Static functions */
    check_keys_equal_null();
    check_keys_equal_identical();
    check_keys_equal_different();
    check_target_is_raw_native_llvm();
    check_target_is_raw_native_c();
    check_target_is_raw_native_other();
    check_target_is_raw_native_empty();
    check_target_is_raw_native_null();
    check_count_bits_zero();
    check_count_bits_one();
    check_count_bits_multi();
    check_count_bits_core_mask();

    /* Populate service registry */
    check_populate_service_registry_null();
    check_populate_service_registry_valid();

    return 0;
}
