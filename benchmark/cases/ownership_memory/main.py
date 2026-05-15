ITERATIONS = 750_000
MODULUS = 1_000_000_007
EXPECTED = 758_650_175


def run() -> int:
    cell = [0]
    i = 0
    while i < ITERATIONS:
        current = cell[0]
        cell[0] = ((current * 33) + i + 7) % MODULUS
        i += 1
    return cell[0]


def main() -> int:
    return 0 if run() == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
