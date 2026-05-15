ITERATIONS = 2_000_000
EXPECTED = 403_591_996
MODULUS = 1_000_000_007


def scalar_lane(value: int) -> int:
    return ((value * 31) + 7) % MODULUS


def wide_lane(value: int) -> int:
    return ((value * 31) + 7) % MODULUS


def choose(value: int) -> int:
    return wide_lane(value)


def mix(value: int) -> int:
    return ((value * 17) + 11) % MODULUS


def pipeline(value: int) -> int:
    return mix(choose(value))


def main() -> int:
    acc = 1
    i = 0
    while i < ITERATIONS:
        acc = pipeline(acc + i)
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
