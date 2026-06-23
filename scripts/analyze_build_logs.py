#!/usr/bin/env python3
"""
Kain Build Log Analyzer -- CLI with date range, repo collection, refresh.

Usage:
    python scripts/analyze_build_logs.py                          # Analyze all logs in .logs/
    python scripts/analyze_build_logs.py --from 2026-06-19 --to 2026-06-21   # Date range
    python scripts/analyze_build_logs.py --collect                # Scan repo for session JSONs, copy to .logs/
    python scripts/analyze_build_logs.py --refresh                # Collect + analyze all
    python scripts/analyze_build_logs.py --since 2026-06-20       # From date to today
    python scripts/analyze_build_logs.py --project blades/kain    # Single project only
"""
import argparse, json, os, re, shutil, sys, time
from collections import Counter, defaultdict
from datetime import datetime, timezone

REPO_ROOT = os.environ.get("KAIN_REPO_ROOT", "X:/")
LOG_DIR  = os.path.join(REPO_ROOT, ".logs")
OUT_DIR  = os.path.join(LOG_DIR, "reports")
SCRIPTS_DIR = os.path.join(REPO_ROOT, "scripts")
os.makedirs(OUT_DIR, exist_ok=True)

# ── Regex patterns ──
FILE_LINE_RE = re.compile(r'-->?\s+(.+?):(\d+)(?::(\d+))?')
ERR_CODE_RE  = re.compile(r'error\[(\w+):KAIN-(\w+)-(\d+)\]')
SPAN_RE      = re.compile(r'Span\s*\{\s*start:\s*(\d+),\s*end:\s*(\d+)\s*\}')
CASCADE_RE   = re.compile(r'Found\s+(\d+)\s+error', re.IGNORECASE)
COMPILING_RE = re.compile(r"while compiling\s+'([^']+)'")
KNOWN_DIRS   = {"packages","blades","smoketest","benchmark","stdlib","mcp"}
NOISE        = {".kain","out","x86_64-windows","dev",""}

def extract_project(ws):
    if not ws: return "unknown"
    wsc = ws.replace("\\\\?\\","")
    parts = [p for p in wsc.replace("\\","/").split("/") if p]
    for i,p in enumerate(parts):
        if p in KNOWN_DIRS and i+1 < len(parts): return f"{p}/{parts[i+1]}"
    for p in reversed(parts):
        if p and p not in NOISE: return p
    return parts[-1] if parts else "unknown"

def sev(n): return "[HIGH]" if n>=30 else ("[MED]" if n>=10 else ("[LOW]" if n>=5 else "."))

def classify(err, kain_code):
    u = err.upper()
    if kain_code=="KAIN-TYPE-0004": return "type-collision"
    if kain_code=="KAIN-TYPE-0003": return "world-no-surface"
    if kain_code=="KAIN-TYPE-0002": return "unknown-identifier"
    if kain_code=="KAIN-TYPE-0001": return "type-mismatch"
    if kain_code and "KAIN-PARSE" in kain_code:
        if "RESERVED" in u: return "parse-reserved"
        if "UNEXPECTED" in u: return "parse-unexpected"
        if "EXPECTED" in u: return "parse-expected"
        return "parse-other"
    if kain_code and "KAIN-EFFECT" in kain_code: return "effect-violation"
    if "INSTRUCTION DOES NOT DOMINATE" in u: return "ssa-dominance"
    if "USE OF UNDEFINED VALUE" in u: return "llvm-undef"
    if "UNDEFINED VARIABLE" in u and "SPAN" in u: return "scope-undefined"
    if "UNKNOWN PAYLOAD FIELD" in u: return "destructure-field"
    if "UNSUPPORTED LLVM" in u: return "llvm-unsupported"
    if "CANNOT USE NON-ACTOR" in u: return "actor-type"
    if "NATIVE-EXECUTABLE EXITED" in u or "COMMAND FAILED" in u: return "native-crash"
    if "LLD-LINK" in u or "PERMISSION DENIED" in u: return "linker"
    if "INCLUDE ROOTS" in u: return "include-roots"
    if "FILESYSTEM" in u or "READ_TEXT" in u: return "filesystem"
    if "PYTHON IMPORT" in u or "MODULENOTFOUND" in u: return "python-import"
    if "C FFI HEADER" in u: return "c-ffi-missing"
    if "TOO FEW ARGUMENTS" in u: return "c-ffi-args"
    if "TEST TASK FAILED" in u: return "test-failure"
    if "GPU" in u or "SHADER" in u or "SPIRV" in u: return "gpu-shader"
    if "STRUCT INITIALIZATION" in u: return "struct-init"
    return "other"

CLASS_INFO = {
    "unknown-identifier":"Symbol not found. Usually broken FFI import (`win_MessageBoxA`), renamed API, missing `use`.",
    "type-mismatch":"KAIN-TYPE-0001. Argument/return/assignment type doesn't match expected.",
    "type-collision":"KAIN-TYPE-0004. Type/function/const collides with existing global. Common in amalgamated combined source.",
    "world-no-surface":"KAIN-TYPE-0003. `world` missing required `surface` clause.",
    "native-crash":"Compiled binary exits non-zero. Runtime panic, missing DLL, assertion failure, or actual crash.",
    "scope-undefined":"Variable undefined during LLVM codegen. Scope tracker loses variables across block boundaries (if/while/match).",
    "ssa-dominance":"LLVM IR 'Instruction does not dominate all uses'. Missing PHI node in codegen.",
    "llvm-undef":"LLVM IR 'use of undefined value'. Label/value referenced but never defined. Usually '@f' in loops.",
    "llvm-unsupported":"Unsupported LLVM expression. Missing lowering for Break/Return-with-value/etc.",
    "destructure-field":"Unknown payload field in enum destructure. Pattern match codegen emits wrong field index.",
    "parse-expected":"Parser expected something different. Missing token or wrong syntax.",
    "parse-unexpected":"Parser hit unexpected token. Stray `;`, reserved keyword as identifier, incorrect syntax.",
    "parse-reserved":"Identifier conflicts with reserved keyword. Rename (`line`->`line_`, `default`->`default_`).",
    "parse-other":"Other parse failure.",
    "include-roots":"C `#include <header.h>` couldn't be resolved. Missing include dirs in `system_headers.toml`.",
    "python-import":"Python `import` failed. Module doesn't exist in current venv/PYTHONPATH.",
    "linker":"Linker error (lld-link). Permission denied, file locked, missing library.",
    "filesystem":"Filesystem error. Source file missing or permission denied.",
    "actor-type":"Non-actor type used as actor handle. Typechecker allowed bad cast through to codegen.",
    "c-ffi-missing":"C FFI header file doesn't exist at path.",
    "c-ffi-args":"C bridge function called with wrong argument count.",
    "test-failure":"Test assertions failed.",
    "gpu-shader":"GPU/shader compilation error. Usually compute metadata or resource binding.",
    "effect-violation":"Effect system violation. Pure function calls something with side effects.",
    "struct-init":"Struct initialization with named arguments (not supported in Kain).",
    "other":"Uncategorized. Needs manual inspection.",
}

# ═══════════════════════════════════════
# COLLECT
# ═══════════════════════════════════════
def collect_sessions():
    """Scan entire repo for session-*.json files and copy them into LOG_DIR."""
    print("[collect] Scanning repo for session JSON files...")
    found = 0
    copied = 0
    skipped = 0
    already = set(os.listdir(LOG_DIR))
    
    for root, dirs, files in os.walk(REPO_ROOT):
        # Skip .logs itself, .git, node_modules, .kain/out, target, bazel-*
        dirs[:] = [d for d in dirs if d not in (
            ".git", "node_modules", "target", ".log", "logs",
            "_b", "bazel-bin", "bazel-out", "bazel-testlogs",
        ) and not d.startswith("bazel-")]
        # Skip .kain/out subtrees (massive)
        if ".kain" in root.replace("\\","/").split("/"):
            parts = root.replace("\\","/").split("/")
            if "out" in parts:
                continue
        
        for fname in files:
            if fname.startswith("session-") and fname.endswith(".json") and not fname.endswith(".jsonl"):
                found += 1
                if fname in already:
                    skipped += 1
                    continue
                src = os.path.join(root, fname)
                dst = os.path.join(LOG_DIR, fname)
                try:
                    shutil.copy2(src, dst)
                    copied += 1
                    already.add(fname)
                    if copied % 50 == 0:
                        print(f"  ... {copied} copied, {found} found")
                except Exception as e:
                    print(f"  [WARN] Failed to copy {src}: {e}")
    
    print(f"[collect] Done: {found} found, {copied} copied, {skipped} already present")
    return copied

# ═══════════════════════════════════════
# LOAD & PARSE
# ═══════════════════════════════════════
def load_and_parse(date_from=None, date_to=None, project_filter=None):
    """Load session files, parse errors, return (session_rows, error_rows)."""
    session_rows = []
    error_rows = []
    
    files = [f for f in os.listdir(LOG_DIR) if f.startswith("session-") and f.endswith(".json") and not f.endswith(".jsonl")]
    
    for fname in sorted(files):
        path = os.path.join(LOG_DIR, fname)
        try:
            with open(path,"rt",encoding="utf-8",errors="replace") as fp:
                data = json.load(fp)
        except:
            continue
        
        status = data.get("status","unknown")
        ws = data.get("workspace_root","")
        ts = data.get("started_unix_ms",0) or data.get("finished_unix_ms",0)
        n_tasks = len(data.get("tasks",[]))
        
        # Date filter
        if ts and date_from:
            ts_sec = ts // 1000
            if ts_sec < date_from:
                continue
        if ts and date_to:
            ts_sec = ts // 1000
            if ts_sec > date_to:
                continue
        
        dt = datetime.fromtimestamp(ts//1000,tz=timezone.utc) if ts else None
        day = dt.strftime("%Y-%m-%d") if dt else "?"
        hour = dt.strftime("%H:%M") if dt else "?"
        proj = extract_project(ws)
        
        # Project filter
        if project_filter and proj != project_filter:
            continue
        
        has_errors = any(t.get("error") for t in data.get("tasks",[]))
        session_rows.append({
            "fname":fname,"proj":proj,"day":day,"ts":ts,"status":status,
            "n_tasks":n_tasks,"has_errors":has_errors,
            "size_kb":os.path.getsize(path)//1024,
        })
        
        # Zero-task infra failure
        if n_tasks==0 and status=="failed":
            error_rows.append({
                "fname":fname,"proj":proj,"day":day,"hour":hour,"ts":ts,
                "bug_class":"infra-zero-task","kain_code":None,"kain_cat":None,
                "file_path":None,"file_line":None,"file_col":None,"loc_str":"--",
                "fn_name":None,"span_start":None,"cascade_size":1,
                "summary":f"[INFRA] 0 tasks -- build driver crash. ws={ws[:80]}",
                "err_raw":"",
            })
            continue
        
        for task in data.get("tasks",[]):
            err = task.get("error","") or ""
            if not err or len(err)<10: continue
            compact = " ".join(err.split())
            
            fl = FILE_LINE_RE.search(compact)
            fp = fl.group(1).replace("\\\\?\\","") if fl else None
            fline = int(fl.group(2)) if fl else None
            fcol = int(fl.group(3)) if fl and fl.group(3) else None
            sm = SPAN_RE.search(compact)
            span_s = int(sm.group(1)) if sm else None
            fm = COMPILING_RE.search(compact)
            fn_name = fm.group(1) if fm else None
            cm = ERR_CODE_RE.search(compact)
            kcode = f"KAIN-{cm.group(2)}-{cm.group(3)}" if cm else None
            kcat = cm.group(1) if cm else None
            cam = CASCADE_RE.search(compact)
            casc = int(cam.group(1)) if cam else 1
            bc = classify(compact, kcode)
            
            if fp:
                short = fp
                for px in ["X:/blades/","X:/packages/","X:/mcp/","X:/smoketest/"]:
                    if short.startswith(px): short=short[len(px):]; break
                loc = f"{short}:{fline}" + (f":{fcol}" if fcol else "")
            elif fn_name: loc = f"fn:{fn_name}"
            elif span_s: loc = f"byte:{span_s}"
            else: loc = "--"
            
            first = err.split("\n")[0].strip()
            if first.startswith("Kain error:"): core = first.replace("Kain error: ","").strip()
            elif first.startswith("command failed:"): core = first.replace("command failed: ","").strip()
            else: core = first[:200]
            
            error_rows.append({
                "fname":fname,"proj":proj,"day":day,"hour":hour,"ts":ts,
                "bug_class":bc,"kain_code":kcode,"kain_cat":kcat,
                "file_path":fp,"file_line":fline,"file_col":fcol,"loc_str":loc,
                "fn_name":fn_name,"span_start":span_s,"cascade_size":casc,
                "summary":core[:250],"err_raw":err,
            })
    
    return session_rows, error_rows

# ═══════════════════════════════════════
# REPORT: By Project
# ═══════════════════════════════════════
def write_projects(session_rows, error_rows, suffix=""):
    real_errs = [r for r in error_rows if r["bug_class"]!="infra-zero-task"]
    
    L = []
    L.append("# Analysis by Project" + (f" ({suffix})" if suffix else ""))
    L.append(f"\n_{len(session_rows)} sessions · {len(set(r['proj'] for r in session_rows))} projects · {len(set(r['day'] for r in session_rows))} days_\n")
    
    pd = {}
    for s in session_rows:
        p = s["proj"]
        if p not in pd: pd[p]={"total":0,"failed":0,"ok":0,"infra":0,"real_errs":0,"casc":0,"first":"?","last":"?"}
        d=pd[p]; d["total"]+=1
        if s["status"]=="failed": d["failed"]+=1
        else: d["ok"]+=1
        if d["first"]=="?" or s["day"]<d["first"]: d["first"]=s["day"]
        if d["last"]=="?" or s["day"]>d["last"]: d["last"]=s["day"]
    
    for r in error_rows:
        p=r["proj"]
        if p not in pd: continue
        if r["bug_class"]=="infra-zero-task": pd[p]["infra"]+=1
        else: pd[p]["real_errs"]+=1
        if r["cascade_size"]>1: pd[p]["casc"]+=r["cascade_size"]-1
    
    ranked = sorted(pd.items(), key=lambda x:-x[1]["failed"])
    
    L.append("| # | Project | Sessions | Failed | Rate | Infra | Real Errs | Cascade | Span |")
    L.append("|---|---------|---------:|-------:|-----:|------:|----------:|--------:|------|")
    for i,(p,d) in enumerate(ranked,1):
        if d["total"]<2: continue
        rate = d["failed"]/d["total"]*100
        ico = "[DEAD]" if rate>=80 else ("[FIRE]" if rate>=60 else ("[WARN]" if rate>=40 else "[OK]"))
        cs = f"+{d['casc']}" if d["casc"] else "·"
        L.append(f"| {i} | {p} | {d['total']} | {d['failed']} | {ico} {rate:.0f}% | {d['infra']} | {d['real_errs']} | {cs} | {d['first']}->{d['last']} |")
    L.append("\n> **Infra** = build driver crashed before compiling (0 tasks). **Cascade** = false errors from one root cause.\n")
    
    L.append("## Per-Project Details\n")
    for p,d in ranked[:14]:
        if d["total"]<2: continue
        rate = d["failed"]/d["total"]*100
        L.append(f"### {p}")
        L.append(f"_{d['total']} sessions · {d['failed']} failed ({rate:.0f}%) · {d['infra']} infra · {d['real_errs']} real errors · {d['first']}->{d['last']}_\n")
        
        perrs = [r for r in real_errs if r["proj"]==p]
        cc = Counter(r["bug_class"] for r in perrs)
        if cc:
            L.append("| Bug | N | Location | Summary |")
            L.append("|-----|--:|----------|---------|")
            for bc,cnt in cc.most_common(10):
                s = next((r for r in perrs if r["bug_class"]==bc and r["loc_str"]!="--"), None)
                loc = s["loc_str"] if s else "--"
                summary = s["summary"][:80] if s else "--"
                L.append(f"| {bc} | {cnt} | `{loc}` | {summary} |")
            L.append("")
        
        cascades = [r for r in perrs if r["cascade_size"]>5]
        if cascades:
            L.append("| Cascade | Root Cause |")
            L.append("|--------:|------------|")
            for r in sorted(cascades,key=lambda x:-x["cascade_size"]):
                L.append(f"| {r['cascade_size']} errors | {r['summary'][:120]} |")
            L.append("")
        
        daily = defaultdict(lambda:{"ok":0,"fail":0,"infra":0})
        for s in session_rows:
            if s["proj"]==p:
                if s["status"]=="failed": daily[s["day"]]["fail"]+=1
                else: daily[s["day"]]["ok"]+=1
        for r in [e for e in error_rows if e["bug_class"]=="infra-zero-task"]:
            if r["proj"]==p: daily[r["day"]]["infra"]+=1
        if len(daily)>1:
            L.append("| Day | OK | Fail | Infra |")
            L.append("|-----|---:|-----:|------:|")
            for day in sorted(daily,reverse=True)[:10]:
                d2=daily[day]; L.append(f"| {day} | {d2['ok']} | {d2['fail']} | {d2['infra']} |")
            L.append("")
        L.append("---\n")
    
    fname = f"analysis_by_project{suffix}.md"
    with open(os.path.join(OUT_DIR, fname),"wt",encoding="utf-8") as f:
        f.write("\n".join(L))
    return fname, len(L)

# ═══════════════════════════════════════
# REPORT: By Error
# ═══════════════════════════════════════
def write_errors(session_rows, error_rows, suffix=""):
    real_errs = [r for r in error_rows if r["bug_class"]!="infra-zero-task"]
    infra_errs = [r for r in error_rows if r["bug_class"]=="infra-zero-task"]
    
    L = []
    L.append("# Analysis by Error & Kain Code" + (f" ({suffix})" if suffix else ""))
    L.append(f"\n_{len(error_rows)} total · {len(real_errs)} real errors · {len(infra_errs)} infra · {len(set(r['bug_class'] for r in real_errs))} bug classes · {len(set(r['kain_code'] for r in real_errs if r['kain_code']))} Kain codes_\n")
    
    # Infra section
    if infra_errs:
        L.append("## Infrastructure Failures (zero-task)\n")
        L.append(f"**{len(infra_errs)} sessions** failed with 0 compile tasks. Build driver crashed before any `.kn` compile.\n")
        L.append("| Project | Count | Example Sessions |")
        L.append("|---------|------:|-----------------|")
        for p in sorted(set(r["proj"] for r in infra_errs),key=lambda p:-sum(1 for r in infra_errs if r["proj"]==p)):
            cnt = sum(1 for r in infra_errs if r["proj"]==p)
            samples = ", ".join(sorted(set(r["fname"].replace("session-","").replace(".json","") for r in infra_errs if r["proj"]==p))[:3])
            L.append(f"| {p} | {cnt} | `{samples}` |")
        L.append("\n> **Not compiler bugs.** Check: Bazel server alive? OOM? Disk full? CI timeout? Antivirus locking?\n")
    
    L.append("---\n## Error Class Ranking\n")
    cc = Counter(r["bug_class"] for r in real_errs)
    total = len(real_errs)
    mx = cc.most_common(1)[0][1] if cc else 1
    
    L.append("| # | Error Class | Count | % | Codes |")
    L.append("|---|-------------|------:|---|-------|")
    for i,(bc,cnt) in enumerate(cc.most_common(),1):
        pct = cnt/total*100 if total else 0
        bar = "█"*(max(int(cnt/mx*25),1))
        codes = ", ".join(sorted(set(r["kain_code"] for r in real_errs if r["bug_class"]==bc and r["kain_code"])))
        L.append(f"| {i} | {sev(cnt)} {bc} | {cnt} | {pct:.1f}% {bar} | {codes or '--'} |")
    L.append("")
    
    L.append("---\n## Error Class Deep Dives\n")
    for bc,cnt in cc.most_common():
        subset = [r for r in real_errs if r["bug_class"]==bc]
        projs = Counter(r["proj"] for r in subset)
        codes = Counter(r["kain_code"] for r in subset if r["kain_code"])
        days_set = sorted(set(r["day"] for r in subset))
        info = CLASS_INFO.get(bc,"")
        
        L.append(f"### {sev(cnt)} {bc} -- {cnt}x\n")
        L.append(f">{info}\n")
        L.append(f"**Projects:** {', '.join(f'{p}({c})' for p,c in projs.most_common(8))}  ")
        if codes: L.append(f"**Kain codes:** {', '.join(f'{k}({c})' for k,c in codes.most_common())}  ")
        L.append(f"**When:** {days_set[0]} -> {days_set[-1]}\n")
        
        L.append("| # | Day | Project | Code | File:Line | Summary |")
        L.append("|---|-----|---------|------|-----------|---------|")
        for i,r in enumerate(subset[:15],1):
            kc = r["kain_code"] or "·"
            if r["file_path"]:
                fn = r["file_path"].split("/")[-1].split("\\")[-1]
                loc = f"{fn}:{r['file_line']}" + (f":{r['file_col']}" if r["file_col"] else "")
            else: loc = r["loc_str"] or "--"
            L.append(f"| {i} | {r['day']} | {r['proj']} | {kc} | `{loc}` | {r['summary'][:100]} |")
        L.append("")
        if len(subset)>15: L.append(f"> *+{len(subset)-15} more*\n")
        L.append("---\n")
    
    # Kain Code Reference
    coded = [r for r in real_errs if r["kain_code"]]
    if coded:
        L.append("## Kain Error Code Reference\n")
        cs = {}
        for r in coded:
            kc = r["kain_code"]
            if kc not in cs: cs[kc]={"cnt":0,"projs":set(),"classes":set(),"first":"?","last":"?"}
            cs[kc]["cnt"]+=1; cs[kc]["projs"].add(r["proj"]); cs[kc]["classes"].add(r["bug_class"])
            if cs[kc]["first"]=="?" or r["day"]<cs[kc]["first"]: cs[kc]["first"]=r["day"]
            if cs[kc]["last"]=="?" or r["day"]>cs[kc]["last"]: cs[kc]["last"]=r["day"]
        
        L.append("| Code | Cat | Count | Projects | Bug Classes | First | Last |")
        L.append("|------|-----|------:|----------|-------------|-------|------|")
        for kc in sorted(cs):
            d=cs[kc]; cat=kc.split("-")[1]
            L.append(f"| {sev(d['cnt'])} {kc} | {cat} | {d['cnt']} | {len(d['projs'])} | {', '.join(sorted(d['classes']))} | {d['first']} | {d['last']} |")
        L.append("")
        
        L.append("### Kain Code × Project Matrix\n")
        codes_list = sorted(cs)
        pc = defaultdict(lambda: defaultdict(int))
        for r in coded: pc[r["proj"]][r["kain_code"]]+=1
        pr = sorted(pc,key=lambda p:-sum(pc[p].values()))
        
        header = "| Project |"+"|".join(f" {c} " for c in codes_list)+"|"
        sep = "|---------|"+"|".join(":--:" for _ in codes_list)+"|"
        L.append(header); L.append(sep)
        for p in pr[:15]:
            vals = "|".join(f" **{pc[p][c]}** " if pc[p][c] else " · " for c in codes_list)
            L.append(f"| {p[:30]} |{vals}|")
        L.append("")
    
    fname = f"analysis_by_error{suffix}.md"
    with open(os.path.join(OUT_DIR, fname),"wt",encoding="utf-8") as f:
        f.write("\n".join(L))
    return fname, len(L)

# ═══════════════════════════════════════
# MAIN
# ═══════════════════════════════════════
def main():
    parser = argparse.ArgumentParser(description="Kain Build Log Analyzer")
    parser.add_argument("--collect", action="store_true", help="Scan entire repo for session JSONs, copy to .logs/")
    parser.add_argument("--refresh", action="store_true", help="Collect + analyze all")
    parser.add_argument("--from", dest="date_from", help="Start date (YYYY-MM-DD)")
    parser.add_argument("--to", dest="date_to", help="End date (YYYY-MM-DD)")
    parser.add_argument("--since", help="From date to today (YYYY-MM-DD)")
    parser.add_argument("--project", help="Filter to single project (e.g. blades/kain)")
    parser.add_argument("--errors-only", action="store_true", help="Only generate error report, skip project report")
    parser.add_argument("--projects-only", action="store_true", help="Only generate project report, skip error report")
    parser.add_argument("--status", action="store_true", help="Just show quick stats, no reports")
    args = parser.parse_args()
    
    # ── Collect ──
    if args.collect or args.refresh:
        n = collect_sessions()
        if args.collect and not args.refresh:
            print(f"[collect] {n} new files. Run without --collect to analyze.")
            return
    
    # ── Date parsing ──
    date_from = None
    date_to = None
    if args.date_from:
        date_from = int(datetime.strptime(args.date_from, "%Y-%m-%d").replace(tzinfo=timezone.utc).timestamp())
    if args.date_to:
        date_to = int(datetime.strptime(args.date_to, "%Y-%m-%d").replace(tzinfo=timezone.utc).timestamp()) + 86400  # end of day
    if args.since:
        date_from = int(datetime.strptime(args.since, "%Y-%m-%d").replace(tzinfo=timezone.utc).timestamp())
    
    # ── Date suffix for output files ──
    suffix = ""
    if date_from and date_to:
        d1 = datetime.fromtimestamp(date_from, tz=timezone.utc).strftime("%m%d")
        d2 = datetime.fromtimestamp(date_to-86400, tz=timezone.utc).strftime("%m%d")
        suffix = f"_{d1}-{d2}" if d1 != d2 else f"_{d1}"
    elif date_from:
        d1 = datetime.fromtimestamp(date_from, tz=timezone.utc).strftime("%m%d")
        suffix = f"_from_{d1}"
    
    # ── Load ──
    print(f"[analyze] Loading sessions...")
    session_rows, error_rows = load_and_parse(date_from, date_to, args.project)
    real_errs = [r for r in error_rows if r["bug_class"]!="infra-zero-task"]
    infra_errs = [r for r in error_rows if r["bug_class"]=="infra-zero-task"]
    
    if args.status:
        total = len(session_rows)
        failed = sum(1 for s in session_rows if s["status"]=="failed")
        ok = total - failed
        projs = len(set(s["proj"] for s in session_rows))
        days = len(set(s["day"] for s in session_rows))
        print(f"\n{'='*50}")
        print(f"Build Log Status")
        print(f"{'='*50}")
        print(f"  Sessions:   {total}")
        print(f"  Success:    {ok} ({ok/total*100:.0f}%)" if total else "  Success: 0")
        print(f"  Failed:     {failed} ({failed/total*100:.0f}%)" if total else "  Failed: 0")
        print(f"  Infra fail: {len(infra_errs)}")
        print(f"  Real errors:{len(real_errs)}")
        print(f"  Projects:   {projs}")
        print(f"  Days:       {days}")
        print(f"  Bug classes:{len(set(r['bug_class'] for r in real_errs))}")
        print(f"  Kain codes: {len(set(r['kain_code'] for r in real_errs if r['kain_code']))}")
        return
    
    # ── Generate ──
    files_written = []
    if not args.errors_only:
        fn, lines = write_projects(session_rows, error_rows, suffix)
        files_written.append((fn, lines))
    if not args.projects_only:
        fn, lines = write_errors(session_rows, error_rows, suffix)
        files_written.append((fn, lines))
    
    print(f"\n[analyze] Done. {len(session_rows)} sessions => {len(error_rows)} errors")
    for fn, lines in files_written:
        path = os.path.join(OUT_DIR, fn)
        print(f"  {fn}  ({lines} lines, {os.path.getsize(path)//1024}KB)")

if __name__ == "__main__":
    main()
