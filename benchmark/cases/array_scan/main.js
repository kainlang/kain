"use strict";

const VALUES = [1, 2, 3, 4, 5, 6, 7, 8];
const ITERATIONS = 500_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 103_499_994;

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  let inner = 0;
  let index = 0;
  while (index < VALUES.length) {
    inner = (inner + (VALUES[index] * (index + 1))) % MODULUS;
    index += 1;
  }
  acc = (acc + inner + (i % 7)) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
