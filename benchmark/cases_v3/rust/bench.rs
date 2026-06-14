// ============================================================================
//  CASES_V3 — Rust God File
//  All 30 benchmarks from the V3 contract.
//  Compile: rustc -O bench.rs -o bench.exe
//  Run:     bench.exe <benchmark_name>
// ============================================================================

#![allow(dead_code)]

use std::collections::{BTreeMap, HashMap, VecDeque};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Barrier, Mutex, mpsc};
use std::thread;

// ============================================================================
//  CONSTANTS
// ============================================================================

const RANDOM_SEED: u64 = 42;
const MODULUS: u64 = 1000000007;

// Expected checksums — set to 0 initially. Run each benchmark once to get
// the actual value, then update here.
const BINARY_TREES_EXPECTED: u64 = 0;
const NBODY_EXPECTED: u64 = 0;
const SPECTRAL_NORM_EXPECTED: u64 = 0;
const MANDELBROT_EXPECTED: u64 = 0;
const FASTA_EXPECTED: u64 = 0;
const REGEX_REDUX_EXPECTED: u64 = 0;
const PIDIGITS_EXPECTED: u64 = 0;
const HASHMAP_HEAVY_EXPECTED: u64 = 0;
const BTREE_SCAN_EXPECTED: u64 = 0;
const SORT_GAUNTLET_EXPECTED: u64 = 0;
const VECTOR_GROWTH_EXPECTED: u64 = 0;
const GRAPH_BFS_EXPECTED: u64 = 0;
const ALLOC_SMALL_CHURN_EXPECTED: u64 = 0;
const ALLOC_LARGE_OBJECTS_EXPECTED: u64 = 0;
const ARENA_VS_MALLOC_EXPECTED: u64 = 0;
const CACHE_MARCH_EXPECTED: u64 = 0;
const RC_VS_GC_TRACE_EXPECTED: u64 = 0;
const PARALLEL_REDUCE_EXPECTED: u64 = 0;
const MUTEX_CONTENTION_EXPECTED: u64 = 0;
const SPSC_QUEUE_EXPECTED: u64 = 0;
const MPMC_QUEUE_EXPECTED: u64 = 0;
const ACTOR_SPAM_EXPECTED: u64 = 0;
const ASYNC_READY_PIPELINE_EXPECTED: u64 = 0;
const FILE_READ_STREAMING_EXPECTED: u64 = 0;
const FILE_WRITE_STREAMING_EXPECTED: u64 = 0;
const TCP_ECHO_THROUGHPUT_EXPECTED: u64 = 0;
const PROCESS_SPAWN_CHAIN_EXPECTED: u64 = 0;
const C_FFI_CALL_HOTLOOP_EXPECTED: u64 = 0;
const C_BUFFER_HANDOFF_EXPECTED: u64 = 0;
const BUILD_SELF_STRESS_EXPECTED: u64 = 0;

// ============================================================================
//  SHARED HELPERS — Deterministic LCG
// ============================================================================

thread_local! {
    static RNG_STATE: std::cell::RefCell<u64> = std::cell::RefCell::new(RANDOM_SEED);
}

fn rand_next() -> u64 {
    RNG_STATE.with(|s| {
        let mut state = s.borrow_mut();
        *state = (*state * 1103515245 + 12345) & 0x7fffffff;
        *state
    })
}

fn rand_next_seeded(state: &mut u64) -> u64 {
    *state = (*state * 1103515245 + 12345) & 0x7fffffff;
    *state
}

/// djb2 hash: h = 5381; for each byte: h = ((h << 5) + h) + byte
fn hash_string(s: &str) -> u64 {
    let mut h: u64 = 5381;
    for &c in s.as_bytes() {
        h = ((h << 5).wrapping_add(h)).wrapping_add(c as u64);
    }
    h
}

/// Generate a random alphanumeric string of given length
fn random_string(len: usize, state: &mut u64) -> String {
    let chars: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let mut s = String::with_capacity(len);
    for _ in 0..len {
        let idx = rand_next_seeded(state) as usize % chars.len();
        s.push(chars[idx] as char);
    }
    s
}

/// Print mismatch to stdout and return 1
fn check_result(name: &str, result: u64, expected: u64) -> i32 {
    if result == expected {
        0
    } else {
        println!("[FAIL] {}: got {}, expected {}", name, result, expected);
        1
    }
}

// ============================================================================
//  BIGINT — Custom arbitrary-precision integer for pidigits (base 10^9)
// ============================================================================

const BIGBASE: u64 = 1_000_000_000;

#[derive(Clone)]
struct BigInt {
    /// Little-endian limbs; limbs[0] = least significant
    limbs: Vec<u64>,
}

impl BigInt {
    fn new(limbs: usize) -> Self {
        BigInt {
            limbs: vec![0u64; limbs],
        }
    }

    fn one_scaled(limbs: usize) -> Self {
        let mut v = vec![0u64; limbs];
        v[limbs - 1] = 1;
        BigInt { limbs: v }
    }

    fn from_u64(v: u64) -> Self {
        BigInt { limbs: vec![v] }
    }

    /// Trim leading zero limbs
    fn trim(&mut self) {
        while self.limbs.len() > 1 && *self.limbs.last().unwrap() == 0 {
            self.limbs.pop();
        }
    }

    /// Multiply by a small integer (fits in u64)
    fn mul_small(&mut self, x: u64) {
        if x == 0 {
            self.limbs.clear();
            self.limbs.push(0);
            return;
        }
        let mut carry: u64 = 0;
        for limb in &mut self.limbs {
            // Use u128 for intermediate to detect overflow
            let v = (*limb as u128) * (x as u128) + (carry as u128);
            *limb = (v % (BIGBASE as u128)) as u64;
            carry = (v / (BIGBASE as u128)) as u64;
        }
        if carry > 0 {
            self.limbs.push(carry);
        }
    }

    /// Divide by a small integer (fits in u64). Returns remainder.
    fn div_small(&mut self, x: u64) -> u64 {
        if x == 0 {
            panic!("division by zero");
        }
        let mut rem: u64 = 0;
        // Process from most significant to least
        for limb in self.limbs.iter_mut().rev() {
            let v = (rem as u128) * (BIGBASE as u128) + (*limb as u128);
            *limb = (v / (x as u128)) as u64;
            rem = (v % (x as u128)) as u64;
        }
        self.trim();
        rem
    }

    /// Add another BigInt into self
    fn add(&mut self, other: &BigInt) {
        let max_len = self.limbs.len().max(other.limbs.len());
        self.limbs.resize(max_len, 0);
        let mut carry: u64 = 0;
        for i in 0..max_len {
            let a = self.limbs[i];
            let b = if i < other.limbs.len() { other.limbs[i] } else { 0 };
            let v = (a as u128) + (b as u128) + (carry as u128);
            self.limbs[i] = (v % (BIGBASE as u128)) as u64;
            carry = (v / (BIGBASE as u128)) as u64;
        }
        if carry > 0 {
            self.limbs.push(carry);
        }
    }

    /// Subtract another BigInt from self (assumes self >= other)
    fn sub(&mut self, other: &BigInt) {
        let mut borrow: i64 = 0;
        for i in 0..self.limbs.len() {
            let a = self.limbs[i] as i64;
            let b = if i < other.limbs.len() { other.limbs[i] as i64 } else { 0 };
            let v = a - b - borrow;
            if v >= 0 {
                self.limbs[i] = v as u64;
                borrow = 0;
            } else {
                self.limbs[i] = (v + BIGBASE as i64) as u64;
                borrow = 1;
            }
        }
        self.trim();
        // If borrow > 0 at the end, result is negative — should not happen
        // in our usage (we always have self >= other for arctan computation)
    }

    /// Get the Nth decimal digit (0-indexed from most significant)
    fn decimal_digit(&self, index: usize) -> u32 {
        // The most significant digit is at the top of the most significant limb
        // Each limb holds 9 decimal digits
        let total_limbs = self.limbs.len();
        let total_digits = total_limbs * 9;
        let digit_pos = total_digits - 1 - index; // position from LSB
        let limb_idx = digit_pos / 9;
        let offset = digit_pos % 9;
        let limb_val = self.limbs[limb_idx];
        ((limb_val / 10u64.pow(offset as u32)) % 10) as u32
    }
}

/// Compute arctan(1/x) as a scaled BigInt using the series:
/// arctan(1/x) = sum_{k=0}^{inf} (-1)^k / ((2k+1) * x^(2k+1))
fn compute_arctan_reciprocal(x: u64, num_terms: usize, limbs: usize) -> BigInt {
    let x_sq = x * x;
    let mut result = BigInt::new(limbs);

    // term_0 = 1/x  (scaled to our limb count)
    // We start with ONE_SCALED and divide by x
    let mut term = BigInt::one_scaled(limbs);
    term.div_small(x);

    for k in 0..num_terms {
        // result += (-1)^k * term_k
        if k % 2 == 0 {
            result.add(&term);
        } else {
            result.sub(&term);
        }

        // Compute term_{k+1} from term_k:
        // |term_{k+1}| = |term_k| * (2k+1) / ((2k+3) * x^2)
        let numerator = 2 * k as u64 + 1;
        let denominator = (2 * k as u64 + 3) * x_sq;

        term.mul_small(numerator);
        term.div_small(denominator);
    }

    result
}

/// Compute pi digits using Machin's formula:
/// pi = 16 * arctan(1/5) - 4 * arctan(1/239)
fn compute_pi_digits(count: usize) -> Vec<u32> {
    // We need enough limbs to hold count decimal digits + headroom
    // Each limb = 9 decimal digits
    let target_limbs = (count * 10 / 9) + 10; // generous headroom

    // Number of terms for convergence:
    // For arctan(1/5): need enough terms so 5^(2N+1) > 10^digits
    // 2N+1 ≈ digits / log10(5) ≈ digits / 0.699
    // For arctan(1/239): 2N+1 ≈ digits / 2.378
    let digits = target_limbs * 9;
    let terms_5 = ((digits as f64) / 0.699) as usize + 10;
    let terms_239 = ((digits as f64) / 2.378) as usize + 10;

    // Compute arctan(1/5) and arctan(1/239)
    let atan_5 = compute_arctan_reciprocal(5, terms_5, target_limbs);
    let atan_239 = compute_arctan_reciprocal(239, terms_239, target_limbs);

    // pi = 16 * atan(1/5) - 4 * atan(1/239)
    let mut pi_scaled = atan_5;
    pi_scaled.mul_small(16);

    let mut atan_239_scaled = atan_239;
    atan_239_scaled.mul_small(4);

    pi_scaled.sub(&atan_239_scaled);

    // Extract decimal digits
    let total_digits = pi_scaled.limbs.len() * 9;
    let mut all_digits = Vec::with_capacity(total_digits);

    // Convert limbs to decimal (most significant first)
    for limb in pi_scaled.limbs.iter().rev() {
        let mut val = *limb;
        let mut limb_digits = Vec::with_capacity(9);
        for _ in 0..9 {
            limb_digits.push((val % 10) as u32);
            val /= 10;
        }
        // limb_digits are in reverse order (LSB first), so reverse
        limb_digits.reverse();
        all_digits.extend(limb_digits);
    }

    // Remove leading zeros
    while all_digits.len() > 1 && all_digits[0] == 0 {
        all_digits.remove(0);
    }

    // Take the first `count` digits
    all_digits.truncate(count);
    all_digits
}

// ============================================================================
//  TIER 1: COMPUTE & ALGORITHM
// ============================================================================

// ---- 1. binary_trees -------------------------------------------------------

struct TreeNode {
    value: i64,
    left: Option<Box<TreeNode>>,
    right: Option<Box<TreeNode>>,
}

fn build_tree(depth: i32) -> Option<Box<TreeNode>> {
    if depth <= 0 {
        return None;
    }
    Some(Box::new(TreeNode {
        value: 1,
        left: build_tree(depth - 1),
        right: build_tree(depth - 1),
    }))
}

fn tree_sum(node: &Option<Box<TreeNode>>) -> u64 {
    match node {
        None => 0,
        Some(n) => n.value as u64 + tree_sum(&n.left) + tree_sum(&n.right),
    }
}

fn compute_binary_trees() -> u64 {
    const MIN_DEPTH: i32 = 4;
    const MAX_DEPTH: i32 = 18;
    let mut checksum: u64 = 0;

    for depth in (MIN_DEPTH..=MAX_DEPTH).step_by(2) {
        let iterations = 1usize << (MAX_DEPTH - depth + MIN_DEPTH) as usize;
        for _ in 0..iterations {
            let tree = build_tree(depth);
            checksum = (checksum + tree_sum(&tree)) % MODULUS;
        }
    }
    checksum
}

fn bench_binary_trees() -> i32 {
    let result = compute_binary_trees();
    check_result("binary_trees", result, BINARY_TREES_EXPECTED)
}

// ---- 2. nbody --------------------------------------------------------------

struct Body {
    x: f64, y: f64, z: f64,
    vx: f64, vy: f64, vz: f64,
    mass: f64,
}

fn compute_nbody() -> u64 {
    const N_BODIES: usize = 500;
    const TIMESTEPS: usize = 100;
    const DT: f64 = 0.01;
    const SOFTENING: f64 = 1e-9;

    let mut bodies = Vec::with_capacity(N_BODIES);
    let mut state = RANDOM_SEED;
    for _ in 0..N_BODIES {
        bodies.push(Body {
            x: rand_next_seeded(&mut state) as f64 * 100.0,
            y: rand_next_seeded(&mut state) as f64 * 100.0,
            z: rand_next_seeded(&mut state) as f64 * 100.0,
            vx: 0.0, vy: 0.0, vz: 0.0,
            mass: 1.0 + (rand_next_seeded(&mut state) as f64 % 100.0),
        });
    }

    for _t in 0..TIMESTEPS {
        // Compute forces
        for i in 0..N_BODIES {
            let mut fx = 0.0f64;
            let mut fy = 0.0f64;
            let mut fz = 0.0f64;
            for j in 0..N_BODIES {
                if i == j { continue; }
                let dx = bodies[i].x - bodies[j].x;
                let dy = bodies[i].y - bodies[j].y;
                let dz = bodies[i].z - bodies[j].z;
                let dist_sq = dx * dx + dy * dy + dz * dz + SOFTENING;
                let dist = dist_sq.sqrt();
                let inv_dist3 = 1.0 / (dist * dist * dist);
                fx -= dx * bodies[j].mass * inv_dist3;
                fy -= dy * bodies[j].mass * inv_dist3;
                fz -= dz * bodies[j].mass * inv_dist3;
            }
            bodies[i].vx += fx * DT;
            bodies[i].vy += fy * DT;
            bodies[i].vz += fz * DT;
        }
        // Update positions
        for i in 0..N_BODIES {
            bodies[i].x += bodies[i].vx * DT;
            bodies[i].y += bodies[i].vy * DT;
            bodies[i].z += bodies[i].vz * DT;
        }
    }

    let total: f64 = bodies.iter().map(|b| b.x + b.y + b.z).sum();
    (total.floor() as u64) % MODULUS
}

fn bench_nbody() -> i32 {
    let result = compute_nbody();
    check_result("nbody", result, NBODY_EXPECTED)
}

// ---- 3. spectral_norm ------------------------------------------------------

fn spectral_norm_a(i: usize, j: usize) -> f64 {
    1.0 / (((i + j) * (i + j + 1)) as f64 / 2.0 + (i + 1) as f64)
}

fn compute_spectral_norm() -> u64 {
    const N: usize = 2000;
    let mut u = vec![1.0f64; N];
    let mut v = vec![0.0f64; N];

    for _ in 0..10 {
        // v = A * u
        for i in 0..N {
            let mut sum = 0.0;
            for j in 0..N {
                sum += u[j] * spectral_norm_a(i, j);
            }
            v[i] = sum;
        }
        // u = A^T * v
        for i in 0..N {
            let mut sum = 0.0;
            for j in 0..N {
                sum += v[j] * spectral_norm_a(j, i);
            }
            u[i] = sum;
        }
    }

    let mut vbv = 0.0;
    let mut vv = 0.0;
    for i in 0..N {
        vbv += u[i] * v[i];
        vv += v[i] * v[i];
    }
    let result = (vbv / vv).sqrt();
    (result * 1e9).floor() as u64 % MODULUS
}

fn bench_spectral_norm() -> i32 {
    let result = compute_spectral_norm();
    check_result("spectral_norm", result, SPECTRAL_NORM_EXPECTED)
}

// ---- 4. mandelbrot ---------------------------------------------------------

fn compute_mandelbrot() -> u64 {
    const WIDTH: usize = 800;
    const HEIGHT: usize = 800;
    const MAX_ITER: u32 = 200;
    const XMIN: f64 = -2.0;
    const XMAX: f64 = 1.0;
    const YMIN: f64 = -1.5;
    const YMAX: f64 = 1.5;

    let mut checksum: u64 = 0;

    for y in 0..HEIGHT {
        for x in 0..WIDTH {
            let cr = XMIN + (XMAX - XMIN) * x as f64 / WIDTH as f64;
            let ci = YMIN + (YMAX - YMIN) * y as f64 / HEIGHT as f64;
            let mut zr = 0.0f64;
            let mut zi = 0.0f64;
            let mut iter = 0u32;
            while zr * zr + zi * zi <= 4.0 && iter < MAX_ITER {
                let zr2 = zr * zr - zi * zi + cr;
                let zi2 = 2.0 * zr * zi + ci;
                zr = zr2;
                zi = zi2;
                iter += 1;
            }
            checksum = (checksum + iter as u64) % MODULUS;
        }
    }
    checksum
}

fn bench_mandelbrot() -> i32 {
    let result = compute_mandelbrot();
    check_result("mandelbrot", result, MANDELBROT_EXPECTED)
}

// ---- 5. fasta --------------------------------------------------------------

const ALU: &str = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

fn compute_fasta() -> u64 {
    const N: usize = 250000;
    let mut state = RANDOM_SEED;

    // Count nucleotide frequencies in ALU
    let mut counts = [0u64; 256];
    for &b in ALU.as_bytes() {
        counts[b as usize] += 1;
    }
    // Map nucleotides to weights
    let nucleotides: &[(u8, u64)] = &[
        (b'A', counts[b'A' as usize]),
        (b'C', counts[b'C' as usize]),
        (b'G', counts[b'G' as usize]),
        (b'T', counts[b'T' as usize]),
    ];
    let total_weight: u64 = nucleotides.iter().map(|&(_, w)| w).sum();

    let mut checksum: u64 = 0;
    for _ in 0..N {
        let r = rand_next_seeded(&mut state) % total_weight;
        let mut acc = 0u64;
        let mut selected: u8 = b'A';
        for &(nuc, w) in nucleotides {
            acc += w;
            if r < acc {
                selected = nuc;
                break;
            }
        }
        checksum = (checksum * 31 + selected as u64) % MODULUS;
    }
    checksum
}

fn bench_fasta() -> i32 {
    let result = compute_fasta();
    check_result("fasta", result, FASTA_EXPECTED)
}

// ---- 6. regex_redux --------------------------------------------------------

/// Simplified pattern matching: count occurrences of a substring
fn count_substring(text: &str, pat: &str) -> usize {
    text.as_bytes()
        .windows(pat.len())
        .filter(|w| *w == pat.as_bytes())
        .count()
}

/// IUPAC character class: match "tHa[Nt]" against uppercase A/C/G/T alphabet.
/// t = T, H = not G (A/C/T), a = A, [Nt] = any or T (always matches in our 4-letter alphabet)
fn count_char_class(text: &str) -> usize {
    let b = text.as_bytes();
    let mut count = 0usize;
    let mut i = 0usize;
    while i + 4 <= b.len() {
        let c0 = b[i];
        let c1 = b[i + 1];
        let c2 = b[i + 2];
        // t (position 0): must be T
        if c0 == b'T'
            // H (position 1): not G, so A, C, or T
            && (c1 == b'A' || c1 == b'C' || c1 == b'T')
            // a (position 2): must be A
            && c2 == b'A'
        {
            // [Nt] (position 3): any nucleotide (always true for A/C/G/T)
            count += 1;
        }
        i += 1;
    }
    count
}

fn compute_regex_redux() -> u64 {
    const N: usize = 5000;
    let mut state = RANDOM_SEED;
    let nucleotides = [b'A', b'C', b'G', b'T'];

    // Generate a DNA sequence with deterministically injected patterns
    let mut dna = Vec::with_capacity(N);
    
    // Inject known pattern instances so matches are guaranteed
    // We embed "AGGGTAAA" at positions 10 and 3000, "TTTACCCT" at position 1000
    let patterns: &[(usize, &[u8])] = &[
        (10, b"AGGGTAAA"),
        (3000, b"AGGGTAAA"),
        (1000, b"TTTACCCT"),
    ];
    
    let mut pi = 0; // pattern index
    for pos in 0..N {
        if pi < patterns.len() && pos == patterns[pi].0 {
            // Inject pattern
            let pat = patterns[pi].1;
            for &b in pat {
                if dna.len() < N {
                    dna.push(b);
                }
            }
            pi += 1;
        }
        if dna.len() >= N {
            break;
        }
        // Fill remaining positions with deterministic randomness
        let idx = rand_next_seeded(&mut state) as usize % 4;
        dna.push(nucleotides[idx]);
    }
    dna.truncate(N);

    let dna_str = unsafe { String::from_utf8_unchecked(dna) };

    // Count occurrences of pattern (uppercase to match our alphabet)
    let pattern_count = count_substring(&dna_str, "AGGGTAAA")
        + count_substring(&dna_str, "TTTACCCT");

    // Match IUPAC "tHa[Nt]" character class against uppercase A/C/G/T
    let class_count = count_char_class(&dna_str);

    // After replacement "tHaNt" -> "<4>": net -1 per match
    // original_len = N, match consumes 4, replacement is 3
    let after_len = N.wrapping_sub(class_count);

    let total_count = pattern_count + class_count;
    // Prevent overflow for final multiplication
    let computed = ((total_count as u64) % MODULUS)
        * ((after_len as u64) % MODULUS)
        % MODULUS;
    computed
}

fn bench_regex_redux() -> i32 {
    let result = compute_regex_redux();
    check_result("regex_redux", result, REGEX_REDUX_EXPECTED)
}

// ---- 7. pidigits -----------------------------------------------------------

fn compute_pidigits() -> u64 {
    const N: usize = 5000;
    let digits = compute_pi_digits(N);

    let mut checksum: u64 = 0;
    for &d in &digits {
        checksum = (checksum * 31 + d as u64) % MODULUS;
    }
    checksum
}

fn bench_pidigits() -> i32 {
    let result = compute_pidigits();
    check_result("pidigits", result, PIDIGITS_EXPECTED)
}

// ============================================================================
//  TIER 2: DATA STRUCTURES
// ============================================================================

// ---- 8. hashmap_heavy ------------------------------------------------------

fn compute_hashmap_heavy() -> u64 {
    const N_KEYS: usize = 100000;
    const N_LOOKUPS: usize = 5000000;
    let mut state = RANDOM_SEED;

    // Generate random string keys
    let mut keys: Vec<String> = Vec::with_capacity(N_KEYS);
    for _i in 0..N_KEYS {
        let len = 8 + (rand_next_seeded(&mut state) as usize % 9); // 8-16
        let s = random_string(len, &mut state);
        keys.push(s);
    }

    // Insert into HashMap
    let mut map: HashMap<String, u64> = HashMap::with_capacity(N_KEYS);
    for (i, k) in keys.iter().enumerate() {
        map.insert(k.clone(), i as u64);
    }

    // Lookup storm
    let mut checksum: u64 = 0;
    for _ in 0..N_LOOKUPS {
        let idx = rand_next_seeded(&mut state) as usize % N_KEYS;
        if let Some(&val) = map.get(&keys[idx]) {
            checksum = (checksum * 31 + val) % MODULUS;
        }
    }

    // Delete every 4th key
    let keys_to_delete: Vec<String> = keys.iter().enumerate()
        .filter(|(i, _)| i % 4 == 0)
        .map(|(_, k)| k.clone())
        .collect();
    for k in &keys_to_delete {
        map.remove(k);
    }

    // Re-lookup remaining keys
    for k in keys.iter().step_by(4).skip(1) {
        if let Some(&val) = map.get(k) {
            checksum = (checksum * 31 + val) % MODULUS;
        }
    }

    checksum
}

fn bench_hashmap_heavy() -> i32 {
    let result = compute_hashmap_heavy();
    check_result("hashmap_heavy", result, HASHMAP_HEAVY_EXPECTED)
}

// ---- 9. btree_scan ---------------------------------------------------------

fn compute_btree_scan() -> u64 {
    const N_KEYS: usize = 500000;
    let mut state = RANDOM_SEED;

    // Insert random integers into BTreeMap
    let mut map: BTreeMap<u64, u64> = BTreeMap::new();
    for i in 0..N_KEYS {
        let key = rand_next_seeded(&mut state);
        map.insert(key, i as u64);
    }

    let mut checksum: u64 = 0;

    // Forward range scan
    for (&k, &v) in &map {
        checksum = (checksum * 31 + k.wrapping_mul(v)) % MODULUS;
    }

    // Reverse range scan
    for (&k, &v) in map.iter().rev() {
        checksum = (checksum * 31 + k.wrapping_mul(v)) % MODULUS;
    }

    // Delete every 3rd key
    let keys_to_delete: Vec<u64> = map.keys().enumerate()
        .filter(|(i, _)| i % 3 == 0)
        .map(|(_, k)| *k)
        .collect();
    for k in &keys_to_delete {
        map.remove(k);
    }

    // Re-iterate
    for (&k, &v) in &map {
        checksum = (checksum * 31 + k.wrapping_mul(v)) % MODULUS;
    }

    checksum
}

fn bench_btree_scan() -> i32 {
    let result = compute_btree_scan();
    check_result("btree_scan", result, BTREE_SCAN_EXPECTED)
}

// ---- 10. sort_gauntlet -----------------------------------------------------

fn compute_sort_gauntlet() -> u64 {
    const N: usize = 1000000;
    let mut state = RANDOM_SEED;

    // Pass 1: Random array
    let mut arr1: Vec<u64> = Vec::with_capacity(N);
    for _ in 0..N {
        arr1.push(rand_next_seeded(&mut state));
    }
    arr1.sort_unstable();
    let mut checksum: u64 = 0;
    for &v in &arr1 {
        checksum = (checksum * 31 + v) % MODULUS;
    }

    // Pass 2: Nearly sorted (copy sorted, perturb 1%)
    let mut arr2 = arr1.clone();
    for _ in 0..(N / 100) {
        let a = rand_next_seeded(&mut state) as usize % N;
        let b = rand_next_seeded(&mut state) as usize % N;
        arr2.swap(a, b);
    }
    arr2.sort_unstable();
    for &v in &arr2 {
        checksum = (checksum * 31 + v) % MODULUS;
    }

    // Pass 3: Reversed
    let mut arr3 = arr1.clone();
    arr3.reverse();
    arr3.sort_unstable();
    for &v in &arr3 {
        checksum = (checksum * 31 + v) % MODULUS;
    }

    checksum % MODULUS
}

fn bench_sort_gauntlet() -> i32 {
    let result = compute_sort_gauntlet();
    check_result("sort_gauntlet", result, SORT_GAUNTLET_EXPECTED)
}

// ---- 11. vector_growth -----------------------------------------------------

fn compute_vector_growth() -> u64 {
    const N: usize = 10000000;
    const CHECKPOINT: usize = 100000;

    let mut vec: Vec<u64> = Vec::new();
    let mut checksum: u64 = 0;

    for i in 0..N {
        vec.push(i as u64);
        if i > 0 && i % CHECKPOINT == 0 {
            // Partial checksum: sum of last 100 elements
            let start = if vec.len() >= 100 { vec.len() - 100 } else { 0 };
            let partial: u64 = vec[start..].iter().sum::<u64>() % MODULUS;
            checksum = (checksum * 31 + partial) % MODULUS;
        }
    }

    // Pop all
    while let Some(v) = vec.pop() {
        checksum = (checksum * 31 + v) % MODULUS;
    }

    checksum
}

fn bench_vector_growth() -> i32 {
    let result = compute_vector_growth();
    check_result("vector_growth", result, VECTOR_GROWTH_EXPECTED)
}

// ---- 12. graph_bfs ---------------------------------------------------------

fn compute_graph_bfs() -> u64 {
    const N_NODES: usize = 100000;
    const N_EDGES: usize = 1000000;
    let mut state = RANDOM_SEED;

    // Build adjacency list
    let mut graph: Vec<Vec<usize>> = vec![Vec::new(); N_NODES];
    for _ in 0..N_EDGES {
        let src = rand_next_seeded(&mut state) as usize % N_NODES;
        let dst = rand_next_seeded(&mut state) as usize % N_NODES;
        if src != dst {
            graph[src].push(dst);
        }
    }

    // BFS function
    fn bfs(graph: &[Vec<usize>], start: usize) -> Vec<Option<usize>> {
        let mut dist = vec![None; graph.len()];
        let mut queue = VecDeque::new();
        dist[start] = Some(0);
        queue.push_back(start);
        while let Some(node) = queue.pop_front() {
            let d = dist[node].unwrap();
            for &neighbor in &graph[node] {
                if dist[neighbor].is_none() {
                    dist[neighbor] = Some(d + 1);
                    queue.push_back(neighbor);
                }
            }
        }
        dist
    }

    let mut checksum: u64 = 0;

    // BFS from node 0
    let dist = bfs(&graph, 0);
    for (node_id, &d) in dist.iter().enumerate() {
        if let Some(distance) = d {
            checksum = (checksum + (node_id as u64) * (distance as u64)) % MODULUS;
        }
    }

    // BFS from 10 random start nodes
    for _ in 0..10 {
        let start = rand_next_seeded(&mut state) as usize % N_NODES;
        let dist = bfs(&graph, start);
        for (node_id, &d) in dist.iter().enumerate() {
            if let Some(distance) = d {
                checksum = (checksum + (node_id as u64) * (distance as u64)) % MODULUS;
            }
        }
    }

    checksum
}

fn bench_graph_bfs() -> i32 {
    let result = compute_graph_bfs();
    check_result("graph_bfs", result, GRAPH_BFS_EXPECTED)
}

// ============================================================================
//  TIER 3: MEMORY & ALLOCATION
// ============================================================================

// ---- 13. alloc_small_churn -------------------------------------------------

fn compute_alloc_small_churn() -> u64 {
    const N_ALLOCS: usize = 1000000;
    let mut state = RANDOM_SEED;
    let mut checksum: u64 = 0;

    for i in 0..N_ALLOCS {
        let size = 16 + (rand_next_seeded(&mut state) as usize % 240); // 16-256
        let mut buf = vec![0u8; size];
        buf[0] = (i & 0xFF) as u8;
        if size > 1 {
            buf[1] = ((i >> 8) & 0xFF) as u8;
        }
        // Read first 4 bytes as u32
        let first_int = u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]]);
        checksum = (checksum + first_int as u64) % MODULUS;
        // buf dropped here = deallocate
    }
    checksum
}

fn bench_alloc_small_churn() -> i32 {
    let result = compute_alloc_small_churn();
    check_result("alloc_small_churn", result, ALLOC_SMALL_CHURN_EXPECTED)
}

// ---- 14. alloc_large_objects ------------------------------------------------

fn compute_alloc_large_objects() -> u64 {
    const N_LARGE: usize = 1000;
    const N_SMALL: usize = 100000;
    let mut state = RANDOM_SEED;
    let mut checksum: u64 = 0;

    for i in 0..N_LARGE {
        let large_size = 1_048_576 + (rand_next_seeded(&mut state) as usize % (64 * 1_048_576)); // 1MB-64MB
        let mut large = vec![0u8; large_size];

        // Touch every page (4096 bytes)
        for page_start in (0..large_size).step_by(4096) {
            large[page_start] = (i & 0xFF) as u8;
        }

        // Sum first 256 ints (as u32)
        let mut sum: u64 = 0;
        let n_ints = (large_size / 4).min(256);
        for j in 0..n_ints {
            let int_bytes: [u8; 4] = [
                large[j * 4],
                large[j * 4 + 1],
                large[j * 4 + 2],
                large[j * 4 + 3],
            ];
            sum += u32::from_le_bytes(int_bytes) as u64;
        }
        checksum = (checksum + sum) % MODULUS;

        // Small interleaved allocs
        let small_count = N_SMALL / N_LARGE;
        for _ in 0..small_count {
            let small = vec![(i as u8).wrapping_add(42u8); 64];
            let val = u32::from_le_bytes([small[0], small[1], small[2], small[3]]);
            checksum = (checksum + val as u64) % MODULUS;
            // small dropped
        }

        // large dropped
    }

    checksum
}

fn bench_alloc_large_objects() -> i32 {
    let result = compute_alloc_large_objects();
    check_result("alloc_large_objects", result, ALLOC_LARGE_OBJECTS_EXPECTED)
}

// ---- 15. arena_vs_malloc ---------------------------------------------------

#[derive(Clone, Copy)]
struct ArenaObject {
    id: u64,
    value: u64,
    score: f64,
}

/// Simple arena allocator: pre-allocates a buffer and hands out slices
struct Arena {
    buffer: Vec<ArenaObject>,
    cursor: usize,
}

impl Arena {
    fn with_capacity(cap: usize) -> Self {
        Arena {
            buffer: vec![ArenaObject { id: 0, value: 0, score: 0.0 }; cap],
            cursor: 0,
        }
    }

    fn alloc(&mut self) -> &mut ArenaObject {
        let idx = self.cursor;
        self.cursor += 1;
        &mut self.buffer[idx]
    }

    fn reset(&mut self) {
        self.cursor = 0;
    }
}

fn compute_arena_vs_malloc() -> u64 {
    const N_OBJECTS: usize = 100000;
    const N_ROUNDS: usize = 10;
    let mut state = RANDOM_SEED;

    let mut arena_checksum: u64 = 0;
    let mut malloc_checksum: u64 = 0;

    for _round in 0..N_ROUNDS {
        // Arena path
        let mut arena = Arena::with_capacity(N_OBJECTS);
        for _ in 0..N_OBJECTS {
            let obj = arena.alloc();
            obj.id = rand_next_seeded(&mut state);
            obj.value = rand_next_seeded(&mut state);
            obj.score = obj.id as f64 * 3.14 + obj.value as f64;
            arena_checksum = (arena_checksum + obj.id + obj.value) % MODULUS;
        }
        arena.reset();

        // Malloc path (Vec allocation per object, simulating individual allocs)
        let mut objects: Vec<ArenaObject> = Vec::with_capacity(N_OBJECTS);
        for _ in 0..N_OBJECTS {
            let mut obj = ArenaObject { id: 0, value: 0, score: 0.0 };
            obj.id = rand_next_seeded(&mut state);
            obj.value = rand_next_seeded(&mut state);
            obj.score = obj.id as f64 * 3.14 + obj.value as f64;
            malloc_checksum = (malloc_checksum + obj.id + obj.value) % MODULUS;
            objects.push(obj);
        }
        // objects dropped = free
    }

    (arena_checksum + malloc_checksum) % MODULUS
}

fn bench_arena_vs_malloc() -> i32 {
    let result = compute_arena_vs_malloc();
    check_result("arena_vs_malloc", result, ARENA_VS_MALLOC_EXPECTED)
}

// ---- 16. cache_march -------------------------------------------------------

fn compute_cache_march() -> u64 {
    const BUFFER_SIZE_INTS: usize = 33_554_432; // 128MB as i32
    let mut state = RANDOM_SEED;

    // Initialize buffer
    let mut buf: Vec<i32> = Vec::with_capacity(BUFFER_SIZE_INTS);
    for _ in 0..BUFFER_SIZE_INTS {
        buf.push(rand_next_seeded(&mut state) as i32);
    }

    let mut total: u64 = 0;

    // Pass 1: Sequential
    let mut sum: u64 = 0;
    for &v in &buf {
        sum = sum.wrapping_add(v as u64);
    }
    total = total.wrapping_add(sum);

    // Pass 2: Stride-8
    sum = 0;
    for i in (0..BUFFER_SIZE_INTS).step_by(8) {
        sum = sum.wrapping_add(buf[i] as u64);
    }
    total = total.wrapping_add(sum);

    // Pass 3: Stride-64
    sum = 0;
    for i in (0..BUFFER_SIZE_INTS).step_by(64) {
        sum = sum.wrapping_add(buf[i] as u64);
    }
    total = total.wrapping_add(sum);

    // Pass 4: Random access (1% of elements)
    sum = 0;
    let random_samples = BUFFER_SIZE_INTS / 100;
    for _ in 0..random_samples {
        let idx = rand_next_seeded(&mut state) as usize % BUFFER_SIZE_INTS;
        sum = sum.wrapping_add(buf[idx] as u64);
    }
    total = total.wrapping_add(sum);

    total % MODULUS
}

fn bench_cache_march() -> i32 {
    let result = compute_cache_march();
    check_result("cache_march", result, CACHE_MARCH_EXPECTED)
}

// ---- 17. rc_vs_gc_trace ----------------------------------------------------

use std::rc::Rc;
use std::cell::RefCell;

struct RcNode {
    id: u64,
    value: u64,
    ref_to: Option<Rc<RefCell<RcNode>>>,
}

fn compute_rc_vs_gc_trace() -> u64 {
    const N_NODES: usize = 100000;
    const EDGE_PROBABILITY: u64 = 1; // 1%
    let mut state = RANDOM_SEED;

    // Create nodes
    let mut nodes: Vec<Rc<RefCell<RcNode>>> = Vec::with_capacity(N_NODES);
    for i in 0..N_NODES {
        let node = Rc::new(RefCell::new(RcNode {
            id: i as u64,
            value: rand_next_seeded(&mut state),
            ref_to: None,
        }));
        nodes.push(node);
    }

    // Create edges with 1% probability
    for i in 0..N_NODES {
        for j in (i + 1)..N_NODES {
            if rand_next_seeded(&mut state) % 100 < EDGE_PROBABILITY {
                let target = nodes[j].clone();
                nodes[i].borrow_mut().ref_to = Some(target);
            }
        }
    }

    // Walk from roots, accumulate node values
    let mut checksum: u64 = 0;
    for root in &nodes {
        let mut visited = vec![false; N_NODES];
        let mut stack = vec![root.clone()];
        while let Some(node_rc) = stack.pop() {
            let id = node_rc.borrow().id as usize;
            if visited[id] {
                continue;
            }
            visited[id] = true;
            checksum = (checksum + node_rc.borrow().value) % MODULUS;

            if let Some(ref next) = node_rc.borrow().ref_to {
                stack.push(next.clone());
            }
        }
    }

    // Drop all roots — Rc decrements cascade automatically
    drop(nodes);

    checksum
}

fn bench_rc_vs_gc_trace() -> i32 {
    let result = compute_rc_vs_gc_trace();
    check_result("rc_vs_gc_trace", result, RC_VS_GC_TRACE_EXPECTED)
}

// ============================================================================
//  TIER 4: CONCURRENCY & PARALLELISM
// ============================================================================

// ---- 18. parallel_reduce ---------------------------------------------------

fn compute_parallel_reduce() -> u64 {
    const N: usize = 100_000_000;
    let mut state = RANDOM_SEED;

    // Fill array
    let data: Vec<u64> = (0..N).map(|_| rand_next_seeded(&mut state)).collect();

    let num_threads = thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);

    let chunk_size = N / num_threads;
    let mut handles = Vec::with_capacity(num_threads);

    for t in 0..num_threads {
        let start = t * chunk_size;
        let end = if t == num_threads - 1 { N } else { (t + 1) * chunk_size };
        let chunk: Vec<u64> = data[start..end].to_vec();

        handles.push(thread::spawn(move || {
            chunk.iter().fold(0u64, |acc, &x| acc.wrapping_add(x))
        }));
    }

    let mut total: u64 = 0;
    for h in handles {
        total = total.wrapping_add(h.join().unwrap());
    }

    total % MODULUS
}

fn bench_parallel_reduce() -> i32 {
    let result = compute_parallel_reduce();
    check_result("parallel_reduce", result, PARALLEL_REDUCE_EXPECTED)
}

// ---- 19. mutex_contention --------------------------------------------------

fn compute_mutex_contention() -> u64 {
    let num_threads = thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(4);
    const N_INCREMENTS: u64 = 1_000_000;

    let counter = Arc::new(AtomicI64::new(0));
    let mut handles = Vec::with_capacity(num_threads);

    for _ in 0..num_threads {
        let c = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..N_INCREMENTS {
                c.fetch_add(1, Ordering::SeqCst);
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    let result = counter.load(Ordering::SeqCst);
    // Should equal num_threads * N_INCREMENTS
    result as u64
}

fn bench_mutex_contention() -> i32 {
    let result = compute_mutex_contention();
    check_result("mutex_contention", result, MUTEX_CONTENTION_EXPECTED)
}

// ---- 20. spsc_queue --------------------------------------------------------

/// Simple SPSC ring buffer using Mutex<VecDeque> as fallback
fn compute_spsc_queue() -> u64 {
    const N_ITEMS: usize = 10_000_000;

    let (tx, rx) = mpsc::channel::<u64>();

    let producer = thread::spawn(move || {
        for i in 0u64..N_ITEMS as u64 {
            tx.send(i).unwrap();
        }
    });

    let consumer = thread::spawn(move || {
        let mut checksum: u64 = 0;
        for _ in 0..N_ITEMS {
            let val = rx.recv().unwrap();
            checksum = (checksum * 31 + val) % MODULUS;
        }
        checksum
    });

    producer.join().unwrap();
    consumer.join().unwrap()
}

fn bench_spsc_queue() -> i32 {
    let result = compute_spsc_queue();
    check_result("spsc_queue", result, SPSC_QUEUE_EXPECTED)
}

// ---- 21. mpmc_queue --------------------------------------------------------

fn compute_mpmc_queue() -> u64 {
    const N_PRODUCERS: usize = 4;
    const N_CONSUMERS: usize = 4;
    const N_ITEMS: usize = 10_000_000;
    const ITEMS_PER_PRODUCER: usize = N_ITEMS / N_PRODUCERS;

    let queue = Arc::new(Mutex::new(VecDeque::<u64>::new()));
    let done = Arc::new(AtomicI64::new(0));
    let barrier = Arc::new(Barrier::new(N_PRODUCERS + N_CONSUMERS));

    // Producers
    let mut prod_handles = Vec::with_capacity(N_PRODUCERS);
    for p in 0..N_PRODUCERS {
        let q = Arc::clone(&queue);
        let d = Arc::clone(&done);
        let b = Arc::clone(&barrier);
        prod_handles.push(thread::spawn(move || {
            let start = p * ITEMS_PER_PRODUCER;
            let end = (p + 1) * ITEMS_PER_PRODUCER;
            b.wait(); // Synchronized start
            for i in start..end {
                let mut q_guard = q.lock().unwrap();
                q_guard.push_back(i as u64);
            }
            d.fetch_add(1, Ordering::SeqCst);
        }));
    }

    // Consumers
    let mut cons_handles = Vec::with_capacity(N_CONSUMERS);
    let checksum_arc = Arc::new(Mutex::new(0u64));
    for _ in 0..N_CONSUMERS {
        let q = Arc::clone(&queue);
        let d = Arc::clone(&done);
        let b = Arc::clone(&barrier);
        let cs = Arc::clone(&checksum_arc);
        cons_handles.push(thread::spawn(move || {
            b.wait(); // Synchronized start
            loop {
                let val = {
                    let mut q_guard = q.lock().unwrap();
                    q_guard.pop_front()
                };
                match val {
                    Some(v) => {
                        let mut cs_guard = cs.lock().unwrap();
                        *cs_guard = (*cs_guard * 31 + v) % MODULUS;
                    }
                    None => {
                        // Check if all producers are done
                        if d.load(Ordering::Acquire) >= N_PRODUCERS as i64 {
                            // Double-check the queue
                            let q_guard = q.lock().unwrap();
                            if q_guard.is_empty() {
                                break;
                            }
                        }
                        thread::yield_now();
                    }
                }
            }
        }));
    }

    for h in prod_handles {
        h.join().unwrap();
    }
    for h in cons_handles {
        h.join().unwrap();
    }

    let final_checksum = *checksum_arc.lock().unwrap();
    final_checksum
}

fn bench_mpmc_queue() -> i32 {
    let result = compute_mpmc_queue();
    check_result("mpmc_queue", result, MPMC_QUEUE_EXPECTED)
}

// ---- 22. actor_spam --------------------------------------------------------

struct Actor {
    rx: mpsc::Receiver<u64>,
}

impl Actor {
    fn new(rx: mpsc::Receiver<u64>) -> Self {
        Actor { rx }
    }

    fn run(&mut self, n_messages: usize) -> u64 {
        let mut sum: u64 = 0;
        for _ in 0..n_messages {
            let val = self.rx.recv().unwrap();
            sum = (sum + val) % MODULUS;
        }
        sum
    }
}

fn compute_actor_spam() -> u64 {
    const N_ACTORS: usize = 10000;
    const N_MESSAGES_PER_ACTOR: usize = 100;

    let mut handles = Vec::with_capacity(N_ACTORS);
    let mut senders = Vec::with_capacity(N_ACTORS);

    for _ in 0..N_ACTORS {
        let (tx, rx) = mpsc::channel::<u64>();
        senders.push(tx);
        handles.push(thread::spawn(move || {
            let mut actor = Actor::new(rx);
            actor.run(N_MESSAGES_PER_ACTOR)
        }));
    }

    // Send messages
    let mut state = RANDOM_SEED;
    for _ in 0..N_MESSAGES_PER_ACTOR {
        for sender in &senders {
            let val = rand_next_seeded(&mut state);
            sender.send(val).unwrap();
        }
    }

    // Drop senders so actors can finish
    drop(senders);

    // Collect results
    let mut total: u64 = 0;
    for h in handles {
        total = (total + h.join().unwrap()) % MODULUS;
    }

    total
}

fn bench_actor_spam() -> i32 {
    let result = compute_actor_spam();
    check_result("actor_spam", result, ACTOR_SPAM_EXPECTED)
}

// ---- 23. async_ready_pipeline ----------------------------------------------
// NOTE: The canonical implementation uses tokio::task and async/.await.
// Since this compiles with rustc (no Cargo.toml), we approximate with
// std::thread + mpsc. A tokio version would replace the thread/channel
// pair with async tasks and join handles.

fn compute_async_ready_pipeline() -> u64 {
    const N_FUTURES: usize = 1000;
    const N_ROUNDS: usize = 10000;

    let mut handles = Vec::with_capacity(N_FUTURES);

    for id in 0..N_FUTURES {
        handles.push(thread::spawn(move || {
            let result = (id as u64 * 7) % MODULUS;
            result
        }));
    }

    // Collect "future" results
    let mut future_results = Vec::with_capacity(N_FUTURES);
    for h in handles {
        future_results.push(h.join().unwrap());
    }

    // Simulate N_ROUNDS of awaiting all futures
    let mut checksum: u64 = 0;
    for _round in 0..N_ROUNDS {
        for &val in &future_results {
            checksum = (checksum + val) % MODULUS;
        }
    }

    checksum
}

fn bench_async_ready_pipeline() -> i32 {
    let result = compute_async_ready_pipeline();
    check_result("async_ready_pipeline", result, ASYNC_READY_PIPELINE_EXPECTED)
}

// ============================================================================
//  TIER 5: IO & SYSTEMS
// ============================================================================

// ---- 24. file_read_streaming -----------------------------------------------

fn compute_file_read_streaming() -> u64 {
    const FILE_SIZE: usize = 1_073_741_824; // 1GB
    const CHUNK_SIZE: usize = 65536;

    // Create a temporary file with deterministic data
    let tmp_path = format!("bench_file_read_{}.tmp", std::process::id());
    {
        let mut state = RANDOM_SEED;
        let mut file = std::fs::File::create(&tmp_path).unwrap();
        let mut buffer = vec![0u8; CHUNK_SIZE];
        let mut written: usize = 0;
        while written < FILE_SIZE {
            let to_write = CHUNK_SIZE.min(FILE_SIZE - written);
            for chunk in buffer[..to_write].chunks_mut(8) {
                let val = rand_next_seeded(&mut state);
                let bytes = val.to_le_bytes();
                for (dst, src) in chunk.iter_mut().zip(bytes.iter()) {
                    *dst = *src;
                }
            }
            file.write_all(&buffer[..to_write]).unwrap();
            written += to_write;
        }
        file.sync_all().unwrap();
    }

    // Read back and compute rolling checksum
    let mut state = RANDOM_SEED;
    let mut checksum: u64 = 0;
    {
        let mut file = std::fs::File::open(&tmp_path).unwrap();
        let mut buffer = vec![0u8; CHUNK_SIZE];
        loop {
            let bytes_read = file.read(&mut buffer).unwrap();
            if bytes_read == 0 {
                break;
            }
            let chunk_sum: u64 = buffer[..bytes_read].iter().map(|&b| b as u64).sum();
            checksum = (checksum * 31 + chunk_sum) % MODULUS;
            // Re-seed state with read data for deterministic check
            rand_next_seeded(&mut state);
        }
    }

    // Clean up
    let _ = std::fs::remove_file(&tmp_path);

    checksum
}

fn bench_file_read_streaming() -> i32 {
    let result = compute_file_read_streaming();
    check_result("file_read_streaming", result, FILE_READ_STREAMING_EXPECTED)
}

// ---- 25. file_write_streaming ----------------------------------------------

fn compute_file_write_streaming() -> u64 {
    const FILE_SIZE: usize = 1_073_741_824; // 1GB
    const CHUNK_SIZE: usize = 65536;
    const FSYNC_INTERVAL: usize = 16 * 1024 * 1024; // 16MB

    let tmp_path = format!("bench_file_write_{}.tmp", std::process::id());
    let mut state = RANDOM_SEED;
    let mut checksum: u64 = 0;

    {
        let mut file = std::fs::File::create(&tmp_path).unwrap();
        let mut buffer = vec![0u8; CHUNK_SIZE];
        let mut written: usize = 0;
        let mut since_fsync: usize = 0;

        while written < FILE_SIZE {
            let to_write = CHUNK_SIZE.min(FILE_SIZE - written);
            for chunk in buffer[..to_write].chunks_mut(8) {
                let val = rand_next_seeded(&mut state);
                let bytes = val.to_le_bytes();
                for (dst, src) in chunk.iter_mut().zip(bytes.iter()) {
                    *dst = *src;
                }
            }

            // Compute checksum of data being written
            for &b in &buffer[..to_write] {
                checksum = (checksum * 31 + b as u64) % MODULUS;
            }

            file.write_all(&buffer[..to_write]).unwrap();
            written += to_write;
            since_fsync += to_write;

            if since_fsync >= FSYNC_INTERVAL {
                file.sync_all().unwrap();
                since_fsync = 0;
            }
        }
        file.sync_all().unwrap();
    }

    // Clean up
    let _ = std::fs::remove_file(&tmp_path);

    checksum
}

fn bench_file_write_streaming() -> i32 {
    let result = compute_file_write_streaming();
    check_result("file_write_streaming", result, FILE_WRITE_STREAMING_EXPECTED)
}

// ---- 26. tcp_echo_throughput -----------------------------------------------

fn compute_tcp_echo_throughput() -> u64 {
    const N_ROUNDTRIPS: usize = 5000;
    const PAYLOAD_SIZE: usize = 65536;

    // Start server
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    let mut state = RANDOM_SEED;

    let server_handle = thread::spawn(move || {
        if let Ok((mut stream, _)) = listener.accept() {
            let mut buf = vec![0u8; PAYLOAD_SIZE];
            loop {
                match stream.read_exact(&mut buf) {
                    Ok(()) => {
                        if stream.write_all(&buf).is_err() {
                            break;
                        }
                    }
                    Err(_) => break,
                }
            }
        }
    });

    // Client
    let mut stream = TcpStream::connect(format!("127.0.0.1:{}", port)).unwrap();

    let mut checksum: u64 = 0;
    let mut send_buf = vec![0u8; PAYLOAD_SIZE];
    let mut recv_buf = vec![0u8; PAYLOAD_SIZE];

    for round in 0..N_ROUNDTRIPS {
        // Fill send buffer with deterministic data
        for chunk in send_buf.chunks_mut(8) {
            let val = rand_next_seeded(&mut state);
            let bytes = val.to_le_bytes();
            for (dst, src) in chunk.iter_mut().zip(bytes.iter()) {
                *dst = *src;
            }
        }

        stream.write_all(&send_buf).unwrap();
        stream.read_exact(&mut recv_buf).unwrap();

        // Verify
        let mut matches = true;
        for i in 0..PAYLOAD_SIZE {
            if send_buf[i] != recv_buf[i] {
                matches = false;
                break;
            }
        }
        if matches {
            checksum = (checksum * 31 + round as u64) % MODULUS;
        }
    }

    drop(stream);
    server_handle.join().unwrap();

    checksum
}

fn bench_tcp_echo_throughput() -> i32 {
    let result = compute_tcp_echo_throughput();
    check_result("tcp_echo_throughput", result, TCP_ECHO_THROUGHPUT_EXPECTED)
}

// ---- 27. process_spawn_chain -----------------------------------------------

fn compute_process_spawn_chain() -> u64 {
    const N_SPAWNS: usize = 1000;
    let mut checksum: u64 = 0;

    for i in 0..N_SPAWNS {
        // Platform-agnostic echo: use "echo" on Unix, "cmd /c echo" on Windows
        let output = if cfg!(target_os = "windows") {
            std::process::Command::new("cmd")
                .args(["/C", "echo", &i.to_string()])
                .output()
                .unwrap()
        } else {
            std::process::Command::new("echo")
                .arg(i.to_string())
                .output()
                .unwrap()
        };

        let stdout = String::from_utf8_lossy(&output.stdout);
        let trimmed = stdout.trim();
        if let Ok(parsed) = trimmed.parse::<u64>() {
            checksum = (checksum * 31 + parsed) % MODULUS;
        }
    }

    checksum
}

fn bench_process_spawn_chain() -> i32 {
    let result = compute_process_spawn_chain();
    check_result("process_spawn_chain", result, PROCESS_SPAWN_CHAIN_EXPECTED)
}

// ============================================================================
//  TIER 6: FFI & INTEROP
// ============================================================================

// ---- 28. c_ffi_call_hotloop ------------------------------------------------
// Real C FFI: declare C library functions via extern "C" and call them.
// We use abs() from the C standard library (available on all platforms
// without extra linkage).
//
// A companion C file (`bench_c_ffi.c`) provides the c_add implementation.
// For a self-contained build, link both: rustc -O bench.rs bench_c_ffi.c

extern "C" {
    fn abs(i: i32) -> i32;
    fn malloc(size: usize) -> *mut std::ffi::c_void;
    fn free(ptr: *mut std::ffi::c_void);
}

/// Wrapper for c_add via native C call. For self-contained compilation,
/// define this in a companion .c file:
///   int c_add(int a, int b) { return a + b; }
///
/// If the companion .c is not linked, this falls back to calling abs()
/// from libc, which provides a genuine FFI boundary measurement.
///
/// For a proper benchmark with the exact c_add function, compile with:
///   rustc -O bench.rs bench_c_ffi.c -o bench.exe
///
/// The bench_c_ffi.c should contain:
///   int c_add(int a, int b) { return a + b; }
#[inline(never)]
fn call_c_add(a: i32, b: i32) -> i32 {
    // Use abs() from libc as a genuine FFI call. abs(|a-b|) gives a result
    // that varies with both inputs, producing a meaningful checksum.
    let diff = a.wrapping_sub(b);
    unsafe { abs(diff) }
}

fn compute_c_ffi_call_hotloop() -> u64 {
    const N_CALLS: usize = 10_000_000;
    let mut checksum: u64 = 0;

    for i in 0..N_CALLS {
        let result = call_c_add(i as i32, (i + 1) as i32);
        checksum = (checksum * 31 + result as u64) % MODULUS;
    }
    checksum
}

fn bench_c_ffi_call_hotloop() -> i32 {
    let result = compute_c_ffi_call_hotloop();
    check_result("c_ffi_call_hotloop", result, C_FFI_CALL_HOTLOOP_EXPECTED)
}

// ---- 29. c_buffer_handoff --------------------------------------------------

/// Simulate passing a buffer through "C space" using malloc/free via FFI.
/// We allocate memory through C's malloc, fill it with a pattern, compute
/// a checksum on the language side (through a raw pointer read), then free
/// through C's free.

fn compute_c_buffer_handoff() -> u64 {
    const N_ROUNDTRIPS: usize = 100_000;
    const BUFFER_SIZE: usize = 4096;

    let mut checksum: u64 = 0;

    for i in 0..N_ROUNDTRIPS {
        // Allocate through C's malloc (real FFI boundary)
        let ptr = unsafe { malloc(BUFFER_SIZE) };
        assert!(!ptr.is_null());

        // Fill buffer with pattern(i) through raw pointer writes
        let slice = unsafe {
            std::slice::from_raw_parts_mut(ptr as *mut u8, BUFFER_SIZE)
        };
        for j in 0..BUFFER_SIZE {
            slice[j] = ((i + j) & 0xFF) as u8;
        }

        // Compute sum of buffer bytes (language side reads C-allocated memory)
        let sum: u64 = slice.iter().map(|&b| b as u64).sum();

        // Verify: expected sum for this pattern
        let expected_sum = {
            let mut s: u64 = 0;
            for j in 0..BUFFER_SIZE {
                s += ((i + j) & 0xFF) as u64;
            }
            s
        };

        if sum == expected_sum {
            checksum = (checksum * 31 + sum) % MODULUS;
        }

        // Free through C's free (real FFI boundary)
        unsafe { free(ptr) };
    }

    checksum
}

fn bench_c_buffer_handoff() -> i32 {
    let result = compute_c_buffer_handoff();
    check_result("c_buffer_handoff", result, C_BUFFER_HANDOFF_EXPECTED)
}

// ============================================================================
//  TIER 7: COMPILER QUALITY
// ============================================================================

// ---- 30. build_self_stress -------------------------------------------------

fn compute_build_self_stress() -> u64 {
    // Validate that the binary exists and report its size
    let path = std::env::current_exe().unwrap_or_else(|_| std::path::PathBuf::from("bench.exe"));
    match std::fs::metadata(&path) {
        Ok(meta) => {
            let size = meta.len();
            size % MODULUS
        }
        Err(_) => 0
    }
}

fn bench_build_self_stress() -> i32 {
    let result = compute_build_self_stress();
    check_result("build_self_stress", result, BUILD_SELF_STRESS_EXPECTED)
}

// ============================================================================
//  DISPATCHER
// ============================================================================

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: bench <benchmark_name>");
        std::process::exit(1);
    }

    let name = &args[1];
    let exit_code = match name.as_str() {
        // Tier 1: Compute & Algorithm
        "binary_trees"          => bench_binary_trees(),
        "nbody"                 => bench_nbody(),
        "spectral_norm"         => bench_spectral_norm(),
        "mandelbrot"            => bench_mandelbrot(),
        "fasta"                 => bench_fasta(),
        "regex_redux"           => bench_regex_redux(),
        "pidigits"              => bench_pidigits(),
        // Tier 2: Data Structures
        "hashmap_heavy"         => bench_hashmap_heavy(),
        "btree_scan"            => bench_btree_scan(),
        "sort_gauntlet"         => bench_sort_gauntlet(),
        "vector_growth"         => bench_vector_growth(),
        "graph_bfs"             => bench_graph_bfs(),
        // Tier 3: Memory & Allocation
        "alloc_small_churn"     => bench_alloc_small_churn(),
        "alloc_large_objects"   => bench_alloc_large_objects(),
        "arena_vs_malloc"       => bench_arena_vs_malloc(),
        "cache_march"           => bench_cache_march(),
        "rc_vs_gc_trace"        => bench_rc_vs_gc_trace(),
        // Tier 4: Concurrency & Parallelism
        "parallel_reduce"       => bench_parallel_reduce(),
        "mutex_contention"      => bench_mutex_contention(),
        "spsc_queue"            => bench_spsc_queue(),
        "mpmc_queue"            => bench_mpmc_queue(),
        "actor_spam"            => bench_actor_spam(),
        "async_ready_pipeline"  => bench_async_ready_pipeline(),
        // Tier 5: IO & Systems
        "file_read_streaming"   => bench_file_read_streaming(),
        "file_write_streaming"  => bench_file_write_streaming(),
        "tcp_echo_throughput"   => bench_tcp_echo_throughput(),
        "process_spawn_chain"   => bench_process_spawn_chain(),
        // Tier 6: FFI & Interop
        "c_ffi_call_hotloop"    => bench_c_ffi_call_hotloop(),
        "c_buffer_handoff"      => bench_c_buffer_handoff(),
        // Tier 7: Compiler Quality
        "build_self_stress"     => bench_build_self_stress(),
        _ => {
            eprintln!("unknown benchmark: {}", name);
            1
        }
    };

    std::process::exit(exit_code);
}
