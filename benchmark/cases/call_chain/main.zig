const std = @import("std");

const iterations: i64 = 1_500_000;
const modulus: i64 = 1_000_000_007;
const expected: i64 = 61_920_954;

fn step_a(value: i64) i64 {
    return @mod((value * 3) + 1, modulus);
}

fn step_b(value: i64) i64 {
    return @mod((@call(.never_inline, step_a, .{value}) + 5) * 7, modulus);
}

fn step_c(value: i64) i64 {
    return @mod(
        @call(.never_inline, step_b, .{value}) +
            @call(.never_inline, step_a, .{value + 11}) +
            13,
        modulus,
    );
}

fn step_d(value: i64) i64 {
    return @mod(
        (@call(.never_inline, step_c, .{value}) * 3) +
            @call(.never_inline, step_b, .{value + 17}) +
            19,
        modulus,
    );
}

pub fn main() void {
    var acc: i64 = 1;
    var index: i64 = 0;
    while (index < iterations) : (index += 1) {
        acc = @call(.never_inline, step_d, .{acc + index});
    }
    if (acc != expected) {
        std.process.exit(1);
    }
}
