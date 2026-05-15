CELLS = 262_144
MODULUS = 1_000_000_007
EXPECTED = 149_653_729


def main() -> int:
    buffer = [0] * CELLS
    i = 0
    while i < CELLS:
        buffer[i] = ((i * 31) + 7) % MODULUS
        i += 1

    checksum = 0
    j = 0
    while j < CELLS:
        checksum = (checksum + buffer[j]) % MODULUS
        j += 1
    return 0 if checksum == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
