ITERATIONS = 2_000_000
ADDEND = 17
OFFSET = ADDEND + 5
MODULUS = 1_000_000_007
EXPECTED = 42_986_000


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        acc = (acc + i + OFFSET) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
