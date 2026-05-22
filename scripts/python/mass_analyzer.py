import os
import re
import json

# Paths
ROOT_DIR = r"D:\Kain-Lang"
RUNTIME_CORE_DIR = os.path.join(ROOT_DIR, "runtime", "native", "src", "core")
OUTPUT_REPORT_PATH = os.path.join(ROOT_DIR, "z3", "reports", "mass_math_analysis.json")

# Rules to scan for risky patterns
RISKY_PATTERNS = [
    {
        "id": "allocation_multiplication_overflow",
        "description": "Multiplication inside an allocation function argument (malloc, realloc, calloc, __kain_alloc) without explicit overflow protection. Can cause small allocation and heap overflow.",
        "regex": r"\b(malloc|realloc|calloc|__kain_alloc|__kain_realloc|kain_alloc|alloc_zeroed)\s*\(\s*([^,)]*\s*\*\s*[^,)]*)\s*[\),]"
    },
    {
        "id": "unsafe_addition_in_offset",
        "description": "Addition or subtraction in pointer offset, bounds calculation, or size accounting. Risk of integer overflow bypassing bounds checks.",
        "regex": r"\b(ptr_offset|mem_load|mem_store|volatile_store|volatile_load)\s*\(\s*[^,]+,\s*([^,)]*[\+\-][^,)]*)\s*,"
    },
    {
        "id": "unchecked_shift_left",
        "description": "Left shift operator that may cause signed overflow or undefined behavior if the shift count is out of range or operands are not explicitly cast to unsigned.",
        "regex": r"\b[a-zA-Z_]\w*\s*(<<|<<=)\s*[a-zA-Z0-9_\+\-\*\/\(\)]+"
    },
    {
        "id": "potential_division_by_zero",
        "description": "Modulo or division operator using a non-literal right operand. Risk of division-by-zero crashes.",
        "regex": r"(?<!/)/[^/=\*][a-zA-Z0-9_\(\)]*|%\s*[a-zA-Z_]\w*"
    }
]

def scan_file(filepath):
    results = []
    filename = os.path.relpath(filepath, ROOT_DIR)
    
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()
        
    current_function = "unknown"
    function_decl_pattern = re.compile(r"^\s*(?:[a-zA-Z_]\w*\s+)+([a-zA-Z_]\w*)\s*\([^\)]*\)\s*\{?")
    
    for i, line in enumerate(lines):
        line_num = i + 1
        stripped = line.strip()
        
        # Simple heuristic to track current function
        func_match = function_decl_pattern.match(line)
        if func_match:
            current_function = func_match.group(1)
            
        # Ignore comments
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
            
        for pattern in RISKY_PATTERNS:
            matches = re.finditer(pattern["regex"], line)
            for match in matches:
                matched_expr = match.group(0)
                matched_groups = match.groups()
                
                # Exclude obvious safe literals
                if pattern["id"] == "potential_division_by_zero":
                    # Check if right side is a pure number or literal digit
                    is_safe = False
                    # Extract the characters immediately following / or % to see if they are literal digits
                    for operator in ["/", "%"]:
                        parts = stripped.split(operator)
                        if len(parts) > 1:
                            for part in parts[1:]:
                                tokens = part.strip().split()
                                if tokens:
                                    token = tokens[0].replace(";", "").replace(")", "").replace("}", "")
                                    if token.isdigit():
                                        is_safe = True
                                        break
                    if is_safe:
                        continue
                            
                results.append({
                    "file": filename,
                    "line": line_num,
                    "function": current_function,
                    "rule_id": pattern["id"],
                    "description": pattern["description"],
                    "matched_code": stripped,
                    "extracted": matched_expr
                })
                
    return results

def main():
    print(f"Scanning runtime core directory: {RUNTIME_CORE_DIR} ...")
    all_findings = []
    
    for root, _, files in os.walk(RUNTIME_CORE_DIR):
        for file in files:
            if file.endswith((".c", ".h")):
                filepath = os.path.join(root, file)
                findings = scan_file(filepath)
                all_findings.extend(findings)
                
    # Group findings by file and rule
    print(f"Scan complete. Found {len(all_findings)} potential risky math operations.")
    
    os.makedirs(os.path.dirname(OUTPUT_REPORT_PATH), exist_ok=True)
    with open(OUTPUT_REPORT_PATH, "w", encoding="utf-8") as f:
        json.dump(all_findings, f, indent=4)
        
    print(f"Report successfully saved to: {OUTPUT_REPORT_PATH}")
    
    # Summarize top findings
    summary = {}
    for f in all_findings:
        summary[f["rule_id"]] = summary.get(f["rule_id"], 0) + 1
    print("\nSummary of Risks:")
    for rule, count in summary.items():
        print(f" - {rule}: {count}")

if __name__ == "__main__":
    main()
