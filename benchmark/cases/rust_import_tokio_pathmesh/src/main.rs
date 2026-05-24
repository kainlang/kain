use std::path::PathBuf;
use tokio::time::{sleep, Duration};

const ITERATIONS: i64 = 150_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 625_422_207;

enum Mode {
    Warm,
    Hot,
}

struct LaneState {
    root: PathBuf,
    stride: i64,
    salt: i64,
}

impl LaneState {
    fn label_len_for_round(&self, round: i64) -> i64 {
        let label = if (round & 1) == 0 {
            self.root.join("warm.lane").to_string_lossy().to_string()
        } else {
            self.root.join("hot.lane").to_string_lossy().to_string()
        };
        label.len() as i64
    }

    fn fold(&self, mode: Mode, round: i64, pulse: i64, label_len: i64) -> i64 {
        match mode {
            Mode::Warm => {
                (((round + label_len) * self.stride) + pulse + self.salt + 7) % MODULUS
            }
            Mode::Hot => {
                (((round + label_len) * (self.stride + 3)) + pulse + self.salt + 19) % MODULUS
            }
        }
    }
}

fn select_mode(round: i64) -> Mode {
    if (round & 1) == 0 {
        Mode::Warm
    } else {
        Mode::Hot
    }
}

async fn pulse_once(label_len: i64, round: i64) -> i64 {
    sleep(Duration::from_millis(0)).await;
    ((label_len * 13) + (round * 17) + 23) % MODULUS
}

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let state = LaneState {
        root: PathBuf::from("benchmark")
            .join("cases")
            .join("rust_import_tokio_pathmesh"),
        stride: 17,
        salt: 29,
    };

    let mut acc = 0_i64;
    let mut round = 0_i64;
    while round < ITERATIONS {
        let mode = select_mode(round);
        let label_len = state.label_len_for_round(round);
        let pulse = pulse_once(label_len, round).await;
        acc = (acc + state.fold(mode, round, pulse, label_len)) % MODULUS;
        round += 1;
    }

    println!("{acc}");
    assert_eq!(acc, EXPECTED);
}
