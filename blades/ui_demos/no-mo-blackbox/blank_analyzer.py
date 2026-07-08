#!/usr/bin/env python3
"""
no-mo-blackbox BLANK WINDOW ANALYZER
======================================
Detects blank (unrendered) windows from Kain component .exe files.
Combines ghost harness capture with pixel analysis to determine if
a window is BLANK (solid color), PARTIAL (some content), or FULL (proper render).

STRATEGY:
  1. Launch .exe via ghost harness (from harness.py) — invisible, fullscreen
  2. Capture via PrintWindow(PW_RENDERFULLCONTENT) — raw GPU buffer
  3. Pixel analysis:
     a. Compute color histogram — if >95% same color → BLANK
     b. Identify the exact blank color → cross-reference blank_colors
     c. Count unique colors — low diversity = BLANK/PARTIAL
     d. Edge detection — zero edges = solid fill
  4. Cross-reference with vtable trace to see what draw calls were made

BLANK COLOR SIGNATURES (from taxonomy.toml):
  0xFFFFFF — White (GDI default clear)
  0x000000 — Black (Vulkan/D3D12 no-draw, DWM uninitialized)
  0xCDCDCD — Gray (uninitialized heap / debug fill)
  0xC00000 — Dark red (crash handler background)

RENDERING THRESHOLDS:
  >95% same color  → BLANK (nothing rendered)
  50-95% same      → PARTIAL (some content, not complete)
  <50% same        → FULL (proper render)

OUTPUT:
  blank_analysis_<timestamp>.md  — full analysis with color data
  blank_analysis_<timestamp>.json — machine-readable pixel data

USAGE:
  python blank_analyzer.py <component.exe>
  python blank_analyzer.py --kn <component.kn>
  python blank_analyzer.py --png <screenshot.png>     # analyze existing capture
"""

import sys, os, json, time, struct, ctypes, subprocess, argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Tuple
from collections import Counter

# ============================================================================
# CONSTANTS
# ============================================================================

BLANK_THRESHOLD = 95.0       # >95% single color = BLANK
PARTIAL_THRESHOLD = 50.0     # 50-95% single color = PARTIAL
MIN_UNIQUE_COLORS = 10       # <10 unique colors = suspicious

KNOWN_BLANK_COLORS = {
    0xFFFFFF: ("White (GDI default clear)", "Window was created but no draw calls reached the framebuffer"),
    0x000000: ("Black (Vulkan/D3D12 no-draw)", "GPU backend initialized but no shaders dispatched — no content"),
    0xCDCDCD: ("Gray (uninitialized heap)", "Framebuffer memory was allocated but never written to"),
    0xC00000: ("Dark red (crash handler)", "Application crashed before rendering — crash handler backdrop"),
    0x1A1A2E: ("Kain dark bg (#1A1A2E)", "Kain default theme background — possible but no content"),
    0xF0F0F0: ("Light gray (#F0F0F0)", "Default Windows window background — no draw calls"),
    0x2D2D2D: ("Dark gray (#2D2D2D)", "Dark mode default — no components rendered"),
}

OUTPUT_DIR = Path(os.environ.get("NO_MO_BLACKBOX_OUTPUT", Path(__file__).parent / "forensics_output"))


@dataclass
class PixelAnalysis:
    """Results of pixel-level analysis."""
    total_pixels: int
    unique_colors: int
    dominant_color_hex: str
    dominant_color_pct: float
    dominant_color_name: str
    top_colors: List[Tuple[str, int, float]]  # (hex, count, pct)
    is_solid: bool
    edge_count: int  # number of edge pixels (Sobel-like)
    verdict: str  # BLANK, PARTIAL, FULL
    confidence: str  # HIGH, MEDIUM, LOW


@dataclass
class BlankReport:
    """Complete blank window analysis report."""
    image_path: str
    exe_path: str
    timestamp: str
    width: int
    height: int
    pixel_analysis: Optional[PixelAnalysis] = None
    vtable_calls_made: List[str] = field(default_factory=list)
    expected_draw_calls: List[str] = field(default_factory=list)
    missing_draw_calls: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    verdict: str = "UNKNOWN"
    capture_method: str = "PrintWindow"


# ============================================================================
# GHOST CAPTURE — adapted from harness.py
# ============================================================================

class GhostCapturer:
    """Launch .exe in ghost mode (invisible) and capture raw framebuffer."""

    def __init__(self, exe_path: str, wait_ms: int = 3000):
        self.exe_path = os.path.abspath(exe_path)
        self.wait_ms = wait_ms

    def capture(self) -> Tuple[Optional[object], Optional[str]]:
        """
        Launch and capture. Returns (PIL.Image, error_string).
        Image is None if capture failed.
        """
        try:
            from PIL import Image
            import win32gui, win32process, win32con, win32ui
            from ctypes import windll
        except ImportError as e:
            return None, f"Missing dependency: {e}. Install: pip install pywin32 Pillow"

        ctypes.windll.user32.SetProcessDPIAware()

        try:
            # Launch
            proc = subprocess.Popen([self.exe_path])
            time.sleep(self.wait_ms / 1000.0)

            # Find window
            hwnd = self._find_window(proc.pid)
            if not hwnd:
                proc.kill()
                return None, f"No window found for PID {proc.pid}"

            # Ghost the window
            self._ghost_window(hwnd)
            time.sleep(0.5)

            # Capture
            img = self._capture_raw(hwnd)
            proc.kill()

            if img:
                return img, None
            else:
                return None, "PrintWindow capture returned empty image"

        except Exception as e:
            return None, f"Ghost capture failed: {e}"

    def _find_window(self, pid: int):
        """Find visible window for PID."""
        import win32gui, win32process
        result = []

        def callback(hwnd, result):
            if win32gui.IsWindowVisible(hwnd):
                _, found_pid = win32process.GetWindowThreadProcessId(hwnd)
                if found_pid == pid:
                    result.append(hwnd)
            return True

        win32gui.EnumWindows(callback, result)
        return result[0] if result else None

    def _ghost_window(self, hwnd):
        """Make window transparent and maximized."""
        import win32gui, win32con
        ex_style = win32gui.GetWindowLong(hwnd, win32con.GWL_EXSTYLE)
        win32gui.SetWindowLong(hwnd, win32con.GWL_EXSTYLE,
            ex_style | win32con.WS_EX_LAYERED | win32con.WS_EX_TRANSPARENT | win32con.WS_EX_TOOLWINDOW)
        win32gui.SetLayeredWindowAttributes(hwnd, 0, 1, win32con.LWA_ALPHA)
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        win32gui.SetWindowPos(hwnd, win32con.HWND_TOPMOST, 0, 0, 0, 0,
                              win32con.SWP_NOMOVE | win32con.SWP_NOSIZE | win32con.SWP_NOACTIVATE)

    def _capture_raw(self, hwnd):
        """Extract raw pixel buffer via PrintWindow."""
        import win32gui, win32con, win32ui
        from ctypes import windll
        from PIL import Image

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

        result = windll.user32.PrintWindow(hwnd, save_dc.GetSafeHdc(), 2)  # PW_RENDERFULLCONTENT

        if result != 1:
            self._cleanup(bitmap, save_dc, mfc_dc, hwnd, hwnd_dc)
            return None

        bmpinfo = bitmap.GetInfo()
        bmpstr = bitmap.GetBitmapBits(True)
        im = Image.frombuffer('RGB', (bmpinfo['bmWidth'], bmpinfo['bmHeight']),
                              bmpstr, 'raw', 'BGRX', 0, 1)
        self._cleanup(bitmap, save_dc, mfc_dc, hwnd, hwnd_dc)
        return im

    def _cleanup(self, bitmap, save_dc, mfc_dc, hwnd, hwnd_dc):
        import win32gui
        win32gui.DeleteObject(bitmap.GetHandle())
        save_dc.DeleteDC()
        mfc_dc.DeleteDC()
        win32gui.ReleaseDC(hwnd, hwnd_dc)


# ============================================================================
# PIXEL ANALYSIS ENGINE
# ============================================================================

class PixelAnalyzer:
    """Analyze pixel data to determine blank/partial/full rendering."""

    def analyze(self, image) -> PixelAnalysis:
        """
        Analyze a PIL Image. Returns PixelAnalysis with verdict.
        Works on any PIL Image — doesn't need to be from ghost capture.
        """
        from PIL import Image
        import collections

        width, height = image.size
        total = width * height

        # Convert to RGB if needed
        if image.mode != 'RGB':
            image = image.convert('RGB')

        # Sample pixels (for large images, sample every Nth pixel)
        sample_step = max(1, min(width, height) // 200)

        # Build color histogram
        color_counts: Counter = collections.Counter()
        pixels = image.load()

        for y in range(0, height, sample_step):
            for x in range(0, width, sample_step):
                r, g, b = pixels[x, y]
                # Round to nearest 8 for grouping near-identical colors
                r8 = (r // 8) * 8
                g8 = (g // 8) * 8
                b8 = (b // 8) * 8
                hex_color = (r8 << 16) | (g8 << 8) | b8
                color_counts[hex_color] += 1

        if not color_counts:
            return PixelAnalysis(
                total_pixels=0, unique_colors=0,
                dominant_color_hex="0x000000", dominant_color_pct=0.0,
                dominant_color_name="Unknown",
                top_colors=[], is_solid=True, edge_count=0,
                verdict="ERROR", confidence="LOW",
            )

        sampled = sum(color_counts.values())
        dominant_color = color_counts.most_common(1)[0]
        dominant_hex = f"0x{dominant_color[0]:06X}"
        dominant_pct = (dominant_color[1] / sampled) * 100.0
        unique = len(color_counts)

        # Top 5 colors
        top = []
        for color_val, count in color_counts.most_common(5):
            pct = (count / sampled) * 100.0
            top.append((f"0x{color_val:06X}", count, round(pct, 1)))

        # Edge detection (simple Sobel-like gradient check)
        edge_count = self._count_edges(image, width, height, sample_step)

        # Identify the dominant color
        color_name = "Unknown"
        raw_color = dominant_color[0]
        # Check exact match
        if raw_color in KNOWN_BLANK_COLORS:
            color_name = KNOWN_BLANK_COLORS[raw_color][0]
        else:
            # Check approximate match (±8 per channel)
            for known, (name, _) in KNOWN_BLANK_COLORS.items():
                kr, kg, kb = (known >> 16) & 0xFF, (known >> 8) & 0xFF, known & 0xFF
                dr, dg, db = (raw_color >> 16) & 0xFF, (raw_color >> 8) & 0xFF, raw_color & 0xFF
                if abs(int(dr) - kr) < 16 and abs(int(dg) - kg) < 16 and abs(int(db) - kb) < 16:
                    color_name = name
                    break

        # Verdict
        is_solid = dominant_pct > BLANK_THRESHOLD
        if dominant_pct > BLANK_THRESHOLD:
            verdict = "BLANK"
            confidence = "HIGH" if dominant_pct > 98 else "MEDIUM"
        elif dominant_pct > PARTIAL_THRESHOLD:
            verdict = "PARTIAL"
            confidence = "MEDIUM"
        elif edge_count < 5 and unique < 5:
            verdict = "BLANK"  # solid but not perfectly uniform (gradients)
            confidence = "LOW"
        else:
            verdict = "FULL"
            confidence = "HIGH" if unique > 50 else "MEDIUM"

        return PixelAnalysis(
            total_pixels=total,
            unique_colors=unique,
            dominant_color_hex=dominant_hex,
            dominant_color_pct=round(dominant_pct, 2),
            dominant_color_name=color_name,
            top_colors=top,
            is_solid=is_solid,
            edge_count=edge_count,
            verdict=verdict,
            confidence=confidence,
        )

    def _count_edges(self, image, width: int, height: int, step: int) -> int:
        """Count edge pixels using simplified gradient detection."""
        pixels = image.load()
        edge_count = 0
        threshold = 30  # gradient threshold

        for y in range(1, height - 1, step):
            for x in range(1, width - 1, step):
                r0, g0, b0 = pixels[x, y]
                r1, g1, b1 = pixels[x + 1, y]
                r2, g2, b2 = pixels[x, y + 1]
                gx = abs(r0 - r1) + abs(g0 - g1) + abs(b0 - b1)
                gy = abs(r0 - r2) + abs(g0 - g2) + abs(b0 - b2)
                if (gx + gy) > threshold:
                    edge_count += 1

        return edge_count


# ============================================================================
# ANALYSIS ENGINE
# ============================================================================

class BlankAnalyzer:
    """Full blank window analysis pipeline."""

    def __init__(self, exe_path: str, wait_ms: int = 3000, vtable_trace_data: dict = None):
        self.exe_path = os.path.abspath(exe_path)
        self.wait_ms = wait_ms
        self.vtable_trace_data = vtable_trace_data or {}

    def analyze(self) -> BlankReport:
        """Run full blank analysis. Returns BlankReport."""
        report = BlankReport(
            image_path="",
            exe_path=self.exe_path,
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S"),
            width=0,
            height=0,
        )

        if not os.path.exists(self.exe_path):
            report.verdict = "EXE_NOT_FOUND"
            return report

        # Phase 1: Ghost capture
        print(f"  [blank_analyzer] Ghost capturing...")
        capturer = GhostCapturer(self.exe_path, self.wait_ms)
        img, error = capturer.capture()

        if img is None:
            report.verdict = "CAPTURE_FAILED"
            report.recommendations.append(error)
            return report

        report.width, report.height = img.size
        report.capture_method = "PrintWindow(PW_RENDERFULLCONTENT)"

        # Save capture
        ts = time.strftime("%Y%m%d_%H%M%S")
        cap_dir = OUTPUT_DIR / "blank_captures"
        cap_dir.mkdir(parents=True, exist_ok=True)
        cap_path = cap_dir / f"blank_cap_{Path(self.exe_path).stem}_{ts}.png"
        img.save(str(cap_path))
        report.image_path = str(cap_path)
        print(f"  [blank_analyzer] Capture: {img.width}x{img.height} → {cap_path}")

        # Phase 2: Pixel analysis
        print(f"  [blank_analyzer] Analyzing pixels...")
        analyzer = PixelAnalyzer()
        pixel = analyzer.analyze(img)
        report.pixel_analysis = pixel

        # Phase 3: Determine verdict
        report.verdict = pixel.verdict

        # Phase 4: Cross-reference with vtable trace
        if self.vtable_trace_data:
            self._cross_reference(report)

        # Phase 5: Build recommendations
        self._build_recommendations(report)

        return report

    def _cross_reference(self, report: BlankReport):
        """Cross-reference pixel analysis with vtable trace."""
        calls = self.vtable_trace_data.get('calls', [])
        slot_counts = self.vtable_trace_data.get('slot_counts', {})

        # Map vtable slots to draw-related operations
        draw_slots = {
            0: "session_create",
            2: "element_begin", 3: "element_end",
            4: "element_set_text",
            5: "element_set_attr_i64", 6: "element_set_attr_f64",
            7: "element_set_attr_string",
            10: "begin_frame", 11: "end_frame", 12: "present",
            15: "window_open",
        }

        for slot, name in draw_slots.items():
            if slot in slot_counts and slot_counts[slot] > 0:
                report.vtable_calls_made.append(f"slot {slot}: {name} ({slot_counts[slot]}x)")

        # Expected calls for a non-blank render
        expected = ["session_create", "window_open", "begin_frame",
                     "element_begin", "end_frame", "present"]
        for exp in expected:
            found = any(exp in c for c in report.vtable_calls_made)
            if not found:
                report.expected_draw_calls.append(exp)

        # Missing calls
        for exp in expected:
            found = any(exp in c for c in report.vtable_calls_made)
            if not found:
                report.missing_draw_calls.append(f"{exp} was never called")

    def _build_recommendations(self, report: BlankReport):
        """Generate recommendations based on pixel analysis."""
        recs = []
        pixel = report.pixel_analysis

        if not pixel:
            return

        if pixel.verdict == "BLANK":
            recs.append("Window is completely blank — no content was rendered")

            if pixel.dominant_color_hex == "0xFFFFFF":
                recs.append("White default clear: GDI framebuffer was cleared but no draw calls executed")
                recs.append("Check that `begin_frame` → `element_begin` → `end_frame` → `present` sequence is complete")
                recs.append("Verify component `render` block produces at least one JSX element")
            elif pixel.dominant_color_hex == "0x000000":
                recs.append("Black screen: GPU/Vulkan backend initialized but no shaders dispatched")
                recs.append("Check that `get_gpu_extension` (slot 18) is called and returns a valid extension")
                recs.append("Verify SPIR-V shader bundle exists and is loaded")
            elif pixel.dominant_color_hex == "0xCDCDCD":
                recs.append("Uninitialized heap memory: framebuffer was allocated but never written to")
                recs.append("This is a critical runtime error — the render backend failed to initialize")

            if report.missing_draw_calls:
                recs.append(f"Missing vtable calls: {', '.join(report.missing_draw_calls)}")

        elif pixel.verdict == "PARTIAL":
            recs.append("Window is partially rendered — some draw calls executed but content is incomplete")
            recs.append("Check that all child components are properly nested in the render tree")
            recs.append("Verify layout system is computing positions for all elements")

        report.recommendations = recs


def analyze_image_file(image_path: str) -> PixelAnalysis:
    """Analyze an existing PNG screenshot (no exe needed)."""
    from PIL import Image
    img = Image.open(image_path)
    analyzer = PixelAnalyzer()
    return analyzer.analyze(img)


# ============================================================================
# OUTPUT
# ============================================================================

def write_blank_report(report: BlankReport, output_dir: Path = None):
    """Write blank analysis report as markdown + JSON."""
    if output_dir is None:
        output_dir = OUTPUT_DIR
    output_dir.mkdir(parents=True, exist_ok=True)

    ts = time.strftime("%Y%m%d_%H%M%S")
    base = Path(report.exe_path).stem if report.exe_path else "image"

    # ── Markdown ──
    md_path = output_dir / f"blank_analysis_{base}_{ts}.md"
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(f"# Blank Window Analysis: {Path(report.exe_path).name}\n\n")
        f.write(f"**Executable:** `{report.exe_path}`  \n")
        f.write(f"**Timestamp:** {report.timestamp}  \n")
        f.write(f"**Resolution:** {report.width}x{report.height}  \n")
        f.write(f"**Capture:** `{report.image_path}`  \n")
        f.write(f"**Verdict:** **{report.verdict}**  \n\n")

        pixel = report.pixel_analysis
        if pixel:
            f.write("## Pixel Analysis\n\n")
            f.write(f"| Metric | Value |\n")
            f.write(f"|--------|-------|\n")
            f.write(f"| Total pixels | {pixel.total_pixels:,} |\n")
            f.write(f"| Unique colors | {pixel.unique_colors:,} |\n")
            f.write(f"| Dominant color | `{pixel.dominant_color_hex}` ({pixel.dominant_color_name}) |\n")
            f.write(f"| Dominant color % | {pixel.dominant_color_pct}% |\n")
            f.write(f"| Solid fill? | {'YES' if pixel.is_solid else 'NO'} |\n")
            f.write(f"| Edge pixels | {pixel.edge_count} |\n")
            f.write(f"| Confidence | {pixel.confidence} |\n\n")

            f.write("### Top Colors\n\n")
            f.write("| Color | Count | % |\n")
            f.write("|-------|-------|---|\n")
            for hex_c, count, pct in pixel.top_colors:
                f.write(f"| `{hex_c}` | {count} | {pct}% |\n")
            f.write("\n")

            # Color swatch (inline HTML-style for md viewers that support it)
            f.write(f"### Dominant Color Preview\n\n")
            f.write(f"![{pixel.dominant_color_hex}]({report.image_path})\n\n")

        if report.vtable_calls_made:
            f.write("## Vtable Calls Made\n\n")
            for c in report.vtable_calls_made:
                f.write(f"- {c}\n")
            f.write("\n")

        if report.missing_draw_calls:
            f.write("## Missing Draw Calls\n\n")
            for c in report.missing_draw_calls:
                f.write(f"- ❌ {c}\n")
            f.write("\n")

        if report.recommendations:
            f.write("## Recommendations\n\n")
            for r in report.recommendations:
                f.write(f"- {r}\n")
            f.write("\n")

    print(f"  [blank_analyzer] MD → {md_path}")

    # ── JSON ──
    json_path = output_dir / f"blank_analysis_{base}_{ts}.json"
    report_dict = {
        'exe_path': report.exe_path,
        'image_path': report.image_path,
        'timestamp': report.timestamp,
        'width': report.width,
        'height': report.height,
        'verdict': report.verdict,
        'pixel_analysis': asdict(report.pixel_analysis) if report.pixel_analysis else None,
        'vtable_calls_made': report.vtable_calls_made,
        'missing_draw_calls': report.missing_draw_calls,
        'recommendations': report.recommendations,
    }
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report_dict, f, indent=2, ensure_ascii=False, default=str)
    print(f"  [blank_analyzer] JSON → {json_path}")

    return md_path, json_path


def analyze_blank(exe_path: str, wait_ms: int = 3000, vtable_trace: dict = None) -> BlankReport:
    """Run blank window analysis on a Kain component .exe."""
    print(f"\n[BLANK_ANALYZER] Target: {exe_path}")
    analyzer = BlankAnalyzer(exe_path, wait_ms, vtable_trace)
    report = analyzer.analyze()
    print(f"[BLANK_ANALYZER] Verdict: {report.verdict} | Color: {report.pixel_analysis.dominant_color_hex if report.pixel_analysis else 'N/A'}")
    return report


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="no-mo-blackbox Blank Window Analyzer — detect unrendered Kain UI windows",
    )
    parser.add_argument("target", help="Path to .exe, .kn file, or .png image")
    parser.add_argument("--kn", action="store_true", help="Target is .kn (build first)")
    parser.add_argument("--png", action="store_true", help="Target is an existing PNG screenshot")
    parser.add_argument("--wait", type=int, default=3000, help="Wait ms for window to render (default: 3000)")
    parser.add_argument("--output", help="Output directory")

    args = parser.parse_args()
    output_dir = Path(args.output) if args.output else None

    if args.png or args.target.endswith('.png'):
        print(f"\n[BLANK_ANALYZER] Analyzing existing screenshot: {args.target}")
        pixel = analyze_image_file(args.target)
        print(f"  Dominant color: {pixel.dominant_color_hex} ({pixel.dominant_color_pct}%)")
        print(f"  Unique colors: {pixel.unique_colors}")
        print(f"  Edges detected: {pixel.edge_count}")
        print(f"  Verdict: {pixel.verdict} ({pixel.confidence})")
        return

    exe_path = args.target
    if args.kn or args.target.endswith('.kn'):
        print(f"  [build] {args.target}")
        r = subprocess.run(
            ["kain", "build", os.path.abspath(args.target), "--target", "llvm"],
            capture_output=True, text=True, timeout=120,
        )
        for line in (r.stdout + r.stderr).split('\n'):
            if '.exe' in line:
                p = line.strip()
                if os.path.exists(p):
                    exe_path = os.path.abspath(p)
                    break

    report = analyze_blank(exe_path, wait_ms=args.wait)
    write_blank_report(report, output_dir)

    print(f"\n{'='*60}")
    print(f"BLANK ANALYZER: {report.verdict}")
    if report.pixel_analysis:
        print(f"  Color: {report.pixel_analysis.dominant_color_hex}")
        print(f"  Coverage: {report.pixel_analysis.dominant_color_pct}%")
        print(f"  Edges: {report.pixel_analysis.edge_count}")
    print(f"{'='*60}")


if __name__ == "__main__":
    sys.exit(main())
