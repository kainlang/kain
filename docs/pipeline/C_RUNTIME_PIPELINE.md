# C Runtime Pipeline

This document captures the operational pipeline for the raw-native C runtime lane.
It exists to keep the runtime contract, validation passes, and output hygiene in one place.

## Canonical Truth Sources

- `M:\Code\Kain\runtime\native\C_RUNTIME_CONTRACT_PIPELINE.md` for the contract-first lane.
- `M:\Code\Kain\runtime\README.md` for the native runtime overview.
- `M:\Code\Kain\runtime\conformance\` for the C-level validation suite.
- `M:\Code\Kain\docs\pipeline\README.md` for the pipeline index.

## Scope

The C runtime pipeline covers:

- loading runtime bundles from JSON or environment variable input
- validating contract schema and material bindings
- hosting GPU compute payloads through the C runtime surface
- confirming viewport readiness and summary formatting

It does not cover Rust/Zig parallel runtime work. That belongs to `runtime/parallel/`.

## Core Flow

1. Author or assemble a runtime bundle JSON (material bindings, shader bundle refs, assets).
2. Load the bundle via `kain_runtime_graphics_load_from_json` or `kain_runtime_graphics_load_from_path`.
3. Validate the bundle using `kain_runtime_graphics_validate_bundle`.
4. Confirm GL/compute support with `kain_win32_gl_surface_supports_graphics_bundle`.
5. Persist summary output for review, then clean temporary files.

## Primary Validation Lane

The smoke and conformance checks live under:

- `M:\Code\Kain\runtime\conformance\graphics_runtime\`
- `M:\Code\Kain\smoketest\` for cross-lane integration

Key expected outcomes:

- schema version and target are validated
- material bindings are present and well-typed
- compute metadata is present and executable
- GL surface readiness returns true for valid bundles

## Output Hygiene

Runtime validation creates short-lived artifacts (JSON bundles, compiled outputs, caches).
Keep them disposable and out of the repo root.

Preferred locations:

- `M:\Code\Kain\generated\` for short-lived JSON bundles and runtime outputs
- `M:\Code\Kain\docs\validation\` for the few results worth keeping

Clean up after each run:

- JSON bundles created for validation (`graphics_runtime_smoke_*`)
- compiled artifacts (`.exe`, `.dll`, `.lib`, `.obj`, `.o`, `.pdb`, `.ilk`)
- build caches (`target\`, `.kain`, `.kain-runtime`)

## Edge Cases To Watch

- Relative path writes: tests that emit bundles using relative paths will drop into the working directory.
  Run those tests from `runtime\conformance\` or redirect outputs into `generated\`.
- Environment path leaks: `KAIN_RUNTIME_GRAPHICS_ENV` should be cleared after use.
- Mixed target bundles: ensure `target` is consistent with the intended runtime lane (`llvm` vs `rust`).
- Asset refs: `assets` entries should point at real file locations or stubbed test data.

## Future Improvements

- Update the conformance tests to write temporary bundles into `generated\` by default.
- Add a small script in `scripts/` to standardize runtime bundle validation runs.
