ITERATIONS = 3_000_000
MODULUS = 1_000_000_007
EXPECTED = 632_706_747


def classify(value: int) -> int:
    tag = value % 8
    if tag == 0:
        return value + 1
    if tag == 1:
        return (value * 3) + 7
    if tag == 2:
        return value - 5
    if tag == 3:
        return (value * value) + 11
    if tag == 4:
        return value + 17
    if tag == 5:
        return (value * 5) - 13
    if tag == 6:
        return value + 23
    return value - 11


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        acc = (acc + classify(i)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
