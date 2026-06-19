# V3 Benchmarks — MarkScript Edition

## scalar_mix

```markscript
let MOD = 1000000007
let cs = 0
let i = 0
let x = 42
while i < 5000:
    x = x * 13 + 7
    x = x - (x / MOD) * MOD
    cs = cs + x
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```

## recursive_sum

```markscript
let N = 5000
let sum = 0
let i = 1
while i < 5001:
    sum = sum + i
    i = i + 1
print(str(sum))
```

## branch_dispatch

```markscript
let MOD = 1000000007
let cs = 0
let i = 0
let x = 42
while i < 5000:
    x = x * 13 + 7
    x = x - (x / MOD) * MOD
    let l = x - (x / 8) * 8
    if l == 0:
        cs = cs + x
    elif l == 1:
        cs = cs * 31 + x
    elif l == 2:
        cs = cs + x * 3
    elif l == 3:
        cs = cs + x / 2
    elif l == 4:
        cs = cs * 7 + x - (x / 100) * 100
    elif l == 5:
        cs = cs + x * x
    elif l == 6:
        cs = cs + x / 4 + i
    else:
        cs = cs + x + 1
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```

## call_chain

```markscript
let MOD = 1000000007
let cs = 0
let i = 0
while i < 1000:
    let a = i + 7
    let b = (a * 3 + 11) / 2
    let c = ((b * 5 + 3) * 7 + 13)
    c = c - (c / 1000) * 1000
    let d = ((c + 17) * 11 + 5) * 3 + 1
    let e = (((d * 13 + 7) * 5 + 3) * 7 + 11) * 3 + 1
    cs = cs + e
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```

## mandelbrot

```markscript
let w = 10
let h = 10
let mx = 8
let sc = 64
let xmin = -128
let xmax = 64
let ymin = -96
let ymax = 96
let fsc = 16384
let tot = 0
let py = 0
while py < h:
    let px = 0
    while px < w:
        let cr = xmin + (xmax - xmin) * px / w
        let ci = ymin + (ymax - ymin) * py / h
        let zr = 0
        let zi = 0
        let it = 0
        let done = 0
        while done == 0:
            if zr * zr + zi * zi < fsc + 1:
                if it < mx:
                    let zr2 = (zr * zr - zi * zi) / sc + cr
                    let zi2 = (2 * zr * zi) / sc + ci
                    zr = zr2
                    zi = zi2
                    it = it + 1
                else:
                    done = 1
            else:
                done = 1
        tot = tot + it
        px = px + 1
    py = py + 1
print(str(tot))
```

## fasta_lite

```markscript
let MOD = 1000000007
let cs = 0
let i = 0
let x = 42
while i < 5000:
    x = x * 13 + 7
    x = x - (x / MOD) * MOD
    let r = x - (x / 100) * 100
    let bv = 0
    if r < 22:
        bv = 65
    elif r < 50:
        bv = 67
    elif r < 78:
        bv = 71
    else:
        bv = 84
    cs = cs * 31 + bv
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```

## fizzbuzz_bomb

```markscript
let MOD = 1000000007
let fbc = 0
let bc = 0
let fc = 0
let ns = 0
let i = 1
while i < 501:
    let t15 = i - (i / 15) * 15
    let t3 = i - (i / 3) * 3
    let t5 = i - (i / 5) * 5
    if t15 == 0:
        fbc = fbc + 1
    elif t3 == 0:
        fc = fc + 1
    elif t5 == 0:
        bc = bc + 1
    else:
        ns = ns + i
    i = i + 1
ns = ns - (ns / MOD) * MOD
let cs = fbc * 10000 + fc * 100 + bc + ns
cs = cs - (cs / MOD) * MOD
print(str(cs))
```

## prime_sieve

```markscript
let MOD = 1000000007
let lim = 300
let cs = 0
let n = 2
while n < 301:
    let ip = 1
    let d = 2
    while d < n / d + 1:
        if ip > 0:
            let tn = n - (n / d) * d
            if tn == 0:
                ip = 0
        d = d + 1
    if ip > 0:
        cs = cs * 31 + n
        cs = cs - (cs / MOD) * MOD
    n = n + 1
print(str(cs))
```

## collatz_conjecture

```markscript
let MOD = 1000000007
let ev = 500
let ts = 0
let n = 1
while n < 501:
    let v = n
    let st = 0
    while v > 1:
        let te = v - (v / 2) * 2
        if te == 0:
            v = v / 2
        else:
            v = v * 3 + 1
        st = st + 1
    ts = ts + st
    n = n + 1
print(str(ts))
```

## fibonacci_mod

```markscript
let MOD = 1000000007
let fn = 5000
let a = 0
let b = 1
let i = 0
while i < 5000:
    let c = a + b
    c = c - (c / MOD) * MOD
    a = b
    b = c
    i = i + 1
print(str(b))
```

## pi_approx

```markscript
let MOD = 1000000007
let ps = 1000000
let sum = 0
let i = 0
while i < 500:
    let t = (4 * ps) / (2 * i + 1)
    let te = i - (i / 2) * 2
    if te == 0:
        sum = sum + t
    else:
        sum = sum - t
    i = i + 1
while sum < 0:
    sum = sum + MOD
sum = sum - (sum / MOD) * MOD
print(str(sum))
```

## vm_bytecode_stress

```markscript
let MOD = 1000000007
let cs = 0
let a = 0
while a < 5:
    let b = 0
    while b < 5:
        let c = 0
        while c < 5:
            let pa = a - (a / 2) * 2
            if pa == 0:
                let pb = b - (b / 3) * 3
                if pb == 0:
                    let pc = c - (c / 5) * 5
                    if pc == 0:
                        cs = cs + a * 100 + b * 10 + c
                    else:
                        cs = cs + a * 50 + b * 5
                else:
                    cs = cs + a * 10 + c
            else:
                let pb2 = b - (b / 2) * 2
                if pb2 == 0:
                    cs = cs + b * 10 + c
                else:
                    cs = cs + a + b + c
            cs = cs - (cs / MOD) * MOD
            c = c + 1
        b = b + 1
    a = a + 1
print(str(cs))
```

## checksum_ladder

```markscript
let MOD = 1000000007
let cs = 42
let i = 0
let x = 42
while i < 1000:
    x = x * 13 + 7
    x = x - (x / MOD) * MOD
    let mix = x - (x / 1000) * 1000
    cs = cs + mix
    cs = cs - (cs / MOD) * MOD
    cs = cs * 31 + x - (x / 100) * 100
    cs = cs - (cs / MOD) * MOD
    cs = cs + i * 7
    cs = cs - (cs / MOD) * MOD
    let te = cs - (cs / 2) * 2
    if te == 0:
        cs = cs / 2
    else:
        cs = cs * 3 + 1
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```

## array_scan

```markscript
let MOD = 1000000007
let cs = 0
cs = cs + 3
cs = cs + 1 * 2
cs = cs + 4 * 3
cs = cs + 1
cs = cs + 5 * 2
cs = cs + 9 * 3
cs = cs + 2
cs = cs + 6 * 2
cs = cs + 5 * 3
cs = cs + 3
cs = cs + 5 * 2
cs = cs + 8 * 3
cs = cs + 9
cs = cs + 7 * 2
cs = cs + 9 * 3
cs = cs + 3
cs = cs - (cs / MOD) * MOD
print(str(cs))
```

## string_ops

```markscript
let MOD = 1000000007
let cs = 0
let i = 0
while i < 500:
    cs = cs + i * 7
    cs = cs - (cs / MOD) * MOD
    i = i + 1
print(str(cs))
```
