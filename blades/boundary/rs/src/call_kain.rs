// Rust calling into Kain via FFI — boundary test
// Links against a Kain-compiled .obj + kain_runtime.lib
//
// Build:
//   rustc call_kain.rs -L X:/.kain/out/x86_64-windows/dev/ll/kain_math/compile/ \
//     --extern kain_math=X:/.kain/out/x86_64-windows/dev/ll/kain_math/compile/kain_math.obj \
//     -l kain_runtime:X:/.kain/lib/kain_runtime.lib \
//     -l user32 -l gdi32 -l opengl32 \
//     -C link-args="/NODEFAULTLIB:libcmt"
//
// Or via msvc:
//   cl call_kain.rs /link kain_math.obj kain_runtime.lib user32.lib gdi32.lib opengl32.lib

extern "C" {
    fn kain_add(a: i64, b: i64) -> i64;
    fn kain_fib(n: i64) -> i64;
    fn kain_multiply(a: i64, b: i64) -> i64;
}

fn main() {
    // SAFETY: Kain functions have C ABI and are stateless
    unsafe {
        let sum = kain_add(5, 7);
        println!("kain_add(5, 7) = {} (expect 12)", sum);
        assert_eq!(sum, 12);

        let fib = kain_fib(10);
        println!("kain_fib(10) = {} (expect 55)", fib);
        assert_eq!(fib, 55);

        let mul = kain_multiply(6, 7);
        println!("kain_multiply(6, 7) = {} (expect 42)", mul);
        assert_eq!(mul, 42);

        println!("All tests passed! Kain ↔ Rust FFI works.");
    }
}
