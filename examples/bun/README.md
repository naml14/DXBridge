# Bun TypeScript examples for `dxbridge.dll`

## Overview

This suite uses Bun FFI to load the real `dxbridge.dll` and call the published C ABI directly from TypeScript. It follows the same five-step progression used across the DXBridge v1 example families.

## Included examples

- `examples/bun/dxbridge.ts` - handwritten Bun FFI bindings, DLL lookup, ABI struct encoding, and shared helpers
- `examples/bun/win32.ts` - tiny Win32 helper that creates a real `HWND` and pumps messages through Bun FFI
- `examples/bun/example01_load_version_logs.ts` - DLL loading, version decoding, and log callback lifetime
- `examples/bun/example02_enumerate_adapters.ts` - adapter enumeration using `DXBridge_EnumerateAdapters`
- `examples/bun/example03_create_device_errors.ts` - immediate native error retrieval and successful `DXBridge_CreateDevice`
- `examples/bun/example04_dx11_clear_window.ts` - real Win32 window creation plus a DX11 clear/present loop
- `examples/bun/example05_dx11_triangle.ts` - real window, shader compilation, dynamic vertex buffer, and triangle rendering

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
- `build/debug/Debug/dxbridge.dll`
- `build/debug/examples/Debug/dxbridge.dll`
- `build/debug/tests/Debug/dxbridge.dll`
- `out/build/ci/Debug/dxbridge.dll`

You can override the DLL path explicitly:

```bat
bun run examples/bun/example01_load_version_logs.ts --dll D:\trabajo\TestDLL\proyect\build\debug\Debug\dxbridge.dll
```

## How to run

From the repository root:

```bat
bun run examples/bun/example01_load_version_logs.ts
bun run examples/bun/example02_enumerate_adapters.ts
bun run examples/bun/example03_create_device_errors.ts --backend dx11
bun run examples/bun/example04_dx11_clear_window.ts --hidden --frames 3
bun run examples/bun/example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
```

From `examples/bun`:

```bat
bun run example01_load_version_logs.ts
bun run example02_enumerate_adapters.ts
bun run example03_create_device_errors.ts --debug
bun run example04_dx11_clear_window.ts --hidden --frames 3
bun run example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
```

## Notes

- Every ABI struct passed into `dxbridge.dll` must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failing call on the same thread.
- The log callback must stay alive on the Bun side while native code still holds its pointer.
- Handles are represented as 64-bit values, so the shared helper uses `bigint` for `DXBDevice` and other opaque handles.
- DX11 swap chains need a real Win32 `HWND`. The helper creates an actual window, and `--hidden` only hides it visually.
- The Bun bindings manually encode ABI structs with 64-bit Windows alignment, so swap-chain, shader, pipeline, and viewport layouts must stay in sync with `include/dxbridge/dxbridge.h`.

## Validation

Recommended local checks:

```bat
bun run examples/bun/example01_load_version_logs.ts
bun run examples/bun/example02_enumerate_adapters.ts
bun run examples/bun/example03_create_device_errors.ts --debug
bun run examples/bun/example04_dx11_clear_window.ts --hidden --frames 3
bun run examples/bun/example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
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
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
