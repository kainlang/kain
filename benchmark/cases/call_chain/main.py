ITERATIONS = 1_500_000
MODULUS = 1_000_000_007
EXPECTED = 61_920_954


def step_a(value: int) -> int:
    return ((value * 3) + 1) % MODULUS


def step_b(value: int) -> int:
    return ((step_a(value) + 5) * 7) % MODULUS


def step_c(value: int) -> int:
    return (step_b(value) + step_a(value + 11) + 13) % MODULUS


def step_d(value: int) -> int:
    return ((step_c(value) * 3) + step_b(value + 17) + 19) % MODULUS


def main() -> int:
    acc = 1
    i = 0
    while i < ITERATIONS:
        acc = step_d(acc + i)
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
