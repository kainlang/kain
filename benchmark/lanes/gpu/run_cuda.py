#!/usr/bin/env python3
"""Live CUDA/PTX gauntlet runner for Kain-authored GPU kernels."""

from __future__ import annotations

import argparse
import ctypes
import json
import math
import os
import shutil
import statistics
import struct
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


GPU_ROOT = Path(__file__).resolve().parent
BENCHMARK_ROOT = GPU_ROOT.parent.parent
REPO_ROOT = BENCHMARK_ROOT.parent
OUT_ROOT = BENCHMARK_ROOT / "out"
BUILD_ROOT = OUT_ROOT / "build" / "gpu-cuda"
REPORT_ROOT = OUT_ROOT / "reports"
SNAPSHOT_ROOT = OUT_ROOT / "snapshots"
DEFAULT_MANIFEST = GPU_ROOT / "cuda_cases.json"
GPU_RUNTIME_LIBRARY_NAME = "kain_gpu_runtime.dll" if os.name == "nt" else "libkain_gpu_runtime.so"


class GpuRuntimeDispatchRequest(ctypes.Structure):
    _fields_ = [
        ("shader_bundle_path", ctypes.c_char_p),
        ("compute_residency_path", ctypes.c_char_p),
        ("compute_key", ctypes.c_char_p),
    ]


class GpuRuntimeDispatchResult(ctypes.Structure):
    _fields_ = [
        ("status_code", ctypes.c_int32),
        ("dispatch_invocations", ctypes.c_uint64),
        ("tensor_binding_count", ctypes.c_uint32),
        ("stream_binding_count", ctypes.c_uint32),
        ("neural_node_count", ctypes.c_uint32),
        ("output_binding_count", ctypes.c_uint32),
        ("total_output_bytes", ctypes.c_uint64),
        ("message", ctypes.c_char * 256),
    ]


def optional_ctypes_symbol(dll: ctypes.CDLL, name: str):
    try:
        return getattr(dll, name)
    except AttributeError:
        return None


def repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO_ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def load_manifest(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def select_cases(manifest: dict[str, Any], requested: list[str]) -> list[dict[str, Any]]:
    cases = manifest.get("cases", [])
    if not requested:
        return cases
    wanted = set(requested)
    selected = [case for case in cases if case.get("id") in wanted]
    missing = sorted(wanted.difference(case.get("id") for case in selected))
    if missing:
        raise SystemExit(f"unknown CUDA gauntlet case(s): {', '.join(missing)}")
    return selected


def resolve_kain_bin(args: argparse.Namespace) -> str:
    if args.kain_bin:
        return args.kain_bin
    if os.environ.get("KAIN_BIN"):
        return os.environ["KAIN_BIN"]
    repo_launcher = REPO_ROOT / ".kain" / "bin" / ("kain.exe" if os.name == "nt" else "kain")
    if repo_launcher.exists():
        return str(repo_launcher)
    return "kain"


def run_command(command: list[str], env: dict[str, str], cwd: Path) -> dict[str, Any]:
    start = time.perf_counter()
    proc = subprocess.run(
        command,
        cwd=str(cwd),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    return {
        "command": command,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "elapsed_ms": elapsed_ms,
    }


def artifact_paths(case_build_root: Path) -> dict[str, Path]:
    artifact_base = case_build_root / "kain"
    return {
        "artifact_base": artifact_base,
        "bundle": case_build_root / "kain.shader_bundle.json",
        "reflection": case_build_root / "kain.reflect.json",
        "rust_host": case_build_root / "kain.gpu.rs",
        "ptx": case_build_root / "kain.derived.ptx",
        "spirv": case_build_root / "kain.spv",
        "residency": case_build_root / "kain_compute_residency.json",
        "telemetry": case_build_root / "kain.cuda_telemetry.json",
    }


def summarize_ptx(ptx_text: str, tracked: list[str]) -> dict[str, Any]:
    lines = ptx_text.splitlines()
    return {
        "bytes": len(ptx_text.encode("utf-8")),
        "lines": len(lines),
        "tracked": {token: (token in ptx_text) for token in tracked},
        "target_directives": [line.strip() for line in lines if line.strip().startswith(".target")],
        "entry_count": ptx_text.count(".visible .entry"),
    }


def decode_message(value: GpuRuntimeDispatchResult) -> str:
    return bytes(value.message).split(b"\0", 1)[0].decode("utf-8", errors="replace")


def pack_u32(values: list[int]) -> bytes:
    return b"".join(struct.pack("<I", value) for value in values)


def pack_f32(values: list[float]) -> bytes:
    return b"".join(struct.pack("<f", value) for value in values)


def decode_u32s(raw: bytes) -> list[int]:
    if len(raw) % 4 != 0:
        raise ValueError(f"u32 payload length {len(raw)} is not divisible by 4")
    return [value[0] for value in struct.iter_unpack("<I", raw)]


def decode_f32s(raw: bytes) -> list[float]:
    if len(raw) % 4 != 0:
        raise ValueError(f"f32 payload length {len(raw)} is not divisible by 4")
    return [value[0] for value in struct.iter_unpack("<f", raw)]


def product(values: list[int]) -> int:
    result = 1
    for value in values:
        result *= int(value)
    return result


def contiguous_strides(shape: list[int]) -> list[int]:
    if not shape:
        return []
    strides = [1] * len(shape)
    running = 1
    for index in range(len(shape) - 1, -1, -1):
        strides[index] = running
        running *= int(shape[index])
    return strides


def build_case_runtime_plan(case: dict[str, Any]) -> dict[str, Any]:
    case_id = str(case["id"])
    params = case.get("params", {})

    if case_id == "cuda_warp_reduce_sum":
        count = int(params.get("count", 4096))
        if count <= 0 or count % 32 != 0:
            raise ValueError("cuda_warp_reduce_sum count must be a positive multiple of 32")
        input_values = [
            ((index % 23) - 11) * 0.125 + ((index // 32) % 5) * 0.25 for index in range(count)
        ]
        expected = [
            math.fsum(input_values[base : base + 32]) for base in range(0, count, 32)
        ]
        return {
            "dispatch_size": [count, 1, 1],
            "workgroup_size": list(params.get("workgroup_size", [128, 1, 1])),
            "cuda_stream_policy": str(params.get("cuda_stream_policy", "non_blocking")),
            "dynamic_shared_memory_bytes": int(params.get("dynamic_shared_memory_bytes", 0)),
            "bindings": {
                "input": {
                    "bytes": pack_f32(input_values),
                    "shape": [count],
                    "strides": [1],
                },
                "output": {
                    "bytes": bytes(len(expected) * 4),
                    "shape": [len(expected)],
                    "strides": [1],
                    "expected_kind": "f32",
                    "expected_values": expected,
                    "epsilon": 1e-4,
                },
                "count": {
                    "bytes": pack_u32([count]),
                    "shape": [1],
                    "strides": [1],
                },
            },
        }

    if case_id == "cuda_packed_embedding_gather":
        source_rows = int(params.get("source_rows", 256))
        row_count = int(params.get("row_count", 96))
        dim = int(params.get("dim", 24))
        table = [((row * 17 + col * 13 + 19) % 251) for row in range(source_rows) for col in range(dim)]
        indices = [((row * 11) + 7) % source_rows for row in range(row_count)]
        expected = [
            table[source_row * dim + col]
            for source_row in indices
            for col in range(dim)
        ]
        return {
            "dispatch_size": [row_count, dim, 1],
            "workgroup_size": list(params.get("workgroup_size", [8, 8, 1])),
            "cuda_stream_policy": str(params.get("cuda_stream_policy", "default")),
            "dynamic_shared_memory_bytes": int(params.get("dynamic_shared_memory_bytes", 0)),
            "bindings": {
                "table": {
                    "bytes": bytes(table),
                    "shape": [source_rows, dim],
                    "strides": contiguous_strides([source_rows, dim]),
                },
                "indices": {
                    "bytes": pack_u32(indices),
                    "shape": [row_count],
                    "strides": [1],
                },
                "output": {
                    "bytes": bytes(len(expected) * 4),
                    "shape": [row_count, dim],
                    "strides": contiguous_strides([row_count, dim]),
                    "expected_kind": "u32",
                    "expected_values": expected,
                },
                "row_count": {
                    "bytes": pack_u32([row_count]),
                    "shape": [1],
                    "strides": [1],
                },
                "dim": {
                    "bytes": pack_u32([dim]),
                    "shape": [1],
                    "strides": [1],
                },
            },
        }

    if case_id == "cuda_attention_score_tile":
        key_count = int(params.get("key_count", 192))
        dim = int(params.get("dim", 64))
        query = [((index % 9) - 4) * 0.125 + 0.5 for index in range(dim)]
        keys = [
            (((row * 5) + (col * 3)) % 29 - 14) * 0.0625
            for row in range(key_count)
            for col in range(dim)
        ]
        expected = [
            math.fsum(query[col] * keys[row * dim + col] for col in range(dim))
            for row in range(key_count)
        ]
        return {
            "dispatch_size": [key_count, 1, 1],
            "workgroup_size": list(params.get("workgroup_size", [64, 1, 1])),
            "cuda_stream_policy": str(params.get("cuda_stream_policy", "default")),
            "dynamic_shared_memory_bytes": int(params.get("dynamic_shared_memory_bytes", 0)),
            "bindings": {
                "query": {
                    "bytes": pack_f32(query),
                    "shape": [dim],
                    "strides": [1],
                },
                "keys": {
                    "bytes": pack_f32(keys),
                    "shape": [key_count, dim],
                    "strides": contiguous_strides([key_count, dim]),
                },
                "scores": {
                    "bytes": bytes(len(expected) * 4),
                    "shape": [key_count],
                    "strides": [1],
                    "expected_kind": "f32",
                    "expected_values": expected,
                    "epsilon": 1e-4,
                },
                "key_count": {
                    "bytes": pack_u32([key_count]),
                    "shape": [1],
                    "strides": [1],
                },
                "dim": {
                    "bytes": pack_u32([dim]),
                    "shape": [1],
                    "strides": [1],
                },
            },
        }

    raise ValueError(f"no runtime plan is defined for CUDA case {case_id}")


def prepare_case_sidecars(case: dict[str, Any], artifact_info: dict[str, Path]) -> dict[str, Any]:
    plan = build_case_runtime_plan(case)
    residency_path = artifact_info["residency"]
    residency = json.loads(residency_path.read_text(encoding="utf-8"))
    if residency.get("compute_shader_count") != 1 or len(residency.get("compute_shaders", [])) != 1:
        raise RuntimeError(
            f"CUDA gauntlet currently expects exactly one compute shader entry in {residency_path}"
        )

    entry = residency["compute_shaders"][0]
    entry["dispatch_size"] = [int(value) for value in plan["dispatch_size"]]
    entry["workgroup_size"] = [int(value) for value in plan["workgroup_size"]]
    entry["cuda_stream_policy"] = plan["cuda_stream_policy"]
    entry["dynamic_shared_memory_bytes"] = int(plan["dynamic_shared_memory_bytes"])

    binding_specs = plan["bindings"]
    binding_index = {binding["key"]: binding for binding in entry["bindings"]}
    for key, binding_plan in binding_specs.items():
        if key not in binding_index:
            raise RuntimeError(f"binding {key} was not found in compute residency for {case['id']}")
        binding = binding_index[key]
        binding["byte_length"] = len(binding_plan["bytes"])
        binding["shape"] = [int(value) for value in binding_plan["shape"]]
        binding["strides"] = [int(value) for value in binding_plan["strides"]]

    residency_path.write_text(json.dumps(residency, indent=2), encoding="utf-8")

    expected_outputs: dict[str, dict[str, Any]] = {}
    for key, binding_plan in binding_specs.items():
        binding = binding_index[key]
        payload_path = artifact_info["residency"].parent / binding["payload_file"]
        payload_path.write_bytes(binding_plan["bytes"])
        if "expected_kind" in binding_plan:
            expected_outputs[key] = {
                "kind": binding_plan["expected_kind"],
                "values": binding_plan["expected_values"],
                "epsilon": binding_plan.get("epsilon", 0.0),
                "payload_path": payload_path,
                "slot": binding["slot"],
                "byte_length": len(binding_plan["bytes"]),
            }

    return {
        "compute_key": str(entry["key"]),
        "dispatch_size": [int(value) for value in entry["dispatch_size"]],
        "workgroup_size": [int(value) for value in entry["workgroup_size"]],
        "expected_outputs": expected_outputs,
        "binding_payloads": {
            key: {
                "payload_path": artifact_info["residency"].parent / binding_index[key]["payload_file"],
                "bytes": binding_specs[key]["bytes"],
            }
            for key in binding_specs
        },
    }


def validate_expected_output(expected: dict[str, Any]) -> dict[str, Any]:
    raw = expected["payload_path"].read_bytes()
    if expected["kind"] == "u32":
        actual_values = decode_u32s(raw)
        if actual_values != expected["values"]:
            raise AssertionError(
                f"u32 output mismatch for {expected['payload_path'].name}: "
                f"expected {expected['values'][:8]}, actual {actual_values[:8]}"
            )
    elif expected["kind"] == "f32":
        actual_values = decode_f32s(raw)
        epsilon = float(expected.get("epsilon", 0.0))
        if len(actual_values) != len(expected["values"]):
            raise AssertionError(
                f"f32 output length mismatch for {expected['payload_path'].name}: "
                f"expected {len(expected['values'])}, actual {len(actual_values)}"
            )
        for index, (actual, expected_value) in enumerate(zip(actual_values, expected["values"])):
            if abs(actual - expected_value) > epsilon:
                raise AssertionError(
                    f"f32 output mismatch for {expected['payload_path'].name} at index {index}: "
                    f"actual={actual}, expected={expected_value}, epsilon={epsilon}"
                )
    else:
        raise ValueError(f"unsupported expected output kind {expected['kind']}")

    return {
        "slot": int(expected["slot"]),
        "byte_length": int(expected["byte_length"]),
        "payload_path": repo_relative(expected["payload_path"]),
    }


class CudaRuntimeLibrary:
    def __init__(self, library_path: Path):
        self.library_path = library_path
        self.dll = ctypes.CDLL(str(library_path))
        self.create_handle = optional_ctypes_symbol(
            self.dll, "kain_gpu_runtime_create_nvidia_ptx_primary"
        )
        self.destroy_handle = optional_ctypes_symbol(
            self.dll, "kain_gpu_runtime_destroy_nvidia_ptx_primary"
        )
        self.dispatch_with_handle = optional_ctypes_symbol(
            self.dll, "kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted_with_handle"
        )
        self.dispatch_once = optional_ctypes_symbol(
            self.dll, "kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted"
        )
        if self.dispatch_once is None:
            raise RuntimeError(
                f"{library_path} does not export kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted"
            )
        if self.create_handle is not None:
            self.create_handle.restype = ctypes.c_void_p
        if self.destroy_handle is not None:
            self.destroy_handle.argtypes = [ctypes.c_void_p]
            self.destroy_handle.restype = None
        self.dispatch_once.argtypes = [
            ctypes.POINTER(GpuRuntimeDispatchRequest),
            ctypes.POINTER(GpuRuntimeDispatchResult),
        ]
        self.dispatch_once.restype = ctypes.c_int32
        if self.dispatch_with_handle is not None:
            self.dispatch_with_handle.argtypes = [
                ctypes.c_void_p,
                ctypes.POINTER(GpuRuntimeDispatchRequest),
                ctypes.POINTER(GpuRuntimeDispatchResult),
            ]
            self.dispatch_with_handle.restype = ctypes.c_int32
        self.handle = None
        if (
            self.create_handle is not None
            and self.destroy_handle is not None
            and self.dispatch_with_handle is not None
        ):
            self.handle = self.create_handle()
        self.handle_reuse_enabled = self.handle is not None

    def close(self) -> None:
        if self.handle and self.destroy_handle is not None:
            self.destroy_handle(self.handle)
            self.handle = None

    def dispatch(self, shader_bundle_path: Path, compute_residency_path: Path, compute_key: str) -> dict[str, Any]:
        request = GpuRuntimeDispatchRequest(
            shader_bundle_path=str(shader_bundle_path).encode("utf-8"),
            compute_residency_path=str(compute_residency_path).encode("utf-8"),
            compute_key=compute_key.encode("utf-8"),
        )
        result = GpuRuntimeDispatchResult()
        start = time.perf_counter()
        if self.handle and self.dispatch_with_handle is not None:
            status = self.dispatch_with_handle(
                self.handle,
                ctypes.byref(request),
                ctypes.byref(result),
            )
        else:
            status = self.dispatch_once(
                ctypes.byref(request),
                ctypes.byref(result),
            )
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        return {
            "ffi_status": int(status),
            "status_code": int(result.status_code),
            "dispatch_invocations": int(result.dispatch_invocations),
            "tensor_binding_count": int(result.tensor_binding_count),
            "stream_binding_count": int(result.stream_binding_count),
            "neural_node_count": int(result.neural_node_count),
            "output_binding_count": int(result.output_binding_count),
            "total_output_bytes": int(result.total_output_bytes),
            "message": decode_message(result),
            "elapsed_ms": elapsed_ms,
        }

    @property
    def handle_api_enabled(self) -> bool:
        return self.handle_reuse_enabled


def gpu_runtime_library_candidates(kain_bin: str) -> list[Path]:
    candidates: list[Path] = []

    explicit = os.environ.get("KAIN_GPU_RUNTIME_LIBRARY")
    if explicit:
        candidates.append(Path(explicit))

    kain_path = Path(kain_bin)
    if kain_path.exists():
        for parent in [kain_path.parent, kain_path.parent / "deps"]:
            candidates.append(parent / GPU_RUNTIME_LIBRARY_NAME)

    cargo_target_dir = os.environ.get("CARGO_TARGET_DIR")
    if cargo_target_dir:
        for profile in ["debug", "release"]:
            candidates.append(Path(cargo_target_dir) / profile / GPU_RUNTIME_LIBRARY_NAME)
            candidates.append(Path(cargo_target_dir) / profile / "deps" / GPU_RUNTIME_LIBRARY_NAME)

    for profile in ["debug", "release"]:
        candidates.append(REPO_ROOT / "target" / profile / GPU_RUNTIME_LIBRARY_NAME)
        candidates.append(REPO_ROOT / "target" / profile / "deps" / GPU_RUNTIME_LIBRARY_NAME)
        candidates.append(Path(r"F:\DevTools\kain-agent\cargo-target") / profile / GPU_RUNTIME_LIBRARY_NAME)
        candidates.append(Path(r"F:\DevTools\kain-agent\cargo-target") / profile / "deps" / GPU_RUNTIME_LIBRARY_NAME)
        candidates.append(REPO_ROOT / ".kain" / "cache" / "run" / "llvm" / GPU_RUNTIME_LIBRARY_NAME)

    seen: set[str] = set()
    ordered: list[Path] = []
    for candidate in candidates:
        key = str(candidate).lower()
        if key not in seen:
            seen.add(key)
            ordered.append(candidate)
    return ordered


def probe_gpu_runtime_library(path: Path) -> dict[str, Any] | None:
    try:
        dll = ctypes.CDLL(str(path))
    except OSError:
        return None
    dispatch_once = optional_ctypes_symbol(
        dll, "kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted"
    )
    if dispatch_once is None:
        return None
    return {
        "path": path,
        "mtime_ns": path.stat().st_mtime_ns,
        "size": path.stat().st_size,
        "has_handle_api": all(
            optional_ctypes_symbol(dll, symbol) is not None
            for symbol in [
                "kain_gpu_runtime_create_nvidia_ptx_primary",
                "kain_gpu_runtime_destroy_nvidia_ptx_primary",
                "kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted_with_handle",
            ]
        ),
    }


def select_gpu_runtime_library(candidates: list[Path]) -> Path | None:
    viable = [probe for probe in (probe_gpu_runtime_library(path) for path in candidates) if probe]
    if not viable:
        return None
    viable.sort(
        key=lambda probe: (
            bool(probe["has_handle_api"]),
            int(probe["mtime_ns"]),
            int(probe["size"]),
        ),
        reverse=True,
    )
    return viable[0]["path"]


def ensure_gpu_runtime_library(args: argparse.Namespace, kain_bin: str) -> Path:
    if args.gpu_runtime_library:
        path = Path(args.gpu_runtime_library)
        if path.is_file():
            return path
        raise SystemExit(f"requested GPU runtime library does not exist: {path}")

    existing = [candidate for candidate in gpu_runtime_library_candidates(kain_bin) if candidate.is_file()]
    selected = select_gpu_runtime_library(existing)
    if selected is not None:
        return selected

    build = run_command(["cargo", "build", "-p", "kain-gpu-runtime"], os.environ.copy(), REPO_ROOT)
    if build["returncode"] != 0:
        raise SystemExit(
            "failed to build kain-gpu-runtime for the CUDA gauntlet:\n"
            + build["stdout"]
            + "\n"
            + build["stderr"]
        )
    existing = [candidate for candidate in gpu_runtime_library_candidates(kain_bin) if candidate.is_file()]
    selected = select_gpu_runtime_library(existing)
    if selected is not None:
        return selected
    raise SystemExit(f"unable to locate {GPU_RUNTIME_LIBRARY_NAME} after cargo build")


def write_case_telemetry(path: Path, payload: dict[str, Any]) -> None:
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def run_case(
    case: dict[str, Any],
    args: argparse.Namespace,
    kain_bin: str,
    runtime: CudaRuntimeLibrary,
    manifest: dict[str, Any],
) -> dict[str, Any]:
    language = "kain"
    spec = case.get("languages", {}).get(language)
    if not spec:
        return {"case": case.get("id"), "language": language, "status": "skipped", "reason": "no Kain spec"}

    shader_path = GPU_ROOT / spec["shader"]
    case_build_root = BUILD_ROOT / case["id"] / language
    if case_build_root.exists():
        shutil.rmtree(case_build_root)
    case_build_root.mkdir(parents=True, exist_ok=True)

    info = artifact_paths(case_build_root)
    env = os.environ.copy()
    env["KAIN_CUDA_ARCH"] = str(case.get("target_arch") or manifest.get("default_target_arch") or "sm_75")
    command = [
        kain_bin,
        "gpu-artifacts",
        str(shader_path),
        "--output",
        str(info["artifact_base"]),
    ]
    compile_result = run_command(command, env, REPO_ROOT)
    if compile_result["returncode"] != 0:
        return {
            "case": case["id"],
            "title": case.get("title", case["id"]),
            "language": language,
            "status": "failed",
            "shader": repo_relative(shader_path),
            "compile": compile_result,
            "failure_stage": "compile",
        }

    required_files = [info["bundle"], info["ptx"], info["residency"]]
    missing = [repo_relative(path) for path in required_files if not path.exists()]
    if missing:
        return {
            "case": case["id"],
            "title": case.get("title", case["id"]),
            "language": language,
            "status": "failed",
            "shader": repo_relative(shader_path),
            "compile": compile_result,
            "failure_stage": "artifact",
            "missing_files": missing,
        }

    runtime_plan = prepare_case_sidecars(case, info)
    ptx_text = info["ptx"].read_text(encoding="utf-8")
    ptx_summary = summarize_ptx(ptx_text, case.get("tracked_ptx", []))
    tracked_ok = all(ptx_summary["tracked"].values()) if ptx_summary["tracked"] else True
    if not tracked_ok:
        return {
            "case": case["id"],
            "title": case.get("title", case["id"]),
            "language": language,
            "status": "failed",
            "shader": repo_relative(shader_path),
            "compile": compile_result,
            "failure_stage": "ptx_validation",
            "ptx": ptx_summary,
        }

    dispatch_samples: list[dict[str, Any]] = []
    validation_records: list[dict[str, Any]] = []
    total_expected_output_bytes = sum(
        int(record["byte_length"]) for record in runtime_plan["expected_outputs"].values()
    )

    try:
        for _ in range(args.warmups):
            for payload in runtime_plan["binding_payloads"].values():
                payload["payload_path"].write_bytes(payload["bytes"])
            dispatch = runtime.dispatch(info["bundle"], info["residency"], runtime_plan["compute_key"])
            if dispatch["ffi_status"] != 0 or dispatch["status_code"] != 0:
                raise RuntimeError(dispatch["message"])
            for expected in runtime_plan["expected_outputs"].values():
                validate_expected_output(expected)

        for _ in range(args.runs):
            for payload in runtime_plan["binding_payloads"].values():
                payload["payload_path"].write_bytes(payload["bytes"])
            dispatch = runtime.dispatch(info["bundle"], info["residency"], runtime_plan["compute_key"])
            if dispatch["ffi_status"] != 0 or dispatch["status_code"] != 0:
                raise RuntimeError(dispatch["message"])
            if dispatch["dispatch_invocations"] != product(runtime_plan["dispatch_size"]):
                raise RuntimeError(
                    f"dispatch invocation mismatch: runtime reported {dispatch['dispatch_invocations']} "
                    f"but launch plan requested {product(runtime_plan['dispatch_size'])}"
                )
            if dispatch["total_output_bytes"] != total_expected_output_bytes:
                raise RuntimeError(
                    f"output byte mismatch: runtime reported {dispatch['total_output_bytes']} "
                    f"but expected {total_expected_output_bytes}"
                )
            sample_validation = [
                validate_expected_output(expected)
                for expected in runtime_plan["expected_outputs"].values()
            ]
            dispatch_samples.append(dispatch)
            validation_records.append({"outputs": sample_validation})
    except Exception as exc:
        return {
            "case": case["id"],
            "title": case.get("title", case["id"]),
            "language": language,
            "status": "failed",
            "shader": repo_relative(shader_path),
            "failure_stage": "dispatch",
            "compile": compile_result,
            "dispatch_samples": dispatch_samples,
            "error": str(exc),
            "ptx": ptx_summary,
            "bundle": repo_relative(info["bundle"]),
            "residency": repo_relative(info["residency"]),
        }

    telemetry = {
        "case": case["id"],
        "target_arch": env["KAIN_CUDA_ARCH"],
        "compile_ms": compile_result["elapsed_ms"],
        "dispatch_ms_samples": [sample["elapsed_ms"] for sample in dispatch_samples],
        "dispatch_invocations": dispatch_samples[-1]["dispatch_invocations"] if dispatch_samples else 0,
        "runtime_message": dispatch_samples[-1]["message"] if dispatch_samples else "",
        "outputs": validation_records[-1]["outputs"] if validation_records else [],
        "bundle": repo_relative(info["bundle"]),
        "residency": repo_relative(info["residency"]),
        "ptx": repo_relative(info["ptx"]),
    }
    write_case_telemetry(info["telemetry"], telemetry)

    dispatch_elapsed = [sample["elapsed_ms"] for sample in dispatch_samples]
    return {
        "case": case["id"],
        "title": case.get("title", case["id"]),
        "language": language,
        "status": "ok",
        "shader": repo_relative(shader_path),
        "bundle": repo_relative(info["bundle"]),
        "residency": repo_relative(info["residency"]),
        "telemetry": repo_relative(info["telemetry"]),
        "ptx_file": repo_relative(info["ptx"]),
        "target_arch": env["KAIN_CUDA_ARCH"],
        "runs": args.runs,
        "warmups": args.warmups,
        "compile_ms": compile_result["elapsed_ms"],
        "dispatch_ms_median": statistics.median(dispatch_elapsed) if dispatch_elapsed else 0.0,
        "dispatch_ms_min": min(dispatch_elapsed) if dispatch_elapsed else 0.0,
        "dispatch_ms_max": max(dispatch_elapsed) if dispatch_elapsed else 0.0,
        "dispatch_invocations": dispatch_samples[-1]["dispatch_invocations"] if dispatch_samples else 0,
        "runtime_message": dispatch_samples[-1]["message"] if dispatch_samples else "",
        "ptx": ptx_summary,
        "compile": compile_result,
        "dispatch_samples": dispatch_samples,
    }


def write_reports(
    results: list[dict[str, Any]],
    manifest: dict[str, Any],
    runtime: CudaRuntimeLibrary,
) -> None:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    SNAPSHOT_ROOT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report = {
        "suite": manifest.get("suite", "gpu-cuda"),
        "generated_at": stamp,
        "case_count": len(results),
        "ok_count": sum(1 for result in results if result.get("status") == "ok"),
        "runtime_library": repo_relative(runtime.library_path),
        "runtime_handle_api": runtime.handle_api_enabled,
        "results": results,
    }
    latest_json = REPORT_ROOT / "latest_cuda_gpu.json"
    stamped_json = REPORT_ROOT / f"{stamp}.cuda_gpu.json"
    latest_md = REPORT_ROOT / "latest_cuda_gpu.llm.md"
    snapshot_md = SNAPSHOT_ROOT / "latest_cuda_gpu.md"
    latest_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    stamped_json.write_text(json.dumps(report, indent=2), encoding="utf-8")

    lines = [
        "# CUDA GPU Gauntlet",
        "",
        f"- generated_at: {stamp}",
        f"- cases: {len(results)}",
        f"- ok: {report['ok_count']}",
        f"- runtime_library: {repo_relative(runtime.library_path)}",
        f"- runtime_handle_api: {runtime.handle_api_enabled}",
        "",
    ]
    for result in results:
        lines.extend(
            [
                f"## {result.get('case')}",
                "",
                f"- status: {result.get('status')}",
                f"- target_arch: {result.get('target_arch', 'n/a')}",
                f"- compile_ms: {result.get('compile_ms', 0.0):.3f}",
                f"- dispatch_ms_median: {result.get('dispatch_ms_median', 0.0):.3f}",
                f"- dispatch_invocations: {result.get('dispatch_invocations', 0)}",
                f"- ptx_bytes: {result.get('ptx', {}).get('bytes', 0)}",
                f"- ptx_entries: {result.get('ptx', {}).get('entry_count', 0)}",
                f"- runtime_message: {result.get('runtime_message', result.get('error', ''))}",
                "",
            ]
        )
    text = "\n".join(lines)
    latest_md.write_text(text, encoding="utf-8")
    snapshot_md.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the Kain CUDA/PTX GPU gauntlet")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--kain-bin", default="")
    parser.add_argument("--gpu-runtime-library", default="")
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    cases = select_cases(manifest, args.case)
    if args.list:
        for case in cases:
            print(f"{case['id']}: {case.get('title', case['id'])}")
        return 0

    kain_bin = resolve_kain_bin(args)
    runtime_library = ensure_gpu_runtime_library(args, kain_bin)
    runtime = CudaRuntimeLibrary(runtime_library)
    try:
        results = [run_case(case, args, kain_bin, runtime, manifest) for case in cases]
    finally:
        runtime.close()

    write_reports(results, manifest, runtime)
    failed = [result for result in results if result.get("status") != "ok"]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
