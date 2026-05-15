"use strict";

const STRING_TEXT = "ka0in0be0nch";
const STRING_NEEDLE = "in";
const STRING_TAIL = "ch";
const ITERATIONS = 100_000;
const MODULUS = 1_000_000_007;
const EXPECTED = 2_050_000;

function startsWithAt(text, index, needle) {
  if (index + needle.length > text.length) {
    return false;
  }
  let offset = 0;
  while (offset < needle.length) {
    if (text[index + offset] !== needle[offset]) {
      return false;
    }
    offset += 1;
  }
  return true;
}

function findSubstring(text, needle, start) {
  if (needle.length === 0) {
    return start;
  }
  let index = start;
  while (index + needle.length <= text.length) {
    if (startsWithAt(text, index, needle)) {
      return index;
    }
    index += 1;
  }
  return text.length;
}

let acc = 0;
let i = 0;
while (i < ITERATIONS) {
  if (i % 2 === 0) {
    acc = (acc + STRING_TEXT.length + findSubstring(STRING_TEXT, STRING_NEEDLE, 0) + STRING_NEEDLE.length) % MODULUS;
  } else {
    acc = (acc + STRING_TEXT.length + findSubstring(STRING_TEXT, STRING_TAIL, 0) + STRING_TAIL.length) % MODULUS;
  }
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
