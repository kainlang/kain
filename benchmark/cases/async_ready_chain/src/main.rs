const ITERATIONS: u64 = 200_000;
const MODULUS: u64 = 1_000_000_007;
const EXPECTED: u64 = 1_399_991;

async fn ready_value() -> u64 {
    2
}

#[tokio::main(flavor = "current_thread")]
async fn main() {
    let mut acc = 0_u64;
    let mut i = 0_u64;
    while i < ITERATIONS {
        let awaited = ready_value().await;
        acc = (acc + awaited + (i % 11)) % MODULUS;
        i += 1;
    }

    if acc != EXPECTED {
        std::process::exit(1);
    }
}
