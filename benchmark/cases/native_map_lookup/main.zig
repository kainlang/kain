const std = @import("std");

const iterations: i64 = 1_200_000;
const modulus: i64 = 1_000_000_007;
const expected: i64 = 351_450_000;
const keys = [_][]const u8{
    "alpha",
    "beta",
    "gamma",
    "delta",
    "epsilon",
    "zeta",
    "eta",
    "theta",
    "iota",
    "kappa",
    "lambda",
    "mu",
    "nu",
    "xi",
    "omicron",
    "pi",
};
const values = [_]i64{ 11, 23, 37, 41, 53, 67, 79, 83, 97, 101, 113, 127, 131, 149, 157, 173 };

pub fn main() void {
    var metrics = std.StringHashMap(i64).init(std.heap.page_allocator);
    defer metrics.deinit();

    var key_index: usize = 0;
    while (key_index < keys.len) : (key_index += 1) {
        metrics.put(keys[key_index], values[key_index]) catch std.process.exit(2);
    }

    var acc: i64 = 0;
    var index: i64 = 0;
    while (index < iterations) : (index += 1) {
        const slot: usize = @intCast(@mod(index, 16));
        const value = metrics.get(keys[slot]).?;
        acc = @mod(acc + (value * (@mod(index, 5) + 1)) + (@as(i64, @intCast(slot)) * 3), modulus);
    }

    if (acc != expected) {
        std.process.exit(1);
    }
}
