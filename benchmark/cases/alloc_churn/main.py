ITERATIONS = 50_000
MODULUS = 1_000_000_007
EXPECTED = 250_324_993


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        cell = [i + 7]
        value = cell[0]
        acc = (acc + value) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
