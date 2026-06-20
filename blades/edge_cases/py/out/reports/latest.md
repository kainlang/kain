# Python Interop Test Report

- **Generated:** 20260620T032751Z

## Build

| Metric | Value |
|--------|-------|
| kain check passed | True |
| Files checked | 6 |
| Files passed | 6 |

## Summary

| Metric | Value |
|--------|-------|
| Total tests | 31 |
| Passed | 31 |
| Failed | 0 |
| Coverage | 100.0% |

## 17 tests in cause.kn

| Status | Test | Cat | Description |
|--------|------|-----|-------------|
| PASS | `cause_sanity` | Base Sanity | Verifies all imports resolve and modules compile correctly |
| PASS | `test_venv_exists_no_path` | Venv Lifecycle | venv_exists returns false for a path that does not exist |
| PASS | `test_venv_from_path_resolves` | Venv Lifecycle | venv_from_path returns a PythonVenv with expected fields |
| PASS | `test_venv_current_not_set` | Venv Lifecycle | venv_current() returns a PythonVenv descriptor |
| PASS | `test_import_numpy_as_np` | Import Resolution | 'import numpy as np' compiles; np is a usable Any binding |
| PASS | `test_import_math_as_py_math` | Import Resolution | 'import math as py_math' compiles; py_math.pi typechecks |
| PASS | `test_from_math_import_sqrt` | Import Resolution | 'from math import sqrt as py_sqrt' compiles; py_sqrt(16.0) typechecks |
| PASS | `test_py_call_basic` | Call Patterns | python_call / py_call signature typechecks |
| PASS | `test_py_call_raw_trunc` | Call Patterns | py_call_raw_f64_trunc_i64 (Any, Float) -> Int compiles |
| PASS | `test_py_getattr_raw` | Call Patterns | python_getattr_raw / py_getattr_raw typechecks |
| PASS | `test_py_setattr_raw` | Call Patterns | python_setattr / py_setattr syntax compiles |
| PASS | `test_py_hasattr` | Call Patterns | python_hasattr / py_hasattr typechecks |
| PASS | `test_region_begin_end` | Region API | python_region_begin / python_region_end pair typechecks |
| PASS | `test_region_import_cached` | Region API | python_region_import + cache counters typecheck |
| PASS | `test_region_getattr` | Region API | python_region_getattr_raw typechecks |
| PASS | `test_region_call` | Region API | python_region_call_raw + attr_raw_f64_trunc_i64 typecheck |
| PASS | `test_region_telemetry` | Region API | import/attr/view/call counters typecheck |

## 8 tests in spookymagic.kn

| Status | Test | Cat | Description |
|--------|------|-----|-------------|
| PASS | `buffer_view_checksum37` | Buffer/View | python_region_buffer_view_checksum37 typechecks |
| PASS | `buffer_view_raw` | Buffer/View | python_region_buffer_view typechecks |
| PASS | `buffer_materialization` | Buffer/View | kain_image_from_py / kain_tensor_from_py / shared_buffer / geometry typecheck |
| PASS | `float_to_int_truncation` | Data Marshaling | py_call_raw_f64_trunc_i64 return type is Int |
| PASS | `ndarray_to_buffer_probe` | Data Marshaling | py_buffer_info + py_buffer_bytes typecheck |
| PASS | `tensor_info_probe` | Data Marshaling | py_tensor_info + py_tensor_bytes + py_tensor_view typecheck |
| PASS | `image_probe` | Data Marshaling | py_image_info + py_image_view + py_image_pixel typecheck |
| PASS | `geometry_probe` | Data Marshaling | py_geometry_info + py_geometry_vertex + py_geometry_face typecheck |

## 6 tests in effect.kn

| Status | Test | Cat | Description |
|--------|------|-----|-------------|
| PASS | `missing_module_error_path` | Error Handling | python_module_available(...) -> Bool error path typechecks |
| PASS | `wrong_attribute_error_path` | Error Handling | python_hasattr + python_getattr_raw error path typechecks |
| PASS | `type_mismatch_return` | Error Handling | to_int + py_call_raw_f64_trunc_i64 type-mismatch lanes typecheck |
| PASS | `gil_state_preserved` | Budget Safety | region begin/end + cache counters preserve GIL state contract |
| PASS | `budget_alloc_zero` | Budget Safety | budget-safe fn (Pure effect) typechecks and composes |
| PASS | `budget_lock_zero` | Budget Safety | ownership primitives stay gated from budget-safe scopes |
