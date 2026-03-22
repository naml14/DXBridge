# Go examples for `dxbridge.dll`

## Overview

This suite uses plain Go on Windows with no `cgo` and no external packages. It mirrors the same five-step progression used across the DXBridge v1 example families.

## Included examples

- `examples/go/go.mod` - standalone Go module for the samples
- `examples/go/internal/dxbridge/dxbridge.go` - minimal DLL bindings, struct definitions, error helpers, and DLL lookup
- `examples/go/internal/dxbridge/win32.go` - tiny Win32 helper for creating a real `HWND`
- `examples/go/cmd/example01_load_version_logs/main.go` - DLL loading, version decoding, and log callback lifetime
- `examples/go/cmd/example02_enumerate_adapters/main.go` - adapter enumeration
- `examples/go/cmd/example03_create_device_errors/main.go` - immediate error retrieval and successful device creation
- `examples/go/cmd/example04_dx11_clear_window/main.go` - real Win32 window creation plus a DX11 clear/present loop
- `examples/go/cmd/example05_dx11_moving_triangle/main.go` - real window, shaders, pipeline setup, and animated triangle rendering
- `examples/go/build.bat` - builds all samples
- `examples/go/run_example.bat` - runs one sample by folder name

## Requirements

- Windows 10 or 11
- Go 1.22+ on `PATH` (validated here with Go 1.26.1)
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- `examples/go/dxbridge.dll`
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```bat
go run ./cmd/example01_load_version_logs --dll D:\trabajo\TestDLL\proyect\out\build\debug\Debug\dxbridge.dll
```

## How to run

From `examples/go`:

```bat
build.bat
run_example.bat example01_load_version_logs
run_example.bat example02_enumerate_adapters
run_example.bat example03_create_device_errors --debug
run_example.bat example04_dx11_clear_window --hidden --frames 3
run_example.bat example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
```

You can also run the commands directly from `examples/go`:

```bat
go run ./cmd/example01_load_version_logs
go run ./cmd/example02_enumerate_adapters
go run ./cmd/example03_create_device_errors --backend dx11
go run ./cmd/example04_dx11_clear_window --hidden --frames 3
go run ./cmd/example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
```

## Notes

- Every ABI struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failure on the same thread.
- The DXBridge log callback uses `syscall.NewCallbackCDecl`, so these samples intentionally keep a single process-wide handler.
- DX11 swap chains need a real Win32 `HWND`. A fake integer handle is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.
- This suite is intentionally 64-bit Windows only, matching the practical setup used by the rest of the repository examples.

## Validation

Recommended local checks from `examples/go`:

```bat
go build ./...
go run ./cmd/example01_load_version_logs
go run ./cmd/example02_enumerate_adapters
go run ./cmd/example03_create_device_errors
go run ./cmd/example04_dx11_clear_window --hidden --frames 3
go run ./cmd/example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
```

## Troubleshooting

If the DX11 windowed examples fail, the most likely causes are:

- the DLL was not built yet
- the process is not running in a normal Windows desktop session
- the requested backend or adapter is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine

## Related documentation

- [`../README.md`](../README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
