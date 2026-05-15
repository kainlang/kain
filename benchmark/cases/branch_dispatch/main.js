"use strict";

const ITERATIONS = 3_000_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 632_706_747;

function classify(value) {
  const tag = value % 8;
  if (tag === 0) {
    return value + 1;
  }
  if (tag === 1) {
    return (value * 3) + 7;
  }
  if (tag === 2) {
    return value - 5;
  }
  if (tag === 3) {
    return (value * value) + 11;
  }
  if (tag === 4) {
    return value + 17;
  }
  if (tag === 5) {
    return (value * 5) - 13;
  }
  if (tag === 6) {
    return value + 23;
  }
  return value - 11;
}

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  acc = (acc + classify(i)) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
