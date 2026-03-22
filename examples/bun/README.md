# Bun TypeScript examples for `dxbridge.dll`

## Overview

This suite uses Bun FFI to load the real `dxbridge.dll` and call the published C ABI directly from TypeScript. It follows the same six-step progression used across the DXBridge v1 example families.

For `v1.3.0`, scenario 06 is the publishable additive onboarding path; scenarios 01 through 05 still demonstrate the unchanged `1.x` rendering flow.

## Included examples

- `examples/bun/dxbridge.ts` - handwritten Bun FFI bindings, DLL lookup, ABI struct encoding, and shared helpers
- `examples/bun/win32.ts` - tiny Win32 helper that creates a real `HWND` and pumps messages through Bun FFI
- `examples/bun/example01_load_version_logs.ts` - DLL loading, version decoding, and log callback lifetime
- `examples/bun/example02_enumerate_adapters.ts` - adapter enumeration using `DXBridge_EnumerateAdapters`
- `examples/bun/example03_create_device_errors.ts` - immediate native error retrieval and successful `DXBridge_CreateDevice`
- `examples/bun/example04_dx11_clear_window.ts` - real Win32 window creation plus a DX11 clear/present loop
- `examples/bun/example05_dx11_triangle.ts` - real window, shader compilation, dynamic vertex buffer, and triangle rendering
- `examples/bun/example06_capability_preflight.ts` - scenario 06: pre-init DX11/DX12 capability discovery with optional legacy comparison

## Requirements

- Windows 10 or 11
- Bun 1.3+ on `PATH`
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- the current working directory
- `examples/bun/dxbridge.dll`
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/release/Release/dxbridge.dll`
- `out/build/release/examples/Release/dxbridge.dll`
- `out/build/release/tests/Release/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```bat
bun run examples/bun/example01_load_version_logs.ts --dll D:\trabajo\TestDLL\proyect\out\build\debug\Debug\dxbridge.dll
```

## How to run

From the repository root:

```bat
bun run examples/bun/example01_load_version_logs.ts
bun run examples/bun/example02_enumerate_adapters.ts
bun run examples/bun/example03_create_device_errors.ts --backend dx11
bun run examples/bun/example04_dx11_clear_window.ts --hidden --frames 3
bun run examples/bun/example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
bun run examples/bun/example06_capability_preflight.ts
bun run examples/bun/example06_capability_preflight.ts --backend dx12 --compare-active dx12
```

From `examples/bun`:

```bat
bun run example01_load_version_logs.ts
bun run example02_enumerate_adapters.ts
bun run example03_create_device_errors.ts --debug
bun run example04_dx11_clear_window.ts --hidden --frames 3
bun run example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
bun run example06_capability_preflight.ts
bun run example06_capability_preflight.ts --backend dx11 --compare-active dx11
```

No extra package install step is required for this Bun suite because the examples only use Bun built-ins plus the local `.ts` files in this folder.

## Notes

- Every ABI struct passed into `dxbridge.dll` must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failing call on the same thread.
- The log callback must stay alive on the Bun side while native code still holds its pointer.
- Handles are represented as 64-bit values, so the shared helper uses `bigint` for `DXBDevice` and other opaque handles.
- `example05` is the triangle-rendering scenario for this runtime; unlike some other language folders, the file name here is `example05_dx11_triangle.ts` rather than `example05_dx11_moving_triangle.*`.
- DX11 swap chains need a real Win32 `HWND`. The helper creates an actual window, and `--hidden` only hides it visually.
- The Bun bindings manually encode ABI structs with 64-bit Windows alignment, so swap-chain, shader, pipeline, and viewport layouts must stay in sync with `include/dxbridge/dxbridge.h`.

## Capability discovery quick check

`DXBridge_QueryCapability()` is available through the shared `DXBridgeLibrary` helper. Scenario 06 now accepts the same flags used by the other refreshed runtimes:

- `--backend all|dx11|dx12` to scope the pre-init pass
- `--compare-active dx11|dx12` to show how legacy `DXBridge_SupportsFeature()` differs after init

The fastest onboarding path is:

```bat
bun run examples/bun/example06_capability_preflight.ts --backend all --compare-active dx11
```

That prints, before `DXBridge_Init()`:

- whether DX11 and DX12 are available on the current machine
- whether each backend exposes a debug layer or GPU validation
- adapter count, adapter software status, and max feature level for each adapter
- an optional post-init comparison showing that `DXBridge_SupportsFeature()` is still active-backend scoped

## Validation

Recommended local checks:

```bat
bun run examples/bun/example01_load_version_logs.ts
bun run examples/bun/example02_enumerate_adapters.ts
bun run examples/bun/example03_create_device_errors.ts --debug
bun run examples/bun/example04_dx11_clear_window.ts --hidden --frames 3
bun run examples/bun/example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
bun run examples/bun/example06_capability_preflight.ts
bun run examples/bun/example06_capability_preflight.ts --backend dx12 --compare-active dx12
```

## Troubleshooting

If a command fails, the most likely causes are:

- the DLL was not built yet
- `DXBRIDGE_DLL` points to the wrong file
- the process is not running in a normal Windows desktop session
- the requested backend is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine
- Bun FFI support changed and a binding needs to be adjusted

## Related documentation

- [`../README.md`](../README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../run_capability_preflight.ps1`](../run_capability_preflight.ps1)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
