# Go examples for `dxbridge.dll`

## Overview

This suite uses plain Go on Windows with no `cgo` and no external packages. It mirrors the same six-step progression used across the DXBridge v1 example families.

For `v1.3.0`, scenario 06 is the publishable additive onboarding path; scenarios 01 through 05 still demonstrate the unchanged `1.x` rendering flow.

## Included examples

- `examples/go/go.mod` - standalone Go module for the samples
- `examples/go/internal/dxbridge/dxbridge.go` - minimal DLL bindings, struct definitions, error helpers, and DLL lookup
- `examples/go/internal/dxbridge/win32.go` - tiny Win32 helper for creating a real `HWND`
- `examples/go/cmd/example01_load_version_logs/main.go` - DLL loading, version decoding, and log callback lifetime
- `examples/go/cmd/example02_enumerate_adapters/main.go` - adapter enumeration
- `examples/go/cmd/example03_create_device_errors/main.go` - immediate error retrieval and successful device creation
- `examples/go/cmd/example04_dx11_clear_window/main.go` - real Win32 window creation plus a DX11 clear/present loop
- `examples/go/cmd/example05_dx11_moving_triangle/main.go` - real window, shaders, pipeline setup, and animated triangle rendering
- `examples/go/cmd/example06_capability_preflight/main.go` - scenario 06: pre-init DX11/DX12 capability discovery with optional legacy comparison
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
- `out/build/release/Release/dxbridge.dll`
- `out/build/release/examples/Release/dxbridge.dll`
- `out/build/release/tests/Release/dxbridge.dll`
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
run_example.bat example06_capability_preflight
run_example.bat preflight --backend dx12 --compare-active dx12
```

You can also run the commands directly from `examples/go`:

```bat
go run ./cmd/example01_load_version_logs
go run ./cmd/example02_enumerate_adapters
go run ./cmd/example03_create_device_errors --backend dx11
go run ./cmd/example04_dx11_clear_window --hidden --frames 3
go run ./cmd/example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
go run ./cmd/example06_capability_preflight
```

## Notes

- Every ABI struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failure on the same thread.
- The DXBridge log callback uses `syscall.NewCallbackCDecl`, so these samples intentionally keep a single process-wide handler.
- DX11 swap chains need a real Win32 `HWND`. A fake integer handle is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.
- This suite is intentionally 64-bit Windows only, matching the practical setup used by the rest of the repository examples.

## Capability discovery quick check

`DXBridge_QueryCapability()` is exposed through the shared `Library` helper. Scenario 06 now accepts the same flags used by the other refreshed runtimes:

- `--backend all|dx11|dx12` to scope the pre-init pass
- `--compare-active dx11|dx12` to show how legacy `DXBridge_SupportsFeature()` differs after init

The fastest onboarding path is:

```bat
run_example.bat preflight --backend all --compare-active dx11
```

That prints, before `DXBridge_Init()`:

- whether DX11 and DX12 are available on the current machine
- whether each backend exposes a debug layer or GPU validation
- adapter count, adapter software status, and max feature level for each adapter
- an optional post-init comparison showing that `DXBridge_SupportsFeature()` is still active-backend scoped

## Validation

Recommended local checks from `examples/go`:

```bat
go build ./...
go run ./cmd/example01_load_version_logs
go run ./cmd/example02_enumerate_adapters
go run ./cmd/example03_create_device_errors
go run ./cmd/example04_dx11_clear_window --hidden --frames 3
go run ./cmd/example05_dx11_moving_triangle --hidden --frames 3 --sync-interval 0
go run ./cmd/example06_capability_preflight
go run ./cmd/example06_capability_preflight --backend dx11 --compare-active dx11
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
- [`../run_capability_preflight.ps1`](../run_capability_preflight.ps1)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
