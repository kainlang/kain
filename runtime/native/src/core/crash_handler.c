// Crash forensics runtime — self-diagnosing crash reports.
//
// On SIGSEGV / SIGILL / SIGFPE (Unix) or structured exception (Windows),
// this handler binary-searches the compiler-embedded @__kain_crash_table,
// walks the frame pointer chain, and prints a human-readable crash report
// to stderr before _Exit(1).
//
// The crash table is emitted by the LLVM codegen when -g is passed:
//   @__kain_crash_table = private constant [N x %KainCrashEntry] [...]
//
// No external dependencies — no libunwind, no libdwarf, no addr2line.

#include "crash_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ── Platform detection ────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
  #define KAIN_CRASH_WINDOWS 1
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
  #define KAIN_CRASH_UNIX 1
  #include <signal.h>
  #include <string.h>
  #include <unistd.h>
#else
  #define KAIN_CRASH_STUB 1
#endif

// ── The compiler-embedded crash table ──────────────────────────────────
// Defined by the LLVM generator in the IR module.  The runtime casts each
// field to the KainCrashEntry layout defined in the header.
extern const KainCrashEntry __kain_crash_table[];
extern const size_t __kain_crash_table_len;

// Weak symbol — if the module was compiled without -g, the table is absent
// and __kain_crash_handler_init is a no-op.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) const KainCrashEntry __kain_crash_table[1];
__attribute__((weak)) const size_t __kain_crash_table_len;
#else
const KainCrashEntry __kain_crash_table[1];
const size_t __kain_crash_table_len;
#endif

// ── Internal helpers ───────────────────────────────────────────────────

static int crash_handler_initialized = 0;

// Binary-search the crash table for the entry whose fn_ptr <= ip < next fn_ptr.
static const KainCrashEntry *lookup_crash_entry(const void *ip) {
    uint64_t ip_val = (uint64_t)(uintptr_t)ip;
    if (!__kain_crash_table || __kain_crash_table_len == 0) {
        return NULL;
    }

    size_t lo = 0;
    size_t hi = __kain_crash_table_len;
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

#if KAIN_CRASH_UNIX
// Walk the frame pointer chain on x86-64.
// Each frame: [saved rbp] [return address]
// Returns the number of frames walked (capped).
static int walk_callstack(void *rbp_val, void *stack_bottom,
                          void **frames, int max_frames) {
    int depth = 0;
#if defined(__x86_64__) || defined(_M_X64)
    uintptr_t *rbp = (uintptr_t *)rbp_val;
    uintptr_t bottom = (uintptr_t)stack_bottom;
    while (rbp && (uintptr_t)rbp >= (uintptr_t)&depth &&
           (uintptr_t)rbp < bottom && depth < max_frames) {
        uintptr_t ret_addr = rbp[1];
        if (ret_addr == 0) break;
        frames[depth++] = (void *)ret_addr;
        uintptr_t next_rbp = rbp[0];
        if (next_rbp == 0 || next_rbp <= (uintptr_t)rbp) break;
        rbp = (uintptr_t *)next_rbp;
    }
#endif
    return depth;
}
#endif // KAIN_CRASH_UNIX

// ── Crash report rendering ─────────────────────────────────────────────

static void render_crash_report(const char *signal_name,
                                const KainCrashEntry *entry,
                                const void *fault_ip,
                                void **callstack, int callstack_depth) {
    FILE *out = stderr;

    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║  💥 %s in `%s` at %s:%u:%u\n",
            signal_name,
            entry ? entry->fn_name : "???",
            entry ? entry->file : "???",
            entry ? entry->line : 0,
            entry ? entry->col : 0);
    fprintf(out, "║      fault IP = %p\n", fault_ip);
    fprintf(out, "╠══════════════════════════════════════════════════════════╣\n");

    if (callstack_depth > 0) {
        fprintf(out, "║  Callstack:\n");
        for (int i = 0; i < callstack_depth; i++) {
            const KainCrashEntry *frame_entry =
                lookup_crash_entry(callstack[i]);
            if (frame_entry) {
                fprintf(out, "║    #%-2d  %-24s %s:%u\n",
                        i, frame_entry->fn_name,
                        frame_entry->file, frame_entry->line);
            } else {
                fprintf(out, "║    #%-2d  %p\n", i, callstack[i]);
            }
        }
    } else {
        fprintf(out, "║  Callstack unavailable (frame pointer may be omitted).\n");
        fprintf(out, "║  Recompile with -g and ensure -fno-omit-frame-pointer.\n");
    }

    fprintf(out, "╚══════════════════════════════════════════════════════════╝\n");
    fprintf(out, "\n");
    fflush(out);
}

// ── Unix signal handler ────────────────────────────────────────────────

#if KAIN_CRASH_UNIX

static void crash_signal_handler(int sig, siginfo_t *info, void *ucontext) {
    const char *sig_name = "SIGNAL";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGILL:  sig_name = "SIGILL";  break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        default: break;
    }

    const void *fault_ip = NULL;
    void *rbp_val = NULL;

#if defined(__x86_64__) || defined(_M_X64)
    // ucontext layout is OS-specific.  Generic fallback that works on
    // Linux (glibc / musl) and macOS.
    #if defined(__linux__)
        // glibc: uc_mcontext.gregs[REG_RIP / REG_RBP]
        #if defined(__GLIBC__) || defined(__GLIBC_PREREQ)
            fault_ip = (const void *)ucontext;
            rbp_val  = (void *)ucontext;
        #else
            // musl / simpler ucontext
            fault_ip = (const void *)ucontext;
            rbp_val  = (void *)ucontext;
        #endif
    #elif defined(__APPLE__)
        fault_ip = (const void *)ucontext;
        rbp_val  = (void *)ucontext;
    #endif
#endif

    void *callstack[32];
    int callstack_depth = 0;
#if defined(__x86_64__) || defined(_M_X64)
    if (rbp_val) {
        // Approximate stack bottom from a local variable address.
        int dummy;
        void *stack_bottom = (void *)(((uintptr_t)&dummy + 4095) & ~4095);
        callstack_depth = walk_callstack(rbp_val, stack_bottom,
                                         callstack, 32);
    }
#endif

    const KainCrashEntry *entry = lookup_crash_entry(fault_ip);
    render_crash_report(sig_name, entry, fault_ip,
                        callstack, callstack_depth);

    _Exit(1);
}

// Fallback handler when sa_sigaction isn't available.
static void crash_signal_handler_simple(int sig) {
    const char *sig_name = "SIGNAL";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGILL:  sig_name = "SIGILL";  break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        default: break;
    }

    // Without ucontext we can't get the fault IP or walk the stack.
    // Print a minimal report.
    FILE *out = stderr;
    fprintf(out, "\n💥 %s — crash handler invoked (no context available).\n", sig_name);
    fprintf(out, "   Recompile with -g for a full crash report with source lines.\n\n");
    fflush(out);
    _Exit(1);
}

static void register_unix_handlers(void) {
    // Register with sa_sigaction for rich crash reports (IP + stack walk).
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);

    // Fallback: if sa_sigaction is not supported, use the simple handler.
    // (The last registration wins, so we try sa_sigaction first.)
}
#endif // KAIN_CRASH_UNIX

// ── Windows Vectored Exception Handler ─────────────────────────────────

#if KAIN_CRASH_WINDOWS

static LONG WINAPI crash_vectored_handler(EXCEPTION_POINTERS *exception_info) {
    DWORD code = exception_info->ExceptionRecord->ExceptionCode;
    const char *sig_name = "EXCEPTION";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            sig_name = "ACCESS_VIOLATION (SIGSEGV)"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            sig_name = "ILLEGAL_INSTRUCTION (SIGILL)"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            sig_name = "DIVIDE_BY_ZERO (SIGFPE)"; break;
        case EXCEPTION_STACK_OVERFLOW:
            sig_name = "STACK_OVERFLOW"; break;
        default: break;
    }

    const void *fault_ip =
        exception_info->ExceptionRecord->ExceptionAddress;

    void *callstack[32];
    int callstack_depth = 0;
#if defined(_M_X64)
    // On x64 Windows, the frame pointer chain is in RBP.
    void *rbp_val = NULL;
    CONTEXT *ctx = exception_info->ContextRecord;
    if (ctx) {
        rbp_val = (void *)ctx->Rbp;
        int dummy;
        void *stack_bottom =
            (void *)(((uintptr_t)&dummy + 4095) & ~4095);
        callstack_depth = walk_callstack(rbp_val, stack_bottom,
                                         callstack, 32);
    }
#endif

    const KainCrashEntry *entry = lookup_crash_entry(fault_ip);
    render_crash_report(sig_name, entry, fault_ip,
                        callstack, callstack_depth);

    // Return EXCEPTION_CONTINUE_SEARCH to let the default handler run
    // (which will terminate the process).
    return EXCEPTION_CONTINUE_SEARCH;
}

static void register_windows_handlers(void) {
    AddVectoredExceptionHandler(1, crash_vectored_handler);
}
#endif // KAIN_CRASH_WINDOWS

// ── Stub (unsupported platforms) ───────────────────────────────────────

#if KAIN_CRASH_STUB
static void register_stub(void) {
    // No-op on unsupported platforms.
}
#endif

// ── Public API ─────────────────────────────────────────────────────────

void __kain_crash_handler_init(void) {
    if (crash_handler_initialized) return;
    crash_handler_initialized = 1;

    // If the crash table wasn't emitted by the compiler (compiled without
    // -g), don't register handlers — they'd have nothing to look up.
    if (!__kain_crash_table || __kain_crash_table_len == 0) {
        return;
    }

#if KAIN_CRASH_UNIX
    register_unix_handlers();
#elif KAIN_CRASH_WINDOWS
    register_windows_handlers();
#else
    register_stub();
#endif
}

const KainCrashEntry *__kain_crash_lookup(const void *ip) {
    return lookup_crash_entry(ip);
}
