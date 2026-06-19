// QEMU debug port output — uses I/O port 0xE9
// This works because we compile with -ffreestanding and the
// __attribute__((noinline)) ensures it's a real function

__attribute__((noinline))
void qemu_debug_putc(char c) {
    __asm__ volatile("outb %0, $0xE9" : : "a"(c));
}

void qemu_debug_puts(const char* s) {
    while (*s) {
        qemu_debug_putc(*s++);
    }
}

// String passthrough (Kain wraps all strings with string_new)
void* string_new(void* s) { return s; }

// Minimal stubs
void print_i64(long long x) {}
void print_str(void* s, long long len) {}
void __kain_crash_handler_init() {}
void rc_retain(void* x) {}
void rc_release(void* x) {}
void* KAIN_alloc(long long size) { return 0; }
