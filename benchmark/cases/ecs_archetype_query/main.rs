const ENTITY_COUNT: usize = 32;
const ITERATIONS: i64 = 350_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 886_666_628;

fn main() {
    let mut position_x = [0_i64; ENTITY_COUNT];
    let mut position_y = [0_i64; ENTITY_COUNT];
    let mut velocity_x = [0_i64; ENTITY_COUNT];
    let mut velocity_y = [0_i64; ENTITY_COUNT];
    let mut health = [0_i64; ENTITY_COUNT];
    let mut team = [0_i64; ENTITY_COUNT];
    let mut active = [false; ENTITY_COUNT];

    let mut index = 0_usize;
    while index < ENTITY_COUNT {
        let i = index as i64;
        position_x[index] = ((i * 17) % 97) + 3;
        position_y[index] = ((i * 29) % 89) + 5;
        velocity_x[index] = ((i * 7) % 11) + 1;
        velocity_y[index] = ((i * 5) % 13) + 2;
        health[index] = ((i * 19) % 41) + 9;
        team[index] = i % 4;
        active[index] = (i % 3) != 1;
        index += 1;
    }

    let mut acc = 0_i64;
    let mut round = 0_i64;
    while round < ITERATIONS {
        let round_phase = round % 5;
        let round_bias = round % 7;
        let mut lane = 0_usize;
        while lane < ENTITY_COUNT {
            let lane_i64 = lane as i64;
            if active[lane] && health[lane] > ((round + lane_i64) % 11) {
                let motion = position_x[lane] + velocity_x[lane] * (round_phase + 1);
                let support = position_y[lane] + velocity_y[lane] * ((round_bias % 3) + 2);
                if ((team[lane] + round + lane_i64) % 3) == 0 {
                    acc = (acc + motion + support + health[lane] + lane_i64) % MODULUS;
                } else {
                    acc = (acc + motion + (support * 2) + team[lane] + 17) % MODULUS;
                }
            } else {
                acc = (acc + team[lane] + lane_i64 + 23) % MODULUS;
            }
            lane += 1;
        }
        round += 1;
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
