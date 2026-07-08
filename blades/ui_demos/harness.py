"""
Kain UI Ghost Harness — deterministic, invisible, 100% reliable window capture.
Uses PrintWindow(PW_RENDERFULLCONTENT) to extract the raw render buffer
before DWM compositing. The app runs fullscreen, transparent (alpha=1/255),
click-through, and invisible to the user.

Usage:
    python harness.py <kain_file.kn>          # build + ghost-capture + analyze
    python harness.py --exe <path.exe>        # ghost-capture pre-built exe
    python harness.py --folder <dir>          # batch analyze all .kn files
    python harness.py --pid <pid>             # ghost-capture already-running process

Requirements:
    - LM Studio on http://localhost:1234 with google/gemma-4-e2b loaded
    - kain CLI in PATH
    - pywin32 (pip install pywin32)
    - Pillow (pip install Pillow)
"""

import subprocess, json, base64, sys, os, time, glob, ctypes
from urllib.request import urlopen, Request
from urllib.error import URLError

# ── Win32 imports ─────────────────────────────────────────────────
try:
    import win32gui, win32process, win32con, win32ui
    from ctypes import windll
    from PIL import Image
except ImportError as e:
    print(f"MISSING DEPENDENCY: {e}")
    print("Install: pip install pywin32 Pillow")
    sys.exit(1)

# ── Configuration ────────────────────────────────────────────────
LM_STUDIO_URL = "http://localhost:1234/api/v1/chat"
MODEL_NAME = "google/gemma-4-e2b"
KAIN = "kain"
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ghost_captures")
LOGS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")

SYSTEM_PROMPT = """You are a senior UI/UX analyst evaluating Kain's native UI framework.

Kain is a new multi-paradigm systems language — its UI layer is not a library bolted onto an existing language. The `component` keyword, `world` state graphs, `entangle` reactive bindings, `patch` journaled mutations, `resonate` tripwire handlers, and `pulse` animation clocks are first-class language semantics that the compiler owns, optimizes, and verifies.

Kain is competing with SwiftUI. The bar is: Apple's trillion-dollar, 5-year-refined, GPU-accelerated, declarative UI framework. That's the FLOOR. Kain must surpass it.

When analyzing screenshots, start your response with EXACTLY ONE WORD on its own line: BLANK, CRASHED, or RENDERING. Then provide the full analysis.

1. VISUAL BREAKDOWN — Colors (exact hex), layout structure, visible elements
2. AESTHETICS — Is it modern? Polished? Does it look like SwiftUI-quality or prototype-quality?
3. HARDCODING ASSESSMENT — Does the UI look like it uses hardcoded C-level hex values and pixel sizes, or does it appear to use a proper theme/data-driven system? Are colors/sizes consistent or arbitrary?
4. SWIFTUI COMPARISON — How does this compare to what you'd expect from a SwiftUI app? What's missing?
5. RECOMMENDATIONS — What would make this feel like a first-class UI framework rather than a collection of hardcoded widgets?

Be brutally honest. Call out hardcoded-looking elements. If a SwiftUI engineer would laugh at it, say so."""

os.makedirs(OUTPUT_DIR, exist_ok=True)
ctypes.windll.user32.SetProcessDPIAware()


# ── Win32 Ghost Engine ────────────────────────────────────────────

def find_hwnd_by_pid(pid):
    """Find the primary visible window handle for a process ID."""
    hwnds = []
    def callback(hwnd, hwnds):
        if win32gui.IsWindowVisible(hwnd):
            _, found_pid = win32process.GetWindowThreadProcessId(hwnd)
            if found_pid == pid:
                hwnds.append(hwnd)
        return True
    win32gui.EnumWindows(callback, hwnds)
    return hwnds[0] if hwnds else None


def ghost_window(hwnd):
    """Make window: transparent (alpha=1), click-through, alt-tab hidden, maximized, topmost.
    GPU renders at full speed. Human sees nothing. Returns (width, height)."""
    # ── Extended styles: layered + transparent + tool window ──
    ex_style = win32gui.GetWindowLong(hwnd, win32con.GWL_EXSTYLE)
    win32gui.SetWindowLong(hwnd, win32con.GWL_EXSTYLE,
        ex_style | win32con.WS_EX_LAYERED | win32con.WS_EX_TRANSPARENT | win32con.WS_EX_TOOLWINDOW)

    # ── Alpha = 1/255: GPU MUST render, human eye sees NOTHING ──
    win32gui.SetLayeredWindowAttributes(hwnd, 0, 1, win32con.LWA_ALPHA)

    # ── Maximize + topmost ──
    win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
    win32gui.SetWindowPos(hwnd, win32con.HWND_TOPMOST, 0, 0, 0, 0,
                          win32con.SWP_NOMOVE | win32con.SWP_NOSIZE | win32con.SWP_NOACTIVATE)

    time.sleep(0.5)  # let the app re-layout for fullscreen

    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    return right - left, bottom - top


def capture_raw_buffer(hwnd):
    """Extract the UN-BLENDED raw pixel buffer via PrintWindow(PW_RENDERFULLCONTENT).
    This captures what the GPU actually rendered BEFORE DWM transparency.
    Returns PIL Image or None."""
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    width = right - left
    height = bottom - top

    if width <= 0 or height <= 0:
        return None

    hwnd_dc = win32gui.GetWindowDC(hwnd)
    mfc_dc = win32ui.CreateDCFromHandle(hwnd_dc)
    save_dc = mfc_dc.CreateCompatibleDC()

    bitmap = win32ui.CreateBitmap()
    bitmap.CreateCompatibleBitmap(mfc_dc, width, height)
    save_dc.SelectObject(bitmap)

    # FLAG 2 = PW_RENDERFULLCONTENT: rips DirectX/UI surface BEFORE alpha compositing
    result = windll.user32.PrintWindow(hwnd, save_dc.GetSafeHdc(), 2)

    if result != 1:
        # Cleanup
        win32gui.DeleteObject(bitmap.GetHandle())
        save_dc.DeleteDC()
        mfc_dc.DeleteDC()
        win32gui.ReleaseDC(hwnd, hwnd_dc)
        return None

    bmpinfo = bitmap.GetInfo()
    bmpstr = bitmap.GetBitmapBits(True)
    im = Image.frombuffer('RGB', (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
                          bmpstr, 'raw', 'BGRX', 0, 1)

    # Cleanup
    win32gui.DeleteObject(bitmap.GetHandle())
    save_dc.DeleteDC()
    mfc_dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, hwnd_dc)

    return im


# ── LM Studio ─────────────────────────────────────────────────────

def ask_gemma(prompt_text, image=None):
    """Send text + optional PIL Image to GEMMA 4. Returns model response."""
    if image:
        # Save to bytes, base64 encode
        import io
        buf = io.BytesIO()
        image.save(buf, format="PNG")
        b64 = base64.b64encode(buf.getvalue()).decode()
        payload = {
            "model": MODEL_NAME,
            "input": [
                {"type": "text", "content": prompt_text},
                {"type": "image", "data_url": f"data:image/png;base64,{b64}"}
            ],
            "system_prompt": SYSTEM_PROMPT,
        }
    else:
        payload = {
            "model": MODEL_NAME,
            "system_prompt": SYSTEM_PROMPT,
            "input": prompt_text,
        }
    try:
        req = Request(LM_STUDIO_URL, data=json.dumps(payload).encode(),
                      headers={"Content-Type": "application/json"})
        resp = urlopen(req, timeout=120)
        data = json.loads(resp.read().decode())
        for out in data.get("output", []):
            if out.get("type") == "message":
                return out.get("content", "")
        if "choices" in data:
            return data["choices"][0]["message"]["content"]
        return json.dumps(data, indent=2)[:2000]
    except URLError as e:
        return f"LM_STUDIO_DOWN: {e}"
    except Exception as e:
        return f"ERROR: {e}"


def derive_verdict(analysis):
    """Extract BLANK/CRASHED/RENDERING from model response.
    Handles both single-word first-line format and multi-section format."""
    if not analysis:
        return "NO_RESPONSE"
    upper = analysis.strip().upper()
    # Check first word of first line
    first_word = upper.split()[0].rstrip(".,;:") if upper.split() else ""
    if first_word in ("BLANK", "CRASHED", "RENDERING"):
        return first_word
    # Check for "RENDERING STATE: RENDERING" pattern (multi-section format)
    for line in upper.split("\n"):
        if "RENDERING STATE" in line and "RENDERING" in line.split("RENDERING STATE")[-1][:20]:
            return "RENDERING"
        if "CRASHED" in line[:50]:
            return "CRASHED"
    # Fallback: scan first 500 chars for verdict keywords
    for v in ("RENDERING", "BLANK", "CRASHED"):
        if v in upper[:500]:
            return v
    return "UNKNOWN"


def log_result(result, kn_source=None):
    """Write test result to structured JSON log + human-readable markdown log."""
    os.makedirs(LOGS_DIR, exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    safe_name = result.get("file", "unknown").replace(".exe", "").replace(".kn", "")
    
    # ── JSON log (machine-readable) ──
    log_entry = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "kain_source": kn_source or result.get("kain_source", ""),
        "exe_path": result.get("exe_path", ""),
        "file": result.get("file", ""),
        "verdict": result.get("verdict", "UNKNOWN"),
        "pid": result.get("pid"),
        "hwnd": result.get("hwnd"),
        "resolution": result.get("resolution", ""),
        "screenshot": result.get("screenshot", ""),
        "analysis": result.get("analysis", ""),
    }
    json_path = os.path.join(LOGS_DIR, f"{safe_name}_{ts}.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(log_entry, f, indent=2, ensure_ascii=False)
    
    # ── Markdown log (human-readable) ──
    md_path = os.path.join(LOGS_DIR, f"{safe_name}_{ts}.md")
    with open(md_path, "w", encoding="utf-8") as f:
        f.write(f"# UI Test: {result.get('file', 'unknown')}\n\n")
        f.write(f"**Date:** {time.strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"| Field | Value |\n|-------|-------|\n")
        if kn_source or result.get("kain_source"):
            f.write(f"| Kain Source | `{kn_source or result.get('kain_source')}` |\n")
        f.write(f"| Executable | `{result.get('exe_path', '')}` |\n")
        f.write(f"| PID | {result.get('pid', 'N/A')} |\n")
        f.write(f"| HWND | {result.get('hwnd', 'N/A')} |\n")
        f.write(f"| Resolution | {result.get('resolution', 'N/A')} |\n")
        f.write(f"| Screenshot | `{result.get('screenshot', '')}` |\n")
        f.write(f"| **Verdict** | **{result.get('verdict', 'UNKNOWN')}** |\n\n")
        f.write(f"## GEMMA 4 Analysis\n\n{result.get('analysis', 'No analysis available.')}\n")
    
    print(f"  [log] {json_path}")
    print(f"  [log] {md_path}")


# ── Pipeline ──────────────────────────────────────────────────────

def build(kn_file):
    """Build .kn to LLVM. Returns absolute .exe path or None."""
    kn_abs = os.path.abspath(kn_file)
    print(f"  [build] {kn_abs}")
    r = subprocess.run([KAIN, "build", kn_abs, "--target", "llvm"],
                       capture_output=True, text=True, timeout=120,
                       cwd=os.path.dirname(kn_abs))
    for line in (r.stdout + r.stderr).split("\n"):
        if ".exe" in line:
            path = line.strip()
            if path.startswith("\\\\?\\"):
                path = path[4:]
            if os.path.exists(path):
                return os.path.abspath(path)
    base = os.path.splitext(os.path.basename(kn_file))[0]
    for c in [
        f"X:/.kain/out/x86_64-windows/dev/ll/{base}/compile/{base}.exe",
        os.path.join(os.path.dirname(kn_abs), ".kain", "out", "x86_64-windows",
                     "dev", "ll", base, "compile", f"{base}.exe"),
    ]:
        if os.path.exists(c):
            return os.path.abspath(c)
    return None


def test_one(target, is_exe=False):
    """Ghost-capture + GEMMA-analyze a Kain executable."""
    name = os.path.basename(target)
    print(f"\n{'='*60}\nGHOST TEST: {name}\n{'='*60}")

    # ── Build ──
    if is_exe:
        exe_path = os.path.abspath(target)
        if not os.path.exists(exe_path):
            return {"file": name, "verdict": "EXE_NOT_FOUND"}
    else:
        exe_path = build(target)
        if not exe_path:
            return {"file": name, "verdict": "BUILD_FAILED"}
    print(f"  [exe] {exe_path}")

    # ── Launch ──
    print(f"  [launch] starting...")
    proc = subprocess.Popen([exe_path])
    time.sleep(3)  # wait for window to materialize

    # ── Find window ──
    hwnd = find_hwnd_by_pid(proc.pid)
    if not hwnd:
        print(f"  [find] NO window for PID={proc.pid}")
        proc.kill()
        return {"file": name, "pid": proc.pid, "verdict": "NO_WINDOW"}
    print(f"  [find] HWND=0x{hwnd:X}")

    # ── Ghost the window ──
    try:
        w, h = ghost_window(hwnd)
        print(f"  [ghost] {w}x{h} fullscreen, alpha=1, click-through, invisible")
    except Exception as e:
        print(f"  [ghost] FAILED: {e}")
        proc.kill()
        return {"file": name, "pid": proc.pid, "hwnd": f"0x{hwnd:X}", "verdict": "GHOST_FAILED"}

    # ── Extract raw render buffer ──
    print(f"  [capture] PrintWindow(PW_RENDERFULLCONTENT)...")
    img = capture_raw_buffer(hwnd)
    if not img:
        proc.kill()
        return {"file": name, "pid": proc.pid, "hwnd": f"0x{hwnd:X}", "verdict": "CAPTURE_FAILED"}

    # Save to disk
    ts = time.strftime("%Y%m%d_%H%M%S")
    safe_name = name.replace(".exe", "").replace(".kn", "")
    screenshot_path = os.path.join(OUTPUT_DIR, f"{safe_name}_{ts}.png")
    img.save(screenshot_path)
    print(f"  [capture] saved {img.width}x{img.height} -> {screenshot_path}")

    # ── Kill process (we have the buffer, app no longer needed) ──
    proc.kill()
    time.sleep(0.5)

    # ── GEMMA analysis ──
    print(f"  [gemma] analyzing {img.width}x{img.height}...")
    analysis = ask_gemma(f"Analyze this Kain UI application screenshot. Source: {target}", img)
    verdict = derive_verdict(analysis)
    print(f"  [gemma] verdict={verdict}")

    result = {
        "file": name,
        "kain_source": target if not is_exe else "",
        "exe_path": exe_path,
        "verdict": verdict,
        "pid": proc.pid,
        "hwnd": f"0x{hwnd:X}",
        "screenshot": screenshot_path,
        "resolution": f"{img.width}x{img.height}",
        "analysis": analysis,
    }
    log_result(result, kn_source=target if not is_exe else None)
    return result


def test_folder(folder):
    """Batch ghost-test all .kn files."""
    kn_files = sorted(glob.glob(os.path.join(folder, "*.kn")))
    if not kn_files:
        print(f"No .kn files in {folder}")
        return []
    results = []
    for i, kn in enumerate(kn_files):
        print(f"\n[{i+1}/{len(kn_files)}]")
        r = test_one(kn)
        results.append(r)
        print(f"  >>> VERDICT: {r['verdict']}")
        time.sleep(2)
    print(f"\n{'='*60}\nRESULTS: {len(results)} files\n{'='*60}")
    counts = {}
    for r in results:
        v = r["verdict"]
        counts[v] = counts.get(v, 0) + 1
        print(f"  [{v:20s}] {r['file']}")
    print(f"\n  Summary: {counts}")
    return results


# ── CLI ──────────────────────────────────────────────────────────
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    target = sys.argv[1]

    if target == "--exe":
        r = test_one(sys.argv[2], is_exe=True)
        print(f"\nVERDICT: {r['verdict']}")
        print(r.get("analysis", "")[:4000])

    elif target == "--folder":
        test_folder(sys.argv[2])

    elif target == "--pid":
        pid = int(sys.argv[2])
        hwnd = find_hwnd_by_pid(pid)
        if not hwnd:
            print(f"No window for PID={pid}")
            sys.exit(1)
        ghost_window(hwnd)
        img = capture_raw_buffer(hwnd)
        if img:
            path = os.path.join(OUTPUT_DIR, f"pid_{pid}_{time.strftime('%H%M%S')}.png")
            img.save(path)
            print(f"Captured: {path}")
            analysis = ask_gemma("Analyze this Kain UI screenshot.", img)
            print(f"\nVERDICT: {derive_verdict(analysis)}")
            print(analysis[:4000])

    elif target.endswith(".kn"):
        r = test_one(target)
        print(f"\nVERDICT: {r['verdict']}")
        print(r.get("analysis", "")[:4000])

    else:
        print(f"Unknown: {target}\n{__doc__}")
