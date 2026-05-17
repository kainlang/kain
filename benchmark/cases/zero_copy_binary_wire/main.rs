const PACKET_COUNT: usize = 64;
const WORDS_PER_PACKET: usize = 4;
const ITERATIONS: i64 = 200_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 924_829_641;

fn main() {
    let mut buffer = [0_i64; PACKET_COUNT * WORDS_PER_PACKET];
    let mut acc = 0_i64;
    let mut round = 0_i64;
    while round < ITERATIONS {
        let mut packet = 0_usize;
        while packet < PACKET_COUNT {
            let packet_i64 = packet as i64;
            let seq = (round * PACKET_COUNT as i64) + packet_i64;
            let version = (packet_i64 % 4) + 1;
            let kind = ((packet_i64 * 3) + round) % 8;
            let flags = (round + packet_i64) % 16;
            let route = ((packet_i64 * 5) + 7) % 64;
            let payload = ((seq * 13) + (route * 17) + 19) % 4096;
            let word0 = (seq * 4096) + (kind * 256) + (flags * 16) + version;
            let word1 = (payload * 128) + route;
            let word2 = ((seq % 97) * 2048) + ((payload % 127) * 16) + flags;
            let word3 = (word0 + word1 + word2 + 97) % 1_000_003;
            let base = packet * WORDS_PER_PACKET;
            buffer[base] = word0;
            buffer[base + 1] = word1;
            buffer[base + 2] = word2;
            buffer[base + 3] = word3;

            let observed0 = buffer[base];
            let observed1 = buffer[base + 1];
            let observed2 = buffer[base + 2];
            let observed3 = buffer[base + 3];
            let observed_version = observed0 % 16;
            let observed_flags = (observed0 / 16) % 16;
            let observed_kind = (observed0 / 256) % 16;
            let observed_seq = observed0 / 4096;
            let observed_route = observed1 % 128;
            let observed_payload = observed1 / 128;
            let observed_epoch = observed2 / 2048;
            acc = (acc
                + observed_version
                + observed_flags
                + observed_kind
                + (observed_seq % 97)
                + observed_route
                + observed_payload
                + observed_epoch
                + observed3)
                % MODULUS;
            packet += 1;
        }
        round += 1;
    }

    if unsafe { std::ptr::read_volatile(&acc) } != EXPECTED {
        std::process::exit(1);
    }
}
