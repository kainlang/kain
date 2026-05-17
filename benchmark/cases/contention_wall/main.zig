const std = @import("std");

const worker_count: usize = 100;
const iterations_per_worker: i64 = 1_000_000;
const expected: i64 = 100_000_000;

const WorkerContext = struct {
    counter: *std.atomic.Value(i64),
};

fn worker_main(context: *WorkerContext) void {
    var index: i64 = 0;
    while (index < iterations_per_worker) : (index += 1) {
        _ = context.counter.fetchAdd(1, .seq_cst);
    }
}

pub fn main() void {
    var counter = std.atomic.Value(i64).init(0);
    var contexts: [worker_count]WorkerContext = undefined;
    var workers: [worker_count]std.Thread = undefined;

    var worker_index: usize = 0;
    while (worker_index < worker_count) : (worker_index += 1) {
        contexts[worker_index] = .{ .counter = &counter };
        workers[worker_index] = std.Thread.spawn(.{}, worker_main, .{&contexts[worker_index]}) catch std.process.exit(2);
    }

    worker_index = 0;
    while (worker_index < worker_count) : (worker_index += 1) {
        workers[worker_index].join();
    }

    if (counter.load(.seq_cst) != expected) {
        std.process.exit(1);
    }
}
