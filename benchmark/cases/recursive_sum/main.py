ITERATIONS = 5_000
DEPTH = 128
MODULUS = 1_000_000_007
EXPECTED = 41_280_000


def recursive_sum(value: int) -> int:
    if value <= 0:
        return 0
    return value + recursive_sum(value - 1)


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        acc = (acc + recursive_sum(DEPTH)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
