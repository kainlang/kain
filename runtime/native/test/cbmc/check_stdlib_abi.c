/*
 * check_stdlib_abi.c — CBMC verification harness for stdlib_abi module
 * ====================================================================
 *
 * Verifies the ABI bridge to Kain standard library constructs: Option/Result
 * tag correctness, tagged union access safety, future lifecycle, runtime
 * init/shutdown, and attrition checkpoint safety.
 *
 * Properties verified (~38 assertions):
 *   1.  Option None: tag == OPTION_NONE, is_none=1, is_some=0
 *   2.  Option Some: tag == OPTION_SOME, is_some=1, is_none=0
 *   3.  Option payload_copy: from Some copies correctly, from None returns -1
 *   4.  Option NULL safety: all option functions handle NULL value
 *   5.  Result Ok/Err: correct tags, is_ok/is_err, payload_copy
 *   6.  Result ok_option: Ok→Some(payload), Err→None
 *   7.  Tagged is_success: Ok/Some returns 1, Err/None returns 0
 *   8.  Tagged matches: exact tag comparison, NULL returns 0
 *   9.  Tagged payload_copy: NULL-safe, size-mismatch detection
 *  10.  Future ready_from_value: tag=FUTURE, inline ready, state=COMPLETED
 *  11.  Future state: NULL returns -1
 *  12.  Future await_payload_copy: NULL-safe, zero-payload safety
 *  13.  Runtime init/shutdown: return 0
 *  14.  Runtime heap_validate: returns 0 or 1 (safe)
 *  15.  Attrition checkpoint/note/result_set with disabled capture: safe
 *
 * Run:  cd runtime/native
 *       python test/scripts/run_pipeline.py cbmc --harness check_stdlib_abi
 * Or:   cbmc --unwind 5 --trace test/cbmc/check_stdlib_abi.c src/core/stdlib_abi.c
 *            -I include -I src/core
 */

#include "stdlib_abi.h"
#include "async.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ── Static backing buffers for pointer provenance ── */

static int64_t g_payload_out[16];          /* output buffer for payload_copy */
static unsigned char g_payload_data[64];    /* source payload bytes */
static const char* g_empty_label = "";


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1: Option — abi_option_none
 * ══════════════════════════════════════════════════════════════════════════ */

void check_option_none(void) {
    void* opt = abi_option_none();
    if (opt == NULL) return; /* OOM path */

    int64_t is_some = abi_option_is_some(opt);
    __CPROVER_assert(is_some == 0,
        "option_none: is_some returns 0");

    int64_t is_none = abi_option_is_none(opt);
    __CPROVER_assert(is_none == 1,
        "option_none: is_none returns 1");

    /* payload_copy on None should return -1 */
    int64_t pc = abi_option_payload_copy(opt, g_payload_out, 8);
    __CPROVER_assert(pc == -1,
        "option_none: payload_copy returns -1");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2: Option — abi_option_some
 * ══════════════════════════════════════════════════════════════════════════ */

void check_option_some(void) {
    __CPROVER_havoc_object(g_payload_data);
    g_payload_data[0] = 0xAB;
    g_payload_data[1] = 0xCD;
    g_payload_data[2] = 0xEF;

    void* opt = abi_option_some(g_payload_data, 3);
    if (opt == NULL) return;

    int64_t is_some = abi_option_is_some(opt);
    __CPROVER_assert(is_some == 1,
        "option_some: is_some returns 1");

    int64_t is_none = abi_option_is_none(opt);
    __CPROVER_assert(is_none == 0,
        "option_some: is_none returns 0");

    /* payload_copy copies the 3 payload bytes */
    memset(g_payload_out, 0, sizeof(g_payload_out));
    int64_t pc = abi_option_payload_copy(opt, g_payload_out, 8);
    __CPROVER_assert(pc == 3,
        "option_some: payload_copy returns 3 (payload_size)");
    __CPROVER_assert(g_payload_out[0] == (int64_t)(unsigned char)0xAB,
        "option_some: byte 0 matches");
}

void check_option_some_large(void) {
    /* Create Some with payload matching output buffer size */
    __CPROVER_havoc_object(g_payload_data);
    void* opt = abi_option_some(g_payload_data, 16);
    if (opt == NULL) return;

    __CPROVER_assert(abi_option_is_some(opt) == 1,
        "option_some(large): is_some returns 1");

    /* Payload copy with exactly matching size */
    memset(g_payload_out, 0, sizeof(g_payload_out));
    int64_t pc = abi_option_payload_copy(opt, g_payload_out, 16);
    __CPROVER_assert(pc == 16,
        "option_some(large): payload_copy returns 16");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3: Option — NULL safety
 * ══════════════════════════════════════════════════════════════════════════ */

void check_option_null_value(void) {
    int64_t is_some = abi_option_is_some(NULL);
    __CPROVER_assert(is_some == 0,
        "option_is_some(NULL): returns 0");

    int64_t is_none = abi_option_is_none(NULL);
    __CPROVER_assert(is_none == 1,
        "option_is_none(NULL): returns 1 (NULL treated as None)");

    int64_t pc = abi_option_payload_copy(NULL, g_payload_out, 8);
    __CPROVER_assert(pc == -1,
        "option_payload_copy(NULL, ...): returns -1");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4: Result — abi_result_ok / abi_result_err
 * ══════════════════════════════════════════════════════════════════════════ */

void check_result_ok(void) {
    int64_t ok_value = 42;
    void* res = abi_result_ok(&ok_value, sizeof(ok_value));
    if (res == NULL) return;

    __CPROVER_assert(abi_result_is_ok(res) == 1,
        "result_ok: is_ok returns 1");
    __CPROVER_assert(abi_result_is_err(res) == 0,
        "result_ok: is_err returns 0");
    __CPROVER_assert(abi_tagged_is_success(res) == 1,
        "result_ok: tagged_is_success returns 1");

    memset(g_payload_out, 0, sizeof(g_payload_out));
    int64_t pc = abi_result_payload_copy(res, g_payload_out, sizeof(g_payload_out));
    __CPROVER_assert(pc == (int64_t)sizeof(ok_value),
        "result_ok: payload_copy returns payload_size");
    __CPROVER_assert(g_payload_out[0] == 42,
        "result_ok: payload value 42");
}

void check_result_err(void) {
    int64_t err_code = -99;
    void* res = abi_result_err(&err_code, sizeof(err_code));
    if (res == NULL) return;

    __CPROVER_assert(abi_result_is_ok(res) == 0,
        "result_err: is_ok returns 0");
    __CPROVER_assert(abi_result_is_err(res) == 1,
        "result_err: is_err returns 1");
    __CPROVER_assert(abi_tagged_is_success(res) == 0,
        "result_err: tagged_is_success returns 0");

    memset(g_payload_out, 0, sizeof(g_payload_out));
    int64_t pc = abi_result_payload_copy(res, g_payload_out, sizeof(g_payload_out));
    __CPROVER_assert(pc == (int64_t)sizeof(err_code),
        "result_err: payload_copy still copies payload");
}

void check_result_ok_option(void) {
    int64_t val = 77;
    void* res = abi_result_ok(&val, sizeof(val));
    if (res == NULL) return;

    void* opt = abi_result_ok_option(res);
    if (opt == NULL) return;

    __CPROVER_assert(abi_option_is_some(opt) == 1,
        "result_ok_option: Ok → Some");

    void* err_res = abi_result_err(&val, sizeof(val));
    if (err_res == NULL) return;

    void* none_opt = abi_result_ok_option(err_res);
    if (none_opt == NULL) return;

    __CPROVER_assert(abi_option_is_none(none_opt) == 1,
        "result_ok_option: Err → None");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5: Result — NULL safety
 * ══════════════════════════════════════════════════════════════════════════ */

void check_result_null_value(void) {
    __CPROVER_assert(abi_result_is_ok(NULL) == 0,
        "result_is_ok(NULL): returns 0");
    __CPROVER_assert(abi_result_is_err(NULL) == 0,
        "result_is_err(NULL): returns 0");

    int64_t pc = abi_result_payload_copy(NULL, g_payload_out, 8);
    __CPROVER_assert(pc == -1,
        "result_payload_copy(NULL, ...): returns -1");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6: Tagged — generic access
 * ══════════════════════════════════════════════════════════════════════════ */

void check_tagged_matches(void) {
    int64_t val = 123;
    void* res = abi_result_ok(&val, sizeof(val));
    if (res == NULL) return;

    /* The ABI_TAG_RESULT_OK value is 2 (defined in stdlib_abi.c) */
    int64_t matches = abi_tagged_matches(res, 2);
    __CPROVER_assert(matches == 1,
        "tagged_matches: result_ok matches tag 2");

    matches = abi_tagged_matches(res, 99);
    __CPROVER_assert(matches == 0,
        "tagged_matches: wrong tag returns 0");

    matches = abi_tagged_matches(NULL, 0);
    __CPROVER_assert(matches == 0,
        "tagged_matches: NULL value returns 0");
}

void check_tagged_payload_copy_null(void) {
    /* NULL out_payload */
    void* opt = abi_option_some(g_payload_data, 4);
    if (opt == NULL) return;

    int64_t pc = abi_tagged_payload_copy(opt, NULL, 8);
    __CPROVER_assert(pc == -1,
        "tagged_payload_copy(..., NULL, ...): returns -1");

    pc = abi_tagged_payload_copy(NULL, g_payload_out, 8);
    __CPROVER_assert(pc == -1,
        "tagged_payload_copy(NULL, ...): returns -1");

    /* Negative out_payload_size */
    pc = abi_tagged_payload_copy(opt, g_payload_out, -1);
    __CPROVER_assert(pc == -1,
        "tagged_payload_copy(..., -1): returns -1");
}

void check_tagged_payload_copy_too_small(void) {
    int64_t big[4] = {1, 2, 3, 4};
    void* opt = abi_option_some(big, sizeof(big));
    if (opt == NULL) return;

    /* Output buffer too small */
    int64_t pc = abi_tagged_payload_copy(opt, g_payload_out, 1);
    __CPROVER_assert(pc == -2,
        "tagged_payload_copy: out too small returns -2");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 7: Future — abi_future_ready_from_value
 * ══════════════════════════════════════════════════════════════════════════ */

void check_future_ready_from_value(void) {
    int64_t val = 42;
    void* fut = abi_future_ready_from_value(&val, sizeof(val));
    if (fut == NULL) return;

    /* Ready future should be COMPLETED */
    int64_t state = abi_future_state(fut);
    __CPROVER_assert(state == (int64_t)KAIN_TASK_STATE_COMPLETED,
        "future_ready: state is COMPLETED (= 3)");

    /* Payload copy works */
    memset(g_payload_out, 0, sizeof(g_payload_out));
    int64_t pc = abi_future_await_payload_copy(fut, g_payload_out, sizeof(g_payload_out));
    __CPROVER_assert(pc == (int64_t)sizeof(val),
        "future_ready: await_payload_copy returns 8");
    __CPROVER_assert(g_payload_out[0] == 42,
        "future_ready: payload value 42");
}

void check_future_ready_zero_payload(void) {
    void* fut = abi_future_ready_from_value(NULL, 0);
    if (fut == NULL) return;

    int64_t state = abi_future_state(fut);
    __CPROVER_assert(state == (int64_t)KAIN_TASK_STATE_COMPLETED,
        "future_ready(zero): state is COMPLETED");
}

void check_future_state_null(void) {
    int64_t state = abi_future_state(NULL);
    __CPROVER_assert(state == -1,
        "future_state(NULL): returns -1");
}

void check_future_await_null(void) {
    /* NULL future_value */
    int64_t pc = abi_future_await_payload_copy(NULL, g_payload_out, 8);
    __CPROVER_assert(pc == -1,
        "future_await_payload_copy(NULL, ...): returns -1");

    /* NULL out_payload */
    int64_t val = 42;
    void* fut = abi_future_ready_from_value(&val, sizeof(val));
    if (fut == NULL) return;

    pc = abi_future_await_payload_copy(fut, NULL, 8);
    __CPROVER_assert(pc == -1,
        "future_await_payload_copy(fut, NULL, ...): returns -1");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 8: Runtime init / shutdown / heap validate
 * ══════════════════════════════════════════════════════════════════════════ */

void check_runtime_init(void) {
    int64_t rc = abi_runtime_init();
    __CPROVER_assert(rc == 0,
        "runtime_init: returns 0");
}

void check_runtime_heap_validate(void) {
    int64_t rc = abi_runtime_heap_validate();
    /* Returns 0 or 1 depending on platform heap state */
    __CPROVER_assert(rc == 0 || rc == 1,
        "runtime_heap_validate: returns 0 or 1 (safe)");
}

void check_runtime_shutdown(void) {
    int64_t rc = abi_runtime_shutdown();
    __CPROVER_assert(rc == 0,
        "runtime_shutdown: returns 0");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 9: Attrition (disabled capture)
 * ══════════════════════════════════════════════════════════════════════════ */

void check_attrition_checkpoint(void) {
    /* With no environment setup, capture is disabled and should be safe */
    int64_t rc = abi_attrition_checkpoint("test-checkpoint", 0);
    __CPROVER_assert(rc == 0,
        "attrition_checkpoint: disabled capture returns 0");
}

void check_attrition_note_progress(void) {
    int64_t rc = abi_attrition_note_progress(1, 0xDEAD);
    __CPROVER_assert(rc == 0,
        "attrition_note_progress: disabled capture returns 0");
}

void check_attrition_result_set(void) {
    int64_t rc = abi_attrition_result_set(0xCAFE, 42, "test failure");
    __CPROVER_assert(rc == 0,
        "attrition_result_set: disabled capture returns 0");
}


/* ══════════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Option */
    check_option_none();
    check_option_some();
    check_option_some_large();
    check_option_null_value();

    /* Result */
    check_result_ok();
    check_result_err();
    check_result_ok_option();
    check_result_null_value();

    /* Tagged */
    check_tagged_matches();
    check_tagged_payload_copy_null();
    check_tagged_payload_copy_too_small();

    /* Future */
    check_future_ready_from_value();
    check_future_ready_zero_payload();
    check_future_state_null();
    check_future_await_null();

    /* Runtime */
    check_runtime_init();
    check_runtime_heap_validate();
    check_runtime_shutdown();

    /* Attrition */
    check_attrition_checkpoint();
    check_attrition_note_progress();
    check_attrition_result_set();

    return 0;
}
