WORKER_COUNT = 100
ITERATIONS_PER_WORKER = 1_000_000
EXPECTED = 100_000_000


def main() -> int:
    counter = 0
    worker = 0
    while worker < WORKER_COUNT:
        i = 0
        while i < ITERATIONS_PER_WORKER:
            counter += 1
            i += 1
        worker += 1
    return 0 if counter == EXPECTED else 1


if __name__ == "__main__":
    raise SystemExit(main())
