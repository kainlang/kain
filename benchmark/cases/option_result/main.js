"use strict";

const ITERATIONS = 300_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 143_207_783;

function maybeValue(value) {
  if (value % 5 === 0) {
    return null;
  }
  return value + 3;
}

function parseValue(value) {
  if (value % 7 === 0) {
    return { ok: false, value: "skip" };
  }
  return { ok: true, value: value * 2 };
}

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  const maybe = maybeValue(i);
  const maybeComponent = maybe === null ? 1 : maybe;
  const parsed = parseValue(i);
  const parsedComponent = parsed.ok ? parsed.value : 2;
  acc = (acc + maybeComponent + parsedComponent) % MODULUS;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
