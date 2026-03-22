# Native hello-triangle examples

## Overview

This folder contains the native Win32 reference examples for DXBridge v1. These are the clearest demonstrations of direct ABI usage from native code, and the language-specific folders under `examples/` mirror the same concepts from managed or scripting runtimes.

## Included examples

- `examples/hello_triangle/main_dx11.cpp` - primary DX11 sample with a `--self-test` mode used by CTest smoke coverage
- `examples/hello_triangle/main_dx12.cpp` - DX12 variant that exercises the same overall rendering path through the DX12 backend

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with the MSVC C++ toolchain
- CMake 3.20+
- a built `dxbridge.dll`
- a normal desktop session for the windowed runs

## How to run

Build the examples through the standard repository presets:

```bat
cmake --preset debug
cmake --build --preset debug
```

Typical commands after a debug build:

```bat
out\build\debug\Debug\hello_triangle.exe --self-test
out\build\debug\Debug\hello_triangle_dx12.exe
```

The generated executables are typically placed under the selected build tree, for example `out\build\debug\Debug\` when using the debug preset.

## Notes

- The DX11 sample is the canonical smoke path because it is already wired into CTest.
- The executables depend on a built `dxbridge.dll`, which the CMake targets copy alongside the binaries after build.
- The examples are the best native baseline before comparing behavior with Bun, Node.js, Go, Python, C#, Java, or Rust.

## Validation

Recommended local checks:

```bat
cmake --preset debug
cmake --build --preset debug
out\build\debug\Debug\hello_triangle.exe --self-test
out\build\debug\Debug\hello_triangle_dx12.exe
```

## Troubleshooting

If a command fails, the most likely causes are:

- the repository was not built yet
- the process is not running in a normal Windows desktop session
- the selected backend or adapter is unavailable on that machine
- build outputs were generated under a different preset or configuration than the command expects

## Related documentation

- [`../README.md`](../README.md)
- [`../../README.md`](../../README.md)
- [`../../docs/build-and-test.md`](../../docs/build-and-test.md)
- [`../../docs/examples.md`](../../docs/examples.md)
