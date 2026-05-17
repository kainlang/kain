const std = @import("std");

const packet_count: i64 = 64;
const words_per_packet: usize = 4;
const iterations: i64 = 200_000;
const modulus: i64 = 1_000_000_007;
const expected: i64 = 924_829_641;
const buffer_word_count: usize = @as(usize, @intCast(packet_count)) * words_per_packet;

pub fn main() void {
    var buffer: [buffer_word_count]i64 = std.mem.zeroes([buffer_word_count]i64);
    var acc: i64 = 0;
    var round: i64 = 0;
    while (round < iterations) : (round += 1) {
        var packet: i64 = 0;
        while (packet < packet_count) : (packet += 1) {
            const seq = (round * packet_count) + packet;
            const version = @mod(packet, 4) + 1;
            const kind = @mod((packet * 3) + round, 8);
            const flags = @mod(round + packet, 16);
            const route = @mod((packet * 5) + 7, 64);
            const payload = @mod((seq * 13) + (route * 17) + 19, 4096);
            const word0 = (seq * 4096) + (kind * 256) + (flags * 16) + version;
            const word1 = (payload * 128) + route;
            const word2 = (@mod(seq, 97) * 2048) + (@mod(payload, 127) * 16) + flags;
            const word3 = @mod(word0 + word1 + word2 + 97, 1_000_003);
            const base: usize = @intCast(packet * @as(i64, words_per_packet));
            buffer[base + 0] = word0;
            buffer[base + 1] = word1;
            buffer[base + 2] = word2;
            buffer[base + 3] = word3;

            const observed0 = buffer[base + 0];
            const observed1 = buffer[base + 1];
            const observed2 = buffer[base + 2];
            const observed3 = buffer[base + 3];
            const observed_version = @mod(observed0, 16);
            const observed_flags = @mod(@divTrunc(observed0, 16), 16);
            const observed_kind = @mod(@divTrunc(observed0, 256), 16);
            const observed_seq = @divTrunc(observed0, 4096);
            const observed_route = @mod(observed1, 128);
            const observed_payload = @divTrunc(observed1, 128);
            const observed_epoch = @divTrunc(observed2, 2048);
            acc = @mod(
                acc +
                    observed_version +
                    observed_flags +
                    observed_kind +
                    @mod(observed_seq, 97) +
                    observed_route +
                    observed_payload +
                    observed_epoch +
                    observed3,
                modulus,
            );
        }
    }

    if (acc != expected) {
        std.process.exit(1);
    }
}
