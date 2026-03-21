# Java JNA examples for `dxbridge.dll`

## Overview

This suite uses Java plus JNA and JNA Platform so it stays close to the published C ABI in `include/dxbridge/dxbridge.h`. It mirrors the same five-step progression used across the DXBridge v1 example families.

## Included examples

- `examples/java/src/dxbridge/DXBridgeLibrary.java` - minimal JNA binding for the public DLL ABI
- `examples/java/src/dxbridge/DXBridge.java` - small helper layer for loading the DLL, decoding errors, and common calls
- `examples/java/src/dxbridge/Win32Window.java` - small Win32 helper built on `jna-platform`
- `examples/java/src/examples/Example01LoadVersionLogs.java` - DLL loading, version decoding, and log callback lifetime
- `examples/java/src/examples/Example02EnumerateAdapters.java` - adapter enumeration
- `examples/java/src/examples/Example03CreateDeviceErrors.java` - immediate error retrieval and successful device creation
- `examples/java/src/examples/Example04Dx11ClearWindow.java` - real Win32 window creation plus a DX11 clear/present loop
- `examples/java/src/examples/Example05Dx11MovingTriangle.java` - real window, shaders, pipeline state, and animated triangle rendering
- `examples/java/fetch_deps.bat` - downloads `jna` and `jna-platform` jars into `examples/java/lib`
- `examples/java/build.bat` - compiles all Java sources into `examples/java/out/classes`
- `examples/java/run_example.bat` - runs one example by class suffix

## Requirements

- Windows 10 or 11
- Java 8+ (`javac` and `java` on `PATH`)
- a built `dxbridge.dll`
- internet access the first time if `examples/java/lib` does not already contain the JNA jars
- a normal desktop session for the DX11 windowed examples

## DLL discovery

The helper searches these locations automatically:

- `DXBRIDGE_DLL`
- `examples/java/dxbridge.dll`
- `build/debug/Debug/dxbridge.dll`
- `build/debug/examples/Debug/dxbridge.dll`
- `build/debug/tests/Debug/dxbridge.dll`
- `out/build/ci/Debug/dxbridge.dll`

You can override the DLL path explicitly:

```bat
run_example.bat Example01LoadVersionLogs --dll D:\trabajo\TestDLL\proyect\build\debug\Debug\dxbridge.dll
```

## How to run

From `examples/java`:

```bat
fetch_deps.bat
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
- The log callback and Win32 window procedure must stay strongly referenced on the Java side so the GC does not collect them while native code still holds the function pointer.
- DX11 swap chains need a real Win32 `HWND`. A fake integer value is not enough.
- `DXBridge_GetBackBuffer()` returns a handle owned by the swap chain. The examples destroy the RTV, swap chain, and device, but there is no separate destroy call for the back-buffer handle itself.

## Validation

Recommended local checks:

```bat
fetch_deps.bat
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
- JNA jars were not downloaded yet
- the process is not running in a normal Windows desktop session
- the requested backend or adapter is unavailable on that machine
- shader compilation support for DX11 is unavailable on that machine

## Related documentation

- [`../README.md`](../README.md)
- [`../hello_triangle/README.md`](../hello_triangle/README.md)
- [`../../docs/examples.md`](../../docs/examples.md)
- [`../../docs/api-reference.md`](../../docs/api-reference.md)
