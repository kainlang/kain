"""LSP smoke test — batch-send all messages upfront, collect full stdout, parse responses."""
import subprocess
import json
import sys
import os
import re

KAIN_BIN = os.environ.get("KAIN_BIN", r"X:\.kain\bin\kain.exe")
LSP_ENTRY = r"X:\blades\lsp\src\main.kn"


def lsp_frame(body: str) -> bytes:
    return f"Content-Length: {len(body)}\r\n\r\n{body}".encode()


def parse_lsp_responses(data: bytes):
    msgs = []
    pos = 0
    while pos < len(data):
        hm = re.search(rb"Content-Length:\s*(\d+)", data[pos:])
        if not hm:
            break
        length = int(hm.group(1))
        bs = data.find(b"\r\n\r\n", pos + hm.start())
        if bs < 0:
            bs = data.find(b"\n\n", pos + hm.start())
        if bs < 0:
            break
        bs += 4 if data[bs:bs+4] == b"\r\n\r\n" else 2
        body = data[bs:bs + length]
        if len(body) == length:
            msgs.append(json.loads(body.decode()))
        pos = bs + length
    return msgs


def main():
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass
    print("=== Kain LSP Smoke Test ===", flush=True)

    input_msgs = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize",
         "params": {"processId": 12345, "capabilities": {}}},
        {"jsonrpc": "2.0", "method": "initialized", "params": {}},
        {"jsonrpc": "2.0", "method": "textDocument/didOpen",
         "params": {"textDocument": {
             "uri": "file:///test.kn", "languageId": "kain",
             "version": 1, "text": "pub fn hello() -> Int:\n    return 42\n"
         }}},
        {"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
         "params": {"textDocument": {"uri": "file:///test.kn"},
                    "position": {"line": 0, "character": 4}}},
        {"jsonrpc": "2.0", "id": 10, "method": "shutdown", "params": {}},
        {"jsonrpc": "2.0", "method": "exit", "params": {}},
    ]

    stdin_data = b"".join(lsp_frame(json.dumps(m)) for m in input_msgs)

    proc = subprocess.Popen(
        [KAIN_BIN, "run", LSP_ENTRY],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        cwd=r"X:\blades\lsp",
    )
    stdout_data, stderr_data = proc.communicate(input=stdin_data, timeout=60)
    print(f"  Exit code: {proc.returncode}", flush=True)

    responses = parse_lsp_responses(stdout_data)
    print(f"  Parsed {len(responses)} JSON-RPC messages", flush=True)

    stderr_lines = [l for l in stderr_data.decode().split("\n")
                    if l.strip() and "Run Once" not in l and "build graph" not in l
                    and "Succeeded" not in l]
    for line in stderr_lines:
        print(f"  [stderr] {line.strip()}", flush=True)

    assert proc.returncode == 0, f"Expected exit 0, got {proc.returncode}"

    resp_by_id = {r.get("id"): r for r in responses if "id" in r}
    assert 1 in resp_by_id, "Missing initialize response"
    assert "capabilities" in resp_by_id[1].get("result", {}), "Missing capabilities"
    print("  [OK] initialize", flush=True)

    if 2 in resp_by_id:
        print(f"  [OK] hover id=2", flush=True)
    else:
        print("  ? no hover response", flush=True)

    assert 10 in resp_by_id, "Missing shutdown response"
    print("  [OK] shutdown", flush=True)

    print("\n=== ALL SMOKE TESTS PASSED ===", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
