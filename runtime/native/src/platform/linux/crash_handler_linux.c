/*
 * KAIN Crash Forensics — Linux signal handler (sigaction)
 *
 * Registered by __kain_crash_platform_register_handlers().
 * Included via linux_sources in the runtime manifest.
 */

#include "../../../include/crash_handler.h"
#include "../../../include/base.h"

#if defined(__linux__)

#include <signal.h>
#include <string.h>
#include <unistd.h>

/* ── Frame pointer stack walk (x64) ───────────────────────────────── */

static int walk_callstack_x64(void *rbp_val, void *stack_bottom,
                              void **frames, int max_frames) {
    int depth = 0;
#if defined(__x86_64__)
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

/* ── Signal handler with siginfo ────────────────────────────────────── */

static void crash_signal_handler(int sig, siginfo_t *info, void *ucontext) {
    const char *sig_name = "SIGNAL";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGILL:  sig_name = "SIGILL";  break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        default: break;
    }

    const void *fault_ip = NULL;
    void *rbp_val   = NULL;

#if defined(__x86_64__)
    {
        ucontext_t *uc = (ucontext_t *)ucontext;
        /* glibc / musl: gregs[REG_RIP] and gregs[REG_RBP].
         * REG_RIP = 16, REG_RBP = 6 in the standard x86-64 mcontext layout. */
#if defined(__GLIBC__) || !defined(__GLIBC__)
        #ifdef REG_RIP
        fault_ip = (const void *)uc->uc_mcontext.gregs[REG_RIP];
        rbp_val  = (void *)uc->uc_mcontext.gregs[REG_RBP];
        #else
        /* Fallback: indices are standard across x86-64 Linux ABIs */
        fault_ip = (const void *)uc->uc_mcontext.gregs[16]; /* REG_RIP */
        rbp_val  = (void *)uc->uc_mcontext.gregs[6];        /* REG_RBP */
        #endif
#endif
    }
#endif

    void *callstack[32];
    int depth = 0;
#if defined(__x86_64__)
    if (rbp_val) {
        int dummy;
        void *stack_bottom = (void *)(((uintptr_t)&dummy + 4095) & ~4095);
        depth = walk_callstack_x64(rbp_val, stack_bottom,
                                   callstack, 32);
    }
#endif

    const KainCrashEntry *entry = __kain_crash_lookup(fault_ip);
    __kain_crash_render_report(sig_name, entry, fault_ip,
                               callstack, depth);

    _Exit(1);
}

/* ── Registration ─────────────────────────────────────────────────── */

void __kain_crash_platform_register_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
}

#else
void __kain_crash_platform_register_handlers(void) { /* no-op */ }
#endif
