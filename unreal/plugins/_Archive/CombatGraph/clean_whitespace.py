import sys

# Read the file
with open('combat_graph.kn', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Strip trailing whitespace from each line
cleaned_lines = [line.rstrip() + '\n' for line in lines]

# Remove trailing newline from last line if it exists
if cleaned_lines and cleaned_lines[-1] == '\n':
    cleaned_lines[-1] = ''
elif cleaned_lines:
    cleaned_lines[-1] = cleaned_lines[-1].rstrip('\n')

# Write back
with open('combat_graph.kn', 'w', encoding='utf-8', newline='\n') as f:
    f.writelines(cleaned_lines)

print(f"Cleaned {len(lines)} lines")
print(f"Line 894 length before: {len(lines[893]) if len(lines) > 893 else 'N/A'}")
print(f"Line 894 length after: {len(cleaned_lines[893]) if len(cleaned_lines) > 893 else 'N/A'}")
