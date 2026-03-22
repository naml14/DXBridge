# Node.js JavaScript examples for `dxbridge.dll`

## Overview

This suite mirrors the Bun examples under `examples/bun`, but runs on plain Node.js and calls the real `dxbridge.dll` through `koffi`. It follows the same five-step progression used across the DXBridge v1 example families.

## Included examples

- `examples/nodejs/package.json` - local Node.js package with the `koffi` dependency and example scripts
- `examples/nodejs/dxbridge.js` - handwritten Node.js FFI bindings, DLL lookup, ABI struct encoding, and shared helpers
- `examples/nodejs/win32.js` - tiny Win32 helper that creates a real `HWND` and pumps messages through Node.js FFI
- `examples/nodejs/example01_load_version_logs.js` - DLL loading, version decoding, and log callback lifetime
- `examples/nodejs/example02_enumerate_adapters.js` - adapter enumeration using `DXBridge_EnumerateAdapters`
- `examples/nodejs/example03_create_device_errors.js` - immediate native error retrieval and successful `DXBridge_CreateDevice`
- `examples/nodejs/example04_dx11_clear_window.js` - real Win32 window creation plus a DX11 clear/present loop
- `examples/nodejs/example05_dx11_triangle.js` - real window, shader compilation, dynamic vertex buffer, and triangle rendering

## Requirements

- Windows 10 or 11
- Node.js 22+ on `PATH` (validated here with Node.js 24.11.1)
- npm on `PATH`
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- the current working directory
- `examples/nodejs/dxbridge.dll`
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/release/Release/dxbridge.dll`
- `out/build/release/examples/Release/dxbridge.dll`
- `out/build/release/tests/Release/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```bat
node examples/nodejs/example01_load_version_logs.js --dll D:\trabajo\TestDLL\proyect\out\build\debug\Debug\dxbridge.dll
```

## How to run

Install dependencies from `examples/nodejs`:

```bat
npm ci
```

This installs the exact `koffi` version recorded in `examples/nodejs/package-lock.json` inside `examples/nodejs/node_modules` and does not change the repository root.

From the repository root:

```bat
npm --prefix examples/nodejs ci
node examples/nodejs/example01_load_version_logs.js
node examples/nodejs/example02_enumerate_adapters.js
node examples/nodejs/example03_create_device_errors.js --backend dx11
node examples/nodejs/example04_dx11_clear_window.js --hidden --frames 3
node examples/nodejs/example05_dx11_triangle.js --hidden --frames 3 --sync-interval 0
```

From `examples/nodejs`:

```bat
npm run example01
npm run example02
npm run example03 -- --debug
npm run example04 -- --hidden --frames 3
npm run example05 -- --hidden --frames 3 --sync-interval 0
```

## Notes

- Every ABI struct passed into `dxbridge.dll` must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failing call on the same thread.
- The log callback and Win32 window procedure must stay alive on the Node.js side while native code still holds their function pointers.
- Handles are represented as 64-bit values, so the helper uses `bigint` for devices, swap chains, shaders, and other opaque handles.
- `example05` is the triangle-rendering scenario for this runtime; unlike some other language folders, the file name here is `example05_dx11_triangle.js` rather than `example05_dx11_moving_triangle.*`.
- DX11 swap chains need a real Win32 `HWND`. The helper creates an actual desktop window, and `--hidden` only hides it visually.
- The Node.js bindings manually encode the ABI structs so their 64-bit Windows layout stays aligned with `include/dxbridge/dxbridge.h`.

## Validation

Recommended local checks:

```bat
npm ci
node examples/nodejs/example01_load_version_logs.js
node examples/nodejs/example02_enumerate_adapters.js
node examples/nodejs/example03_create_device_errors.js --debug
node examples/nodejs/example04_dx11_clear_window.js --hidden --frames 3
node examples/nodejs/example05_dx11_triangle.js --hidden --frames 3 --sync-interval 0
```

## Troubleshooting

If a command fails, the most likely causes are:

- the DLL was not built yet
- `DXBRIDGE_DLL` points to the wrong file
- the process is not running in a normal Windows desktop session
- the requested backend is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine
- local native `koffi` binaries were not installed yet or do not match the active Node.js version

## Related documentation

- [`../README.md`](../README.md)
- [`../bun/README.md`](../bun/README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
