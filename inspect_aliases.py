import re

with open(r'F:\Caches\bazel\output-user-root\n2kwlvv2\external\rules_rust++crate+crates\defs.bzl','r') as f:
    text = f.read()

m = re.search(r'_NORMAL_ALIASES\s*=\s*\{', text)
if m:
    start = m.end() - 1
    depth = 0
    for i, ch in enumerate(text[start:]):
        if ch == '{': depth += 1
        elif ch == '}': depth -= 1
        if depth == 0:
            block = text[start:start+i+1]
            break
    keys = re.findall(r'"([^"]+)"\s*:', block)
    print('Total keys:', len(keys))
    cli_match = re.search(r'"[^"]*cli[^"]*"\s*:\s*\{', block)
    if cli_match:
        cstart = cli_match.end() - 1
        depth = 0
        for i, ch in enumerate(block[cstart:]):
            if ch == '{': depth += 1
            elif ch == '}': depth -= 1
            if depth == 0:
                cli_block = block[cstart:cstart+i+1]
                print('Matched key:', cli_match.group(0))
                for line in cli_block.splitlines():
                    print(line.strip())
                break
    else:
        print('No cli-like entry found')
else:
    print('_NORMAL_ALIASES not found')
