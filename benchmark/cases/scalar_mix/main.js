"use strict";

const ITERATIONS = 2_000_000;
const ADDEND = 17;
const OFFSET = ADDEND + 5;
const MODULUS = 1_000_000_007;
const EXPECTED = 42_986_000;

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  acc = (acc + i + OFFSET) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
