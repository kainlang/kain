// KAINOS freestanding runtime stubs
// Provide minimal symbols the Kain compiler emits

// string_new — Kain wraps all string literals with this.
// In freestanding mode, just return the pointer unchanged.
void* string_new(void* s) {
    return s;
}

// Additional stubs that may be needed
void print_i64(long long x) {}
void print_f64(double x) {}
void print_bool(int x) {}
void print_str(void* s, long long len) {}
void __kain_crash_handler_init() {}
void* to_string(long long x) { return 0; }
void* to_string_any(long long x) { return 0; }
void* str_concat(void* a, void* b) { return a; }
void* str_concat3(void* a, void* b, void* c) { return a; }
void* str_concat4(void* a, void* b, void* c, void* d) { return a; }
void rc_retain(void* x) {}
void rc_release(void* x) {}
long long strlen_kain(void* s) { return 0; }
void* KAIN_alloc(long long size) { return 0; }
long long clock_wrapper() { return 0; }

// Prevent clang from optimizing away string constants
void __keep_strings() {
    // This ensures the string data stays in the binary
}
