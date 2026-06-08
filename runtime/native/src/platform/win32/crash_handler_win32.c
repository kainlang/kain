/*
 * KAIN Crash Forensics — Windows Vectored Exception Handler
 *
 * Registered by __kain_crash_platform_register_handlers().
 * Included via windows_sources in the runtime manifest.
 */

#include "../../../include/crash_handler.h"
#include "../../../include/base.h"

#ifdef _WIN32

/* ── Frame pointer stack walk (x64) ───────────────────────────────── */

static int walk_callstack_x64(void *rbp_val, void *stack_bottom,
                              void **frames, int max_frames) {
    int depth = 0;
#if defined(_M_X64) || defined(__x86_64__)
    uintptr_t *rbp = (uintptr_t *)rbp_val;
    if (!rbp || !stack_bottom) return 0;

    while (rbp && depth < max_frames) {
        uintptr_t next_rbp = rbp[0];
        uintptr_t ret_addr = rbp[1];
        if (ret_addr == 0 || next_rbp == 0) break;
        if (next_rbp <= (uintptr_t)rbp) break;
        if ((uintptr_t)rbp > (uintptr_t)stack_bottom) break;
        frames[depth++] = (void *)ret_addr;
        rbp = (uintptr_t *)next_rbp;
    }
#endif
    return depth;
}

/* ── Vectored Exception Handler ───────────────────────────────────── */

static LONG WINAPI crash_vectored_handler(EXCEPTION_POINTERS *info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;
    const char *sig_name = "EXCEPTION";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            sig_name = "ACCESS_VIOLATION"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            sig_name = "ILLEGAL_INSTRUCTION"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            sig_name = "DIVIDE_BY_ZERO"; break;
        case EXCEPTION_STACK_OVERFLOW:
            sig_name = "STACK_OVERFLOW"; break;
        default: break;
    }

    const void *fault_ip = info->ExceptionRecord->ExceptionAddress;

    void *callstack[32];
    int depth = 0;
    CONTEXT *ctx = info->ContextRecord;
    if (ctx) {
        int dummy;
        void *stack_bottom = (void *)(((uintptr_t)&dummy + 4095) & ~4095);
        depth = walk_callstack_x64((void *)ctx->Rbp, stack_bottom,
                                   callstack, 32);
    }

    const KainCrashEntry *entry = __kain_crash_lookup(fault_ip);
    __kain_crash_render_report(sig_name, entry, fault_ip,
                               callstack, depth);

    return EXCEPTION_CONTINUE_SEARCH;
}

/* ── Registration ─────────────────────────────────────────────────── */

void __kain_crash_platform_register_handlers(void) {
    AddVectoredExceptionHandler(1, crash_vectored_handler);
}

#else
void __kain_crash_platform_register_handlers(void) { /* no-op */ }
#endif
