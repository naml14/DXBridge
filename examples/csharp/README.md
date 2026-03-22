# C# P/Invoke examples for `dxbridge.dll`

## Overview

This suite uses plain .NET plus manual P/Invoke-style bindings built from `NativeLibrary` and delegates. It mirrors the same five-step progression used across the DXBridge v1 example families.

## Included examples

- `examples/csharp/DXBridgeExamples.csproj` - single SDK-style console project
- `examples/csharp/src/dxbridge/DXBridgeNative.cs` - low-level bindings for the exported C ABI
- `examples/csharp/src/dxbridge/DXBridge.cs` - small helper layer for loading the DLL, decoding errors, and common calls
- `examples/csharp/src/dxbridge/Win32Window.cs` - tiny Win32 helper for real `HWND` creation
- `examples/csharp/src/dxbridge/CommandLine.cs` - small parser shared by the samples
- `examples/csharp/src/examples/Example01LoadVersionLogs.cs` - DLL loading, version decoding, and log callback lifetime
- `examples/csharp/src/examples/Example02EnumerateAdapters.cs` - adapter enumeration
- `examples/csharp/src/examples/Example03CreateDeviceErrors.cs` - immediate error retrieval and successful device creation
- `examples/csharp/src/examples/Example04Dx11ClearWindow.cs` - real Win32 window creation plus a DX11 clear/present loop
- `examples/csharp/src/examples/Example05Dx11MovingTriangle.cs` - real window, shaders, pipeline state, and animated triangle rendering
- `examples/csharp/build.bat` - builds the project
- `examples/csharp/run_example.bat` - runs one example by class-style name

## Requirements

- Windows 10 or 11
- .NET SDK 10.0+ on `PATH`
- a built `dxbridge.dll`
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- `examples/csharp/dxbridge.dll`
- `out/build/debug/Debug/dxbridge.dll`
- `out/build/debug/examples/Debug/dxbridge.dll`
- `out/build/debug/tests/Debug/dxbridge.dll`
- `out/build/ci/Release/dxbridge.dll`

You can override the DLL path explicitly:

```bat
run_example.bat Example01LoadVersionLogs --dll D:\trabajo\TestDLL\proyect\out\build\debug\Debug\dxbridge.dll
```

## How to run

From `examples/csharp`:

```bat
build.bat
run_example.bat Example01LoadVersionLogs
run_example.bat Example02EnumerateAdapters
run_example.bat Example03CreateDeviceErrors --debug
run_example.bat Example04Dx11ClearWindow --hidden --frames 3
run_example.bat Example05Dx11MovingTriangle --hidden --frames 3 --sync-interval 0
```

## Notes

- Every ABI struct must set `struct_version = DXBRIDGE_STRUCT_VERSION`.
- `DXBridge_GetLastError()` is thread-local. Read it immediately after a failure on the same thread.
- The log callback and Win32 window procedure must stay strongly referenced on the managed side so the GC does not collect them while native code still holds the function pointer.
- DX11 swap chains need a real Win32 `HWND`. A fake integer value is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.

## Validation

Recommended local checks:

```bat
build.bat
run_example.bat Example01LoadVersionLogs
run_example.bat Example02EnumerateAdapters
run_example.bat Example03CreateDeviceErrors
run_example.bat Example04Dx11ClearWindow --hidden --frames 3
run_example.bat Example05Dx11MovingTriangle --hidden --frames 3 --sync-interval 0
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
