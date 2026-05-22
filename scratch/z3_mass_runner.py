import sys
import yaml
import z3

def run_proof(yaml_file):
    print(f"--- Running Proof: {yaml_file} ---")
    with open(yaml_file, 'r') as f:
        data = yaml.safe_load(f)
    
    smt2_str = data['case']['smt2']
    s = z3.Solver()
    s.from_string(smt2_str)
    
    res = s.check()
    print(f"Result: {res}")
    if res == z3.sat:
        print("Model (Counterexample / Witness):")
        print(s.model())
    print()

run_proof(r"d:\Kain-Lang\z3\proofs\native-bitfield-set-shift-bounds.yaml")
run_proof(r"d:\Kain-Lang\z3\proofs\native-stdlib-abi-string-new-allocation-overflow.yaml")
run_proof(r"d:\Kain-Lang\z3\proofs\native-stdlib-abi-string-new-allocation-safety.yaml")
