"use strict";

const ITERATIONS = 1_500_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 61_920_954;

function stepA(value) {
  return ((value * 3) + 1) % MODULUS;
}

function stepB(value) {
  return ((stepA(value) + 5) * 7) % MODULUS;
}

function stepC(value) {
  return (stepB(value) + stepA(value + 11) + 13) % MODULUS;
}

function stepD(value) {
  return ((stepC(value) * 3) + stepB(value + 17) + 19) % MODULUS;
}

let acc = 1;
let i = 0;
while (i < ITERATIONS) {
  acc = stepD(acc + i);
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
