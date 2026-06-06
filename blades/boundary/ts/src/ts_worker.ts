/**
 * TS-side worker — validates primes for Kain
 * 
 * Called by Kain via process_output_text
 * Input: number as command-line argument
 * Output: JSON { n, is_prime, next_prime }
 */

function isPrime(n: number): boolean {
  if (n < 2) return false;
  for (let i = 2; i * i <= n; i++) {
    if (n % i === 0) return false;
  }
  return true;
}

function nextPrime(n: number): number {
  let candidate = n + 1;
  while (!isPrime(candidate)) candidate++;
  return candidate;
}

const n = parseInt(process.argv[2] || "0");
const prime = isPrime(n);
const next = nextPrime(n);

const result = { n, is_prime: prime, next_prime: next };
console.log(JSON.stringify(result));
