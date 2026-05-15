import socket
import threading


UPDATES = 64
BYTES_PER_PAYLOAD = 1_048_576
EXPECTED_CHECKSUM = 2_080
MODULUS = 1_000_000_007


def read_exact(stream: socket.socket, byte_count: int) -> bytes:
    chunks: list[bytes] = []
    remaining = byte_count
    while remaining > 0:
        chunk = stream.recv(remaining)
        if not chunk:
            raise RuntimeError("socket closed before payload was complete")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def receiver(listener: socket.socket, result: list[int]) -> None:
    connection, _ = listener.accept()
    with connection:
        payload = bytearray(BYTES_PER_PAYLOAD)
        checksum = 0
        for _ in range(UPDATES):
            header = read_exact(connection, 8)
            revision = int.from_bytes(header, "little")
            payload[:] = read_exact(connection, BYTES_PER_PAYLOAD)
            checksum = (checksum + revision) % MODULUS
        result.append(checksum)


def main() -> int:
    result: list[int] = []
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        host, port = listener.getsockname()
        thread = threading.Thread(target=receiver, args=(listener, result))
        thread.start()

        payload = bytearray(BYTES_PER_PAYLOAD)
        with socket.create_connection((host, port)) as stream:
            for revision in range(1, UPDATES + 1):
                seed = revision & 0xFF
                index = 0
                while index < len(payload):
                    payload[index] = (seed + (index & 0xFF)) & 0xFF
                    index += 4096
                stream.sendall(revision.to_bytes(8, "little"))
                stream.sendall(payload)

        thread.join()
    return 0 if result and result[0] == EXPECTED_CHECKSUM else 1


if __name__ == "__main__":
    raise SystemExit(main())
