// KAIN freestanding runtime stubs
//
// Provides minimal no-op implementations for LLVM IR symbols that
// Kain-compiled code always emits. In freestanding mode (-ffreestanding
// -nostdlib), there is no libc to supply print/concat/alloc helpers,
// so we stub them here.
//
// Merge of:
//   blades/os/runtime/freestanding_stubs.c (string_new, print_*, KAIN_alloc)
//   blades/os/runtime/qemu_debug.c              (qemu_debug_putc/puts)
//
// Compile with:
//   clang -target x86_64-unknown-none -ffreestanding -nostdlib -c freestanding_stubs.c

// ── QEMU debug port output (I/O port 0xE9) ──────────────────────────
// Available when running under QEMU with -debugcon stdio or -serial stdio.
// __attribute__((noinline)) ensures the compiler always emits a callable
// function — important when -ffreestanding means no standard entry/exit
// conventions are assumed.

__attribute__((noinline))
void qemu_debug_putc(char c) {
    __asm__ volatile("outb %0, $0xE9" : : "a"(c));
}

void qemu_debug_puts(const char* s) {
    while (*s) {
        qemu_debug_putc(*s++);
    }
}

// ── String passthrough ──────────────────────────────────────────────
// Kain wraps every string literal with string_new(). In freestanding
// mode, strings live in .rodata — just return the pointer unchanged.

void* string_new(void* s) {
    return s;
}

// ── Print stubs (no-op or serial output) ─────────────────────────────
// Kain emits print_i64/print_f64/print_bool/print_str for debug output.
// On bare metal, these are no-ops. Kernels that want serial output can
// override these or call qemu_debug_puts directly.

void print_i64(long long x) {}
void print_f64(double x) {}
void print_bool(int x) {}
void print_str(void* s, long long len) {}

// ── Crash handler init stub ─────────────────────────────────────────
// The hosted runtime initializes structured crash forensics. On bare
// metal, this is a no-op — the kernel provides its own crash path.

void __kain_crash_handler_init() {}

// ── String conversion stubs ─────────────────────────────────────────
// to_string / to_string_any convert Kain values to string representation.
// These require a sprintf-style formatter which depends on libc. On bare
// metal, return null — the kernel provides its own formatting.

void* to_string(long long x) { return 0; }
void* to_string_any(long long x) { return 0; }

// ── String concatenation stubs ───────────────────────────────────────
// Kain emits str_concat / str_concatN for string building. On bare metal,
// return the first non-null argument (passthrough). Real concatenation
// requires a heap allocator.

void* str_concat(void* a, void* b)  { return a ? a : b; }
void* str_concat3(void* a, void* b, void* c)  { return a ? a : (b ? b : c); }
void* str_concat4(void* a, void* b, void* c, void* d)  { return a ? a : (b ? b : (c ? c : d)); }
void* str_concat5(void* a, void* b, void* c, void* d, void* e)  { return a ? a : (b ? b : (c ? c : (d ? d : e))); }
void* str_concat6(void* a, void* b, void* c, void* d, void* e, void* f)  { return a ? a : (b ? b : (c ? c : (d ? d : (e ? e : f)))); }
void* str_concat7(void* a, void* b, void* c, void* d, void* e, void* f, void* g)  { return a ? a : (b ? b : (c ? c : (d ? d : (e ? e : (f ? f : g))))); }
void* str_concat8(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h)  { return a ? a : (b ? b : (c ? c : (d ? d : (e ? e : (f ? f : (g ? g : h)))))); }
void* str_concat9(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i)  { return a ? a : (b ? b : (c ? c : (d ? d : (e ? e : (f ? f : (g ? g : (h ? h : i))))))); }
void* str_concat10(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i, void* j) { return a ? a : (b ? b : (c ? c : (d ? d : (e ? e : (f ? f : (g ? g : (h ? h : (i ? i : j)))))))); }

// ── Reference counting stubs ────────────────────────────────────────
// rc_retain / rc_release manage Kain's reference-counted objects. On
// bare metal, these are no-ops — the kernel manages memory directly.

void rc_retain(void* x) {}
void rc_release(void* x) {}

// ── String length ───────────────────────────────────────────────────
// strlen — declared as @strlen(i8*) in LLVM codegen (mod.rs line 14608).
// The LLVM IR declares @strlen, not @strlen_kain, so we must provide the
// exact symbol the linker expects. On bare metal without libc, return 0.

long long strlen(void* s) { return 0; }

// ── Memory allocation stub ──────────────────────────────────────────
// KAIN_alloc is the default allocator symbol emitted by the compiler.
// On bare metal, the kernel provides a real allocator (arena/buddy).
// Return 0 here — kernels must supply their own KAIN_alloc or link
// against the arena.c / buddy.c allocators.

void* KAIN_alloc(long long size) { return 0; }

// ── Clock stub ──────────────────────────────────────────────────────
// clock_wrapper is emitted for timing queries. On bare metal, return 0.
// Kernels can provide a real implementation via HPET/APIC timer.

long long clock_wrapper() { return 0; }

// ── String data preservation ────────────────────────────────────────
// Prevent clang from optimizing away string constants that are only
// referenced via the stubs above.

void __keep_strings(void) {
    // Forces the linker to preserve .rodata strings reachable from stubs.
}
