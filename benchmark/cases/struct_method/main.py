ITERATIONS = 1_000_000
MODULUS = 1_000_000_007
EXPECTED = 393_996_945


class BenchPair:
    __slots__ = ("x", "y")

    def __init__(self, x: int, y: int) -> None:
        self.x = x
        self.y = y


def make_pair(seed: int) -> BenchPair:
    return BenchPair(seed % 97, (seed * 7) % 101)


def score_pair(pair: BenchPair) -> int:
    return (pair.x * 3) + (pair.y * 5)


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        pair = make_pair(i)
        acc = (acc + score_pair(pair)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
