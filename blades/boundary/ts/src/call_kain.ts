/**
 * boundary/ts — TypeScript calling into Kain via native DLL
 * 
 * Build flow:
 *   1. kain build src/kain_prime.kn --target llvm
 *   2. cp .kain/out/.../kain_prime.ll → src/kain_prime.ll
 *   3. Edit LLVM IR: change `internal` → `dllexport`
 *   4. clang -shared -O2 -nostdlib -Wl,-noentry src/kain_prime.ll -o src/kain_prime.dll
 *   5. npx tsx src/call_kain.ts
 */

import koffi from "koffi";

// ── Load the Kain-compiled DLL ──────────────────────────────────────
const lib = koffi.load("src/kain_prime.dll");

// ── Bind exported Kain functions ────────────────────────────────────
const is_prime = lib.func("bool is_prime(int64 n)");
const nth_prime = lib.func("int64 nth_prime(int64 n)");

// ── Test 1: is_prime ────────────────────────────────────────────────
console.log("═══ Kain ← TypeScript FFI Boundary ═══\n");

const testNumbers = [2, 3, 4, 17, 15, 97, 100];
for (const n of testNumbers) {
  const result = is_prime(n);
  console.log(`  is_prime(${n}) = ${result}`);
}

// ── Test 2: nth_prime ───────────────────────────────────────────────
console.log("");
const primes = [1, 10, 100, 1000, 10001];
for (const n of primes) {
  const result = nth_prime(n);
  console.log(`  nth_prime(${n}) = ${result}`);
}

// ── Verify against known values ─────────────────────────────────────
console.log("\n── Verification ──");
const checks: [number, bigint][] = [
  [1, 2n],      // 1st prime
  [10, 29n],    // 10th prime
  [100, 541n],  // 100th prime
  [1000, 7919n],// 1000th prime
  [10001, 104743n], // 10001st prime
];

let allPass = true;
for (const [n, expected] of checks) {
  const result = Number(nth_prime(n));
  const pass = result === Number(expected);
  if (!pass) allPass = false;
  console.log(`  nth_prime(${n}) = ${result} ${pass ? "✅" : `❌ (expected ${expected})`}`);
}

console.log(allPass ? "\n🎉 ALL PASS — Kain DLL works from TypeScript!" : "\n❌ SOME FAILED");
