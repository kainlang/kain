import os

patterns = {
    'trailing_continuation': [],  # \ at end of line (line continuation)
    'escaped_quote': [],          # \" inside strings
    'escape_n': 0,                # \n
    'escape_t': 0,                # \t
    'windows_path': 0,            # C:\ style
    'other': [],
}

for root, dirs, files in os.walk('src'):
    dirs[:] = [d for d in dirs if d not in ('.kain', 'cache', 'out')]
    for f in files:
        if not f.endswith('.kn'):
            continue
        path = os.path.join(root, f)
        with open(path, 'rb') as fh:
            content = fh.read()
        lines = content.split(b'\n')
        for i, line in enumerate(lines, 1):
            stripped = line.rstrip(b'\r')
            # Check for trailing backslash (line continuation)
            if stripped.endswith(b'\\') and not stripped.endswith(b'\\\\'):
                if stripped.rstrip(b' ').endswith(b'\\'):
                    patterns['trailing_continuation'].append((path, i, stripped.decode('utf-8', errors='replace').strip()))
            # Check for \" inside strings
            if b'\\"' in line:
                patterns['escaped_quote'].append((path, i, line.decode('utf-8', errors='replace').strip()))
            # Check for \n
            if b'\\n' in line:
                patterns['escape_n'] += 1
            # Check for \t
            if b'\\t' in line:
                patterns['escape_t'] += 1

print("=== TRAILING CONTINUATION (backslash at end of line) ===")
print(f"Count: {len(patterns['trailing_continuation'])}")
for path, line, content in patterns['trailing_continuation'][:20]:
    print(f"  {path}:{line}: {content}")

print("\n=== ESCAPED QUOTES (\\\") ===")
print(f"Count: {len(patterns['escaped_quote'])}")
for path, line, content in patterns['escaped_quote'][:20]:
    print(f"  {path}:{line}: {content}")

print(f"\n=== \\n in lines: {patterns['escape_n']} ===")
print(f"=== \\t in lines: {patterns['escape_t']} ===")
