# Brainfuck Turing-Completeness Lab

This lab is the pragmatic proof that Kain can simulate a Turing-complete language without delegating the interpreter to Rust or another host language.

What lives here:

- `main.kn`: the Brainfuck interpreter and proof harness, written entirely in Kain
- `hello_world.bf`, `echo_input.bf`, `alphabet_pair.bf`: fixture programs used by the proof harness

Validation:

```bash
./target/debug/kain run labs/brainfuck/main.kn
./target/debug/kain build labs/brainfuck/main.kn --target llvm -o generated/brainfuck_lab/brainfuck_lab.ll
./generated/brainfuck_lab/brainfuck_lab
```

Proof shape:

- Conditional branching: Brainfuck loop dispatch is implemented with `[` and `]` bracket matching plus data-dependent jumps.
- Arbitrary memory manipulation: the interpreter uses a growable tape (`Array<Int>`) and extends it on demand as the pointer moves right.
- Closure of simulation: because the interpreter itself is written in Kain and executes Brainfuck programs correctly in both the native Kain runtime and the LLVM lane, Kain can simulate a known Turing-complete language.
