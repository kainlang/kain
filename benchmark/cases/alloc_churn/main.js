"use strict";

const ITERATIONS = 50_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 250_324_993;

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  const cell = { value: i + 7 };
  acc = (acc + cell.value) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
