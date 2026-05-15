"use strict";

const CELLS = 262_144;
const MODULUS = 1_000_000_007;
const EXPECTED = 149_653_729;

const buffer = new Array(CELLS).fill(0);
let i = 0;
while (i < CELLS) {
  buffer[i] = (((i * 31) + 7) % MODULUS);
  i += 1;
}

let checksum = 0;
let j = 0;
while (j < CELLS) {
  checksum = (checksum + buffer[j]) % MODULUS;
  j += 1;
}

if (checksum !== EXPECTED) {
  process.exit(1);
}
