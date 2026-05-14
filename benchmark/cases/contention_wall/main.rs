use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::Arc;
use std::thread;

const WORKER_COUNT: usize = 100;
const ITERATIONS_PER_WORKER: i64 = 1_000_000;
const EXPECTED: i64 = 100_000_000;

fn main() {
    let counter = Arc::new(AtomicI64::new(0));
    let mut workers = Vec::with_capacity(WORKER_COUNT);

    for _ in 0..WORKER_COUNT {
        let shared = Arc::clone(&counter);
        workers.push(thread::spawn(move || {
            let mut i = 0_i64;
            while i < ITERATIONS_PER_WORKER {
                shared.fetch_add(1, Ordering::SeqCst);
                i += 1;
            }
        }));
    }

    for worker in workers {
        worker.join().expect("worker thread panicked");
    }

    if counter.load(Ordering::SeqCst) != EXPECTED {
        std::process::exit(1);
    }
}
