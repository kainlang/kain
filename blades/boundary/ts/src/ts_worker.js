// ts_worker.js — called by Kain via process_output_text
// Input: number as command-line argument
// Output: JSON { n, is_prime, next_prime }

function isPrime(n) {
  if (n < 2) return false;
  for (let i = 2; i * i <= n; i++) {
    if (n % i === 0) return false;
  }
  return true;
}

function nextPrime(n) {
  let candidate = n + 1;
  while (!isPrime(candidate)) candidate++;
  return candidate;
}

const n = parseInt(process.argv[2] || "0");
const result = { n, is_prime: isPrime(n), next_prime: nextPrime(n) };
console.log(JSON.stringify(result));
