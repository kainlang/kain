ITERATIONS = 300_000
MODULUS = 1_000_000_007
EXPECTED = 143_207_783


def maybe_value(value: int) -> int | None:
    if value % 5 == 0:
        return None
    return value + 3


def parse_value(value: int) -> tuple[bool, int | str]:
    if value % 7 == 0:
        return False, "skip"
    return True, value * 2


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        maybe = maybe_value(i)
        maybe_component = 1 if maybe is None else maybe
        parsed_ok, parsed_value_result = parse_value(i)
        parsed_component = parsed_value_result if parsed_ok else 2
        acc = (acc + maybe_component + int(parsed_component)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
