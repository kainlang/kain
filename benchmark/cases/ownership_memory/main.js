"use strict";

const ITERATIONS = 750_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 758_650_175;

function run() {
  const cell = { value: 0 };
  let i = 0;
  while (i < ITERATIONS) {
    const current = cell.value;
    cell.value = ((current * 33) + i + 7) % MODULUS;
    i += 1;
  }
  return cell.value;
}

if (run() !== EXPECTED) {
  process.exit(1);
}
