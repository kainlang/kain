import json

report_path = r"D:\Kain-Lang\z3\reports\mass_math_analysis.json"
with open(report_path, "r", encoding="utf-8") as f:
    data = json.load(f)

print("=== ALLOCATION MULTIPLICATION OVERFLOWS ===")
for x in data:
    if x["rule_id"] == "allocation_multiplication_overflow":
        print(f'{x["file"]}:{x["line"]} in {x["function"]}: {x["matched_code"]}')

print("\n=== UNCHECKED SHIFT LEFT ===")
for x in data:
    if x["rule_id"] == "unchecked_shift_left":
        print(f'{x["file"]}:{x["line"]} in {x["function"]}: {x["matched_code"]}')
