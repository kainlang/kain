const std = @import("std");

noinline fn ffiBoundaryMixLocal(value: i64, salt: i64) callconv(.c) i64 {
    const modulus: i64 = 1000000007;
    const lane_a = @rem((value * 1103515245) + 12345 + (salt * 97), modulus);
    const lane_b = @rem(@divTrunc(value, 7) + (salt * 31) + 17, modulus);
    return @rem(lane_a + lane_b + 19, modulus);
}

pub fn main() void {
    const iterations: i64 = 10000000;
    const expected: i64 = 658273918;
    var acc: i64 = 1;
    var i: i64 = 0;
    while (i < iterations) : (i += 1) {
        acc = ffiBoundaryMixLocal(acc + i, i);
    }

    if (acc != expected) {
        std.process.exit(1);
    }
}
