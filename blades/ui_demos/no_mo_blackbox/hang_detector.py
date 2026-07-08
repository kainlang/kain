#!/usr/bin/env python3
"""
no-mo-blackbox HANG DETECTOR
=============================
Detects hung Kain component processes. If process exceeds timeout,
samples the call stack to identify the blocking function.

STRATEGY:
  1. Launch .exe with configurable timeout
  2. Monitor process — if it's still alive after timeout, it's stuck
  3. Sample the stack via:
     a. Toolhelp32Snapshot + StackWalk64 (dbghelp.dll) — primary
     b. Thread suspend + context capture — fallback
     c. External stack dump via procdump-like approach — last resort
  4. Match sampled symbols against known hang patterns
  5. Identify: surface_loop deadlock, actor_mailbox stall, gpu_sync, spin_loop

HANG PATTERNS (from taxonomy.toml):
  surface_loop  — Main frame loop not returning
  actor_mailbox — Actor waiting on undelivered message
  gpu_sync      — GPU fence/queue wait blocking
  spin_loop     — Unbounded while/for loop
  async_await   — Future blocked on incomplete task

OUTPUT:
  hang_report_<timestamp>.md  — full hang analysis
  hang_report_<timestamp>.json — machine-readable

USAGE:
  python hang_detector.py <component.exe>
  python hang_detector.py --kn <component.kn>
  python hang_detector.py --pid <pid> --timeout 5
"""

import sys, os, json, time, struct, ctypes, subprocess, argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Set, Tuple
from ctypes import wintypes

# ============================================================================
# CONSTANTS
# ============================================================================

TH32CS_SNAPTHREAD = 0x00000004
THREAD_SUSPEND_RESUME = 0x0002
THREAD_GET_CONTEXT = 0x0008
THREAD_QUERY_INFORMATION = 0x0040

STACKWALK_MAX_NAMELEN = 256
MAX_SYM_NAME = 2000

# Hang pattern signatures
HANG_PATTERNS = {
    "surface_loop": {
        "symbols": ["surface_frame_loop", "native_ui_surface_present", "kain_ui_runtime_pump",
                     "begin_frame", "end_frame", "present", "ui_system_begin_frame"],
        "description": "Main surface frame loop is stuck — likely deadlock in render path",
    },
    "actor_mailbox": {
        "symbols": ["kain_actor_poll", "kain_mailbox_wait", "kain_actor_receive",
                     "actor_scheduler_wait", "mailbox_dequeue"],
        "description": "Actor is waiting on a mailbox that will never deliver",
    },
    "gpu_sync": {
        "symbols": ["vkQueueWaitIdle", "vkWaitForFences", "vkAcquireNextImage",
                     "kain_gpu_sync", "d3d12_fence_wait", "IDXGISwapChain_Present"],
        "description": "GPU fence/queue wait is blocking indefinitely",
    },
    "spin_loop": {
        "symbols": [],  # absence of known blocking symbols
        "description": "No known blocking symbol found — likely unbounded while/for loop in user code",
    },
    "async_await": {
        "symbols": ["kain_future_wait", "kain_task_block_on", "kain_async_wait",
                     "future_poll", "task_wake"],
        "description": "Future is blocked on a task that never completes",
    },
}

OUTPUT_DIR = Path(os.environ.get("NO_MO_BLACKBOX_OUTPUT", Path(__file__).parent / "forensics_output"))


@dataclass
class StackFrame:
    """A single stack frame."""
    address: int
    module_name: str
    function_name: str
    source_file: str
    source_line: int
    offset: int


@dataclass
class ThreadSample:
    """Stack sample for a single thread."""
    thread_id: int
    stack_frames: List[StackFrame] = field(default_factory=list)
    thread_state: str = "unknown"


@dataclass
class HangReport:
    """Complete hang detection report."""
    exe_path: str
    pid: int
    timestamp: str
    hung: bool
    timeout_s: float
    elapsed_s: float
    pattern: str = "none"
    pattern_description: str = ""
    thread_samples: List[ThreadSample] = field(default_factory=list)
    blocked_function: str = ""
    recommendations: List[str] = field(default_factory=list)
    raw_output: str = ""
    verdict: str = "UNKNOWN"


# ============================================================================
# STACK WALKER — uses dbghelp.dll StackWalk64
# ============================================================================

class StackSampler:
    """Sample call stacks of a running process."""

    def __init__(self, pid: int):
        self.pid = pid
        self._kernel32 = ctypes.windll.kernel32
        self._dbghelp = ctypes.windll.dbghelp
        self._h_process = None
        self._symbols_loaded = False

    def __enter__(self):
        self._h_process = self._kernel32.OpenProcess(
            0x0410 | 0x0008 | 0x0010 | 0x0020 | 0x0400,  # enough rights
            False, self.pid
        )
        if self._h_process:
            self._load_symbols()
        return self

    def __exit__(self, *args):
        if self._symbols_loaded:
            self._dbghelp.SymCleanup(self._h_process)
        if self._h_process:
            self._kernel32.CloseHandle(self._h_process)

    def _load_symbols(self):
        """Initialize symbol handler for the process."""
        search_path = os.path.dirname(os.path.abspath(__file__))
        if self._dbghelp.SymInitialize(self._h_process, search_path.encode(), True):
            self._symbols_loaded = True
            # Set options for best resolution
            self._dbghelp.SymSetOptions(
                0x00000002 | 0x00000004 | 0x00000100  # SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES
            )

    def sample_all_threads(self) -> List[ThreadSample]:
        """Sample all threads in the process. Returns list of ThreadSample."""
        samples = []
        thread_ids = self._enum_threads()

        for tid in thread_ids:
            sample = self._sample_thread(tid)
            if sample:
                samples.append(sample)

        return samples

    def _enum_threads(self) -> List[int]:
        """Enumerate all thread IDs in the target process."""
        thread_ids = []

        # Create toolhelp32 snapshot
        h_snap = self._kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
        if h_snap == -1:
            return thread_ids

        class THREADENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("cntUsage", wintypes.DWORD),
                ("th32ThreadID", wintypes.DWORD),
                ("th32OwnerProcessID", wintypes.DWORD),
                ("tpBasePri", wintypes.LONG),
                ("tpDeltaPri", wintypes.LONG),
                ("dwFlags", wintypes.DWORD),
            ]

        te32 = THREADENTRY32()
        te32.dwSize = ctypes.sizeof(THREADENTRY32)

        if self._kernel32.Thread32First(h_snap, ctypes.byref(te32)):
            while True:
                if te32.th32OwnerProcessID == self.pid:
                    thread_ids.append(te32.th32ThreadID)
                if not self._kernel32.Thread32Next(h_snap, ctypes.byref(te32)):
                    break

        self._kernel32.CloseHandle(h_snap)
        return thread_ids

    def _sample_thread(self, thread_id: int) -> Optional[ThreadSample]:
        """Sample a single thread's stack."""
        sample = ThreadSample(thread_id=thread_id)

        # Open the thread
        h_thread = self._kernel32.OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
            False, thread_id
        )
        if not h_thread:
            return sample

        try:
            # Suspend the thread
            self._kernel32.SuspendThread(h_thread)

            # Get thread context
            context = self._get_thread_context(h_thread)
            if not context:
                self._kernel32.ResumeThread(h_thread)
                return sample

            # Walk the stack
            frames = self._stack_walk(h_thread, context)
            sample.stack_frames = frames

            # Resume
            self._kernel32.ResumeThread(h_thread)

        except Exception:
            try:
                self._kernel32.ResumeThread(h_thread)
            except Exception:
                pass
        finally:
            self._kernel32.CloseHandle(h_thread)

        return sample

    def _get_thread_context(self, h_thread) -> Optional[bytes]:
        """Get the CONTEXT for a suspended thread (x64)."""
        # CONTEXT structure for x64
        CONTEXT_SIZE = 1232
        context = ctypes.create_string_buffer(CONTEXT_SIZE)

        # Set ContextFlags to CONTEXT_FULL
        ctypes.c_ulong.from_buffer(context, 0).value = 0x10007  # CONTEXT_FULL
        # Set the flag at offset 48 for x64 (where RFlags must be 0x10007)
        # Actually, the ContextFlags field is at offset 0

        if self._kernel32.GetThreadContext(h_thread, context):
            return context.raw
        return None

    def _stack_walk(self, h_thread, context: bytes) -> List[StackFrame]:
        """Walk the call stack using StackWalk64."""
        frames = []

        # Parse CONTEXT to get register values
        # x64 CONTEXT layout (simplified):
        # offset 0: P1Home, P2Home, ..., ContextFlags
        # offset 0x78: Rax (0x78), Rcx (0x80), Rdx (0x88), Rbx (0x90)
        # offset 0x98: Rsp, 0xA0: Rbp, 0xA8: Rsi, 0xAC: Rdi
        # offset 0xF0: Rip, 0xF8: SegCs, 0x100: EFlags

        # Extract RIP and RSP — these are what StackWalk64 needs
        # For x64, RIP is at offset 0xF8 and RSP at offset 0x98
        rip_offset = 0xF8
        rsp_offset = 0x98
        rip = struct.unpack_from('<Q', context, rip_offset)[0]
        rsp = struct.unpack_from('<Q', context, rsp_offset)[0]

        # Create STACKFRAME64
        class STACKFRAME64(ctypes.Structure):
            _fields_ = [
                ("AddrPC", ctypes.c_ulonglong),
                ("AddrReturn", ctypes.c_ulonglong),
                ("AddrFrame", ctypes.c_ulonglong),
                ("AddrStack", ctypes.c_ulonglong),
                ("AddrBStore", ctypes.c_ulonglong),
                ("FuncTableEntry", ctypes.c_void_p),
                ("Params", ctypes.c_ulonglong * 4),
                ("Far", ctypes.c_bool),
                ("Virtual", ctypes.c_bool),
                ("Reserved", ctypes.c_ubyte * 3),
                ("KdHelp", ctypes.c_ulonglong),
            ]

        sf = STACKFRAME64()
        sf.AddrPC = rip
        sf.AddrFrame = rip  # start from RIP
        sf.AddrStack = rsp

        machine_type = 0x8664  # IMAGE_FILE_MACHINE_AMD64

        MAX_FRAMES = 64
        frame_count = 0

        while frame_count < MAX_FRAMES:
            result = self._dbghelp.StackWalk64(
                machine_type,
                self._h_process,
                h_thread,
                ctypes.byref(sf),
                context,
                None,  # ReadMemoryRoutine
                self._dbghelp.SymFunctionTableAccess64,
                self._dbghelp.SymGetModuleBase64,
                None,  # TranslateAddress
            )

            if not result or sf.AddrPC == 0:
                break

            # Resolve the symbol at this address
            frame = self._resolve_frame(sf.AddrPC)
            if frame:
                frames.append(frame)

            frame_count += 1

        return frames

    def _resolve_frame(self, address: int) -> Optional[StackFrame]:
        """Resolve an address to a StackFrame with symbol info."""
        frame = StackFrame(
            address=address,
            module_name="<unknown>",
            function_name="<unknown>",
            source_file="<unknown>",
            source_line=0,
            offset=0,
        )

        # Get module info
        module_info = ctypes.create_string_buffer(1096)  # IMAGEHLP_MODULE64
        ctypes.c_ulong.from_buffer(module_info, 0).value = 1096
        if self._dbghelp.SymGetModuleInfo64(self._h_process, address, module_info):
            module_name = ctypes.c_char_p(ctypes.addressof(module_info) + 32).value
            if module_name:
                frame.module_name = module_name.decode('utf-8', errors='replace')

        # Get symbol name
        sym_info_size = 8 + 8 + 4 + 4 + MAX_SYM_NAME
        sym_info = ctypes.create_string_buffer(sym_info_size)
        ctypes.c_ulong.from_buffer(sym_info, 0).value = 88
        displacement = ctypes.c_ulonglong(0)

        if self._dbghelp.SymFromAddr(self._h_process, address, ctypes.byref(displacement), sym_info):
            name_offset = 8 + 8 + 4 + 4
            fn_name = ctypes.c_char_p(ctypes.addressof(sym_info) + name_offset).value
            if fn_name:
                frame.function_name = fn_name.decode('utf-8', errors='replace')
                frame.offset = displacement.value

        return frame


# ============================================================================
# HANG DETECTION ENGINE
# ============================================================================

class HangDetector:
    """Launch .exe, monitor for hang, sample stacks if stuck."""

    def __init__(self, exe_path: str, timeout_s: float = 15.0):
        self.exe_path = os.path.abspath(exe_path)
        self.timeout_s = timeout_s

    def detect(self) -> HangReport:
        """Run hang detection. Returns HangReport."""
        report = HangReport(
            exe_path=self.exe_path,
            pid=0,
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S"),
            hung=False,
            timeout_s=self.timeout_s,
            elapsed_s=0.0,
        )

        if not os.path.exists(self.exe_path):
            report.verdict = "EXE_NOT_FOUND"
            return report

        t0 = time.time()

        try:
            proc = subprocess.Popen(
                [self.exe_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            report.pid = proc.pid

            try:
                stdout, stderr = proc.communicate(timeout=self.timeout_s)
                elapsed = time.time() - t0
                report.elapsed_s = elapsed

                # Process completed within timeout — no hang
                report.hung = False
                report.verdict = "NO_HANG"
                report.raw_output = stdout or ""

            except subprocess.TimeoutExpired:
                elapsed = time.time() - t0
                report.elapsed_s = elapsed
                report.hung = True

                print(f"  [hang_detector] Process survived {self.timeout_s}s timeout — HUNG")
                print(f"  [hang_detector] Sampling call stacks...")

                # Sample stacks
                try:
                    report.thread_samples = self._sample_stacks(proc.pid)
                except Exception as e:
                    report.recommendations.append(f"Stack sampling failed: {e}")

                # Kill the process
                try:
                    proc.kill()
                    proc.communicate(timeout=2)
                except Exception:
                    pass

                # Analyze the hang
                report = self._analyze_hang(report)

        except Exception as e:
            report.verdict = "DETECTOR_ERROR"
            report.recommendations.append(str(e))

        return report

    def _sample_stacks(self, pid: int) -> List[ThreadSample]:
        """Sample all thread stacks from a hung process."""
        with StackSampler(pid) as sampler:
            return sampler.sample_all_threads()

    def _analyze_hang(self, report: HangReport) -> HangReport:
        """Analyze stack samples to identify the hang pattern."""
        # Collect all function names across all threads
        all_functions: Set[str] = set()
        for sample in report.thread_samples:
            for frame in sample.stack_frames:
                all_functions.add(frame.function_name.lower())

        # Match against known patterns
        best_match = ("unknown", "No recognized blocking pattern detected")
        best_score = 0

        for pattern_name, pattern in HANG_PATTERNS.items():
            score = 0
            for sym in pattern["symbols"]:
                if any(sym.lower() in fn for fn in all_functions):
                    score += 1

            if score > best_score:
                best_score = score
                best_match = (pattern_name, pattern["description"])

        report.pattern = best_match[0]
        report.pattern_description = best_match[1]

        # Find the top-frame blocked function across threads
        for sample in report.thread_samples:
            if sample.stack_frames:
                top = sample.stack_frames[0]
                report.blocked_function = top.function_name
                break

        # Build recommendations
        recs = []
        if report.pattern == "surface_loop":
            recs.append("Check `begin_frame` / `end_frame` pairing in component render loop")
            recs.append("Verify `present` is not blocked on a GPU sync point")
            recs.append("Check for deadlock between resonate handler and surface loop")
        elif report.pattern == "actor_mailbox":
            recs.append("Verify all actors have matching `on` handlers for sent messages")
            recs.append("Check for circular `ask()` waits between actors")
            recs.append("Increase `ask()` timeout or use `send` for fire-and-forget")
        elif report.pattern == "gpu_sync":
            recs.append("Verify GPU fence/queue is signaled after dispatch")
            recs.append("Check that shader kernel completes in bounded time")
            recs.append("Ensure Vulkan/D3D12 device is not lost")
        elif report.pattern == "spin_loop":
            recs.append("Add a break condition to the while/for loop")
            recs.append("Check for unterminated `pulse` handler iterations")
            recs.append("Add a frame budget and exit after exceeding it")
        elif report.pattern == "async_await":
            recs.append("Verify the awaited future is spawned on a running executor")
            recs.append("Check for circular await chains")
            recs.append("Use `await_timeout` instead of bare `await`")

        report.recommendations = recs
        report.verdict = "HANG_DETECTED" if report.hung else "NO_HANG"

        return report


# ============================================================================
# ATTACH TO RUNNING PROCESS
# ============================================================================

def attach_and_detect(pid: int, timeout_s: float = 5.0) -> HangReport:
    """Attach to an already-running process and sample its stacks."""
    report = HangReport(
        exe_path=f"PID:{pid}",
        pid=pid,
        timestamp=time.strftime("%Y-%m-%dT%H:%M:%S"),
        hung=False,
        timeout_s=timeout_s,
        elapsed_s=0.0,
    )

    print(f"  [hang_detector] Attaching to PID {pid}...")

    try:
        report.thread_samples = []
        with StackSampler(pid) as sampler:
            report.thread_samples = sampler.sample_all_threads()

        if report.thread_samples:
            report.hung = True  # we're sampling because it was suspected hung
            report = HangDetector._analyze_hang(HangDetector(""), report)

    except Exception as e:
        report.verdict = "ATTACH_FAILED"
        report.recommendations.append(str(e))

    return report


# ============================================================================
# OUTPUT
# ============================================================================

def write_hang_report(report: HangReport, output_dir: Path = None):
    """Write hang report as markdown + JSON."""
    if output_dir is None:
        output_dir = OUTPUT_DIR
    output_dir.mkdir(parents=True, exist_ok=True)

    ts = time.strftime("%Y%m%d_%H%M%S")
    base = Path(report.exe_path).stem if report.exe_path else f"pid_{report.pid}"

    # ── Markdown ──
    md_path = output_dir / f"hang_report_{base}_{ts}.md"
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(f"# Hang Detection Report\n\n")
        f.write(f"**Executable:** `{report.exe_path}`  \n")
        f.write(f"**PID:** {report.pid}  \n")
        f.write(f"**Timestamp:** {report.timestamp}  \n")
        f.write(f"**Timeout:** {report.timeout_s}s  \n")
        f.write(f"**Elapsed:** {report.elapsed_s:.1f}s  \n")
        f.write(f"**Hung:** {'YES' if report.hung else 'NO'}  \n")
        f.write(f"**Verdict:** {report.verdict}  \n\n")

        if report.hung:
            f.write(f"## Hang Pattern: `{report.pattern}`\n\n")
            f.write(f"{report.pattern_description}\n\n")

            f.write("### Blocked Function\n\n")
            f.write(f"`{report.blocked_function}`\n\n")

            if report.recommendations:
                f.write("### Recommendations\n\n")
                for r in report.recommendations:
                    f.write(f"- {r}\n")
                f.write("\n")

            f.write(f"### Thread Samples ({len(report.thread_samples)} threads)\n\n")
            for i, sample in enumerate(report.thread_samples):
                f.write(f"#### Thread {sample.thread_id}\n\n")
                if sample.stack_frames:
                    f.write("| # | Function | Module | Address |\n")
                    f.write("|---|----------|--------|----------|\n")
                    for j, frame in enumerate(sample.stack_frames[:20]):
                        addr = hex(frame.address) if frame.address else "-"
                        f.write(f"| {j} | `{frame.function_name}` | {frame.module_name} | {addr} |\n")
                else:
                    f.write("*No stack frames captured*\n")
                f.write("\n")
        else:
            f.write("Process completed within timeout — no hang detected.\n\n")

    print(f"  [hang_detector] MD → {md_path}")

    # ── JSON ──
    json_path = output_dir / f"hang_report_{base}_{ts}.json"
    report_dict = {
        'exe_path': report.exe_path,
        'pid': report.pid,
        'timestamp': report.timestamp,
        'hung': report.hung,
        'timeout_s': report.timeout_s,
        'elapsed_s': report.elapsed_s,
        'pattern': report.pattern,
        'pattern_description': report.pattern_description,
        'blocked_function': report.blocked_function,
        'verdict': report.verdict,
        'recommendations': report.recommendations,
        'thread_samples': [
            {
                'thread_id': s.thread_id,
                'frames': [
                    {
                        'function': f.function_name,
                        'module': f.module_name,
                        'address': hex(f.address),
                        'offset': f.offset,
                    }
                    for f in s.stack_frames[:20]
                ]
            }
            for s in report.thread_samples[:8]
        ],
    }
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report_dict, f, indent=2, ensure_ascii=False, default=str)
    print(f"  [hang_detector] JSON → {json_path}")

    return md_path, json_path


def detect_hang(exe_path: str, timeout_s: float = 15.0) -> HangReport:
    """Run hang detection on a Kain component .exe."""
    print(f"\n[HANG_DETECTOR] Target: {exe_path} (timeout={timeout_s}s)")
    detector = HangDetector(exe_path, timeout_s)
    report = detector.detect()
    print(f"[HANG_DETECTOR] Result: {report.verdict} | Hung: {report.hung} | Pattern: {report.pattern}")
    return report


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="no-mo-blackbox Hang Detector — detect hung Kain component processes",
    )
    parser.add_argument("target", help="Path to .exe or .kn file")
    parser.add_argument("--kn", action="store_true", help="Target is .kn (build first)")
    parser.add_argument("--pid", type=int, help="Attach to running process by PID")
    parser.add_argument("--timeout", type=float, default=15.0, help="Hang timeout in seconds")
    parser.add_argument("--output", help="Output directory")

    args = parser.parse_args()

    if args.pid:
        report = attach_and_detect(args.pid, args.timeout)
    else:
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

        report = detect_hang(exe_path, args.timeout)

    output_dir = Path(args.output) if args.output else None
    write_hang_report(report, output_dir)

    print(f"\n{'='*60}")
    print(f"HANG DETECTOR: {report.verdict}")
    if report.hung:
        print(f"  Pattern: {report.pattern}")
        print(f"  Blocked at: {report.blocked_function}")
    print(f"{'='*60}")

    return 1 if report.hung else 0


if __name__ == "__main__":
    sys.exit(main())
