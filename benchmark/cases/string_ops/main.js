"use strict";

const STRING_TEXT = "ka0in0be0nch";
const STRING_NEEDLE = "in";
const STRING_TAIL = "ch";
const ITERATIONS = 100_000;
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
  const needleLength = needle.length;
  if (needleLength === 0) {
    return start;
  }
  let index = start;
  while (index + needleLength <= text.length) {
    if (startsWithAt(text, index, needle)) {
      return index;
    }
    index += 1;
  }
  return text.length;
}

let acc = 0;
let i = 0;
let useNeedle = true;
while (i < ITERATIONS) {
  if (useNeedle) {
    acc = acc + STRING_TEXT.length + findSubstring(STRING_TEXT, STRING_NEEDLE, 0) + STRING_NEEDLE.length;
  } else {
    acc = acc + STRING_TEXT.length + findSubstring(STRING_TEXT, STRING_TAIL, 0) + STRING_TAIL.length;
  }
  useNeedle = !useNeedle;
  i += 1;
}

if (acc !== EXPECTED) {
  process.exit(1);
}
