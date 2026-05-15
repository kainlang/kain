"use strict";

const ITERATIONS = 1_000_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 393_996_945;

function makePair(seed) {
  return {
    x: seed % 97,
    y: (seed * 7) % 101,
  };
}

function scorePair(pair) {
  return (pair.x * 3) + (pair.y * 5);
}

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  const pair = makePair(i);
  acc = (acc + scorePair(pair)) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
