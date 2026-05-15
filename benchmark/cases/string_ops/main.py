STRING_TEXT = "ka0in0be0nch"
STRING_NEEDLE = "in"
STRING_TAIL = "ch"
ITERATIONS = 100_000
MODULUS = 1_000_000_007
EXPECTED = 2_050_000


def starts_with_at(text: str, index: int, needle: str) -> bool:
    if index + len(needle) > len(text):
        return False
    offset = 0
    while offset < len(needle):
        if text[index + offset] != needle[offset]:
            return False
        offset += 1
    return True


def find_substring(text: str, needle: str, start: int) -> int:
    if len(needle) == 0:
        return start
    index = start
    while index + len(needle) <= len(text):
        if starts_with_at(text, index, needle):
            return index
        index += 1
    return len(text)


def main() -> int:
    acc = 0
    i = 0
    while i < ITERATIONS:
        if i % 2 == 0:
            acc = (acc + len(STRING_TEXT) + find_substring(STRING_TEXT, STRING_NEEDLE, 0) + len(STRING_NEEDLE)) % MODULUS
        else:
            acc = (acc + len(STRING_TEXT) + find_substring(STRING_TEXT, STRING_TAIL, 0) + len(STRING_TAIL)) % MODULUS
        i += 1
    return 0 if acc == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
