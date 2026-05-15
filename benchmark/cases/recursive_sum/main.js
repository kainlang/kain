"use strict";

const ITERATIONS = 5_000;
const DEPTH = 128;
const MODULUS = 1_000_000_007;
const EXPECTED = 41_280_000;

function recursiveSum(value) {
  if (value <= 0) {
    return 0;
  }
  return value + recursiveSum(value - 1);
}

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  acc = (acc + recursiveSum(DEPTH)) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
