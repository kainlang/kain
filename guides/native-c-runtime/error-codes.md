# Error Codes

The native runtime uses stable diagnostic families with explicit numeric ranges.

## Families

- `0-999`: common
- `1000-1999`: contract
- `2000-2999`: reflection
- `3000-3999`: actor
- `4000-4999`: async
- `5000-5999`: UI
- `6000-6999`: graphics
- `7000-7999`: platform
- `8000-8999`: host bridge
- `9000-9999`: memory
- `10000-10999`: compatibility

## Most Important Codes

- `KAIN_DIAG_CODE_SUCCESS`
- `KAIN_DIAG_CODE_GENERIC_ERROR`
- `KAIN_DIAG_CODE_CONTRACT_*`
- `KAIN_DIAG_CODE_REFLECTION_*`
- `KAIN_DIAG_CODE_ACTOR_*`
- `KAIN_DIAG_CODE_ASYNC_*`
- `KAIN_DIAG_CODE_UI_*`
- `KAIN_DIAG_CODE_GFX_*`
- `KAIN_DIAG_CODE_PLATFORM_*`
- `KAIN_DIAG_CODE_HOST_BRIDGE_*`
- `KAIN_DIAG_CODE_MEMORY_*`
- `KAIN_DIAG_CODE_COMPAT_*`

## Usage Rule

Diagnostics should be structured and machine-readable. The runtime favors
explicit subsystem codes over print-only failures.
