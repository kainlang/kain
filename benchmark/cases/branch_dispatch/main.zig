const std = @import("std");

const iterations: i64 = 3_000_000;
const modulus: i64 = 1_000_000_007;
const expected: i64 = 632_706_747;

fn classify(value: i64) i64 {
    const tag = @mod(value, 8);
    if (tag == 0) {
        return value + 1;
    }
    if (tag == 1) {
        return (value * 3) + 7;
    }
    if (tag == 2) {
        return value - 5;
    }
    if (tag == 3) {
        return (value * value) + 11;
    }
    if (tag == 4) {
        return value + 17;
    }
    if (tag == 5) {
        return (value * 5) - 13;
    }
    if (tag == 6) {
        return value + 23;
    }
    return value - 11;
}

pub fn main() void {
    var acc: i64 = 0;
    var index: i64 = 0;
    while (index < iterations) : (index += 1) {
        acc = @mod(acc + @call(.never_inline, classify, .{index}), modulus);
    }
    if (acc != expected) {
        std.process.exit(1);
    }
}
