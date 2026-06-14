# V3 Benchmarks — MarkScript Edition

> V3 benchmark suite for the CASES_V3 pipeline, implemented in the MarkScript mini-language.
> MarkScript is a markdown-native bytecode VM for Kain — your documentation IS your program.
>
> Each `##` section is a standalone benchmark that computes a deterministic integer result,
> prints a `CHECKSUM:` line for the runner, and asserts `PASS` or `FAIL`.
> Use `mks run bench.md` to execute all sections sequentially.
>
> Limitations: No heap, no floats, no concurrency, no FFI, no IO, no functions.
> Strengths: Pure compute, control flow, string ops, recurrence, fixed-point arithmetic, branch stress.
>
> **Run:** `mks run bench.md`
> **Grep:** `mks run bench.md | findstr "CHECKSUM"` to extract all checksums.
>
> All benchmarks use the V3 shared constants:
> - MODULUS = 1000000007 (prime modulus for checksum accumulation)
> - LCG: state = (state * 1103515245 + 12345) & 0x7FFFFFFF
> - RANDOM_SEED = 42

---

## scalar_mix

> LCG modulo accumulation. Classic scalar hot loop.
> 100,000 iterations: deterministic LCG step + weighted checksum fold.
> Measures raw arithmetic throughput in the VM.

```markscript
let MODULUS = 1000000007
let state = 42
let checksum = 0
let i = 0
while i < 100000:
    state = (state * 1103515245 + 12345) & 2147483647
    checksum = (checksum + state) % MODULUS
    i = i + 1
print("scalar_mix CHECKSUM: " + str(checksum))
print("PASS")
```

---

## recursive_sum

> Triangular number via while loop. Verifies result against the closed-form
> formula n*(n+1)/2 at the end. N=50000, sum should equal 1,250,025,000.
> Asserts FAIL if the computed sum does not match the formula.

```markscript
let MODULUS = 1000000007
let N = 50000
let sum = 0
let i = 1
while i <= N:
    sum = sum + i
    i = i + 1
let expected = N * (N + 1) / 2
let checksum = sum % MODULUS
print("recursive_sum CHECKSUM: " + str(checksum))
if sum == expected:
    print("PASS")
else:
    print("FAIL")
```

---

## branch_dispatch

> 8-lane if/elif dispatch chain driven by LCG output modulo 8. Each lane
> applies a different arithmetic folding operation:
>   0: add      1: multiply-31   2: triple-add   3: half-add
>   4: mix-7    5: square-add    6: quarter-add  7: add-one
> 50,000 iterations. Exercises the VM's multi-way branch handling.

```markscript
let MODULUS = 1000000007
let state = 42
let checksum = 0
let i = 0
while i < 50000:
    state = (state * 1103515245 + 12345) & 2147483647
    let lane = state % 8
    if lane == 0:
        checksum = (checksum + state) % MODULUS
    elif lane == 1:
        checksum = (checksum * 31 + state) % MODULUS
    elif lane == 2:
        checksum = (checksum + state * 3) % MODULUS
    elif lane == 3:
        checksum = (checksum + state / 2) % MODULUS
    elif lane == 4:
        checksum = (checksum * 7 + state % 100) % MODULUS
    elif lane == 5:
        checksum = (checksum + state * state) % MODULUS
    elif lane == 6:
        checksum = (checksum + state / 4 + i) % MODULUS
    else:
        checksum = (checksum + state + 1) % MODULUS
    i = i + 1
print("branch_dispatch CHECKSUM: " + str(checksum))
print("PASS")
```

---

## call_chain

> Deeply nested arithmetic pipeline simulating function call overhead.
> Each iteration computes a 5-deep chain of dependent operations:
>   a = i + 7
>   b = (a * 3 + 11) / 2
>   c = ((b * 5 + 3) * 7 + 13) % 1000
>   d = ((c + 17) * 11 + 5) * 3 + 1
>   e = (((d * 13 + 7) * 5 + 3) * 7 + 11) * 3 + 1
> 50,000 iterations. Exercises data-dependent latency chains.

```markscript
let MODULUS = 1000000007
let checksum = 0
let i = 0
while i < 50000:
    let a = i + 7
    let b = (a * 3 + 11) / 2
    let c = ((b * 5 + 3) * 7 + 13) % 1000
    let d = ((c + 17) * 11 + 5) * 3 + 1
    let e = (((d * 13 + 7) * 5 + 3) * 7 + 11) * 3 + 1
    checksum = (checksum + e) % MODULUS
    i = i + 1
print("call_chain CHECKSUM: " + str(checksum))
print("PASS")
```

---

## string_ops

> String construction via concatenation with cyclic pattern selection.
> Builds a 600-character string by cycling through "ABC", "DEF", "GHI"
> patterns over 200 iterations. Measures string allocation, concatenation
> throughput, and len() in the VM.

```markscript
let MODULUS = 1000000007
let s = ""
let i = 0
while i < 200:
    if i % 3 == 0:
        s = s + "ABC"
    elif i % 3 == 1:
        s = s + "DEF"
    else:
        s = s + "GHI"
    i = i + 1
let length = len(s)
let checksum = (length * 31 + 7) % MODULUS
print("string_ops CHECKSUM: " + str(checksum))
print("PASS")
```

---

## array_scan

> Fixed 16-element integer array with position-weighted accumulation.
> Array contains the first 16 digits of Pi: [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3].
> Each element is multiplied by weight = (index % 3) + 1 before folding.
> Measures array literal parsing, indexing, and loop-based reduction.

```markscript
let MODULUS = 1000000007
let arr = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3]
let checksum = 0
let i = 0
while i < 16:
    let val = arr[i]
    let weight = (i % 3) + 1
    checksum = (checksum + val * weight) % MODULUS
    i = i + 1
print("array_scan CHECKSUM: " + str(checksum))
print("PASS")
```

---

## mandelbrot

> Fixed-point Mandelbrot set on a 40x40 grid with max 30 iterations per pixel.
> Uses integer scaling (scale=64) to avoid floating point. Coordinates:
>   x: [-2.0, 1.0]  →  scaled: [-128, 64]
>   y: [-1.5, 1.5]  →  scaled: [-96, 96]
> Escape condition: zr^2 + zi^2 <= 4 * scale^2 (= 16384)
> Total iteration count across all pixels is reduced modulo MODULUS.
> Exercises deep nesting (y-loop / x-loop / iteration-loop) and fixed-point
> arithmetic in a compute-heavy kernel.

```markscript
let MODULUS = 1000000007
let width = 40
let height = 40
let max_iter = 30
let scale = 64
let xmin = -128
let xmax = 64
let ymin = -96
let ymax = 96
let four_scale_sq = 16384
let total_iters = 0
let py = 0
let px = 0
let cr = 0
let ci = 0
let zr = 0
let zi = 0
let iter = 0
let zr2 = 0
let zi2 = 0

py = 0
while py < height:
    px = 0
    while px < width:
        cr = xmin + (xmax - xmin) * px / width
        ci = ymin + (ymax - ymin) * py / height
        zr = 0
        zi = 0
        iter = 0
        while zr * zr + zi * zi <= four_scale_sq and iter < max_iter:
            zr2 = (zr * zr - zi * zi) / scale + cr
            zi2 = (2 * zr * zi) / scale + ci
            zr = zr2
            zi = zi2
            iter = iter + 1
        total_iters = total_iters + iter
        px = px + 1
    py = py + 1

let checksum = total_iters % MODULUS
print("mandelbrot CHECKSUM: " + str(checksum))
print("PASS")
```

---

## fasta_lite

> LCG-driven DNA sequence simulation. Generates 50,000 nucleotides with
> weighted A/C/G/T frequencies using the deterministic LCG:
>   A: 22%   C: 28%   G: 28%   T: 22%
> Each nucleotide's byte value (ASCII: A=65, C=67, G=71, T=84) is folded
> into a rolling checksum via (checksum * 31 + byte) % MODULUS.
> Measures LCG generation + multi-branch selection in a compute loop.

```markscript
let MODULUS = 1000000007
let state = 42
let checksum = 0
let i = 0
let byte_val = 0
while i < 50000:
    state = (state * 1103515245 + 12345) & 2147483647
    let r = state % 100
    if r < 22:
        byte_val = 65
    elif r < 50:
        byte_val = 67
    elif r < 78:
        byte_val = 71
    else:
        byte_val = 84
    checksum = (checksum * 31 + byte_val) % MODULUS
    i = i + 1
print("fasta_lite CHECKSUM: " + str(checksum))
print("PASS")
```

---

## fizzbuzz_bomb

> FizzBuzz from 1 to 10,000. Counts each category (FizzBuzz, Fizz, Buzz)
> and accumulates the sum of plain numbers. The final checksum packs
> the four counters into a single value. Exercises modulo dispatch,
> branch prediction, and sustained loop throughput over a larger range.

```markscript
let MODULUS = 1000000007
let fizzbuzz_count = 0
let buzz_count = 0
let fizz_count = 0
let num_sum = 0
let i = 1
while i <= 10000:
    if i % 15 == 0:
        fizzbuzz_count = fizzbuzz_count + 1
    elif i % 3 == 0:
        fizz_count = fizz_count + 1
    elif i % 5 == 0:
        buzz_count = buzz_count + 1
    else:
        num_sum = (num_sum + i) % MODULUS
    i = i + 1
let checksum = (fizzbuzz_count * 10000 + fizz_count * 100 + buzz_count + num_sum) % MODULUS
print("fizzbuzz_bomb CHECKSUM: " + str(checksum))
print("PASS")
```

---

## prime_sieve

> Trial-division prime counting up to 5000. For each n from 2 to 5000,
> tests divisibility by all d from 2 to sqrt(n) using `and` short-circuit.
> The checksum is accumulated from each prime by:
>   checksum = (checksum * 31 + prime) % MODULUS
> Exercises nested while loops with early-exit via a flag variable.

```markscript
let MODULUS = 1000000007
let limit = 5000
let prime_count = 0
let checksum = 0
let n = 2
let is_prime = 1
let d = 2
while n <= limit:
    is_prime = 1
    d = 2
    while d * d <= n and is_prime == 1:
        if n % d == 0:
            is_prime = 0
        d = d + 1
    if is_prime == 1:
        prime_count = prime_count + 1
        checksum = (checksum * 31 + n) % MODULUS
    n = n + 1
print("prime_sieve CHECKSUM: " + str(checksum))
print("PASS")
```

---

## collatz_conjecture

> Collatz sequence length for each starting number from 1 to 5000.
> For each n: while val > 1: if even → val/2, if odd → 3*val+1, count steps.
> Total steps across all starting values is accumulated and reduced modulo MODULUS.
> Exercises conditional branching inside a hot loop with data-dependent iteration count.

```markscript
let MODULUS = 1000000007
let end_val = 5000
let total_steps = 0
let n = 1
let val = 0
let steps = 0
while n <= end_val:
    val = n
    steps = 0
    while val > 1:
        if val % 2 == 0:
            val = val / 2
        else:
            val = val * 3 + 1
        steps = steps + 1
    total_steps = total_steps + steps
    n = n + 1
let checksum = total_steps % MODULUS
print("collatz_conjecture CHECKSUM: " + str(checksum))
print("PASS")
```

---

## fibonacci_mod

> Compute Fibonacci(100,000) modulo 1000000007 using the classic recurrence:
>   F_0 = 0, F_1 = 1,  F_n = (F_{n-1} + F_{n-2}) % MODULUS
> Exercises tight-loop recurrence with modular reduction at every step.

```markscript
let MODULUS = 1000000007
let n = 100000
let a = 0
let b = 1
let i = 2
let c = 0
while i <= n:
    c = (a + b) % MODULUS
    a = b
    b = c
    i = i + 1
let checksum = b
print("fibonacci_mod CHECKSUM: " + str(checksum))
print("PASS")
```

---

## pi_approx

> Leibniz series for pi/4 computed with fixed-point integer arithmetic:
>   pi/4 = 1 - 1/3 + 1/5 - 1/7 + 1/9 - ...
> Scale factor = 1,000,000. 20,000 alternating terms. Each term is computed
> as (4 * scale) / (2*i + 1), then added or subtracted based on parity.
> The fixed-point sum approximates pi * scale ≈ 3,141,593.
> Exercises alternating accumulation and large-number integer division.

```markscript
let MODULUS = 1000000007
let pi_scale = 1000000
let pi_sum = 0
let i = 0
let term = 0
while i < 20000:
    term = (4 * pi_scale) / (2 * i + 1)
    if i % 2 == 0:
        pi_sum = pi_sum + term
    else:
        pi_sum = pi_sum - term
    i = i + 1
let checksum = pi_sum % MODULUS
print("pi_approx CHECKSUM: " + str(checksum))
print("PASS")
```

---

## vm_bytecode_stress

> Triple-nested while loops (15x15x15 = 3,375 inner iterations) with
> conditional dispatch at each level. At the innermost level, an if/elif/else
> tree selects among 8 different folding operations based on parity and
> divisibility of the loop counters. Exercises the VM's loop frame stack,
> nested scope handling, multi-level branch prediction, and register pressure.

```markscript
let MODULUS = 1000000007
let checksum = 0
let a = 0
let b = 0
let c = 0
a = 0
while a < 15:
    b = 0
    while b < 15:
        c = 0
        while c < 15:
            if a % 2 == 0:
                if b % 3 == 0:
                    if c % 5 == 0:
                        checksum = (checksum + a * 100 + b * 10 + c) % MODULUS
                    else:
                        checksum = (checksum + a * 50 + b * 5) % MODULUS
                else:
                    checksum = (checksum + a * 10 + c) % MODULUS
            else:
                if b % 2 == 0:
                    checksum = (checksum + b * 10 + c) % MODULUS
                else:
                    checksum = (checksum + a + b + c) % MODULUS
            c = c + 1
        b = b + 1
    a = a + 1
print("vm_bytecode_stress CHECKSUM: " + str(checksum))
print("PASS")
```

---

## checksum_ladder

> Chain of five dependent operations per iteration, designed to stress the
> VM's evaluation stack depth and operation chaining:
>   1. LCG step (state update)
>   2. Mix fold: checksum = (checksum + state % 1000) % MODULUS
>   3. Multiply fold: checksum = (checksum * 31 + state % 100) % MODULUS
>   4. Counter fold: checksum = (checksum + i * 7) % MODULUS
>   5. Collatz step: if checksum is even → /2, else → *3+1, reduce
> 10,000 iterations. Exercises deep data-dependent chains across multiple
> operations including multiply, divide, modulo, and conditional branching.

```markscript
let MODULUS = 1000000007
let state = 42
let checksum = 42
let i = 0
let mix = 0
while i < 10000:
    state = (state * 1103515245 + 12345) & 2147483647
    mix = state % 1000
    checksum = (checksum + mix) % MODULUS
    checksum = (checksum * 31 + state % 100) % MODULUS
    checksum = (checksum + i * 7) % MODULUS
    if checksum % 2 == 0:
        checksum = checksum / 2
    else:
        checksum = checksum * 3 + 1
    checksum = checksum % MODULUS
    i = i + 1
print("checksum_ladder CHECKSUM: " + str(checksum))
print("PASS")
```

---

# Benchmark Registry

| Section | Iterations | Type | Description |
|---------|-----------|------|-------------|
| scalar_mix | 100,000 | compute | LCG + modulo accumulation |
| recursive_sum | 50,000 | compute | Triangular number + self-verify |
| branch_dispatch | 50,000 | control | 8-lane if/elif on LCG modulo |
| call_chain | 50,000 | compute | 5-deep arithmetic dependency chain |
| string_ops | 200 | string | Concatenation + len() |
| array_scan | 16 | array | Fixed array indexing + reduction |
| mandelbrot | 1,600 pixels | compute | 3-level nested loop + fixed-point |
| fasta_lite | 50,000 | compute | LCG + 4-way weighted nucleotide dispatch |
| fizzbuzz_bomb | 10,000 | compute | Modulo + multi-class accumulation |
| prime_sieve | 5,000 | compute | Nested trial-division loop |
| collatz_conjecture | 5,000 | compute | Data-dependent iteration length |
| fibonacci_mod | 100,000 | compute | Modular recurrence |
| pi_approx | 20,000 | compute | Alternating-series fixed-point |
| vm_bytecode_stress | 3,375 | control | Triple-nested if/elif dispatch |
| checksum_ladder | 10,000 | compute | 5-stage chained operation pipeline |

---

# Runner Integration

To add expected values after the first reference run:

1. Run: `mks run bench.md | findstr "CHECKSUM:" > checksums.txt`
2. Record each checksum value
3. Update the corresponding `print("PASS")` line to include a comparison

Example for the runner (bench.py pseudo-dispatcher):

```
benchmarks:
  scalar_mix:         mks run bench.md | findstr "scalar_mix CHECKSUM"
  recursive_sum:      mks run bench.md | findstr "recursive_sum CHECKSUM"
  branch_dispatch:    mks run bench.md | findstr "branch_dispatch CHECKSUM"
  ...
```

To run a single benchmark, comment out other sections or use an external
script that greps the relevant output line.

---

*Built with [MarkScript](https://kain-lang.org/markscript) — the prose-native scripting runtime for Kain.*
*"Your documentation is your program."*
