const std = @import("std");

extern fn ffi_boundary_mix(value: i64, salt: i64) callconv(.c) i64;

pub fn main() void {
    const iterations: i64 = 10000000;
    const expected: i64 = 658273918;
    var acc: i64 = 1;
    var i: i64 = 0;
    while (i < iterations) : (i += 1) {
        acc = ffi_boundary_mix(acc + i, i);
    }

    if (acc != expected) {
        std.process.exit(1);
    }
}
