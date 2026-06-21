#!/usr/bin/env python3
"""Extract function ranges, branch ladders, constants, and state machines from ownership.c.

Usage: python ownership_extract.py [path/to/ownership.c]

Outputs JSON with:
  - functions: name, line_range, branches, state_machine_type
  - tables: static tables found
  - constants: enum values and #defines
  - optimizable_ladders: if/else chains that could be table lookups
"""

import re
import sys
import json
from pathlib import Path


def extract_functions(text):
    """Extract function definitions with line ranges."""
    func_pattern = re.compile(
        r'^static\s+\w+(?:\s*\*)?\s*(\w+)\s*\(', re.MULTILINE
    )
    functions = []
    lines = text.split('\n')
    
    for match in func_pattern.finditer(text):
        name = match.group(1)
        start = text[:match.start()].count('\n') + 1
        
        # Find matching brace
        brace_start = text.find('{', match.start())
        if brace_start < 0:
            continue
        depth = 0
        pos = brace_start
        while pos < len(text):
            if text[pos] == '{':
                depth += 1
            elif text[pos] == '}':
                depth -= 1
                if depth == 0:
                    end = text[:pos].count('\n') + 1
                    body = text[brace_start:pos+1]
                    functions.append({
                        'name': name,
                        'line_start': start,
                        'line_end': end,
                        'body_length': len(body),
                        'branch_count': body.count('if '),
                        'switch_count': body.count('switch '),
                    })
                    break
            pos += 1
    
    return functions


def classify_branch_ladder(text, func_name, start_line):
    """Analyze a branch ladder for table-lookup optimization potential."""
    lines = text.split('\n')
    # Find if-chains that test the same variable with equality
    pattern = re.compile(
        r'if\s*\(\s*(\w+)\s*==\s*(\w+)\s*\)', re.MULTILINE
    )
    
    ladders = []
    for i, line in enumerate(lines):
        matches = list(pattern.finditer(line))
        if matches:
            for m in matches:
                ladders.append({
                    'line': i + 1,
                    'variable': m.group(1),
                    'value': m.group(2),
                    'func': func_name,
                })
    
    return ladders


def extract_tables(text):
    """Find static const table definitions."""
    table_pattern = re.compile(
        r'static\s+(?:const\s+)?\w+\s+(\w+)\[(\d+|\w+)\]\s*=\s*\{([^}]+)\}', 
        re.DOTALL
    )
    tables = []
    for match in table_pattern.finditer(text):
        tables.append({
            'name': match.group(1),
            'size': match.group(2),
            'preview': match.group(3)[:80].strip(),
            'line': text[:match.start()].count('\n') + 1,
        })
    return tables


def extract_constants(text):
    """Extract enum and #define constants."""
    constants = []
    
    # Enums
    enum_pattern = re.compile(
        r'enum\s*\{([^}]+)\}', re.DOTALL
    )
    for match in enum_pattern.finditer(text):
        body = match.group(1)
        for item in body.split(','):
            item = item.strip()
            if item:
                constants.append({
                    'type': 'enum',
                    'value': item,
                    'line': text[:match.start()].count('\n') + 1,
                })
    
    # Defines
    define_pattern = re.compile(
        r'#define\s+(\w+)\s+(.+)', re.MULTILINE
    )
    for match in define_pattern.finditer(text):
        constants.append({
            'type': 'define',
            'name': match.group(1),
            'value': match.group(2).strip(),
            'line': text[:match.start()].count('\n') + 1,
        })
    
    return constants


def find_guard_ladders(text):
    """Find repeated guard ladders (same pattern across functions)."""
    # Match the state-machine guard pattern
    guard_pattern = re.compile(
        r'if\s*\(.*state\s*==\s*KAIN_OWNERSHIP_STATE_(\w+)\).*'
        r'(?:\n.*?){0,3}'
        r'if\s*\(.*state\s*==\s*KAIN_OWNERSHIP_STATE_(\w+)\).*'
        r'(?:\n.*?){0,3}'
        r'if\s*\(.*state\s*==\s*KAIN_OWNERSHIP_STATE_(\w+)\).*'
        r'(?:\n.*?){0,3}'
        r'if\s*\(.*state\s*==\s*KAIN_OWNERSHIP_STATE_(\w+)',
        re.DOTALL
    )
    guards = []
    for match in guard_pattern.finditer(text):
        guards.append({
            'line': text[:match.start()].count('\n') + 1,
            'states_checked': list(match.groups()),
            'length': match.end() - match.start(),
        })
    return guards


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else \
        r'X:/runtime/native/src/core/ownership.c'
    
    text = Path(path).read_text(encoding='utf-8')
    
    result = {
        'file': path,
        'lines': text.count('\n') + 1,
        'size_bytes': len(text),
        'functions': extract_functions(text),
        'tables': extract_tables(text),
        'constants': extract_constants(text),
        'guard_ladders': find_guard_ladders(text),
        'summary': {
            'total_functions': 0,
            'total_branches': 0,
            'total_switches': 0,
            'optimizable_ladders': 0,
        }
    }
    
    funcs = result['functions']
    result['summary']['total_functions'] = len(funcs)
    result['summary']['total_branches'] = sum(f['branch_count'] for f in funcs)
    result['summary']['total_switches'] = sum(f['switch_count'] for f in funcs)
    result['summary']['optimizable_ladders'] = len(result['guard_ladders'])
    
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
