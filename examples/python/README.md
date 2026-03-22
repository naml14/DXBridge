# Python `ctypes` examples for `dxbridge.dll`

## Overview

This suite shows the minimal Python side needed to call `dxbridge.dll` through `ctypes.CDLL`. It follows the same six-step progression used across the DXBridge v1 example families.

For `v1.3.0`, scenario 06 is the publishable additive onboarding path; scenarios 01 through 05 still demonstrate the unchanged `1.x` rendering flow.

## Included examples

- `examples/python/dxbridge_ctypes.py` - small shared binding layer for the functions and structs used by the examples
- `examples/python/example_01_load_version_logs.py` - DLL loading, version decoding, and log callback lifetime
- `examples/python/example_02_enumerate_adapters.py` - adapter enumeration with the ABI two-call pattern
- `examples/python/example_03_create_device_errors.py` - immediate error retrieval and successful device creation
- `examples/python/example_04_dx11_clear_window.py` - real Win32 `HWND`, DX11 swap chain, and clear/present loop
- `examples/python/example_05_dx11_moving_triangle.py` - real Win32 `HWND`, shaders, pipeline state, and animated triangle draw loop
- `examples/python/example_06_capability_preflight.py` - scenario 06: pre-init DX11/DX12 capability discovery with optional legacy comparison

## Requirements

- Windows 10 or 11
- Python 3.10+ on `PATH` (validated here with Python 3.14)
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- the current working directory
- `examples/python/dxbridge.dll`
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/release/Release/dxbridge.dll`
- `out/build/release/examples/Release/dxbridge.dll`
- `out/build/release/tests/Release/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```powershell
python examples/python/example_01_load_version_logs.py --dll out/build/debug/Debug/dxbridge.dll
```

## How to run

From the repository root:

```powershell
python examples/python/example_01_load_version_logs.py
python examples/python/example_02_enumerate_adapters.py
python examples/python/example_03_create_device_errors.py --debug
python examples/python/example_04_dx11_clear_window.py --frames 180
python examples/python/example_05_dx11_moving_triangle.py --frames 300
python examples/python/example_06_capability_preflight.py
python examples/python/example_06_capability_preflight.py --backend dx12 --compare-active dx12
```

For a non-interactive smoke run of the windowed examples:

```powershell
python examples/python/example_04_dx11_clear_window.py --hidden --frames 3
python examples/python/example_05_dx11_moving_triangle.py --hidden --frames 3
```

## Notes

- `ctypes` stays very close to the C ABI, so `examples/python/dxbridge_ctypes.py` keeps the repeated boilerplate in one place instead of acting as a full SDK wrapper.
- Every ABI struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failing call on the same thread.
- The Python log callback must stay alive. If it gets garbage-collected, the native side still holds the stale function pointer.
- DX11 swap chains need a real Win32 `HWND`. A fake integer handle is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.

## Capability discovery quick check

`DXBridge_QueryCapability()` is exposed through the shared `DXBridgeLibrary` helper. Scenario 06 now accepts the same flags used by the other refreshed runtimes:

- `--backend all|dx11|dx12` to scope the pre-init pass
- `--compare-active dx11|dx12` to show how legacy `DXBridge_SupportsFeature()` differs after init

The fastest onboarding path is:

```powershell
python examples/python/example_06_capability_preflight.py --backend all --compare-active dx11
```

That prints, before `DXBridge_Init()`:

- whether DX11 and DX12 are available on the current machine
- whether each backend exposes a debug layer or GPU validation
- adapter count, adapter software status, and max feature level for each adapter
- an optional post-init comparison showing that `DXBridge_SupportsFeature()` is still active-backend scoped

## Validation

Recommended local checks:

```powershell
python -m py_compile examples/python/dxbridge_ctypes.py examples/python/example_01_load_version_logs.py examples/python/example_02_enumerate_adapters.py examples/python/example_03_create_device_errors.py examples/python/example_04_dx11_clear_window.py examples/python/example_05_dx11_moving_triangle.py
python examples/python/example_01_load_version_logs.py
python examples/python/example_02_enumerate_adapters.py
python examples/python/example_03_create_device_errors.py
python examples/python/example_04_dx11_clear_window.py --hidden --frames 3
python examples/python/example_05_dx11_moving_triangle.py --hidden --frames 3
python examples/python/example_06_capability_preflight.py
python examples/python/example_06_capability_preflight.py --backend dx11 --compare-active dx11
```

## Troubleshooting

If the commands fail, the most likely causes are:

- the DLL was not built yet
- the process is not running in a normal Windows desktop session
- the active adapter or backend is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine

## Related documentation

- [`../README.md`](../README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../run_capability_preflight.ps1`](../run_capability_preflight.ps1)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
