#!/usr/bin/env python
"""LSP smoke runner — called from lsp_smoke_py.kn via subprocess.run.
Spawns kain-lsp, sends LSP messages, writes result JSON to .kain/_smoke_result.json"""

import subprocess, json, os, re, sys

KAIN_BIN = os.environ.get("KAIN_BIN", r"X:\.kain\bin\kain.exe")
BLADE_ROOT = os.path.dirname(os.path.abspath(__file__))
RESULT_PATH = os.path.join(BLADE_ROOT, ".kain", "_smoke_result.json")

def cl_frame(body):
    return f"Content-Length: {len(body)}\r\n\r\n{body}"

def main():
    lsp_entry = "main.kn"
    bodies = [
        '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":123,"capabilities":{}}}',
        '{"jsonrpc":"2.0","method":"initialized","params":{}}',
        '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///t.kn","languageId":"kain","version":1,"text":"pub fn hello() -> Int:\\n    return 42\\n"}}}',
        '{"jsonrpc":"2.0","id":2,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///t.kn"},"position":{"line":0,"character":4}}}',
        '{"jsonrpc":"2.0","id":3,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///t.kn"},"position":{"line":0,"character":12}}}',
        '{"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///t.kn"},"position":{"line":0,"character":4}}}',
        '{"jsonrpc":"2.0","id":5,"method":"textDocument/references","params":{"textDocument":{"uri":"file:///t.kn"},"position":{"line":0,"character":4}}}',
        '{"jsonrpc":"2.0","id":6,"method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file:///t.kn"}}}',
        '{"jsonrpc":"2.0","id":7,"method":"textDocument/formatting","params":{"textDocument":{"uri":"file:///t.kn"},"options":{"tabSize":4,"insertSpaces":true}}}',
        '{"jsonrpc":"2.0","id":8,"method":"textDocument/diagnostic","params":{"textDocument":{"uri":"file:///t.kn"}}}',
        '{"jsonrpc":"2.0","id":9,"method":"textDocument/codeAction","params":{"textDocument":{"uri":"file:///t.kn"},"range":{"start":{"line":0,"character":0},"end":{"line":1,"character":0}},"context":{"diagnostics":[]}}}',
        '{"jsonrpc":"2.0","id":11,"method":"textDocument/codeLens","params":{"textDocument":{"uri":"file:///t.kn"}}}',
        '{"jsonrpc":"2.0","id":10,"method":"shutdown","params":{}}',
        '{"jsonrpc":"2.0","method":"exit","params":{}}',
    ]

    stdin_data = "".join(cl_frame(b) for b in bodies)

    os.makedirs(os.path.dirname(RESULT_PATH), exist_ok=True)

    proc = subprocess.Popen(
        [KAIN_BIN, "run", lsp_entry],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=BLADE_ROOT,
    )
    stdout_bytes, stderr_bytes = proc.communicate(input=stdin_data.encode("utf-8"), timeout=120)
    stdout_text = stdout_bytes.decode("utf-8", errors="replace")
    rc = proc.returncode

    responses = []
    pos = 0
    while pos < len(stdout_text):
        m = re.search(r"Content-Length:\s*(\d+)", stdout_text[pos:])
        if not m: break
        cl = int(m.group(1))
        bs = stdout_text.find("\r\n\r\n", pos + m.start())
        if bs < 0: bs = stdout_text.find("\n\n", pos + m.start())
        if bs < 0: break
        bs += 4 if stdout_text[bs:bs+4] == "\r\n\r\n" else 2
        body = stdout_text[bs:bs+cl]
        if len(body) == cl:
            try: responses.append(json.loads(body))
            except: pass
        pos = bs + cl

    with open(RESULT_PATH, "w") as f:
        json.dump({"rc": rc, "n": len(responses), "responses": responses}, f)

    print(f"smoke done: rc={rc} n={len(responses)}", file=sys.stderr)

if __name__ == "__main__":
    main()
