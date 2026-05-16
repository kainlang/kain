const X: [i64; 8] = [3, 13, 29, 43, 61, 79, 101, 113];
const Y: [i64; 8] = [5, 17, 31, 47, 67, 83, 103, 127];
const VX: [i64; 8] = [7, 19, 37, 53, 71, 89, 107, 131];
const VY: [i64; 8] = [11, 23, 41, 59, 73, 97, 109, 137];
const ALIVE: [bool; 8] = [true, false, true, false, true, false, true, false];

fn main() {
    const ITERATIONS: i64 = 500_000;
    const EXPECTED: i64 = -1_399_052_960;
    let mut acc = 0i64;
    let mut round = 0i64;

    while round < ITERATIONS {
        let mut lane = 0usize;
        while lane < X.len() {
            if ALIVE[lane] {
                acc += (((X[lane] + round) % 97) * VX[lane]) + Y[lane] + lane as i64;
            } else {
                acc = acc - (((Y[lane] + round) % 89) * VY[lane]) + X[lane] - lane as i64;
            }
            lane += 1;
        }
        round += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
