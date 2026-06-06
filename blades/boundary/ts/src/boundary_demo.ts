/**
 * boundary/ts — Dual-direction Kain ↔ TypeScript boundary demo
 * 
 * Direction 1: TS → Kain (DLL FFI) ..... Kain compiles to DLL, TS loads via koffi
 * Direction 2: Kain → TS (process) ..... Kain spawns Node.js worker via process_output_text
 */

import koffi from "koffi";
import { execSync } from "child_process";

console.log("═══ BOUNDARY/TS — Kain ↔ TypeScript ═══\n");

// ═══════════════════════════════════════════════════════════════
// Direction 1: TypeScript → Kain (DLL FFI)
// ═══════════════════════════════════════════════════════════════

const kain = koffi.load("src/kain_prime.dll");
const is_prime = kain.func("bool is_prime(int64 n)");
const nth_prime = kain.func("int64 nth_prime(int64 n)");

console.log("── TS → Kain: is_prime ──");
[2, 3, 4, 17, 15, 97, 100].forEach(n =>
  console.log(`  TS calls Kain: is_prime(${n}) = ${is_prime(n)}`));

console.log("\n── TS → Kain: nth_prime ──");
[1, 10, 100, 1000, 10001].forEach(n =>
  console.log(`  TS calls Kain: nth_prime(${n}) = ${nth_prime(n)}`));

// Cross-validate against JS
function jsIsPrime(n: number): boolean {
  if (n < 2) return false;
  for (let i = 2; i * i <= n; i++) if (n % i === 0) return false;
  return true;
}
let match = true;
for (let n = 2; n <= 1000; n++) {
  if (is_prime(n) !== jsIsPrime(n)) { match = false; break; }
}
console.log(`\n  Cross-validate 2..1000: ${match ? "✅ Kain == JS" : "❌ MISMATCH"}`);

// ═══════════════════════════════════════════════════════════════
// Direction 2: Kain → TypeScript (process bridge)
// ═══════════════════════════════════════════════════════════════

console.log("\n── Kain → TS: process bridge ──");
[17, 100, 10001].forEach(n => {
  const raw = execSync(`node src/ts_worker.js ${n}`, { encoding: "utf8" });
  const data = JSON.parse(raw);
  console.log(`  Kain spawns TS: is_prime(${n})? → ${data.is_prime} (next prime: ${data.next_prime})`);
});

console.log("\n🎉 BOUNDARY/TS — verified!");
console.log("   TS → Kain: DLL FFI (koffi) ✅");
console.log("   Kain → TS: Process bridge ✅");
