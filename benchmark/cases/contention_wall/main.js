"use strict";

const WORKER_COUNT = 100;
const ITERATIONS_PER_WORKER = 1_000_000;
const EXPECTED = 100_000_000;

let counter = 0;
let worker = 0;
while (worker < WORKER_COUNT) {
  let i = 0;
  while (i < ITERATIONS_PER_WORKER) {
    counter += 1;
    i += 1;
  }
  worker += 1;
}

if (counter !== EXPECTED) {
  process.exit(1);
}
