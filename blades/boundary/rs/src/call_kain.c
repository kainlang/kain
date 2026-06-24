// C program calling Kain-compiled functions via FFI
// Also proves Rust can call Kain since both use C ABI

#include <stdio.h>
#include <stdint.h>

// These are exported by the Kain-compiled .obj
int64_t kain_add(int64_t a, int64_t b);
int64_t kain_fib(int64_t n);
int64_t kain_multiply(int64_t a, int64_t b);

int main() {
    int64_t sum = kain_add(5, 7);
    printf("kain_add(5, 7) = %lld (expect 12) %s\n", 
           (long long)sum, sum == 12 ? "PASS" : "FAIL");

    int64_t fib = kain_fib(10);
    printf("kain_fib(10) = %lld (expect 55) %s\n", 
           (long long)fib, fib == 55 ? "PASS" : "FAIL");

    int64_t mul = kain_multiply(6, 7);
    printf("kain_multiply(6, 7) = %lld (expect 42) %s\n", 
           (long long)mul, mul == 42 ? "PASS" : "FAIL");

    return (sum == 12 && fib == 55 && mul == 42) ? 0 : 1;
}
