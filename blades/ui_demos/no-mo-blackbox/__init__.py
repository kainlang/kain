"""
no-mo-blackbox — Kain UI Component Forensics Kit
=================================================
Importable from Kain via: `import no_mo_blackbox.run_forensics as forensics`

Provides:
  run_forensics.run_full_pipeline("component.exe") → UnifiedReport
  run_forensics.run_suite("smoke") → List[UnifiedReport]
  vtable_tracer.trace_exe("component.exe") → VtableTrace
  crash_forensics.analyze_exe("component.exe") → CrashReport
  hang_detector.detect_hang("component.exe") → HangReport
  blank_analyzer.analyze_blank("component.exe") → BlankReport
"""

__version__ = "1.0.0"
__description__ = "Comprehensive de-blackbox debugging kit for Kain UI component executables"

# Re-export key symbols for convenient import
from .run_forensics import (
    run_full_pipeline,
    run_suite,
    run_all_kn_files,
    write_unified_report,
    build_kn,
    UnifiedReport,
)

from .vtable_tracer import (
    trace_exe,
    write_trace_output,
    VtableTrace,
    VtableCall,
)

from .crash_forensics import (
    analyze_exe,
    write_crash_report,
    CrashReport,
    CrashContext,
    CrashLocation,
)

from .hang_detector import (
    detect_hang,
    write_hang_report,
    HangReport,
    ThreadSample,
    StackFrame,
)

from .blank_analyzer import (
    analyze_blank,
    analyze_image_file,
    write_blank_report,
    BlankReport,
    PixelAnalysis,
)

__all__ = [
    # Master runner
    "run_full_pipeline",
    "run_suite",
    "run_all_kn_files",
    "write_unified_report",
    "build_kn",
    "UnifiedReport",
    # Vtable tracer
    "trace_exe",
    "write_trace_output",
    "VtableTrace",
    "VtableCall",
    # Crash forensics
    "analyze_exe",
    "write_crash_report",
    "CrashReport",
    "CrashContext",
    "CrashLocation",
    # Hang detector
    "detect_hang",
    "write_hang_report",
    "HangReport",
    "ThreadSample",
    "StackFrame",
    # Blank analyzer
    "analyze_blank",
    "analyze_image_file",
    "write_blank_report",
    "BlankReport",
    "PixelAnalysis",
]
