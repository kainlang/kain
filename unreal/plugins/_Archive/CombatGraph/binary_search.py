import subprocess
import sys

# Read the file
with open('combat_graph.kn', 'r', encoding='utf-8') as f:
    lines = f.readlines()

total_lines = len(lines)
print(f"Total lines: {total_lines}")

# Binary search to find the problematic section
left = 0
right = total_lines

while left < right - 1:
    mid = (left + right) // 2
    print(f"\nTesting lines {left} to {mid}...")
    
    # Create test file with first half
    test_lines = lines[:mid]
    with open('test_binary.kn', 'w', encoding='utf-8') as f:
        f.writelines(test_lines)
    
    # Test compilation
    result = subprocess.run(
        ['M:\\Code\\Kain\\target\\release\\kain.exe', 'build', 'test_binary.kn', '--targets', 'rust'],
        capture_output=True,
        text=True,
        cwd='.'
    )
    
    if 'Parse error' in result.stderr or 'parse error' in result.stdout:
        print(f"  ✗ Parse error in first half (lines 1-{mid})")
        right = mid
    else:
        print(f"  ✓ First half parses OK (lines 1-{mid})")
        left = mid

print(f"\n\nProblem is around line {left}-{right}")
print(f"Lines {left-5} to {right+5}:")
for i in range(max(0, left-5), min(total_lines, right+5)):
    print(f"{i+1:4d}: {lines[i]}", end='')
