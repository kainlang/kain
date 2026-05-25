"""Quick verification that scanned metadata loads correctly."""
import json
from pathlib import Path

metadata_dir = Path(__file__).resolve().parents[1] / "metadata"

for path in sorted(metadata_dir.iterdir()):
    fname = path.name
    if not fname.endswith("_scanned.json"):
        continue
    with path.open(encoding="utf-8") as handle:
        d = json.load(handle)
    nc = len(d.get("classes", []))
    ns = len(d.get("structs", []))
    ne = len(d.get("enums", []))
    ni = len(d.get("include_map", {}))
    print(f"\n=== {fname} ===")
    print(f"  Classes: {nc}, Structs: {ns}, Enums: {ne}, Total: {nc+ns+ne}")
    print(f"  Include map entries: {ni}")
    
    # Spot-check hierarchy
    chars = [c for c in d["classes"] if c["name"] == "ACharacter"]
    if chars:
        c = chars[0]
        print(f"  ACharacter -> parent: {c['parent']}, funcs: {len(c['functions'])}, props: {len(c['properties'])}")
    
    # Spot-check Niagara module detection
    niag = [c for c in d["classes"] if c["name"] == "UNiagaraComponent"]
    if niag:
        print(f"  UNiagaraComponent -> module: {niag[0]['module']}, header: {niag[0]['header']}")
    
    # Spot-check some structs
    vecs = [s for s in d["structs"] if s["name"] == "FVector"]
    if vecs:
        print(f"  FVector -> header: {vecs[0]['header']}, fields: {len(vecs[0].get('fields', []))}")
    
    # Count classes with parent info
    with_parent = sum(1 for c in d["classes"] if c.get("parent"))
    print(f"  Classes with parent info: {with_parent}/{nc} ({100*with_parent//max(nc,1)}%)")

print("\nDone.")
