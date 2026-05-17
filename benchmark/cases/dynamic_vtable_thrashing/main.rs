trait Kernel {
    fn score(&self, value: i64) -> i64;
}

struct AddKernel {
    bias: i64,
}

struct MultiplyKernel {
    bias: i64,
}

struct ModKernel {
    bias: i64,
}

struct SquareKernel {
    bias: i64,
}

struct BiasSquareKernel {
    bias: i64,
}

struct FoldKernel {
    bias: i64,
}

struct ExpandKernel {
    bias: i64,
}

struct XorKernel {
    bias: i64,
}

impl Kernel for AddKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        value + (self.bias * 3) + 7
    }
}

impl Kernel for MultiplyKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        (value * (self.bias + 5)) + 11
    }
}

impl Kernel for ModKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        ((value + self.bias) % 257) + (self.bias * 13)
    }
}

impl Kernel for SquareKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        (value * value) + (self.bias * 17) + 3
    }
}

impl Kernel for BiasSquareKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        (value * 9) + (self.bias * self.bias) + 19
    }
}

impl Kernel for FoldKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        (((value + 31) * (self.bias + 7)) % 4099) + 23
    }
}

impl Kernel for ExpandKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        (value * 5) + ((self.bias + 1) * 29)
    }
}

impl Kernel for XorKernel {
    #[inline(never)]
    fn score(&self, value: i64) -> i64 {
        ((value * 7) ^ (self.bias * 41)) + 37
    }
}

fn boxed_kernel(kind: i64, bias: i64) -> Box<dyn Kernel> {
    match kind {
        0 => Box::new(AddKernel { bias }),
        1 => Box::new(MultiplyKernel { bias }),
        2 => Box::new(ModKernel { bias }),
        3 => Box::new(SquareKernel { bias }),
        4 => Box::new(BiasSquareKernel { bias }),
        5 => Box::new(FoldKernel { bias }),
        6 => Box::new(ExpandKernel { bias }),
        _ => Box::new(XorKernel { bias }),
    }
}

const KERNEL_COUNT: usize = 64;
const ITERATIONS: i64 = 1_800_000;
const MODULUS: i64 = 1_000_000_007;
const EXPECTED: i64 = 185_456_717;

fn main() {
    let mut kernels = Vec::<Box<dyn Kernel>>::with_capacity(KERNEL_COUNT);
    let mut slot = 0_i64;
    while slot < KERNEL_COUNT as i64 {
        let kind = ((slot * 5) + 3) % 8;
        let bias = ((slot * 17) % 23) + 1;
        kernels.push(boxed_kernel(kind, bias));
        slot += 1;
    }

    let mut acc = 0_i64;
    let mut index = 0_i64;
    while index < ITERATIONS {
        let slot = (index % KERNEL_COUNT as i64) as usize;
        let value = ((index * 13) + 7) % 1009;
        let score = kernels[slot].score(value);
        acc = (acc + score + slot as i64) % MODULUS;
        index += 1;
    }

    let observed = unsafe { std::ptr::read_volatile(&acc) };
    if observed != EXPECTED {
        std::process::exit(1);
    }
}
