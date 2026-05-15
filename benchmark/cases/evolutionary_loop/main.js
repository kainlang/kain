"use strict";

const ITERATIONS = 2_000_000;
const EXPECTED = 403_591_996;
const MODULUS = 1_000_000_007;

function scalarLane(value) {
  return ((value * 31) + 7) % MODULUS;
}

function wideLane(value) {
  return ((value * 31) + 7) % MODULUS;
}

function choose(value) {
  return wideLane(value);
}

function mix(value) {
  return ((value * 17) + 11) % MODULUS;
}

function pipeline(value) {
  return mix(choose(value));
}

let acc = 1;
let i = 0;
while (i < ITERATIONS) {
  acc = pipeline(acc + i);
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
