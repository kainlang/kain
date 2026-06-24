import os

# Count \n IN STRING LITERALS vs in comments
str_n_count = 0
comment_n_count = 0
files_with_n = {}

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
            if b'\\n' in line:
                # Check if it's in a string (between quotes) vs comment
                stripped = line.lstrip()
                if stripped.startswith(b'//'):
                    comment_n_count += 1
                else:
                    # In a string literal - look for pattern "...\n..."
                    if b'"' in line:
                        str_n_count += 1
                        if path not in files_with_n:
                            files_with_n[path] = []
                        files_with_n[path].append((i, line.decode('utf-8', errors='replace').strip()))
                    else:
                        comment_n_count += 1

print(f"\n in string literals: {str_n_count}")
print(f"\n in comments: {comment_n_count}")
print(f"\nFiles with \\n in strings:")
for path, items in sorted(files_with_n.items()):
    print(f"  {path}: {len(items)} occurrences")
    for line_no, content in items[:3]:
        print(f"    {line_no}: {content[:100]}")
    if len(items) > 3:
        print(f"    ... and {len(items)-3} more")
