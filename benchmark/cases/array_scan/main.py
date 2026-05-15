VALUES = [1, 2, 3, 4, 5, 6, 7, 8]
ITERATIONS = 500_000
MODULUS = 1_000_000_007
EXPECTED = 103_499_994


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        inner = 0
        index = 0
        while index < len(VALUES):
            inner = (inner + (VALUES[index] * (index + 1))) % MODULUS
            index += 1
        acc = (acc + inner + (i % 7)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
